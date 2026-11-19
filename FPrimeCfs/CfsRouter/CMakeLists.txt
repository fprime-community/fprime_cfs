####
# F Prime CMakeLists.txt:
#
# SOURCES: list of source files (to be compiled)
# AUTOCODER_INPUTS: list of files to be passed to the autocoders
# DEPENDS: list of libraries that this module depends on
#
# More information in the F´ CMake API documentation:
# https://fprime.jpl.nasa.gov/latest/docs/reference/api/cmake/API/
#
####
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Types")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/config/CfsRouterConfig")

register_fprime_module(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/CfsRouter.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/CfsRouter.cpp"
    DEPENDS
        Fw_DataStructures
        FPrimeCfs_Ports
        FPrimeCfs_Types
)

### Unit Tests ###
if (BUILD_TESTING AND NOT __FPRIME_NO_UT_GEN__)
    add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/test/ut/config")
endif()
register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/CfsRouter.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/CfsRouterTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/CfsRouterTester.cpp"
    DEPENDS
        STest # For rules-based testing
    UT_AUTO_HELPERS
)
