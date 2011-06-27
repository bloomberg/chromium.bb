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

struct PP_FileInfo;

namespace pp {

class CompletionCallback;
class FileRef;

// Wraps methods from ppb_file_system.h
class FileSystem : public Resource {
 public:
  FileSystem(Instance* instance, PP_FileSystemType type);

  int32_t Open(int64_t expected_size, const CompletionCallback& cc);
};

}  // namespace pp

#endif  // PPAPI_CPP_FILE_SYSTEM_H_
