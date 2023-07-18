#Copied from cmake-2.8 because we want to find the static library on Windows

# - Find zlib
# Find the native ZLIB includes and library.
# Once done this will define
#
#  ZLIB_INCLUDE_DIRS   - where to find zlib.h, etc.
#  ZLIB_LIBRARIES      - List of libraries when using zlib.
#  ZLIB_FOUND          - True if zlib found.
#
#  ZLIB_VERSION_STRING - The version of zlib found (x.y.z)
#  ZLIB_VERSION_MAJOR  - The major version of zlib
#  ZLIB_VERSION_MINOR  - The minor version of zlib
#  ZLIB_VERSION_PATCH  - The patch version of zlib
#  ZLIB_VERSION_TWEAK  - The tweak version of zlib
#
# The following variable are provided for backward compatibility
#
#  ZLIB_MAJOR_VERSION  - The major version of zlib
#  ZLIB_MINOR_VERSION  - The minor version of zlib
#  ZLIB_PATCH_VERSION  - The patch version of zlib

#=============================================================================
# Copyright 2001-2009 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

FIND_PATH(ZLIB_INCLUDE_DIR zlib.h
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\GnuWin32\\Zlib;InstallPath]/include"
	"C:\\zlib\\"
	"C:\\zlib-1.2.8\\"
	"C:\\zlib-1.2.7\\"
)

SET(ZLIB_NAMES_DBG zlibd zlibd1 zlibstaticd)
set(ZLIB_NAMES_REL z zlib zdll zlib1 zlibstatic)

if(WIN32)
  set(ZLIB_NAMES_DBG zlibstaticd ${ZLIB_NAMES_DBG}) #prefer static on windows
  set(ZLIB_NAMES_REL zlibstatic ${ZLIB_NAMES_REL})
else()
  set(ZLIB_NAMES_DBG ${ZLIB_NAMES_DBG} zlibstaticd)
  set(ZLIB_NAMES_REL ${ZLIB_NAMES_REL} zlibstatic)
endif()

FIND_LIBRARY(ZLIB_LIBRARY_DBG
    NAMES
        ${ZLIB_NAMES_DBG}
    PATHS
        "[HKEY_LOCAL_MACHINE\\SOFTWARE\\GnuWin32\\Zlib;InstallPath]/lib"
		"C:\\zlib\\Debug"
		"C:\\zlib-1.2.8\\Debug"
		"C:\\zlib-1.2.7\\Debug"
)

FIND_LIBRARY(ZLIB_LIBRARY_REL
    NAMES
        ${ZLIB_NAMES_REL}
    PATHS
        "[HKEY_LOCAL_MACHINE\\SOFTWARE\\GnuWin32\\Zlib;InstallPath]/lib"
		"C:\\zlib\\Release"
		"C:\\zlib-1.2.8\\Release"
		"C:\\zlib-1.2.7\\Release"
)

MARK_AS_ADVANCED(ZLIB_LIBRARY_DBG ZLIB_LIBRARY_REL ZLIB_INCLUDE_DIR)


IF(ZLIB_INCLUDE_DIR AND EXISTS "${ZLIB_INCLUDE_DIR}/zlib.h")
    FILE(STRINGS "${ZLIB_INCLUDE_DIR}/zlib.h" ZLIB_H REGEX "^#define ZLIB_VERSION \"[^\"]*\"$")

    STRING(REGEX REPLACE "^.*ZLIB_VERSION \"([0-9]+).*$" "\\1" ZLIB_VERSION_MAJOR "${ZLIB_H}")
    STRING(REGEX REPLACE "^.*ZLIB_VERSION \"[0-9]+\\.([0-9]+).*$" "\\1" ZLIB_VERSION_MINOR  "${ZLIB_H}")
    STRING(REGEX REPLACE "^.*ZLIB_VERSION \"[0-9]+\\.[0-9]+\\.([0-9]+).*$" "\\1" ZLIB_VERSION_PATCH "${ZLIB_H}")
    SET(ZLIB_VERSION_STRING "${ZLIB_VERSION_MAJOR}.${ZLIB_VERSION_MINOR}.${ZLIB_VERSION_PATCH}")

    # only append a TWEAK version if it exists:
    SET(ZLIB_VERSION_TWEAK "")
    IF( "${ZLIB_H}" MATCHES "^.*ZLIB_VERSION \"[0-9]+\\.[0-9]+\\.[0-9]+\\.([0-9]+).*$")
        SET(ZLIB_VERSION_TWEAK "${CMAKE_MATCH_1}")
        SET(ZLIB_VERSION_STRING "${ZLIB_VERSION_STRING}.${ZLIB_VERSION_TWEAK}")
    ENDIF( "${ZLIB_H}" MATCHES "^.*ZLIB_VERSION \"[0-9]+\\.[0-9]+\\.[0-9]+\\.([0-9]+).*$")

    SET(ZLIB_MAJOR_VERSION "${ZLIB_VERSION_MAJOR}")
    SET(ZLIB_MINOR_VERSION "${ZLIB_VERSION_MINOR}")
    SET(ZLIB_PATCH_VERSION "${ZLIB_VERSION_PATCH}")
ENDIF()

# handle the QUIETLY and REQUIRED arguments and set ZLIB_FOUND to TRUE if 
# all listed variables are TRUE


INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ZLIB REQUIRED_VARS ZLIB_INCLUDE_DIR ZLIB_LIBRARY_REL # rel should always be there
                                       VERSION_VAR ZLIB_VERSION_STRING)

IF(ZLIB_FOUND)
    SET(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR})
	if(ZLIB_LIBRARY_DBG)
	set(ZLIB_LIBRARIES debug ${ZLIB_LIBRARY_DBG} optimized ${ZLIB_LIBRARY_REL})
	else()
	SET(ZLIB_LIBRARIES ${ZLIB_LIBRARY_REL})
	endif()
ENDIF()

