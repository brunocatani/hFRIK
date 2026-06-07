#pragma once

#include <cctype>
#include <optional>
#include <string>
#include <string_view>

namespace frik::api::tag_policy
{
    [[nodiscard]] inline bool isWhitespace(const char value)
    {
        return std::isspace(static_cast<unsigned char>(value)) != 0;
    }

    [[nodiscard]] inline std::optional<std::string> normalizeTag(const char* tag)
    {
        if (!tag) {
            return std::nullopt;
        }

        const std::string_view value(tag);
        std::size_t begin = 0;
        while (begin < value.size() && isWhitespace(value[begin])) {
            ++begin;
        }

        std::size_t end = value.size();
        while (end > begin && isWhitespace(value[end - 1])) {
            --end;
        }

        if (begin == end) {
            return std::nullopt;
        }

        return std::string(value.substr(begin, end - begin));
    }
}

