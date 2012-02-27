// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_FILE_IO_H_
#define PPAPI_CPP_FILE_IO_H_

#include "ppapi/c/pp_time.h"
#include "ppapi/cpp/resource.h"

/// @file
/// This file defines the API to create a file i/o object.

struct PP_FileInfo;

namespace pp {

class CompletionCallback;
class FileRef;
class InstanceHandle;

/// The <code>FileIO</code> class represents a regular file.
class FileIO : public Resource {
 public:
  /// Default constructor for creating an is_null() <code>FileIO</code>
  /// object.
  FileIO();

  /// A constructor used to create a <code>FileIO</code> and associate it with
  /// the provided <code>Instance</code>.
  ///
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  explicit FileIO(const InstanceHandle& instance);

  /// The copy constructor for <code>FileIO</code>.
  ///
  /// @param[in] other A pointer to a <code>FileIO</code>.
  FileIO(const FileIO& other);

  /// Open() opens the specified regular file for I/O according to the given
  /// open flags, which is a bit-mask of the PP_FileOpenFlags values.  Upon
  /// success, the corresponding file is classified as "in use" by this FileIO
  /// object until such time as the FileIO object is closed or destroyed.
  ///
  /// @param[in] file_ref A <code>PP_Resource</code> corresponding to a file
  /// reference.
  /// @param[in] open_flags A bit-mask of the <code>PP_FileOpenFlags</code>
  /// values.
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of Open().
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  int32_t Open(const FileRef& file_ref,
               int32_t open_flags,
               const CompletionCallback& cc);

  /// Query() queries info about the file opened by this FileIO object. This
  /// function will fail if the FileIO object has not been opened.
  ///
  /// @param[in] result_buf The <code>PP_FileInfo</code> structure representing
  /// all information about the file.
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of Query().
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  int32_t Query(PP_FileInfo* result_buf,
                const CompletionCallback& cc);

  /// Touch() Updates time stamps for the file opened by this FileIO object.
  /// This function will fail if the FileIO object has not been opened.
  ///
  /// @param[in] last_access_time The last time the FileIO was accessed.
  /// @param[in] last_modified_time The last time the FileIO was modified.
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of Touch().
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  int32_t Touch(PP_Time last_access_time,
                PP_Time last_modified_time,
                const CompletionCallback& cc);

  /// Read() reads from an offset in the file.  The size of the buffer must be
  /// large enough to hold the specified number of bytes to read.  This
  /// function might perform a partial read.
  ///
  /// @param[in] offset The offset into the file.
  /// @param[in] buffer The buffer to hold the specified number of bytes read.
  /// @param[in] bytes_to_read The number of bytes to read from
  /// <code>offset</code>.
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of Read().
  ///
  /// @return An The number of bytes read an error code from
  /// <code>pp_errors.h</code>. If the return value is 0, then end-of-file was
  /// reached. It is valid to call Read() multiple times with a completion
  /// callback to queue up parallel reads from the file at different offsets.
  int32_t Read(int64_t offset,
               char* buffer,
               int32_t bytes_to_read,
               const CompletionCallback& cc);

  /// Write() writes to an offset in the file.  This function might perform a
  /// partial write. The FileIO object must have been opened with write access.
  ///
  /// @param[in] offset The offset into the file.
  /// @param[in] buffer The buffer to hold the specified number of bytes read.
  /// @param[in] bytes_to_write The number of bytes to write to
  /// <code>offset</code>.
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of Write().
  ///
  /// @return An The number of bytes written or an error code from
  /// <code>pp_errors.h</code>. If the return value is 0, then end-of-file was
  /// reached. It is valid to call Write() multiple times with a completion
  /// callback to queue up parallel writes to the file at different offsets.
  int32_t Write(int64_t offset,
                const char* buffer,
                int32_t bytes_to_write,
                const CompletionCallback& cc);

  /// SetLength() sets the length of the file.  If the file size is extended,
  /// then the extended area of the file is zero-filled.  The FileIO object must
  /// have been opened with write access.
  ///
  /// @param[in] length The length of the file to be set.
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of SetLength().
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  int32_t SetLength(int64_t length,
                    const CompletionCallback& cc);

  /// Flush() flushes changes to disk.  This call can be very expensive!
  ///
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of Flush().
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  int32_t Flush(const CompletionCallback& cc);

  /// Close() cancels any IO that may be pending, and closes the FileIO object.
  /// Any pending callbacks will still run, reporting
  /// <code>PP_ERROR_ABORTED</code> if pending IO was interrupted.  It is not
  /// valid to call Open() again after a call to this method.
  ///
  /// <strong>Note:</strong> If the FileIO object is destroyed, and it is still
  /// open, then it will be implicitly closed, so you are not required to call
  /// Close().
  void Close();
};

}  // namespace pp

#endif  // PPAPI_CPP_FILE_IO_H_
