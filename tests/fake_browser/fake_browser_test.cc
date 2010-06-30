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

#include <dlfcn.h>

#include <map>
#include <string>
#include <queue>
#include <vector>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/third_party/npapi/files/include/npupp.h"
#include "native_client/src/trusted/service_runtime/nacl_assert.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"


#define CheckRetval(rc) ASSERT_EQ((rc), NPERR_NO_ERROR)

#define NOT_IMPLEMENTED() ASSERT_MSG(0, "Not implemented")

typedef NPError (*NPInitializeType)(NPNetscapeFuncs*, NPPluginFuncs*);
typedef NPError (*NPGetEntryPointsType)(NPPluginFuncs*);
typedef void (*NPShutdownType)();

class Callback;

NPPluginFuncs plugin_funcs;
std::map<std::string, char*> interned_strings;
std::queue<Callback> callback_queue;
std::vector<NPObject*> all_objects;
std::map<std::string, std::string> url_to_filename;

void fb_NPN_ReleaseObject(NPObject* npobj);
NPObject* MakeWindowObject();


class Callback {
public:
  std::string filename;
  void* notify_data;
  void run(NPP instance) {
    NPStream stream;
    memset(reinterpret_cast<void*>(&stream), 0, sizeof(stream));
    stream.notifyData = notify_data;
    stream.url = "http://localhost/some/url";
    NPBool is_seekable = true;
    NPMIMEType mime_type = const_cast<char*>("some_mime_type");
    uint16 requested_type;
    // NPP_NewStream
    CheckRetval(plugin_funcs.newstream(instance, mime_type, &stream,
                                       is_seekable, &requested_type));
    ASSERT_EQ(requested_type, NP_ASFILEONLY);
    // NPP_StreamAsFile
    plugin_funcs.asfile(instance, &stream, filename.c_str());
  };
};


void* fb_NPN_MemAlloc(uint32_t size) {
  return malloc(size);
}

void fb_NPN_MemFree(void* ptr) {
  free(ptr);
}

NPIdentifier fb_NPN_GetStringIdentifier(const NPUTF8* name) {
  if (interned_strings[std::string(name)] == NULL) {
    interned_strings[std::string(name)] = strdup(name);
  }
  return interned_strings[std::string(name)];
}

bool fb_NPN_IdentifierIsString(NPIdentifier identifier) {
  UNREFERENCED_PARAMETER(identifier);
  return true;
}

NPUTF8* fb_NPN_UTF8FromIdentifier(NPIdentifier identifier) {
  return static_cast<NPUTF8*>(identifier);
}

NPError fb_NPN_GetValue(NPP instance, NPNVariable variable, void* value) {
  UNREFERENCED_PARAMETER(instance);
  if (variable == NPNVWindowNPObject) {
    NPObject** dest = reinterpret_cast<NPObject**>(value);
    *dest = MakeWindowObject();
    return NPERR_NO_ERROR;
  } else {
    return NPERR_GENERIC_ERROR;
  }
}

NPError fb_NPN_SetValue(NPP instance, NPPVariable variable, void* value) {
  NOT_IMPLEMENTED();
  UNREFERENCED_PARAMETER(instance);
  UNREFERENCED_PARAMETER(variable);
  UNREFERENCED_PARAMETER(value);
}

NPError fb_NPN_GetURLNotify(NPP instance, const char* url,
                            const char* target, void* notify_data) {
  UNREFERENCED_PARAMETER(instance);
  CHECK(url != NULL);
  CHECK(target == NULL);
  Callback callback;
  callback.notify_data = notify_data;
  CHECK(url_to_filename.find(url) != url_to_filename.end());
  callback.filename = url_to_filename[url];
  callback_queue.push(callback);
  return NPERR_NO_ERROR;
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
      free(const_cast<char*>(variant->value.stringValue.UTF8Characters));
      break;
    default:
      NOT_IMPLEMENTED();
      break;
  }
}

NPObject* fb_NPN_CreateObject(NPP npp, NPClass* a_class) {
  NPObject* npobj;
  if (a_class->allocate == NULL) {
    npobj = reinterpret_cast<NPObject*>(malloc(sizeof(NPObject)));
  } else {
    npobj = a_class->allocate(npp, a_class);
  }
  CHECK(npobj != NULL);
  npobj->_class = a_class;
  npobj->referenceCount = 1;
  all_objects.push_back(npobj);
  return npobj;
}

NPObject* fb_NPN_RetainObject(NPObject* npobj) {
  CHECK(npobj->referenceCount > 0);
  npobj->referenceCount++;
  return npobj;
}

void RemoveObject(NPObject* npobj) {
  for (std::vector<NPObject*>::iterator it = all_objects.begin();
       it != all_objects.end();
       it++) {
    if (*it == npobj) {
      all_objects.erase(it);
      return;
    }
  }
  ASSERT_MSG(0, "Object not found in global list");
}

void fb_NPN_ReleaseObject(NPObject* npobj) {
  CHECK(npobj->referenceCount > 0);
  if (--npobj->referenceCount == 0) {
    if (npobj->_class->deallocate == NULL) {
      free(npobj);
    } else {
      npobj->_class->deallocate(npobj);
    }
    RemoveObject(npobj);
  }
}

bool fb_NPN_Invoke(NPP npp, NPObject* npobj, NPIdentifier method_name,
                   const NPVariant* args, uint32_t arg_count,
                   NPVariant* result) {
  UNREFERENCED_PARAMETER(npp);
  return npobj->_class->invoke(npobj, method_name, args, arg_count, result);
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
  UNREFERENCED_PARAMETER(npp);
  return npobj->_class->getProperty(npobj, property_name, result);
}

bool fb_NPN_SetProperty(NPP npp, NPObject* npobj, NPIdentifier property_name,
                        const NPVariant* value) {
  UNREFERENCED_PARAMETER(npp);
  return npobj->_class->setProperty(npobj, property_name, value);
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

void InitBrowserFuncs(NPNetscapeFuncs *browser_funcs) {
  memset(reinterpret_cast<void*>(browser_funcs), 0, sizeof(*browser_funcs));
  browser_funcs->version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
  browser_funcs->size = sizeof(browser_funcs);
  browser_funcs->memalloc = fb_NPN_MemAlloc;
  browser_funcs->memfree = fb_NPN_MemFree;
  browser_funcs->getvalue = fb_NPN_GetValue;
  browser_funcs->setvalue = fb_NPN_SetValue;
  browser_funcs->geturlnotify = fb_NPN_GetURLNotify;
  // npruntime calls
  browser_funcs->getstringidentifier = fb_NPN_GetStringIdentifier;
  browser_funcs->identifierisstring = fb_NPN_IdentifierIsString;
  browser_funcs->utf8fromidentifier = fb_NPN_UTF8FromIdentifier;
  browser_funcs->releasevariantvalue = fb_NPN_ReleaseVariantValue;
  browser_funcs->createobject = fb_NPN_CreateObject;
  browser_funcs->retainobject = fb_NPN_RetainObject;
  browser_funcs->releaseobject = fb_NPN_ReleaseObject;
  browser_funcs->invoke = fb_NPN_Invoke;
  browser_funcs->invokeDefault = fb_NPN_InvokeDefault;
  browser_funcs->evaluate = fb_NPN_Evaluate;
  browser_funcs->getproperty = fb_NPN_GetProperty;
  browser_funcs->setproperty = fb_NPN_SetProperty;
  browser_funcs->removeproperty = fb_NPN_RemoveProperty;
  browser_funcs->hasproperty = fb_NPN_HasProperty;
  browser_funcs->hasmethod = fb_NPN_HasMethod;
  browser_funcs->enumerate = fb_NPN_Enumerate;
  browser_funcs->construct = fb_NPN_Construct;
}


// Fake window.location object.

bool LocationGetProperty(NPObject* npobj, NPIdentifier property_name,
                         NPVariant* result) {
  UNREFERENCED_PARAMETER(npobj);
  if (property_name == fb_NPN_GetStringIdentifier("href")) {
    STRINGZ_TO_NPVARIANT(strdup("http://localhost/page.html"), *result);
    return true;
  } else {
    return false;
  }
}

NPObject* MakeLocationObject() {
  static NPClass npclass;
  npclass.getProperty = LocationGetProperty;
  return fb_NPN_CreateObject(NULL, &npclass);
}


// Fake window object, used to get window.location.

bool WindowGetProperty(NPObject* npobj, NPIdentifier property_name,
                       NPVariant* result) {
  UNREFERENCED_PARAMETER(npobj);
  if (property_name == fb_NPN_GetStringIdentifier("location")) {
    OBJECT_TO_NPVARIANT(MakeLocationObject(), *result);
    return true;
  } else {
    return false;
  }
}

NPObject* MakeWindowObject() {
  static NPClass npclass;
  npclass.getProperty = WindowGetProperty;
  return fb_NPN_CreateObject(NULL, &npclass);
}


void TestNewAndDestroy() {
  printf("Test NPP_New() and NPP_Destroy()...\n");

  NPMIMEType mime_type = const_cast<char*>("application/x-nacl-srpc");
  NPP plugin_instance = new NPP_t;
  int argc = 0;
  char** arg_names = NULL;
  char** arg_values = NULL;
  NPSavedData* saved_data = NULL;
  // NPP_New
  CheckRetval(plugin_funcs.newp(mime_type, plugin_instance, NP_EMBED,
                                argc, arg_names, arg_values, saved_data));

  // This reproduced a double-free bug:
  // http://code.google.com/p/nativeclient/issues/detail?id=653.
  // It tests the case where the plugin NPObject is never reffed by
  // the browser.

  // NPP_Destroy
  CheckRetval(plugin_funcs.destroy(plugin_instance, NULL));
}

void TestHelloWorldMethod(const char* nexe_url) {
  NPMIMEType mime_type = const_cast<char*>("application/x-nacl-srpc");
  NPP plugin_instance = new NPP_t;
  int argc = 0;
  char** arg_names = NULL;
  char** arg_values = NULL;
  NPSavedData* saved_data = NULL;
  // NPP_New
  CheckRetval(plugin_funcs.newp(mime_type, plugin_instance, NP_EMBED,
                                argc, arg_names, arg_values, saved_data));

  NPObject* plugin_obj;
  // NPP_GetValue
  CheckRetval(plugin_funcs.getvalue(plugin_instance,
                                    NPPVpluginScriptableNPObject, &plugin_obj));
  NPVariant url;
  STRINGZ_TO_NPVARIANT(nexe_url, url);
  bool is_ok = fb_NPN_SetProperty(plugin_instance, plugin_obj,
                                  fb_NPN_GetStringIdentifier("src"), &url);
  CHECK(is_ok);
  // The following call is optional.  Leaving the object referenced
  // outside the plugin helps to trigger bug 652, because the
  // NPP_Destroy() call does not free the plugin's NPObjects.
  //fb_NPN_ReleaseObject(plugin_obj);

  while (!callback_queue.empty()) {
    callback_queue.front().run(plugin_instance);
    callback_queue.pop();
  }

  // Test invoking an SRPC method.
  NPVariant result;
  is_ok = fb_NPN_Invoke(plugin_instance, plugin_obj,
                        fb_NPN_GetStringIdentifier("helloworld"), NULL, 0,
                        &result);
  CHECK(is_ok);
  CHECK(NPVARIANT_IS_STRING(result));
  std::string actual(result.value.stringValue.UTF8Characters,
                     result.value.stringValue.UTF8Length);
  std::string expected = "hello, world.";
  if (actual != expected) {
    fprintf(stderr, "Expected '%s' but got '%s'",
            expected.c_str(), actual.c_str());
    abort();
  }
  fb_NPN_ReleaseVariantValue(&result);

  printf("object count = %"NACL_PRIuS"\n", all_objects.size());

  // NPP_Destroy
  CheckRetval(plugin_funcs.destroy(plugin_instance, NULL));

  printf("object count = %"NACL_PRIuS"\n", all_objects.size());

  // Uncomment to reproduce the bug
  // http://code.google.com/p/nativeclient/issues/detail?id=652:
  //reverse(all_objects.begin(), all_objects.end());

  // We do not use an iterator to iterate across the vector, because
  // the vector can change during the iteration:  deallocate() can call
  // NPN_ReleaseObject().  However, that is actually a bug for an
  // invalidate()d object.
  // See http://code.google.com/p/nativeclient/issues/detail?id=652
  // TODO(mseaborn): Ensure that all_objects does not change here.
  int count = 0;
  while (all_objects.size() > 0) {
    NPObject* npobj = all_objects.front();
    all_objects.erase(all_objects.begin());
    printf("forcibly destroy object %i (%"NACL_PRIuS" remaining)...\n",
           count++, all_objects.size());
    if (npobj->_class->invalidate != NULL) {
      npobj->_class->invalidate(npobj);
    }
    if (npobj->_class->deallocate != NULL) {
      npobj->_class->deallocate(npobj);
    } else {
      free(npobj);
    }
  }
}

int main(int argc, char** argv) {
  NaClLogModuleInit();

  // Usage: fake_browser_test plugin_path [leafname filename]*
  CHECK(argc >= 2);
  char* plugin_file = argv[1];
  int arg;
  // Read URL->filename mapping.
  for(arg = 2; arg + 1 < argc; arg += 2) {
    std::string url = std::string("http://localhost/") + argv[arg];
    std::string filename = argv[arg + 1];
    url_to_filename[url] = filename;
  }
  CHECK(arg == argc);

  void* dl_handle = dlopen(plugin_file, RTLD_NOW | RTLD_LOCAL);
  CHECK(dl_handle != NULL);

  NPNetscapeFuncs browser_funcs;
  InitBrowserFuncs(&browser_funcs);

  NPInitializeType initialize_func =
    reinterpret_cast<NPInitializeType>(
      reinterpret_cast<uintptr_t>(dlsym(dl_handle, "NP_Initialize")));
  CHECK(initialize_func != NULL);
  CheckRetval(initialize_func(&browser_funcs, &plugin_funcs));

  if (NACL_OSX) {
    NPGetEntryPointsType get_entry_points =
      reinterpret_cast<NPGetEntryPointsType>(
        reinterpret_cast<uintptr_t>(dlsym(dl_handle, "NP_GetEntryPoints")));
    CHECK(get_entry_points != NULL);
    CheckRetval(get_entry_points(&plugin_funcs));
  }
  // Sanity check.
  CHECK(plugin_funcs.newp != NULL);

  TestNewAndDestroy();

  printf("Test running srpc_hw...\n");
  TestHelloWorldMethod("http://localhost/srpc_hw.nexe");

  printf("Test running npapi_hw...\n");
  TestHelloWorldMethod("http://localhost/npapi_hw.nexe");

  NPShutdownType shutdown_func =
    reinterpret_cast<NPShutdownType>(
      reinterpret_cast<uintptr_t>(dlsym(dl_handle, "NP_Shutdown")));
  CHECK(shutdown_func != NULL);
  shutdown_func();

  int rc = dlclose(dl_handle);
  CHECK(rc == 0);

  return 0;
}
