
find_path(MPCDEC_INCLUDE_DIR mpc/mpcdec.h)
find_library(MPCDEC_LIBRARY mpcdec)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(MPCDEC
    REQUIRED_VARS MPCDEC_LIBRARY MPCDEC_INCLUDE_DIR)

mark_as_advanced(MPCDEC_INCLUDE_DIR MPCDEC_LIBRARY)

if(MPCDEC_FOUND AND NOT TARGET MPC::Dec)
    add_library(MPC::Dec UNKNOWN IMPORTED)
    set_target_properties(MPC::Dec PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${MPCDEC_INCLUDE_DIR}
        IMPORTED_LOCATION ${MPCDEC_LIBRARY})
endif()

