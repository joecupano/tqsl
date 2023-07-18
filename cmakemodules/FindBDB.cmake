# Find Berkeley DB

# Look for the header file.
if(APPLE OR CMAKE_HOST_SYSTEM_NAME STREQUAL "FreeBSD") #exclude obsolete default db
FIND_PATH(BDB_INCLUDE_DIR db.h NO_DEFAULT_PATH PATHS
  "/usr/local/BerkeleyDB.6.0/include"
  "/usr/local/BerkeleyDB.5.3/include"
  "/usr/local/include/db6"
  "/usr/local/include/db5"
  "${BDB_PREFIX}/include/db6"
  "${BDB_PREFIX}/include/db5"
  "${BDB_PREFIX}/include"
) 
else()
FIND_PATH(BDB_INCLUDE_DIR db.h
  #windows dirs (to be determined)
  "C:\\db\\build_windows"
  #*nix dirs
  "${BDB_PREFIX}/include"
  "${BDB_PREFIX}/include/db6"
  "${BDB_PREFIX}/include/db5"
)
endif()
MARK_AS_ADVANCED(BDB_INCLUDE_DIR)

# Look for the library.
FIND_LIBRARY(BDB_LIBRARY NAMES 
  db
  #windows build products
  #prefer small version
  libdb_small60s
  libdb60s
  libdb_small61s
  libdb61s
  libdb6-0.3
  libdb6-0.so
  libdb_small53s
  libdb5
  libdb53s
  libdb5-5.3
  libdb5-5.so
  libdb5
  PATHS
  "C:\\db\\build_windows\\Win32\\Static Release\\"
  "C:\\db\\build_windows\\Win32\\Static_Release\\"	#vc08 adds underscore
  #OSX (and probably other unix) src build
  "/usr/local/BerkeleyDB.6.0/lib"
  "/usr/local/BerkeleyDB.5.3/lib"
  "/usr/local/lib/db6"
  "${BDB_PREFIX}/lib"
  "${BDB_PREFIX}/lib/db6"
  "${BDB_PREFIX}/lib/db5"
  "${BDB_PREFIX}"
)
MARK_AS_ADVANCED(BDB_LIBRARY)

# handle the QUIETLY and REQUIRED arguments and set CURL_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(BDB DEFAULT_MSG BDB_LIBRARY BDB_INCLUDE_DIR)

IF(BDB_FOUND)
  SET(BDB_LIBRARIES ${BDB_LIBRARY})
  SET(BDB_INCLUDE_DIRS ${BDB_INCLUDE_DIR})
ENDIF(BDB_FOUND)
