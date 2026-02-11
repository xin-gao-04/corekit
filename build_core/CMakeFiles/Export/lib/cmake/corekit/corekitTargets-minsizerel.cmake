#----------------------------------------------------------------
# Generated CMake target import file for configuration "MinSizeRel".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "corekit::corekit" for configuration "MinSizeRel"
set_property(TARGET corekit::corekit APPEND PROPERTY IMPORTED_CONFIGURATIONS MINSIZEREL)
set_target_properties(corekit::corekit PROPERTIES
  IMPORTED_IMPLIB_MINSIZEREL "${_IMPORT_PREFIX}/lib/corekit.lib"
  IMPORTED_LOCATION_MINSIZEREL "${_IMPORT_PREFIX}/bin/corekit.dll"
  )

list(APPEND _IMPORT_CHECK_TARGETS corekit::corekit )
list(APPEND _IMPORT_CHECK_FILES_FOR_corekit::corekit "${_IMPORT_PREFIX}/lib/corekit.lib" "${_IMPORT_PREFIX}/bin/corekit.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
