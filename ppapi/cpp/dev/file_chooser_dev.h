// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_FILE_CHOOSER_DEV_H_
#define PPAPI_CPP_DEV_FILE_CHOOSER_DEV_H_

#include "ppapi/cpp/resource.h"

struct PP_FileChooserOptions_Dev;

namespace pp {

class CompletionCallback;
class FileRef;
class Instance;

class FileChooser_Dev : public Resource {
 public:
  // Creates an is_null() FileChooser object.
  FileChooser_Dev() {}

  FileChooser_Dev(const Instance& instance,
                  const PP_FileChooserOptions_Dev& options);

  FileChooser_Dev(const FileChooser_Dev& other);

  // PPB_FileChooser methods:
  int32_t Show(const CompletionCallback& cc);
  FileRef GetNextChosenFile() const;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_FILE_CHOOSER_DEV_H_
