/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * TODO(mseaborn): It would be nice to be able to write this in C, but
 * third_party/npapi/bindings/npruntime.h depends on
 * base/basictypes.h, which depends on C++.
 */

#include <assert.h>

#include "native_client/src/third_party/npapi/files/include/npupp.h"
#include "native_client/src/trusted/service_runtime/nacl_assert.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"


#define CheckRetval(rc) ASSERT_EQ((rc), NPERR_NO_ERROR)

#define NOT_IMPLEMENTED() ASSERT_MSG(0, "Not implemented")

namespace {

void fb_NPN_ReleaseObject(NPObject* npobj);

NPIdentifier fb_NPN_GetStringIdentifier(const NPUTF8* name) {
  // TODO(mseaborn): Intern the string.
  return reinterpret_cast<void*>(strdup(name));
}

NPError fb_NPN_GetValue(NPP instance, NPNVariable variable, void* value) {
  UNREFERENCED_PARAMETER(instance);
  UNREFERENCED_PARAMETER(variable);
  UNREFERENCED_PARAMETER(value);
  return NPERR_GENERIC_ERROR;
}

NPError fb_NPN_SetValue(NPP instance, NPPVariable variable, void* value) {
  NOT_IMPLEMENTED();
  UNREFERENCED_PARAMETER(instance);
  UNREFERENCED_PARAMETER(variable);
  UNREFERENCED_PARAMETER(value);
}

void fb_NPN_ReleaseVariantValue(NPVariant* variant) {
  switch (variant->type) {
    case NPVariantType_Void:
    case NPVariantType_Null:
    case NPVariantType_Bool:
    case NPVariantType_Int32:
    case NPVariantType_Double:
      break;
    case NPVariantType_Object:
      fb_NPN_ReleaseObject(variant->value.objectValue);
      break;
    case NPVariantType_String:
    default:
      NOT_IMPLEMENTED();
      break;
  }
}

NPObject* fb_NPN_CreateObject(NPP npp, NPClass* a_class) {
  NPObject* npobj = a_class->allocate(npp, a_class);
  npobj->_class = a_class;
  npobj->referenceCount = 1;
  return npobj;
}

NPObject* fb_NPN_RetainObject(NPObject* npobj) {
  NOT_IMPLEMENTED();
  UNREFERENCED_PARAMETER(npobj);
}

void fb_NPN_ReleaseObject(NPObject* npobj) {
  assert(npobj->referenceCount > 0);
  if (--npobj->referenceCount == 0) {
    npobj->_class->deallocate(npobj);
  }
}

bool fb_NPN_Invoke(NPP npp, NPObject* npobj, NPIdentifier method_name,
                   const NPVariant* args, uint32_t arg_count,
                   NPVariant* result) {
  NOT_IMPLEMENTED();
  UNREFERENCED_PARAMETER(npp);
  UNREFERENCED_PARAMETER(npobj);
  UNREFERENCED_PARAMETER(method_name);
  UNREFERENCED_PARAMETER(args);
  UNREFERENCED_PARAMETER(arg_count);
  UNREFERENCED_PARAMETER(result);
}

bool fb_NPN_InvokeDefault(NPP npp, NPObject* npobj, const NPVariant* args,
                          uint32_t arg_count, NPVariant* result) {
  NOT_IMPLEMENTED();
  UNREFERENCED_PARAMETER(npp);
  UNREFERENCED_PARAMETER(npobj);
  UNREFERENCED_PARAMETER(args);
  UNREFERENCED_PARAMETER(arg_count);
  UNREFERENCED_PARAMETER(result);
}

bool fb_NPN_Evaluate(NPP npp, NPObject* npobj, NPString* script,
                     NPVariant* result) {
  NOT_IMPLEMENTED();
  UNREFERENCED_PARAMETER(npp);
  UNREFERENCED_PARAMETER(npobj);
  UNREFERENCED_PARAMETER(script);
  UNREFERENCED_PARAMETER(result);
}

bool fb_NPN_GetProperty(NPP npp, NPObject* npobj, NPIdentifier property_name,
                        NPVariant* result) {
  NOT_IMPLEMENTED();
  UNREFERENCED_PARAMETER(npp);
  UNREFERENCED_PARAMETER(npobj);
  UNREFERENCED_PARAMETER(property_name);
  UNREFERENCED_PARAMETER(result);
}

bool fb_NPN_SetProperty(NPP npp, NPObject* npobj, NPIdentifier property_name,
                        const NPVariant* value) {
  NOT_IMPLEMENTED();
  UNREFERENCED_PARAMETER(npp);
  UNREFERENCED_PARAMETER(npobj);
  UNREFERENCED_PARAMETER(property_name);
  UNREFERENCED_PARAMETER(value);
}

bool fb_NPN_RemoveProperty(NPP npp, NPObject* npobj,
                           NPIdentifier property_name) {
  NOT_IMPLEMENTED();
  UNREFERENCED_PARAMETER(npp);
  UNREFERENCED_PARAMETER(npobj);
  UNREFERENCED_PARAMETER(property_name);
}

bool fb_NPN_HasProperty(NPP npp, NPObject* npobj, NPIdentifier property_name) {
  NOT_IMPLEMENTED();
  UNREFERENCED_PARAMETER(npp);
  UNREFERENCED_PARAMETER(npobj);
  UNREFERENCED_PARAMETER(property_name);
}

bool fb_NPN_HasMethod(NPP npp, NPObject* npobj, NPIdentifier method_name) {
  NOT_IMPLEMENTED();
  UNREFERENCED_PARAMETER(npp);
  UNREFERENCED_PARAMETER(npobj);
  UNREFERENCED_PARAMETER(method_name);
}

bool fb_NPN_Enumerate(NPP npp, NPObject* npobj, NPIdentifier** identifier,
                      uint32_t* count) {
  NOT_IMPLEMENTED();
  UNREFERENCED_PARAMETER(npp);
  UNREFERENCED_PARAMETER(npobj);
  UNREFERENCED_PARAMETER(identifier);
  UNREFERENCED_PARAMETER(count);
}

bool fb_NPN_Construct(NPP npp, NPObject* npobj, const NPVariant* args,
                      uint32_t arg_count, NPVariant* result) {
  NOT_IMPLEMENTED();
  UNREFERENCED_PARAMETER(npp);
  UNREFERENCED_PARAMETER(npobj);
  UNREFERENCED_PARAMETER(args);
  UNREFERENCED_PARAMETER(arg_count);
  UNREFERENCED_PARAMETER(result);
}

}  // namespace

int main() {
  NPNetscapeFuncs browser_funcs;
  NPPluginFuncs plugin_funcs;
  memset(reinterpret_cast<void*>(&browser_funcs), 0, sizeof(browser_funcs));
  browser_funcs.version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
  browser_funcs.size = sizeof(browser_funcs);
  browser_funcs.getstringidentifier = fb_NPN_GetStringIdentifier;
  browser_funcs.getvalue = fb_NPN_GetValue;
  browser_funcs.setvalue = fb_NPN_SetValue;
  // npruntime calls
  browser_funcs.releasevariantvalue = fb_NPN_ReleaseVariantValue;
  browser_funcs.createobject = fb_NPN_CreateObject;
  browser_funcs.retainobject = fb_NPN_RetainObject;
  browser_funcs.releaseobject = fb_NPN_ReleaseObject;
  browser_funcs.invoke = fb_NPN_Invoke;
  browser_funcs.invokeDefault = fb_NPN_InvokeDefault;
  browser_funcs.evaluate = fb_NPN_Evaluate;
  browser_funcs.getproperty = fb_NPN_GetProperty;
  browser_funcs.setproperty = fb_NPN_SetProperty;
  browser_funcs.removeproperty = fb_NPN_RemoveProperty;
  browser_funcs.hasproperty = fb_NPN_HasProperty;
  browser_funcs.hasmethod = fb_NPN_HasMethod;
  browser_funcs.enumerate = fb_NPN_Enumerate;
  browser_funcs.construct = fb_NPN_Construct;
  CheckRetval(NP_Initialize(&browser_funcs, &plugin_funcs));

  // Is this necessary in addition to NP_Initialize()?
  // CheckRetval(NPP_Initialize());

  NPMIMEType mime_type = const_cast<char*>("application/x-nacl-srpc");
  NPP plugin_instance = new NPP_t;
  NPSavedData* saved_data = NULL;
  int argc = 0;
  char** arg_names = NULL;
  char** arg_values = NULL;
  CheckRetval(NPP_New(mime_type, plugin_instance, NP_EMBED,
                      argc, arg_names, arg_values, saved_data));

  // TODO(mseaborn): This reproduces a plugin bug, probably the same
  // one as reported in:
  // http://code.google.com/p/nativeclient/issues/detail?id=641
  // CheckRetval(NPP_Destroy(plugin_instance, NULL));

  NPP_Shutdown();
  return 0;
}
