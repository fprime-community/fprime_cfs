# Load the F Prime build system (not its code) while cFS prepares the arch build, so that later arch_build steps of
# other modules can register custom autocoders/targets before CMakeLists.txt includes the F Prime code.
set(FPRIME_INCLUDE_FRAMEWORK_CODE OFF)
find_package(FPrime REQUIRED PATHS "${fprime_MISSION_DIR}")
