// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_RENDERER_WEB_ENGINE_CONTENT_RENDERER_CLIENT_H_
#define FUCHSIA_ENGINE_RENDERER_WEB_ENGINE_CONTENT_RENDERER_CLIENT_H_

#include "base/macros.h"
#include "content/public/renderer/content_renderer_client.h"
#include "fuchsia/engine/renderer/web_engine_render_frame_observer.h"

namespace util {
class MultiSourceMemoryPressureMonitor;
}  // namespace util

class WebEngineContentRendererClient : public content::ContentRendererClient {
 public:
  WebEngineContentRendererClient();
  ~WebEngineContentRendererClient() override;

  // Returns the WebEngineRenderFrameObserver corresponding to
  // |render_frame_id|.
  WebEngineRenderFrameObserver* GetWebEngineRenderFrameObserverForRenderFrameId(
      int render_frame_id) const;

 private:
  // Called by WebEngineRenderFrameObserver when its corresponding RenderFrame
  // is in the process of being deleted.
  void OnRenderFrameDeleted(int render_frame_id);

  // content::ContentRendererClient overrides.
  void RenderThreadStarted() override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void AddSupportedKeySystems(
      std::vector<std::unique_ptr<media::KeySystemProperties>>* key_systems)
      override;
  bool IsSupportedVideoType(const media::VideoType& type) override;
  std::unique_ptr<blink::URLLoaderThrottleProvider>
  CreateURLLoaderThrottleProvider(
      blink::URLLoaderThrottleProviderType type) override;
  bool DeferMediaLoad(content::RenderFrame* render_frame,
                      bool has_played_media_before,
                      base::OnceClosure closure) override;
  std::unique_ptr<media::Demuxer> OverrideDemuxerForUrl(
      content::RenderFrame* render_frame,
      const GURL& url,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner) override;

  bool RunClosureWhenInForeground(content::RenderFrame* render_frame,
                                  base::OnceClosure closure);

  // Map of RenderFrame ID to WebEngineRenderFrameObserver.
  std::map<int, std::unique_ptr<WebEngineRenderFrameObserver>>
      render_frame_id_to_observer_map_;

  // Initiates cache purges and Blink/V8 garbage collection when free memory
  // is limited.
  std::unique_ptr<util::MultiSourceMemoryPressureMonitor>
      memory_pressure_monitor_;

  DISALLOW_COPY_AND_ASSIGN(WebEngineContentRendererClient);
};

#endif  // FUCHSIA_ENGINE_RENDERER_WEB_ENGINE_CONTENT_RENDERER_CLIENT_H_
