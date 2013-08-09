/*
 * Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <deque>

#include "ppapi/c/private/ppb_nacl_private.h"
#include "ppapi/cpp/module.h"

namespace plugin {

class ModulePpapi : public pp::Module {
 public:
  ModulePpapi();

  virtual ~ModulePpapi();

  virtual bool Init();

  virtual pp::Instance* CreateInstance(PP_Instance pp_instance);

  // NaCl crash throttling.  If RegisterPluginCrash is called too many times
  // within a time period, IsPluginUnstable reports true.  As long as
  // IsPluginUnstable returns true, NaCl modules will fail to load.
  void RegisterPluginCrash();
  bool IsPluginUnstable();

 private:
  bool init_was_successful_;
  const PPB_NaCl_Private* private_interface_;

  // Crash throttling support.
  std::deque<int64_t> crash_times_;
};

}  // namespace plugin


namespace pp {

Module* CreateModule();

}  // namespace pp
