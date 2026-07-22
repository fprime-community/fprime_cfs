cmake_minimum_required(VERSION 3.22)
project(CFE_FPRIME_CFS C CXX)

# When built standalone (e.g. `fprime-util generate --ut` for unit testing), the F Prime
# build system has not been loaded yet. When built as part of a cFS mission build, the
# fprime app has already loaded it.
if (NOT COMMAND register_fprime_project)
    find_package(FPrime REQUIRED PATHS "${FPRIME_FRAMEWORK_PATH}")
endif()
register_fprime_project()

# In this library, we add -fPIC to the global interface target ... so everything compiles with -fPIC. This is needed
# because cFS builds apps as shared libraries. Only the final app should be shared (not intermediate F Prime
# libraries). This means we cannot get away with just setting BUILD_SHARED_LIBS to ON.
target_compile_options("${FPRIME_GLOBAL_INTERFACE_TARGET}" INTERFACE -fPIC)

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/FPrimeCfs")
