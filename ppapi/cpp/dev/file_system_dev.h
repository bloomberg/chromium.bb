// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_FILE_SYSTEM_DEV_H_
#define PPAPI_CPP_DEV_FILE_SYSTEM_DEV_H_

#include "ppapi/c/dev/pp_file_info_dev.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/resource.h"

struct PP_FileInfo_Dev;

namespace pp {

class CompletionCallback;
class FileRef_Dev;

// Wraps methods from ppb_file_system.h
class FileSystem_Dev : public Resource {
 public:
  FileSystem_Dev(Instance* instance, PP_FileSystemType_Dev type);

  int32_t Open(int64_t expected_size, const CompletionCallback& cc);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_FILE_SYSTEM_DEV_H_
