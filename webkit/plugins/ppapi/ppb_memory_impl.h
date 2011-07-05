// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_MEMORY_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_MEMORY_IMPL_H_

struct PPB_Memory_Dev;

namespace webkit {
namespace ppapi {

class PPB_Memory_Impl {
 public:
  // Returns a pointer to the interface implementing PPB_Memory_Dev that is
  // exposed to the plugin.
  static const PPB_Memory_Dev* GetInterface();
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_MEMORY_IMPL_H_
