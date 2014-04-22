// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/file_manager/file_manager_resource_util.h"

#include "grit/file_manager_resources_map.h"
#include "ui/base/resource/resource_bundle.h"

namespace file_manager {

const GritResourceMap* GetFileManagerResources(size_t* size) {
  DCHECK(size);

  // TODO(yoshiki): Get the array and size from file_manager_resources_map.cc.
  // As for now, put the empty array and zero size manually, because the
  // file_manager_resources_map.cc can't be complied if the resource list is
  // empty.
  *size = 0;
  static const GritResourceMap kEmptyResourceList[] = {};
  return kEmptyResourceList;
}

}  // namespace keyboard
