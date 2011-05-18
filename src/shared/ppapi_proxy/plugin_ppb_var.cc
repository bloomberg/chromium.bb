// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_var.h"

#include <stdio.h>

#include <map>
#include <set>
#include <string>
#include <utility>

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
bool StringIsUtf8(const char* data, uint32_t len) {
  for (uint32_t i = 0; i < len; i++) {
    if ((data[i] & 0x80) == 0) {
      // Single-byte symbol.
      continue;
    } else if ((data[i] & 0xc0) == 0x80) {
      // Invalid first byte.
      DebugPrintf("Invalid first byte %02x\n", data[i]);
      return false;
    }
    // This is a multi-byte symbol.
    DebugPrintf("Multi-byte %02x\n", data[i]);
    // Discard the uppermost bit.  The remaining high-order bits are the
    // unary count of continuation bytes (up to 5 of them).
    char first = data[i] << 1;
    uint32_t continuation_bytes = 0;
    const uint32_t kMaxContinuationBytes = 5;
    while (first & 0x80) {
      if (++i >= len) {
        DebugPrintf("String ended before enough continuation bytes"
                    "were found.\n");
        return false;
      } else if (++continuation_bytes > kMaxContinuationBytes) {
        DebugPrintf("Too many continuation bytes were requested.\n");
        return false;
      } else if ((data[i] & 0xc0) != 0x80) {
        DebugPrintf("Invalid continuation byte.\n");
        return false;
      }
      first <<= 1;
    }
  }
  return true;
}

// The PP_Var interface contains two subtypes that require more complicated
// handling: objects and strings.  Both of these subtypes are implemented
// by a secondary structure pointed to by the value.as_id member.  The type
// used to represent both is the template class VarImpl.
// T is the contained type, which is a pair for objects, or a string.
// There is a lookup method FromVar that extracts the implementation type
// and has to check that the var's type field matches the appropriate value.
// P is the PP_Var type used for the type check in FromVar.
template<typename T, PP_VarType P> class VarImpl {
 public:
  static VarImpl* New(T contents);
  ~VarImpl() { }
  // Add a reference to this object or string.
  void AddRef();
  // Remove a reference to this object or string.
  void Release();
  // Get the contained information.  (Either the pair or the string).
  T contents() const { return contents_; }
  // Get the reference count of this object.
  uint64_t ref_count() const { return ref_count_; }
  // Get the id of this object (used for logging).
  uint64_t id() const { return id_; }
  // Get the VarImpl (either the object or string instantiation) corresponding
  // to var, if there is one.  Otherwise return NULL.
  static VarImpl* FromVar(PP_Var var);

 private:
  VarImpl(T contents, uint64_t id);

  // A mapping to keep track of the instances we have allocated.
  static std::set<VarImpl*>* instances;

  T contents_;
  uint64_t ref_count_;
  uint64_t id_;
  NACL_DISALLOW_COPY_AND_ASSIGN(VarImpl);
};

template<typename T, PP_VarType P>
VarImpl<T, P>::VarImpl(T contents, uint64_t id)
  : contents_(contents),
  ref_count_(1),
  id_(id) {
}

template<typename T, PP_VarType P>
VarImpl<T, P>* VarImpl<T, P>::New(T contents) {
  static uint64_t impl_id = 0;
  uint64_t id = impl_id++;
  VarImpl<T, P>* impl = new VarImpl<T, P>(contents, id);
  if (instances == NULL) {
    instances = new std::set<VarImpl<T, P>*>();
  }
  instances->insert(impl);
  return impl;
}

template<typename T, PP_VarType P>
void VarImpl<T, P>::AddRef() {
  ++ref_count_;
}

template<typename T, PP_VarType P>
void VarImpl<T, P>::Release() {
  --ref_count_;
  // TODO(sehr, polina): pull the generic Release below into specializations.
}

template<typename T, PP_VarType P>
VarImpl<T, P>* VarImpl<T, P>::FromVar(PP_Var var) {
  if (var.type != P) {
    return NULL;
  }
  VarImpl<T, P>* obj = reinterpret_cast<VarImpl<T, P>*>(var.value.as_id);
  if (instances->find(obj) == instances->end()) {
    obj = NULL;
  }
  return obj;
}

template<typename T, PP_VarType P>
std::set<VarImpl<T, P>*>* VarImpl<T, P>::instances = NULL;

// The implementation of scriptable objects is done through ObjImpl, which
// contains a pair consisting of object_class, and object_data.
typedef std::pair<const PPP_Class_Deprecated*, void*> ObjPair;
typedef VarImpl<ObjPair, PP_VARTYPE_OBJECT> ObjImpl;

// The implementation of strings is done through StrImpl, which contains a
// std::string&.
typedef VarImpl<std::string&, PP_VARTYPE_STRING> StrImpl;

void AddRef(PP_Var var) {
  ObjImpl* obj_impl = ObjImpl::FromVar(var);
  if (obj_impl != NULL) {
    DebugPrintf("PPB_Var::AddRef: %"NACL_PRIu64"\n", obj_impl->id());
    obj_impl->AddRef();
  }
  StrImpl* str_impl = StrImpl::FromVar(var);
  if (str_impl != NULL) {
    DebugPrintf("PPB_Var::AddRef: '%s'\n", str_impl->contents().c_str());
    str_impl->AddRef();
  }
}

void Release(PP_Var var) {
  ObjImpl* obj_impl = ObjImpl::FromVar(var);
  if (obj_impl != NULL) {
    DebugPrintf("PPB_Var::Release: object(%"NACL_PRIu64")\n", obj_impl->id());
    obj_impl->Release();
    if (obj_impl->ref_count() == 0) {
      if (obj_impl->contents().first == NULL) {
        free(obj_impl->contents().second);
      } else {
        obj_impl->contents().first->Deallocate(obj_impl->contents().second);
      }
      // TODO(polina): under test/ppapi_geturl this deletes a window object
      // prematurely. Most likely we are missing an AddRef somewhere. For now
      // just comment out the delete and leak.
      // http://code.google.com/p/nativeclient/issues/detail?id=1308
      // delete obj_impl;
    }
  }
  StrImpl* str_impl = StrImpl::FromVar(var);
  if (str_impl != NULL) {
    DebugPrintf("PPB_Var::Release: string('%s')\n",
                str_impl->contents().c_str());
    str_impl->Release();
    if (str_impl->ref_count() == 0) {
      delete str_impl;
    }
  }
}

PP_Var VarFromUtf8(PP_Module module_id, const char* data, uint32_t len) {
  UNREFERENCED_PARAMETER(module_id);
  if (!StringIsUtf8(data, len)) {
    DebugPrintf("PPB_Var::VarFromUtf8: string '%.*s' is not UTF8\n",
                len, data);
    return PP_MakeNull();
  }
  // TODO(sehr,polina): string vars should have a unique StrImpl.
  std::string* str = new std::string(data, len);
  // StrImpl takes ownership of string.
  StrImpl* impl = StrImpl::New(*str);
  PP_Var result;
  result.type = PP_VARTYPE_STRING;
  result.value.as_id = reinterpret_cast<int64_t>(impl);
  return result;
}

const char* VarToUtf8(PP_Var var, uint32_t* len) {
  StrImpl* str_impl = StrImpl::FromVar(var);
  if (str_impl == NULL) {
    *len = 0;
    return NULL;
  } else {
    *len = static_cast<uint32_t>(str_impl->contents().size());
    // Mimics PPAPI implementation: as long as StrImpl is alive, the
    // return value can be validly used.
    return str_impl->contents().c_str();
  }
}

bool HasProperty(PP_Var object,
                 PP_Var name,
                 PP_Var* exception) {
  ObjImpl* impl = ObjImpl::FromVar(object);
  if (impl == NULL) {
    if (exception != NULL) {
      *exception =
          PluginVar::StringToPPVar(0, "HasProperty called on non-object");
    }
    return false;
  }
  DebugPrintf("PPB_Var::HasProperty: id=%"NACL_PRIu64"\n", impl->id());
  DebugPrintf("PPB_Var::HasProperty: "
              "object.type = %d; name.type = %d\n", object.type, name.type);
  const PPP_Class_Deprecated* object_class = impl->contents().first;
  if (object_class == NULL || object_class->HasProperty == NULL) {
    return false;
  }
  return object_class->HasProperty(impl->contents().second, name, exception);
}

bool HasMethod(PP_Var object,
               PP_Var name,
               PP_Var* exception) {
  DebugPrintf("PPB_Var::HasMethod: \n");
  ObjImpl* impl = ObjImpl::FromVar(object);
  if (impl == NULL) {
    if (exception != NULL) {
      *exception =
          PluginVar::StringToPPVar(0, "HasMethod called on non-object");
    }
    return false;
  }
  DebugPrintf("PPB_Var::HasMethod: id=%"NACL_PRIu64"\n", impl->id());
  const PPP_Class_Deprecated* object_class = impl->contents().first;
  if (object_class == NULL || object_class->HasMethod == NULL) {
    return false;
  }
  return object_class->HasMethod(impl->contents().second, name, exception);
}

PP_Var GetProperty(PP_Var object,
                   PP_Var name,
                   PP_Var* exception) {
  ObjImpl* impl = ObjImpl::FromVar(object);
  if (impl == NULL) {
    if (exception != NULL) {
      *exception =
          PluginVar::StringToPPVar(0, "GetProperty called on non-object");
    }
    return PP_MakeUndefined();
  }
  DebugPrintf("PPB_Var::GetProperty: id=%"NACL_PRIu64"\n", impl->id());
  const PPP_Class_Deprecated* object_class = impl->contents().first;
  if (object_class == NULL || object_class->GetProperty == NULL) {
    return PP_MakeUndefined();
  }
  return object_class->GetProperty(impl->contents().second, name, exception);
}

void GetAllPropertyNames(PP_Var object,
                         uint32_t* property_count,
                         PP_Var** properties,
                         PP_Var* exception) {
  ObjImpl* impl = ObjImpl::FromVar(object);
  if (impl == NULL) {
    if (exception != NULL) {
      *exception =
          PluginVar::StringToPPVar(0,
              "GetAllPropertyNames called on non-object");
    }
    *property_count = 0;
    *properties = NULL;
    return;
  }
  DebugPrintf("PPB_Var::GetAllPropertyNames: id=%"NACL_PRIu64"\n",
              impl->id());
  const PPP_Class_Deprecated* object_class = impl->contents().first;
  if (object_class == NULL || object_class->GetAllPropertyNames == NULL) {
    return;
  }
  object_class->GetAllPropertyNames(impl->contents().second,
                                    property_count,
                                    properties,
                                    exception);
}

void SetProperty(PP_Var object,
                 PP_Var name,
                 PP_Var value,
                 PP_Var* exception) {
  ObjImpl* impl = ObjImpl::FromVar(object);
  if (impl == NULL) {
    if (exception != NULL) {
      *exception =
          PluginVar::StringToPPVar(0, "SetProperty called on non-object");
    }
    return;
  }
  DebugPrintf("PPB_Var::SetProperty: id=%"NACL_PRIu64"\n", impl->id());
  const PPP_Class_Deprecated* object_class = impl->contents().first;
  if (object_class == NULL || object_class->SetProperty == NULL) {
    return;
  }
  object_class->SetProperty(impl->contents().second, name, value, exception);
}

void RemoveProperty(PP_Var object,
                    PP_Var name,
                    PP_Var* exception) {
  ObjImpl* impl = ObjImpl::FromVar(object);
  if (impl == NULL) {
    if (exception != NULL) {
      *exception =
          PluginVar::StringToPPVar(0, "RemoveProperty called on non-object");
    }
    return;
  }
  DebugPrintf("PPB_Var::RemoveProperty: id=%"NACL_PRIu64"\n",
              impl->id());
  const PPP_Class_Deprecated* object_class = impl->contents().first;
  if (object_class == NULL || object_class->RemoveProperty == NULL) {
    return;
  }
  object_class->RemoveProperty(impl->contents().second, name, exception);
}

PP_Var Call(PP_Var object,
            PP_Var method_name,
            uint32_t argc,
            PP_Var* argv,
            PP_Var* exception) {
  ObjImpl* impl = ObjImpl::FromVar(object);
  if (impl == NULL) {
    if (exception != NULL) {
      *exception = PluginVar::StringToPPVar(0, "Call called on non-object");
    }
    return PP_MakeUndefined();
  }
  DebugPrintf("PPB_Var::Call: id=%"NACL_PRIu64"\n", impl->id());
  const PPP_Class_Deprecated* object_class = impl->contents().first;
  if (object_class == NULL || object_class->Call == NULL) {
    return PP_MakeUndefined();
  }
  return object_class->Call(impl->contents().second,
                            method_name,
                            argc,
                            argv,
                            exception);
}

PP_Var Construct(PP_Var object,
                 uint32_t argc,
                 PP_Var* argv,
                 PP_Var* exception) {
  ObjImpl* impl = ObjImpl::FromVar(object);
  if (impl == NULL) {
    if (exception != NULL) {
      *exception =
          PluginVar::StringToPPVar(0, "Construct called on non-object");
    }
    return PP_MakeUndefined();
  }
  DebugPrintf("PPB_Var::Construct: %"NACL_PRIu64"\n", impl->id());
  const PPP_Class_Deprecated* object_class = impl->contents().first;
  if (object_class == NULL || object_class->Construct == NULL) {
    return PP_MakeUndefined();
  }
  return
      object_class->Construct(impl->contents().second, argc, argv, exception);
}

bool IsInstanceOf(PP_Var var,
                  const PPP_Class_Deprecated* object_class,
                  void** object_data) {
  ObjImpl* impl = ObjImpl::FromVar(var);
  *object_data = NULL;
  if (impl == NULL) {
    return false;
  }
  DebugPrintf("PPB_Var::IsInstanceOf: id=%"NACL_PRIu64"\n", impl->id());
  DebugPrintf("PPB_Var::IsInstanceOf: is instance %p %p\n",
              reinterpret_cast<const void*>(impl->contents().first),
              reinterpret_cast<const void*>(object_class));
  if (object_class != impl->contents().first) {
    return false;
  }
  *object_data = impl->contents().second;
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
  return GetObjectId(ObjImpl::FromVar(var));
}

PP_Var CreateObject(PP_Instance instance_id,
                    const PPP_Class_Deprecated* object_class,
                    void* object_data) {
  UNREFERENCED_PARAMETER(instance_id);
  PP_Var result;
  result.type = PP_VARTYPE_OBJECT;
  result.value.as_id = reinterpret_cast<uint64_t>(
      ObjImpl::New(ObjPair(object_class, object_data)));
  return result;
}

PP_Var CreateObjectWithModuleDeprecated(
    PP_Module module_id,
    const PPP_Class_Deprecated* object_class,
    void* object_data) {
  UNREFERENCED_PARAMETER(module_id);
  PP_Var result;
  result.type = PP_VARTYPE_OBJECT;
  result.value.as_id = reinterpret_cast<uint64_t>(
      ObjImpl::New(ObjPair(object_class, object_data)));
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
