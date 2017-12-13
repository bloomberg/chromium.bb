// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/window_port_mus.h"

#include "components/viz/client/client_layer_tree_frame_sink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/local/layer_tree_frame_sink_local.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_switches_util.h"

namespace aura {

class WindowPortMusTest : public test::AuraTestBase {
 public:
  WindowPortMusTest() { EnableMusWithTestWindowTree(); }

  ~WindowPortMusTest() override = default;

  base::WeakPtr<cc::LayerTreeFrameSink> GetFrameSinkFor(Window* window) {
    auto* window_mus = WindowPortMus::Get(window);
    return window_mus->local_layer_tree_frame_sink_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowPortMusTest);
};

TEST_F(WindowPortMusTest, LayerTreeFrameSinkGetsCorrectLocalSurfaceId) {
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  window.SetBounds(gfx::Rect(300, 300));
  // Notify the window that it will embed an external client, so that it
  // correctly generates LocalSurfaceId.
  window.set_embed_frame_sink_id(viz::FrameSinkId(0, 1));

  viz::LocalSurfaceId local_surface_id = window.GetLocalSurfaceId();
  ASSERT_TRUE(local_surface_id.is_valid());

  std::unique_ptr<cc::LayerTreeFrameSink> frame_sink(
      window.CreateLayerTreeFrameSink());
  EXPECT_TRUE(frame_sink.get());

  auto mus_frame_sink = GetFrameSinkFor(&window);
  ASSERT_TRUE(mus_frame_sink);
  auto frame_sink_local_surface_id =
      switches::IsMusHostingViz()
          ? static_cast<viz::ClientLayerTreeFrameSink*>(mus_frame_sink.get())
                ->local_surface_id()
          : static_cast<LayerTreeFrameSinkLocal*>(mus_frame_sink.get())
                ->local_surface_id();
  EXPECT_TRUE(frame_sink_local_surface_id.is_valid());
  EXPECT_EQ(frame_sink_local_surface_id, local_surface_id);
}

}  // namespace aura
