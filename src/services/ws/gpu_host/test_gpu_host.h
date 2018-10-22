// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_GPU_HOST_TEST_GPU_HOST_H_
#define SERVICES_WS_GPU_HOST_TEST_GPU_HOST_H_

#include "base/macros.h"
#include "components/viz/test/test_frame_sink_manager.h"
#include "services/ws/gpu_host/gpu_host.h"

namespace ws {
namespace gpu_host {

class TestGpuHost : public gpu_host::GpuHost {
 public:
  TestGpuHost();
  ~TestGpuHost() override;

 private:
  void Add(mojom::GpuRequest request) override {}
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override {}
  void OnAcceleratedWidgetDestroyed(gfx::AcceleratedWidget widget) override {}
  void CreateFrameSinkManager(
      viz::mojom::FrameSinkManagerParamsPtr params) override;
#if defined(OS_CHROMEOS)
  void AddArc(mojom::ArcRequest request) override {}
#endif  // defined(OS_CHROMEOS)

  std::unique_ptr<viz::TestFrameSinkManagerImpl> frame_sink_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestGpuHost);
};

}  // namespace gpu_host
}  // namespace ws

#endif  // SERVICES_WS_GPU_HOST_TEST_GPU_HOST_H_
