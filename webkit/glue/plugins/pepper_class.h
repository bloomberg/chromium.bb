// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_CLASS_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_CLASS_H_

#include "webkit/glue/plugins/pepper_resource.h"

#include <string>

#include "base/hash_tables.h"
#include "ppapi/c/ppb_class.h"

namespace pepper {

class PluginModule;

class VarObjectClass : public Resource {
 public:
  struct Property {
    explicit Property(const PP_ClassProperty& prop);

    const PP_ClassFunction method;
    const PP_ClassFunction getter;
    const PP_ClassFunction setter;
    const bool writable;
    const bool enumerable;
  };

  struct InstanceData;

  typedef base::hash_map<std::string, Property> PropertyMap;

  // Returns the PPB_Var interface for the plugin to use.
  static const PPB_Class* GetInterface();

  VarObjectClass(PluginModule* module, PP_ClassDestructor destruct,
                 PP_ClassFunction invoke, PP_ClassProperty* properties);
  virtual ~VarObjectClass();

  // Resource override.
  virtual VarObjectClass* AsVarObjectClass() { return this; }

  const PropertyMap &properties() const { return properties_; }

  PP_ClassDestructor instance_native_destructor() {
    return instance_native_destructor_;
  }

  PP_ClassFunction instance_invoke() {
    return instance_invoke_;
  }

 private:
  PropertyMap properties_;
  PP_ClassDestructor instance_native_destructor_;
  PP_ClassFunction instance_invoke_;

  DISALLOW_COPY_AND_ASSIGN(VarObjectClass);
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_CLASS_H_

