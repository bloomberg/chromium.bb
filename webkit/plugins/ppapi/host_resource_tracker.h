// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_HOST_RESOURCE_TRACKER_H_
#define WEBKIT_PLUGINS_PPAPI_HOST_RESOURCE_TRACKER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/shared_impl/resource_tracker.h"

namespace webkit {
namespace ppapi {

class HostResourceTracker : public ::ppapi::ResourceTracker {
 public:
  HostResourceTracker();
  virtual ~HostResourceTracker();

  // ppapi::ResourceTracker overrides.
  virtual void LastPluginRefWasDeleted(::ppapi::Resource* object) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostResourceTracker);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_HOST_RESOURCE_TRACKER_H_
