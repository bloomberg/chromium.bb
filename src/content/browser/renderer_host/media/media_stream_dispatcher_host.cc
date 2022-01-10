// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/task/bind_post_task.h"
#include "base/task/post_task.h"
#include "base/task/task_runner_util.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "url/origin.h"

#if !defined(OS_ANDROID)
#include "content/browser/media/capture/crop_id_web_contents_helper.h"
#endif

namespace content {

namespace {

void BindMediaStreamDeviceObserverReceiver(
    int render_process_id,
    int render_frame_id,
    mojo::PendingReceiver<blink::mojom::MediaStreamDeviceObserver> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (render_frame_host && render_frame_host->IsRenderFrameCreated())
    render_frame_host->GetRemoteInterfaces()->GetInterface(std::move(receiver));
}

std::unique_ptr<MediaStreamWebContentsObserver, BrowserThread::DeleteOnUIThread>
StartObservingWebContents(int render_process_id,
                          int render_frame_id,
                          base::RepeatingClosure focus_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* const web_contents = WebContents::FromRenderFrameHost(
      RenderFrameHost::FromID(render_process_id, render_frame_id));
  std::unique_ptr<MediaStreamWebContentsObserver,
                  BrowserThread::DeleteOnUIThread>
      web_contents_observer;
  if (web_contents) {
    web_contents_observer.reset(new MediaStreamWebContentsObserver(
        web_contents, base::BindPostTask(GetIOThreadTaskRunner({}),
                                         std::move(focus_callback))));
  }
  return web_contents_observer;
}

#if !defined(OS_ANDROID)
// Checks whether a track living in the WebContents indicated by
// (render_process_id, render_frame_id) may be cropped to the crop-target
// indicated by |crop_id|.
bool IsCropTargetValid(int render_process_id,
                       int render_frame_id,
                       const base::Token& crop_id) {
  RenderFrameHost* const rfh =
      RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!rfh) {
    return false;
  }

  WebContents* const web_contents =
      WebContents::FromRenderFrameHost(rfh->GetMainFrame());
  if (!web_contents) {
    return false;
  }

  CropIdWebContentsHelper* const helper =
      CropIdWebContentsHelper::FromWebContents(web_contents);
  if (!helper) {
    // No crop-IDs were ever produced on this WebContents.
    // Any non-zero crop-ID should be rejected on account of being
    // invalid. A zero crop-ID would ultimately be rejected on account
    // of the track being uncropped, so we can unconditionally reject.
    return false;
  }

  // * crop_id.is_zero() = uncrop-request.
  // * !crop_id.is_zero() = crop-request.
  return crop_id.is_zero() || helper->IsAssociatedWithCropId(crop_id);
}
#endif

}  // namespace

int MediaStreamDispatcherHost::next_requester_id_ = 0;

// Holds pending request information so that we process requests only when the
// Webcontent is in focus.
struct MediaStreamDispatcherHost::PendingAccessRequest {
  PendingAccessRequest(
      int32_t page_request_id,
      const blink::StreamControls& controls,
      bool user_gesture,
      blink::mojom::StreamSelectionInfoPtr audio_stream_selection_info_ptr,
      GenerateStreamCallback callback,
      MediaDeviceSaltAndOrigin salt_and_origin)
      : page_request_id(page_request_id),
        controls(controls),
        user_gesture(user_gesture),
        audio_stream_selection_info_ptr(
            std::move(audio_stream_selection_info_ptr)),
        callback(std::move(callback)),
        salt_and_origin(salt_and_origin) {}
  ~PendingAccessRequest() = default;

  int32_t page_request_id;
  const blink::StreamControls controls;
  bool user_gesture;
  blink::mojom::StreamSelectionInfoPtr audio_stream_selection_info_ptr;
  GenerateStreamCallback callback;
  MediaDeviceSaltAndOrigin salt_and_origin;
};

MediaStreamDispatcherHost::MediaStreamDispatcherHost(
    int render_process_id,
    int render_frame_id,
    MediaStreamManager* media_stream_manager)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      requester_id_(next_requester_id_++),
      media_stream_manager_(media_stream_manager),
      salt_and_origin_callback_(
          base::BindRepeating(&GetMediaDeviceSaltAndOrigin)) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(crbug.com/1265369): Register focus_callback only when needed.
  base::RepeatingClosure focus_callback =
      base::BindRepeating(&MediaStreamDispatcherHost::OnWebContentsFocused,
                          weak_factory_.GetWeakPtr());
  base::PostTaskAndReplyWithResult(
      GetUIThreadTaskRunner({}).get(), FROM_HERE,
      base::BindOnce(&StartObservingWebContents, render_process_id_,
                     render_frame_id_, std::move(focus_callback)),
      base::BindOnce(&MediaStreamDispatcherHost::SetWebContentsObserver,
                     weak_factory_.GetWeakPtr()));
}

MediaStreamDispatcherHost::~MediaStreamDispatcherHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  web_contents_observer_.reset();
  CancelAllRequests();
}

void MediaStreamDispatcherHost::Create(
    int render_process_id,
    int render_frame_id,
    MediaStreamManager* media_stream_manager,
    mojo::PendingReceiver<blink::mojom::MediaStreamDispatcherHost> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<MediaStreamDispatcherHost>(
          render_process_id, render_frame_id, media_stream_manager),
      std::move(receiver));
}

void MediaStreamDispatcherHost::SetWebContentsObserver(
    std::unique_ptr<MediaStreamWebContentsObserver,
                    BrowserThread::DeleteOnUIThread> web_contents_observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  web_contents_observer_ = std::move(web_contents_observer);
}

void MediaStreamDispatcherHost::OnDeviceStopped(
    const std::string& label,
    const blink::MediaStreamDevice& device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetMediaStreamDeviceObserver()->OnDeviceStopped(label, device);
}

void MediaStreamDispatcherHost::OnDeviceChanged(
    const std::string& label,
    const blink::MediaStreamDevice& old_device,
    const blink::MediaStreamDevice& new_device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetMediaStreamDeviceObserver()->OnDeviceChanged(label, old_device,
                                                  new_device);
}

void MediaStreamDispatcherHost::OnDeviceRequestStateChange(
    const std::string& label,
    const blink::MediaStreamDevice& device,
    const blink::mojom::MediaStreamStateChange new_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetMediaStreamDeviceObserver()->OnDeviceRequestStateChange(label, device,
                                                             new_state);
}

void MediaStreamDispatcherHost::OnDeviceCaptureHandleChange(
    const std::string& label,
    const blink::MediaStreamDevice& device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(device.display_media_info.has_value());

  GetMediaStreamDeviceObserver()->OnDeviceCaptureHandleChange(label, device);
}

void MediaStreamDispatcherHost::OnWebContentsFocused() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  while (!pending_requests_.empty()) {
    std::unique_ptr<PendingAccessRequest> request =
        std::move(pending_requests_.front());
    media_stream_manager_->GenerateStream(
        render_process_id_, render_frame_id_, requester_id_,
        request->page_request_id, request->controls,
        std::move(request->salt_and_origin), request->user_gesture,
        std::move(request->audio_stream_selection_info_ptr),
        std::move(request->callback),
        base::BindRepeating(&MediaStreamDispatcherHost::OnDeviceStopped,
                            weak_factory_.GetWeakPtr()),
        base::BindRepeating(&MediaStreamDispatcherHost::OnDeviceChanged,
                            weak_factory_.GetWeakPtr()),
        base::BindRepeating(
            &MediaStreamDispatcherHost::OnDeviceRequestStateChange,
            weak_factory_.GetWeakPtr()),
        base::BindRepeating(
            &MediaStreamDispatcherHost::OnDeviceCaptureHandleChange,
            weak_factory_.GetWeakPtr()));
    pending_requests_.pop_front();
  }
}

const mojo::Remote<blink::mojom::MediaStreamDeviceObserver>&
MediaStreamDispatcherHost::GetMediaStreamDeviceObserver() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (media_stream_device_observer_)
    return media_stream_device_observer_;

  auto dispatcher_receiver =
      media_stream_device_observer_.BindNewPipeAndPassReceiver();
  media_stream_device_observer_.set_disconnect_handler(base::BindOnce(
      &MediaStreamDispatcherHost::OnMediaStreamDeviceObserverConnectionError,
      weak_factory_.GetWeakPtr()));
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&BindMediaStreamDeviceObserverReceiver, render_process_id_,
                     render_frame_id_, std::move(dispatcher_receiver)));
  return media_stream_device_observer_;
}

void MediaStreamDispatcherHost::OnMediaStreamDeviceObserverConnectionError() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_device_observer_.reset();
}

void MediaStreamDispatcherHost::CancelAllRequests() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (auto& pending_request : pending_requests_) {
    std::move(pending_request->callback)
        .Run(blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN,
             std::string(), blink::MediaStreamDevices(),
             blink::MediaStreamDevices(),
             /*pan_tilt_zoom_allowed=*/false);
  }
  pending_requests_.clear();
  media_stream_manager_->CancelAllRequests(render_process_id_, render_frame_id_,
                                           requester_id_);
}

void MediaStreamDispatcherHost::GenerateStream(
    int32_t page_request_id,
    const blink::StreamControls& controls,
    bool user_gesture,
    blink::mojom::StreamSelectionInfoPtr audio_stream_selection_info_ptr,
    GenerateStreamCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (audio_stream_selection_info_ptr->strategy ==
          blink::mojom::StreamSelectionStrategy::SEARCH_BY_SESSION_ID &&
      (!audio_stream_selection_info_ptr->session_id.has_value() ||
       audio_stream_selection_info_ptr->session_id->is_empty())) {
    bad_message::ReceivedBadMessage(
        render_process_id_, bad_message::MDDH_INVALID_STREAM_SELECTION_INFO);
    return;
  }

  base::PostTaskAndReplyWithResult(
      GetUIThreadTaskRunner({}).get(), FROM_HERE,
      base::BindOnce(salt_and_origin_callback_, render_process_id_,
                     render_frame_id_),
      base::BindOnce(&MediaStreamDispatcherHost::DoGenerateStream,
                     weak_factory_.GetWeakPtr(), page_request_id, controls,
                     user_gesture, std::move(audio_stream_selection_info_ptr),
                     std::move(callback)));
}

void MediaStreamDispatcherHost::DoGenerateStream(
    int32_t page_request_id,
    const blink::StreamControls& controls,
    bool user_gesture,
    blink::mojom::StreamSelectionInfoPtr audio_stream_selection_info_ptr,
    GenerateStreamCallback callback,
    MediaDeviceSaltAndOrigin salt_and_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!MediaStreamManager::IsOriginAllowed(render_process_id_,
                                           salt_and_origin.origin)) {
    std::move(callback).Run(
        blink::mojom::MediaStreamRequestResult::INVALID_SECURITY_ORIGIN,
        std::string(), blink::MediaStreamDevices(), blink::MediaStreamDevices(),
        /*pan_tilt_zoom_allowed=*/false);
    return;
  }

  bool is_gum_request = (controls.audio.stream_type ==
                         blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) ||
                        (controls.video.stream_type ==
                         blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  bool needs_focus =
      is_gum_request &&
      base::FeatureList::IsEnabled(features::kUserMediaCaptureOnFocus) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForMediaStream) &&
      !salt_and_origin.is_background;
  if (needs_focus && !salt_and_origin.has_focus) {
    pending_requests_.push_back(std::make_unique<PendingAccessRequest>(
        page_request_id, controls, user_gesture,
        std::move(audio_stream_selection_info_ptr), std::move(callback),
        salt_and_origin));
    return;
  }

  media_stream_manager_->GenerateStream(
      render_process_id_, render_frame_id_, requester_id_, page_request_id,
      controls, std::move(salt_and_origin), user_gesture,
      std::move(audio_stream_selection_info_ptr), std::move(callback),
      base::BindRepeating(&MediaStreamDispatcherHost::OnDeviceStopped,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&MediaStreamDispatcherHost::OnDeviceChanged,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &MediaStreamDispatcherHost::OnDeviceRequestStateChange,
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &MediaStreamDispatcherHost::OnDeviceCaptureHandleChange,
          weak_factory_.GetWeakPtr()));
}

void MediaStreamDispatcherHost::CancelRequest(int page_request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->CancelRequest(render_process_id_, render_frame_id_,
                                       requester_id_, page_request_id);
}

void MediaStreamDispatcherHost::StopStreamDevice(
    const std::string& device_id,
    const absl::optional<base::UnguessableToken>& session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->StopStreamDevice(
      render_process_id_, render_frame_id_, requester_id_, device_id,
      session_id.value_or(base::UnguessableToken()));
}

void MediaStreamDispatcherHost::OpenDevice(int32_t page_request_id,
                                           const std::string& device_id,
                                           blink::mojom::MediaStreamType type,
                                           OpenDeviceCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // OpenDevice is only supported for microphone or webcam capture.
  if (type != blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE &&
      type != blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
    bad_message::ReceivedBadMessage(
        render_process_id_, bad_message::MDDH_INVALID_DEVICE_TYPE_REQUEST);
    return;
  }

  base::PostTaskAndReplyWithResult(
      GetUIThreadTaskRunner({}).get(), FROM_HERE,
      base::BindOnce(salt_and_origin_callback_, render_process_id_,
                     render_frame_id_),
      base::BindOnce(&MediaStreamDispatcherHost::DoOpenDevice,
                     weak_factory_.GetWeakPtr(), page_request_id, device_id,
                     type, std::move(callback)));
}

void MediaStreamDispatcherHost::DoOpenDevice(
    int32_t page_request_id,
    const std::string& device_id,
    blink::mojom::MediaStreamType type,
    OpenDeviceCallback callback,
    MediaDeviceSaltAndOrigin salt_and_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!MediaStreamManager::IsOriginAllowed(render_process_id_,
                                           salt_and_origin.origin)) {
    std::move(callback).Run(false /* success */, std::string(),
                            blink::MediaStreamDevice());
    return;
  }

  media_stream_manager_->OpenDevice(
      render_process_id_, render_frame_id_, requester_id_, page_request_id,
      device_id, type, std::move(salt_and_origin), std::move(callback),
      base::BindRepeating(&MediaStreamDispatcherHost::OnDeviceStopped,
                          weak_factory_.GetWeakPtr()));
}

void MediaStreamDispatcherHost::CloseDevice(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->CancelRequest(label);
}

void MediaStreamDispatcherHost::SetCapturingLinkSecured(
    const absl::optional<base::UnguessableToken>& session_id,
    blink::mojom::MediaStreamType type,
    bool is_secure) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->SetCapturingLinkSecured(
      render_process_id_, session_id.value_or(base::UnguessableToken()), type,
      is_secure);
}

void MediaStreamDispatcherHost::OnStreamStarted(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->OnStreamStarted(label);
}

#if !defined(OS_ANDROID)
void MediaStreamDispatcherHost::FocusCapturedSurface(const std::string& label,
                                                     bool focus) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->SetCapturedDisplaySurfaceFocus(
      label, focus,
      /*is_from_microtask=*/false,
      /*is_from_timer=*/false);
}

void MediaStreamDispatcherHost::Crop(const base::UnguessableToken& device_id,
                                     const base::Token& crop_id,
                                     CropCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Hop to the UI thread to verify that cropping to |crop_id| is permitted
  // from this particular context. Namely, cropping is currently only allowed
  // for self-capture, so the crop_id has to be associated with the top-level
  // WebContents belonging to this very tab.
  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&IsCropTargetValid, render_process_id_, render_frame_id_,
                     crop_id),
      base::BindOnce(&MediaStreamDispatcherHost::OnCropValidationComplete,
                     weak_factory_.GetWeakPtr(), device_id, crop_id,
                     std::move(callback)));
}

void MediaStreamDispatcherHost::OnCropValidationComplete(
    const base::UnguessableToken& device_id,
    const base::Token& crop_id,
    CropCallback callback,
    bool crop_id_passed_validation) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!crop_id_passed_validation) {
    std::move(callback).Run(media::mojom::CropRequestResult::kErrorGeneric);
    return;
  }
  media_stream_manager_->video_capture_manager()->Crop(device_id, crop_id,
                                                       std::move(callback));
}
#endif

}  // namespace content
