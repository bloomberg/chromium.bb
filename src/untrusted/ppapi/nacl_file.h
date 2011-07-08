/*
  Copyright (c) 2011 The Native Client Authors. All rights reserved.
  Use of this source code is governed by a BSD-style license that can be
  found in the LICENSE file.
*/

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_PPAPI_NACL_FILE_H
#define NATIVE_CLIENT_SRC_UNTRUSTED_PPAPI_NACL_FILE_H

#include <stdarg.h>
#include <unistd.h>
#include "native_client/src/third_party/ppapi/c/pp_completion_callback.h"
#include "native_client/src/third_party/ppapi/c/pp_instance.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * Usage: nacl_file is an experimental wrapper that emulates basic read-only
 * CRT file I/O. It works by intercepting open(), close(), read(), and lseek(),
 * and returning fake descriptors that reference a cached file fetched via
 * LoadUrl. To use this library from an application, it must first load the
 * files from the main PPAPI thread using LoadUrl(). When the completion
 * callback fires, the file can then be opened, read, and closed as many times
 * as needed, up until kNaClFileDescMaxOpen. A fake descriptor handed out by
 * nacl_file will have the value kNaClFileDescriptorBias added to it, to help
 * distinguish it from a normal descriptor. Note: Please don't depend on this
 * property except as a debugging aid. Future versions of nacl_file may rely
 * on dup() to generate unique descriptors.
 *
 * For example, from the main PPAPI thread:
 *   ...
 *   PP_CompletionCallback cb = PP_MakeCompletionCallback(Loaded, NULL);
 *   int32_t result = LoadUrl(pp_instance, "readme.txt", cb);
 *   if (result != PP_OK_COMPLETIONPENDING)
 *     PP_RunCompletionCallback(&cb, result);
 *   ...
 *
 * void Loaded(void* user_data, int32_t result) {
 *    if (result == PP_OK) {
 *      int fd = open("readme.txt", O_RDONLY);
 *      if (fd != -1) {
 *        ...lseek(), read(), close()...
 *
 * Returns: LoadUrl(), on success, returns PP_OK_COMPLETIONPENDING to indicate
 * that the load is in progress.  When the load has completed, the user supplied
 * completion callback will be invoked.  If the load was successful, the result
 * value passed into the completion callback will be PP_OK, and it will then
 * be okay to perform open(), lseek(), read(), and close().
 * Otherwise, the result code will be an error, please see pp_error.h for more
 * details.
 *
 * There are some additional rules regarding LoadUrl().  First, LoadUrl() can
 * only be made from the main thread at the moment, and the completion callback
 * will always occur on the main thread. Next, if open() is attempted on the
 * main thread before the LoadUrl() completion callback fires, open() will
 * return failure and set errno to EDEADLK.  If open() is attempted on a thread
 * besides the main thread between the LoadUrl() and the completion callback,
 * open() will block until the completion callback fires.
 *
 * When linking against nacl_file, it is neccessary to include extra linker
 * options to splice in the interception of open(), close(), read(), and
 * lseek().  Use the linker's --wrap name option.
 *
 * nacl-g++ application.cc -lnacl_file ...
 *     -Wl,--wrap=open,--wrap=read,--wrap=lseek,--wrap=close
 *
 * Note that when intercepting and replacing these low level interfaces,
 * nacl_file also permits use of fopen and C++ streams.  It also allows linking
 * against pre-built libraries that attempt to do traditional file I/O.
 *
 * Optionally, an application could skip the linker magic, and directly call
 * __wrap_read() provided by the nacl_file library.  In this case, the
 * application should replace all use of read(...) with __wrap_read(...), and
 * then provide:
 *   int __real_read(int fd, void *buf, size_t count) {
 *     return read(fd, buf, count);
 *   }
 * ...and do the same for open, lseek and close to satisfy the linker.
 */
int32_t LoadUrl(PP_Instance instance,
                const char* url,
                struct PP_CompletionCallback callback);

/* See notes above for LoadUrl on how to use __wrap_ and __real_ */
int __real_open(char const* pathname, int mode, ...);
int __real_close(int descriptor);
int __real_read(int descriptor, void* buffer, size_t size);
off_t __real_lseek(int descriptor, off_t offset, int whence);
int __wrap_open(char const* pathname, int mode, ...);
int __wrap_close(int descriptor);
int __wrap_read(int descriptor, void* buffer, size_t size);
off_t __wrap_lseek(int descriptor, off_t offset, int whence);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  /*  NATIVE_CLIENT_SRC_UNTRUSTED_PPAPI_NACL_FILE_H */
