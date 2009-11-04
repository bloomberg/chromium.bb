/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// NPAPI Simple RPC Interface

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SCRIPTABLE_HANDLE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SCRIPTABLE_HANDLE_H_

#include <stdio.h>
#include <string.h>

#include <set>

#include "native_client/src/include/portability.h"
#include "third_party/npapi/bindings/npapi.h"
#include "native_client/src/trusted/plugin/srpc/npapi_native.h"
#include "native_client/src/trusted/plugin/srpc/ret_array.h"
#include "native_client/src/trusted/plugin/srpc/socket_address.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

namespace nacl_srpc {

// Forward declarations for externals.
class Plugin;
typedef NPObject BrowserScriptableObject;

class ScriptableHandleBase: public NPObject {
 public:
  ScriptableHandleBase() {
    if (NULL == valid_handles) {
      valid_handles = new(std::nothrow) std::set<const ScriptableHandleBase*>;
    }
    if (NULL == valid_handles) {
      // There's no clean way to handle errors in a constructor.
      // TODO(sehr): move to invoking an insert method in factories.
      abort();
    }
    valid_handles->insert(this);
    dprintf(("ScriptableHandle::ScriptableHandle(%p, count %"PRIuS")\n",
             static_cast<void*>(this),
             valid_handles->count(this)));
  }
  virtual PortableHandle* get_handle() = 0;
  virtual ~ScriptableHandleBase() {
    valid_handles->erase(this);
    dprintf(("ScriptableHandle::~ScriptableHandle(%p, count %"PRIuS")\n",
             static_cast<void*>(this),
             valid_handles->count(this)));
  }
  // Check that an object is a validly created UnknownHandle.
  static bool is_valid(const ScriptableHandleBase* handle) {
    dprintf(("ScriptableHandleBase::is_valid(%p)\n",
             static_cast<void*>(const_cast<ScriptableHandleBase*>(handle))));
    if (NULL == valid_handles) {
      dprintf(("ScriptableHandleBase::is_valid -- no set\n"));
      return false;
    }
    dprintf(("ScriptableHandleBase::is_valid(%p, count %"PRIuS")\n",
             static_cast<void*>(const_cast<ScriptableHandleBase*>(handle)),
             valid_handles->count(handle)));
    return 0 != valid_handles->count(handle);
  }

 private:
  static std::set<const ScriptableHandleBase*>* valid_handles;
};

// ScriptableHandle is the struct used to represent handles that are opaque to
// the javascript bridge.  This type is instantiated once for each of the
// scriptable types used by the plugin (e.g., RetArray, ConnectedSocket, ...)
template <typename HandleType>
class ScriptableHandle: public ScriptableHandleBase {
 public:
  // Create a new ScriptableHandle.
  static ScriptableHandle<HandleType>* New(
      struct PortableHandleInitializer* init_info) {
    static NPClass scriptableHandleClass = { NP_CLASS_STRUCT_VERSION,
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

    dprintf(("ScriptableHandle::New\n"));

    ScriptableHandle<HandleType>* scriptable_handle =
      static_cast<ScriptableHandle<HandleType>*>(NPN_CreateObject(
        init_info->plugin_interface_->GetPluginIdentifier(),
        &scriptableHandleClass));
    scriptable_handle->plugin_interface_ = init_info->plugin_interface_;
    if (!scriptable_handle->handle_->Init(init_info)) {
      // Initialization failed
      scriptable_handle->Unref();
      return NULL;
    }
    return scriptable_handle;
  }
  // Get the contained objects.
  virtual PortableHandle* get_handle() {
    return static_cast<PortableHandle*>(handle_);
  }

  void Unref() { NPN_ReleaseObject(this); }
  void AddRef() { NPN_RetainObject(this); }

  // There are derived classes, so the constructor and destructor must
  // be visible.
  explicit ScriptableHandle() {
    dprintf(("ScriptableHandle::ScriptableHandle(%p)\n",
             static_cast<void*>(this)));
    handle_ = new(std::nothrow) HandleType();
  }
  virtual ~ScriptableHandle() {
    dprintf(("ScriptableHandle::~ScriptableHandle(%p)\n",
             static_cast<void*>(this)));
    delete handle_;
    handle_ = NULL;
  }

  static bool Invoke(NPObject* obj,
                     NPIdentifier name,
                     const NPVariant* args,
                     uint32_t arg_count,
                     NPVariant* result) {
    ScriptableHandle<HandleType>* unknown_handle =
        static_cast<ScriptableHandle<HandleType>*>(obj);

      dprintf(("ScriptableHandle::Invoke(%p, %s, %d)\n",
               static_cast<void*>(obj),
               PortablePluginInterface::IdentToString(
                   reinterpret_cast<uintptr_t>(name)),
               arg_count));

    PortablePluginInterface* intf = unknown_handle->plugin_interface_;
    if (NULL == intf->nacl_instance()) {
      return unknown_handle->GenericInvoke(name,
                                           METHOD_CALL,
                                           args,
                                           arg_count,
                                           result);
    } else {
      NPObject* proxy = intf->nacl_instance();
      bool retval = proxy->_class->invoke(proxy, name, args, arg_count, result);
      printf("scriptable handle returned %d\n", retval);
      return retval;
    }
  }

  static bool InvokeDefault(NPObject* obj,
                            const NPVariant* args,
                            uint32_t arg_count,
                            NPVariant* result) {
    ScriptableHandle<HandleType>* unknown_handle =
        static_cast<ScriptableHandle<HandleType>*>(obj);

    dprintf(("ScriptableHandle::InvokeDefault(%p, %d)\n",
             static_cast<void*>(obj),
             arg_count));

    PortablePluginInterface* intf = unknown_handle->plugin_interface_;
    if (NULL == intf->nacl_instance()) {
      return false;
    } else {
      NPObject* proxy = intf->nacl_instance();
      return proxy->_class->invokeDefault(proxy, args, arg_count, result);
    }
  }

  // Property accessors/mutators.
  static bool HasProperty(NPObject* obj, NPIdentifier name) {
    ScriptableHandle<HandleType>* unknown_handle =
      static_cast<ScriptableHandle<HandleType>*>(obj);

    dprintf(("ScriptableHandle::HasProperty(%p, %s)\n",
             static_cast<void*>(obj),
             PortablePluginInterface::IdentToString(
                 reinterpret_cast<uintptr_t>(name))));

    PortablePluginInterface* intf = unknown_handle->plugin_interface_;
    if (NULL == intf->nacl_instance()) {
      // If the property is supported,
      // the interface should include both set and get methods.
      return unknown_handle->handle_->HasMethod(
          reinterpret_cast<uintptr_t>(name),
          PROPERTY_GET);
    } else {
      NPObject* proxy = intf->nacl_instance();
      return proxy->_class->hasProperty(proxy, name);
    }
  }

  static bool GetProperty(NPObject* obj,
                          NPIdentifier name,
                          NPVariant* variant) {
    ScriptableHandle<HandleType>* unknown_handle =
        static_cast<ScriptableHandle<HandleType>*>(obj);

    dprintf(("ScriptableHandle::GetProperty(%p, %s)\n",
             static_cast<void*>(obj),
             PortablePluginInterface::IdentToString(
                 reinterpret_cast<uintptr_t>(name))));

    PortablePluginInterface* intf = unknown_handle->plugin_interface_;
    if (NULL == intf->nacl_instance()) {
      return unknown_handle->GenericInvoke(name,
                                           PROPERTY_GET,
                                           NULL,
                                           0,
                                           variant);
    } else {
      NPObject* proxy = intf->nacl_instance();
      return proxy->_class->getProperty(proxy, name, variant);
    }
  }

  static bool SetProperty(NPObject* obj,
                          NPIdentifier name,
                          const NPVariant* variant) {
    ScriptableHandle<HandleType>* unknown_handle =
        static_cast<ScriptableHandle<HandleType>*>(obj);

    dprintf(("ScriptableHandle::SetProperty(%p, %s, %p)\n",
             static_cast<void*>(obj),
             PortablePluginInterface::IdentToString(
                 reinterpret_cast<uintptr_t>(name)),
             static_cast<void*>(const_cast<NPVariant*>(variant))));

    PortablePluginInterface* intf = unknown_handle->plugin_interface_;
    if (NULL == intf->nacl_instance()) {
      return unknown_handle->GenericInvoke(name,
                                           PROPERTY_SET,
                                           variant,
                                           1,
                                           NULL);
    } else {
      NPObject* proxy = intf->nacl_instance();
      return proxy->_class->setProperty(proxy, name, variant);
    }
  }

  static bool RemoveProperty(NPObject* obj,
                             NPIdentifier name) {
    ScriptableHandle<HandleType>* unknown_handle =
      static_cast<ScriptableHandle<HandleType>*>(obj);

    dprintf(("ScriptableHandle::RemoveProperty(%p, %s)\n",
             static_cast<void*>(obj),
             PortablePluginInterface::IdentToString(
                 reinterpret_cast<uintptr_t>(name))));

    PortablePluginInterface* intf = unknown_handle->plugin_interface_;
    if (NULL == intf->nacl_instance()) {
      return false;
    } else {
      NPObject* proxy = intf->nacl_instance();
      return proxy->_class->removeProperty(proxy, name);
    }
  }

  static bool Enumerate(NPObject* obj,
                        NPIdentifier** names,
                        uint32_t* name_count) {
    dprintf(("ScriptableHandle::Enumerate(%p)\n",
             static_cast<void*>(obj)));

    UNREFERENCED_PARAMETER(obj);
    UNREFERENCED_PARAMETER(names);
    UNREFERENCED_PARAMETER(name_count);

    // TODO(sehr): connect this.
    return false;
  }

  static bool Construct(NPObject* obj,
                        const NPVariant* args,
                        uint32_t arg_count,
                        NPVariant* result) {
    dprintf(("ScriptableHandle::Construct(%p)\n",
             static_cast<void*>(obj)));

    UNREFERENCED_PARAMETER(obj);
    UNREFERENCED_PARAMETER(args);
    UNREFERENCED_PARAMETER(arg_count);
    UNREFERENCED_PARAMETER(result);
    // TODO(sehr): connect this.
    return false;
  }

  // Allocation and deallocation.
  static NPObject* Allocate(NPP npp, NPClass* theClass) {
    UNREFERENCED_PARAMETER(npp);
    UNREFERENCED_PARAMETER(theClass);
    dprintf(("ScriptableHandle::Allocate(%d)\n", HandleType::number_alive()));
    return new(std::nothrow) ScriptableHandle<HandleType>();
  }

  static void Deallocate(NPObject* obj) {
    ScriptableHandle<HandleType>* unknown_handle =
        static_cast<ScriptableHandle<HandleType>*>(obj);

    dprintf(("ScriptableHandle::Deallocate(%p, %d)\n",
             static_cast<void*>(obj), HandleType::number_alive()));

    // Release the contained descriptor.
    delete unknown_handle->handle_;
    unknown_handle->handle_ = NULL;
    // And free the memory.
    delete unknown_handle;
  }

  static void Invalidate(NPObject* obj) {
    ScriptableHandle<HandleType>* unknown_handle =
        static_cast<ScriptableHandle<HandleType>*>(obj);

    dprintf(("ScriptableHandle::Invalidate(%p)\n",
             static_cast<void*>(unknown_handle)));

    // After invalidation, the browser does not respect reference counting,
    // so we shut down here what we can and prevent attempts to shut down
    // other linked structures in Deallocate.

    // delete(unknown_handle->handle_);
    // unknown_handle->handle_ = NULL;
  }

  static int number_alive() { return HandleType::number_alive(); }

 private:
  // Pointers to private methods are given to the browser during initialization
  // Method invocation.
  static bool HasMethod(NPObject* obj, NPIdentifier name) {
    ScriptableHandle<HandleType>* unknown_handle =
        static_cast<ScriptableHandle<HandleType>*>(obj);

    dprintf(("ScriptableHandle::HasMethod(%p, %s)\n",
             static_cast<void*>(obj),
             PortablePluginInterface::IdentToString(
                 reinterpret_cast<uintptr_t>(name))));
    if (NULL == unknown_handle->plugin_interface_->nacl_instance()) {
      return unknown_handle->handle_->HasMethod(
          reinterpret_cast<uintptr_t>(name),
          METHOD_CALL);
    } else {
      PortablePluginInterface* intf = unknown_handle->plugin_interface_;
      NPObject* proxy = intf->nacl_instance();
      printf("Invoking HASMETHOD %p\n", static_cast<void*>(proxy));
      return proxy->_class->hasMethod(proxy, name);
    }
  }

  bool GenericInvoke(NPIdentifier name,
                     CallType call_type,
                     const NPVariant* args,
                     uint32_t arg_count,
                     NPVariant* result) {
    SrpcParams params;
    const char* str_name =
        PortablePluginInterface::IdentToString(
            reinterpret_cast<uintptr_t>(name));
    dprintf(("ScriptableHandle::GenericInvoke: calling %s\n", str_name));

    if (NULL != result) {
      NULL_TO_NPVARIANT(*result);
    }
    if (!handle_->InitParams(reinterpret_cast<uintptr_t>(name),
                             call_type, &params)) {
      dprintf(("ScriptableHandle::GenericInvoke: InitParams failed\n"));
      return false;
    }

    // The arguments passed to the method contain, in order:
    // 1) the actual values to be passed to the RPC
    // 2) the length of any array-typed return value
    // Hence the number of parameters passed to the invocation must equal
    // the sum of the inputs and the number of array-typed return values.
    if (METHOD_CALL == call_type) {
      const uint32_t kSignatureLength = SignatureLength(params.ins,
                                                        params.outs);
      if (kSignatureLength != static_cast<uint32_t>(arg_count)) {
        dprintf(("ScriptableHandle::Invoke: signature length incorrect %u %d\n",
                kSignatureLength,
                arg_count));
        NPN_SetException(this, "Bad argument count");
        return false;
      }
    }

    // if there are any inputs - marshal them
    if (NULL != args) {
      // Marshall the arguments from NPVariants into NaClSrpcArgs
      if (!MarshallInputs(args,
                          arg_count,
                          params.ins,
                          params.outs)) {
        dprintf(("ScriptableHandle::Invoke: MarshallInputs failed\n"));
        return false;
      }
    }

    if (!handle_->Invoke(reinterpret_cast<uintptr_t>(name),
                         call_type,
                         &params)) {
      // failure
      if (params.HasExceptionInfo()) {
        NPN_SetException(this, params.GetExceptionInfo());
      } else {
        NPN_SetException(this, "Invocation failed");
      }
      return false;
    }

    if (!MarshallOutputs(const_cast<const NaClSrpcArg**>(params.outs),
                         result)) {
      dprintf(("ScriptableHandle::Invoke: MarshallOutputs failed\n"));
      return false;
    }
    return true;
  }

  // Functions for marshalling arguments from NPAPI world into NaCl SRPC and
  // vice versa.
  //
  // Utilities to support marshalling to/from NPAPI.
  // TODO(gregoryd): move these two functions out of a header file.
  //
  bool MarshallInputs(const NPVariant* args,
                      int argc,
                      NaClSrpcArg* inputs[],
                      NaClSrpcArg* outputs[]) {
    int i;
    int inputs_length;
    UNREFERENCED_PARAMETER(argc);
    // TODO(gregoryd): we should be checking argc also.
    for (i = 0; (i < NACL_SRPC_MAX_ARGS) && (NULL != inputs[i]); ++i) {
      switch (inputs[i]->tag) {
        case NACL_SRPC_ARG_TYPE_BOOL:
          /* SCOPE */ {
            bool val;
            if (!NPVariantToScalar(&args[i], &val)) {
              return false;
            }
            inputs[i]->u.bval = (0 != val);
          }
          break;
        case NACL_SRPC_ARG_TYPE_DOUBLE:
          if (!NPVariantToScalar(&args[i], &inputs[i]->u.dval)) {
            return false;
          }
          break;
        case NACL_SRPC_ARG_TYPE_HANDLE:
          /* SCOPE */ {
            NPObject* obj;
            if (!NPVariantToScalar(&args[i], &obj)) {
              dprintf(("Handle was not an NPObject (or derived type)\n"));
              return false;
            }
            // All our handles derive from ScriptableHandleBase
            ScriptableHandleBase* scriptable_base =
                static_cast<ScriptableHandleBase*>(obj);
            // This function is called only when we are dealing with a
            // DescBasedHandle
            DescBasedHandle* desc_handle =
                static_cast<DescBasedHandle*>(scriptable_base->get_handle());

            inputs[i]->u.hval = desc_handle->desc();
          }
          break;
        case NACL_SRPC_ARG_TYPE_OBJECT:
          if (!GetObject(&inputs[i]->u.oval, args[i])) {
            return false;
          }
          break;
        case NACL_SRPC_ARG_TYPE_INT:
          if (!NPVariantToScalar(&args[i], &inputs[i]->u.ival)) {
            return false;
          }
          break;
        case NACL_SRPC_ARG_TYPE_STRING:
          if (!NPVariantToScalar(&args[i], &inputs[i]->u.sval)) {
            return false;
          }
          break;

        case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
          /* SCOPE */ {
            if (!NPVariantToArray(&args[i],
                                  plugin_interface_->GetPluginIdentifier(),
                                  &inputs[i]->u.caval.count,
                                  &inputs[i]->u.caval.carr)) {
              return false;
            }
          }
          break;

        case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
          /* SCOPE */ {
            if (!NPVariantToArray(&args[i],
                                  plugin_interface_->GetPluginIdentifier(),
                                  &inputs[i]->u.daval.count,
                                  &inputs[i]->u.daval.darr)) {
              return false;
            }
          }
          break;

        case NACL_SRPC_ARG_TYPE_INT_ARRAY:
          /* SCOPE */ {
            if (!NPVariantToArray(&args[i],
                                  plugin_interface_->GetPluginIdentifier(),
                                  &inputs[i]->u.iaval.count,
                                  &inputs[i]->u.iaval.iarr)) {
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
          if (!NPVariantToAllocatedArray(&args[i + inputs_length],
              &outputs[i]->u.caval.count,
              &outputs[i]->u.caval.carr)) {
            return false;
          }
          break;
        case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
          if (!NPVariantToAllocatedArray(&args[i + inputs_length],
              &outputs[i]->u.daval.count,
              &outputs[i]->u.daval.darr)) {
            return false;
          }
          break;
        case NACL_SRPC_ARG_TYPE_INT_ARRAY:
          if (!NPVariantToAllocatedArray(&args[i + inputs_length],
              &outputs[i]->u.iaval.count,
              &outputs[i]->u.iaval.iarr)) {
            return false;
          }
          break;
        case NACL_SRPC_ARG_TYPE_BOOL:
        case NACL_SRPC_ARG_TYPE_DOUBLE:
        case NACL_SRPC_ARG_TYPE_HANDLE:
        case NACL_SRPC_ARG_TYPE_INT:
        case NACL_SRPC_ARG_TYPE_STRING:
          break;
        case NACL_SRPC_ARG_TYPE_OBJECT:
        case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
        case NACL_SRPC_ARG_TYPE_INVALID:
        default:
          return false;
      }
    }
    return true;
  }

  bool MarshallOutputs(const NaClSrpcArg** outs,
                       NPVariant* result) {
    // Find the length of the return vector.
    uint32_t length = ArgsLength(outs);

    // If the return vector only has one element, return that as a scalar.
    // Otherwise, return a RetArray with the results.
    NPVariant* retvalue = NULL;

    RetArray*  retarray = NULL;
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
      retarray = new(std::nothrow) RetArray(
          plugin_interface_->GetPluginIdentifier());
      if (NULL == retarray) {
        return false;
      }
      ScalarToNPVariant(retarray->ExportObject(), result);
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
          ScalarToNPVariant(val, retvalue);
        }
        break;
      case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
        /* SCOPE */ {
          // TODO(gregoryd): implement and use ArrayToNPVariant equivalent
          uint32_t sublength = outs[i]->u.caval.count;
          RetArray subarray(plugin_interface_->GetPluginIdentifier());

          NPVariant subvalue;
          for (uint32_t j = 0; j < sublength; ++j) {
            ScalarToNPVariant(outs[i]->u.caval.carr[j], &subvalue);
            subarray.SetAt(j, &subvalue);
          }
          subarray.ExportVariant(retvalue);
        }
        break;
      case NACL_SRPC_ARG_TYPE_DOUBLE:
        ScalarToNPVariant(static_cast<double>(outs[i]->u.dval), retvalue);
        break;
      case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
        /* SCOPE */ {
          uint32_t sublength = outs[i]->u.daval.count;
          RetArray subarray(plugin_interface_->GetPluginIdentifier());

          NPVariant subvalue;
          for (uint32_t j = 0; j < sublength; ++j) {
            ScalarToNPVariant(outs[i]->u.daval.darr[j], &subvalue);
            subarray.SetAt(j, &subvalue);
          }
          subarray.ExportVariant(retvalue);
        }
        break;
      case NACL_SRPC_ARG_TYPE_HANDLE:
        /* SCOPE */ {
          struct NaClDesc* desc = outs[i]->u.hval;
          Plugin* plugin = handle_->GetPlugin();

          if (NACL_DESC_CONN_CAP == desc->vtbl->typeTag) {
            SocketAddressInitializer init_info(plugin_interface_, desc, plugin);
            NPObject* sock_addr = ScriptableHandle<SocketAddress>::New(
                static_cast<PortableHandleInitializer*>(&init_info));
            if (NULL == sock_addr) {
              return false;
            }
            ScalarToNPVariant(sock_addr, retvalue);
          } else {
            DescHandleInitializer init_info(plugin_interface_, desc, plugin);
            NPObject* handle = ScriptableHandle<DescBasedHandle>::New(
                static_cast<PortableHandleInitializer*>(&init_info));
            if (NULL == handle) {
              return false;
            }
            ScalarToNPVariant(handle, retvalue);
          }
        }
        break;
      case NACL_SRPC_ARG_TYPE_OBJECT:
        /* SCOPE */ {
          ScalarToNPVariant(
              reinterpret_cast<BrowserScriptableObject*>(outs[i]->u.oval),
              retvalue);
        }
        break;
      case NACL_SRPC_ARG_TYPE_INT:
        ScalarToNPVariant(outs[i]->u.ival, retvalue);
        break;
      case NACL_SRPC_ARG_TYPE_INT_ARRAY:
        /* SCOPE */ {
          uint32_t sublength = outs[i]->u.iaval.count;
          RetArray subarray(plugin_interface_->GetPluginIdentifier());

          NPVariant subvalue;
          for (uint32_t j = 0; j < sublength; ++j) {
            ScalarToNPVariant(outs[i]->u.iaval.iarr[j], &subvalue);
            subarray.SetAt(j, &subvalue);
          }
          subarray.ExportVariant(retvalue);
        }
        break;
      case NACL_SRPC_ARG_TYPE_STRING:
        /* SCOPE */ {
          size_t len = strlen(outs[i]->u.sval) + 1;
          char* tmpstr= reinterpret_cast<char*>(NPN_MemAlloc(len));
          strncpy(tmpstr, outs[i]->u.sval, len);
          ScalarToNPVariant(tmpstr, retvalue);
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

  // Computes the number of parameters that should be passed to a method.  The
  // number is the sum of the input argument count and the number of array-typed
  // outputs.
  static uint32_t SignatureLength(NaClSrpcArg* in_index[],
                                  NaClSrpcArg* out_index[]) {
    uint32_t ins = ArgsLength(const_cast<const NaClSrpcArg**>(in_index));
    uint32_t outs = ArgsLength(const_cast<const NaClSrpcArg**>(out_index));
    uint32_t array_outs = 0;

    for (uint32_t i = 0; i < outs; ++i) {
      switch (out_index[i]->tag) {
     case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
     case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
     case NACL_SRPC_ARG_TYPE_INT_ARRAY:
       ++array_outs;
       break;
     case NACL_SRPC_ARG_TYPE_STRING:
     case NACL_SRPC_ARG_TYPE_BOOL:
     case NACL_SRPC_ARG_TYPE_DOUBLE:
     case NACL_SRPC_ARG_TYPE_INT:
     case NACL_SRPC_ARG_TYPE_HANDLE:
     case NACL_SRPC_ARG_TYPE_INVALID:
     case NACL_SRPC_ARG_TYPE_OBJECT:
     case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
     default:
       break;
      }
    }
    return ins + array_outs;
  }

  static uint32_t ArgsLength(const NaClSrpcArg* index[]) {
    uint32_t i;

    for (i = 0; (i < NACL_SRPC_MAX_ARGS) && NULL != index[i]; ++i) {
      // Empty body.  Avoids warning.
    }
    return i;
  }

  static bool GetObject(void** obj, NPVariant var) {
    *obj = NULL;
    if (NPVARIANT_IS_OBJECT(var)) {
      *obj = reinterpret_cast<void*>(NPVARIANT_TO_OBJECT(var));
      return true;
    }
    return false;
  }

 private:

  HandleType* handle_;
  PortablePluginInterface* plugin_interface_;
};

}  // namespace nacl_srpc

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_SCRIPTABLE_HANDLE_H_
