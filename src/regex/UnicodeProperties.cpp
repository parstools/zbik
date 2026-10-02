#include "regex/UnicodeProperties.h"

#include <stdexcept>
#include <unicode/uchar.h>
#include <unicode/uversion.h>
#include <vector>

namespace zbik {
namespace {

auto buildProperty(UProperty property) -> CodePointClass {
    std::vector<CodePointRange> ranges;
    bool inside = false;
    CodePoint first = 0;
    for (CodePoint value = 0; value <= maxUnicodeCodePoint; ++value) {
        const bool member = isUnicodeScalar(value) &&
                u_hasBinaryProperty(static_cast<UChar32>(value), property);
        if (member && !inside) {
            first = value;
            inside = true;
        } else if (!member && inside) {
            ranges.push_back({first, value - 1U});
            inside = false;
        }
    }
    if (inside) ranges.push_back({first, maxUnicodeCodePoint});
    return CodePointClass(std::move(ranges));
}

} // namespace

CodePointClass unicodeProperty(std::string_view name) {
    if (name == "XID_Start") {
        static const CodePointClass value = buildProperty(UCHAR_XID_START);
        return value;
    }
    if (name == "XID_Continue") {
        static const CodePointClass value = buildProperty(UCHAR_XID_CONTINUE);
        return value;
    }
    throw std::invalid_argument("unsupported Unicode property `" +
                                std::string(name) + "`");
}

std::string unicodeDataVersion() {
    UVersionInfo version{};
    u_getUnicodeVersion(version);
    char text[U_MAX_VERSION_STRING_LENGTH]{};
    u_versionToString(version, text);
    return text;
}

} // namespace zbik
