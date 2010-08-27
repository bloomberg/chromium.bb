/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <assert.h>

#include "native_client/src/trusted/plugin/ppapi/var_utils.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability_io.h"
#include  "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/ppapi/array_ppapi.h"
#include "native_client/src/trusted/plugin/ppapi/scriptable_handle_ppapi.h"
#include "native_client/src/trusted/plugin/socket_address.h"
#include "native_client/src/trusted/plugin/utility.h"

#include "ppapi/cpp/scriptable_object.h"

namespace plugin {


// In JavaScript, foo[1] is equivalent to foo["1"], so map both indexed and
// string names to a string.
nacl::string NameAsString(const pp::Var& name) {
  if (name.is_string())
    return name.AsString();
  assert(name.is_int());
  nacl::stringstream namestream;
  namestream << name.AsInt();
  return namestream.str();
}


// TODO(polina): see if this can be moved to ppapi/cpp/var.h::Var.
nacl::string VarToString(const pp::Var& var) {
  char buf[256];
  if (var.is_void())
    SNPRINTF(buf, sizeof(buf), "Var<VOID>");
  else if (var.is_null())
    SNPRINTF(buf, sizeof(buf), "Var<NULL>");
  else if (var.is_bool())
    SNPRINTF(buf, sizeof(buf), var.AsBool() ? "Var<true>" : "Var<false>");
  else if (var.is_int())
    SNPRINTF(buf, sizeof(buf), "Var<%"NACL_PRIu32">", var.AsInt());
  else if (var.is_double())
    SNPRINTF(buf, sizeof(buf), "Var<%f>", var.AsDouble());
  else if (var.is_string())
    SNPRINTF(buf, sizeof(buf), "Var<'%s'>", var.AsString().c_str());
  else if (var.is_object())
    SNPRINTF(buf, sizeof(buf), "Var<OBJECT>");
  return buf;
}


//-----------------------------------------------------------------------------
// Translation from pp::Var to NaClSrpcArg
//-----------------------------------------------------------------------------

namespace {

// Sets |array_length| and allocate |array_data| based on the length
// represented by |var|. Sets |exception| on error.
template<typename T> void PPVarToAllocateArray(const pp::Var& var,
                                               nacl_abi_size_t* array_length,
                                               T** array_data,
                                               pp::Var* exception) {
  // Initialize result values for error cases.
  *array_length = 0;
  *array_data = NULL;

  if (!var.is_int()) {
    *exception = "incompatible argument: unable to get array length";
    return;
  }
  size_t length = var.AsInt();

  // Check for overflow on size multiplication and IMC array size limit.
  if ((length > SIZE_T_MAX / sizeof(T)) ||
      (length * sizeof(T) > NACL_ABI_IMC_USER_BYTES_MAX)) {
    *exception = "incompatible argument: array length is too long";
    return;
  }

  *array_length = static_cast<nacl_abi_size_t>(length);
  *array_data = reinterpret_cast<T*>(malloc(sizeof(T) * length));
  if (array_data == NULL) {
    *exception = "incompatible argument: internal error";
  }
}


// Translates |var| into |array_length| and |array_data| for type |type|.
// Sets |exception| on error.
template<typename T> void PPVarToArray(const pp::Var& var,
                                       nacl_abi_size_t* array_length,
                                       T** array_data,
                                       pp::Var* exception) {
  if (!var.is_object()) {
    *exception = "incompatible argument: type is not array";
    return;
  }

  pp::Var length_var = var.GetProperty(pp::Var("length"), exception);
  PLUGIN_PRINTF(("  PPVarToArray (length=%s)\n",
                 VarToString(length_var).c_str()));
  PPVarToAllocateArray(length_var, array_length, array_data, exception);
  if (!exception->is_void()) {
    return;
  }

  for (size_t i = 0; i < *array_length; ++i) {
    int32_t index = nacl::assert_cast<int32_t>(i);
    pp::Var element = var.GetProperty(pp::Var(index), exception);
    PLUGIN_PRINTF(("  PPVarToArray (array[%d]=%s)\n",
                   index, VarToString(element).c_str()));
    if (!exception->is_void()) {
      break;
    }
    if (!element.is_number()) {
      *exception = "incompatible argument: non-numeric element type";
      break;
    }
    if (element.is_int()) {
      (*array_data)[i] = static_cast<T>(element.AsInt());
    } else if (element.is_double()) {
      (*array_data)[i] = static_cast<T>(element.AsDouble());
    } else {
      NACL_NOTREACHED();
    }
  }
  if (!exception->is_void()) {
    free(array_data);
    *array_length = 0;
    *array_data = NULL;
  }
}


// Returns NaClDesc* corresponding to |var|. Sets |exception| on error.
NaClDesc* PPVarToNaClDesc(const pp::Var& var, pp::Var* exception) {
  if (!var.is_object()) {
    *exception = "incompatible argument: type is not handle object";
    return NULL;
  }

  // TODO(polina): Extract the underlying object once the method is
  // available in pp::Var:
  //   http://code.google.com/p/chromium/issues/detail?id=53125

  // TODO(sehr): We need to be very careful in how we implement this,
  // as there are security implications.  We need a reliable way to ensure
  // that a pp::Var is in fact a wrapper around one of our scriptable classes
  // (the whole descriptor hierarchy) or a malicious JS could be constructed
  // to spoof these classes.

  *exception = "incompatible argument: handle is not yet supported";
  return NULL;
}


// Allocates and returns a pointer to string corresponding to |var|.
// Sets |exception| on error.
char* PPVarToString(const pp::Var& var, pp::Var* exception) {
  if (!var.is_string()) {
    *exception = "incompatible argument: type is not string";
    return NULL;
  }

  nacl::string var_as_string = var.AsString();
  size_t size = var_as_string.size() + 1;  // + \0
  char* string_as_chars = reinterpret_cast<char*>(malloc(size));
  if (string_as_chars == NULL) {
    *exception = "incompatible argument: internal error";
    return NULL;
  }

  memcpy(string_as_chars, var_as_string.c_str(), size);
  return string_as_chars;
}


// Returns a number corresponding to |var|. JavaScript might mix int and
// double types, so use them interchangeably. Sets |exception| on error.
template<typename T> T PPVarToNumber(const pp::Var& var, pp::Var* exception) {
  T result = 0;
  if (!var.is_number())
    *exception = "incompatible argument: type is not numeric";
  else if (var.is_double())
    result = static_cast<T>(var.AsDouble());
  else if (var.is_int())
    result = static_cast<T>(var.AsInt());
  return result;
}

}  // namespace


// Allocates |arg| of the array length represented by |var|. No-op for scalars.
// Sets |exception| and returns false on error.
bool PPVarToAllocateNaClSrpcArg(const pp::Var& var,
                                NaClSrpcArg* arg, pp::Var* exception) {
  PLUGIN_PRINTF(("  PPVarToAllocateNaClSrpcArg (var=%s, arg->tag='%c')\n",
                 VarToString(var).c_str(), arg->tag));
  switch (arg->tag) {
    case NACL_SRPC_ARG_TYPE_BOOL:
    case NACL_SRPC_ARG_TYPE_DOUBLE:
    case NACL_SRPC_ARG_TYPE_INT:
    case NACL_SRPC_ARG_TYPE_STRING:
    case NACL_SRPC_ARG_TYPE_HANDLE:
    case NACL_SRPC_ARG_TYPE_OBJECT:
      break;  // nothing to do
    case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
      PPVarToAllocateArray(var, &arg->u.caval.count, &arg->u.caval.carr,
                           exception);
      break;
    case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
      PPVarToAllocateArray(var, &arg->u.daval.count, &arg->u.daval.darr,
                           exception);
      break;
    case NACL_SRPC_ARG_TYPE_INT_ARRAY:
      PPVarToAllocateArray(var, &arg->u.iaval.count, &arg->u.iaval.iarr,
                           exception);
      break;
    case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
    case NACL_SRPC_ARG_TYPE_INVALID:
    default:
      *exception = "variant array and invalid type arguments are not supported";
  }
  PLUGIN_PRINTF(("  PPVarToAllocateNaClSrpcArg (return exception=%s)\n",
                 VarToString(*exception).c_str()));
  return exception->is_void();
}


// Translates |var| into |arg|. Returns false and sets exception on error.
bool PPVarToNaClSrpcArg(const pp::Var& var,
                        NaClSrpcArg* arg, pp::Var* exception) {
  PLUGIN_PRINTF(("  PPVarToNaClSrpcArg (var=%s, arg->tag='%c')\n",
                 VarToString(var).c_str(), arg->tag));
  switch (arg->tag) {
    case NACL_SRPC_ARG_TYPE_BOOL:
      if (!var.is_bool())
        *exception = "incompatible argument: type is not bool";
      else
        arg->u.bval = var.AsBool();
      break;
    case NACL_SRPC_ARG_TYPE_DOUBLE:
      arg->u.dval = PPVarToNumber<double>(var, exception);
      break;
    case NACL_SRPC_ARG_TYPE_INT:
      arg->u.ival = PPVarToNumber<int>(var, exception);
      break;
    case NACL_SRPC_ARG_TYPE_STRING:
      arg->u.sval = PPVarToString(var, exception);
      break;
    case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
      PPVarToArray(var, &arg->u.caval.count, &arg->u.caval.carr, exception);
      break;
    case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
      PPVarToArray(var, &arg->u.daval.count, &arg->u.daval.darr, exception);
      break;
    case NACL_SRPC_ARG_TYPE_INT_ARRAY:
      PPVarToArray(var, &arg->u.iaval.count, &arg->u.iaval.iarr, exception);
      break;
    case NACL_SRPC_ARG_TYPE_HANDLE:
      arg->u.hval = reinterpret_cast<NaClSrpcImcDescType>(
          PPVarToNaClDesc(var, exception));
      break;
    case NACL_SRPC_ARG_TYPE_OBJECT:
      // TODO(polina): extract the underlying object once the method is
      // available in pp::Var:
      // http://code.google.com/p/chromium/issues/detail?id=53125
      //
      // There are currently no predeclared PPAPI plugin methods that
      // take objects as input, so this will never be reached.
      NACL_NOTREACHED();
      break;
    case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
    case NACL_SRPC_ARG_TYPE_INVALID:
    default:
      *exception = "variant array and invalid type arguments are not supported";
  }
  PLUGIN_PRINTF(("  PPVarToNaClSrpcArg (return exception=%s)\n",
                 VarToString(*exception).c_str()));
  return exception->is_void();
}


//-----------------------------------------------------------------------------
// Translation NaClSrpcArg to pp::Var
//-----------------------------------------------------------------------------

namespace {

// Return a pp::Var constructed from |array_data|. Sets |exception| on error.
template<typename T> pp::Var ArrayToPPVar(T* array_data,
                                          nacl_abi_size_t array_length,
                                          PluginPpapi* plugin,
                                          pp::Var* exception) {
  ArrayPpapi* array = new(std::nothrow) ArrayPpapi(plugin);
  if (array == NULL) {
    *exception = "incompatible argument: internal error";
    return pp::Var();
  }

  for (size_t i = 0; i < array_length; ++i) {
    int32_t index = static_cast<int32_t>(i);
    array->SetProperty(pp::Var(index), pp::Var(array_data[i]), exception);
  }
  return pp::Var(array);
}


// Returns a pp::Var corresponding to |desc| or void. Sets |exception| on error.
pp::Var NaClDescToPPVar(NaClDesc* desc, PluginPpapi* plugin,
                               pp::Var* exception) {
  nacl::DescWrapper* wrapper = plugin->wrapper_factory()->MakeGeneric(desc);

  DescBasedHandle* desc_handle = NULL;
  if (NACL_DESC_CONN_CAP == wrapper->type_tag() ||
      NACL_DESC_CONN_CAP_FD == wrapper->type_tag()) {
    desc_handle = SocketAddress::New(plugin, wrapper);
  } else {
    desc_handle = DescBasedHandle::New(plugin, wrapper);
  }

  pp::ScriptableObject* object = ScriptableHandlePpapi::New(desc_handle);
  if (object == NULL) {
    *exception = "incompatible argument: failed to create handle var";
    return pp::Var();
  }
  return pp::Var(object);
}


// Returns a pp::Var corresponding to |obj|. Only predeclared plugin methods
// can return objects and they only return a ScriptableHandle that is actually
// a ScriptableHandlePpapi.
pp::Var ObjectToPPVar(void* obj) {
  ScriptableHandle* handle = reinterpret_cast<ScriptableHandle*>(obj);
  // This confirms that this this is indeed a valid SriptableHandle that was
  // created by us. In theory, a predeclared method could receive and return
  // an opaque JavaScript object that is not a ScriptableHandle. But we don't
  // have methods like this at the time. If one ever creates such a method,
  // this CHECK will fail and remind the author to update this code to handle
  // arbitrary objects.
  CHECK(plugin::ScriptableHandle::is_valid(handle));
  return pp::Var(static_cast<ScriptableHandlePpapi*>(handle));
}

}  // namespace

// Returns a pp::Var corresponding to |arg| or void. Sets |exception| on error.
pp::Var NaClSrpcArgToPPVar(const NaClSrpcArg* arg, PluginPpapi* plugin,
                           pp::Var* exception) {
  PLUGIN_PRINTF(("  NaClSrpcArgToPPVar (arg->tag='%c')\n", arg->tag));
  pp::Var var;
  switch (arg->tag) {
    case NACL_SRPC_ARG_TYPE_BOOL:
      var = pp::Var(arg->u.bval != 0);
      break;
    case NACL_SRPC_ARG_TYPE_DOUBLE:
      var = pp::Var(arg->u.dval);
      break;
    case NACL_SRPC_ARG_TYPE_INT:
      var = pp::Var(arg->u.ival);
      break;
    case NACL_SRPC_ARG_TYPE_STRING:
      var = pp::Var(arg->u.sval);
      break;
    case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
      var = ArrayToPPVar(arg->u.caval.carr, arg->u.caval.count, plugin,
                         exception);
      break;
    case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
      var = ArrayToPPVar(arg->u.daval.darr, arg->u.daval.count, plugin,
                        exception);
      break;
    case NACL_SRPC_ARG_TYPE_INT_ARRAY:
      var = ArrayToPPVar(arg->u.iaval.iarr, arg->u.iaval.count, plugin,
                         exception);
      break;
    case NACL_SRPC_ARG_TYPE_HANDLE:
      var = NaClDescToPPVar(arg->u.hval, plugin, exception);
      break;
    case NACL_SRPC_ARG_TYPE_OBJECT:
      var = ObjectToPPVar(arg->u.oval);
      break;
    case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
    case NACL_SRPC_ARG_TYPE_INVALID:
    default:
      *exception = "variant array and invalid argument types are not supproted";
  }
  PLUGIN_PRINTF(("  NaClSrpcArgToPPVar (return var=%s, exception=%s)\n",
                 VarToString(var).c_str(), VarToString(*exception).c_str()));
  return var;
}

}  // namespace plugin
