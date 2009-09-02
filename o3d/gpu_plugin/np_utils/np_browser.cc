// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/np_utils/np_browser.h"
#include "base/logging.h"
#include "webkit/glue/plugins/nphostapi.h"

namespace o3d {
namespace gpu_plugin {

NPBrowser* NPBrowser::browser_;

NPBrowser::NPBrowser(NPNetscapeFuncs* funcs)
    : netscape_funcs_(funcs),
      chromium_funcs_(NULL) {
  // Attempt to get the Chromium functions.
  if (netscape_funcs_ && netscape_funcs_->getvalue) {
    netscape_funcs_->getvalue(NULL, NPNVchromiumFuncs, &chromium_funcs_);
  }

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

NPSharedMemory* NPBrowser::MapSharedMemory(NPP id,
                                           NPObject* object,
                                           size_t size,
                                           bool read_only) {
  DCHECK(chromium_funcs_);
  return chromium_funcs_->mapsharedmemory(id, object, size, read_only);
}

void NPBrowser::UnmapSharedMemory(NPP id,
                                  NPSharedMemory* shared_memory) {
  DCHECK(chromium_funcs_);
  chromium_funcs_->unmapsharedmemory(id, shared_memory);
}
}  // namespace gpu_plugin
}  // namespace o3d
