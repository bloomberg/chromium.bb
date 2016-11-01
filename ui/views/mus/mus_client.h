// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_MUS_CLIENT_INIT_H_
#define UI_VIEWS_MUS_MUS_CLIENT_INIT_H_

#include <string>

#include "base/macros.h"
#include "ui/aura/mus/window_tree_client_delegate.h"
#include "ui/base/dragdrop/os_exchange_data_provider_factory.h"
#include "ui/views/mus/mus_export.h"
#include "ui/views/mus/screen_mus_delegate.h"
#include "ui/views/widget/widget.h"

namespace aura {
class Env;
class PropertyConverter;
class Window;
class WindowPort;
class WindowTreeClient;
}

namespace base {
class SingleThreadTaskRunner;
}

namespace service_manager {
class Connector;
class Identity;
}

namespace ui {
class GpuService;
}

namespace wm {
class FocusController;
class WMState;
}

namespace views {

class AuraInit;
class ScreenMus;
class SurfaceContextFactory;

namespace internal {
class NativeWidgetDelegate;
}

// MusClient establishes a connection to mus and sets up necessary state so that
// aura and views target mus. This class is useful for typical clients, not the
// WindowManager.
class VIEWS_MUS_EXPORT MusClient
    : public aura::WindowTreeClientDelegate,
      public ScreenMusDelegate,
      public ui::OSExchangeDataProviderFactory::Factory {
 public:
  MusClient(
      service_manager::Connector* connector,
      const service_manager::Identity& identity,
      const std::string& resource_file,
      const std::string& resource_file_200 = std::string(),
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner = nullptr);
  ~MusClient() override;

  service_manager::Connector* connector() { return connector_; }

 private:
  NativeWidget* CreateNativeWidget(const Widget::InitParams& init_params,
                                   internal::NativeWidgetDelegate* delegate);

  // Creates aura::WindowPortMus.
  std::unique_ptr<aura::WindowPort> CreateWindowPort(aura::Window* window);

  // aura::WindowTreeClientDelegate:
  void OnEmbed(aura::Window* root) override;
  void OnLostConnection(aura::WindowTreeClient* client) override;
  void OnEmbedRootDestroyed(aura::Window* root) override;
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              aura::Window* target) override;
  aura::client::FocusClient* GetFocusClient() override;
  aura::client::CaptureClient* GetCaptureClient() override;
  aura::PropertyConverter* GetPropertyConverter() override;

  // ScreenMusDelegate:
  void OnWindowManagerFrameValuesChanged() override;
  gfx::Point GetCursorScreenPoint() override;
  aura::Window* GetWindowAtScreenPoint(const gfx::Point& point) override;

  // ui:OSExchangeDataProviderFactory::Factory:
  std::unique_ptr<OSExchangeData::Provider> BuildProvider() override;

  service_manager::Connector* connector_;
  service_manager::Identity identity_;

  std::unique_ptr<AuraInit> aura_init_;

  std::unique_ptr<wm::WMState> wm_state_;

  std::unique_ptr<ScreenMus> screen_;

  std::unique_ptr<wm::FocusController> focus_controller_;

  std::unique_ptr<aura::PropertyConverter> property_converter_;

  std::unique_ptr<aura::WindowTreeClient> window_tree_client_;

  std::unique_ptr<ui::GpuService> gpu_service_;

  std::unique_ptr<SurfaceContextFactory> compositor_context_factory_;

  DISALLOW_COPY_AND_ASSIGN(MusClient);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_MUS_CLIENT_INIT_H_
