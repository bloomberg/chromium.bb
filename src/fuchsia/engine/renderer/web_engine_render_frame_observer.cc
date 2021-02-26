// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/renderer/web_engine_render_frame_observer.h"

#include "content/public/renderer/render_frame.h"

WebEngineRenderFrameObserver::WebEngineRenderFrameObserver(
    content::RenderFrame* render_frame,
    base::OnceCallback<void(int)> on_render_frame_deleted_callback)
    : content::RenderFrameObserver(render_frame),
      url_request_rules_receiver_(render_frame),
      cast_streaming_receiver_(render_frame),
      on_render_frame_deleted_callback_(
          std::move(on_render_frame_deleted_callback)) {
  DCHECK(render_frame);
  DCHECK(on_render_frame_deleted_callback_);
}

WebEngineRenderFrameObserver::~WebEngineRenderFrameObserver() = default;

void WebEngineRenderFrameObserver::OnDestruct() {
  std::move(on_render_frame_deleted_callback_).Run(routing_id());
}
