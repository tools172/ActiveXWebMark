#include "version.h"

// Supplied by CMake from the project version (or the release tag).
#ifndef AWM_VERSION_STRING
#define AWM_VERSION_STRING "0.0.0-dev"
#endif

namespace awm {

std::string version()
{
    return AWM_VERSION_STRING;
}

} // namespace awm
