// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extensions_browser_interface_binders.h"

#include "base/bind.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/media/router/media_router_feature.h"       // nogncheck
#include "chrome/browser/media/router/mojo/media_router_desktop.h"  // nogncheck
#include "chrome/common/media_router/mojom/media_router.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"

#if defined(OS_CHROMEOS)
#include "base/task/post_task.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/services/media_perception/public/mojom/media_perception.mojom.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/chromeos_camera/camera_app_helper_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/video_capture_service.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/media_perception_private/media_perception_api_delegate.h"
#include "media/capture/video/chromeos/camera_app_device_provider_impl.h"
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/aura/window.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#endif
#endif

namespace extensions {

namespace {
#if defined(OS_CHROMEOS)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Resolves InputEngineManager receiver in InputMethodManager.
void BindInputEngineManager(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<chromeos::ime::mojom::InputEngineManager> receiver) {
  chromeos::input_method::InputMethodManager::Get()->ConnectInputEngineManager(
      std::move(receiver));
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Translates the renderer-side source ID to video device id.
void TranslateVideoDeviceId(
    const std::string& salt,
    const url::Origin& origin,
    const std::string& source_id,
    base::OnceCallback<void(const base::Optional<std::string>&)> callback) {
  auto callback_on_io_thread = base::BindOnce(
      [](const std::string& salt, const url::Origin& origin,
         const std::string& source_id,
         base::OnceCallback<void(const base::Optional<std::string>&)>
             callback) {
        content::GetMediaDeviceIDForHMAC(
            blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, salt,
            std::move(origin), source_id, std::move(callback));
      },
      salt, std::move(origin), source_id, std::move(callback));
  base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                 std::move(callback_on_io_thread));
}

void HandleCameraResult(
    content::BrowserContext* context,
    uint32_t intent_id,
    arc::mojom::CameraIntentAction action,
    const std::vector<uint8_t>& data,
    chromeos_camera::mojom::CameraAppHelper::HandleCameraResultCallback
        callback) {
  auto* intent_helper =
      arc::ArcIntentHelperBridge::GetForBrowserContext(context);
  intent_helper->HandleCameraResult(intent_id, action, data,
                                    std::move(callback));
}

// Connects to CameraAppDeviceProvider which could be used to get
// CameraAppDevice from video capture service through CameraAppDeviceBridge.
void ConnectToCameraAppDeviceProvider(
    content::RenderFrameHost* source,
    mojo::PendingReceiver<cros::mojom::CameraAppDeviceProvider> receiver) {
  mojo::PendingRemote<cros::mojom::CameraAppDeviceBridge> device_bridge;
  auto device_bridge_receiver = device_bridge.InitWithNewPipeAndPassReceiver();

  // Connects to CameraAppDeviceBridge from video_capture service.
  content::GetVideoCaptureService().ConnectToCameraAppDeviceBridge(
      std::move(device_bridge_receiver));

  auto security_origin = source->GetLastCommittedOrigin();
  auto media_device_id_salt =
      source->GetProcess()->GetBrowserContext()->GetMediaDeviceIDSalt();

  auto mapping_callback =
      base::BindRepeating(&TranslateVideoDeviceId, media_device_id_salt,
                          std::move(security_origin));

  auto camera_app_device_provider =
      std::make_unique<media::CameraAppDeviceProviderImpl>(
          std::move(device_bridge), std::move(mapping_callback));
  mojo::MakeSelfOwnedReceiver(std::move(camera_app_device_provider),
                              std::move(receiver));
}

// Connects to CameraAppHelper that could handle camera intents.
void ConnectToCameraAppHelper(
    content::RenderFrameHost* source,
    mojo::PendingReceiver<chromeos_camera::mojom::CameraAppHelper> receiver) {
  auto handle_result_callback = base::BindRepeating(
      &HandleCameraResult, source->GetProcess()->GetBrowserContext());
  auto* window = source->GetNativeView()->GetToplevelWindow();
  auto camera_app_helper =
      std::make_unique<chromeos_camera::CameraAppHelperImpl>(
          std::move(handle_result_callback), window);
  mojo::MakeSelfOwnedReceiver(std::move(camera_app_helper),
                              std::move(receiver));
}
#endif
}  // namespace

void PopulateChromeFrameBindersForExtension(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map,
    content::RenderFrameHost* render_frame_host,
    const Extension* extension) {
  DCHECK(extension);
  auto* context = render_frame_host->GetProcess()->GetBrowserContext();
  if (media_router::MediaRouterEnabled(context) &&
      extension->permissions_data()->HasAPIPermission(
          APIPermission::kMediaRouterPrivate)) {
    binder_map->Add<media_router::mojom::MediaRouter>(
        base::BindRepeating(&media_router::MediaRouterDesktop::BindToReceiver,
                            base::RetainedRef(extension), context));
  }

#if defined(OS_CHROMEOS)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Registry InputEngineManager for official Google XKB Input only.
  if (extension->id() == chromeos::extension_ime_util::kXkbExtensionId) {
    binder_map->Add<chromeos::ime::mojom::InputEngineManager>(
        base::BindRepeating(&BindInputEngineManager));
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  if (extension->permissions_data()->HasAPIPermission(
          APIPermission::kMediaPerceptionPrivate)) {
    extensions::ExtensionsAPIClient* client =
        extensions::ExtensionsAPIClient::Get();
    extensions::MediaPerceptionAPIDelegate* delegate = nullptr;
    if (client)
      delegate = client->GetMediaPerceptionAPIDelegate();
    if (delegate) {
      // Note that it is safe to use base::Unretained here because |delegate| is
      // owned by the |client|, which is instantiated by the
      // ChromeExtensionsBrowserClient, which in turn is owned and lives as long
      // as the BrowserProcessImpl.
      binder_map->Add<chromeos::media_perception::mojom::MediaPerception>(
          base::BindRepeating(&extensions::MediaPerceptionAPIDelegate::
                                  ForwardMediaPerceptionReceiver,
                              base::Unretained(delegate)));
    }
  }

  if (extension->id().compare(extension_misc::kCameraAppId) == 0 ||
      extension->id().compare(extension_misc::kCameraAppDevId) == 0) {
    binder_map->Add<cros::mojom::CameraAppDeviceProvider>(
        base::BindRepeating(&ConnectToCameraAppDeviceProvider));
    binder_map->Add<chromeos_camera::mojom::CameraAppHelper>(
        base::BindRepeating(&ConnectToCameraAppHelper));
  }
#endif
}

}  // namespace extensions
