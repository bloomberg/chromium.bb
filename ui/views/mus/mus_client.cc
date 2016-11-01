// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/mus_client.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/cpp/gpu_service.h"
#include "services/ui/public/interfaces/event_matcher.mojom.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/os_exchange_data_provider_mus.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/clipboard_mus.h"
#include "ui/views/mus/screen_mus.h"
#include "ui/views/mus/surface_context_factory.h"
#include "ui/views/views_delegate.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/wm_state.h"

namespace views {
namespace {

// TODO: this is temporary, figure out what the real one should be like.
class FocusRulesMus : public wm::BaseFocusRules {
 public:
  FocusRulesMus() {}
  ~FocusRulesMus() override {}

  // wm::BaseFocusRules::
  bool SupportsChildActivation(aura::Window* window) const override {
    NOTIMPLEMENTED();
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusRulesMus);
};

class PropertyConverterImpl : public aura::PropertyConverter {
 public:
  PropertyConverterImpl() {}
  ~PropertyConverterImpl() override {}

  // aura::PropertyConverter:
  bool ConvertPropertyForTransport(
      aura::Window* window,
      const void* key,
      std::string* transport_name,
      std::unique_ptr<std::vector<uint8_t>>* transport_value) override {
    return false;
  }
  std::string GetTransportNameForPropertyKey(const void* key) override {
    return std::string();
  }
  void SetPropertyFromTransportValue(
      aura::Window* window,
      const std::string& transport_name,
      const std::vector<uint8_t>* transport_data) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PropertyConverterImpl);
};

}  // namespace

MusClient::MusClient(service_manager::Connector* connector,
                     const service_manager::Identity& identity,
                     const std::string& resource_file,
                     const std::string& resource_file_200,
                     scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : connector_(connector), identity_(identity) {
  aura_init_ = base::MakeUnique<AuraInit>(
      connector, resource_file, resource_file_200,
      base::Bind(&MusClient::CreateWindowPort, base::Unretained(this)));

  property_converter_ = base::MakeUnique<PropertyConverterImpl>();

  wm_state_ = base::MakeUnique<wm::WMState>();

  gpu_service_ = ui::GpuService::Create(connector, std::move(io_task_runner));
  compositor_context_factory_ =
      base::MakeUnique<views::SurfaceContextFactory>(gpu_service_.get());
  aura::Env::GetInstance()->set_context_factory(
      compositor_context_factory_.get());
  // TODO(sky): need a class similar to DesktopFocusRules.
  focus_controller_ = base::MakeUnique<wm::FocusController>(new FocusRulesMus);
  window_tree_client_ =
      base::MakeUnique<aura::WindowTreeClient>(this, nullptr, nullptr);
  window_tree_client_->ConnectViaWindowTreeFactory(connector_);

  // TODO(sky): wire up PointerWatcherEventRouter.

  screen_ = base::MakeUnique<ScreenMus>(this);
  screen_->Init(connector);

  std::unique_ptr<ClipboardMus> clipboard = base::MakeUnique<ClipboardMus>();
  clipboard->Init(connector);
  ui::Clipboard::SetClipboardForCurrentThread(std::move(clipboard));

  ui::OSExchangeDataProviderFactory::SetFactory(this);

  ViewsDelegate::GetInstance()->set_native_widget_factory(
      base::Bind(&MusClient::CreateNativeWidget, base::Unretained(this)));
}

MusClient::~MusClient() {
  // ~WindowTreeClient calls back to us (we're its delegate), destroy it while
  // we are still valid.
  window_tree_client_.reset();
  ui::OSExchangeDataProviderFactory::SetFactory(nullptr);
  ui::Clipboard::DestroyClipboardForCurrentThread();
  gpu_service_.reset();

  if (ViewsDelegate::GetInstance()) {
    ViewsDelegate::GetInstance()->set_native_widget_factory(
        ViewsDelegate::NativeWidgetFactory());
  }
}

NativeWidget* MusClient::CreateNativeWidget(
    const Widget::InitParams& init_params,
    internal::NativeWidgetDelegate* delegate) {
  // TYPE_CONTROL widgets require a NativeWidgetAura. So we let this fall
  // through, so that the default NativeWidgetPrivate::CreateNativeWidget() is
  // used instead.
  if (init_params.type == Widget::InitParams::TYPE_CONTROL)
    return nullptr;
  return nullptr;
}

std::unique_ptr<aura::WindowPort> MusClient::CreateWindowPort(
    aura::Window* window) {
  return base::MakeUnique<aura::WindowPortMus>(window_tree_client_.get());
}

void MusClient::OnEmbed(aura::Window* root) {}

void MusClient::OnLostConnection(aura::WindowTreeClient* client) {}

void MusClient::OnEmbedRootDestroyed(aura::Window* root) {
  // Not called for MusClient as WindowTreeClient isn't created by
  // way of an Embed().
  NOTREACHED();
}

void MusClient::OnPointerEventObserved(const ui::PointerEvent& event,
                                       aura::Window* target) {
  // TODO(sky): wire up pointer events.
  NOTIMPLEMENTED();
}

void MusClient::OnWindowManagerFrameValuesChanged() {
  // TODO(sky): wire up to DesktopNativeWidgetAura.
  NOTIMPLEMENTED();
}

aura::client::FocusClient* MusClient::GetFocusClient() {
  return focus_controller_.get();
}

aura::client::CaptureClient* MusClient::GetCaptureClient() {
  return wm::CaptureController::Get();
}

aura::PropertyConverter* MusClient::GetPropertyConverter() {
  return property_converter_.get();
}

gfx::Point MusClient::GetCursorScreenPoint() {
  return window_tree_client_->GetCursorScreenPoint();
}

aura::Window* MusClient::GetWindowAtScreenPoint(const gfx::Point& point) {
  for (aura::Window* root : window_tree_client_->GetRoots()) {
    aura::WindowTreeHost* window_tree_host = root->GetHost();
    if (!window_tree_host)
      continue;
    // TODO: this likely gets z-order wrong.
    gfx::Point relative_point(point);
    window_tree_host->ConvertPointFromNativeScreen(&relative_point);
    if (gfx::Rect(root->bounds().size()).Contains(relative_point))
      return root->GetTopWindowContainingPoint(relative_point);
  }
  return nullptr;
}

std::unique_ptr<OSExchangeData::Provider> MusClient::BuildProvider() {
  return base::MakeUnique<aura::OSExchangeDataProviderMus>();
}

}  // namespace views
