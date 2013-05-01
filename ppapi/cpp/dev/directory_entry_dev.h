// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_DIRECTORY_ENTRY_DEV_H_
#define PPAPI_CPP_DEV_DIRECTORY_ENTRY_DEV_H_

#include <vector>

#include "ppapi/c/dev/ppb_directory_reader_dev.h"
#include "ppapi/c/pp_array_output.h"
#include "ppapi/cpp/array_output.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/pass_ref.h"
#include "ppapi/cpp/output_traits.h"

namespace pp {

class DirectoryEntry_Dev {
 public:
  DirectoryEntry_Dev();
  DirectoryEntry_Dev(PassRef, const PP_DirectoryEntry_Dev& data);
  DirectoryEntry_Dev(const DirectoryEntry_Dev& other);

  ~DirectoryEntry_Dev();

  DirectoryEntry_Dev& operator=(const DirectoryEntry_Dev& other);

  // Returns true if the DirectoryEntry is invalid or uninitialized.
  bool is_null() const { return !data_.file_ref; }

  // Returns the FileRef held by this DirectoryEntry.
  FileRef file_ref() const { return FileRef(data_.file_ref); }

  // Returns the type of the file referenced by this DirectoryEntry.
  PP_FileType file_type() const { return data_.file_type; }

 private:
  PP_DirectoryEntry_Dev data_;
};

namespace internal {

class DirectoryEntryArrayOutputAdapterWithStorage
    : public ArrayOutputAdapter<PP_DirectoryEntry_Dev> {
 public:
  DirectoryEntryArrayOutputAdapterWithStorage();
  virtual ~DirectoryEntryArrayOutputAdapterWithStorage();

  // Returns the final array of resource objects, converting the
  // PP_DirectoryEntry_Dev written by the browser to pp::DirectoryEntry_Dev
  // objects.
  //
  // This function should only be called once or we would end up converting
  // the array more than once, which would mess up the refcounting.
  std::vector<DirectoryEntry_Dev>& output();

 private:
  // The browser will write the PP_DirectoryEntry_Devs into this array.
  std::vector<PP_DirectoryEntry_Dev> temp_storage_;

  // When asked for the output, the PP_DirectoryEntry_Devs above will be
  // converted to the pp::DirectoryEntry_Devs in this array for passing to the
  // calling code.
  std::vector<DirectoryEntry_Dev> output_storage_;
};

// A specialization of CallbackOutputTraits to provide the callback system the
// information on how to handle vectors of pp::DirectoryEntry_Dev. This converts
// PP_DirectoryEntry_Dev to pp::DirectoryEntry_Dev when passing to the plugin.
template <>
struct CallbackOutputTraits< std::vector<DirectoryEntry_Dev> > {
  typedef PP_ArrayOutput APIArgType;
  typedef DirectoryEntryArrayOutputAdapterWithStorage StorageType;

  static inline APIArgType StorageToAPIArg(StorageType& t) {
    return t.pp_array_output();
  }

  static inline std::vector<DirectoryEntry_Dev>& StorageToPluginArg(
      StorageType& t) {
    return t.output();
  }
};

}  // namespace internal
}  // namespace pp

#endif  // PPAPI_CPP_DEV_DIRECTORY_ENTRY_DEV_H_
