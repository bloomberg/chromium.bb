// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_HOST_DISPLAY_CLIENT_H_
#define COMPONENTS_VIZ_HOST_HOST_DISPLAY_CLIENT_H_

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/host/viz_host_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"
#include "ui/gfx/native_widget_types.h"

namespace viz {

class LayeredWindowUpdaterImpl;

// mojom::DisplayClient implementation that relays calls to platform specific
// functions.
class VIZ_HOST_EXPORT HostDisplayClient : public mojom::DisplayClient {
 public:
  explicit HostDisplayClient(gfx::AcceleratedWidget widget);

  HostDisplayClient(const HostDisplayClient&) = delete;
  HostDisplayClient& operator=(const HostDisplayClient&) = delete;

  ~HostDisplayClient() override;

  mojo::PendingRemote<mojom::DisplayClient> GetBoundRemote(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

 private:
  // mojom::DisplayClient implementation:
#if defined(OS_APPLE)
  void OnDisplayReceivedCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override;
#endif

#if defined(OS_WIN)
  void CreateLayeredWindowUpdater(
      mojo::PendingReceiver<mojom::LayeredWindowUpdater> receiver) override;
#endif

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  void DidCompleteSwapWithNewSize(const gfx::Size& size) override;
#endif

  mojo::Receiver<mojom::DisplayClient> receiver_{this};
#if defined(OS_APPLE) || defined(OS_WIN)
  gfx::AcceleratedWidget widget_;
#endif

#if defined(OS_WIN)
  std::unique_ptr<LayeredWindowUpdaterImpl> layered_window_updater_;
#endif
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_HOST_DISPLAY_CLIENT_H_
