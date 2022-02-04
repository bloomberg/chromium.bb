// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/display_media_access_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/bad_message.h"
#include "chrome/browser/media/webrtc/desktop_capture_devices_util.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_factory_impl.h"
#include "chrome/browser/media/webrtc/native_desktop_media_list.h"
#include "chrome/browser/media/webrtc/tab_desktop_media_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {

// Helper function to get the title of the calling application.
std::u16string GetApplicationTitle(content::WebContents* web_contents) {
  return url_formatter::FormatOriginForSecurityDisplay(
      web_contents->GetMainFrame()->GetLastCommittedOrigin(),
      url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
}

}  // namespace

DisplayMediaAccessHandler::DisplayMediaAccessHandler()
    : picker_factory_(new DesktopMediaPickerFactoryImpl()),
      web_contents_collection_(this) {}

DisplayMediaAccessHandler::DisplayMediaAccessHandler(
    std::unique_ptr<DesktopMediaPickerFactory> picker_factory,
    bool display_notification)
    : display_notification_(display_notification),
      picker_factory_(std::move(picker_factory)),
      web_contents_collection_(this) {}

DisplayMediaAccessHandler::~DisplayMediaAccessHandler() = default;

bool DisplayMediaAccessHandler::SupportsStreamType(
    content::WebContents* web_contents,
    const blink::mojom::MediaStreamType stream_type,
    const extensions::Extension* extension) {
  return stream_type == blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE ||
         stream_type ==
             blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB;
  // This class handles MEDIA_DISPLAY_AUDIO_CAPTURE as well, but only if it is
  // accompanied by MEDIA_DISPLAY_VIDEO_CAPTURE request as per spec.
  // https://w3c.github.io/mediacapture-screen-share/#mediadevices-additions
  // 5.1 MediaDevices Additions
  // "The user agent MUST reject audio-only requests."
}

bool DisplayMediaAccessHandler::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  return false;
}

void DisplayMediaAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (capture_policy::GetAllowedCaptureLevel(request.security_origin,
                                             web_contents) ==
      AllowedScreenCaptureLevel::kDisallowed) {
    std::move(callback).Run(
        blink::MediaStreamDevices(),
        blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
        /*ui=*/nullptr);
    return;
  }

  // SafeBrowsing Delayed Warnings experiment can delay some SafeBrowsing
  // warnings until user interaction. If the current page has a delayed warning,
  // it'll have a user interaction observer attached. Show the warning
  // immediately in that case.
  safe_browsing::SafeBrowsingUserInteractionObserver* observer =
      safe_browsing::SafeBrowsingUserInteractionObserver::FromWebContents(
          web_contents);
  if (observer) {
    std::move(callback).Run(
        blink::MediaStreamDevices(),
        blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
        /*ui=*/nullptr);
    observer->OnDesktopCaptureRequest();
    return;
  }

#if BUILDFLAG(IS_MAC)
  // Do not allow picker UI to be shown on a page that isn't in the foreground
  // in Mac, because the UI implementation in Mac pops a window over any content
  // which might be confusing for the users. See https://crbug.com/1407733 for
  // details.
  // TODO(emircan): Remove this once Mac UI doesn't use a window.
  if (web_contents->GetVisibility() != content::Visibility::VISIBLE &&
      request.request_type != blink::MEDIA_DEVICE_UPDATE) {
    LOG(ERROR) << "Do not allow getDisplayMedia() on a backgrounded page.";
    std::move(callback).Run(
        blink::MediaStreamDevices(),
        blink::mojom::MediaStreamRequestResult::INVALID_STATE, /*ui=*/nullptr);
    return;
  }
#endif  // BUILDFLAG(IS_MAC)

  if (request.request_type == blink::MEDIA_DEVICE_UPDATE) {
    DCHECK(!request.requested_video_device_id.empty());
    // The share-this-tab-instead button is not shown when the screen capture is
    // initiated with preferCurrentTab: true, so it should not be possible to
    // reach HandleRequest in that case.
    DCHECK(request.video_type !=
           blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB);
    ProcessChangeSourceRequest(web_contents, request, std::move(callback));
    return;
  }

  if (request.video_type ==
          blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB ||
      request.video_type ==
          blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE) {
    // Repeat the permission test from the render process.
    content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
        request.render_process_id, request.render_frame_id);
    if (!rfh) {
      std::move(callback).Run(
          blink::MediaStreamDevices(),
          blink::mojom::MediaStreamRequestResult::INVALID_STATE,
          /*ui=*/nullptr);
      return;
    }

    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    // The kDisplayCapturePermissionsPolicyEnabled preference controls whether
    // the display-capture permissions-policy is applied or skipped.
    if (profile->GetPrefs()->GetBoolean(
            prefs::kDisplayCapturePermissionsPolicyEnabled)) {
      // If the display-capture permissions-policy disallows capture, the render
      // process was not supposed to send this message.
      if (!rfh->IsFeatureEnabled(
              blink::mojom::PermissionsPolicyFeature::kDisplayCapture)) {
        bad_message::ReceivedBadMessage(
            rfh->GetProcess(), bad_message::BadMessageReason::
                                   RFH_DISPLAY_CAPTURE_PERMISSION_MISSING);
        std::move(callback).Run(
            blink::MediaStreamDevices(),
            blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
            /*ui=*/nullptr);
        return;
      }
    }
  }

  std::unique_ptr<DesktopMediaPicker> picker =
      picker_factory_->CreatePicker(&request);
  if (!picker) {
    std::move(callback).Run(
        blink::MediaStreamDevices(),
        blink::mojom::MediaStreamRequestResult::INVALID_STATE, /*ui=*/nullptr);
    return;
  }

  // Ensure we are observing the deletion of |web_contents|.
  web_contents_collection_.StartObserving(web_contents);

  RequestsQueue& queue = pending_requests_[web_contents];

  queue.push_back(std::make_unique<PendingAccessRequest>(
      std::move(picker), request, std::move(callback),
      GetApplicationTitle(web_contents), display_notification_,
      /*is_allowlisted_extension=*/false));
  // If this is the only request then pop picker UI.
  if (queue.size() == 1)
    ProcessQueuedAccessRequest(queue, web_contents);
}

void DisplayMediaAccessHandler::UpdateMediaRequestState(
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

void DisplayMediaAccessHandler::ProcessChangeSourceRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Ensure we are observing the deletion of |web_contents|.
  web_contents_collection_.StartObserving(web_contents);

  RequestsQueue& queue = pending_requests_[web_contents];
  queue.push_back(std::make_unique<PendingAccessRequest>(
      /*picker=*/nullptr, request, std::move(callback),
      GetApplicationTitle(web_contents), display_notification_,
      /*is_allowlisted_extension=*/false));
  // If this is the only request then pop it. Otherwise, there is already a task
  // scheduled to pop the next request.
  if (queue.size() == 1)
    ProcessQueuedAccessRequest(queue, web_contents);
}

void DisplayMediaAccessHandler::ProcessQueuedAccessRequest(
    const RequestsQueue& queue,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const PendingAccessRequest& pending_request = *queue.front();
  UpdateTrusted(pending_request.request, false /* is_trusted */);

  const GURL& request_origin = pending_request.request.security_origin;
  AllowedScreenCaptureLevel capture_level =
      capture_policy::GetAllowedCaptureLevel(request_origin, web_contents);

  // If Capture is not allowed, then reject.
  if (capture_level == AllowedScreenCaptureLevel::kDisallowed) {
    RejectRequest(web_contents,
                  blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED);
    return;
  }

  if (pending_request.request.request_type == blink::MEDIA_DEVICE_UPDATE) {
    ProcessQueuedChangeSourceRequest(pending_request.request, web_contents);
  } else {
    ProcessQueuedPickerRequest(pending_request, web_contents, capture_level,
                               request_origin);
  }
}

void DisplayMediaAccessHandler::ProcessQueuedPickerRequest(
    const PendingAccessRequest& pending_request,
    content::WebContents* web_contents,
    AllowedScreenCaptureLevel capture_level,
    const GURL& request_origin) {
  std::vector<DesktopMediaList::Type> media_types;
  if (pending_request.request.video_type ==
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB) {
    media_types = {DesktopMediaList::Type::kCurrentTab,
                   DesktopMediaList::Type::kWebContents,
                   DesktopMediaList::Type::kWindow,
                   DesktopMediaList::Type::kScreen};
  } else {
    media_types = {DesktopMediaList::Type::kScreen,
                   DesktopMediaList::Type::kWindow,
                   DesktopMediaList::Type::kWebContents};
  }

  capture_policy::FilterMediaList(media_types, capture_level);

  // Avoid offering window-capture as a separate source, since PipeWire's
  // content-picker will offer both screen and window sources.
  // See crbug.com/1157006.
  if (content::desktop_capture::CanUsePipeWire() &&
      base::Contains(media_types, DesktopMediaList::Type::kScreen)) {
    base::Erase(media_types, DesktopMediaList::Type::kWindow);
  }

  auto includable_web_contents_filter =
      capture_policy::GetIncludableWebContentsFilter(request_origin,
                                                     capture_level);

  auto source_lists = picker_factory_->CreateMediaList(
      media_types, web_contents, includable_web_contents_filter);

  // base::Unretained(this) is safe because DisplayMediaAccessHandler is owned
  // by MediaCaptureDevicesDispatcher, which is a lazy singleton which is
  // destroyed when the browser process terminates.
  DesktopMediaPicker::DoneCallback done_callback =
      base::BindOnce(&DisplayMediaAccessHandler::OnDisplaySurfaceSelected,
                     base::Unretained(this), web_contents);
  DesktopMediaPicker::Params picker_params;
  picker_params.web_contents = web_contents;
  gfx::NativeWindow parent_window = web_contents->GetTopLevelNativeWindow();
  picker_params.context = parent_window;
  picker_params.parent = parent_window;
  picker_params.app_name = GetApplicationTitle(web_contents);
  picker_params.target_name = picker_params.app_name;
  picker_params.request_audio =
      pending_request.request.audio_type ==
      blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE;
  picker_params.restricted_by_policy =
      (capture_level != AllowedScreenCaptureLevel::kUnrestricted);
  pending_request.picker->Show(picker_params, std::move(source_lists),
                               std::move(done_callback));
}

void DisplayMediaAccessHandler::ProcessQueuedChangeSourceRequest(
    const content::MediaStreamRequest& request,
    content::WebContents* web_contents) {
  DCHECK(!request.requested_video_device_id.empty());
  content::WebContentsMediaCaptureId web_contents_id;
  if (!content::WebContentsMediaCaptureId::Parse(
          request.requested_video_device_id, &web_contents_id)) {
    RejectRequest(web_contents,
                  blink::mojom::MediaStreamRequestResult::INVALID_STATE);
    return;
  }
  content::DesktopMediaID media_id(content::DesktopMediaID::TYPE_WEB_CONTENTS,
                                   content::DesktopMediaID::kNullId,
                                   web_contents_id);
  media_id.audio_share =
      request.audio_type != blink::mojom::MediaStreamType::NO_SERVICE;
  OnDisplaySurfaceSelected(web_contents, media_id);
}

void DisplayMediaAccessHandler::RejectRequest(
    content::WebContents* web_contents,
    blink::mojom::MediaStreamRequestResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(web_contents);

  auto it = pending_requests_.find(web_contents);
  if (it == pending_requests_.end())
    return;
  RequestsQueue& mutable_queue = it->second;
  if (mutable_queue.empty())
    return;
  PendingAccessRequest& mutable_request = *mutable_queue.front();
  std::move(mutable_request.callback)
      .Run(blink::MediaStreamDevices(), result, /*ui=*/nullptr);
  mutable_queue.pop_front();
  if (!mutable_queue.empty())
    ProcessQueuedAccessRequest(mutable_queue, web_contents);
}

void DisplayMediaAccessHandler::AcceptRequest(
    content::WebContents* web_contents,
    const content::DesktopMediaID& media_id) {
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

  const bool disable_local_echo =
      (media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS) &&
      media_id.web_contents_id.disable_local_echo;

  blink::MediaStreamDevices devices;
  std::unique_ptr<content::MediaStreamUI> ui = GetDevicesForDesktopCapture(
      pending_request.request, web_contents, media_id, media_id.audio_share,
      disable_local_echo, display_notification_,
      GetApplicationTitle(web_contents), &devices);
  UpdateTarget(pending_request.request, media_id);

  std::move(pending_request.callback)
      .Run(devices, blink::mojom::MediaStreamRequestResult::OK, std::move(ui));
  queue.pop_front();

  if (!queue.empty())
    ProcessQueuedAccessRequest(queue, web_contents);
}

void DisplayMediaAccessHandler::OnDisplaySurfaceSelected(
    content::WebContents* web_contents,
    content::DesktopMediaID media_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(web_contents);

  if (media_id.is_null()) {
    RejectRequest(web_contents,
                  blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED);
    return;
  }

  // If the media id is tied to a tab, check that it hasn't been destroyed.
  if (media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS &&
      !content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(
              media_id.web_contents_id.render_process_id,
              media_id.web_contents_id.main_render_frame_id))) {
    RejectRequest(web_contents,
                  blink::mojom::MediaStreamRequestResult::TAB_CAPTURE_FAILURE);
    return;
  }

#if BUILDFLAG(IS_MAC)
  // Check screen capture permissions on Mac if necessary.
  if ((media_id.type == content::DesktopMediaID::TYPE_SCREEN ||
       media_id.type == content::DesktopMediaID::TYPE_WINDOW) &&
      system_media_permissions::CheckSystemScreenCapturePermission() !=
          system_media_permissions::SystemPermission::kAllowed) {
    RejectRequest(
        web_contents,
        blink::mojom::MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED);
    return;
  }
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS)
  // Check Data Leak Prevention restrictions on Chrome.
  // base::Unretained(this) is safe because DisplayMediaAccessHandler is owned
  // by MediaCaptureDevicesDispatcher, which is a lazy singleton which is
  // destroyed when the browser process terminates.
  policy::DlpContentManager::Get()->CheckScreenShareRestriction(
      media_id, GetApplicationTitle(web_contents),
      base::BindOnce(&DisplayMediaAccessHandler::OnDlpRestrictionChecked,
                     base::Unretained(this), web_contents->GetWeakPtr(),
                     media_id));
#else   // BUILDFLAG(OS_CHROMEOS)
  AcceptRequest(web_contents, media_id);
#endif  // !BUILDFLAG(OS_CHROMEOS)
}

#if BUILDFLAG(IS_CHROMEOS)
void DisplayMediaAccessHandler::OnDlpRestrictionChecked(
    base::WeakPtr<content::WebContents> web_contents,
    const content::DesktopMediaID& media_id,
    bool is_dlp_allowed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!web_contents) {
    return;
  }

  if (!is_dlp_allowed) {
    RejectRequest(web_contents.get(),
                  blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED);
  }
  AcceptRequest(web_contents.get(), media_id);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void DisplayMediaAccessHandler::DeletePendingAccessRequest(
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

void DisplayMediaAccessHandler::WebContentsDestroyed(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  pending_requests_.erase(web_contents);
}
