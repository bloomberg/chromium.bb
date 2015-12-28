// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/mojo/mojo_vfs.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "components/filesystem/public/interfaces/file.mojom.h"
#include "components/filesystem/public/interfaces/file_system.mojom.h"
#include "components/filesystem/public/interfaces/types.mojom.h"
#include "mojo/public/cpp/bindings/lib/template_util.h"
#include "mojo/util/capture_util.h"
#include "third_party/sqlite/sqlite3.h"

using mojo::Capture;

namespace sql {

sqlite3_vfs* GetParentVFS(sqlite3_vfs* mojo_vfs) {
  return static_cast<ScopedMojoFilesystemVFS*>(mojo_vfs->pAppData)->parent_;
}

filesystem::DirectoryPtr& GetRootDirectory(sqlite3_vfs* mojo_vfs) {
  return static_cast<ScopedMojoFilesystemVFS*>(mojo_vfs->pAppData)->
      root_directory_;
}

namespace {

// Implementation of the sqlite3 Mojo proxying vfs.
//
// This is a bunch of C callback objects which transparently proxy sqlite3's
// filesystem reads/writes over the mojo:filesystem service. The main
// entrypoint is sqlite3_mojovfs(), which proxies all the file open/delete/etc
// operations. mojo:filesystem has support for passing a raw file descriptor
// over the IPC barrier, and most of the implementation of sqlite3_io_methods
// is derived from the default sqlite3 unix VFS and operates on the raw file
// descriptors.

const int kMaxPathName = 512;

// A struct which extends the base sqlite3_file to also hold on to a file
// pipe. We reinterpret_cast our sqlite3_file structs to this struct
// instead. This is "safe" because this struct is really just a slab of
// malloced memory, of which we tell sqlite how large we want with szOsFile.
struct MojoVFSFile {
  // The "vtable" of our sqlite3_file "subclass".
  sqlite3_file base;

  // We keep an open pipe to the File object to keep it from cleaning itself
  // up.
  filesystem::FilePtr file_ptr;
};

filesystem::FilePtr& GetFSFile(sqlite3_file* vfs_file) {
  return reinterpret_cast<MojoVFSFile*>(vfs_file)->file_ptr;
}

int MojoVFSClose(sqlite3_file* file) {
  DVLOG(1) << "MojoVFSClose(*)";
  TRACE_EVENT0("sql", "MojoVFSClose");
  using filesystem::FilePtr;
  filesystem::FileError error = filesystem::FILE_ERROR_FAILED;
  // Must call File::Close explicitly instead of just deleting the file, since
  // otherwise we wouldn't have an object to wait on.
  GetFSFile(file)->Close(mojo::Capture(&error));
  GetFSFile(file).WaitForIncomingResponse();
  GetFSFile(file).~FilePtr();
  return SQLITE_OK;
}

int MojoVFSRead(sqlite3_file* sql_file,
                void* buffer,
                int size,
                sqlite3_int64 offset) {
  DVLOG(1) << "MojoVFSRead (" << size << " @ " << offset << ")";
  TRACE_EVENT0("sql", "MojoVFSRead");
  filesystem::FileError error = filesystem::FILE_ERROR_FAILED;
  mojo::Array<uint8_t> mojo_data;
  GetFSFile(sql_file)->Read(size, offset, filesystem::WHENCE_FROM_BEGIN,
                            Capture(&error, &mojo_data));
  GetFSFile(sql_file).WaitForIncomingResponse();

  if (error != filesystem::FILE_ERROR_OK) {
    // TODO(erg): Better implementation here.
    NOTIMPLEMENTED();
    return SQLITE_IOERR_READ;
  }

  if (mojo_data.size())
    memcpy(buffer, &mojo_data.front(), mojo_data.size());
  if (mojo_data.size() == static_cast<size_t>(size))
    return SQLITE_OK;

  // We didn't read the entire buffer. Fill the rest of the buffer with zeros.
  memset(reinterpret_cast<char*>(buffer) + mojo_data.size(), 0,
         size - mojo_data.size());

  return SQLITE_IOERR_SHORT_READ;
}

int MojoVFSWrite(sqlite3_file* sql_file,
                 const void* buffer,
                 int size,
                 sqlite_int64 offset) {
  DVLOG(1) << "MojoVFSWrite(*, " << size << ", " << offset << ")";
  TRACE_EVENT0("sql", "MojoVFSWrite");
  mojo::Array<uint8_t> mojo_data(size);
  memcpy(&mojo_data.front(), buffer, size);

  filesystem::FileError error = filesystem::FILE_ERROR_FAILED;
  uint32_t num_bytes_written = 0;
  GetFSFile(sql_file)->Write(std::move(mojo_data), offset,
                             filesystem::WHENCE_FROM_BEGIN,
                             Capture(&error, &num_bytes_written));
  GetFSFile(sql_file).WaitForIncomingResponse();
  if (error != filesystem::FILE_ERROR_OK) {
    // TODO(erg): Better implementation here.
    NOTIMPLEMENTED();
    return SQLITE_IOERR_WRITE;
  }
  if (num_bytes_written != static_cast<uint32_t>(size)) {
    NOTIMPLEMENTED();
    return SQLITE_IOERR_WRITE;
  }

  return SQLITE_OK;
}

int MojoVFSTruncate(sqlite3_file* sql_file, sqlite_int64 size) {
  DVLOG(1) << "MojoVFSTruncate(*, " << size << ")";
  TRACE_EVENT0("sql", "MojoVFSTruncate");
  filesystem::FileError error = filesystem::FILE_ERROR_FAILED;
  GetFSFile(sql_file)->Truncate(size, Capture(&error));
  GetFSFile(sql_file).WaitForIncomingResponse();
  if (error != filesystem::FILE_ERROR_OK) {
    // TODO(erg): Better implementation here.
    NOTIMPLEMENTED();
    return SQLITE_IOERR_TRUNCATE;
  }

  return SQLITE_OK;
}

int MojoVFSSync(sqlite3_file* sql_file, int flags) {
  DVLOG(1) << "MojoVFSSync(*, " << flags << ")";
  TRACE_EVENT0("sql", "MojoVFSSync");
  filesystem::FileError error = filesystem::FILE_ERROR_FAILED;
  GetFSFile(sql_file)->Flush(Capture(&error));
  GetFSFile(sql_file).WaitForIncomingResponse();
  if (error != filesystem::FILE_ERROR_OK) {
    // TODO(erg): Better implementation here.
    NOTIMPLEMENTED();
    return SQLITE_IOERR_FSYNC;
  }

  return SQLITE_OK;
}

int MojoVFSFileSize(sqlite3_file* sql_file, sqlite_int64* size) {
  DVLOG(1) << "MojoVFSFileSize(*)";
  TRACE_EVENT0("sql", "MojoVFSFileSize");

  filesystem::FileError err = filesystem::FILE_ERROR_FAILED;
  filesystem::FileInformationPtr file_info;
  GetFSFile(sql_file)->Stat(Capture(&err, &file_info));
  GetFSFile(sql_file).WaitForIncomingResponse();

  if (err != filesystem::FILE_ERROR_OK) {
    // TODO(erg): Better implementation here.
    NOTIMPLEMENTED();
    return SQLITE_IOERR_FSTAT;
  }

  *size = file_info->size;
  return SQLITE_OK;
}

// TODO(erg): The current base::File interface isn't sufficient to handle
// sqlite's locking primitives, which are done on byte ranges in the file. (See
// "File Locking Notes" in sqlite3.c.)
int MojoVFSLock(sqlite3_file* pFile, int eLock) {
  DVLOG(1) << "MojoVFSLock(*, " << eLock << ")";
  return SQLITE_OK;
}
int MojoVFSUnlock(sqlite3_file* pFile, int eLock) {
  DVLOG(1) << "MojoVFSUnlock(*, " << eLock << ")";
  return SQLITE_OK;
}
int MojoVFSCheckReservedLock(sqlite3_file* pFile, int* pResOut) {
  DVLOG(1) << "MojoVFSCheckReservedLock(*)";
  *pResOut = 0;
  return SQLITE_OK;
}

// TODO(erg): This is the minimal implementation to get a few tests passing;
// lots more needs to be done here.
int MojoVFSFileControl(sqlite3_file* pFile, int op, void* pArg) {
  DVLOG(1) << "MojoVFSFileControl(*, " << op << ", *)";
  if (op == SQLITE_FCNTL_PRAGMA) {
    // Returning NOTFOUND tells sqlite that we aren't doing any processing.
    return SQLITE_NOTFOUND;
  }

  return SQLITE_OK;
}

int MojoVFSSectorSize(sqlite3_file* pFile) {
  DVLOG(1) << "MojoVFSSectorSize(*)";
  // Use the default sector size.
  return 0;
}

int MojoVFSDeviceCharacteristics(sqlite3_file* pFile) {
  DVLOG(1) << "MojoVFSDeviceCharacteristics(*)";
  // TODO(erg): Figure out what to return here. (This function is super spammy,
  // so not leaving a NOTIMPLEMENTED().)
  return 0;
}

static sqlite3_io_methods mojo_vfs_io_methods = {
    1,                            /* iVersion */
    MojoVFSClose,                 /* xClose */
    MojoVFSRead,                  /* xRead */
    MojoVFSWrite,                 /* xWrite */
    MojoVFSTruncate,              /* xTruncate */
    MojoVFSSync,                  /* xSync */
    MojoVFSFileSize,              /* xFileSize */
    MojoVFSLock,                  /* xLock */
    MojoVFSUnlock,                /* xUnlock */
    MojoVFSCheckReservedLock,     /* xCheckReservedLock */
    MojoVFSFileControl,           /* xFileControl */
    MojoVFSSectorSize,            /* xSectorSize */
    MojoVFSDeviceCharacteristics, /* xDeviceCharacteristics */
};

int MojoVFSOpen(sqlite3_vfs* mojo_vfs,
                const char* name,
                sqlite3_file* file,
                int flags,
                int* pOutFlags) {
  DVLOG(1) << "MojoVFSOpen(*, " << name << ", *, " << flags << ")";
  TRACE_EVENT2("sql", "MojoVFSOpen",
               "name", name,
               "flags", flags);
  int open_flags = 0;
  if (flags & SQLITE_OPEN_EXCLUSIVE) {
    DCHECK(flags & SQLITE_OPEN_CREATE);
    open_flags = filesystem::kFlagCreate;
  } else if (flags & SQLITE_OPEN_CREATE) {
    DCHECK(flags & SQLITE_OPEN_READWRITE);
    open_flags = filesystem::kFlagOpenAlways;
  } else {
    open_flags = filesystem::kFlagOpen;
  }
  open_flags |= filesystem::kFlagRead;
  if (flags & SQLITE_OPEN_READWRITE)
    open_flags |= filesystem::kFlagWrite;
  if (flags & SQLITE_OPEN_DELETEONCLOSE)
    open_flags |= filesystem::kDeleteOnClose;

  mojo::String mojo_name;
  if (name) {
    // Don't let callers open the pattern of our temporary databases. When we
    // open with a null name and SQLITE_OPEN_DELETEONCLOSE, we unlink the
    // database after we open it. If we create a database here, close it
    // normally, and then open the same file through the other path, we could
    // delete the database.
    CHECK(strncmp("Temp_", name, 5) != 0);
    mojo_name = name;
  } else {
    DCHECK(flags & SQLITE_OPEN_DELETEONCLOSE);
    static int temp_number = 0;
    mojo_name = base::StringPrintf("Temp_%d.db", temp_number++);
  }

  // Grab the incoming file
  filesystem::FilePtr file_ptr;
  filesystem::FileError error = filesystem::FILE_ERROR_FAILED;
  GetRootDirectory(mojo_vfs)->OpenFile(mojo_name, GetProxy(&file_ptr),
                                       open_flags, Capture(&error));
  GetRootDirectory(mojo_vfs).WaitForIncomingResponse();
  if (error != filesystem::FILE_ERROR_OK) {
    // TODO(erg): Translate more of the mojo error codes into sqlite error
    // codes.
    return SQLITE_CANTOPEN;
  }

  // Set the method table so we can be closed (and run the manual dtor call to
  // match the following placement news).
  file->pMethods = &mojo_vfs_io_methods;

  // |file| is actually a malloced buffer of size szOsFile. This means that we
  // need to manually use placement new to construct the C++ object which owns
  // the pipe to our file.
  new (&GetFSFile(file)) filesystem::FilePtr(std::move(file_ptr));

  return SQLITE_OK;
}

int MojoVFSDelete(sqlite3_vfs* mojo_vfs, const char* filename, int sync_dir) {
  DVLOG(1) << "MojoVFSDelete(*, " << filename << ", " << sync_dir << ")";
  TRACE_EVENT2("sql", "MojoVFSDelete",
               "name", filename,
               "sync_dir", sync_dir);
  // TODO(erg): The default windows sqlite VFS has retry code to work around
  // antivirus software keeping files open. We'll probably have to do something
  // like that in the far future if we ever support Windows.
  filesystem::FileError error = filesystem::FILE_ERROR_FAILED;
  GetRootDirectory(mojo_vfs)->Delete(filename, 0, Capture(&error));
  GetRootDirectory(mojo_vfs).WaitForIncomingResponse();

  if (error == filesystem::FILE_ERROR_OK && sync_dir) {
    GetRootDirectory(mojo_vfs)->Flush(Capture(&error));
    GetRootDirectory(mojo_vfs).WaitForIncomingResponse();
  }

  return error == filesystem::FILE_ERROR_OK ? SQLITE_OK : SQLITE_IOERR_DELETE;
}

int MojoVFSAccess(sqlite3_vfs* mojo_vfs,
                  const char* filename,
                  int flags,
                  int* result) {
  DVLOG(1) << "MojoVFSAccess(*, " << filename << ", " << flags << ", *)";
  TRACE_EVENT2("sql", "MojoVFSAccess",
               "name", filename,
               "flags", flags);
  filesystem::FileError error = filesystem::FILE_ERROR_FAILED;

  if (flags == SQLITE_ACCESS_READWRITE || flags == SQLITE_ACCESS_READ) {
    bool is_writable = false;
    GetRootDirectory(mojo_vfs)
        ->IsWritable(filename, Capture(&error, &is_writable));
    GetRootDirectory(mojo_vfs).WaitForIncomingResponse();
    *result = is_writable;
    return SQLITE_OK;
  }

  if (flags == SQLITE_ACCESS_EXISTS) {
    bool exists = false;
    GetRootDirectory(mojo_vfs)->Exists(filename, Capture(&error, &exists));
    GetRootDirectory(mojo_vfs).WaitForIncomingResponse();
    *result = exists;
    return SQLITE_OK;
  }

  return SQLITE_IOERR;
}

int MojoVFSFullPathname(sqlite3_vfs* mojo_vfs,
                        const char* relative_path,
                        int absolute_path_size,
                        char* absolute_path) {
  // The sandboxed process doesn't need to know the absolute path of the file.
  sqlite3_snprintf(absolute_path_size, absolute_path, "%s", relative_path);
  return SQLITE_OK;
}

// Don't let SQLite dynamically load things. (If we are using the
// mojo:filesystem proxying VFS, then it's highly likely that we are sandboxed
// and that any attempt to dlopen() a shared object is folly.)
void* MojoVFSDlOpen(sqlite3_vfs*, const char*) {
  return 0;
}

void MojoVFSDlError(sqlite3_vfs*, int buf_size, char* error_msg) {
  sqlite3_snprintf(buf_size, error_msg, "Dynamic loading not supported");
}

void (*MojoVFSDlSym(sqlite3_vfs*, void*, const char*))(void) {
  return 0;
}

void MojoVFSDlClose(sqlite3_vfs*, void*) {
  return;
}

int MojoVFSRandomness(sqlite3_vfs* mojo_vfs, int size, char* out) {
  base::RandBytes(out, size);
  return size;
}

// Proxy the rest of the calls down to the OS specific handler.
int MojoVFSSleep(sqlite3_vfs* mojo_vfs, int micro) {
  return GetParentVFS(mojo_vfs)->xSleep(GetParentVFS(mojo_vfs), micro);
}

int MojoVFSCurrentTime(sqlite3_vfs* mojo_vfs, double* time) {
  return GetParentVFS(mojo_vfs)->xCurrentTime(GetParentVFS(mojo_vfs), time);
}

int MojoVFSGetLastError(sqlite3_vfs* mojo_vfs, int a, char* b) {
  return 0;
}

int MojoVFSCurrentTimeInt64(sqlite3_vfs* mojo_vfs, sqlite3_int64* out) {
  return GetParentVFS(mojo_vfs)->xCurrentTimeInt64(GetParentVFS(mojo_vfs), out);
}

static sqlite3_vfs mojo_vfs = {
    1,                      /* iVersion */
    sizeof(MojoVFSFile),    /* szOsFile */
    kMaxPathName,           /* mxPathname */
    0,                      /* pNext */
    "mojo",                 /* zName */
    0,                      /* pAppData */
    MojoVFSOpen,            /* xOpen */
    MojoVFSDelete,          /* xDelete */
    MojoVFSAccess,          /* xAccess */
    MojoVFSFullPathname,    /* xFullPathname */
    MojoVFSDlOpen,          /* xDlOpen */
    MojoVFSDlError,         /* xDlError */
    MojoVFSDlSym,           /* xDlSym */
    MojoVFSDlClose,         /* xDlClose */
    MojoVFSRandomness,      /* xRandomness */
    MojoVFSSleep,           /* xSleep */
    MojoVFSCurrentTime,     /* xCurrentTime */
    MojoVFSGetLastError,    /* xGetLastError */
    MojoVFSCurrentTimeInt64 /* xCurrentTimeInt64 */
};

}  // namespace

ScopedMojoFilesystemVFS::ScopedMojoFilesystemVFS(
    filesystem::DirectoryPtr root_directory)
    : parent_(sqlite3_vfs_find(NULL)),
      root_directory_(std::move(root_directory)) {
  CHECK(!mojo_vfs.pAppData);
  mojo_vfs.pAppData = this;
  mojo_vfs.mxPathname = parent_->mxPathname;

  CHECK(sqlite3_vfs_register(&mojo_vfs, 1) == SQLITE_OK);
}

ScopedMojoFilesystemVFS::~ScopedMojoFilesystemVFS() {
  CHECK(mojo_vfs.pAppData);
  mojo_vfs.pAppData = nullptr;

  CHECK(sqlite3_vfs_register(parent_, 1) == SQLITE_OK);
  CHECK(sqlite3_vfs_unregister(&mojo_vfs) == SQLITE_OK);
}

filesystem::DirectoryPtr& ScopedMojoFilesystemVFS::GetDirectory() {
  return root_directory_;
}

}  // namespace sql
