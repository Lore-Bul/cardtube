/*
 * SPDX-FileCopyrightText: 2026 CardTube contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace i18n {

enum class Language {
    English,
    Italian,
};

Language language();
void set_language(Language language);

// Translates an English source string. Unknown keys fall back to English, so
// new UI text keeps working even before a translation is added.
const char* tr(const char* english);
std::string tr(const std::string& english);

const char* language_code(Language language);
const char* language_name(Language language);
Language language_from_code(const std::string& code);

} // namespace i18n
