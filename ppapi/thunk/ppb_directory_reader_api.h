// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_DIRECTORY_READER_API_H_
#define PPAPI_THUNK_DIRECTORY_READER_API_H_

#include "base/memory/ref_counted.h"
#include "ppapi/c/dev/ppb_directory_reader_dev.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPB_DirectoryReader_API {
 public:
  virtual ~PPB_DirectoryReader_API() {}

  virtual int32_t GetNextEntry(PP_DirectoryEntry_Dev* entry,
                               scoped_refptr<TrackedCallback> callback) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_DIRECTORY_READER_API_H_
