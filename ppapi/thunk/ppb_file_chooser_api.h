// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_FILE_CHOOSER_API_H_
#define PPAPI_THUNK_PPB_FILE_CHOOSER_API_H_

#include "ppapi/c/dev/ppb_file_chooser_dev.h"

namespace ppapi {
namespace thunk {

class PPB_FileChooser_API {
 public:
  virtual ~PPB_FileChooser_API() {}

  virtual int32_t Show(const PP_CompletionCallback& callback) = 0;
  virtual PP_Resource GetNextChosenFile() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_FILE_CHOOSER_API_H_
