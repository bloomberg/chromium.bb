// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/web_plugin_impl.h"

#include "libcef/browser/context.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/widevine_loader.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "content/browser/plugin_service_impl.h"

namespace {

void PluginsCallbackImpl(
    CefRefPtr<CefWebPluginInfoVisitor> visitor,
    const std::vector<content::WebPluginInfo>& all_plugins) {
  CEF_REQUIRE_UIT();

  int count = 0;
  int total = static_cast<int>(all_plugins.size());

  std::vector<content::WebPluginInfo>::const_iterator it = all_plugins.begin();
  for (; it != all_plugins.end(); ++it, ++count) {
    CefRefPtr<CefWebPluginInfoImpl> info(new CefWebPluginInfoImpl(*it));
    if (!visitor->Visit(info.get(), count, total))
      break;
  }
}

#if !(BUILDFLAG(ENABLE_WIDEVINE) && BUILDFLAG(ENABLE_LIBRARY_CDMS)) || \
    defined(OS_LINUX)

void DeliverWidevineCdmError(const std::string& error_message,
                             CefRefPtr<CefRegisterCdmCallback> callback) {
  LOG(ERROR) << error_message;
  if (callback.get()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::Bind(&CefRegisterCdmCallback::OnCdmRegistrationComplete,
                   callback.get(), CEF_CDM_REGISTRATION_ERROR_NOT_SUPPORTED,
                   error_message));
  }
}

#endif

}  // namespace

// CefWebPluginInfoImpl

CefWebPluginInfoImpl::CefWebPluginInfoImpl(
    const content::WebPluginInfo& plugin_info)
    : plugin_info_(plugin_info) {}

CefString CefWebPluginInfoImpl::GetName() {
  return plugin_info_.name;
}

CefString CefWebPluginInfoImpl::GetPath() {
  return plugin_info_.path.value();
}

CefString CefWebPluginInfoImpl::GetVersion() {
  return plugin_info_.version;
}

CefString CefWebPluginInfoImpl::GetDescription() {
  return plugin_info_.desc;
}

// Global functions.

void CefVisitWebPluginInfo(CefRefPtr<CefWebPluginInfoVisitor> visitor) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  if (!visitor.get()) {
    NOTREACHED() << "invalid parameter";
    return;
  }

  if (CEF_CURRENTLY_ON_UIT()) {
    content::PluginServiceImpl::GetInstance()->GetPlugins(
        base::Bind(PluginsCallbackImpl, visitor));
  } else {
    // Execute on the UI thread.
    CEF_POST_TASK(CEF_UIT, base::Bind(CefVisitWebPluginInfo, visitor));
  }
}

void CefRefreshWebPlugins() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  // No thread affinity.
  content::PluginServiceImpl::GetInstance()->RefreshPlugins();
}

void CefUnregisterInternalWebPlugin(const CefString& path) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  if (path.empty()) {
    NOTREACHED() << "invalid parameter";
    return;
  }

  // No thread affinity.
  content::PluginServiceImpl::GetInstance()->UnregisterInternalPlugin(
      base::FilePath(path));
}

void CefRegisterWebPluginCrash(const CefString& path) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  if (path.empty()) {
    NOTREACHED() << "invalid parameter";
    return;
  }

  if (CEF_CURRENTLY_ON_IOT()) {
    content::PluginServiceImpl::GetInstance()->RegisterPluginCrash(
        base::FilePath(path));
  } else {
    // Execute on the IO thread.
    CEF_POST_TASK(CEF_IOT, base::Bind(CefRegisterWebPluginCrash, path));
  }
}

void CefIsWebPluginUnstable(const CefString& path,
                            CefRefPtr<CefWebPluginUnstableCallback> callback) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  if (path.empty() || !callback.get()) {
    NOTREACHED() << "invalid parameter";
    return;
  }

  if (CEF_CURRENTLY_ON_IOT()) {
    callback->IsUnstable(
        path, content::PluginServiceImpl::GetInstance()->IsPluginUnstable(
                  base::FilePath(path)));
  } else {
    // Execute on the IO thread.
    CEF_POST_TASK(CEF_IOT, base::Bind(CefIsWebPluginUnstable, path, callback));
  }
}

void CefRegisterWidevineCdm(const CefString& path,
                            CefRefPtr<CefRegisterCdmCallback> callback) {
#if BUILDFLAG(ENABLE_WIDEVINE) && BUILDFLAG(ENABLE_LIBRARY_CDMS)
#if defined(OS_LINUX)
  // Enforce the requirement that CefRegisterWidevineCdm() is called before
  // CefInitialize() on Linux. See comments in
  // CefWidevineLoader::AddPepperPlugins for details.
  if (CONTEXT_STATE_VALID()) {
    DeliverWidevineCdmError(
        "Widevine registration is not supported after context initialization",
        callback);
    return;
  }
#endif  // defined(OS_LINUX)

  CefWidevineLoader::GetInstance()->LoadWidevineCdm(path, callback);
#else
  DeliverWidevineCdmError("Widevine registration is not supported", callback);
#endif  // BUILDFLAG(ENABLE_WIDEVINE) && BUILDFLAG(ENABLE_LIBRARY_CDMS)
}
