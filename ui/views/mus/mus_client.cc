// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/mus_client.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread.h"
#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/cpp/gpu/gpu.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/public/interfaces/event_matcher.mojom.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/capture_synchronizer.h"
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
#include "ui/views/mus/mus_property_mirror.h"
#include "ui/views/mus/pointer_watcher_event_router.h"
#include "ui/views/mus/screen_mus.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/shadow_types.h"
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

MusClient::MusClient(service_manager::Connector* connector,
                     const service_manager::Identity& identity,
                     scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
                     bool create_wm_state)
    : identity_(identity) {
  DCHECK(!instance_);
  DCHECK(aura::Env::GetInstance());
  instance_ = this;

  if (!io_task_runner) {
    io_thread_ = base::MakeUnique<base::Thread>("IOThread");
    base::Thread::Options thread_options(base::MessageLoop::TYPE_IO, 0);
    thread_options.priority = base::ThreadPriority::NORMAL;
    CHECK(io_thread_->StartWithOptions(thread_options));
    io_task_runner = io_thread_->task_runner();
  }

  // TODO(msw): Avoid this... use some default value? Allow clients to extend?
  property_converter_ = base::MakeUnique<aura::PropertyConverter>();
  property_converter_->RegisterProperty(
      wm::kShadowElevationKey,
      ui::mojom::WindowManager::kShadowElevation_Property,
      base::Bind(&wm::IsValidShadowElevation));

  if (create_wm_state)
    wm_state_ = base::MakeUnique<wm::WMState>();

  discardable_memory::mojom::DiscardableSharedMemoryManagerPtr manager_ptr;
  connector->BindInterface(ui::mojom::kServiceName, &manager_ptr);

  discardable_shared_memory_manager_ = base::MakeUnique<
      discardable_memory::ClientDiscardableSharedMemoryManager>(
      std::move(manager_ptr), io_task_runner);
  base::DiscardableMemoryAllocator::SetInstance(
      discardable_shared_memory_manager_.get());

  window_tree_client_ = base::MakeUnique<aura::WindowTreeClient>(
      connector, this, nullptr /* window_manager_delegate */,
      nullptr /* window_tree_client_request */, std::move(io_task_runner));
  aura::Env::GetInstance()->SetWindowTreeClient(window_tree_client_.get());
  window_tree_client_->ConnectViaWindowTreeFactory();

  pointer_watcher_event_router_ =
      base::MakeUnique<PointerWatcherEventRouter>(window_tree_client_.get());

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

  if (ViewsDelegate::GetInstance()) {
    ViewsDelegate::GetInstance()->set_native_widget_factory(
        ViewsDelegate::NativeWidgetFactory());
  }

  base::DiscardableMemoryAllocator::SetInstance(nullptr);
  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
  DCHECK(aura::Env::GetInstance());
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
  using WindowManager = ui::mojom::WindowManager;
  using TransportType = std::vector<uint8_t>;

  std::map<std::string, TransportType> properties = init_params.mus_properties;

  // Widget::InitParams::Type matches ui::mojom::WindowType.
  properties[WindowManager::kWindowType_InitProperty] =
      mojo::ConvertTo<TransportType>(static_cast<int32_t>(init_params.type));

  properties[WindowManager::kFocusable_InitProperty] =
      mojo::ConvertTo<TransportType>(init_params.CanActivate());

  if (!init_params.bounds.IsEmpty()) {
    properties[WindowManager::kBounds_InitProperty] =
        mojo::ConvertTo<TransportType>(init_params.bounds);
  }

  if (!init_params.name.empty()) {
    properties[WindowManager::kName_Property] =
        mojo::ConvertTo<TransportType>(init_params.name);
  }

  properties[WindowManager::kAlwaysOnTop_Property] =
      mojo::ConvertTo<TransportType>(
          static_cast<PrimitiveType>(init_params.keep_on_top));

  properties[WindowManager::kRemoveStandardFrame_InitProperty] =
      mojo::ConvertTo<TransportType>(init_params.remove_standard_frame);

  if (!Widget::RequiresNonClientView(init_params.type))
    return properties;

  if (init_params.delegate) {
    if (properties.count(WindowManager::kResizeBehavior_Property) == 0) {
      properties[WindowManager::kResizeBehavior_Property] =
          mojo::ConvertTo<TransportType>(static_cast<PrimitiveType>(
              init_params.delegate->GetResizeBehavior()));
    }

    // TODO(crbug.com/667566): Support additional scales or gfx::Image[Skia].
    gfx::ImageSkia app_icon = init_params.delegate->GetWindowAppIcon();
    SkBitmap app_bitmap = app_icon.GetRepresentation(1.f).sk_bitmap();
    if (!app_bitmap.isNull()) {
      properties[WindowManager::kAppIcon_Property] =
          mojo::ConvertTo<TransportType>(app_bitmap);
    }
    // TODO(crbug.com/667566): Support additional scales or gfx::Image[Skia].
    gfx::ImageSkia window_icon = init_params.delegate->GetWindowIcon();
    SkBitmap window_bitmap = window_icon.GetRepresentation(1.f).sk_bitmap();
    if (!window_bitmap.isNull()) {
      properties[WindowManager::kWindowIcon_Property] =
          mojo::ConvertTo<TransportType>(window_bitmap);
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

void MusClient::OnCaptureClientSet(
    aura::client::CaptureClient* capture_client) {
  pointer_watcher_event_router_->AttachToCaptureClient(capture_client);
  window_tree_client_->capture_synchronizer()->AttachToCaptureClient(
      capture_client);
}

void MusClient::OnCaptureClientUnset(
    aura::client::CaptureClient* capture_client) {
  pointer_watcher_event_router_->DetachFromCaptureClient(capture_client);
  window_tree_client_->capture_synchronizer()->DetachFromCaptureClient(
      capture_client);
}

void MusClient::AddObserver(MusClientObserver* observer) {
  observer_list_.AddObserver(observer);
}

void MusClient::RemoveObserver(MusClientObserver* observer) {
  observer_list_.RemoveObserver(observer);
}
void MusClient::SetMusPropertyMirror(
    std::unique_ptr<MusPropertyMirror> mirror) {
  mus_property_mirror_ = std::move(mirror);
}

void MusClient::OnEmbed(
    std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) {
  NOTREACHED();
}

void MusClient::OnLostConnection(aura::WindowTreeClient* client) {}

void MusClient::OnEmbedRootDestroyed(
    aura::WindowTreeHostMus* window_tree_host) {
  static_cast<DesktopWindowTreeHostMus*>(window_tree_host)
      ->ServerDestroyedWindow();
}

void MusClient::OnPointerEventObserved(const ui::PointerEvent& event,
                                       aura::Window* target) {
  pointer_watcher_event_router_->OnPointerEventObserved(event, target);
}

void MusClient::OnWindowManagerFrameValuesChanged() {
  for (auto& observer : observer_list_)
    observer.OnWindowManagerFrameValuesChanged();
}

aura::PropertyConverter* MusClient::GetPropertyConverter() {
  return property_converter_.get();
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
