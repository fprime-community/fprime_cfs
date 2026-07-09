###########################################################
#
# FPRIME_CFS mission build setup
#
# This file is evaluated as part of the "prepare" stage
# and can be used to set up prerequisites for the build,
# such as generating header files
#
###########################################################

# Generate the "fprime_cfs_compatibility.h" wrapper so the compatibility
# definitions are available mission-wide. generate_config_includefile() is
# used directly (rather than generate_configfile_set()) because it exists in
# all supported cFE versions.
generate_config_includefile(
    FILE_NAME     "fprime_cfs_compatibility.h"
    FALLBACK_FILE "${CMAKE_CURRENT_LIST_DIR}/config/default_fprime_cfs_compatibility.h"
)
