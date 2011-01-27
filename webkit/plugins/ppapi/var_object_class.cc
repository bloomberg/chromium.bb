// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/var_object_class.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "webkit/plugins/ppapi/npapi_glue.h"
#include "webkit/plugins/ppapi/resource_tracker.h"
#include "webkit/plugins/ppapi/var.h"

using WebKit::WebBindings;

namespace webkit {
namespace ppapi {

namespace {

// VarObjectAccessorWithIdentifier ---------------------------------------------

// Helper class for the new (PPB_Class) NPObject wrapper. This converts a call
// from WebKit where it gives us an NPObject and an NPIdentifier to an
// easily-accessible InstanceData (corresponding to the NPObject) and
// std::string and Property (corresponding to the NPIdentifier).
class VarObjectAccessorWithIdentifier {
 public:
  VarObjectAccessorWithIdentifier(NPObject* object, NPIdentifier identifier)
      : exists_(false),
        instance_data_(static_cast<VarObjectClass::InstanceData*>(object)),
        property_(NULL) {
    if (instance_data_) {
      const NPUTF8* string_value = NULL;
      int32_t int_value = 0;
      bool is_string = false;
      WebBindings::extractIdentifierData(identifier, string_value, int_value,
                                         is_string);
      if (is_string) {
        property_name_ = string_value;

        const VarObjectClass::PropertyMap& properties =
            instance_data_->object_class->properties();
        VarObjectClass::PropertyMap::const_iterator it =
            properties.find(property_name_);
        if (it != properties.end()) {
          property_ = &it->second;
          exists_ = true;
        }
      }
    }
  }

  // Return true if the object is valid, the identifier is valid, and the
  // property with said name exists.
  bool exists() const { return exists_; }
  bool is_method() const { return exists() && property_->method; }
  bool is_readable() const { return exists() && property_->getter; }
  bool is_writable() const {
    return exists() && property_->setter && property_->writable;
  }
  const VarObjectClass::InstanceData* instance_data() const {
    return instance_data_;
  }
  const VarObjectClass::Property* property() const { return property_; }
  PluginInstance* instance() const {
    return instance_data_ ? instance_data_->object_class->instance() : NULL;
  }

 private:
  bool exists_;
  const VarObjectClass::InstanceData* instance_data_;
  std::string property_name_;
  const VarObjectClass::Property* property_;

  DISALLOW_COPY_AND_ASSIGN(VarObjectAccessorWithIdentifier);
};

NPObject* VarObjectClassAllocate(NPP npp, NPClass* the_class) {
  return new VarObjectClass::InstanceData;
}

void VarObjectClassDeallocate(NPObject* object) {
  VarObjectClass::InstanceData* instance =
      static_cast<VarObjectClass::InstanceData*>(object);
  if (instance->object_class->instance_native_destructor())
    instance->object_class->instance_native_destructor()(instance->native_data);
  delete instance;
}

bool VarObjectClassHasMethod(NPObject* np_obj, NPIdentifier name) {
  VarObjectAccessorWithIdentifier accessor(np_obj, name);
  return accessor.is_method();
}

bool VarObjectClassInvoke(NPObject* np_obj, NPIdentifier name,
                          const NPVariant* args, uint32 arg_count,
                          NPVariant* result) {
  VarObjectAccessorWithIdentifier accessor(np_obj, name);
  if (!accessor.is_method())
    return false;

  PPResultAndExceptionToNPResult result_converter(np_obj, result);
  PPVarArrayFromNPVariantArray arguments(accessor.instance(), arg_count, args);
  PPVarFromNPObject self(accessor.instance(), np_obj);

  return result_converter.SetResult(accessor.property()->method(
      accessor.instance_data()->native_data, self.var(), arguments.array(),
      arg_count, result_converter.exception()));
}

bool VarObjectClassInvokeDefault(NPObject* np_obj,
                                 const NPVariant* args,
                                 uint32 arg_count,
                                 NPVariant* result) {
  VarObjectClass::InstanceData* instance =
      static_cast<VarObjectClass::InstanceData*>(np_obj);
  if (!instance || !instance->object_class->instance_invoke())
    return false;

  PPResultAndExceptionToNPResult result_converter(np_obj, result);
  PPVarArrayFromNPVariantArray arguments(instance->object_class->instance(),
                                         arg_count, args);
  PPVarFromNPObject self(instance->object_class->instance(), np_obj);

  return result_converter.SetResult(instance->object_class->instance_invoke()(
      instance->native_data, self.var(), arguments.array(), arg_count,
      result_converter.exception()));
}

bool VarObjectClassHasProperty(NPObject* np_obj, NPIdentifier name) {
  VarObjectAccessorWithIdentifier accessor(np_obj, name);
  return accessor.is_readable();
}

bool VarObjectClassGetProperty(NPObject* np_obj, NPIdentifier name,
                               NPVariant* result) {
  VarObjectAccessorWithIdentifier accessor(np_obj, name);
  if (!accessor.is_readable()) {
    return false;
  }

  PPResultAndExceptionToNPResult result_converter(np_obj, result);
  PPVarFromNPObject self(accessor.instance(), np_obj);

  return result_converter.SetResult(accessor.property()->getter(
      accessor.instance_data()->native_data, self.var(), 0, 0,
      result_converter.exception()));
}

bool VarObjectClassSetProperty(NPObject* np_obj, NPIdentifier name,
                               const NPVariant* variant) {
  VarObjectAccessorWithIdentifier accessor(np_obj, name);
  if (!accessor.is_writable()) {
    return false;
  }

  PPResultAndExceptionToNPResult result_converter(np_obj, NULL);
  PPVarArrayFromNPVariantArray arguments(accessor.instance(), 1, variant);
  PPVarFromNPObject self(accessor.instance(), np_obj);

  // Ignore return value.
  Var::PluginReleasePPVar(accessor.property()->setter(
      accessor.instance_data()->native_data, self.var(), arguments.array(), 1,
      result_converter.exception()));

  return result_converter.CheckExceptionForNoResult();
}

bool VarObjectClassEnumerate(NPObject *np_obj, NPIdentifier **value,
                             uint32_t *count) {
  VarObjectClass::InstanceData* instance =
      static_cast<VarObjectClass::InstanceData*>(np_obj);
  *count = 0;
  *value = NULL;
  if (!instance)
    return false;

  const VarObjectClass::PropertyMap& properties =
      instance->object_class->properties();

  // Don't bother calculating the size of enumerable properties, just allocate
  // enough for all and then fill it partially.
  *value = static_cast<NPIdentifier*>(
      malloc(sizeof(NPIdentifier) * properties.size()));

  NPIdentifier* inserter = *value;
  for (VarObjectClass::PropertyMap::const_iterator i = properties.begin();
      i != properties.end(); ++i)
    if (i->second.enumerable)
      *inserter++ = WebBindings::getStringIdentifier(i->first.c_str());

  *count = inserter - *value;
  return true;
}

NPClass objectclassvar_class = {
  NP_CLASS_STRUCT_VERSION,
  &VarObjectClassAllocate,
  &VarObjectClassDeallocate,
  NULL,
  &VarObjectClassHasMethod,
  &VarObjectClassInvoke,
  &VarObjectClassInvokeDefault,
  &VarObjectClassHasProperty,
  &VarObjectClassGetProperty,
  &VarObjectClassSetProperty,
  NULL,
  &VarObjectClassEnumerate,
};

// PPB_Class -------------------------------------------------------------------

PP_Resource Create(PP_Instance instance_id, PP_ClassDestructor destruct,
                   PP_ClassFunction invoke, PP_ClassProperty* properties) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!properties || !instance)
    return 0;
  scoped_refptr<VarObjectClass> cls = new VarObjectClass(instance,
                                                         destruct,
                                                         invoke,
                                                         properties);
  if (!cls)
    return 0;
  return cls->GetReference();
}

PP_Var Instantiate(PP_Resource class_object, void* native_data,
                   PP_Var* exception) {
  scoped_refptr<VarObjectClass> object_class =
      Resource::GetAs<VarObjectClass>(class_object);
  if (!object_class)
    return PP_MakeUndefined();
  NPObject* obj = WebBindings::createObject(NULL, &objectclassvar_class);
  VarObjectClass::InstanceData* instance_data =
      static_cast<VarObjectClass::InstanceData*>(obj);
  instance_data->object_class = object_class;
  instance_data->native_data = native_data;
  return ObjectVar::NPObjectToPPVar(object_class->instance(), obj);
}

}  // namespace

// VarObjectClass --------------------------------------------------------------

VarObjectClass::Property::Property(const PP_ClassProperty& prop)
    : method(prop.method),
      getter(prop.getter),
      setter(prop.setter),
      writable(!(prop.modifiers & PP_OBJECTPROPERTY_MODIFIER_READONLY)),
      enumerable(!(prop.modifiers & PP_OBJECTPROPERTY_MODIFIER_DONTENUM)) {
}

VarObjectClass::InstanceData::InstanceData() : native_data(NULL) {
}

VarObjectClass::InstanceData::~InstanceData() {}

VarObjectClass::VarObjectClass(PluginInstance* instance,
                               PP_ClassDestructor destruct,
                               PP_ClassFunction invoke,
                               PP_ClassProperty* properties)
    : Resource(instance),
      instance_native_destructor_(destruct),
      instance_invoke_(invoke) {
  PP_ClassProperty* prop = properties;
  while (prop->name) {
    properties_.insert(std::make_pair(std::string(prop->name),
                                      Property(*prop)));
    ++prop;
  }
}

// virtual
VarObjectClass::~VarObjectClass() {
}

VarObjectClass* VarObjectClass::AsVarObjectClass() {
  return this;
}

// static
const PPB_Class* VarObjectClass::GetInterface() {
  static PPB_Class interface = {
    &Create,
    &Instantiate
  };
  return &interface;
}

}  // namespace ppapi
}  // namespace webkit
