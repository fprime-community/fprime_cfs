cmake_minimum_required(VERSION 3.22)
project(CFE_FPRIME_CFS C CXX)

# When built standalone (e.g. `fprime-util generate --ut` for unit testing), the F Prime
# build system has not been loaded yet. When built as part of a cFS mission build, the
# fprime app has already loaded it.
if (NOT COMMAND register_fprime_project)
    set(FPRIME_INCLUDE_FRAMEWORK_CODE OFF)
    find_package(FPrime REQUIRED PATHS "${FPRIME_FRAMEWORK_PATH}")
endif()
register_fprime_project()
# Include the F Prime code exactly once: the mission's `fprime` module normally does it (FPrime-Code.cmake);
# standalone builds (fprime-util) do it here, after every arch_build step had its chance to register
get_property(_FPRIME_CFS_CODE_INCLUDED GLOBAL PROPERTY MODULE_DETECTION SET)
if (NOT _FPRIME_CFS_CODE_INCLUDED)
    fprime_setup_included_code()
endif()

# In this library, we add -fPIC to the global interface target ... so everything compiles with -fPIC. This is needed
# because cFS builds apps as shared libraries. Only the final app should be shared (not intermediate F Prime
# libraries). This means we cannot get away with just setting BUILD_SHARED_LIBS to ON.
target_compile_options("${FPRIME_GLOBAL_INTERFACE_TARGET}" INTERFACE -fPIC)

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/config/FPrimeCfsConfig")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/FPrimeCfs")
