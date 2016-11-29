// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/mus_client.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/cpp/gpu/gpu_service.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/event_matcher.mojom.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/mus_context_factory.h"
#include "ui/aura/mus/os_exchange_data_provider_mus.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/clipboard_mus.h"
#include "ui/views/mus/desktop_window_tree_host_mus.h"
#include "ui/views/mus/pointer_watcher_event_router2.h"
#include "ui/views/mus/screen_mus.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/wm_state.h"

// Widget::InitParams::Type must match that of ui::mojom::WindowType.
#define WINDOW_TYPES_MATCH(NAME)                                      \
  static_assert(                                                      \
      static_cast<int32_t>(views::Widget::InitParams::TYPE_##NAME) == \
          static_cast<int32_t>(ui::mojom::WindowType::NAME),          \
      "Window type constants must match")

WINDOW_TYPES_MATCH(WINDOW);
WINDOW_TYPES_MATCH(PANEL);
WINDOW_TYPES_MATCH(WINDOW_FRAMELESS);
WINDOW_TYPES_MATCH(CONTROL);
WINDOW_TYPES_MATCH(POPUP);
WINDOW_TYPES_MATCH(MENU);
WINDOW_TYPES_MATCH(TOOLTIP);
WINDOW_TYPES_MATCH(BUBBLE);
WINDOW_TYPES_MATCH(DRAG);
// ui::mojom::WindowType::UNKNOWN does not correspond to a value in
// Widget::InitParams::Type.

namespace views {

// static
MusClient* MusClient::instance_ = nullptr;

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

  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

// static
bool MusClient::ShouldCreateDesktopNativeWidgetAura(
    const Widget::InitParams& init_params) {
  // TYPE_CONTROL and child widgets require a NativeWidgetAura.
  return init_params.type != Widget::InitParams::TYPE_CONTROL &&
         !init_params.child;
}

// static
std::map<std::string, std::vector<uint8_t>>
MusClient::ConfigurePropertiesFromParams(
    const Widget::InitParams& init_params) {
  using PrimitiveType = aura::PropertyConverter::PrimitiveType;
  std::map<std::string, std::vector<uint8_t>> properties =
      init_params.mus_properties;

  // Widget::InitParams::Type matches ui::mojom::WindowType.
  properties[ui::mojom::WindowManager::kWindowType_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int32_t>(init_params.type));

  if (!init_params.bounds.IsEmpty()) {
    properties[ui::mojom::WindowManager::kInitialBounds_Property] =
        mojo::ConvertTo<std::vector<uint8_t>>(init_params.bounds);
  }

  if (!init_params.name.empty()) {
    properties[ui::mojom::WindowManager::kName_Property] =
        mojo::ConvertTo<std::vector<uint8_t>>(init_params.name);
  }

  properties[ui::mojom::WindowManager::kAlwaysOnTop_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<PrimitiveType>(init_params.keep_on_top));

  if (!Widget::RequiresNonClientView(init_params.type))
    return properties;

  if (init_params.delegate) {
    if (properties.count(ui::mojom::WindowManager::kResizeBehavior_Property) ==
        0) {
      properties[ui::mojom::WindowManager::kResizeBehavior_Property] =
          mojo::ConvertTo<std::vector<uint8_t>>(static_cast<PrimitiveType>(
              DesktopWindowTreeHostMus::GetResizeBehaviorFromDelegate(
                  init_params.delegate)));
    }

    // TODO(crbug.com/667566): Support additional scales or gfx::Image[Skia].
    gfx::ImageSkia app_icon = init_params.delegate->GetWindowAppIcon();
    SkBitmap app_bitmap = app_icon.GetRepresentation(1.f).sk_bitmap();
    if (!app_bitmap.isNull()) {
      properties[ui::mojom::WindowManager::kAppIcon_Property] =
          mojo::ConvertTo<std::vector<uint8_t>>(app_bitmap);
    }
    // TODO(crbug.com/667566): Support additional scales or gfx::Image[Skia].
    gfx::ImageSkia window_icon = init_params.delegate->GetWindowIcon();
    SkBitmap window_bitmap = window_icon.GetRepresentation(1.f).sk_bitmap();
    if (!window_bitmap.isNull()) {
      properties[ui::mojom::WindowManager::kWindowIcon_Property] =
          mojo::ConvertTo<std::vector<uint8_t>>(window_bitmap);
    }
  }

  return properties;
}

NativeWidget* MusClient::CreateNativeWidget(
    const Widget::InitParams& init_params,
    internal::NativeWidgetDelegate* delegate) {
  if (!ShouldCreateDesktopNativeWidgetAura(init_params)) {
    // A null return value results in creating NativeWidgetAura.
    return nullptr;
  }

  DesktopNativeWidgetAura* native_widget =
      new DesktopNativeWidgetAura(delegate);
  if (init_params.desktop_window_tree_host) {
    native_widget->SetDesktopWindowTreeHost(
        base::WrapUnique(init_params.desktop_window_tree_host));
  } else {
    std::map<std::string, std::vector<uint8_t>> mus_properties =
        ConfigurePropertiesFromParams(init_params);
    native_widget->SetDesktopWindowTreeHost(
        base::MakeUnique<DesktopWindowTreeHostMus>(delegate, native_widget,
                                                   &mus_properties));
  }
  return native_widget;
}

void MusClient::AddObserver(MusClientObserver* observer) {
  observer_list_.AddObserver(observer);
}

void MusClient::RemoveObserver(MusClientObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

MusClient::MusClient(service_manager::Connector* connector,
                     const service_manager::Identity& identity,
                     scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : connector_(connector), identity_(identity) {
  DCHECK(!instance_);
  instance_ = this;
  // TODO(msw): Avoid this... use some default value? Allow clients to extend?
  property_converter_ = base::MakeUnique<aura::PropertyConverter>();

  wm_state_ = base::MakeUnique<wm::WMState>();

  gpu_service_ = ui::GpuService::Create(connector, std::move(io_task_runner));
  compositor_context_factory_ =
      base::MakeUnique<aura::MusContextFactory>(gpu_service_.get());
  aura::Env::GetInstance()->set_context_factory(
      compositor_context_factory_.get());
  window_tree_client_ =
      base::MakeUnique<aura::WindowTreeClient>(this, nullptr, nullptr);
  aura::Env::GetInstance()->SetWindowTreeClient(window_tree_client_.get());
  window_tree_client_->ConnectViaWindowTreeFactory(connector_);

  pointer_watcher_event_router_ =
      base::MakeUnique<PointerWatcherEventRouter2>(window_tree_client_.get());

  screen_ = base::MakeUnique<ScreenMus>(this);
  screen_->Init(connector);

  std::unique_ptr<ClipboardMus> clipboard = base::MakeUnique<ClipboardMus>();
  clipboard->Init(connector);
  ui::Clipboard::SetClipboardForCurrentThread(std::move(clipboard));

  ui::OSExchangeDataProviderFactory::SetFactory(this);

  ViewsDelegate::GetInstance()->set_native_widget_factory(
      base::Bind(&MusClient::CreateNativeWidget, base::Unretained(this)));
}

void MusClient::OnEmbed(
    std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) {
  NOTREACHED();
}

void MusClient::OnLostConnection(aura::WindowTreeClient* client) {}

void MusClient::OnEmbedRootDestroyed(aura::Window* root) {
  // Not called for MusClient as WindowTreeClient isn't created by
  // way of an Embed().
  NOTREACHED();
}

void MusClient::OnPointerEventObserved(const ui::PointerEvent& event,
                                       aura::Window* target) {
  pointer_watcher_event_router_->OnPointerEventObserved(event, target);
}

void MusClient::OnWindowManagerFrameValuesChanged() {
  for (auto& observer : observer_list_)
    observer.OnWindowManagerFrameValuesChanged();
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
    // TODO: this likely gets z-order wrong. http://crbug.com/663606.
    gfx::Point relative_point(point);
    window_tree_host->ConvertScreenInPixelsToDIP(&relative_point);
    if (gfx::Rect(root->bounds().size()).Contains(relative_point))
      return root->GetTopWindowContainingPoint(relative_point);
  }
  return nullptr;
}

std::unique_ptr<OSExchangeData::Provider> MusClient::BuildProvider() {
  return base::MakeUnique<aura::OSExchangeDataProviderMus>();
}

}  // namespace views
