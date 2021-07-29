// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/desktop_capture_access_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/webrtc/desktop_capture_devices_util.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_factory_impl.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/media/webrtc/native_desktop_media_list.h"
#include "chrome/browser/media/webrtc/tab_desktop_media_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/screen_capture_notification_ui.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/desktop_streams_registry.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#include "chrome/browser/ash/policy/dlp/dlp_content_manager.h"
#include "ui/base/ui_base_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_MAC)
#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"
#endif  // defined(OS_MAC)

using content::BrowserThread;
using extensions::mojom::ManifestLocation;

namespace {

// Helper to get title of the calling application shown in the screen capture
// notification.
std::u16string GetApplicationTitle(content::WebContents* web_contents,
                                   const extensions::Extension* extension) {
  // Use extension name as title for extensions and host/origin for drive-by
  // web.
  std::string title;
  if (extension) {
    title = extension->name();
    return base::UTF8ToUTF16(title);
  }
  GURL url = web_contents->GetURL();
  title = network::IsUrlPotentiallyTrustworthy(url)
              ? net::GetHostAndOptionalPort(url)
              : url.GetOrigin().spec();
  return base::UTF8ToUTF16(title);
}

// Returns whether an on-screen notification should appear after desktop capture
// is approved for |extension|.  Component extensions do not display a
// notification.
bool ShouldDisplayNotification(const extensions::Extension* extension) {
  return !(extension &&
           (extension->location() == ManifestLocation::kComponent ||
            extension->location() == ManifestLocation::kExternalComponent));
}

// Returns true if an on-screen notification should not be displayed after
// desktop capture is taken for the |url|.
bool HasNotificationExemption(const GURL& url) {
  return (url.spec() == chrome::kChromeUIFeedbackURL &&
          base::FeatureList::IsEnabled(features::kWebUIFeedback));
}

#if !defined(OS_ANDROID)
// Find browser or app window from a given |web_contents|.
gfx::NativeWindow FindParentWindowForWebContents(
    content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser && browser->window())
    return browser->window()->GetNativeWindow();

  const extensions::AppWindowRegistry::AppWindowList& window_list =
      extensions::AppWindowRegistry::Get(web_contents->GetBrowserContext())
          ->app_windows();
  for (auto iter = window_list.begin(); iter != window_list.end(); ++iter) {
    if ((*iter)->web_contents() == web_contents)
      return (*iter)->GetNativeWindow();
  }

  return NULL;
}
#endif

}  // namespace

// Holds pending request information so that we display one picker UI at a time
// for each content::WebContents.
struct DesktopCaptureAccessHandler::PendingAccessRequest {
  PendingAccessRequest(std::unique_ptr<DesktopMediaPicker> picker,
                       const content::MediaStreamRequest& request,
                       content::MediaResponseCallback callback,
                       const extensions::Extension* extension)
      : picker(std::move(picker)),
        request(request),
        callback(std::move(callback)),
        extension(extension) {}
  ~PendingAccessRequest() = default;

  std::unique_ptr<DesktopMediaPicker> picker;
  content::MediaStreamRequest request;
  content::MediaResponseCallback callback;
  const extensions::Extension* extension;
};

DesktopCaptureAccessHandler::DesktopCaptureAccessHandler()
    : picker_factory_(new DesktopMediaPickerFactoryImpl()),
      display_notification_(true),
      web_contents_collection_(this) {}

DesktopCaptureAccessHandler::DesktopCaptureAccessHandler(
    std::unique_ptr<DesktopMediaPickerFactory> picker_factory)
    : picker_factory_(std::move(picker_factory)),
      display_notification_(false),
      web_contents_collection_(this) {}

DesktopCaptureAccessHandler::~DesktopCaptureAccessHandler() = default;

void DesktopCaptureAccessHandler::ProcessScreenCaptureAccessRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  blink::MediaStreamDevices devices;
  std::unique_ptr<content::MediaStreamUI> ui;

  DCHECK_EQ(request.video_type,
            blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE);

  UpdateExtensionTrusted(request, extension);

  bool loopback_audio_supported = false;
#if defined(USE_CRAS) || defined(OS_WIN)
  // Currently loopback audio capture is supported only on Windows and ChromeOS.
  loopback_audio_supported = true;
#endif

  bool screen_capture_enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableUserMediaScreenCapturing) ||
      MediaCaptureDevicesDispatcher::IsOriginForCasting(
          request.security_origin) ||
      IsExtensionAllowedForScreenCapture(extension) ||
      IsBuiltInFeedbackUI(request.security_origin);

  const bool origin_is_secure =
      network::IsUrlPotentiallyTrustworthy(request.security_origin) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowHttpScreenCapture);

  // If basic conditions (screen capturing is enabled and origin is secure)
  // aren't fulfilled, we'll use "invalid state" as result. Otherwise, we set
  // it after checking permission.
  // TODO(grunell): It would be good to change this result for something else,
  // probably a new one.
  blink::mojom::MediaStreamRequestResult result =
      blink::mojom::MediaStreamRequestResult::INVALID_STATE;

  // Approve request only when the following conditions are met:
  //  1. Screen capturing is enabled via command line switch or white-listed for
  //     the given origin.
  //  2. Request comes from a page with a secure origin or from an extension.
  if (screen_capture_enabled && origin_is_secure) {
    // Get title of the calling application prior to showing the message box.
    // chrome::ShowQuestionMessageBox() starts a nested run loop which may
    // allow |web_contents| to be destroyed on the UI thread before the messag
    // box is closed. See http://crbug.com/326690.
    std::u16string application_title =
        GetApplicationTitle(web_contents, extension);
#if !defined(OS_ANDROID)
    gfx::NativeWindow parent_window =
        FindParentWindowForWebContents(web_contents);
#else
    gfx::NativeWindow parent_window = NULL;
#endif

    // Some extensions do not require user approval, because they provide their
    // own user approval UI.
    bool is_approved = IsDefaultApproved(extension) ||
                       IsDefaultApproved(request.security_origin);
    if (!is_approved) {
      std::u16string application_name =
          base::UTF8ToUTF16(request.security_origin.spec());
      if (extension)
        application_name = base::UTF8ToUTF16(extension->name());
      std::u16string confirmation_text = l10n_util::GetStringFUTF16(
          request.audio_type == blink::mojom::MediaStreamType::NO_SERVICE
              ? IDS_MEDIA_SCREEN_CAPTURE_CONFIRMATION_TEXT
              : IDS_MEDIA_SCREEN_AND_AUDIO_CAPTURE_CONFIRMATION_TEXT,
          application_name);
      chrome::MessageBoxResult result = chrome::ShowQuestionMessageBoxSync(
          parent_window,
          l10n_util::GetStringFUTF16(
              IDS_MEDIA_SCREEN_CAPTURE_CONFIRMATION_TITLE, application_name),
          confirmation_text);
      is_approved = (result == chrome::MESSAGE_BOX_RESULT_YES);
    }

    if (is_approved) {
      content::DesktopMediaID screen_id;
#if BUILDFLAG(IS_CHROMEOS_ASH)
      screen_id = content::DesktopMediaID::RegisterNativeWindow(
          content::DesktopMediaID::TYPE_SCREEN,
          primary_root_window_for_testing_
              ? primary_root_window_for_testing_
              : ash::Shell::Get()->GetPrimaryRootWindow());
      if (policy::DlpContentManager::Get()->IsScreenCaptureRestricted(
              screen_id)) {
        std::move(callback).Run(
            devices, blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
            std::move(ui));
        return;
      }
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
      screen_id = content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                                          webrtc::kFullDesktopScreenId);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

      bool capture_audio =
          (request.audio_type ==
               blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE &&
           loopback_audio_supported);

      // Determine if the extension is required to display a notification.
      const bool display_notification =
          display_notification_ && ShouldDisplayNotification(extension) &&
          !HasNotificationExemption(request.security_origin);

      if (!content::WebContents::FromRenderFrameHost(
              content::RenderFrameHost::FromID(request.render_process_id,
                                               request.render_frame_id))) {
        std::move(callback).Run(
            devices, blink::mojom::MediaStreamRequestResult::INVALID_STATE,
            std::move(ui));
        return;
      }
      ui = GetDevicesForDesktopCapture(
          web_contents, url::Origin::Create(request.security_origin), &devices,
          screen_id, blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
          blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE,
          capture_audio, request.disable_local_echo, display_notification,
          application_title, application_title);
      DCHECK(!devices.empty());
    }

    // The only case when devices can be empty is if the user has denied
    // permission.
    result = devices.empty()
                 ? blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED
                 : blink::mojom::MediaStreamRequestResult::OK;
  }

  std::move(callback).Run(devices, result, std::move(ui));
}

bool DesktopCaptureAccessHandler::IsDefaultApproved(
    const extensions::Extension* extension) {
  return extension &&
         (extension->location() == ManifestLocation::kComponent ||
          extension->location() == ManifestLocation::kExternalComponent ||
          IsExtensionAllowedForScreenCapture(extension));
}

bool DesktopCaptureAccessHandler::IsDefaultApproved(const GURL& url) {
  // allow the Feedback WebUI chrome://feedback/ to take screenshot without
  // user's approval. The screenshot will not be shared by default. So the
  // user can still decide whether the screenshot taken is shared or not.
  return url.spec() == chrome::kChromeUIFeedbackURL;
}

bool DesktopCaptureAccessHandler::SupportsStreamType(
    content::WebContents* web_contents,
    const blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  return type == blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE ||
         type == blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE;
}

bool DesktopCaptureAccessHandler::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  return false;
}

void DesktopCaptureAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  blink::MediaStreamDevices devices;
  std::unique_ptr<content::MediaStreamUI> ui;

  if (request.video_type !=
      blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE) {
    std::move(callback).Run(
        devices, blink::mojom::MediaStreamRequestResult::INVALID_STATE,
        std::move(ui));
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile->GetPrefs()->GetBoolean(prefs::kScreenCaptureAllowed)) {
    std::move(callback).Run(
        devices, blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
        std::move(ui));
    return;
  }

  if (request.request_type == blink::MEDIA_DEVICE_UPDATE) {
    ProcessChangeSourceRequest(web_contents, request, std::move(callback),
                               extension);
    return;
  }

  // If the device id wasn't specified then this is a screen capture request
  // (i.e. chooseDesktopMedia() API wasn't used to generate device id).
  if (request.requested_video_device_id.empty()) {
#if defined(OS_MAC)
    if (system_media_permissions::CheckSystemScreenCapturePermission() !=
        system_media_permissions::SystemPermission::kAllowed) {
      std::move(callback).Run(
          blink::MediaStreamDevices(),
          blink::mojom::MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED,
          nullptr);
      return;
    }
#endif
    ProcessScreenCaptureAccessRequest(web_contents, request,
                                      std::move(callback), extension);
    return;
  }

  // Resolve DesktopMediaID for the specified device id.
  content::DesktopMediaID media_id;
  // TODO(miu): Replace "main RenderFrame" IDs with the request's actual
  // RenderFrame IDs once the desktop capture extension API implementation is
  // fixed.  http://crbug.com/304341
  content::WebContents* const web_contents_for_stream =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(request.render_process_id,
                                           request.render_frame_id));
  content::RenderFrameHost* const main_frame =
      web_contents_for_stream ? web_contents_for_stream->GetMainFrame() : NULL;
  if (main_frame) {
    media_id =
        content::DesktopStreamsRegistry::GetInstance()->RequestMediaForStreamId(
            request.requested_video_device_id,
            main_frame->GetProcess()->GetID(), main_frame->GetRoutingID(),
            url::Origin::Create(request.security_origin), nullptr,
            content::kRegistryStreamTypeDesktop);
  }

  // Received invalid device id.
  if (media_id.type == content::DesktopMediaID::TYPE_NONE) {
    std::move(callback).Run(
        devices, blink::mojom::MediaStreamRequestResult::INVALID_STATE,
        std::move(ui));
    return;
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  {
    if (policy::DlpContentManager::Get()->IsScreenCaptureRestricted(media_id)) {
      std::move(callback).Run(
          devices, blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
          std::move(ui));
      return;
    }
  }
#endif
#if defined(OS_MAC)
  if (media_id.type != content::DesktopMediaID::TYPE_WEB_CONTENTS &&
      system_media_permissions::CheckSystemScreenCapturePermission() !=
          system_media_permissions::SystemPermission::kAllowed) {
    std::move(callback).Run(
        blink::MediaStreamDevices(),
        blink::mojom::MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED,
        nullptr);
    return;
  }
#endif

  if (media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS &&
      !content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(
              media_id.web_contents_id.render_process_id,
              media_id.web_contents_id.main_render_frame_id))) {
    std::move(callback).Run(
        devices, blink::mojom::MediaStreamRequestResult::TAB_CAPTURE_FAILURE,
        std::move(ui));
    return;
  }

  bool loopback_audio_supported = false;
#if defined(USE_CRAS) || defined(OS_WIN)
  // Currently loopback audio capture is supported only on Windows and ChromeOS.
  loopback_audio_supported = true;
#endif

  // This value essentially from the checkbox on picker window, so it
  // corresponds to user permission.
  const bool audio_permitted = media_id.audio_share;

  // This value essentially from whether getUserMedia requests audio stream.
  const bool audio_requested =
      request.audio_type ==
      blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE;

  // This value shows for a given capture type, whether the system or our code
  // can support audio sharing. Currently audio is only supported for screen and
  // tab/webcontents capture streams.
  const bool audio_supported =
      (media_id.type == content::DesktopMediaID::TYPE_SCREEN &&
       loopback_audio_supported) ||
      media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS;

  const bool check_audio_permission =
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          extensions::switches::kDisableDesktopCaptureAudio);
  const bool capture_audio =
      (check_audio_permission ? audio_permitted : true) && audio_requested &&
      audio_supported;

  // Determine if the extension is required to display a notification.
  const bool display_notification =
      display_notification_ && ShouldDisplayNotification(extension);

  ui = GetDevicesForDesktopCapture(
      web_contents, url::Origin::Create(request.security_origin), &devices,
      media_id, blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
      blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE, capture_audio,
      request.disable_local_echo, display_notification,
      GetApplicationTitle(web_contents, extension),
      GetApplicationTitle(web_contents, extension));
  UpdateExtensionTrusted(request, extension);
  std::move(callback).Run(devices, blink::mojom::MediaStreamRequestResult::OK,
                          std::move(ui));
}

void DesktopCaptureAccessHandler::ProcessChangeSourceRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<DesktopMediaPicker> picker;

  if (!base::FeatureList::IsEnabled(
          features::kDesktopCaptureTabSharingInfobar) ||
      request.requested_video_device_id.empty()) {
    picker = picker_factory_->CreatePicker(&request);
    if (!picker) {
      std::move(callback).Run(
          blink::MediaStreamDevices(),
          blink::mojom::MediaStreamRequestResult::INVALID_STATE, nullptr);
      return;
    }
  }

  // Ensure we are observing the deletion of |web_contents|.
  web_contents_collection_.StartObserving(web_contents);

  RequestsQueue& queue = pending_requests_[web_contents];
  queue.push_back(std::make_unique<PendingAccessRequest>(
      std::move(picker), request, std::move(callback), extension));
  // If this is the only request then pop picker UI.
  if (queue.size() == 1)
    ProcessQueuedAccessRequest(queue, web_contents);
}

void DesktopCaptureAccessHandler::UpdateMediaRequestState(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    blink::mojom::MediaStreamType stream_type,
    content::MediaRequestState state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (state != content::MEDIA_REQUEST_STATE_DONE &&
      state != content::MEDIA_REQUEST_STATE_CLOSING) {
    return;
  }

  if (state == content::MEDIA_REQUEST_STATE_CLOSING) {
    DeletePendingAccessRequest(render_process_id, render_frame_id,
                               page_request_id);
  }
  CaptureAccessHandlerBase::UpdateMediaRequestState(
      render_process_id, render_frame_id, page_request_id, stream_type, state);

  // This method only gets called with the above checked states when all
  // requests are to be canceled. Therefore, we don't need to process the
  // next queued request.
}

void DesktopCaptureAccessHandler::ProcessQueuedAccessRequest(
    const RequestsQueue& queue,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const PendingAccessRequest& pending_request = *queue.front();

  if (!pending_request.picker) {
    DCHECK(!pending_request.request.requested_video_device_id.empty());
    content::WebContentsMediaCaptureId web_contents_id;
    if (content::WebContentsMediaCaptureId::Parse(
            pending_request.request.requested_video_device_id,
            &web_contents_id)) {
      content::DesktopMediaID media_id(
          content::DesktopMediaID::TYPE_WEB_CONTENTS,
          content::DesktopMediaID::kNullId, web_contents_id);
      media_id.audio_share = pending_request.request.audio_type !=
                             blink::mojom::MediaStreamType::NO_SERVICE;
      OnPickerDialogResults(web_contents, media_id);
      return;
    }
  }

  auto source_lists = picker_factory_->CreateMediaList(
      {DesktopMediaList::Type::kWebContents}, web_contents);

  DesktopMediaPicker::DoneCallback done_callback =
      base::BindOnce(&DesktopCaptureAccessHandler::OnPickerDialogResults,
                     base::Unretained(this), web_contents);
  DesktopMediaPicker::Params picker_params;
  picker_params.web_contents = web_contents;
  gfx::NativeWindow parent_window = web_contents->GetTopLevelNativeWindow();
  picker_params.context = parent_window;
  picker_params.parent = parent_window;
  picker_params.app_name =
      GetApplicationTitle(web_contents, pending_request.extension);
  picker_params.target_name = picker_params.app_name;
  picker_params.request_audio = (pending_request.request.audio_type ==
                                 blink::mojom::MediaStreamType::NO_SERVICE)
                                    ? false
                                    : true;
  pending_request.picker->Show(picker_params, std::move(source_lists),
                               std::move(done_callback));

  // Focus on the tab with the picker for easy access.
  if (auto* delegate = web_contents->GetDelegate())
    delegate->ActivateContents(web_contents);
}

void DesktopCaptureAccessHandler::OnPickerDialogResults(
    content::WebContents* web_contents,
    content::DesktopMediaID media_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(web_contents);

  auto it = pending_requests_.find(web_contents);
  if (it == pending_requests_.end())
    return;
  RequestsQueue& queue = it->second;
  if (queue.empty()) {
    // UpdateMediaRequestState() called with MEDIA_REQUEST_STATE_CLOSING. Don't
    // need to do anything.
    return;
  }

  PendingAccessRequest& pending_request = *queue.front();
  blink::MediaStreamDevices devices;
  blink::mojom::MediaStreamRequestResult request_result =
      blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;
  const extensions::Extension* extension = pending_request.extension;
  std::unique_ptr<content::MediaStreamUI> ui;
  if (media_id.is_null()) {
    request_result = blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;
  } else {
    request_result = blink::mojom::MediaStreamRequestResult::OK;
    // Determine if the extension is required to display a notification.
    const bool display_notification =
        display_notification_ && ShouldDisplayNotification(extension);
    ui = GetDevicesForDesktopCapture(
        web_contents,
        url::Origin::Create(pending_request.request.security_origin), &devices,
        media_id, pending_request.request.video_type,
        pending_request.request.audio_type, media_id.audio_share,
        pending_request.request.disable_local_echo, display_notification,
        GetApplicationTitle(web_contents, extension),
        GetApplicationTitle(web_contents, extension));
  }

  std::move(pending_request.callback)
      .Run(devices, request_result, std::move(ui));
  queue.pop_front();

  if (!queue.empty())
    ProcessQueuedAccessRequest(queue, web_contents);
}

void DesktopCaptureAccessHandler::WebContentsDestroyed(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  pending_requests_.erase(web_contents);
}

void DesktopCaptureAccessHandler::DeletePendingAccessRequest(
    int render_process_id,
    int render_frame_id,
    int page_request_id) {
  for (auto& queue_it : pending_requests_) {
    RequestsQueue& queue = queue_it.second;
    for (auto it = queue.begin(); it != queue.end(); ++it) {
      const PendingAccessRequest& pending_request = **it;
      if (pending_request.request.render_process_id == render_process_id &&
          pending_request.request.render_frame_id == render_frame_id &&
          pending_request.request.page_request_id == page_request_id) {
        queue.erase(it);
        return;
      }
    }
  }
}
