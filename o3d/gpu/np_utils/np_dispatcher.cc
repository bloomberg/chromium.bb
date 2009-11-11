// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/np_utils/np_dispatcher.h"

namespace gpu_plugin {

bool DispatcherHasMethodHelper(BaseNPDispatcher* chain,
                               NPObject* object,
                               NPIdentifier name) {
  for (BaseNPDispatcher* dispatcher = chain;
       dispatcher;
       dispatcher = dispatcher->next()) {
    if (dispatcher->name() == name) {
      return true;
    }
  }

  return false;
}

bool DispatcherInvokeHelper(BaseNPDispatcher* chain,
                            NPObject* object,
                            NPIdentifier name,
                            const NPVariant* args,
                            uint32_t num_args,
                            NPVariant* result) {
  VOID_TO_NPVARIANT(*result);

  for (BaseNPDispatcher* dispatcher = chain;
       dispatcher;
       dispatcher = dispatcher->next()) {
    if (dispatcher->name() == name &&
        dispatcher->num_args() == static_cast<int>(num_args)) {
      if (dispatcher->Invoke(object, args, num_args, result))
        return true;
    }
  }

  return false;
}

bool DispatcherEnumerateHelper(BaseNPDispatcher* chain,
                               NPObject* object,
                               NPIdentifier** names,
                               uint32_t* num_names) {
  // Count the number of names.
  *num_names = 0;
  for (BaseNPDispatcher* dispatcher = chain;
       dispatcher;
       dispatcher = dispatcher->next()) {
    ++(*num_names);
  }

  // Copy names into the array.
  *names = static_cast<NPIdentifier*>(
      NPBrowser::get()->MemAlloc((*num_names) * sizeof(**names)));
  int i = 0;
  for (BaseNPDispatcher* dispatcher = chain;
       dispatcher;
       dispatcher = dispatcher->next()) {
    (*names)[i] = dispatcher->name();
    ++i;
  }

  return true;
}

BaseNPDispatcher::BaseNPDispatcher(BaseNPDispatcher* next, const NPUTF8* name)
    : next_(next) {
  // Convert first character to lower case if it is the ASCII range.
  // TODO(apatrick): do this correctly for non-ASCII characters.
  std::string java_script_style_name(name);
  if (isupper(java_script_style_name[0])) {
    java_script_style_name[0] = tolower(java_script_style_name[0]);
  }

  name_ = NPBrowser::get()->GetStringIdentifier(
      java_script_style_name.c_str());
}

BaseNPDispatcher::~BaseNPDispatcher() {
}

}  // namespace gpu_plugin
