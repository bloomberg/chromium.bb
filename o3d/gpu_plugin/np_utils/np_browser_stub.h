// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_NP_UTILS_NP_BROWSER_STUB_H_
#define O3D_GPU_PLUGIN_NP_UTILS_NP_BROWSER_STUB_H_

#include <set>
#include <string>

#include "o3d/gpu_plugin/np_utils/np_browser.h"

namespace gpu_plugin {

// Simple implementation of subset of the NPN functions for testing.
class StubNPBrowser : public NPBrowser {
 public:
  StubNPBrowser();
  virtual ~StubNPBrowser();

  // Standard functions from NPNetscapeFuncs.

  virtual NPIdentifier GetStringIdentifier(const NPUTF8* name);

  virtual void* MemAlloc(size_t size);

  virtual void MemFree(void* p);

  virtual NPObject* CreateObject(NPP npp, const NPClass* cl);

  virtual NPObject* RetainObject(NPObject* object);

  virtual void ReleaseObject(NPObject* object);

  virtual void ReleaseVariantValue(NPVariant* variant);

  virtual bool HasProperty(NPP npp,
                           NPObject* object,
                           NPIdentifier name);

  virtual bool GetProperty(NPP npp,
                           NPObject* object,
                           NPIdentifier name,
                           NPVariant* result);

  virtual bool SetProperty(NPP npp,
                           NPObject* object,
                           NPIdentifier name,
                           const NPVariant* result);

  virtual bool RemoveProperty(NPP npp,
                              NPObject* object,
                              NPIdentifier name);

  virtual bool HasMethod(NPP npp,
                           NPObject* object,
                           NPIdentifier name);
  virtual bool Invoke(NPP npp,
                      NPObject* object,
                      NPIdentifier name,
                      const NPVariant* args,
                      uint32_t num_args,
                      NPVariant* result);

  virtual NPObject* GetWindowNPObject(NPP npp);

  virtual void PluginThreadAsyncCall(NPP npp,
                                     PluginThreadAsyncCallProc callback,
                                     void* data);
  virtual void* MapMemory(NPP npp,
                          NPObject* object,
                          size_t* size);

 private:
  DISALLOW_COPY_AND_ASSIGN(StubNPBrowser);
};

}  // namespace gpu_plugin

#endif  // O3D_GPU_PLUGIN_NP_UTILS_NP_BROWSER_STUB_H_
