// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_FILE_REF_H_
#define PPAPI_CPP_FILE_REF_H_

#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/var.h"

/// @file
/// This file defines the API to create a file reference or "weak pointer" to a
/// file in a file system.

namespace pp {

class FileSystem;
class CompletionCallback;
template <typename T> class CompletionCallbackWithOutput;

/// The <code>FileRef</code> class represents a "weak pointer" to a file in
/// a file system.
class FileRef : public Resource {
 public:
  /// Default constructor for creating an is_null() <code>FileRef</code>
  /// object.
  FileRef() {}

  /// A constructor used when you have an existing PP_Resource for a FileRef
  /// and which to create a C++ object that takes an additional reference to
  /// the resource.
  ///
  /// @param[in] resource A PP_Resource corresponding to file reference.
  explicit FileRef(PP_Resource resource);

  /// A constructor used when you have received a PP_Resource as a return
  /// value that has already been reference counted.
  ///
  /// @param[in] resource A PP_Resource corresponding to file reference.
  FileRef(PassRef, PP_Resource resource);

  /// A constructor that creates a weak pointer to a file in the given file
  /// system. File paths are POSIX style.
  ///
  /// @param[in] file_system A <code>FileSystem</code> corresponding to a file
  /// system typ.
  /// @param[in] path A path to the file.
  FileRef(const FileSystem& file_system, const char* path);

  /// The copy constructor for <code>FileRef</code>.
  ///
  /// @param[in] other A pointer to a <code>FileRef</code>.
  FileRef(const FileRef& other);

  /// GetFileSystemType() returns the type of the file system.
  ///
  /// @return A <code>PP_FileSystemType</code> with the file system type if
  /// valid or <code>PP_FILESYSTEMTYPE_INVALID</code> if the provided resource
  /// is not a valid file reference.
  PP_FileSystemType GetFileSystemType() const;

  /// GetName() returns the name of the file.
  ///
  /// @return A <code>Var</code> containing the name of the file.  The value
  /// returned by this function does not include any path components (such as
  /// the name of the parent directory, for example). It is just the name of the
  /// file. Use GetPath() to get the full file path.
  Var GetName() const;

  /// GetPath() returns the absolute path of the file.
  ///
  /// @return A <code>Var</code> containing the absolute path of the file.
  /// This function fails if the file system type is
  /// <code>PP_FileSystemType_External</code>.
  Var GetPath() const;

  /// GetParent() returns the parent directory of this file.  If
  /// <code>file_ref</code> points to the root of the filesystem, then the root
  /// is returned.
  ///
  /// @return A <code>FileRef</code> containing the parent directory of the
  /// file. This function fails if the file system type is
  /// <code>PP_FileSystemType_External</code>.
  FileRef GetParent() const;

  /// MakeDirectory() makes a new directory in the file system.  It is not
  /// valid to make a directory in the external file system.
  /// <strong>Note:</strong> Use MakeDirectoryIncludingAncestors() to create
  /// parent directories.
  ///
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of MakeDirectory().
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  /// Fails if the directory already exists.
  int32_t MakeDirectory(const CompletionCallback& cc);

  /// MakeDirectoryIncludingAncestors() makes a new directory in the file
  /// system as well as any parent directories. It is not valid to make a
  /// directory in the external file system.
  ///
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of MakeDirectoryIncludingAncestors().
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  /// Fails if the directory already exists.
  int32_t MakeDirectoryIncludingAncestors(const CompletionCallback& cc);

  /// Touch() Updates time stamps for a file.  You must have write access to the
  /// file if it exists in the external filesystem.
  ///
  /// @param[in] last_access_time The last time the file was accessed.
  /// @param[in] last_modified_time The last time the file was modified.
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of Touch().
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  int32_t Touch(PP_Time last_access_time,
                PP_Time last_modified_time,
                const CompletionCallback& cc);

  /// Delete() deletes a file or directory. If <code>file_ref</code> refers to
  /// a directory, then the directory must be empty. It is an error to delete a
  /// file or directory that is in use.  It is not valid to delete a file in
  /// the external file system.
  ///
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of Delete().
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  int32_t Delete(const CompletionCallback& cc);

  /// Rename() renames a file or directory. Argument <code>new_file_ref</code>
  /// must refer to files in the same file system as in this object. It is an
  /// error to rename a file or directory that is in use.  It is not valid to
  /// rename a file in the external file system.
  ///
  /// @param[in] new_file_ref A <code>FileRef</code> corresponding to a new
  /// file reference.
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of Rename().
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  int32_t Rename(const FileRef& new_file_ref, const CompletionCallback& cc);

  ///
  /// Query() queries info about a file or directory. You must have access to
  /// read this file or directory if it exists in the external filesystem.
  ///
  /// @param[in] callback A <code>CompletionCallbackWithOutput</code>
  /// to be called upon completion of Query().
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  int32_t Query(const CompletionCallbackWithOutput<PP_FileInfo>& callback);
};

}  // namespace pp

#endif  // PPAPI_CPP_FILE_REF_H_
