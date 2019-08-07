//===- Errno.cpp - errno support --------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the errno wrappers.
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/Errno.h"
#include "llvm/Config/config.h"     // Get autoconf configuration settings

#if HAVE_STRING_H
#include <string.h>

#if HAVE_ERRNO_H
#include <errno.h>
#endif

//===----------------------------------------------------------------------===//
//=== WARNING: Implementation here must contain only TRULY operating system
//===          independent code.
//===----------------------------------------------------------------------===//

namespace llvm {
namespace sys {

#if HAVE_ERRNO_H
std::string StrError() {
  return StrError(errno);
}
#endif  // HAVE_ERRNO_H

std::string StrError(int errnum) {
  const int MaxErrStrLen = 2000;
  char buffer[MaxErrStrLen];
  buffer[0] = '\0';
  char* str = buffer;
#ifdef HAVE_STRERROR_R
  // strerror_r is thread-safe.
  if (errnum)
# if defined(__GLIBC__) && defined(_GNU_SOURCE)
    // glibc defines its own incompatible version of strerror_r
    // which may not use the buffer supplied.
    str = strerror_r(errnum,buffer,MaxErrStrLen-1);
# else
    strerror_r(errnum,buffer,MaxErrStrLen-1);
# endif
#elif HAVE_DECL_STRERROR_S // "Windows Secure API"
    if (errnum)
      strerror_s(buffer, errnum);
#elif defined(HAVE_STRERROR)
  // Copy the thread un-safe result of strerror into
  // the buffer as fast as possible to minimize impact
  // of collision of strerror in multiple threads.
  if (errnum)
    strncpy(buffer,strerror(errnum),MaxErrStrLen-1);
  buffer[MaxErrStrLen-1] = '\0';
#else
  // Strange that this system doesn't even have strerror
  // but, oh well, just use a generic message
  sprintf(buffer, "Error #%d", errnum);
#endif
  return str;
}

}  // namespace sys
}  // namespace llvm

#endif  // HAVE_STRING_H
