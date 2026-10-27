# Load the F Prime build system (not its code) while cFS prepares the arch build, so that later arch_build steps of
# other modules can register custom autocoders/targets before CMakeLists.txt includes the F Prime code.
# cFS is a C project; F Prime fixes its required C++ options at first load, so the C++ compiler must be known now
enable_language(CXX)
set(FPRIME_INCLUDE_FRAMEWORK_CODE OFF)
find_package(FPrime REQUIRED PATHS "${fprime_MISSION_DIR}")
