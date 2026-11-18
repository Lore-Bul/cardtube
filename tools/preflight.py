#!/usr/bin/env python3
"""Validate everything the CardputerZero store checks before an upload.

Run from anywhere:

    python tools/preflight.py

It verifies the upload manifest (``app-builder.json``), the assets it points
to, the packaged identity in ``cmake/cm0-package.cmake`` and the consistency
with ``CMakeLists.txt``. Exit code 1 means "do not publish yet".

Checks performed:

* no template placeholders left (``TODO``, ``Your Name``, ``example.invalid``)
* version/package name match the CMake project
* ``store.summary`` is at most 80 characters
* 1-6 screenshots, every file a real 320x170 PNG
* square icon between 128 and 512 pixels
* ``share_code`` is 4 characters (A-Z, 0-9)
* categories come from the store enum (1-2 entries)
* all seven permission flags are booleans
* ``APP_MAINTAINER`` is a ``Name <email>`` pair

Pass ``--skip-identity`` to check layout/assets only on a checkout where the
publishing identity has not been filled in yet.
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MANIFEST = ROOT / "app-builder.json"
CMAKE_LISTS = ROOT / "CMakeLists.txt"
PACKAGE_CMAKE = ROOT / "cmake" / "cm0-package.cmake"

SCREENSHOT_SIZE = (320, 170)
ICON_MIN = 128
ICON_MAX = 512
SUMMARY_LIMIT = 80

STORE_CATEGORIES = {
    "System Tools",
    "Development",
    "Hardware & IoT",
    "Security",
    "Radio & Comms",
    "AI",
    "Creative & Office",
    "Media",
    "Games",
    "Emulators",
    "Education",
    "Lifestyle",
    "Other",
}

PERMISSIONS = (
    "camera",
    "microphone",
    "imu",
    "network",
    "additional_hardware",
    "background_service",
    "external_display",
)

PLACEHOLDER_MARKERS = ("TODO", "your-login", "Your Name", "example.invalid", "CHANGE-ME")

SHARE_CODE = re.compile(r"^[A-Z0-9]{4}$")
GITHUB_LOGIN = re.compile(r"^[A-Za-z0-9](?:[A-Za-z0-9-]{0,37}[A-Za-z0-9])?$")
REPO_URL = re.compile(r"^(?:https?://)?github\.com/[A-Za-z0-9-]+/[A-Za-z0-9._-]+/?$")
SPDX = re.compile(r"^[A-Za-z0-9.+-]+$")
MAINTAINER_VALUE = re.compile(r"^(.+?)\s+<([^<>@\s]+@[^<>@\s]+)>$")

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


class Report:
    def __init__(self) -> None:
        self.errors: list[str] = []
        self.warnings: list[str] = []

    def error(self, message: str) -> None:
        self.errors.append(message)

    def warn(self, message: str) -> None:
        self.warnings.append(message)

    def print(self) -> None:
        for warning in self.warnings:
            print(f"warning: {warning}")
        for error in self.errors:
            print(f"error: {error}")


def png_dimensions(path: Path) -> tuple[int, int] | tuple[None, None]:
    try:
        with path.open("rb") as handle:
            header = handle.read(24)
    except OSError:
        return None, None
    if len(header) < 24 or header[:8] != PNG_SIGNATURE or header[12:16] != b"IHDR":
        return None, None
    width, height = struct.unpack(">II", header[16:24])
    return width, height


def is_placeholder(value: str) -> bool:
    return any(marker.casefold() in value.casefold() for marker in PLACEHOLDER_MARKERS)


def check_manifest(report: Report, skip_identity: bool) -> None:
    if not MANIFEST.is_file():
        report.error("app-builder.json is missing")
        return
    try:
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        report.error(f"app-builder.json is not valid JSON: {error}")
        return

    for field in ("app_name", "package_name", "version", "store"):
        if field not in manifest:
            report.error(f"app-builder.json: missing top-level field '{field}'")
    store = manifest.get("store")
    if not isinstance(store, dict):
        report.error("app-builder.json: 'store' must be an object")
        return

    project_name, project_version = read_cmake_project(report)
    if project_name and manifest.get("package_name") != project_name:
        report.error(
            f"package_name '{manifest.get('package_name')}' does not match CMake project '{project_name}'"
        )
    if project_version and manifest.get("version") != project_version:
        report.error(
            f"version '{manifest.get('version')}' does not match CMake project version '{project_version}'"
        )

    for field in (
        "summary",
        "categories",
        "screenshots",
        "icon",
        "license",
        "source_repo",
        "author",
        "share_code",
        "permissions",
    ):
        if field not in store:
            report.error(f"app-builder.json: missing store.{field}")

    summary = store.get("summary")
    if not isinstance(summary, str) or not summary.strip():
        report.error("store.summary must be a non-empty string")
    else:
        if len(summary) > SUMMARY_LIMIT:
            report.error(
                f"store.summary is {len(summary)} characters, the limit is {SUMMARY_LIMIT}"
            )
        if not skip_identity and is_placeholder(summary):
            report.error("store.summary still contains a placeholder")

    description = store.get("description")
    if description is not None and not isinstance(description, str):
        report.error("store.description must be a string when present")

    check_categories(report, store.get("categories"))
    check_screenshots(report, store.get("screenshots"))
    check_icon(report, store.get("icon"))

    license_value = store.get("license")
    if not isinstance(license_value, str) or not SPDX.match(license_value):
        report.error("store.license must be an SPDX identifier such as 'MIT'")

    if not skip_identity:
        check_identity(report, store)

    permissions = store.get("permissions")
    if not isinstance(permissions, dict):
        report.error("store.permissions must be an object")
    else:
        for key in PERMISSIONS:
            if key not in permissions:
                report.error(f"store.permissions.{key} is missing")
            elif not isinstance(permissions[key], bool):
                report.error(f"store.permissions.{key} must be a boolean")
        for key in sorted(set(permissions) - set(PERMISSIONS)):
            report.warn(f"store.permissions.{key} is not part of the store schema")


def check_categories(report: Report, categories: object) -> None:
    if not isinstance(categories, list) or not categories:
        report.error("store.categories must be a non-empty array")
        return
    if len(categories) > 2:
        report.error("store.categories accepts at most 2 entries")
    for category in categories:
        if category not in STORE_CATEGORIES:
            report.error(f"'{category}' is not a known store category")


def check_screenshots(report: Report, screenshots: object) -> None:
    if not isinstance(screenshots, list) or not screenshots:
        report.error("store.screenshots must be a non-empty array")
        return
    if len(screenshots) > 6:
        report.error("store.screenshots accepts at most 6 entries")
    for entry in screenshots:
        path = ROOT / str(entry)
        if not path.is_file():
            report.error(f"screenshot does not exist: {entry}")
            continue
        width, height = png_dimensions(path)
        if width is None:
            report.error(f"screenshot is not a PNG: {entry}")
        elif (width, height) != SCREENSHOT_SIZE:
            report.error(f"screenshot {entry} is {width}x{height}, expected 320x170")


def check_icon(report: Report, icon: object) -> None:
    if not isinstance(icon, str) or not icon:
        report.error("store.icon must be a repository-relative path")
        return
    path = ROOT / icon
    if not path.is_file():
        report.error(f"icon does not exist: {icon}")
        return
    width, height = png_dimensions(path)
    if width is None:
        report.error(f"icon is not a PNG: {icon}")
    elif width != height:
        report.error(f"icon {icon} is {width}x{height}, it must be square")
    elif not ICON_MIN <= width <= ICON_MAX:
        report.error(
            f"icon {icon} is {width}x{height}, it must be between {ICON_MIN} and {ICON_MAX} pixels"
        )


def check_identity(report: Report, store: dict) -> None:
    repo = store.get("source_repo")
    if not isinstance(repo, str) or not REPO_URL.match(repo):
        report.error("store.source_repo must be github.com/<login>/<repo> (https:// optional)")
    elif is_placeholder(repo):
        report.error("store.source_repo still contains a placeholder")

    author = store.get("author")
    if not isinstance(author, dict):
        report.error("store.author must be an object with display_name")
    else:
        display_name = author.get("display_name")
        if not isinstance(display_name, str) or not display_name.strip():
            report.error("store.author.display_name must be a non-empty string")
        elif is_placeholder(display_name):
            report.error("store.author.display_name still contains a placeholder")

        github = author.get("github")
        if github is not None:
            if not isinstance(github, str) or not GITHUB_LOGIN.match(github):
                report.error("store.author.github must be a valid GitHub login")
            elif is_placeholder(github):
                report.error("store.author.github still contains a placeholder")

        email = author.get("email")
        if email is not None and (
            not isinstance(email, str) or is_placeholder(email) or "@" not in email
        ):
            report.error("store.author.email must be a valid email address")

    share_code = store.get("share_code")
    if not isinstance(share_code, str) or not SHARE_CODE.match(share_code):
        report.error("store.share_code must be exactly 4 characters from A-Z and 0-9")
    elif is_placeholder(share_code):
        report.error("store.share_code still contains a placeholder")

    check_maintainer(report)


def check_maintainer(report: Report) -> None:
    if not PACKAGE_CMAKE.is_file():
        report.error("cmake/cm0-package.cmake is missing")
        return
    text = PACKAGE_CMAKE.read_text(encoding="utf-8")
    match = re.search(r'set\(APP_MAINTAINER\s+"([^"]*)"', text)
    if not match:
        report.error("cmake/cm0-package.cmake: APP_MAINTAINER not found")
        return
    maintainer = match.group(1).strip()
    if is_placeholder(maintainer):
        report.error(
            "APP_MAINTAINER still contains a placeholder; run tools/set_identity.py"
        )
        return
    if not MAINTAINER_VALUE.match(maintainer):
        report.error("APP_MAINTAINER must use the 'Name <email@example.com>' format")


def read_cmake_project(report: Report) -> tuple[str | None, str | None]:
    if not CMAKE_LISTS.is_file():
        report.error("CMakeLists.txt is missing")
        return None, None
    text = CMAKE_LISTS.read_text(encoding="utf-8")
    match = re.search(
        r"project\(\s*([A-Za-z0-9_.+-]+)\s+VERSION\s+([0-9]+(?:\.[0-9]+)+)",
        text,
    )
    if not match:
        report.error("CMakeLists.txt: could not read project() name and version")
        return None, None
    return match.group(1), match.group(2)


def main() -> int:
    parser = argparse.ArgumentParser(description="Check CardTube release metadata.")
    parser.add_argument(
        "--skip-identity",
        action="store_true",
        help="skip maintainer/repository/share-code checks",
    )
    args = parser.parse_args()

    report = Report()
    check_manifest(report, args.skip_identity)
    report.print()

    errors = len(report.errors)
    warnings = len(report.warnings)
    if errors:
        print(f"\npreflight failed: {errors} error(s), {warnings} warning(s)")
        return 1
    print(f"preflight passed: 0 error(s), {warnings} warning(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
