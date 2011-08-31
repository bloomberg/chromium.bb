// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_FILE_SYSTEM_H_
#define PPAPI_CPP_FILE_SYSTEM_H_

#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/resource.h"

/// @file
/// This file defines the API to create a file system associated with a file.

struct PP_FileInfo;

namespace pp {

class CompletionCallback;
class FileRef;

/// The <code>FileSystem</code> class identifies the file system type
/// associated with a file.
class FileSystem : public Resource {
 public:

  /// This constructor creates a file system object of the given type.
  ///
  /// @param[in] instance A <code>Instance</code> identifying the instance
  /// with the file.
  /// @param[in] type A file system type as defined by
  /// <code>PP_FileSystemType</code> enum.
  FileSystem(Instance* instance, PP_FileSystemType type);

  /// Open() opens the file system. A file system must be opened before running
  /// any other operation on it.
  ///
  /// @param[in] expected_size The expected size of the file system.
  /// @param[in] cc A <code>PP_CompletionCallback</code> to be called upon
  /// completion of Open().
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  int32_t Open(int64_t expected_size, const CompletionCallback& cc);
};

}  // namespace pp

#endif  // PPAPI_CPP_FILE_SYSTEM_H_
