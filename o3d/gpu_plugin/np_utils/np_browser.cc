// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/np_utils/np_browser.h"
#include "base/logging.h"

#if defined(O3D_IN_CHROME)
#include "webkit/glue/plugins/nphostapi.h"
#else
#include "o3d/third_party/npapi/include/npupp.h"

// TODO: Remove this when we figure out what to do about NPN_MapMemory.
#include "o3d/gpu_plugin/system_services/shared_memory.h"
#endif

namespace gpu_plugin {

NPBrowser* NPBrowser::browser_;

NPBrowser::NPBrowser(NPNetscapeFuncs* funcs)
    : netscape_funcs_(funcs) {
  // Make this the first browser in the linked list.
  previous_browser_ = browser_;
  browser_ = this;
}

NPBrowser::~NPBrowser() {
  // Remove this browser from the linked list.
  DCHECK(browser_ == this);
  browser_ = previous_browser_;
}

NPIdentifier NPBrowser::GetStringIdentifier(const NPUTF8* name) {
  return netscape_funcs_->getstringidentifier(name);
}

void* NPBrowser::MemAlloc(size_t size) {
  return netscape_funcs_->memalloc(size);
}

void NPBrowser::MemFree(void* p) {
  netscape_funcs_->memfree(p);
}

NPObject* NPBrowser::CreateObject(NPP npp, const NPClass* cl) {
  return netscape_funcs_->createobject(npp, const_cast<NPClass*>(cl));
}

NPObject* NPBrowser::RetainObject(NPObject* object) {
  return netscape_funcs_->retainobject(object);
}

void NPBrowser::ReleaseObject(NPObject* object) {
  netscape_funcs_->releaseobject(object);
}

void NPBrowser::ReleaseVariantValue(NPVariant* variant) {
  netscape_funcs_->releasevariantvalue(variant);
}

bool NPBrowser::HasProperty(NPP npp,
                            NPObject* object,
                            NPIdentifier name) {
  return netscape_funcs_->hasproperty(npp, object, name);
}

bool NPBrowser::GetProperty(NPP npp,
                            NPObject* object,
                            NPIdentifier name,
                            NPVariant* result) {
  return netscape_funcs_->getproperty(npp, object, name, result);
}

bool NPBrowser::SetProperty(NPP npp,
                            NPObject* object,
                            NPIdentifier name,
                            const NPVariant* result) {
  return netscape_funcs_->setproperty(npp, object, name, result);
}

bool NPBrowser::RemoveProperty(NPP npp,
                               NPObject* object,
                               NPIdentifier name) {
  return netscape_funcs_->removeproperty(npp, object, name);
}

bool NPBrowser::HasMethod(NPP npp,
                          NPObject* object,
                          NPIdentifier name) {
  return netscape_funcs_->hasmethod(npp, object, name);
}

bool NPBrowser::Invoke(NPP npp,
                       NPObject* object,
                       NPIdentifier name,
                       const NPVariant* args,
                       uint32_t num_args,
                       NPVariant* result) {
  return netscape_funcs_->invoke(npp, object, name, args, num_args, result);
}

NPObject* NPBrowser::GetWindowNPObject(NPP npp) {
  NPObject* window;
  if (NPERR_NO_ERROR == netscape_funcs_->getvalue(npp,
                                                  NPNVWindowNPObject,
                                                  &window)) {
    return window;
  } else {
    return NULL;
  }
}

void NPBrowser::PluginThreadAsyncCall(NPP npp,
                                      PluginThreadAsyncCallProc callback,
                                      void* data) {
  netscape_funcs_->pluginthreadasynccall(npp, callback, data);
}

void* NPBrowser::MapMemory(NPP npp,
                           NPObject* object,
                           size_t* size) {
  // NPN_MapMemory is an experiment. It only exists in NPNetscapeFuncs in
  // a hacked version of Chromium.
#if defined(O3D_IN_CHROME)
  return NULL;
#else
  return NPN_MapMemory(npp, object, size);
#endif
}

}  // namespace gpu_plugin
