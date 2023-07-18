cmake_minimum_required (VERSION 2.8 FATAL_ERROR)

project(TrustedQSL)

find_package(OpenSSL REQUIRED)


if ( CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" )
  if ( CMAKE_VERSION VERSION_LESS 2.8.12 )
    SET ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC" )
  else()
    ADD_COMPILE_OPTIONS("-fPIC")
  endif ( CMAKE_VERSION VERSION_LESS 2.8.12 )
endif( CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" )

SET (LINUX FALSE)
IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    SET(LINUX TRUE)
ENDIF ()

# These attempts to play with LIB_SUFFIX aren't portable.
# Comment this out, and let GNUInstallDirs do this.
# Uncomment if you're sure it's needed.
#SET (LIB_SUFFIX "")
#
# Pre-set LIB_SUFFIX for 64-bit Linux platforms
#IF (LINUX AND CMAKE_SHARED_LIBRARY_PREFIX STREQUAL "lib64")
#  SET (LIB_SUFFIX "64")
#ENDIF()
#
# Make sure that there's a value for LIB_SUFFIX
#IF (LINUX AND NOT CMAKE_SHARED_LIBRARY_PREFIX AND NOT LIB_SUFFIX) 
#  # check 64 bit
#    IF (CMAKE_SIZEOF_VOID_P EQUAL 4)
#      SET (LIB_SUFFIX "")
#    ELSE (CMAKE_SIZEOF_VOID_P EQUAL 4)
#      SET (LIB_SUFFIX "64")
#    ENDIF (CMAKE_SIZEOF_VOID_P EQUAL 4)
#ENDIF (LINUX AND NOT CMAKE_SHARED_LIBRARY_PREFIX AND NOT LIB_SUFFIX) 

if(LINUX)
	include(GNUInstallDirs)
	if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
		if(CMAKE_SIZEOF_VOID_P EQUAL 4)
			set(CMAKE_INSTALL_LIBDIR lib)
		else()
			set(CMAKE_INSTALL_LIBDIR lib64)
		endif()
	endif()
	if(NOT DEFINED CMAKE_INSTALL_DATADIR)
		set(CMAKE_INSTALL_DATADIR share)
	endif()
	if(NOT DEFINED CMAKE_INSTALL_INCLUDEDIR)
		set(CMAKE_INSTLL_INCLUDEDIR include)
	endif()
	if(NOT DEFINED CMAKE_INSTALL_BINDIR)
		set(CMAKE_INSTALL_BINDIR bin)
	endif()
elseif (NOT WIN32 AND NOT APPLE)
	set(CMAKE_INSTALL_LIBDIR lib)
	set(CMAKE_INSTALL_DATADIR share)
	set(CMAKE_INSTALL_INCLUDEDIR include)
	set(CMAKE_INSTALL_BINDIR bin)
endif(LINUX)

if(WIN32)
  set(CMAKE_FIND_LIBRARY_SUFFIXES .lib .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
  # just windows for now... unix knows how to find libraries better
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmakemodules/")
find_package(OptionalAddSubdirectory REQUIRED)

option(USE_STATIC_MSVCRT "Use a static Visual C++ Runtime when building with MSVC")

if(MSVC AND USE_STATIC_MSVCRT)
foreach(flag_var
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
   if(${flag_var} MATCHES "/MD")
      string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
   endif(${flag_var} MATCHES "/MD")
endforeach(flag_var)
else()
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
endif()

# Prefer LMDB, fall back to BDB if that's not available
find_package(LMDB)
if(NOT LMDB_FOUND)
	set (LMDB_LIBRARIES "")
	find_package(BDB REQUIRED)
	set (LMDB_INCLUDE_DIR "")
else()
	add_definitions(-DUSE_LMDB)
	set (BDB_INCLUDE_DIR "")
endif()

if(NOT APPLE AND NOT WIN32)
	add_definitions("-DCONFDIR=\"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/TrustedQSL/\"")
endif()

# Get version number

file(STRINGS "apps/tqslversion.ver" TQSLVERSION)
string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\1" TQSL_VERSION_MAJOR ${TQSLVERSION})
string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\2" TQSL_VERSION_MINOR ${TQSLVERSION})
string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\3" TQSL_VERSION_UPDATE ${TQSLVERSION})

# Handle case where version number does not have an update i.e. 2.0
if (TQSL_VERSION_MAJOR STREQUAL ${TQSLVERSION}) 
	string(REGEX REPLACE "([0-9]+)\\.([0-9]+)" "\\1" TQSL_VERSION_MAJOR ${TQSLVERSION})
	string(REGEX REPLACE "([0-9]+)\\.([0-9]+)" "\\2" TQSL_VERSION_MINOR ${TQSLVERSION})
	set(TQSL_VERSION_UPDATE "0")
endif()

#get build # with git

find_program(GIT_PROG git "C:\\Program Files (x86)\\Git\\bin" "C:\\Program Files\\Git\\bin")

if(GIT_PROG) #we can use git
	execute_process(COMMAND ${GIT_PROG} describe RESULT_VARIABLE GITERROR ERROR_QUIET OUTPUT_VARIABLE HEAD_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE)
	if(NOT GITERROR) #git ran alright
		set(BUILD ${HEAD_COMMIT})
	else()		# Git error - hope this is a source distro
		if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/apps/tqslbuild.h)
			file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/apps/tqslbuild.h BUILDSTR REGEX "\\[.*\\]")
			string(REGEX REPLACE ".*\\[(.*)\\].*" "\\1" BUILD ${BUILDSTR})
		endif()
	endif()
else()	# No git, again grab from source
	if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/apps/tqslbuild.h)
		file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/apps/tqslbuild.h BUILDSTR REGEX "\\[.*\\]")
		string(REGEX REPLACE ".*\\[(.*)\\].*" "\\1" BUILD ${BUILDSTR})
	endif()
endif()

# If BUILD is set (we have a version) and BUILDSTR is not (we got that build from
# git), then write a tqslbuild.h. Else leave it alone.
if(BUILD AND NOT BUILDSTR)
	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/apps/tqslbuild.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/apps/tqslbuild.h")
else() #hope this is a source distribution, which will always include a correct tqslbuild
	if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/apps/tqslbuild.h")
		#but otherwise someone downloaded a snapshot without a buildfile
		set(BUILD "unknown")
		configure_file("${CMAKE_CURRENT_SOURCE_DIR}/apps/tqslbuild.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/apps/tqslbuild.h")
	endif()
endif()

# In case the versions couldn't be deduced, set them
if(NOT ${TQSL_VERSION_MAJOR})
	set(TQSL_VERSION_MAJOR 2)
endif()

if(NOT TQSL_VERSION_MINOR)
	set(TQSL_VERSION_MINOR 1)
endif()

if (NOT TQSL_VERSION_UPDATE)
	set(TQSL_VERSION_UPDATE 0)
endif()

IF(TQSL_VERSION_UPDATE AND NOT TQSL_VERSION_UPDATE EQUAL 0) 
	set(TQSLVERSION "${TQSL_VERSION_MAJOR}.${TQSL_VERSION_MINOR}.${TQSL_VERSION_UPDATE}")
else()
	set(TQSLVERSION "${TQSL_VERSION_MAJOR}.${TQSL_VERSION_MINOR}")
endif()
#
add_subdirectory(src) #tqsllib
add_subdirectory(apps)
macro_optional_add_subdirectory(tests)
