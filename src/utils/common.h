/*
	bumo is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	bumo is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with bumo.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef UTILS_COMMON_H_
#define UTILS_COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <memory>
#include <functional>

#include <errno.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>

#include <string>
#include <string.h>
#include <list>
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <algorithm>


#ifndef NDEBUG
#include <assert.h>
#endif

#define MSG_NOSIGNAL_ 0
#define SIGHUP       1
#define SIGQUIT      3
#define SIGKILL      9

#ifdef WIN32

#include <winsock2.h>
#include <ws2tcpip.h>
#include <shlwapi.h>
#include <io.h>
#include <process.h>

#define fseek64  _fseeki64
#define ftell64  _ftelli64

#ifndef EXCLUDE_WINDOW_H
//#Include <windows.h>
#endif //EXCLUDE_WINDOW_H
typedef CRITICAL_SECTION pthread_mutex_t;

#define __func__ __FUNCTION__

#pragma warning(disable:4819)
#pragma warning(disable:4996)
#define snprintf _snprintf

#if _MSC_VER < 1500 // MSVC 2008
#define vsnprintf _vsnprintf
#endif
#else

#include <pthread.h>
#include <limits.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <ifaddrs.h>
#include <dirent.h>

#define ERROR_SUCCESS            0
#define ERROR_ALREADY_EXISTS     EEXIST
#define ERROR_NOT_READY          ENOENT
#define ERROR_ACCESS_DENIED      EPERM
#define ERROR_NO_DATA            ENOENT
#define ERROR_TIMEOUT            ETIMEDOUT
#define ERROR_CONNECTION_REFUSED ECONNREFUSED
#define ERROR_INVALID_PARAMETER  ERANGE
#define ERROR_NOT_SUPPORTED      ENODATA
#define ERROR_INTERNAL_ERROR     EFAULT
#define ERROR_IO_DEVICE          EIO

#define fseek64     fseeko
#define ftell64     ftello

#endif

#ifdef WIN32
#define FMT_I64 "%I64d"
#define FMT_I64_EX(fmt) "%" #fmt "I64d"
#define FMT_U64 "%I64u"
#define FMT_X64 "%I64X"
#define FMT_SIZE "%lu"
#else
#ifdef __x86_64__
#define FMT_I64 "%ld"
#define FMT_I64_EX(fmt) "%" #fmt "ld"
#define FMT_U64 "%lu"
#define FMT_X64 "%lX"
#define FMT_SIZE "%lu"
#else
#define FMT_I64 "%lld"
#define FMT_I64_EX(fmt) "%" #fmt "lld"
#define FMT_U64 "%llu"
#define FMT_X64 "%llX"
#define FMT_SIZE "%u"
#endif

#endif


namespace utils {

	using namespace std::placeholders; // for std::bind

	// It is not allowed to copy ctor and assign opt
#undef UTILS_DISALLOW_EVIL_CONSTRUCTORS
#define UTILS_DISALLOW_EVIL_CONSTRUCTORS(TypeName)    \
    TypeName(const TypeName&);                         \
    void operator=(const TypeName&)

	// Delete the object safely
#define SAFE_DELETE(p)        \
    if (NULL != p) {          \
        delete p;             \
        p = NULL;             \
	    }

	// Delete the object array safely
#define SAFE_DELETE_ARRAY(p)  \
    if (NULL != p) {          \
        delete []p;           \
        p = NULL;             \
	    }

#ifndef MIN
#define MIN(a,b) ((a)<(b)) ? (a) : (b)
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)) ? (a) : (b)
#endif

#define CHECK_ERROR_RET(func, ecode, ret) \
	if( func )\
		{\
	utils::set_error_code(ecode); \
	return ret; \
		}

#define DISALLOW_COPY_AND_ASSIGN(cls) private:\
	cls( const cls & );\
	cls & operator =( const cls & )
}

#ifndef WIN32
extern "C"
{
	void * __wrap_memcpy(void *dest, const void *src, size_t n);
}
#endif

//#Define DEBUG_NEW_ENABLE
//#Include "debug_new.h"

#endif 
