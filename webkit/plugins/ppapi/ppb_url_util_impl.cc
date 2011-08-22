// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_url_util_impl.h"

#include "googleurl/src/gurl.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/shared_impl/url_util_impl.h"
#include "ppapi/shared_impl/var.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_var_impl.h"
#include "webkit/plugins/ppapi/resource_tracker.h"
#include "webkit/plugins/ppapi/string.h"

using ppapi::StringVar;
using ppapi::URLUtilImpl;

namespace webkit {
namespace ppapi {

namespace {

// Returns the PP_Module associated with the given string, or 0 on failure.
PP_Module GetModuleFromVar(PP_Var string_var) {
  StringVar* str = StringVar::FromPPVar(string_var);
  if (!str)
    return 0;
  return str->pp_module();
}

// Sets |*security_origin| to be the WebKit security origin associated with the
// document containing the given plugin instance. On success, returns true. If
// the instance is invalid, returns false and |*security_origin| will be
// unchanged.
bool SecurityOriginForInstance(PP_Instance instance_id,
                               WebKit::WebSecurityOrigin* security_origin) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return false;

  WebKit::WebElement plugin_element = instance->container()->element();
  *security_origin = plugin_element.document().securityOrigin();
  return true;
}

PP_Var Canonicalize(PP_Var url, PP_URLComponents_Dev* components) {
  return URLUtilImpl::Canonicalize(GetModuleFromVar(url), url, components);
}

PP_Var ResolveRelativeToURL(PP_Var base_url,
                            PP_Var relative,
                            PP_URLComponents_Dev* components) {
  return URLUtilImpl::ResolveRelativeToURL(GetModuleFromVar(base_url),
                                           base_url, relative, components);
}

PP_Var ResolveRelativeToDocument(PP_Instance instance_id,
                                 PP_Var relative,
                                 PP_URLComponents_Dev* components) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return PP_MakeNull();

  StringVar* relative_string = StringVar::FromPPVar(relative);
  if (!relative_string)
    return PP_MakeNull();

  WebKit::WebElement plugin_element = instance->container()->element();
  GURL document_url = plugin_element.document().baseURL();
  return URLUtilImpl::GenerateURLReturn(
      instance->module()->pp_module(),
      document_url.Resolve(relative_string->value()),
      components);
}

PP_Bool IsSameSecurityOrigin(PP_Var url_a, PP_Var url_b) {
  return URLUtilImpl::IsSameSecurityOrigin(url_a, url_b);
}

PP_Bool DocumentCanRequest(PP_Instance instance, PP_Var url) {
  StringVar* url_string = StringVar::FromPPVar(url);
  if (!url_string)
    return PP_FALSE;

  WebKit::WebSecurityOrigin security_origin;
  if (!SecurityOriginForInstance(instance, &security_origin))
    return PP_FALSE;

  GURL gurl(url_string->value());
  if (!gurl.is_valid())
    return PP_FALSE;

  return BoolToPPBool(security_origin.canRequest(gurl));
}

PP_Bool DocumentCanAccessDocument(PP_Instance active, PP_Instance target) {
  WebKit::WebSecurityOrigin active_origin;
  if (!SecurityOriginForInstance(active, &active_origin))
    return PP_FALSE;

  WebKit::WebSecurityOrigin target_origin;
  if (!SecurityOriginForInstance(active, &target_origin))
    return PP_FALSE;

  return BoolToPPBool(active_origin.canAccess(target_origin));
}

PP_Var GetDocumentURL(PP_Instance instance_id,
                      PP_URLComponents_Dev* components) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return PP_MakeNull();

  WebKit::WebDocument document = instance->container()->element().document();
  return URLUtilImpl::GenerateURLReturn(instance->module()->pp_module(),
                                        document.url(), components);
}

PP_Var GetPluginInstanceURL(PP_Instance instance_id,
                            PP_URLComponents_Dev* components) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return PP_MakeNull();

  const GURL& url = instance->plugin_url();
  return URLUtilImpl::GenerateURLReturn(instance->module()->pp_module(),
                                        url, components);
}

const PPB_URLUtil_Dev ppb_url_util = {
  &Canonicalize,
  &ResolveRelativeToURL,
  &ResolveRelativeToDocument,
  &IsSameSecurityOrigin,
  &DocumentCanRequest,
  &DocumentCanAccessDocument,
  &GetDocumentURL,
  &GetPluginInstanceURL
};

}  // namespace

// static
const PPB_URLUtil_Dev* PPB_URLUtil_Impl::GetInterface() {
  return &ppb_url_util;
}

}  // namespace ppapi
}  // namespace webkit

