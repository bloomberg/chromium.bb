// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_media_blocker.h"

#include <utility>

#include "base/threading/thread_checker.h"
#include "chromecast/common/mojom/media_playback_options.mojom.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace chromecast {

CastMediaBlocker::CastMediaBlocker(content::WebContents* web_contents)
    : CastMediaBlocker(content::MediaSession::Get(web_contents)) {
  content::WebContentsObserver::Observe(web_contents);
}

CastMediaBlocker::CastMediaBlocker(content::MediaSession* media_session)
    : blocked_(false),
      paused_by_user_(true),
      suspended_(true),
      controllable_(false),
      background_video_playback_enabled_(false),
      media_session_(media_session) {
  media_session::mojom::MediaSessionObserverPtr observer;
  observer_binding_.Bind(mojo::MakeRequest(&observer));
  media_session_->AddObserver(std::move(observer));
}

CastMediaBlocker::~CastMediaBlocker() {}

void CastMediaBlocker::BlockMediaLoading(bool blocked) {
  if (blocked_ == blocked)
    return;

  blocked_ = blocked;
  UpdateMediaBlockedState();

  LOG(INFO) << __FUNCTION__ << " blocked=" << blocked_
            << " suspended=" << suspended_ << " controllable=" << controllable_
            << " paused_by_user=" << paused_by_user_;

  // If blocking media, suspend if possible.
  if (blocked_) {
    if (!suspended_ && controllable_) {
      Suspend();
    }
    return;
  }

  // If unblocking media, resume if media was not paused by user.
  if (!paused_by_user_ && suspended_ && controllable_) {
    paused_by_user_ = true;
    Resume();
  }
}

void CastMediaBlocker::EnableBackgroundVideoPlayback(bool enabled) {
  if (!web_contents())
    return;

  background_video_playback_enabled_ = enabled;
  UpdateBackgroundVideoPlaybackState();
}

void CastMediaBlocker::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  bool is_suspended = session_info->playback_state ==
                      media_session::mojom::MediaPlaybackState::kPaused;

  LOG(INFO) << __FUNCTION__ << " blocked=" << blocked_
            << " is_suspended=" << is_suspended
            << " is_controllable=" << session_info->is_controllable
            << " paused_by_user=" << paused_by_user_;

  // Process controllability first.
  if (controllable_ != session_info->is_controllable) {
    controllable_ = session_info->is_controllable;

    // If not blocked, and we regain control and the media wasn't paused when
    // blocked, resume media if suspended.
    if (!blocked_ && !paused_by_user_ && is_suspended && controllable_) {
      paused_by_user_ = true;
      Resume();
    }

    // Suspend if blocked and the session becomes controllable.
    if (blocked_ && !is_suspended && controllable_) {
      // Only suspend if suspended_ doesn't change. Otherwise, this will be
      // handled in the suspended changed block.
      if (suspended_ == is_suspended)
        Suspend();
    }
  }

  // Process suspended state next.
  if (suspended_ != is_suspended) {
    suspended_ = is_suspended;
    // If blocking, suspend media whenever possible.
    if (blocked_ && !suspended_) {
      // If media was resumed when blocked, the user tried to play music.
      paused_by_user_ = false;
      if (controllable_)
        Suspend();
    }

    // If not blocking, cache the user's play intent.
    if (!blocked_)
      paused_by_user_ = suspended_;
  }
}

void CastMediaBlocker::Suspend() {
  if (!media_session_)
    return;

  LOG(INFO) << "Suspending media session.";
  media_session_->Suspend(content::MediaSession::SuspendType::kSystem);
}

void CastMediaBlocker::Resume() {
  if (!media_session_)
    return;

  LOG(INFO) << "Resuming media session.";
  media_session_->Resume(content::MediaSession::SuspendType::kSystem);
}

void CastMediaBlocker::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  UpdateRenderFrameMediaBlockedState(render_frame_host);
  UpdateRenderFrameBackgroundVideoPlaybackState(render_frame_host);
}

void CastMediaBlocker::RenderViewReady() {
  UpdateMediaBlockedState();
}

void CastMediaBlocker::UpdateMediaBlockedState() {
  if (!web_contents())
    return;

  const std::vector<content::RenderFrameHost*> frames =
      web_contents()->GetAllFrames();
  for (content::RenderFrameHost* frame : frames) {
    UpdateRenderFrameMediaBlockedState(frame);
  }
}

void CastMediaBlocker::UpdateBackgroundVideoPlaybackState() {
  if (!web_contents())
    return;
  const std::vector<content::RenderFrameHost*> frames =
      web_contents()->GetAllFrames();
  for (content::RenderFrameHost* frame : frames) {
    UpdateRenderFrameBackgroundVideoPlaybackState(frame);
  }
}

void CastMediaBlocker::UpdateRenderFrameMediaBlockedState(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);
  chromecast::shell::mojom::MediaPlaybackOptionsAssociatedPtr
      media_playback_options;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &media_playback_options);
  media_playback_options->SetMediaLoadingBlocked(media_loading_blocked());
}

void CastMediaBlocker::UpdateRenderFrameBackgroundVideoPlaybackState(
    content::RenderFrameHost* frame) {
  chromecast::shell::mojom::MediaPlaybackOptionsAssociatedPtr
      media_playback_options;
  frame->GetRemoteAssociatedInterfaces()->GetInterface(&media_playback_options);
  media_playback_options->SetBackgroundVideoPlaybackEnabled(
      background_video_playback_enabled_);
}

}  // namespace chromecast
