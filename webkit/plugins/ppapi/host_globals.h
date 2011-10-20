// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_HOST_GLOBALS_H_
#define WEBKIT_PLUGINS_PPAPI_HOST_GLOBALS_H_

#include "base/compiler_specific.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "webkit/plugins/ppapi/resource_tracker.h"

namespace webkit {
namespace ppapi {

class HostGlobals : public ::ppapi::PpapiGlobals {
 public:
  HostGlobals();
  virtual ~HostGlobals();

  // Getter for the global singleton. Generally, you should use
  // PpapiGlobals::Get() when possible. Use this only when you need some
  // host-specific functionality.
  inline static HostGlobals* Get() { return host_globals_; }

  // PpapiGlobals implementation.
  virtual ::ppapi::ResourceTracker* GetResourceTracker() OVERRIDE;
  virtual ::ppapi::VarTracker* GetVarTracker() OVERRIDE;

  ResourceTracker* host_resource_tracker() { return &host_resource_tracker_; }

 private:
  static HostGlobals* host_globals_;

  ResourceTracker host_resource_tracker_;
  ::ppapi::VarTracker host_var_tracker_;

  DISALLOW_COPY_AND_ASSIGN(HostGlobals);
};

}  // namespace ppapi
}  // namespace webkit

#endif   // WEBKIT_PLUGINS_PPAPI_HOST_GLOBALS_H_
