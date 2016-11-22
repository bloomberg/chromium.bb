// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_MUS_CLIENT_H_
#define UI_VIEWS_MUS_MUS_CLIENT_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "ui/aura/mus/window_tree_client_delegate.h"
#include "ui/base/dragdrop/os_exchange_data_provider_factory.h"
#include "ui/views/mus/mus_export.h"
#include "ui/views/mus/screen_mus_delegate.h"
#include "ui/views/widget/widget.h"

namespace aura {
class PropertyConverter;
class MusContextFactory;
class Window;
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
class WMState;
}

namespace views {

class MusClientObserver;
class PointerWatcherEventRouter2;
class ScreenMus;

namespace internal {
class NativeWidgetDelegate;
}

namespace test {
class MusClientTestApi;
}

// MusClient establishes a connection to mus and sets up necessary state so that
// aura and views target mus. This class is useful for typical clients, not the
// WindowManager. This class is created by AuraInit when
// AuraInit::Mode::AURA_MUS is passed as the mode.
// AuraInit::Mode::AURA to AuraInit and MusClient will be created for us.
class VIEWS_MUS_EXPORT MusClient
    : public aura::WindowTreeClientDelegate,
      public ScreenMusDelegate,
      public ui::OSExchangeDataProviderFactory::Factory {
 public:
  ~MusClient() override;

  static MusClient* Get() { return instance_; }

  // Returns true if a DesktopNativeWidgetAura should be created given the
  // specified params. If this returns false a NativeWidgetAura should be
  // created.
  static bool ShouldCreateDesktopNativeWidgetAura(
      const Widget::InitParams& init_params);

  // Returns the properties to supply to mus when creating a window.
  static std::map<std::string, std::vector<uint8_t>>
  ConfigurePropertiesFromParams(const Widget::InitParams& init_params);

  service_manager::Connector* connector() { return connector_; }

  aura::WindowTreeClient* window_tree_client() {
    return window_tree_client_.get();
  }

  PointerWatcherEventRouter2* pointer_watcher_event_router() {
    return pointer_watcher_event_router_.get();
  }

  // Creates DesktopNativeWidgetAura with DesktopWindowTreeHostMus. This is
  // set as the factory function used for creating NativeWidgets when a
  //  NativeWidget has not been explicitly set.
  NativeWidget* CreateNativeWidget(const Widget::InitParams& init_params,
                                   internal::NativeWidgetDelegate* delegate);

  void AddObserver(MusClientObserver* observer);
  void RemoveObserver(MusClientObserver* observer);

 private:
  friend class AuraInit;
  friend class test::MusClientTestApi;

  MusClient(
      service_manager::Connector* connector,
      const service_manager::Identity& identity,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner = nullptr);

  // aura::WindowTreeClientDelegate:
  void OnEmbed(
      std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) override;
  void OnLostConnection(aura::WindowTreeClient* client) override;
  void OnEmbedRootDestroyed(aura::Window* root) override;
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              aura::Window* target) override;
  aura::client::CaptureClient* GetCaptureClient() override;
  aura::PropertyConverter* GetPropertyConverter() override;

  // ScreenMusDelegate:
  void OnWindowManagerFrameValuesChanged() override;
  gfx::Point GetCursorScreenPoint() override;
  aura::Window* GetWindowAtScreenPoint(const gfx::Point& point) override;

  // ui:OSExchangeDataProviderFactory::Factory:
  std::unique_ptr<OSExchangeData::Provider> BuildProvider() override;

  static MusClient* instance_;

  service_manager::Connector* connector_;
  service_manager::Identity identity_;

  base::ObserverList<MusClientObserver> observer_list_;

  std::unique_ptr<wm::WMState> wm_state_;

  std::unique_ptr<ScreenMus> screen_;

  std::unique_ptr<aura::PropertyConverter> property_converter_;

  std::unique_ptr<aura::WindowTreeClient> window_tree_client_;

  std::unique_ptr<ui::GpuService> gpu_service_;

  std::unique_ptr<PointerWatcherEventRouter2> pointer_watcher_event_router_;

  std::unique_ptr<aura::MusContextFactory> compositor_context_factory_;

  DISALLOW_COPY_AND_ASSIGN(MusClient);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_MUS_CLIENT_H_
