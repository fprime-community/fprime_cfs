####
# CMakeLists.txt:
#
# Sets up the fprime module build within CMake.
####
register_fprime_module(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/CfsTypes.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/CfsChecksum.cpp"
    DEPENDS
        Fw_Types
        Svc_Ccsds_Types
)
