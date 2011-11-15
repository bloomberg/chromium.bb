// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppapi_interface_factory.h"

#include <algorithm>

#include "base/logging.h"

namespace webkit {
namespace ppapi {

base::LazyInstance<PpapiInterfaceFactoryManager>
    g_ppapi_interface_factory_manager = LAZY_INSTANCE_INITIALIZER;

PpapiInterfaceFactoryManager::PpapiInterfaceFactoryManager() {
}

PpapiInterfaceFactoryManager::~PpapiInterfaceFactoryManager() {
}

void PpapiInterfaceFactoryManager::RegisterFactory(InterfaceFactory* factory) {
  DCHECK(std::find(interface_factory_list_.begin(),
                   interface_factory_list_.end(), factory) ==
         interface_factory_list_.end());
  interface_factory_list_.push_back(factory);
}

void PpapiInterfaceFactoryManager::UnregisterFactory(
    InterfaceFactory* factory) {
  FactoryList::iterator index =
      std::find(interface_factory_list_.begin(), interface_factory_list_.end(),
                factory);
  if (index != interface_factory_list_.end())
    interface_factory_list_.erase(index);
}

const void* PpapiInterfaceFactoryManager::GetInterface(
    const std::string& interface_name) {
  FactoryList::iterator index;

  const void* ppapi_interface = NULL;

  for (index = interface_factory_list_.begin();
       index != interface_factory_list_.end();
       ++index) {
    ppapi_interface = (*index)(interface_name);
    if (ppapi_interface)
      break;
  }
  return ppapi_interface;
}

// static
PpapiInterfaceFactoryManager* PpapiInterfaceFactoryManager::GetInstance() {
  return &g_ppapi_interface_factory_manager.Get();
}

}  // namespace ppapi
}  // namespace webkit

