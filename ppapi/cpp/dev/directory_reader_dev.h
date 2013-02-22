// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_DIRECTORY_READER_DEV_H_
#define PPAPI_CPP_DEV_DIRECTORY_READER_DEV_H_

#include <vector>

#include "ppapi/c/dev/ppb_directory_reader_dev.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class DirectoryEntry_Dev;
class FileRef;
template<typename T> class CompletionCallbackWithOutput;

class DirectoryReader_Dev : public Resource {
 public:
  /// A constructor that creates a DirectoryReader resource for the given
  /// directory.
  ///
  /// @param[in] directory_ref A <code>PP_Resource</code> corresponding to the
  /// directory reference to be read.
  explicit DirectoryReader_Dev(const FileRef& directory_ref);

  DirectoryReader_Dev(const DirectoryReader_Dev& other);

  /// ReadEntries() Reads all entries in the directory.
  ///
  /// @param[in] cc A <code>CompletionCallbackWithOutput</code> to be called
  /// upon completion of ReadEntries(). On success, the directory entries will
  /// be passed to the given function.
  ///
  /// Normally you would use a CompletionCallbackFactory to allow callbacks to
  /// be bound to your class. See completion_callback_factory.h for more
  /// discussion on how to use this. Your callback will generally look like:
  ///
  /// @code
  ///   void OnReadEntries(int32_t result,
  ///                      const std::vector<DirectoryEntry_Dev>& entries) {
  ///     if (result == PP_OK)
  ///       // use entries...
  ///   }
  /// @endcode
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  int32_t ReadEntries(
      const CompletionCallbackWithOutput< std::vector<DirectoryEntry_Dev> >&
          callback);
};

}  // namespace pp

#endif  // PPAPI_CPP_DIRECTORY_READER_H_
