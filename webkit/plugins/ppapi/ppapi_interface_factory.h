// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PLUGIN_INTERFACE_FACTORY_H_
#define WEBKIT_PLUGINS_PPAPI_PLUGIN_INTERFACE_FACTORY_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "webkit/plugins/webkit_plugins_export.h"

namespace webkit {
namespace ppapi {

// This class provides functionality to manage custom PPAPI interface
// factories.
class PpapiInterfaceFactoryManager {
 public:
  typedef const void* (InterfaceFactory)(const std::string& interface_name);

  // Registers a custom PPAPI interface factory.
  WEBKIT_PLUGINS_EXPORT void RegisterFactory(InterfaceFactory* factory);

  // Unregisters the custom PPAPI interface factory passed in.
  WEBKIT_PLUGINS_EXPORT void UnregisterFactory(InterfaceFactory* factory);

  // Returns a pointer to the interface identified by the name passed in.
  // Returns NULL if no factory handles this interface.
  const void* GetInterface(const std::string& interface_name);

  // Returns a pointer to the global instance of the
  // PpapiInterfaceFactoryManager class.
  WEBKIT_PLUGINS_EXPORT static PpapiInterfaceFactoryManager* GetInstance();

 private:
  friend struct base::DefaultLazyInstanceTraits<PpapiInterfaceFactoryManager>;

  PpapiInterfaceFactoryManager();
  ~PpapiInterfaceFactoryManager();

  typedef std::vector<InterfaceFactory*> FactoryList;

  // List of registered factories.
  FactoryList interface_factory_list_;

  DISALLOW_COPY_AND_ASSIGN(PpapiInterfaceFactoryManager);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PLUGIN_INTERFACE_FACTORY_H_

