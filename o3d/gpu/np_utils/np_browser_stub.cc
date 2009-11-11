// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/np_utils/np_browser_stub.h"
#include "base/logging.h"
#include "base/message_loop.h"

namespace gpu_plugin {

StubNPBrowser::StubNPBrowser() : NPBrowser(NULL) {
}

StubNPBrowser::~StubNPBrowser() {
}

NPIdentifier StubNPBrowser::GetStringIdentifier(const NPUTF8* name) {
  static std::set<std::string> names;
  std::set<std::string>::iterator it = names.find(name);
  if (it == names.end()) {
    it = names.insert(name).first;
  }
  return const_cast<NPUTF8*>((*it).c_str());
}

void* StubNPBrowser::MemAlloc(size_t size) {
  return malloc(size);
}

void StubNPBrowser::MemFree(void* p) {
  free(p);
}

NPObject* StubNPBrowser::CreateObject(NPP npp, const NPClass* cl) {
  NPObject* object = cl->allocate(npp, const_cast<NPClass*>(cl));
  object->referenceCount = 1;
  object->_class = const_cast<NPClass*>(cl);
  return object;
}

NPObject* StubNPBrowser::RetainObject(NPObject* object) {
  ++object->referenceCount;
  return object;
}

void StubNPBrowser::ReleaseObject(NPObject* object) {
  DCHECK_GE(object->referenceCount, 0u);
  --object->referenceCount;
  if (object->referenceCount == 0) {
    object->_class->deallocate(object);
  }
}

void StubNPBrowser::ReleaseVariantValue(NPVariant* variant) {
  if (NPVARIANT_IS_STRING(*variant)) {
    MemFree(const_cast<NPUTF8*>(variant->value.stringValue.UTF8Characters));
  } else if (NPVARIANT_IS_OBJECT(*variant)) {
    ReleaseObject(NPVARIANT_TO_OBJECT(*variant));
  }
}

bool StubNPBrowser::HasProperty(NPP npp,
                                NPObject* object,
                                NPIdentifier name) {
  return object->_class->hasProperty(object, name);
}

bool StubNPBrowser::GetProperty(NPP npp,
                         NPObject* object,
                         NPIdentifier name,
                         NPVariant* result) {
  return object->_class->getProperty(object, name, result);
}

bool StubNPBrowser::SetProperty(NPP npp,
                            NPObject* object,
                            NPIdentifier name,
                            const NPVariant* result) {
  return object->_class->setProperty(object, name, result);
}

bool StubNPBrowser::RemoveProperty(NPP npp,
                                   NPObject* object,
                                   NPIdentifier name) {
  return object->_class->removeProperty(object, name);
}

bool StubNPBrowser::HasMethod(NPP npp,
                              NPObject* object,
                              NPIdentifier name) {
  return object->_class->hasMethod(object, name);
}

bool StubNPBrowser::Invoke(NPP npp,
                    NPObject* object,
                    NPIdentifier name,
                    const NPVariant* args,
                    uint32_t num_args,
                    NPVariant* result) {
  return object->_class->invoke(object, name, args, num_args, result);
}

NPObject* StubNPBrowser::GetWindowNPObject(NPP npp) {
  return NULL;
}

void StubNPBrowser::PluginThreadAsyncCall(
    NPP npp,
    PluginThreadAsyncCallProc callback,
    void* data) {
  MessageLoop::current()->PostTask(FROM_HERE,
                                   NewRunnableFunction(callback, data));
}

uint32 StubNPBrowser::ScheduleTimer(NPP npp,
                                    uint32 interval,
                                    bool repeat,
                                    TimerProc callback) {
  return 0;
}

void StubNPBrowser::UnscheduleTimer(NPP npp, uint32 timer_id) {
}

}  // namespace gpu_plugin
