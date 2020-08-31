// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/capture_access_handler_base.h"

#include <string>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/extension.h"
#include "ui/gfx/native_widget_types.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

#if defined(OS_CHROMEOS)
#include "base/hash/sha1.h"
#include "base/strings/string_number_conversions.h"
#endif  // defined(OS_CHROMEOS)

using content::BrowserThread;

namespace {

// Keep track of the outermost WebContents for a particular tab.
// Similar semantics as base::WeakPtr<WebContents> as WebContentsObserver
// will detect when the WebContents no longer exists.
class WeakPtrToWebContents : private content::WebContentsObserver {
 public:
  WeakPtrToWebContents() = default;
  ~WeakPtrToWebContents() override = default;

  void Set(int render_process_id, int render_frame_id) {
    auto* target_web_contents = content::WebContents::FromRenderFrameHost(
        content::RenderFrameHost::FromID(render_process_id, render_frame_id));
    // Use the outermost WebContents in the WebContents tree, if possible.
    // If we can't find the WebContents, clear the observer.
    WebContentsObserver::Observe(
        target_web_contents ? target_web_contents->GetOutermostWebContents()
                            : nullptr);
  }

  content::WebContents* get() const {
    return WebContentsObserver::web_contents();
  }

 private:
  // WebContentsObserver does not allow copy or assign.
  DISALLOW_COPY_AND_ASSIGN(WeakPtrToWebContents);
};

}  // namespace

// Tracks MEDIA_DESKTOP/TAB_VIDEO_CAPTURE sessions. Sessions are removed when
// MEDIA_REQUEST_STATE_CLOSING is encountered.
struct CaptureAccessHandlerBase::Session {
  int request_process_id;
  int request_frame_id;
  int page_request_id;
  // Extensions control the routing of the captured MediaStream content.
  // Therefore, only built-in extensions (and certain whitelisted ones) can be
  // trusted to set-up secure links.
  bool is_trusted;

  // This is true only if all connected video sinks are reported secure.
  bool is_capturing_link_secure;

  // Reflects what this session is capturing. If the whole display is captured,
  // then |capturing_type| is TYPE_SCREEN. If a specific window is specified,
  // then |capturing_type| is TYPE_WINDOW and |target_window| is set. If a
  // specific tab is the target, then |capturing_type| is TYPE_WEB_CONTENTS and
  // |target_weak_web_contents| is set.
  content::DesktopMediaID::Type capturing_type;
  gfx::NativeWindow target_window;
  std::unique_ptr<WeakPtrToWebContents> target_weak_web_contents;
};

CaptureAccessHandlerBase::CaptureAccessHandlerBase() = default;

CaptureAccessHandlerBase::~CaptureAccessHandlerBase() = default;

void CaptureAccessHandlerBase::AddCaptureSession(int render_process_id,
                                                 int render_frame_id,
                                                 int page_request_id,
                                                 bool is_trusted) {
  Session session = {
      render_process_id, render_frame_id, page_request_id, is_trusted, true,
      // Assume that the target is the same tab that is
      // requesting capture, not the display or any particular
      // window. This can be changed by calling UpdateTarget().
      content::DesktopMediaID::TYPE_WEB_CONTENTS, gfx::kNullNativeWindow,
      std::make_unique<WeakPtrToWebContents>()};
  session.target_weak_web_contents->Set(render_process_id, render_frame_id);

  sessions_.push_back(std::move(session));
}

void CaptureAccessHandlerBase::RemoveCaptureSession(int render_process_id,
                                                    int render_frame_id,
                                                    int page_request_id) {
  auto it = FindSession(render_process_id, render_frame_id, page_request_id);
  if (it != sessions_.end())
    sessions_.erase(it);
}

std::list<CaptureAccessHandlerBase::Session>::iterator
CaptureAccessHandlerBase::FindSession(int render_process_id,
                                      int render_frame_id,
                                      int page_request_id) {
  return std::find_if(sessions_.begin(), sessions_.end(),
                      [render_process_id, render_frame_id,
                       page_request_id](const Session& session) {
                        return session.request_process_id ==
                                   render_process_id &&
                               session.request_frame_id == render_frame_id &&
                               session.page_request_id == page_request_id;
                      });
}

void CaptureAccessHandlerBase::UpdateMediaRequestState(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    blink::mojom::MediaStreamType stream_type,
    content::MediaRequestState state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if ((stream_type !=
       blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE) &&
      (stream_type != blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE) &&
      (stream_type != blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE) &&
      (stream_type != blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE)) {
    return;
  }

  if (state == content::MEDIA_REQUEST_STATE_DONE) {
    if (FindSession(render_process_id, render_frame_id, page_request_id) ==
        sessions_.end()) {
      AddCaptureSession(render_process_id, render_frame_id, page_request_id,
                        false);
      DVLOG(2) << "Add new session while UpdateMediaRequestState"
               << " render_process_id: " << render_process_id
               << " render_frame_id: " << render_frame_id
               << " page_request_id: " << page_request_id;
    }
  } else if (state == content::MEDIA_REQUEST_STATE_CLOSING) {
    RemoveCaptureSession(render_process_id, render_frame_id, page_request_id);
    DVLOG(2) << "Remove session: "
             << " render_process_id: " << render_process_id
             << " render_frame_id: " << render_frame_id
             << " page_request_id: " << page_request_id;
  }
}

void CaptureAccessHandlerBase::UpdateExtensionTrusted(
    const content::MediaStreamRequest& request,
    const extensions::Extension* extension) {
  const bool is_trusted = MediaCaptureDevicesDispatcher::IsOriginForCasting(
                              request.security_origin) ||
                          IsExtensionWhitelistedForScreenCapture(extension) ||
                          IsBuiltInExtension(request.security_origin);
  UpdateTrusted(request, is_trusted);
}

void CaptureAccessHandlerBase::UpdateTrusted(
    const content::MediaStreamRequest& request,
    bool is_trusted) {
  auto it = FindSession(request.render_process_id, request.render_frame_id,
                        request.page_request_id);
  if (it != sessions_.end()) {
    it->is_trusted = is_trusted;
    DVLOG(2) << "CaptureAccessHandlerBase::UpdateTrusted"
             << " render_process_id: " << request.render_process_id
             << " render_frame_id: " << request.render_frame_id
             << " page_request_id: " << request.page_request_id
             << " is_trusted: " << is_trusted;
    return;
  }

  AddCaptureSession(request.render_process_id, request.render_frame_id,
                    request.page_request_id, is_trusted);
  DVLOG(2) << "Add new session while UpdateTrusted"
           << " render_process_id: " << request.render_process_id
           << " render_frame_id: " << request.render_frame_id
           << " page_request_id: " << request.page_request_id
           << " is_trusted: " << is_trusted;
}

void CaptureAccessHandlerBase::UpdateTarget(
    const content::MediaStreamRequest& request,
    const content::DesktopMediaID& target) {
  DVLOG(2) << __func__ << " requestor: " << request.render_process_id << ":"
           << request.render_frame_id << ", target: " << target.ToString();

  auto it = FindSession(request.render_process_id, request.render_frame_id,
                        request.page_request_id);
  if (it == sessions_.end()) {
    DLOG(WARNING) << "UpdateTarget() can not find the session.";
    return;
  }

  // Update target fields.
  it->capturing_type = target.type;
  if (target.type == content::DesktopMediaID::TYPE_WINDOW) {
    // If this is the Chrome window, then any tab in this window could be
    // captured.
    // TODO(crbug.com/856276): Implement this for MacOS.
#if defined(USE_AURA)
    it->target_window = content::DesktopMediaID::GetNativeWindowById(target);
#endif
  } else if (target.type == content::DesktopMediaID::TYPE_WEB_CONTENTS) {
    // Specific tab captured.
    it->target_weak_web_contents->Set(
        target.web_contents_id.render_process_id,
        target.web_contents_id.main_render_frame_id);
  }
}

bool CaptureAccessHandlerBase::IsInsecureCapturingInProgress(
    int render_process_id,
    int render_frame_id) {
  DVLOG(3) << __func__ << " checking for " << render_process_id << ":"
           << render_frame_id;
  if (sessions_.empty())
    return false;

  // Check if the frame is being captured insecurely as the target of any
  // session.
  for (const Session& session : sessions_) {
    if (MatchesSession(session, render_process_id, render_frame_id))
      if (!session.is_trusted || !session.is_capturing_link_secure)
        return true;
  }

  return false;
}

// static
bool CaptureAccessHandlerBase::MatchesSession(const Session& session,
                                              int target_process_id,
                                              int target_frame_id) {
  switch (session.capturing_type) {
    case content::DesktopMediaID::TYPE_NONE:
      // Session has no target, so it doesn't match.
      return false;

    case content::DesktopMediaID::TYPE_SCREEN:
      // Capturing the whole screen.
      return true;

    case content::DesktopMediaID::TYPE_WINDOW:
#if defined(USE_AURA)
    {
      // Check if the window hosting this frame is the target.
      auto* host =
          content::RenderFrameHost::FromID(target_process_id, target_frame_id);
      if (!host)
        return false;

      gfx::NativeWindow window = host->GetNativeView()->GetRootWindow();
      return window && window == session.target_window;
    }
#else
      // Unable to determine the window.
      // TODO(crbug.com/856276): Implement this for MacOS.
      return false;
#endif  // defined(USE_AURA)

    case content::DesktopMediaID::TYPE_WEB_CONTENTS:
      // Check that the target is the frame selected.
      auto* target_web_contents = session.target_weak_web_contents->get();
      if (!target_web_contents)
        return false;
      auto* web_contents = content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(target_process_id, target_frame_id));
      if (!web_contents)
        return false;
      return target_web_contents == web_contents->GetOutermostWebContents();
  }

  NOTREACHED();
  return false;
}

void CaptureAccessHandlerBase::UpdateVideoScreenCaptureStatus(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    bool is_secure) {
  auto it = FindSession(render_process_id, render_frame_id, page_request_id);
  if (it != sessions_.end()) {
    it->is_capturing_link_secure = is_secure;
    DVLOG(2) << "UpdateVideoScreenCaptureStatus:"
             << " render_process_id: " << render_process_id
             << " render_frame_id: " << render_frame_id
             << " page_request_id: " << page_request_id
             << " is_capturing_link_secure: " << is_secure;
  }
}

bool CaptureAccessHandlerBase::IsExtensionWhitelistedForScreenCapture(
    const extensions::Extension* extension) {
  if (!extension)
    return false;

#if defined(OS_CHROMEOS)
  std::string hash = base::SHA1HashString(extension->id());
  std::string hex_hash = base::HexEncode(hash.c_str(), hash.length());

  // crbug.com/446688
  return hex_hash == "4F25792AF1AA7483936DE29C07806F203C7170A0" ||
         hex_hash == "BD8781D757D830FC2E85470A1B6E8A718B7EE0D9" ||
         hex_hash == "4AC2B6C63C6480D150DFDA13E4A5956EB1D0DDBB" ||
         hex_hash == "81986D4F846CEDDDB962643FA501D1780DD441BB";
#else
  return false;
#endif  // defined(OS_CHROMEOS)
}

bool CaptureAccessHandlerBase::IsBuiltInExtension(const GURL& origin) {
  return
      // Feedback Extension.
      origin.spec() == "chrome-extension://gfdkimpbcpahaombhbimeihdjnejgicl/";
}
