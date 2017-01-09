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
class Thread;
}

namespace discardable_memory {
class ClientDiscardableSharedMemoryManager;
}

namespace service_manager {
class Connector;
class Identity;
}

namespace ui {
class Gpu;
}

namespace wm {
class WMState;
}

namespace views {

class MusClientObserver;
class PointerWatcherEventRouter;
class ScreenMus;

namespace internal {
class NativeWidgetDelegate;
}

namespace test {
class MusClientTestApi;
}

// MusClient establishes a connection to mus and sets up necessary state so that
// aura and views target mus. This class is useful for typical clients, not the
// WindowManager. Most clients don't create this directly, rather use
// AuraInit.
class VIEWS_MUS_EXPORT MusClient
    : public aura::WindowTreeClientDelegate,
      public ScreenMusDelegate,
      public ui::OSExchangeDataProviderFactory::Factory {
 public:
  // Most clients should use AuraInit, which creates a MusClient.
  // |create_wm_state| indicates whether MusClient should create a wm::WMState.
  MusClient(
      service_manager::Connector* connector,
      const service_manager::Identity& identity,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner = nullptr,
      bool create_wm_state = true);
  ~MusClient() override;

  static MusClient* Get() { return instance_; }
  static bool Exists() { return instance_ != nullptr; }

  // Returns true if a DesktopNativeWidgetAura should be created given the
  // specified params. If this returns false a NativeWidgetAura should be
  // created.
  static bool ShouldCreateDesktopNativeWidgetAura(
      const Widget::InitParams& init_params);

  // Returns the properties to supply to mus when creating a window.
  static std::map<std::string, std::vector<uint8_t>>
  ConfigurePropertiesFromParams(const Widget::InitParams& init_params);

  aura::WindowTreeClient* window_tree_client() {
    return window_tree_client_.get();
  }

  PointerWatcherEventRouter* pointer_watcher_event_router() {
    return pointer_watcher_event_router_.get();
  }

  ui::Gpu* gpu() { return gpu_.get(); }

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

  // aura::WindowTreeClientDelegate:
  void OnEmbed(
      std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) override;
  void OnLostConnection(aura::WindowTreeClient* client) override;
  void OnEmbedRootDestroyed(aura::WindowTreeHostMus* window_tree_host) override;
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              aura::Window* target) override;
  aura::client::CaptureClient* GetCaptureClient() override;
  aura::PropertyConverter* GetPropertyConverter() override;

  // ScreenMusDelegate:
  void OnWindowManagerFrameValuesChanged() override;
  aura::Window* GetWindowAtScreenPoint(const gfx::Point& point) override;

  // ui:OSExchangeDataProviderFactory::Factory:
  std::unique_ptr<OSExchangeData::Provider> BuildProvider() override;

  static MusClient* instance_;

  service_manager::Identity identity_;

  std::unique_ptr<base::Thread> io_thread_;

  base::ObserverList<MusClientObserver> observer_list_;

  // NOTE: this may be null (creation is based on argument supplied to
  // constructor).
  std::unique_ptr<wm::WMState> wm_state_;

  std::unique_ptr<ScreenMus> screen_;

  std::unique_ptr<aura::PropertyConverter> property_converter_;

  std::unique_ptr<aura::WindowTreeClient> window_tree_client_;

  std::unique_ptr<ui::Gpu> gpu_;

  std::unique_ptr<PointerWatcherEventRouter> pointer_watcher_event_router_;

  std::unique_ptr<aura::MusContextFactory> compositor_context_factory_;

  std::unique_ptr<discardable_memory::ClientDiscardableSharedMemoryManager>
      discardable_shared_memory_manager_;

  DISALLOW_COPY_AND_ASSIGN(MusClient);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_MUS_CLIENT_H_
