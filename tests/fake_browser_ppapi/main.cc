/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>

#include <cstring>
#include <string>
#include <vector>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_var.h"
#include "native_client/tests/fake_browser_ppapi/fake_core.h"
#include "native_client/tests/fake_browser_ppapi/fake_file_io.h"
#include "native_client/tests/fake_browser_ppapi/fake_file_io_trusted.h"
#include "native_client/tests/fake_browser_ppapi/fake_host.h"
#include "native_client/tests/fake_browser_ppapi/fake_instance.h"
#include "native_client/tests/fake_browser_ppapi/fake_nacl_private.h"
#include "native_client/tests/fake_browser_ppapi/fake_resource.h"
#include "native_client/tests/fake_browser_ppapi/fake_url_loader.h"
#include "native_client/tests/fake_browser_ppapi/fake_url_request_info.h"
#include "native_client/tests/fake_browser_ppapi/fake_url_response_info.h"
#include "native_client/tests/fake_browser_ppapi/fake_window.h"
#include "native_client/tests/fake_browser_ppapi/test_scriptable.h"
#include "native_client/tests/fake_browser_ppapi/utility.h"

#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_uma_private.h"

using fake_browser_ppapi::DebugPrintf;
using fake_browser_ppapi::Host;
using fake_browser_ppapi::FakeWindow;

namespace {

Host* host = NULL;

const void* FakeGetBrowserInterface(const char* interface_name) {
  DebugPrintf("PPB_GetInterface: name='%s'\n", interface_name);
  const void* ppb = NULL;
  if (std::strcmp(interface_name, PPB_CORE_INTERFACE) == 0) {
    ppb = fake_browser_ppapi::Core::GetInterface();
  } else if (std::strcmp(interface_name, PPB_INSTANCE_INTERFACE) == 0) {
    ppb = fake_browser_ppapi::Instance::GetInterface();
  } else if (std::strcmp(interface_name, PPB_VAR_DEPRECATED_INTERFACE) == 0) {
    ppb = host->var_interface();
  } else if (std::strcmp(interface_name, PPB_URLLOADER_INTERFACE) == 0) {
    ppb = fake_browser_ppapi::URLLoader::GetInterface();
  } else if (std::strcmp(interface_name,
                         PPB_URLREQUESTINFO_INTERFACE) == 0) {
    ppb = fake_browser_ppapi::URLRequestInfo::GetInterface();
  } else if (std::strcmp(interface_name,
                         PPB_URLRESPONSEINFO_INTERFACE) == 0) {
    ppb = fake_browser_ppapi::URLResponseInfo::GetInterface();
  } else if (std::strcmp(interface_name, PPB_FILEIO_DEV_INTERFACE) == 0) {
    ppb = fake_browser_ppapi::FileIO::GetInterface();
  } else if (std::strcmp(interface_name,
                         PPB_FILEIOTRUSTED_DEV_INTERFACE) == 0) {
    ppb = fake_browser_ppapi::FileIOTrusted::GetInterface();
  } else if (std::strcmp(interface_name,
                         PPB_NACL_PRIVATE_INTERFACE) == 0) {
    ppb = fake_browser_ppapi::NaClPrivate::GetInterface();
  } else if (std::strcmp(interface_name,
                         PPB_UMA_PRIVATE_INTERFACE) == 0) {
    ppb = NULL;
  } else {
    NACL_UNIMPLEMENTED();
  }
  DebugPrintf("PPB_GetInterface: value=%p\n", ppb);
  return ppb;
}

// Module ids are needed for some call APIs, but the fake browser does
// not implement the storage tracking APIs that would use a real value.
// Since we have just two modules (browser and plugin), we'll statically
// assign id of 1 to one, 100 to other.
// TODO(sehr): implement storage tracking.

// The storage allocated by the browser for the window object, etc., are
// attributed to the browser's module id.
PP_Module BrowserModuleId() {
  static PP_Module id = 100;
  return id;
}

// The storage allocated by the plugin for its scriptable objects are
// attributed to the its module id.
PP_Module PluginModuleId() {
  static PP_Module id = 1;
  return id;
}

bool ParseArgs(const char* str,
               uint32_t* argc,
               const char*** argn,
               const char*** argv) {
  std::vector<std::string> argn_vector;
  std::vector<std::string> argv_vector;
  *argc = 0;
  char* embed_arg = std::strtok(strdup(str), ";");
  while (embed_arg != NULL) {
    char* equal_loc = std::strchr(embed_arg, '=');
    if (equal_loc == NULL) {
      return false;
    }
    size_t name_length = static_cast<size_t>(equal_loc - embed_arg);
    argn_vector.push_back(std::string(embed_arg, name_length));
    argv_vector.push_back(equal_loc + 1);
    ++*argc;
    embed_arg = std::strtok(NULL, ";");
  }

  *argn = reinterpret_cast<const char**>(malloc(*argc * sizeof(*argn)));
  *argv = reinterpret_cast<const char**>(malloc(*argc * sizeof(*argv)));
  for (uint32_t i = 0; i < *argc; ++i) {
    (*argn)[i] = strdup(argn_vector[i].c_str());
    (*argv)[i] = strdup(argv_vector[i].c_str());
    DebugPrintf("ParseArgs(): arg[%u]: '%s' = '%s'\n",
                i, (*argn)[i], (*argv)[i]);
  }
  return true;
}

}  // namespace

namespace fake_browser_ppapi {

PP_Resource TrackResource(Resource* resource) {
  PP_Resource resource_id = host->TrackResource(resource);
  DebugPrintf("TrackResource: resource_id=%"NACL_PRId32"\n", resource_id);
  return resource_id;
}

Resource* GetResource(PP_Resource resource_id) {
  return host->GetResource(resource_id);
}

PP_Instance TrackInstance(Instance* instance) {
  PP_Instance instance_id = host->TrackInstance(instance);
  DebugPrintf("TrackInstance: instance_id=%"NACL_PRId32"\n", instance_id);
  return instance_id;
}

Instance* GetInstance(PP_Instance instance_id) {
  return host->GetInstance(instance_id);
}

}  // namespace fake_browser_ppapi

namespace {

// Test instance execution.
void TestInstance(PP_Module browser_module_id,
                  const PPP_Instance* instance_interface,
                  const char* page_url,
                  uint32_t argc,
                  const char** argn,
                  const char** argv) {
  DebugPrintf("TestInstance(): page url %s\n", page_url);
  // Create an instance and the corresponding id.
  fake_browser_ppapi::Instance* instance = new fake_browser_ppapi::Instance;
  PP_Instance instance_id = TrackInstance(instance);
  // Create a fake window object.
  FakeWindow window(browser_module_id, instance_id, host, page_url);
  instance->set_window(&window);
  // Create and initialize plugin instance.
  CHECK(instance_interface->DidCreate(instance_id, argc, argn, argv));
  // Test the scriptable object for the instance.
  PP_Var instance_object = instance_interface->GetInstanceObject(instance_id);
  const PPB_Var_Deprecated* var_interface =
      reinterpret_cast<const PPB_Var_Deprecated*>(
          FakeGetBrowserInterface(PPB_VAR_DEPRECATED_INTERFACE));
  TestScriptableObject(instance_object,
                       fake_browser_ppapi::Instance::GetInterface(),
                       var_interface,
                       instance_id,
                       browser_module_id);
}

}  // namespace

int main(int argc, char** argv) {
  // Turn off stdout buffering to aid debugging in case of a crash.
  setvbuf(stdout, NULL, _IONBF, 0);

  NaClLogModuleInit();

  if (argc < 5) {
    fprintf(stderr,
            "Usage: fake_browser_ppapi plugin page_url \"embed args\""
            " root_path\n");
    return 1;
  }

  const char* plugin_name = argv[1];
  host = new fake_browser_ppapi::Host(plugin_name);
  // TODO(polina): Change FakeWindow functions to not rely on host for
  // the var interface.
  host->set_var_interface(ppapi_proxy::PluginVar::GetInterface());

  // Test startup.
  CHECK(host->InitializeModule(PluginModuleId(), FakeGetBrowserInterface) ==
        PP_OK);

  // Get an instance of the plugin.
  const PPP_Instance* instance_interface =
      reinterpret_cast<const PPP_Instance*>(
          host->GetInterface(PPP_INSTANCE_INTERFACE));
  CHECK(instance_interface != NULL);
  const char* page_url = argv[2];

  // Get the embed argc/argn/argv.
  const char* embed_args = argv[3];
  uint32_t embed_argc = 0;
  const char** embed_argn = NULL;
  const char** embed_argv = NULL;
  CHECK(ParseArgs(embed_args, &embed_argc, &embed_argn, &embed_argv));

  // Set url path and local path for nexe - required by fake url loader.
  std::string url_path = page_url;
  size_t last_slash = url_path.rfind("/");
  CHECK(last_slash != std::string::npos);
  url_path.erase(last_slash, url_path.size());
  fake_browser_ppapi::g_nacl_ppapi_url_path = url_path;
  fake_browser_ppapi::g_nacl_ppapi_local_path = argv[4];

  // Test an instance.
  TestInstance(BrowserModuleId(),
               instance_interface,
               page_url,
               embed_argc,
               embed_argn,
               embed_argv);

  // Shutdown.
  host->ShutdownModule();

  // Close the plugin .so.
  delete host;

  printf("PASS\n");
  return 0;
}
