/*
  Copyright (c) 2011 The Native Client Authors. All rights reserved.
  Use of this source code is governed by a BSD-style license that can be
  found in the LICENSE file.
*/

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <list>
#include <map>
#include <new>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_nacl_file.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_core.h"
#include "native_client/src/untrusted/ppapi/nacl_file.h"
#include "native_client/src/third_party/ppapi/c/pp_bool.h"
#include "native_client/src/third_party/ppapi/c/pp_errors.h"
#include "native_client/src/third_party/ppapi/c/pp_instance.h"
#include "native_client/src/third_party/ppapi/c/ppb_core.h"

// TODO(nfullagar): Currently, files loaded with LoadUrl() cannot be unloaded;
//                  nacl_file holds the real descriptor open for the duration
//                  of the nexe. If needed, add a way to unload URLs, remove the
//                  entry from our internal list and close the real descriptor.
// TODO(nfullagar): Finer grained locking for better performance.
// TODO(nfullagar): Use dup() to manufacture unique descriptors.

using ppapi_proxy::MayForceCallback;

namespace {

// Max number of open fake descriptors allowed.
const int kNaClFileDescriptorMaxOpen = 1024;
// Bias to add to returned fake descriptors; used to distinguish from real
// descriptors.
const int kNaClFileDescriptorBias = 10000;

// Overall status of a FileEntry.
enum EntryStatus {
  kStatusInprogress,
  kStatusReady,
  kStatusFailed
};

// Each call to LoadUrl() will add one FileEntry to the list of files.
struct FileEntry {
  PP_Instance instance;
  PP_CompletionCallback callback;    // user supplied callback
  char* url;                         // name of url (or filename) from LoadUrl()
  int real_descriptor;               // shared descriptor of open file
  pthread_mutex_t completion_mutex;  // locked: LoadUrl() to completion callback
  EntryStatus load_status;
};

// Multiple fake descriptors map to a single open file.  Because this open
// file is shared, we must track a file offset for each fake descriptor.
struct IndirectFileEntry {
  FileEntry* entry;
  off_t offset;
};

// Global mutex for general purpose nacl_file thread safety.
pthread_mutex_t global_nacl_file_mutex = PTHREAD_MUTEX_INITIALIZER;
// List of files, constructed with calls to LoadUrl().
std::list<FileEntry*> global_real_files;
// Table mapping fake descriptors to FileEntry.
IndirectFileEntry global_nacl_files[kNaClFileDescriptorMaxOpen];

// Initialize global indirect table
void InitializeIndirectFileEntries() {
  for (int i = 0; i < kNaClFileDescriptorMaxOpen; ++i) {
    global_nacl_files[i].entry = NULL;
    global_nacl_files[i].offset = 0;
  }
}

// Internal completion callback used by LoadUrl().
void UrlDidLoad(void* user_data, int32_t pp_error) {
  FileEntry* entry = reinterpret_cast<FileEntry*>(user_data);
  if (PP_OK == pp_error) {
    int real_descriptor = ppapi_proxy::GetFileDesc(entry->instance, entry->url);
    pthread_mutex_lock(&global_nacl_file_mutex);
    entry->real_descriptor = real_descriptor;
    entry->load_status = kStatusReady;
    pthread_mutex_unlock(&global_nacl_file_mutex);
  } else {
    // Leave ourselves in the list and mark as failed.
    pthread_mutex_lock(&global_nacl_file_mutex);
    entry->load_status = kStatusFailed;
    pthread_mutex_unlock(&global_nacl_file_mutex);
  }
  // Other threads waiting inside __wrap_open on this lock will proceed.
  pthread_mutex_unlock(&entry->completion_mutex);
  PP_RunCompletionCallback(&entry->callback, pp_error);
  // TODO(nfullagar): Failed files should be removed from the file list...
}
}  // namespace

int32_t LoadUrl(PP_Instance instance,
                const char* url,
                PP_CompletionCallback callback) {
  // TODO(nfullagar): Support calling from any thread.  Currenty LoadUrl()
  // must be called from the main PPAPI thread.
  if (PP_FALSE == ppapi_proxy::PPBCoreInterface()->IsMainThread()) {
    NACL_UNTESTED();
    return MayForceCallback(callback, PP_ERROR_FAILED);
  }
  // TODO(nfullagar): Add support for blocking version.
  if (NULL == callback.func)
    return MayForceCallback(callback, PP_ERROR_BADARGUMENT);

  FileEntry* entry = new(std::nothrow) FileEntry;
  if (NULL == entry)
    return MayForceCallback(callback, PP_ERROR_NOMEMORY);
  entry->instance = instance;
  entry->url = strdup(url);
  if (NULL == entry->url) {
    delete entry;
    return MayForceCallback(callback, PP_ERROR_NOMEMORY);
  }
  entry->callback = callback;
  entry->load_status = kStatusInprogress;
  entry->real_descriptor = NACL_NO_FILE_DESC;
  // Take the lock; when the file finishes loading, the lock will be lifted.
  // While locked, __wrap_open will block when called off the main thread.
  pthread_mutex_init(&entry->completion_mutex, NULL);
  pthread_mutex_lock(&entry->completion_mutex);

  pthread_mutex_lock(&global_nacl_file_mutex);
  global_real_files.push_front(entry);
  pthread_mutex_unlock(&global_nacl_file_mutex);

  PP_CompletionCallback loaded = PP_MakeCompletionCallback(UrlDidLoad, entry);
  loaded.flags = callback.flags;
  int32_t pp_error = ppapi_proxy::StreamAsFile(instance, url, loaded);
  CHECK(pp_error != PP_OK);  // Never succeeds synchronously.
  return MayForceCallback(loaded, pp_error);  // loaded runs callback.
}

// Generally, __wrap_open(), __wrap_read(), __wrap_lseek(), __wrap_close() can
// be used from any thread.  They might block in a nacl syscall, but will not
// attempt to use any PPAPI calls other than PPB_Core::IsMainThread().
// Please see nacl_file.h for more details.
int __wrap_open(char const* pathname, int flags, ...) {
  // Look through the list created so far by LoadUrl()
  pthread_mutex_lock(&global_nacl_file_mutex);
  FileEntry* entry = NULL;
  int real_descriptor = NACL_NO_FILE_DESC;
  EntryStatus load_status = kStatusFailed;
  std::list<FileEntry*>::iterator entry_itr = global_real_files.begin();
  while (entry_itr != global_real_files.end()) {
    if (0 == strcmp((*entry_itr)->url, pathname)) {
      entry = *entry_itr;
      real_descriptor = entry->real_descriptor;
      load_status = entry->load_status;
      break;
    }
    entry_itr++;
  }
  pthread_mutex_unlock(&global_nacl_file_mutex);

  // If the entry isn't in the list, try __real_open().  This might be useful
  // for running a nexe in debug mode, which may allow access to the filesystem.
  if (NULL == entry) {
    int mode = 0;
    if (O_CREAT & flags) {
      va_list va;
      va_start(va, flags);
      mode = va_arg(va, int);
      va_end(va);
    }
    return __real_open(pathname, flags, mode);
  }

  // nacl_file only supports read-only file ops.
  if (O_RDONLY != flags) {
    errno = EACCES;
    return NACL_NO_FILE_DESC;
  }
  if (kStatusInprogress == load_status) {
    // Please see comments in nacl_file.h regarding threading.
    if (PP_TRUE == ppapi_proxy::PPBCoreInterface()->IsMainThread()) {
      errno = EDEADLK;
      return NACL_NO_FILE_DESC;
    } else {
      // ...we're not on the main thread; take the lock and release it.
      // If the file hasn't finished fetching, the lock will stall until the
      // load completes. It is okay for open() to block when not on the main
      // thread.
      pthread_mutex_lock(&entry->completion_mutex);
      pthread_mutex_unlock(&entry->completion_mutex);
    }
  }

  // Look for an available file slot and return one if available.
  pthread_mutex_lock(&global_nacl_file_mutex);
  int nacl_file_descriptor = NACL_NO_FILE_DESC;
  // If entry load_status is not ready, a serious failure occurred.
  if (kStatusReady != entry->load_status) {
    errno = EIO;
  } else {
    static bool nacl_file_table_initialized = false;
    if (!nacl_file_table_initialized) {
      InitializeIndirectFileEntries();
      nacl_file_table_initialized = true;
    }
    for (int i = 0; i < kNaClFileDescriptorMaxOpen; ++i) {
      if (NULL == global_nacl_files[i].entry) {
        global_nacl_files[i].entry = entry;
        global_nacl_files[i].offset = 0;
        nacl_file_descriptor = i + kNaClFileDescriptorBias;
        break;
      }
    }
  }
  pthread_mutex_unlock(&global_nacl_file_mutex);
  // If no free entry is available, set errno to EMFILE (too many open files.)
  if (NACL_NO_FILE_DESC == nacl_file_descriptor)
    errno = EMFILE;
  return nacl_file_descriptor;
}

int __wrap_close(int descriptor) {
  if (descriptor < kNaClFileDescriptorBias)  // not owned by nacl_file
    return __real_close(descriptor);

  int index = descriptor - kNaClFileDescriptorBias;
  int ret = -1;
  pthread_mutex_lock(&global_nacl_file_mutex);
  if ((index >= kNaClFileDescriptorMaxOpen) ||
      (NULL == global_nacl_files[index].entry)) {
    errno = EBADF;
  } else {
    // Mark entry as closed by setting it to NULL.
    global_nacl_files[index].entry = NULL;
    ret = 0;
  }
  pthread_mutex_unlock(&global_nacl_file_mutex);
  return ret;
}

int __wrap_read(int descriptor, void* buf, size_t count) {
  if (descriptor < kNaClFileDescriptorBias)  // not owned by nacl_file
    return __real_read(descriptor, buf, count);

  int index = descriptor - kNaClFileDescriptorBias;
  int bytes_read = -1;
  pthread_mutex_lock(&global_nacl_file_mutex);
  if ((index >= kNaClFileDescriptorMaxOpen) ||
      (NULL == global_nacl_files[index].entry)) {
    errno = EBADF;
  } else {
    // lseek to position, read, and update fake descriptor's file offset.
    int real_fd = global_nacl_files[index].entry->real_descriptor;
    off_t offset = global_nacl_files[index].offset;
    if (-1 != __real_lseek(real_fd, offset, SEEK_SET)) {
      bytes_read = __real_read(real_fd, buf, count);
      if (bytes_read > 0)
        global_nacl_files[index].offset += bytes_read;
    }
  }
  pthread_mutex_unlock(&global_nacl_file_mutex);
  return bytes_read;
}

off_t __wrap_lseek(int descriptor, off_t offset, int whence) {
  if (descriptor < kNaClFileDescriptorBias)  // not owned by nacl_file
    return __real_lseek(descriptor, offset, whence);

  int index = descriptor - kNaClFileDescriptorBias;
  pthread_mutex_lock(&global_nacl_file_mutex);
  if ((index >= kNaClFileDescriptorMaxOpen) ||
      (NULL == global_nacl_files[index].entry)) {
    offset = -1;
    errno = EBADF;
  } else {
    int real_fd = global_nacl_files[index].entry->real_descriptor;
    switch (whence) {
      case SEEK_SET:
        offset = __real_lseek(real_fd, offset, SEEK_SET);
        break;
      case SEEK_CUR:
        offset = global_nacl_files[index].offset + offset;
        offset = __real_lseek(real_fd, offset, SEEK_SET);
        break;
      case SEEK_END:
        offset = __real_lseek(real_fd, offset, SEEK_END);
        break;
      default:
        offset = -1;
        errno = EINVAL;
        break;
    }
    if (offset != -1)
      global_nacl_files[index].offset = offset;
  }
  pthread_mutex_unlock(&global_nacl_file_mutex);
  return offset;
}
