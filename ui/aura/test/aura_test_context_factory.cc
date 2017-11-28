// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/aura_test_context_factory.h"

#include "cc/test/fake_output_surface.h"
#include "cc/test/test_context_provider.h"
#include "components/viz/test/test_layer_tree_frame_sink.h"

namespace aura {
namespace test {
namespace {

class FrameSinkClient : public viz::TestLayerTreeFrameSinkClient {
 public:
  explicit FrameSinkClient(
      scoped_refptr<viz::ContextProvider> display_context_provider)
      : display_context_provider_(std::move(display_context_provider)) {}

  // viz::TestLayerTreeFrameSinkClient:
  std::unique_ptr<viz::OutputSurface> CreateDisplayOutputSurface(
      scoped_refptr<viz::ContextProvider> compositor_context_provider)
      override {
    return cc::FakeOutputSurface::Create3d(
        std::move(display_context_provider_));
  }
  void DisplayReceivedLocalSurfaceId(
      const viz::LocalSurfaceId& local_surface_id) override {}
  void DisplayReceivedCompositorFrame(
      const viz::CompositorFrame& frame) override {}
  void DisplayWillDrawAndSwap(
      bool will_draw_and_swap,
      const viz::RenderPassList& render_passes) override {}
  void DisplayDidDrawAndSwap() override {}

 private:
  scoped_refptr<viz::ContextProvider> display_context_provider_;

  DISALLOW_COPY_AND_ASSIGN(FrameSinkClient);
};

}  // namespace

AuraTestContextFactory::AuraTestContextFactory() = default;

AuraTestContextFactory::~AuraTestContextFactory() = default;

void AuraTestContextFactory::CreateLayerTreeFrameSink(
    base::WeakPtr<ui::Compositor> compositor) {
  scoped_refptr<cc::TestContextProvider> context_provider =
      cc::TestContextProvider::Create();
  std::unique_ptr<FrameSinkClient> frame_sink_client =
      std::make_unique<FrameSinkClient>(context_provider);
  constexpr bool synchronous_composite = false;
  constexpr bool disable_display_vsync = false;
  const double refresh_rate = GetRefreshRate();
  auto frame_sink = std::make_unique<viz::TestLayerTreeFrameSink>(
      context_provider, cc::TestContextProvider::CreateWorker(), nullptr,
      GetGpuMemoryBufferManager(), renderer_settings(),
      base::ThreadTaskRunnerHandle::Get().get(), synchronous_composite,
      disable_display_vsync, refresh_rate);
  frame_sink->SetClient(frame_sink_client.get());
  compositor->SetLayerTreeFrameSink(std::move(frame_sink));
  frame_sink_clients_.insert(std::move(frame_sink_client));
}

}  // namespace test
}  // namespace aura
