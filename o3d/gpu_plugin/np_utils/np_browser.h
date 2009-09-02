// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_NP_UTILS_NP_BROWSER_H_
#define O3D_GPU_PLUGIN_NP_UTILS_NP_BROWSER_H_

#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"

typedef struct _NPNetscapeFuncs NPNetscapeFuncs;
typedef struct _NPChromiumFuncs NPChromiumFuncs;

namespace o3d {
namespace gpu_plugin {

// This class exposes the functions provided by the browser to a plugin (the
// ones prefixed NPN_).
class NPBrowser {
 public:
  explicit NPBrowser(NPNetscapeFuncs* funcs);
  virtual ~NPBrowser();

  static NPBrowser* get() {
    return browser_;
  }

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

  // Chromium specific additions.
  virtual NPSharedMemory* MapSharedMemory(NPP id,
                                          NPObject* object,
                                          size_t size,
                                          bool read_only);

  virtual void UnmapSharedMemory(NPP id,
                                 NPSharedMemory* shared_memory);

 private:
  static NPBrowser* browser_;
  NPBrowser* previous_browser_;
  NPNetscapeFuncs* netscape_funcs_;
  NPChromiumFuncs* chromium_funcs_;
  DISALLOW_COPY_AND_ASSIGN(NPBrowser);
};

}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_NP_UTILS_NP_BROWSER_H_
