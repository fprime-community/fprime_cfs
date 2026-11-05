####
# CMakeLists.txt:
#
# Interfaces for cFS message handlers and producers (FPP only, no sources).
####
register_fprime_module(
    FPrimeCfs_Interfaces
  AUTOCODER_INPUTS
    "${CMAKE_CURRENT_LIST_DIR}/FPrimeCfsInterfaces.fpp"
  INTERFACE
)
