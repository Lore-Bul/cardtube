#!/usr/bin/env python3
"""Fill the publishing identity of CardTube in every file that needs it.

Usage (from the repository root):

    python tools/set_identity.py \
        --name "Mario Rossi" \
        --email "mario@example.com" \
        --github "mario-rossi" \
        --display-name "Mario Rossi" \
        --share-code "AB12" \
        --repo "https://github.com/mario-rossi/cardtube"

The script updates:

* ``app-builder.json``  -> ``store.source_repo``, ``store.author``, ``store.share_code``
* ``cmake/cm0-package.cmake`` -> ``APP_MAINTAINER``

It writes files in place and fails without touching anything when an argument
does not look valid.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MANIFEST = ROOT / "app-builder.json"
PACKAGE_CMAKE = ROOT / "cmake" / "cm0-package.cmake"

GITHUB_LOGIN = re.compile(r"^[A-Za-z0-9](?:[A-Za-z0-9-]{0,37}[A-Za-z0-9])?$")
EMAIL = re.compile(r"^[^<>@\s]+@[^<>@\s]+\.[^<>@\s]+$")
SHARE_CODE = re.compile(r"^[A-Z0-9]{4}$")
REPO_URL = re.compile(r"^(?:https?://)?github\.com/[A-Za-z0-9-]+/[A-Za-z0-9._-]+/?$")
MAINTAINER_LINE = re.compile(r'(set\(APP_MAINTAINER\s+)"([^"]*)"')


def fail(message: str) -> None:
    print(f"error: {message}", file=sys.stderr)
    raise SystemExit(1)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Set the CardTube maintainer/store identity.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--name", required=True, help="Maintainer full name used in the .deb")
    parser.add_argument("--email", required=True, help="Maintainer email used in the .deb")
    parser.add_argument("--github", required=True, help="GitHub login of the author")
    parser.add_argument(
        "--display-name",
        default=None,
        help="Author name shown in the store (defaults to --name)",
    )
    parser.add_argument(
        "--share-code",
        required=True,
        help="4 character store code, A-Z and 0-9 (assigned by the store portal)",
    )
    parser.add_argument(
        "--repo",
        default=None,
        help="Source repository URL (defaults to https://github.com/<github>/cardtube)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    name = args.name.strip()
    email = args.email.strip()
    github = args.github.strip().lstrip("@")
    display_name = (args.display_name or name).strip()
    share_code = args.share_code.strip().upper()
    repo = (args.repo or f"https://github.com/{github}/cardtube").strip()

    if not name or any(character in name for character in "<>"):
        fail("--name must be a non-empty plain text name")
    if not EMAIL.match(email):
        fail("--email must look like name@example.com")
    if not GITHUB_LOGIN.match(github):
        fail("--github must be a valid GitHub login (letters, digits and dashes)")
    if not display_name:
        fail("--display-name must not be empty")
    if not SHARE_CODE.match(share_code):
        fail("--share-code must be exactly 4 characters from A-Z and 0-9")
    if not REPO_URL.match(repo):
        fail("--repo must be a github.com/<login>/<repo> URL (https:// optional)")

    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    store = manifest.setdefault("store", {})
    store["source_repo"] = repo
    store["author"] = {"github": github, "display_name": display_name}
    store["share_code"] = share_code
    MANIFEST.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
        newline="\n",
    )

    maintainer = f"{name} <{email}>"
    cmake_text = PACKAGE_CMAKE.read_text(encoding="utf-8")
    updated_text, replacements = MAINTAINER_LINE.subn(
        lambda match: f'{match.group(1)}"{maintainer}"',
        cmake_text,
        count=1,
    )
    if replacements != 1:
        fail("could not find set(APP_MAINTAINER ...) in cmake/cm0-package.cmake")
    PACKAGE_CMAKE.write_text(updated_text, encoding="utf-8", newline="\n")

    print("identity updated")
    print(f"  maintainer  : {maintainer}")
    print(f"  author      : {display_name} (@{github})")
    print(f"  source repo : {repo}")
    print(f"  share code  : {share_code}")
    print()
    print("next steps:")
    print("  python tools/preflight.py")
    print("  git add -A && git commit -m 'Release 0.3.0'")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
