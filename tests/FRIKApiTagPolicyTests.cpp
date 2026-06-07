#include "api/FRIKApiTagPolicy.h"

#include <cstdio>
#include <cstring>

namespace
{
    bool expectNoTag(const char* name, const char* input)
    {
        const auto actual = frik::api::tag_policy::normalizeTag(input);
        if (actual) {
            std::printf("%s expected no tag got '%s'\n", name, actual->c_str());
            return false;
        }
        return true;
    }

    bool expectTag(const char* name, const char* input, const char* expected)
    {
        const auto actual = frik::api::tag_policy::normalizeTag(input);
        if (!actual || std::strcmp(actual->c_str(), expected) != 0) {
            std::printf("%s expected '%s' got '%s'\n", name, expected, actual ? actual->c_str() : "(none)");
            return false;
        }
        return true;
    }
}

int main()
{
    bool ok = true;
    ok &= expectNoTag("null tag rejected", nullptr);
    ok &= expectNoTag("empty tag rejected", "");
    ok &= expectNoTag("whitespace tag rejected", " \t\r\n ");
    ok &= expectTag("plain tag preserved", "ROCK_Grab", "ROCK_Grab");
    ok &= expectTag("tag trimmed", "  ROCK_Grab\t", "ROCK_Grab");
    ok &= expectTag("internal whitespace preserved", "  ROCK Grab  ", "ROCK Grab");
    return ok ? 0 : 1;
}

