// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_var.h"

#include <stdio.h>

#include <map>
#include <string>

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/dev/ppp_class_deprecated.h"
#include "ppapi/c/pp_var.h"

namespace ppapi_proxy {

namespace {

class ObjImpl {
 public:
  ObjImpl(const PPP_Class_Deprecated* object_class,
          void* object_data) : object_class_(object_class),
                               object_data_(object_data),
                               ref_count_(1) {
    static uint64_t impl_id = 0;
    id_ = impl_id++;
  }
  ~ObjImpl() { }
  void AddRef() { ++ref_count_; }
  void Release() { --ref_count_; }
  const PPP_Class_Deprecated* object_class() const { return object_class_; }
  void* object_data() const { return object_data_; }
  uint64_t ref_count() const { return ref_count_; }
  uint64_t id() const { return id_; }

 private:
  const PPP_Class_Deprecated* object_class_;
  void* object_data_;
  uint64_t ref_count_;
  uint64_t id_;
  NACL_DISALLOW_COPY_AND_ASSIGN(ObjImpl);
};

class StrImpl {
 public:
  StrImpl(const char* data, uint32_t len) :
    str_(data, len),
    ref_count_(1) {
  }
  ~StrImpl() { }
  void AddRef() { ++ref_count_; }
  void Release() { --ref_count_; }
  const std::string& str() const { return str_; }
  uint64_t ref_count() const { return ref_count_; }

 private:
  std::string str_;
  uint64_t ref_count_;
  NACL_DISALLOW_COPY_AND_ASSIGN(StrImpl);
};

ObjImpl* VarToObjImpl(PP_Var var) {
  if (var.type == PP_VARTYPE_OBJECT) {
    return reinterpret_cast<ObjImpl*>(var.value.as_id);
  } else {
    return NULL;
  }
}

StrImpl* VarToStrImpl(PP_Var var) {
  if (var.type == PP_VARTYPE_STRING) {
    return reinterpret_cast<StrImpl*>(var.value.as_id);
  } else {
    return NULL;
  }
}

void AddRef(PP_Var var) {
  ObjImpl* obj_impl = VarToObjImpl(var);
  if (obj_impl != NULL) {
    DebugPrintf("PPB_Var::AddRef: %"NACL_PRIu64"\n", obj_impl->id());
    obj_impl->AddRef();
  }
  StrImpl* str_impl = VarToStrImpl(var);
  if (str_impl != NULL) {
    DebugPrintf("PPB_Var::AddRef: '%s'\n", str_impl->str().c_str());
    str_impl->AddRef();
  }
}

void Release(PP_Var var) {
  ObjImpl* obj_impl = VarToObjImpl(var);
  if (obj_impl != NULL) {
    DebugPrintf("PPB_Var::Release: object(%"NACL_PRIu64")\n",
                obj_impl->id());
    obj_impl->Release();
    if (obj_impl->ref_count() == 0) {
      if (obj_impl->object_class() == NULL) {
        free(obj_impl->object_data());
      } else {
        obj_impl->object_class()->Deallocate(obj_impl->object_data());
      }
      // TODO(polina): under test/ppapi_geturl this deletes a window object
      // prematurely. Most likely we are missing an AddRef somewhere. For now
      // just comment out the delete and leak.
      // http://code.google.com/p/nativeclient/issues/detail?id=1308
      // delete obj_impl;
    }
  }
  StrImpl* str_impl = VarToStrImpl(var);
  if (str_impl != NULL) {
    DebugPrintf("PPB_Var::Release: string('%s')\n",
                str_impl->str().c_str());
    str_impl->Release();
    if (str_impl->ref_count() == 0) {
      delete str_impl;
    }
  }
}

PP_Var VarFromUtf8(PP_Module module_id, const char* data, uint32_t len) {
  UNREFERENCED_PARAMETER(module_id);
  StrImpl* impl = new StrImpl(data, len);
  PP_Var result;
  result.type = PP_VARTYPE_STRING;
  result.value.as_id = reinterpret_cast<int64_t>(impl);
  return result;
}

const char* VarToUtf8(PP_Var var, uint32_t* len) {
  StrImpl* str_impl = VarToStrImpl(var);
  if (str_impl == NULL) {
    *len = 0;
    return NULL;
  } else {
    *len = static_cast<uint32_t>(str_impl->str().size());
    return str_impl->str().c_str();
  }
}

bool HasProperty(PP_Var object,
                 PP_Var name,
                 PP_Var* exception) {
  ObjImpl* impl = VarToObjImpl(object);
  if (impl == NULL) {
    return false;
  }
  DebugPrintf("PPB_Var::HasProperty: id=%"NACL_PRIu64"\n", impl->id());
  DebugPrintf("PPB_Var::HasProperty: "
              "object.type = %d; name.type = %d\n", object.type, name.type);
  const PPP_Class_Deprecated* object_class = impl->object_class();
  if (object_class == NULL || object_class->HasProperty == NULL) {
    return false;
  }
  return object_class->HasProperty(impl->object_data(), name, exception);
}

bool HasMethod(PP_Var object,
               PP_Var name,
               PP_Var* exception) {
  ObjImpl* impl = VarToObjImpl(object);
  if (impl == NULL) {
    return false;
  }
  DebugPrintf("PPB_Var::HasMethod: id=%"NACL_PRIu64"\n", impl->id());
  const PPP_Class_Deprecated* object_class = impl->object_class();
  if (object_class == NULL || object_class->HasMethod == NULL) {
    return false;
  }
  return object_class->HasMethod(impl->object_data(), name, exception);
}

PP_Var GetProperty(PP_Var object,
                   PP_Var name,
                   PP_Var* exception) {
  ObjImpl* impl = VarToObjImpl(object);
  if (impl == NULL) {
    return PP_MakeUndefined();
  }
  DebugPrintf("PPB_Var::GetProperty: id=%"NACL_PRIu64"\n", impl->id());
  const PPP_Class_Deprecated* object_class = impl->object_class();
  if (object_class == NULL || object_class->GetProperty == NULL) {
    return PP_MakeUndefined();
  }
  return object_class->GetProperty(impl->object_data(), name, exception);
}

void GetAllPropertyNames(PP_Var object,
                         uint32_t* property_count,
                         PP_Var** properties,
                         PP_Var* exception) {
  ObjImpl* impl = VarToObjImpl(object);
  if (impl == NULL) {
    return;
  }
  DebugPrintf("PPB_Var::GetAllPropertyNames: id=%"NACL_PRIu64"\n",
              impl->id());
  const PPP_Class_Deprecated* object_class = impl->object_class();
  if (object_class == NULL || object_class->GetAllPropertyNames == NULL) {
    return;
  }
  object_class->GetAllPropertyNames(impl->object_data(),
                                    property_count,
                                    properties,
                                    exception);
}

void SetProperty(PP_Var object,
                 PP_Var name,
                 PP_Var value,
                 PP_Var* exception) {
  ObjImpl* impl = VarToObjImpl(object);
  if (impl == NULL) {
    return;
  }
  DebugPrintf("PPB_Var::SetProperty: id=%"NACL_PRIu64"\n", impl->id());
  const PPP_Class_Deprecated* object_class = impl->object_class();
  if (object_class == NULL || object_class->SetProperty == NULL) {
    return;
  }
  object_class->SetProperty(impl->object_data(), name, value, exception);
}

void RemoveProperty(PP_Var object,
                    PP_Var name,
                    PP_Var* exception) {
  ObjImpl* impl = VarToObjImpl(object);
  if (impl == NULL) {
    return;
  }
  DebugPrintf("PPB_Var::RemoveProperty: id=%"NACL_PRIu64"\n",
              impl->id());
  const PPP_Class_Deprecated* object_class = impl->object_class();
  if (object_class == NULL || object_class->RemoveProperty == NULL) {
    return;
  }
  object_class->RemoveProperty(impl->object_data(), name, exception);
}

PP_Var Call(PP_Var object,
            PP_Var method_name,
            uint32_t argc,
            PP_Var* argv,
            PP_Var* exception) {
  ObjImpl* impl = VarToObjImpl(object);
  if (impl == NULL) {
    return PP_MakeUndefined();
  }
  DebugPrintf("PPB_Var::Call: id=%"NACL_PRIu64"\n", impl->id());
  const PPP_Class_Deprecated* object_class = impl->object_class();
  if (object_class == NULL || object_class->Call == NULL) {
    return PP_MakeUndefined();
  }
  return object_class->Call(impl->object_data(),
                            method_name,
                            argc,
                            argv,
                            exception);
}

PP_Var Construct(PP_Var object,
                 uint32_t argc,
                 PP_Var* argv,
                 PP_Var* exception) {
  ObjImpl* impl = VarToObjImpl(object);
  if (impl == NULL) {
    return PP_MakeUndefined();
  }
  DebugPrintf("PPB_Var::Construct: %"NACL_PRIu64"\n", impl->id());
  const PPP_Class_Deprecated* object_class = impl->object_class();
  if (object_class == NULL || object_class->Construct == NULL) {
    return PP_MakeUndefined();
  }
  return object_class->Construct(impl->object_data(), argc, argv, exception);
}

bool IsInstanceOf(PP_Var var,
                  const PPP_Class_Deprecated* object_class,
                  void** object_data) {
  ObjImpl* impl = VarToObjImpl(var);
  *object_data = NULL;
  if (impl == NULL) {
    return false;
  }
  DebugPrintf("PPB_Var::IsInstanceOf: id=%"NACL_PRIu64"\n", impl->id());
  DebugPrintf("PPB_Var::IsInstanceOf: is instance %p %p\n",
              reinterpret_cast<const void*>(impl->object_class()),
              reinterpret_cast<const void*>(object_class));
  if (object_class != impl->object_class()) {
    return false;
  }
  *object_data = impl->object_data();
  return true;
}

uint64_t GetObjectId(const void* object) {
  if (object == NULL) {
    return static_cast<uint64_t>(-1);
  } else {
    return reinterpret_cast<const ObjImpl*>(object)->id();
  }
}

uint64_t GetVarId(PP_Var var) {
  return GetObjectId(VarToObjImpl(var));
}

PP_Var CreateObject(PP_Instance instance_id,
                    const PPP_Class_Deprecated* object_class,
                    void* object_data) {
  UNREFERENCED_PARAMETER(instance_id);
  PP_Var result;
  result.type = PP_VARTYPE_OBJECT;
  result.value.as_id =
      reinterpret_cast<uint64_t>(new ObjImpl(object_class, object_data));
  return result;
}

PP_Var CreateObjectWithModuleDeprecated(
    PP_Module module_id,
    const PPP_Class_Deprecated* object_class,
    void* object_data) {
  UNREFERENCED_PARAMETER(module_id);
  PP_Var result;
  result.type = PP_VARTYPE_OBJECT;
  result.value.as_id =
      reinterpret_cast<uint64_t>(new ObjImpl(object_class, object_data));
  return result;
}

}  // namespace

const PPB_Var_Deprecated* PluginVar::GetInterface() {
  static const PPB_Var_Deprecated var_interface = {
    AddRef,
    Release,
    VarFromUtf8,
    VarToUtf8,
    HasProperty,
    HasMethod,
    GetProperty,
    GetAllPropertyNames,
    SetProperty,
    RemoveProperty,
    Call,
    Construct,
    IsInstanceOf,
    CreateObject,
    CreateObjectWithModuleDeprecated
  };
  return &var_interface;
}

std::string PluginVar::DebugString(PP_Var var) {
  switch (var.type) {
    case PP_VARTYPE_UNDEFINED:
      return "##UNDEFINED##";
    case PP_VARTYPE_NULL:
      return "##NULL##";
    case PP_VARTYPE_BOOL:
      return (var.value.as_bool ? "true" : "false");
    case PP_VARTYPE_INT32:
      {
        char buf[32];
        const size_t kBufSize = sizeof(buf);
        SNPRINTF(buf, kBufSize, "%d", static_cast<int>(var.value.as_int));
        return buf;
      }
    case PP_VARTYPE_DOUBLE:
      {
        char buf[32];
        const size_t kBufSize = sizeof(buf);
        SNPRINTF(buf, kBufSize, "%f", var.value.as_double);
        return buf;
      }
    case PP_VARTYPE_STRING:
      {
        uint32_t len;
        const char* data = VarToUtf8(var, &len);
        return std::string(data, len);
      }
    case PP_VARTYPE_OBJECT:
      {
        char buf[32];
        const size_t kBufSize = sizeof(buf);
        SNPRINTF(buf, kBufSize, "%"NACL_PRIu64"", GetVarId(var));
        return std::string("##OBJECT##") + buf + "##";
      }
    case PP_VARTYPE_ARRAY:
    case PP_VARTYPE_DICTIONARY:
      NACL_NOTREACHED();
      break;
  }
  ASSERT_MSG(0, "Unexpected type seen");
  return "##ERROR##";
}

PP_Var PluginVar::StringToPPVar(PP_Module module_id, std::string str) {
  static const PPB_Var_Deprecated* ppb_var = NULL;
  if (ppb_var == NULL) {
    ppb_var = reinterpret_cast<const PPB_Var_Deprecated*>(
        ppapi_proxy::PluginVar::GetInterface());
  }
  if (ppb_var == NULL) {
    return PP_MakeUndefined();
  }
  return ppb_var->VarFromUtf8(module_id,
                              str.c_str(),
                              nacl::assert_cast<uint32_t>(str.size()));
}

std::string PluginVar::PPVarToString(PP_Var var) {
  static const PPB_Var_Deprecated* ppb_var = NULL;
  if (ppb_var == NULL) {
    ppb_var = reinterpret_cast<const PPB_Var_Deprecated*>(
        ppapi_proxy::PluginVar::GetInterface());
  }
  if (ppb_var == NULL || var.type != PP_VARTYPE_STRING) {
    return "";
  }
  uint32_t len;
  return ppb_var->VarToUtf8(var, &len);
}

void PluginVar::Print(PP_Var var) {
  switch (var.type) {
    case PP_VARTYPE_UNDEFINED:
      DebugPrintf("PP_Var(undefined)");
      break;
    case PP_VARTYPE_NULL:
      DebugPrintf("PP_Var(null)");
      break;
    case PP_VARTYPE_BOOL:
      DebugPrintf("PP_Var(bool: %s)", var.value.as_bool ? "true" : "false");
      break;
    case PP_VARTYPE_INT32:
      DebugPrintf("PP_Var(int32: %"NACL_PRId32")", var.value.as_int);
      break;
    case PP_VARTYPE_DOUBLE:
      DebugPrintf("PP_Var(double: %f)", var.value.as_double);
      break;
    case PP_VARTYPE_STRING:
      {
        std::string str = DebugString(var);
        DebugPrintf("PP_Var(string: '%*s')",
                    static_cast<uint32_t>(str.size()),
                    str.c_str());
      }
    case PP_VARTYPE_OBJECT:
      DebugPrintf("PP_Var(object: %"NACL_PRIu64")", GetVarId(var));
      break;
    case PP_VARTYPE_ARRAY:
    case PP_VARTYPE_DICTIONARY:
      NACL_NOTREACHED();
      break;
  }
}

}  // namespace ppapi_proxy
