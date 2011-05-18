// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/tests/fake_browser_ppapi/fake_url_util.h"

#include <string>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_var.h"
#include "native_client/tests/fake_browser_ppapi/fake_url_loader.h"
#include "native_client/tests/fake_browser_ppapi/utility.h"

#include "ppapi/c/pp_errors.h"

using fake_browser_ppapi::DebugPrintf;
using ppapi_proxy::PluginVar;

namespace fake_browser_ppapi {

namespace {

struct PP_Var Canonicalize(struct PP_Var url,
                           struct PP_URLComponents_Dev* components) {
  // Return values as if none of these components exist, the ppapi plugin
  // doesn't require any of these normally, but needs valid values.
  if (components) {
    components->scheme.begin = 0;
    components->scheme.len = -1;
    components->username.begin = 0;
    components->username.len = -1;
    components->password.begin = 0;
    components->password.len = -1;
    components->host.begin = 0;
    components->host.len = -1;
    components->port.begin = 0;
    components->port.len = -1;
    components->path.begin = 0;
    components->path.len = -1;
    components->query.begin = 0;
    components->query.len = -1;
    components->ref.begin = 0;
    components->ref.len = -1;
  }
  // TODO(sehr,polina) This could easily be 'return url;' but refcounting
  // in fake_browser_ppapi is broken.
  std::string url_str = PluginVar::PPVarToString(url);
  return PluginVar::StringToPPVar(0, url_str);
}


struct PP_Var ResolveRelativeToURL(struct PP_Var base_url,
                                   struct PP_Var relative_string,
                                   struct PP_URLComponents_Dev* components) {
  std::string base_url_str = PluginVar::PPVarToString(base_url);
  std::string relative_string_str = PluginVar::PPVarToString(relative_string);
  std::string resolved_str;
  UNREFERENCED_PARAMETER(components);
  // This is a very limited version of canonical URL matching.  It's just
  // enough for simple testing.
  DebugPrintf("ResolveRelativeToURL: %s %s\n",
              base_url_str.c_str(), relative_string_str.c_str());
  size_t last_slash = base_url_str.rfind("/");
  // If there's no slash in the url, it doesn't even have a scheme, which
  // is clearly an error.
  if (last_slash == std::string::npos || last_slash == 0) {
    return PP_MakeNull();
  }
  if (base_url_str[last_slash - 1] == '/' &&
      base_url_str.substr(last_slash).find(".") == std::string::npos) {
    // If the last slash is part of the // before the host and the last slash
    // does not precede "foo.bar", then it is a hostname/path, so just append.
    resolved_str = base_url_str + "/" + relative_string_str;
  } else {
    // Otherwise, the last slash precedes a file name, which we strip off.
    resolved_str = base_url_str.substr(0, last_slash + 1) + relative_string_str;
  }
  return PluginVar::StringToPPVar(0, resolved_str);
}

struct PP_Var ResolveRelativeToDocument(
    PP_Instance instance,
    struct PP_Var relative_string,
    struct PP_URLComponents_Dev* components) {
  UNREFERENCED_PARAMETER(instance);
  UNREFERENCED_PARAMETER(relative_string);
  UNREFERENCED_PARAMETER(components);
  NACL_UNIMPLEMENTED();
  return PP_MakeNull();
}

PP_Bool IsSameSecurityOrigin(struct PP_Var url_a, struct PP_Var url_b) {
  UNREFERENCED_PARAMETER(url_a);
  UNREFERENCED_PARAMETER(url_b);
  NACL_UNIMPLEMENTED();
  return PP_FALSE;
}

PP_Bool DocumentCanRequest(PP_Instance instance, struct PP_Var url) {
  UNREFERENCED_PARAMETER(instance);
  UNREFERENCED_PARAMETER(url);
  NACL_UNIMPLEMENTED();
  return PP_FALSE;
}

PP_Bool DocumentCanAccessDocument(PP_Instance active, PP_Instance target) {
  UNREFERENCED_PARAMETER(active);
  UNREFERENCED_PARAMETER(target);
  NACL_UNIMPLEMENTED();
  return PP_FALSE;
}

struct PP_Var GetDocumentURL(PP_Instance instance,
                             struct PP_URLComponents_Dev* components) {
  UNREFERENCED_PARAMETER(components);
  return PluginVar::StringToPPVar(instance,
                                  fake_browser_ppapi::g_nacl_ppapi_url_path);
}

struct PP_Var GetPluginInstanceURL(PP_Instance instance,
                                   struct PP_URLComponents_Dev* components) {
  UNREFERENCED_PARAMETER(instance);
  UNREFERENCED_PARAMETER(components);
  return PluginVar::StringToPPVar(instance,
                                  fake_browser_ppapi::g_nacl_ppapi_url_path);
}

}  // namespace


const struct PPB_URLUtil_Dev* URLUtil_Dev::GetInterface() {
  static const PPB_URLUtil_Dev url_util_interface = {
      Canonicalize,
      ResolveRelativeToURL,
      ResolveRelativeToDocument,
      IsSameSecurityOrigin ,
      DocumentCanRequest,
      DocumentCanAccessDocument,
      GetDocumentURL,
      GetPluginInstanceURL
  };
  return &url_util_interface;
}

}  // namespace fake_browser_ppapi
