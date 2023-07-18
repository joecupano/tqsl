#ifndef WINSTRDEFS_H
#define WINSTRDEFS_H

#if defined(_WIN32) || defined(_WIN64)
  #define snprintf _snprintf
  #define vsnprintf _vsnprintf
  #define strcasecmp _stricmp
  #define strncasecmp _strnicmp
  #define unlink _unlink
  #define strdup _strdup
  #define fileno _fileno
#endif

#endif//WINSTRDEFS_H
