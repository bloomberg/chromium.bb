// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_PLUGIN_MODULE_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_PLUGIN_MODULE_H_

#include <set>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/native_library.h"
#include "base/ref_counted.h"
#include "third_party/ppapi/c/pp_module.h"
#include "third_party/ppapi/c/ppb.h"

typedef struct _ppb_Core PPB_Core;

namespace pepper {

class PluginDelegate;
class PluginInstance;

class PluginModule : public base::RefCounted<PluginModule> {
 public:
  typedef const void* (*PPP_GetInterfaceFunc)(const char*);
  typedef int (*PPP_InitializeModuleFunc)(PP_Module, PPB_GetInterface);
  typedef void (*PPP_ShutdownModuleFunc)();

  struct EntryPoints {
    EntryPoints()
        : get_interface(NULL),
          initialize_module(NULL),
          shutdown_module(NULL) {
    }

    PPP_GetInterfaceFunc get_interface;
    PPP_InitializeModuleFunc initialize_module;
    PPP_ShutdownModuleFunc shutdown_module;
  };

  ~PluginModule();

  static scoped_refptr<PluginModule> CreateModule(const FilePath& path);
  static scoped_refptr<PluginModule> CreateInternalModule(
      EntryPoints entry_points);

  // Converts the given module ID to an actual module object. Will return NULL
  // if the module is invalid.
  static PluginModule* FromPPModule(PP_Module module);

  static const PPB_Core* GetCore();

  PP_Module GetPPModule() const;

  PluginInstance* CreateInstance(PluginDelegate* delegate);

  // Returns "some" plugin instance associated with this module. This is not
  // guaranteed to be any one in particular. This is normally used to execute
  // callbacks up to the browser layer that are not inherently per-instance,
  // but the delegate lives only on the plugin instance so we need one of them.
  PluginInstance* GetSomeInstance() const;

  const void* GetPluginInterface(const char* name) const;

  // This module is associated with a set of instances. The PluginInstance
  // object declares its association with this module in its destructor and
  // releases us in its destructor.
  void InstanceCreated(PluginInstance* instance);
  void InstanceDeleted(PluginInstance* instance);

 private:
  PluginModule();

  bool InitFromEntryPoints(const EntryPoints& entry_points);
  bool InitFromFile(const FilePath& path);
  static bool LoadEntryPoints(const base::NativeLibrary& library,
                              EntryPoints* entry_points);

  bool initialized_;

  // Holds a reference to the base::NativeLibrary handle if this PluginModule
  // instance wraps functions loaded from a library.  Can be NULL.  If
  // |library_| is non-NULL, PluginModule will attempt to unload the library
  // during destruction.
  base::NativeLibrary library_;

  // Contains pointers to the entry points of the actual plugin
  // implementation.
  EntryPoints entry_points_;

  // Non-owning pointers to all instances associated with this module. When
  // there are no more instances, this object should be deleted.
  typedef std::set<PluginInstance*> PluginInstanceSet;
  PluginInstanceSet instances_;

  DISALLOW_COPY_AND_ASSIGN(PluginModule);
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_PLUGIN_MODULE_H_
