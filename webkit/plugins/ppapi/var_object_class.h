// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_CLASS_H_
#define WEBKIT_PLUGINS_PPAPI_CLASS_H_

#include "webkit/plugins/ppapi/resource.h"

#include <string>

#include "base/hash_tables.h"
#include "base/ref_counted.h"
#include "ppapi/c/ppb_class.h"
#include "third_party/npapi/bindings/npruntime.h"

namespace webkit {
namespace ppapi {

class PluginInstance;

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

  struct InstanceData : public NPObject {
    InstanceData();
    ~InstanceData();

    scoped_refptr<VarObjectClass> object_class;
    void* native_data;
  };

  typedef base::hash_map<std::string, Property> PropertyMap;

  VarObjectClass(PluginInstance* instance, PP_ClassDestructor destruct,
                 PP_ClassFunction invoke, PP_ClassProperty* properties);
  virtual ~VarObjectClass();

  // Resource override.
  virtual VarObjectClass* AsVarObjectClass();

  // Returns the PPB_Var interface for the plugin to use.
  static const PPB_Class* GetInterface();

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

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_CLASS_H_

