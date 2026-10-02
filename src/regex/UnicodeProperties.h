#pragma once

#include <string>
#include <string_view>

#include "regex/RegexAst.h"

namespace zbik {

[[nodiscard]] CodePointClass unicodeProperty(std::string_view name);
[[nodiscard]] std::string unicodeDataVersion();

} // namespace zbik
