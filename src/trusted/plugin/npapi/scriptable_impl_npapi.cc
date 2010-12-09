/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// NPAPI Scriptable handle implementation.

#include "native_client/src/trusted/plugin/npapi/scriptable_impl_npapi.h"

#include <stdio.h>
#include <string.h>

#include <set>

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/portability.h"
#include "third_party/npapi/bindings/npapi.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/plugin/npapi/browser_impl_npapi.h"
#include "native_client/src/trusted/plugin/npapi/npapi_native.h"
#include "native_client/src/trusted/plugin/npapi/plugin_npapi.h"
#include "native_client/src/trusted/plugin/npapi/ret_array.h"
#include "native_client/src/trusted/plugin/socket_address.h"
#include "native_client/src/trusted/plugin/utility.h"


namespace {

// Functions for marshalling arguments from NPAPI world into NaCl SRPC and
// vice versa.

uint32_t ArgsLength(const NaClSrpcArg* index[]) {
  uint32_t i;
  for (i = 0; (i < NACL_SRPC_MAX_ARGS) && NULL != index[i]; ++i) {
    // Empty body.  Avoids warning.
  }
  return i;
}


// Utilities to support marshalling to/from NPAPI.

bool MarshallInputs(plugin::ScriptableImplNpapi* scriptable_handle,
                    const NPVariant* args,
                    int argc,
                    NaClSrpcArg* inputs[],
                    NaClSrpcArg* outputs[]) {
  int i;
  int inputs_length;
  plugin::PortableHandle* handle = scriptable_handle->handle();
  plugin::PluginNpapi* portable_plugin =
      static_cast<plugin::PluginNpapi*>(handle->plugin());
  NPP npp = plugin::InstanceIdentifierToNPP(portable_plugin->instance_id());

  UNREFERENCED_PARAMETER(argc);
  // TODO(gregoryd): we should be checking argc also.
  for (i = 0; (i < NACL_SRPC_MAX_ARGS) && (NULL != inputs[i]); ++i) {
    switch (inputs[i]->tag) {
      case NACL_SRPC_ARG_TYPE_BOOL:
        /* SCOPE */ {
          bool val;
          if (!plugin::NPVariantToScalar(&args[i], &val)) {
            return false;
          }
          inputs[i]->u.bval = (0 != val);
        }
        break;
      case NACL_SRPC_ARG_TYPE_DOUBLE:
        if (!plugin::NPVariantToScalar(&args[i], &inputs[i]->u.dval)) {
          return false;
        }
        break;
      case NACL_SRPC_ARG_TYPE_HANDLE:
        /* SCOPE */ {
          NPObject* obj;
          if (!plugin::NPVariantToScalar(&args[i], &obj)) {
            PLUGIN_PRINTF(("Handle was not an NPObject (or derived type)\n"));
            return false;
          }
          plugin::ScriptableImplNpapi* scriptable_obj =
              static_cast<plugin::ScriptableImplNpapi*>(obj);
          // Ensure that the object is a scriptable handle we have created.
          if (!plugin::ScriptableImplNpapi::is_valid(scriptable_obj)) {
            return false;
          }
          // This function is called only when we are dealing with a
          // DescBasedHandle.
          plugin::PortableHandle* desc_handle = scriptable_obj->handle();
          inputs[i]->u.hval = desc_handle->desc();
        }
        break;
      case NACL_SRPC_ARG_TYPE_OBJECT:
        if (!plugin::NPVariantToObject(&args[i], &inputs[i]->u.oval)) {
          return false;
        }
        break;
      case NACL_SRPC_ARG_TYPE_INT:
        if (!plugin::NPVariantToScalar(&args[i], &inputs[i]->u.ival)) {
          return false;
        }
        break;
      case NACL_SRPC_ARG_TYPE_LONG:
        if (!plugin::NPVariantToScalar(&args[i], &inputs[i]->u.lval)) {
          return false;
        }
        break;
      case NACL_SRPC_ARG_TYPE_STRING:
        if (!plugin::NPVariantToScalar(&args[i], &inputs[i]->u.sval.str)) {
          return false;
        }
        break;

      case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
        /* SCOPE */ {
          if (!plugin::NPVariantToArray(&args[i],
                                        npp,
                                        &inputs[i]->u.caval.count,
                                        &inputs[i]->u.caval.carr)) {
            return false;
          }
        }
        break;

      case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
        /* SCOPE */ {
          if (!plugin::NPVariantToArray(&args[i],
                                        npp,
                                        &inputs[i]->u.daval.count,
                                        &inputs[i]->u.daval.darr)) {
            return false;
          }
        }
        break;

      case NACL_SRPC_ARG_TYPE_INT_ARRAY:
        /* SCOPE */ {
          if (!plugin::NPVariantToArray(&args[i],
                                        npp,
                                        &inputs[i]->u.iaval.count,
                                        &inputs[i]->u.iaval.iarr)) {
            return false;
          }
        }
        break;

      case NACL_SRPC_ARG_TYPE_LONG_ARRAY:
        /* SCOPE */ {
          if (!plugin::NPVariantToArray(&args[i],
                                        npp,
                                        &inputs[i]->u.laval.count,
                                        &inputs[i]->u.laval.larr)) {
            return false;
          }
        }
        break;

      case NACL_SRPC_ARG_TYPE_INVALID:
      case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
      default:
        return false;
    }
  }

  inputs_length = i;

  // Set the length values for the array-typed returns.
  for (i = 0; (i < NACL_SRPC_MAX_ARGS) && (NULL != outputs[i]); ++i) {
    switch (outputs[i]->tag) {
      case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
        if (!plugin::NPVariantToAllocatedArray(&args[i + inputs_length],
                                               &outputs[i]->u.caval.count,
                                               &outputs[i]->u.caval.carr)) {
          return false;
        }
        break;
      case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
        if (!plugin::NPVariantToAllocatedArray(&args[i + inputs_length],
                                               &outputs[i]->u.daval.count,
                                               &outputs[i]->u.daval.darr)) {
          return false;
        }
        break;
      case NACL_SRPC_ARG_TYPE_INT_ARRAY:
        if (!plugin::NPVariantToAllocatedArray(&args[i + inputs_length],
                                               &outputs[i]->u.iaval.count,
                                               &outputs[i]->u.iaval.iarr)) {
          return false;
        }
        break;
      case NACL_SRPC_ARG_TYPE_LONG_ARRAY:
        if (!plugin::NPVariantToAllocatedArray(&args[i + inputs_length],
                                               &outputs[i]->u.laval.count,
                                               &outputs[i]->u.laval.larr)) {
          return false;
        }
        break;
      case NACL_SRPC_ARG_TYPE_BOOL:
      case NACL_SRPC_ARG_TYPE_DOUBLE:
      case NACL_SRPC_ARG_TYPE_HANDLE:
      case NACL_SRPC_ARG_TYPE_INT:
      case NACL_SRPC_ARG_TYPE_LONG:
      case NACL_SRPC_ARG_TYPE_STRING:
      case NACL_SRPC_ARG_TYPE_OBJECT:
        break;
      case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
      case NACL_SRPC_ARG_TYPE_INVALID:
      default:
        return false;
    }
  }
  return true;
}

bool MarshallOutputs(plugin::ScriptableImplNpapi* scriptable_handle,
                     const plugin::SrpcParams& params,
                     NPVariant* result) {
  NaClSrpcArg** outs = params.outs();
  plugin::PortableHandle* handle = scriptable_handle->handle();
  plugin::PluginNpapi* portable_plugin =
      static_cast<plugin::PluginNpapi*>(handle->plugin());
  NPP npp = plugin::InstanceIdentifierToNPP(portable_plugin->instance_id());

  // Find the length of the return vector.
  uint32_t length = params.OutputLength();

  // If the return vector only has one element, return that as a scalar.
  // Otherwise, return a RetArray with the results.
  NPVariant* retvalue = NULL;

  plugin::RetArray*  retarray = NULL;
  if (NACL_SRPC_MAX_ARGS <= length) {
    // Something went badly wrong.  Recover as gracefully as possible.
    return false;
  } else if (0 == length) {
    if (NULL != result) {
      VOID_TO_NPVARIANT(*result);
      return true;
    }
  } else if (1 == length) {
    retvalue = result;
  } else {
    retarray = new(std::nothrow) plugin::RetArray(npp);
    if (NULL == retarray) {
      return false;
    }
    if (!plugin::ScalarToNPVariant(retarray->ExportObject(), result)) {
      return false;
    }
  }

  for (uint32_t i = 0; i < length; ++i) {
    if (length > 1) {
      // allocate a new NPVariant that will then be added to retarray
      // TODO(gregoryd) - use browser allocation (and release) api
      retvalue = new(std::nothrow) NPVariant;
      if (NULL == retvalue) {
        return false;
      }
    }
    switch (outs[i]->tag) {
    case NACL_SRPC_ARG_TYPE_BOOL:
      /* SCOPE */ {
        bool val = (0 != outs[i]->u.bval);
        if (!plugin::ScalarToNPVariant(val, retvalue)) {
          return false;
        }
      }
      break;
    case NACL_SRPC_ARG_TYPE_DOUBLE:
      if (!plugin::ScalarToNPVariant(static_cast<double>(outs[i]->u.dval),
                                     retvalue)) {
        return false;
      }
      break;
    case NACL_SRPC_ARG_TYPE_INT:
      if (!plugin::ScalarToNPVariant(outs[i]->u.ival, retvalue)) {
        return false;
      }
      break;
    case NACL_SRPC_ARG_TYPE_LONG:
      if (!plugin::ScalarToNPVariant(outs[i]->u.lval, retvalue)) {
        return false;
      }
      break;
    case NACL_SRPC_ARG_TYPE_STRING:
      if (!plugin::ScalarToNPVariant(outs[i]->u.sval.str, retvalue)) {
        return false;
      }
      break;
    case NACL_SRPC_ARG_TYPE_HANDLE:
      if (!plugin::NaClDescToNPVariant(portable_plugin,
                                       outs[i]->u.hval, retvalue)) {
        return false;
      }
      break;
    case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
      if (!plugin::ArrayToNPVariant(outs[i]->u.caval.carr,
                                    outs[i]->u.caval.count,
                                    npp,
                                    retvalue)) {
        return false;
      }
      break;
    case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
      if (!plugin::ArrayToNPVariant(outs[i]->u.daval.darr,
                                    outs[i]->u.daval.count,
                                    npp,
                                    retvalue)) {
        return false;
      }
      break;
    case NACL_SRPC_ARG_TYPE_INT_ARRAY:
      if (!plugin::ArrayToNPVariant(outs[i]->u.iaval.iarr,
                                    outs[i]->u.iaval.count,
                                    npp,
                                    retvalue)) {
        return false;
      }
      break;
    case NACL_SRPC_ARG_TYPE_LONG_ARRAY:
      if (!plugin::ArrayToNPVariant(outs[i]->u.laval.larr,
                                    outs[i]->u.laval.count,
                                    npp,
                                    retvalue)) {
        return false;
      }
      break;
    case NACL_SRPC_ARG_TYPE_OBJECT:
      /* SCOPE */ {
        // Only predeclared plugin methods can return objects and they only
        // return a ScriptableHandle that is actually a ScriptableImplNpapi.
        // This is really ugly due to the multiple inheritance involved.
        // oval contains a ScriptableHandle.  And we need an NPObject.
        // So we have to cast down to ScriptableImplNpapi then back up.
        plugin::ScriptableHandle* handle =
            reinterpret_cast<plugin::ScriptableHandle*>(outs[i]->u.oval);
        // This confirms that this this is indeed a valid ScriptableHandle
        // that was created by us. In theory, a predeclared method could
        // receive and return an opaque JavaScript object that is not
        // a ScriptableHandle. But we don't have methods like this at the
        // time. If one every creates such a method, this CHECK will fail
        // and remind the author to update this code to handle arbitrary
        // objects.
        CHECK(plugin::ScriptableHandle::is_valid(handle));
        plugin::ScriptableImplNpapi* handle_npapi =
            static_cast<plugin::ScriptableImplNpapi*>(handle);
        if (!plugin::ScalarToNPVariant(static_cast<NPObject*>(handle_npapi),
                                       retvalue)) {
          return false;
        }
      }
      break;
    case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
    case NACL_SRPC_ARG_TYPE_INVALID:
    default:
      break;
    }
    if (length > 1) {
      // if we are returning an array, add the current retvalue to retarray
      retarray->SetAt(i, retvalue);
    }
  }
  return true;
}

bool GenericInvoke(plugin::ScriptableImplNpapi* scriptable_handle,
                   NPIdentifier name,
                   plugin::CallType call_type,
                   const NPVariant* args,
                   uint32_t arg_count,
                   NPVariant* result) {
  plugin::PortableHandle* handle = scriptable_handle->handle();
  plugin::BrowserInterface* browser_interface = handle->browser_interface();
  plugin::SrpcParams params;

  PLUGIN_PRINTF(("GenericInvoke: calling %s\n",
                 browser_interface->IdentifierToString(
                     reinterpret_cast<uintptr_t>(name)).c_str()));

  if (NULL != result) {
    NULL_TO_NPVARIANT(*result);
  }
  if (!handle->InitParams(reinterpret_cast<uintptr_t>(name),
                          call_type, &params)) {
    PLUGIN_PRINTF(("GenericInvoke: InitParams failed\n"));
    return false;
  }

  // The arguments passed to the method contain, in order:
  // 1) the actual values to be passed to the RPC
  // 2) the length of any array-typed return value
  // Hence the number of parameters passed to the invocation must equal
  // the sum of the inputs and the number of array-typed return values.
  if (plugin::METHOD_CALL == call_type) {
    const uint32_t kSignatureLength = params.SignatureLength();
    if (kSignatureLength != static_cast<uint32_t>(arg_count)) {
      PLUGIN_PRINTF(("Invoke: signature length incorrect "
                     "%u %d\n",
                     kSignatureLength,
                     arg_count));
      NPN_SetException(scriptable_handle, "Bad argument count");
      return false;
    }
  }

  // if there are any inputs - marshall them
  if (NULL != args) {
    // Marshall the arguments from NPVariants into NaClSrpcArgs
    if (!MarshallInputs(scriptable_handle,
                        args,
                        arg_count,
                        params.ins(),
                        params.outs())) {
      PLUGIN_PRINTF(("Invoke: MarshallInputs failed\n"));
      return false;
    }
  }

  if (!handle->Invoke(reinterpret_cast<uintptr_t>(name), call_type, &params)) {
    PLUGIN_PRINTF(("GenericInvoke: invoke failed\n"));
    // failure
    if (NULL == params.exception_string()) {
      NPN_SetException(scriptable_handle, "Invocation failed");
    } else {
      NPN_SetException(scriptable_handle, params.exception_string());
    }
    return false;
  }
  PLUGIN_PRINTF(("GenericInvoke: invoke succeeded\n"));

  if (!MarshallOutputs(scriptable_handle, params, result)) {
    PLUGIN_PRINTF(("Invoke: MarshallOutputs failed\n"));
    return false;
  }
  PLUGIN_PRINTF(("GenericInvoke: succeeded\n"));
  return true;
}

bool Invoke(NPObject* obj,
            NPIdentifier name,
            const NPVariant* args,
            uint32_t arg_count,
            NPVariant* result) {
  plugin::ScriptableImplNpapi* scriptable_handle =
      static_cast<plugin::ScriptableImplNpapi*>(obj);
  plugin::PortableHandle* handle = scriptable_handle->handle();
  plugin::PluginNpapi* plugin =
      static_cast<plugin::PluginNpapi*>(handle->plugin());
  plugin::BrowserInterface* browser_interface = handle->browser_interface();

  PLUGIN_PRINTF(("Invoke(%p, %s, %d)\n",
                 static_cast<void*>(obj),
                 browser_interface->IdentifierToString(
                     reinterpret_cast<uintptr_t>(name)).c_str(),
                 arg_count));

  if (NULL == plugin->nacl_instance()) {
    return GenericInvoke(scriptable_handle,
                         name,
                         plugin::METHOD_CALL,
                         args,
                         arg_count,
                         result);
  } else {
    NPObject* proxy = plugin->nacl_instance();
    bool retval = proxy->_class->invoke(proxy, name, args, arg_count, result);
    return retval;
  }
}

bool InvokeDefault(NPObject* obj,
                   const NPVariant* args,
                   uint32_t arg_count,
                   NPVariant* result) {
  plugin::ScriptableImplNpapi* scriptable_handle =
      static_cast<plugin::ScriptableImplNpapi*>(obj);
  plugin::PortableHandle* handle = scriptable_handle->handle();
  plugin::PluginNpapi* plugin =
      static_cast<plugin::PluginNpapi*>(handle->plugin());

  PLUGIN_PRINTF(("InvokeDefault(%p, %d)\n",
                 static_cast<void*>(obj),
                 arg_count));

  if (NULL == plugin->nacl_instance()) {
    return false;
  } else {
    NPObject* proxy = plugin->nacl_instance();
    return proxy->_class->invokeDefault(proxy, args, arg_count, result);
  }
}

// Property accessors/mutators.
bool HasProperty(NPObject* obj, NPIdentifier name) {
  plugin::ScriptableImplNpapi* scriptable_handle =
      static_cast<plugin::ScriptableImplNpapi*>(obj);
  plugin::PortableHandle* handle = scriptable_handle->handle();
  plugin::PluginNpapi* plugin =
      static_cast<plugin::PluginNpapi*>(handle->plugin());
  plugin::BrowserInterface* browser_interface = handle->browser_interface();

  PLUGIN_PRINTF(("HasProperty(%p, %s)\n",
                 static_cast<void*>(obj),
                 browser_interface->IdentifierToString(
                     reinterpret_cast<uintptr_t>(name)).c_str()));

  if (NULL == plugin->nacl_instance()) {
    // If the property is supported,
    // the interface should include both set and get methods.
    return scriptable_handle->handle()->HasMethod(
        reinterpret_cast<uintptr_t>(name),
        plugin::PROPERTY_GET);
  } else {
    NPObject* proxy = plugin->nacl_instance();
    return proxy->_class->hasProperty(proxy, name);
  }
}

bool GetProperty(NPObject* obj,
                 NPIdentifier name,
                 NPVariant* variant) {
  plugin::ScriptableImplNpapi* scriptable_handle =
      static_cast<plugin::ScriptableImplNpapi*>(obj);
  plugin::PortableHandle* handle = scriptable_handle->handle();
  plugin::PluginNpapi* plugin =
      static_cast<plugin::PluginNpapi*>(handle->plugin());
  plugin::BrowserInterface* browser_interface = handle->browser_interface();

  PLUGIN_PRINTF(("GetProperty(%p, %s)\n",
                 static_cast<void*>(obj),
                 browser_interface->IdentifierToString(
                     reinterpret_cast<uintptr_t>(name)).c_str()));

  if (NULL == plugin->nacl_instance()) {
    return GenericInvoke(scriptable_handle,
                         name,
                         plugin::PROPERTY_GET,
                         NULL,
                         0,
                         variant);
  } else {
    NPObject* proxy = plugin->nacl_instance();
    return proxy->_class->getProperty(proxy, name, variant);
  }
}

bool SetProperty(NPObject* obj,
                 NPIdentifier name,
                 const NPVariant* variant) {
  plugin::ScriptableImplNpapi* scriptable_handle =
      static_cast<plugin::ScriptableImplNpapi*>(obj);
  plugin::PortableHandle* handle = scriptable_handle->handle();
  plugin::PluginNpapi* plugin =
      static_cast<plugin::PluginNpapi*>(handle->plugin());
  plugin::BrowserInterface* browser_interface = handle->browser_interface();

  PLUGIN_PRINTF(("SetProperty(%p, %s, %p)\n",
                 static_cast<void*>(obj),
                 browser_interface->IdentifierToString(
                     reinterpret_cast<uintptr_t>(name)).c_str(),
                 static_cast<void*>(const_cast<NPVariant*>(variant))));

  if (NULL == plugin->nacl_instance()) {
    return GenericInvoke(scriptable_handle,
                         name,
                         plugin::PROPERTY_SET,
                         variant,
                         1,
                         NULL);
  } else {
    NPObject* proxy = plugin->nacl_instance();
    return proxy->_class->setProperty(proxy, name, variant);
  }
}

void Invalidate(NPObject* obj) {
  plugin::ScriptableImplNpapi* scriptable_handle =
      static_cast<plugin::ScriptableImplNpapi*>(obj);

  PLUGIN_PRINTF(("Invalidate (scriptable_handle=%p)\n",
                 static_cast<void*>(scriptable_handle)));
  scriptable_handle->handle()->Invalidate();

  // After invalidation, the browser does not respect reference counting,
  // so we shut down here what we can and prevent attempts to shut down
  // other linked structures in Deallocate.

  // delete(scriptable_handle->handle_);
  // scriptable_handle->handle_ = NULL;
}

// Pointers to private methods are given to the browser during initialization
// Method invocation.
bool HasMethod(NPObject* obj, NPIdentifier name) {
  plugin::ScriptableImplNpapi* scriptable_handle =
      static_cast<plugin::ScriptableImplNpapi*>(obj);
  plugin::PortableHandle* handle = scriptable_handle->handle();
  plugin::PluginNpapi* plugin =
      static_cast<plugin::PluginNpapi*>(handle->plugin());
  plugin::BrowserInterface* browser_interface = handle->browser_interface();

  PLUGIN_PRINTF(("HasMethod(%p, %s)\n",
                 static_cast<void*>(obj),
                 browser_interface->IdentifierToString(
                     reinterpret_cast<uintptr_t>(name)).c_str()));

  if (NULL == plugin->nacl_instance()) {
    return scriptable_handle->handle()->HasMethod(
        reinterpret_cast<uintptr_t>(name),
        plugin::METHOD_CALL);
  } else {
    NPObject* proxy = plugin->nacl_instance();
    return proxy->_class->hasMethod(proxy, name);
  }
}

bool RemoveProperty(NPObject* obj,
                    NPIdentifier name) {
  plugin::ScriptableImplNpapi* scriptable_handle =
      static_cast<plugin::ScriptableImplNpapi*>(obj);
  plugin::PortableHandle* handle = scriptable_handle->handle();
  plugin::PluginNpapi* plugin =
      static_cast<plugin::PluginNpapi*>(handle->plugin());
  plugin::BrowserInterface* browser_interface = handle->browser_interface();

  PLUGIN_PRINTF(("RemoveProperty(%p, %s)\n",
                 static_cast<void*>(obj),
                 browser_interface->IdentifierToString(
                     reinterpret_cast<uintptr_t>(name)).c_str()));

  if (NULL == plugin->nacl_instance()) {
    // TODO(sehr): Need to proxy this across to NPAPI modules.
    return false;
  } else {
    NPObject* proxy = plugin->nacl_instance();
    return proxy->_class->removeProperty(proxy, name);
  }
}

bool Enumerate(NPObject* obj,
               NPIdentifier** names,
               uint32_t* name_count) {
  PLUGIN_PRINTF(("Enumerate(%p)\n", static_cast<void*>(obj)));

  UNREFERENCED_PARAMETER(obj);
  UNREFERENCED_PARAMETER(names);
  UNREFERENCED_PARAMETER(name_count);

  // TODO(sehr): Enumerate is implemented in the object proxy in npruntime.
  // Connect this call to proxying of the object Enumerate method.
  return false;
}

bool Construct(NPObject* obj,
               const NPVariant* args,
               uint32_t arg_count,
               NPVariant* result) {
  PLUGIN_PRINTF(("Construct(%p)\n", static_cast<void*>(obj)));

  UNREFERENCED_PARAMETER(obj);
  UNREFERENCED_PARAMETER(args);
  UNREFERENCED_PARAMETER(arg_count);
  UNREFERENCED_PARAMETER(result);

  // TODO(sehr): Construct is implemented in the object proxy in npruntime.
  // Connect this call to proxying of the object Construct method.
  return false;
}

}  // namespace

namespace plugin {

NPObject* ScriptableImplNpapi::Allocate(NPP npp, NPClass* npapi_class) {
  UNREFERENCED_PARAMETER(npp);
  UNREFERENCED_PARAMETER(npapi_class);
  PLUGIN_PRINTF(("Allocate()\n"));
  return new(std::nothrow) plugin::ScriptableImplNpapi(NULL);
}

void ScriptableImplNpapi::Deallocate(NPObject* obj) {
  plugin::ScriptableImplNpapi* scriptable_handle =
      static_cast<plugin::ScriptableImplNpapi*>(obj);

  PLUGIN_PRINTF(("ScriptableImplNpapi::Deallocate (obj=%p)\n",
                 static_cast<void*>(obj)));

  // Release the contained descriptor.
  scriptable_handle->handle()->Delete();
  scriptable_handle->set_handle(NULL);
  // And free the memory.
  delete scriptable_handle;
}

ScriptableImplNpapi* ScriptableImplNpapi::New(PortableHandle* handle) {
  static NPClass scriptableHandleClass = {
    NP_CLASS_STRUCT_VERSION,
    Allocate,
    Deallocate,
    Invalidate,
    HasMethod,
    Invoke,
    InvokeDefault,
    HasProperty,
    GetProperty,
    SetProperty,
    RemoveProperty,
    Enumerate,
    Construct
  };

  PLUGIN_PRINTF(("ScriptableImplNpapi::New (handle=%p)\n",
                 static_cast<void*>(handle)));

  // Do not wrap NULL as a portable handle.
  if (handle == NULL) {
    return NULL;
  }

  // Create the browser scriptable object.
  NPP npp = InstanceIdentifierToNPP(handle->plugin()->instance_id());
  ScriptableImplNpapi* scriptable_handle =
      static_cast<ScriptableImplNpapi*>(
          NPN_CreateObject(npp, &scriptableHandleClass));
  if (scriptable_handle == NULL) {
    return NULL;
  }

  // Set the pointer to the portable handle.
  scriptable_handle->set_handle(handle);

  return scriptable_handle;
}

ScriptableHandle* ScriptableImplNpapi::AddRef() {
  ScriptableImplNpapi* handle =
      static_cast<ScriptableImplNpapi*>(NPN_RetainObject(this));
  return static_cast<ScriptableHandle*>(handle);
}

void ScriptableImplNpapi::Unref() {
  NPN_ReleaseObject(this);
}

ScriptableImplNpapi::ScriptableImplNpapi(PortableHandle* handle)
  : ScriptableHandle(handle) {
  PLUGIN_PRINTF(("ScriptableImplNpapi::ScriptableImplNpapi "
                 "(this=%p, handle=%p)\n",
                 static_cast<void*>(this), static_cast<void*>(handle)));
}

ScriptableImplNpapi::~ScriptableImplNpapi() {
  PLUGIN_PRINTF(("ScriptableImplNpapi::!ScriptableImplNpapi "
                 "(this=%p, handle=%p)\n",
                 static_cast<void*>(this), static_cast<void*>(handle())));
  handle()->Delete();
  set_handle(NULL);
  PLUGIN_PRINTF(("ScriptableImplNpapi::~ScriptableImplNpapi "
                 "(this=%p, return)\n", static_cast<void*>(this)));
}

}  // namespace plugin
