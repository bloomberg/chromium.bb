// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPAPI_UNITTEST_H_
#define WEBKIT_PLUGINS_PPAPI_PPAPI_UNITTEST_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webkit {
namespace ppapi {

class MockPluginDelegate;
class PluginInstance;
class PluginModule;

class PpapiUnittest : public testing::Test {
 public:
  PpapiUnittest();
  virtual ~PpapiUnittest();

  virtual void SetUp();
  virtual void TearDown();

  PluginModule* module() const { return module_.get(); }
  PluginInstance* instance() const { return instance_.get(); }

  // Provides access to the interfaces implemented by the test. The default one
  // implements PPP_INSTANCE.
  virtual const void* GetMockInterface(const char* interface_name) const;

 private:
  scoped_ptr<MockPluginDelegate> delegate_;

  // Note: module must be declared first since we want it to get destroyed last.
  scoped_refptr<PluginModule> module_;
  scoped_refptr<PluginInstance> instance_;

  DISALLOW_COPY_AND_ASSIGN(PpapiUnittest);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_GLUE_PPAPI_PLUGINS_PPAPI_UNITTEST_H_
