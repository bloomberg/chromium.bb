// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/common/content_switches.h"
#include "media/capture/video/fake_video_capture_device.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

bool IsFeatureEnabled(RenderFrameHost* rfh,
                      bool tests_use_fake_render_frame_hosts,
                      blink::mojom::PermissionsPolicyFeature feature) {
  // Some tests don't (or can't) set up the RenderFrameHost. In these cases we
  // just ignore permissions policy checks (there is no permissions policy to
  // test).
  if (!rfh && tests_use_fake_render_frame_hosts)
    return true;

  if (!rfh)
    return false;

  return rfh->IsFeatureEnabled(feature);
}

class MediaStreamUIProxy::Core {
 public:
  explicit Core(const base::WeakPtr<MediaStreamUIProxy>& proxy,
                RenderFrameHostDelegate* test_render_delegate);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  ~Core();

  void RequestAccess(std::unique_ptr<MediaStreamRequest> request);
  void OnStarted(gfx::NativeViewId* window_id,
                 bool has_source_callback,
                 const std::string& label,
                 std::vector<DesktopMediaID> screen_capture_ids);
  void OnDeviceStopped(const std::string& label,
                       const DesktopMediaID& media_id);

#if !defined(OS_ANDROID)
  void SetFocus(const DesktopMediaID& media_id,
                bool focus,
                bool is_from_microtask,
                bool is_from_timer);
#endif

  void ProcessAccessRequestResponse(
      int render_process_id,
      int render_frame_id,
      const blink::MediaStreamDevices& devices,
      blink::mojom::MediaStreamRequestResult result,
      std::unique_ptr<MediaStreamUI> stream_ui);

  base::WeakPtr<Core> GetWeakPtr() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    // This weak pointer is created in the ctor, which runs on the IO thread.
    // This pointer is always posted from the IO thread to the UI thread,
    // meaning reading |weak_this_| happens on the IO thead, but dereferencing
    // the actual pointer happens in the UI thread. Invalidation happens in the
    // destructor, which runs on the UI thread, so this is safe.
    return weak_this_;
  }

 private:
  friend class FakeMediaStreamUIProxy;
  void ProcessStopRequestFromUI();
  void ProcessChangeSourceRequestFromUI(const DesktopMediaID& media_id);
  void ProcessStateChangeFromUI(const DesktopMediaID& media,
                                blink::mojom::MediaStreamStateChange);
  RenderFrameHostDelegate* GetRenderFrameHostDelegate(int render_process_id,
                                                      int render_frame_id);

  base::WeakPtr<MediaStreamUIProxy> proxy_;
  std::unique_ptr<MediaStreamUI> ui_;

  bool tests_use_fake_render_frame_hosts_;
  const raw_ptr<RenderFrameHostDelegate> test_render_delegate_;

  base::WeakPtr<Core> weak_this_;

  base::WeakPtrFactory<Core> weak_factory_{this};

  // Used for calls supplied to `ui_`. Invalidated every time a new UI is
  // created.
  base::WeakPtrFactory<Core> weak_factory_for_ui_{this};
};

MediaStreamUIProxy::Core::Core(const base::WeakPtr<MediaStreamUIProxy>& proxy,
                               RenderFrameHostDelegate* test_render_delegate)
    : proxy_(proxy),
      tests_use_fake_render_frame_hosts_(test_render_delegate != nullptr),
      test_render_delegate_(test_render_delegate) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  weak_this_ = weak_factory_.GetWeakPtr();
}

MediaStreamUIProxy::Core::~Core() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void MediaStreamUIProxy::Core::RequestAccess(
    std::unique_ptr<MediaStreamRequest> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHostDelegate* render_delegate = GetRenderFrameHostDelegate(
      request->render_process_id, request->render_frame_id);

  // Tab may have gone away, or has no delegate from which to request access.
  if (!render_delegate) {
    ProcessAccessRequestResponse(
        request->render_process_id, request->render_frame_id,
        blink::MediaStreamDevices(),
        blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN,
        std::unique_ptr<MediaStreamUI>());
    return;
  }

  render_delegate->RequestMediaAccessPermission(
      *request,
      base::BindOnce(&Core::ProcessAccessRequestResponse, weak_this_,
                     request->render_process_id, request->render_frame_id));
}

void MediaStreamUIProxy::Core::OnStarted(
    gfx::NativeViewId* window_id,
    bool has_source_callback,
    const std::string& label,
    std::vector<DesktopMediaID> screen_share_ids) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!ui_)
    return;

  MediaStreamUI::SourceCallback device_change_cb;
  if (has_source_callback) {
    device_change_cb =
        base::BindRepeating(&Core::ProcessChangeSourceRequestFromUI,
                            weak_factory_for_ui_.GetWeakPtr());
  }

  *window_id =
      ui_->OnStarted(base::BindOnce(&Core::ProcessStopRequestFromUI,
                                    weak_factory_for_ui_.GetWeakPtr()),
                     device_change_cb, label, screen_share_ids,
                     base::BindRepeating(&Core::ProcessStateChangeFromUI,
                                         weak_factory_for_ui_.GetWeakPtr()));
}

void MediaStreamUIProxy::Core::OnDeviceStopped(const std::string& label,
                                               const DesktopMediaID& media_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (ui_) {
    ui_->OnDeviceStopped(label, media_id);
  }
}

#if !defined(OS_ANDROID)
void MediaStreamUIProxy::Core::SetFocus(const DesktopMediaID& media_id,
                                        bool focus,
                                        bool is_from_microtask,
                                        bool is_from_timer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (ui_) {
    ui_->SetFocus(media_id, focus, is_from_microtask, is_from_timer);
  }
}
#endif

void MediaStreamUIProxy::Core::ProcessAccessRequestResponse(
    int render_process_id,
    int render_frame_id,
    const blink::MediaStreamDevices& devices,
    blink::mojom::MediaStreamRequestResult result,
    std::unique_ptr<MediaStreamUI> stream_ui) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  blink::MediaStreamDevices filtered_devices;
  auto* host = RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  for (const blink::MediaStreamDevice& device : devices) {
    if (device.type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE &&
        !IsFeatureEnabled(
            host, tests_use_fake_render_frame_hosts_,
            blink::mojom::PermissionsPolicyFeature::kMicrophone)) {
      continue;
    }

    if (device.type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE &&
        !IsFeatureEnabled(host, tests_use_fake_render_frame_hosts_,
                          blink::mojom::PermissionsPolicyFeature::kCamera)) {
      continue;
    }

    filtered_devices.push_back(device);
  }
  if (filtered_devices.empty() &&
      result == blink::mojom::MediaStreamRequestResult::OK)
    result = blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;

  if (stream_ui) {
    // Callbacks that were supplied to the existing `ui_` are no longer
    // applicable. This is important as some implementions (TabSharingUIViews)
    // always run the callback when destroyed. However at the point the UI is
    // replaced while the screencast is ongoing. Invalidating ensures the
    // screencast is not terminated. See crbug.com/1155426 for details.
    weak_factory_for_ui_.InvalidateWeakPtrs();
    ui_ = std::move(stream_ui);
  }

  if (host && result == blink::mojom::MediaStreamRequestResult::OK)
    host->OnGrantedMediaStreamAccess();

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaStreamUIProxy::ProcessAccessRequestResponse, proxy_,
                     filtered_devices, result));
}

void MediaStreamUIProxy::Core::ProcessStopRequestFromUI() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaStreamUIProxy::ProcessStopRequestFromUI, proxy_));
}

void MediaStreamUIProxy::Core::ProcessChangeSourceRequestFromUI(
    const DesktopMediaID& media_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaStreamUIProxy::ProcessChangeSourceRequestFromUI,
                     proxy_, media_id));
}

void MediaStreamUIProxy::Core::ProcessStateChangeFromUI(
    const DesktopMediaID& media_id,
    blink::mojom::MediaStreamStateChange new_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&MediaStreamUIProxy::ProcessStateChangeFromUI,
                                proxy_, media_id, new_state));
}

RenderFrameHostDelegate* MediaStreamUIProxy::Core::GetRenderFrameHostDelegate(
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (test_render_delegate_)
    return test_render_delegate_;
  RenderFrameHostImpl* host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  return host ? host->delegate() : nullptr;
}

// static
std::unique_ptr<MediaStreamUIProxy> MediaStreamUIProxy::Create() {
  return std::unique_ptr<MediaStreamUIProxy>(new MediaStreamUIProxy(nullptr));
}

// static
std::unique_ptr<MediaStreamUIProxy> MediaStreamUIProxy::CreateForTests(
    RenderFrameHostDelegate* render_delegate) {
  return std::unique_ptr<MediaStreamUIProxy>(
      new MediaStreamUIProxy(render_delegate));
}

MediaStreamUIProxy::MediaStreamUIProxy(
    RenderFrameHostDelegate* test_render_delegate) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  core_.reset(new Core(weak_factory_.GetWeakPtr(), test_render_delegate));
}

MediaStreamUIProxy::~MediaStreamUIProxy() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void MediaStreamUIProxy::RequestAccess(
    std::unique_ptr<MediaStreamRequest> request,
    ResponseCallback response_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  response_callback_ = std::move(response_callback);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&Core::RequestAccess, core_->GetWeakPtr(),
                                std::move(request)));
}

void MediaStreamUIProxy::OnStarted(
    base::OnceClosure stop_callback,
    MediaStreamUI::SourceCallback source_callback,
    WindowIdCallback window_id_callback,
    const std::string& label,
    std::vector<DesktopMediaID> screen_share_ids,
    MediaStreamUI::StateChangeCallback state_change_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  stop_callback_ = std::move(stop_callback);
  source_callback_ = std::move(source_callback);
  state_change_callback_ = std::move(state_change_callback);

  // Owned by the PostTaskAndReply callback.
  gfx::NativeViewId* window_id = new gfx::NativeViewId(0);

  GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&Core::OnStarted, core_->GetWeakPtr(), window_id,
                     !!source_callback_, label, screen_share_ids),
      base::BindOnce(&MediaStreamUIProxy::OnWindowId,
                     weak_factory_.GetWeakPtr(), std::move(window_id_callback),
                     base::Owned(window_id)));
}

void MediaStreamUIProxy::OnDeviceStopped(const std::string& label,
                                         const DesktopMediaID& media_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&Core::OnDeviceStopped, core_->GetWeakPtr(),
                                label, media_id));
}

#if !defined(OS_ANDROID)
void MediaStreamUIProxy::SetFocus(const DesktopMediaID& media_id,
                                  bool focus,
                                  bool is_from_microtask,
                                  bool is_from_timer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&Core::SetFocus, core_->GetWeakPtr(), media_id,
                                focus, is_from_microtask, is_from_timer));
}
#endif

void MediaStreamUIProxy::ProcessAccessRequestResponse(
    const blink::MediaStreamDevices& devices,
    blink::mojom::MediaStreamRequestResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!response_callback_.is_null());

  std::move(response_callback_).Run(devices, result);
}

void MediaStreamUIProxy::ProcessStopRequestFromUI() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (stop_callback_)
    std::move(stop_callback_).Run();
}

void MediaStreamUIProxy::ProcessChangeSourceRequestFromUI(
    const DesktopMediaID& media_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (source_callback_)
    source_callback_.Run(media_id);
}

void MediaStreamUIProxy::ProcessStateChangeFromUI(
    const DesktopMediaID& media_id,
    blink::mojom::MediaStreamStateChange new_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (state_change_callback_)
    state_change_callback_.Run(media_id, new_state);
}

void MediaStreamUIProxy::OnWindowId(WindowIdCallback window_id_callback,
                                    gfx::NativeViewId* window_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!window_id_callback.is_null())
    std::move(window_id_callback).Run(*window_id);
}

FakeMediaStreamUIProxy::FakeMediaStreamUIProxy(
    bool tests_use_fake_render_frame_hosts)
    : MediaStreamUIProxy(nullptr), mic_access_(true), camera_access_(true) {
  core_->tests_use_fake_render_frame_hosts_ = tests_use_fake_render_frame_hosts;
}

FakeMediaStreamUIProxy::~FakeMediaStreamUIProxy() {}

void FakeMediaStreamUIProxy::SetAvailableDevices(
    const blink::MediaStreamDevices& devices) {
  devices_ = devices;
}

void FakeMediaStreamUIProxy::SetMicAccess(bool access) {
  mic_access_ = access;
}

void FakeMediaStreamUIProxy::SetCameraAccess(bool access) {
  camera_access_ = access;
}

void FakeMediaStreamUIProxy::RequestAccess(
    std::unique_ptr<MediaStreamRequest> request,
    ResponseCallback response_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  response_callback_ = std::move(response_callback);

  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kUseFakeUIForMediaStream) == "deny") {
    // Immediately deny the request.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &MediaStreamUIProxy::Core::ProcessAccessRequestResponse,
            core_->GetWeakPtr(), request->render_process_id,
            request->render_frame_id, blink::MediaStreamDevices(),
            blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
            std::unique_ptr<MediaStreamUI>()));
    return;
  }

  blink::MediaStreamDevices devices_to_use;
  bool accepted_audio = false;
  bool accepted_video = false;

  // Use the first capture device of the same media type in the list for the
  // fake UI.
  for (blink::MediaStreamDevices::const_iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    if (!accepted_audio && blink::IsAudioInputMediaType(request->audio_type) &&
        blink::IsAudioInputMediaType(it->type) &&
        (request->requested_audio_device_id.empty() ||
         request->requested_audio_device_id == it->id)) {
      devices_to_use.push_back(*it);
      accepted_audio = true;
    } else if (!accepted_video &&
               blink::IsVideoInputMediaType(request->video_type) &&
               blink::IsVideoInputMediaType(it->type) &&
               (request->requested_video_device_id.empty() ||
                request->requested_video_device_id == it->id)) {
      devices_to_use.push_back(*it);
      accepted_video = true;
    }
  }

  // Fail the request if a device doesn't exist for the requested type.
  if ((request->audio_type != blink::mojom::MediaStreamType::NO_SERVICE &&
       !accepted_audio) ||
      (request->video_type != blink::mojom::MediaStreamType::NO_SERVICE &&
       !accepted_video)) {
    devices_to_use.clear();
  }

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaStreamUIProxy::Core::ProcessAccessRequestResponse,
                     core_->GetWeakPtr(), request->render_process_id,
                     request->render_frame_id, devices_to_use,
                     devices_to_use.empty()
                         ? blink::mojom::MediaStreamRequestResult::NO_HARDWARE
                         : blink::mojom::MediaStreamRequestResult::OK,
                     std::unique_ptr<MediaStreamUI>()));
}

void FakeMediaStreamUIProxy::OnStarted(
    base::OnceClosure stop_callback,
    MediaStreamUI::SourceCallback source_callback,
    WindowIdCallback window_id_callback,
    const std::string& label,
    std::vector<DesktopMediaID> screen_share_ids,
    MediaStreamUI::StateChangeCallback state_change_callback) {}

void FakeMediaStreamUIProxy::OnDeviceStopped(const std::string& label,
                                             const DesktopMediaID& media_id) {}

}  // namespace content
