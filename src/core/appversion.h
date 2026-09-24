#pragma once

#ifndef TOMOSS_VERSION
#define TOMOSS_VERSION "0.0.0"
#endif

namespace version {

/** CMake project(VERSION) 注入的语义化版本，如 "0.1.0"。 */
inline const char *semver()
{
    return TOMOSS_VERSION;
}

} // namespace version
