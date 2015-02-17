#pragma once


// - - - - - - - VERSION - - - - - - -

#define PICXX_VERSION_MAJOR 1
#define PICXX_VERSION_MINOR 0

#define PICXX_MAKEVERSION( major, minor )  (major << 8) | minor

#define PICXX_VERSION PICXX_MAKEVERSION( PICXX_VERSION_MAJOR, PICXX_VERSION_MINOR )


// - - - - - - - PYTHON - - - - - - -

// pull in python definitions
#include "Python.h"

#ifndef PY_MAJOR_VERSION
#   error not defined PY_MAJOR_VERSION
#endif

// before 3.2 Py_hash_t was missing
#if ( 1000 * PY_MAJOR_VERSION + PY_MINOR_VERSION  <  3002 )
typedef long int Py_hash_t;
#endif

// On some platforms we have to include time.h to get select defined
#if !defined(__WIN32__) && !defined(WIN32) && !defined(_WIN32) && !defined(_WIN64)
#   include <sys/time.h>
#endif

// Prevent multiple conflicting definitions of swab from stdlib.h and unistd.h
#if defined(__sun) || defined(sun)
#   if defined(_XPG4)
#       undef _XPG4
#   endif
#endif

// Python.h will redefine these and generate warning in the process
#undef _XOPEN_SOURCE
#undef _POSIX_C_SOURCE

// fix issue with Python assuming that isspace, toupper etc are macros
#if defined(isspace)
#   undef isspace
#   undef isupper
#   undef islower
#   undef isalnum
#   undef isalpha
#   undef toupper
#   undef tolower
#endif


