// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPAPI_UNITTEST_H_
#define WEBKIT_PLUGINS_PPAPI_PPAPI_UNITTEST_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"

namespace webkit {
namespace ppapi {

class MockPluginDelegate;
class PluginInstance;
class PluginModule;

class PpapiUnittest : public testing::Test,
                      public webkit::ppapi::PluginDelegate::ModuleLifetime {
 public:
  PpapiUnittest();
  virtual ~PpapiUnittest();

  virtual void SetUp();
  virtual void TearDown();

  MockPluginDelegate* delegate() { return delegate_.get(); }
  PluginModule* module() const { return module_.get(); }
  PluginInstance* instance() const { return instance_.get(); }

  // Provides access to the interfaces implemented by the test. The default one
  // implements PPP_INSTANCE.
  virtual const void* GetMockInterface(const char* interface_name) const;

  // Deletes the instance and module to simulate module shutdown.
  void ShutdownModule();

  // Sets the view size of the plugin instance.
  void SetViewSize(int width, int height) const;

 protected:
  virtual MockPluginDelegate* NewPluginDelegate();

 private:
  scoped_ptr<MockPluginDelegate> delegate_;

  // Note: module must be declared first since we want it to get destroyed last.
  scoped_refptr<PluginModule> module_;
  scoped_refptr<PluginInstance> instance_;

  // ModuleLifetime implementation.
  virtual void PluginModuleDead(PluginModule* dead_module);

  DISALLOW_COPY_AND_ASSIGN(PpapiUnittest);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPAPI_UNITTEST_H_
