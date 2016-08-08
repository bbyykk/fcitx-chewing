# - Try to find the TAIGI libraries
# Once done this will define
#
#  TAIGI_FOUND - system has TAIGI
#  TAIGI_INCLUDE_DIR - the TAIGI include directory
#  TAIGI_LIBRARIES - TAIGI library
#
# Copyright (c) 2012 CSSlayer <wengxt@gmail.com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if(TAIGI_INCLUDE_DIR AND TAIGI_LIBRARIES AND TAIGI_DATADIR)
    # Already in cache, be silent
    set(TAIGI_FIND_QUIETLY TRUE)
endif(TAIGI_INCLUDE_DIR AND TAIGI_LIBRARIES AND TAIGI_DATADIR)

find_package(PkgConfig)
pkg_check_modules(PC_LIBTAIGI QUIET taigi)

find_path(TAIGI_MAIN_INCLUDE_DIR
          NAMES taigi.h
          HINTS ${PC_LIBTAIGI_INCLUDEDIR}
          PATH_SUFFIXES taigi)

find_library(TAIGI_LIBRARIES
             NAMES taigi
             HINTS ${PC_LIBTAIGI_LIBDIR})

set(TAIGI_INCLUDE_DIR "${TAIGI_MAIN_INCLUDE_DIR}")
set(TAIGI_FOUND ${PC_LIBTAIGI_FOUND})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Taigi FOUND_VAR TAIGI_FOUND
                                          REQUIRED_VARS TAIGI_LIBRARIES TAIGI_MAIN_INCLUDE_DIR
                                          VERSION_VAR PC_LIBTAIGI_VERSION)

mark_as_advanced(TAIGI_INCLUDE_DIR TAIGI_LIBRARIES)
