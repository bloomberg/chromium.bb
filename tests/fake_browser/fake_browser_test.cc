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

#include <algorithm>
#include <map>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/third_party/npapi/files/include/npupp.h"
#include "native_client/src/trusted/service_runtime/nacl_assert.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"


#if NACL_WINDOWS

#define RTLD_NOW 0
#define RTLD_LOCAL 0

void* dlopen(const char* filename, int flag) {
  return reinterpret_cast<void*>(LoadLibrary(filename));
}

void* dlsym(void *handle, const char* symbol_name) {
  return GetProcAddress(reinterpret_cast<HMODULE>(handle), symbol_name);
}

int dlclose(void* handle) {
  return !FreeLibrary(reinterpret_cast<HMODULE>(handle));
}

#else
# include <dlfcn.h>
#endif


#define CheckRetval(rc) ASSERT_EQ((rc), NPERR_NO_ERROR)

#define NOT_IMPLEMENTED() ASSERT_MSG(0, "Not implemented")

#if NACL_LINUX || NACL_OSX
typedef NPError (OSCALL *NPInitializeType)(NPNetscapeFuncs*, NPPluginFuncs*);
#else
typedef NPError (OSCALL *NPInitializeType)(NPNetscapeFuncs*);
#endif
typedef NPError (OSCALL *NPGetEntryPointsType)(NPPluginFuncs*);
typedef void (OSCALL *NPShutdownType)();

class Callback {
public:
  virtual ~Callback() { }
  virtual void run(NPP instance) = 0;
};

NPPluginFuncs plugin_funcs;
std::map<std::string, char*> interned_strings;
std::vector<NPObject*> all_objects;
std::map<std::string, std::string> url_to_filename;
bool npn_calls_allowed = true;
bool exception_expected = false;
std::queue<Callback*> callback_queue;
struct NaClMutex callback_mutex;
struct NaClCondVar callback_condvar;

void fb_NPN_ReleaseObject(NPObject* npobj);
NPObject* MakeWindowObject();


class URLFetchCallback: public Callback {
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

class AsyncCallCallback: public Callback {
public:
  void (*func)(void* argument);
  void* argument;
  void run(NPP instance) {
    UNREFERENCED_PARAMETER(instance);
    func(argument);
  }
};

void QueueCallback(Callback* cb) {
  NaClXMutexLock(&callback_mutex);
  callback_queue.push(cb);
  NaClXCondVarSignal(&callback_condvar);
  NaClXMutexUnlock(&callback_mutex);
}

void RunQueuedCallbacks(NPP plugin_instance) {
  while (!callback_queue.empty()) {
    Callback* cb = callback_queue.front();
    callback_queue.pop();
    cb->run(plugin_instance);
    delete cb;
  }
}

void AwaitCallbacks(NPP plugin_instance) {
  NaClXMutexLock(&callback_mutex);
  while (callback_queue.empty()) {
    NaClXCondVarWait(&callback_condvar, &callback_mutex);
  }
  RunQueuedCallbacks(plugin_instance);
  NaClXMutexUnlock(&callback_mutex);
}

std::string QuotedString(std::string str) {
  std::stringstream stream;
  for(size_t i = 0; i < str.size(); i++) {
    if (32 <= str[i] && str[i] < 127) {
      stream << str[i];
    } else {
      char buf[10];
      sprintf(buf, "\\x%02x", static_cast<unsigned char>(str[i]));
      stream << buf;
    }
  }
  return stream.str();
}

void AssertStringsEqual(std::string str1, std::string str2) {
  if (str1 != str2) {
    fprintf(stderr, "Assertion failed: \"%s\" != \"%s\"",
            QuotedString(str1).c_str(), QuotedString(str2).c_str());
    abort();
  }
}


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
  char* string = strdup(static_cast<char*>(identifier));
  CHECK(string != NULL);
  return string;
}

NPError fb_NPN_GetValue(NPP instance, NPNVariable variable, void* value) {
  UNREFERENCED_PARAMETER(instance);
  CHECK(npn_calls_allowed);
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
  CHECK(npn_calls_allowed);
  CHECK(url != NULL);
  CHECK(target == NULL);
  URLFetchCallback* callback = new URLFetchCallback;
  callback->notify_data = notify_data;
  CHECK(url_to_filename.find(url) != url_to_filename.end());
  callback->filename = url_to_filename[url];
  QueueCallback(callback);
  return NPERR_NO_ERROR;
}

void fb_NPN_PluginThreadAsyncCall(NPP instance, void (*func)(void* argument),
                                  void* argument) {
  UNREFERENCED_PARAMETER(instance);
  CHECK(npn_calls_allowed);
  AsyncCallCallback* callback = new AsyncCallCallback;
  callback->func = func;
  callback->argument = argument;
  QueueCallback(callback);
}

void fb_NPN_ReleaseVariantValue(NPVariant* variant) {
  CHECK(npn_calls_allowed);
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
  CHECK(npn_calls_allowed);
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
  CHECK(npn_calls_allowed);
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
  CHECK(npn_calls_allowed);
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
  CHECK(npn_calls_allowed);
  UNREFERENCED_PARAMETER(npp);
  return npobj->_class->invoke(npobj, method_name, args, arg_count, result);
}

bool fb_NPN_InvokeDefault(NPP npp, NPObject* npobj, const NPVariant* args,
                          uint32_t arg_count, NPVariant* result) {
  CHECK(npn_calls_allowed);
  UNREFERENCED_PARAMETER(npp);
  return npobj->_class->invokeDefault(npobj, args, arg_count, result);
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
  CHECK(npn_calls_allowed);
  UNREFERENCED_PARAMETER(npp);
  return npobj->_class->getProperty(npobj, property_name, result);
}

bool fb_NPN_SetProperty(NPP npp, NPObject* npobj, NPIdentifier property_name,
                        const NPVariant* value) {
  CHECK(npn_calls_allowed);
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

void fb_NPN_SetException(NPObject* npobj, const NPUTF8* message) {
  CHECK(npn_calls_allowed);
  UNREFERENCED_PARAMETER(npobj);
  printf("NPN_SetException called: %s\n", message);
  if (exception_expected) {
    exception_expected = false;
  } else {
    abort();
  }
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
  browser_funcs->pluginthreadasynccall = fb_NPN_PluginThreadAsyncCall;
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
  browser_funcs->setexception = fb_NPN_SetException;
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

bool WindowInvoke(NPObject* npobj, NPIdentifier method_name,
                  const NPVariant* args, uint32_t arg_count,
                  NPVariant* result) {
  UNREFERENCED_PARAMETER(npobj);
  UNREFERENCED_PARAMETER(method_name);
  UNREFERENCED_PARAMETER(args);
  UNREFERENCED_PARAMETER(arg_count);
  UNREFERENCED_PARAMETER(result);
  return false;
}

NPObject* MakeWindowObject() {
  static NPClass npclass;
  npclass.invoke = WindowInvoke;
  npclass.getProperty = WindowGetProperty;
  return fb_NPN_CreateObject(NULL, &npclass);
}


// Callback object for registering with __setAsyncCallback().

struct TestCallback: public NPObject {
  std::vector<std::string> got_calls;
};

NPObject* TestCallbackAllocate(NPP npp, NPClass* this_class) {
  UNREFERENCED_PARAMETER(npp);
  UNREFERENCED_PARAMETER(this_class);
  return new TestCallback;
}

void TestCallbackDeallocate(NPObject* npobj) {
  TestCallback* self = static_cast<TestCallback*>(npobj);
  delete self;
}

bool TestCallbackInvokeDefault(NPObject* npobj, const NPVariant* args,
                               uint32_t argCount, NPVariant* result) {
  TestCallback* self = static_cast<TestCallback*>(npobj);
  ASSERT_EQ(argCount, 1);
  CHECK(NPVARIANT_IS_STRING(args[0]));
  std::string data(args[0].value.stringValue.UTF8Characters,
                   args[0].value.stringValue.UTF8Length);
  self->got_calls.push_back(data);
  VOID_TO_NPVARIANT(*result);
  return true;
}

TestCallback* MakeTestCallbackObject() {
  static NPClass npclass;
  npclass.allocate = TestCallbackAllocate;
  npclass.deallocate = TestCallbackDeallocate;
  npclass.invokeDefault = TestCallbackInvokeDefault;
  return static_cast<TestCallback*>(fb_NPN_CreateObject(NULL, &npclass));
}


void DestroyPluginInstance(NPP plugin_instance, bool reverse_deallocate) {
  printf("object count = %"NACL_PRIuS"\n", all_objects.size());

  // NPP_Destroy
  CheckRetval(plugin_funcs.destroy(plugin_instance, NULL));

  printf("object count = %"NACL_PRIuS"\n", all_objects.size());

  if (reverse_deallocate) {
    // This helps to test for this bug:
    // http://code.google.com/p/nativeclient/issues/detail?id=652
    reverse(all_objects.begin(), all_objects.end());
  }

  // We avoid using an iterator to iterate across the vector in case
  // all_objects changes during the iteration.  However, this should
  // not happen given the uses of CHECK(npn_calls_allowed).
  // See http://code.google.com/p/nativeclient/issues/detail?id=652
  npn_calls_allowed = false;
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
  npn_calls_allowed = true;
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

  ASSERT_EQ(all_objects.size(), 0);
}

void TestHelloWorldMethod(const char* nexe_url, bool reverse_deallocate) {
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
  RunQueuedCallbacks(plugin_instance);

  // Test invoking an SRPC method.
  NPVariant result;
  is_ok = fb_NPN_Invoke(plugin_instance, plugin_obj,
                        fb_NPN_GetStringIdentifier("helloworld"), NULL, 0,
                        &result);
  CHECK(is_ok);
  CHECK(NPVARIANT_IS_STRING(result));
  std::string actual(result.value.stringValue.UTF8Characters,
                     result.value.stringValue.UTF8Length);
  AssertStringsEqual(actual, "hello, world.");
  fb_NPN_ReleaseVariantValue(&result);
  DestroyPluginInstance(plugin_instance, reverse_deallocate);
}

// This tests for a plugin bug that caused a crash,
// http://code.google.com/p/nativeclient/issues/detail?id=672.
// Test running an executable that does not act as an SRPC server.
void TestMissingSrpcInit() {
  printf("Test failure to init SRPC...\n");
  const char* nexe_url = "http://localhost/hello_world.nexe";

  NPMIMEType mime_type = const_cast<char*>("application/x-nacl-srpc");
  NPP plugin_instance = new NPP_t;
  CheckRetval(plugin_funcs.newp(mime_type, plugin_instance, NP_EMBED,
                                0, NULL, NULL, NULL));

  NPObject* plugin_obj;
  CheckRetval(plugin_funcs.getvalue(plugin_instance,
                                    NPPVpluginScriptableNPObject, &plugin_obj));
  NPVariant url;
  STRINGZ_TO_NPVARIANT(nexe_url, url);
  bool is_ok = fb_NPN_SetProperty(plugin_instance, plugin_obj,
                                  fb_NPN_GetStringIdentifier("src"), &url);
  CHECK(is_ok);
  RunQueuedCallbacks(plugin_instance);

  // Try launching another process.  This also tests for a bug that
  // caused a crash.
  is_ok = fb_NPN_SetProperty(plugin_instance, plugin_obj,
                             fb_NPN_GetStringIdentifier("src"), &url);
  CHECK(is_ok);
  fb_NPN_ReleaseObject(plugin_obj);
  RunQueuedCallbacks(plugin_instance);

  DestroyPluginInstance(plugin_instance, false);
}

void TestAsyncMessages() {
  printf("Test asynchronous messages...\n");
  const char* nexe_url = "http://localhost/async_message_test.nexe";

  NPMIMEType mime_type = const_cast<char*>("application/x-nacl-srpc");
  NPP plugin_instance = new NPP_t;
  CheckRetval(plugin_funcs.newp(mime_type, plugin_instance, NP_EMBED,
                                0, NULL, NULL, NULL));

  NPObject* plugin_obj;
  CheckRetval(plugin_funcs.getvalue(plugin_instance,
                                    NPPVpluginScriptableNPObject, &plugin_obj));
  NPVariant url;
  STRINGZ_TO_NPVARIANT(nexe_url, url);
  bool success = fb_NPN_SetProperty(plugin_instance, plugin_obj,
                                    fb_NPN_GetStringIdentifier("src"), &url);
  CHECK(success);
  RunQueuedCallbacks(plugin_instance);

  // Register async callback.
  TestCallback* callback = MakeTestCallbackObject();
  NPVariant args[2];
  OBJECT_TO_NPVARIANT(callback, args[0]);
  NPVariant result;
  success = fb_NPN_Invoke(plugin_instance, plugin_obj,
                          fb_NPN_GetStringIdentifier("__setAsyncCallback"),
                          args, 1, &result);
  CHECK(success);
  CHECK(NPVARIANT_IS_VOID(result));

  // Attempting to register another callback should be rejected.  The
  // plugin should not spawn two threads to listen on the same socket.
  exception_expected = true;
  success = fb_NPN_Invoke(plugin_instance, plugin_obj,
                          fb_NPN_GetStringIdentifier("__setAsyncCallback"),
                          args, 1, &result);
  CHECK(!success);
  CHECK(!exception_expected);

  fb_NPN_ReleaseObject(plugin_obj);

  printf("waiting for callback...\n");
  // This can block indefinitely.  Might a timeout be better?
  while (callback->got_calls.size() < 3) {
    AwaitCallbacks(plugin_instance);
    printf("received %"NACL_PRIuS" messages\n", callback->got_calls.size());
  }
  ASSERT_EQ(callback->got_calls.size(), 3);
  AssertStringsEqual(callback->got_calls[0],
                     "Aye carumba! This is an async message.");
  char message2[] = "Message\0containing\0NULLs";
  AssertStringsEqual(callback->got_calls[1],
                     std::string(message2, sizeof(message2)));
  AssertStringsEqual(callback->got_calls[2],
                     "Top-bit-set bytes: \xc2\x80 and \xc3\xbf");

  // Test sending a message.
  printf("try sending a message...\n");
  STRINGZ_TO_NPVARIANT("Crikey! Another async message. "
                       "Top-bit-set bytes: \xc2\x80 and \xc3\xbf", args[0]);
  success = fb_NPN_Invoke(plugin_instance, plugin_obj,
                          fb_NPN_GetStringIdentifier("__sendAsyncMessage0"),
                          args, 1, &result);
  CHECK(success);
  // Test getting a response back.
  printf("waiting for response...\n");
  while (callback->got_calls.size() < 4) {
    AwaitCallbacks(plugin_instance);
  }
  ASSERT_EQ(callback->got_calls.size(), 4);
  AssertStringsEqual(callback->got_calls[3],
                     "Echoed: Crikey! Another async message. "
                     "Top-bit-set bytes: \xc2\x80 and \xc3\xbf");

  // Test attempting to send strings that cannot be converted to byte
  // strings.  These should be rejected.
  STRINGZ_TO_NPVARIANT("Char too big: \xc4\x80", args[0]);
  exception_expected = true;
  success = fb_NPN_Invoke(plugin_instance, plugin_obj,
                          fb_NPN_GetStringIdentifier("__sendAsyncMessage0"),
                          args, 1, &result);
  CHECK(!success);
  CHECK(!exception_expected);

  // Test sending a message with a file descriptor.
  // Get an example FD first.
  success = fb_NPN_Invoke(plugin_instance, plugin_obj,
                          fb_NPN_GetStringIdentifier("__defaultSocketAddress"),
                          NULL, 0, &result);
  CHECK(success);
  CHECK(NPVARIANT_IS_OBJECT(result));
  NPObject* example_fd = NPVARIANT_TO_OBJECT(result);
  STRINGZ_TO_NPVARIANT("Message with FD.", args[0]);
  OBJECT_TO_NPVARIANT(example_fd, args[1]);
  success = fb_NPN_Invoke(plugin_instance, plugin_obj,
                          fb_NPN_GetStringIdentifier("__sendAsyncMessage1"),
                          args, 2, &result);
  CHECK(success);
  // Test getting a response back.
  while (callback->got_calls.size() < 5) {
    AwaitCallbacks(plugin_instance);
  }
  ASSERT_EQ(callback->got_calls.size(), 5);
  AssertStringsEqual(callback->got_calls[4], "Received 16 bytes, 1 FDs");
  fb_NPN_ReleaseObject(example_fd);

  fb_NPN_ReleaseObject(callback);

  DestroyPluginInstance(plugin_instance, /* reverse_deallocate= */ false);
}

int main(int argc, char** argv) {
  // Turn off stdout buffering to aid debugging in case of a crash.
  setvbuf(stdout, NULL, _IONBF, 0);

  NaClLogModuleInit();
  NaClMutexCtor(&callback_mutex);
  NaClCondVarCtor(&callback_condvar);

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
  plugin_funcs.size = sizeof(plugin_funcs);

  NPInitializeType initialize_func =
    reinterpret_cast<NPInitializeType>(
      reinterpret_cast<uintptr_t>(dlsym(dl_handle, "NP_Initialize")));
  CHECK(initialize_func != NULL);
#if NACL_LINUX || NACL_OSX
  CheckRetval(initialize_func(&browser_funcs, &plugin_funcs));
#else
  CheckRetval(initialize_func(&browser_funcs));
#endif

  if (NACL_OSX || NACL_WINDOWS) {
    NPGetEntryPointsType get_entry_points =
      reinterpret_cast<NPGetEntryPointsType>(
        reinterpret_cast<uintptr_t>(dlsym(dl_handle, "NP_GetEntryPoints")));
    CHECK(get_entry_points != NULL);
    CheckRetval(get_entry_points(&plugin_funcs));
  }
  // Sanity check.
  CHECK(plugin_funcs.newp != NULL);

  TestNewAndDestroy();

  // TODO(mseaborn): Test running both of these with
  // reverse_deallocate=false as well.  However, there are some
  // reliability problems running these multiple times.
  // See http://code.google.com/p/nativeclient/issues/detail?id=682.
  printf("Test running srpc_hw...\n");
  TestHelloWorldMethod("http://localhost/srpc_hw.nexe", true);

  printf("Test running npapi_hw...\n");
  TestHelloWorldMethod("http://localhost/npapi_hw.nexe", true);

  TestMissingSrpcInit();

  TestAsyncMessages();

  NPShutdownType shutdown_func =
    reinterpret_cast<NPShutdownType>(
      reinterpret_cast<uintptr_t>(dlsym(dl_handle, "NP_Shutdown")));
  CHECK(shutdown_func != NULL);
  shutdown_func();

  int rc = dlclose(dl_handle);
  CHECK(rc == 0);

  NaClCondVarDtor(&callback_condvar);
  NaClMutexDtor(&callback_mutex);

  return 0;
}
