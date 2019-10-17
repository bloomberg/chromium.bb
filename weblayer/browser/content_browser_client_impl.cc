// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/content_browser_client_impl.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/user_agent.h"
#include "content/public/common/web_preferences.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "weblayer/browser/browser_controller_impl.h"
#include "weblayer/browser/browser_main_parts_impl.h"
#include "weblayer/browser/weblayer_content_browser_overlay_manifest.h"
#include "weblayer/public/fullscreen_delegate.h"
#include "weblayer/public/main.h"

#if defined(OS_ANDROID)
#include "base/android/apk_assets.h"
#include "base/android/path_utils.h"
#include "weblayer/browser/android_descriptors.h"
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include "content/public/common/content_descriptors.h"
#endif

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox.h"
#include "services/service_manager/sandbox/win/sandbox_win.h"
#endif

namespace weblayer {

ContentBrowserClientImpl::ContentBrowserClientImpl(MainParams* params)
    : params_(params) {}

ContentBrowserClientImpl::~ContentBrowserClientImpl() = default;

std::unique_ptr<content::BrowserMainParts>
ContentBrowserClientImpl::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  std::unique_ptr<BrowserMainPartsImpl> browser_main_parts =
      std::make_unique<BrowserMainPartsImpl>(params_, parameters);

  return browser_main_parts;
}

std::string ContentBrowserClientImpl::GetAcceptLangs(
    content::BrowserContext* context) {
  return "en-us,en";
}

content::WebContentsViewDelegate*
ContentBrowserClientImpl::GetWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return nullptr;
}
content::DevToolsManagerDelegate*
ContentBrowserClientImpl::GetDevToolsManagerDelegate() {
  return new content::DevToolsManagerDelegate();
}

base::Optional<service_manager::Manifest>
ContentBrowserClientImpl::GetServiceManifestOverlay(base::StringPiece name) {
  if (name == content::mojom::kBrowserServiceName)
    return GetWebLayerContentBrowserOverlayManifest();
  return base::nullopt;
}

std::string ContentBrowserClientImpl::GetUserAgent() {
  std::string product = "Chrome/";
  product += params_->full_version;
  return content::BuildUserAgentFromProduct(product);
}

blink::UserAgentMetadata ContentBrowserClientImpl::GetUserAgentMetadata() {
  blink::UserAgentMetadata metadata;

  metadata.brand = params_->brand;
  metadata.full_version = params_->full_version;
  metadata.major_version = params_->major_version;
  metadata.platform = content::BuildOSCpuInfo(false);
  metadata.architecture = "";
  metadata.model = "";

  return metadata;
}

void ContentBrowserClientImpl::OverrideWebkitPrefs(
    content::RenderViewHost* render_view_host,
    content::WebPreferences* prefs) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(render_view_host);
  if (!web_contents)
    return;
  BrowserControllerImpl* browser_controller =
      BrowserControllerImpl::FromWebContents(web_contents);
  prefs->fullscreen_supported =
      browser_controller && browser_controller->fullscreen_delegate();
}

mojo::Remote<network::mojom::NetworkContext>
ContentBrowserClientImpl::CreateNetworkContext(
    content::BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path) {
  mojo::Remote<network::mojom::NetworkContext> network_context;
  network::mojom::NetworkContextParamsPtr context_params =
      network::mojom::NetworkContextParams::New();
  context_params->user_agent = GetUserAgent();
  context_params->accept_language = "en-us,en";
  if (!context->IsOffTheRecord()) {
    base::FilePath cookie_path = context->GetPath();
    cookie_path = cookie_path.Append(FILE_PATH_LITERAL("Cookies"));
    context_params->cookie_path = cookie_path;
  }
  content::GetNetworkService()->CreateNetworkContext(
      network_context.BindNewPipeAndPassReceiver(), std::move(context_params));
  return network_context;
}

#if defined(OS_LINUX) || defined(OS_ANDROID)
void ContentBrowserClientImpl::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::PosixFileDescriptorInfo* mappings) {
#if defined(OS_ANDROID)
  mappings->ShareWithRegion(
      kPakDescriptor,
      base::GlobalDescriptors::GetInstance()->Get(kPakDescriptor),
      base::GlobalDescriptors::GetInstance()->GetRegion(kPakDescriptor));
#endif
}
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

}  // namespace weblayer
