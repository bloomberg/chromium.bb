// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/window_manager_connection.h"

#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/network/network_type_converters.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/mojo/init/ui_init.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/mus/window_manager_client_area_insets.h"
#include "ui/views/views_delegate.h"

namespace mojo {

gfx::Display::Rotation GFXRotationFromMojomRotation(
    mus::mojom::Rotation input) {
  switch (input) {
    case mus::mojom::ROTATION_VALUE_0:
      return gfx::Display::ROTATE_0;
    case mus::mojom::ROTATION_VALUE_90:
      return gfx::Display::ROTATE_90;
    case mus::mojom::ROTATION_VALUE_180:
      return gfx::Display::ROTATE_180;
    case mus::mojom::ROTATION_VALUE_270:
      return gfx::Display::ROTATE_270;
  }
  return gfx::Display::ROTATE_0;
}

template <>
struct TypeConverter<gfx::Display, mus::mojom::DisplayPtr> {
  static gfx::Display Convert(const mus::mojom::DisplayPtr& input) {
    gfx::Display result;
    result.set_id(input->id);
    result.SetScaleAndBounds(input->device_pixel_ratio,
                             input->bounds.To<gfx::Rect>());
    gfx::Rect work_area(
        gfx::ScaleToFlooredPoint(
            gfx::Point(input->work_area->x, input->work_area->y),
            1.0f / input->device_pixel_ratio),
        gfx::ScaleToFlooredSize(
            gfx::Size(input->work_area->width, input->work_area->height),
            1.0f / input->device_pixel_ratio));
    result.set_work_area(work_area);
    result.set_rotation(GFXRotationFromMojomRotation(input->rotation));
    return result;
  }
};

}  // namespace mojo

namespace views {

namespace {

using WindowManagerConnectionPtr =
    base::ThreadLocalPointer<views::WindowManagerConnection>;

// Env is thread local so that aura may be used on multiple threads.
base::LazyInstance<WindowManagerConnectionPtr>::Leaky lazy_tls_ptr =
    LAZY_INSTANCE_INITIALIZER;

std::vector<gfx::Display> GetDisplaysFromWindowManager(
    mus::mojom::WindowManagerPtr* window_manager) {
  WindowManagerClientAreaInsets client_insets;
  std::vector<gfx::Display> displays;
  (*window_manager)
      ->GetConfig([&displays,
                   &client_insets](mus::mojom::WindowManagerConfigPtr results) {
        displays = results->displays.To<std::vector<gfx::Display>>();
        client_insets.normal_insets =
            results->normal_client_area_insets.To<gfx::Insets>();
        client_insets.maximized_insets =
            results->maximized_client_area_insets.To<gfx::Insets>();
      });
  CHECK(window_manager->WaitForIncomingResponse());
  NativeWidgetMus::SetWindowManagerClientAreaInsets(client_insets);
  return displays;
}

}  // namespace

// static
void WindowManagerConnection::Create(
    mus::mojom::WindowManagerPtr window_manager,
    mojo::ApplicationImpl* app) {
  DCHECK(!lazy_tls_ptr.Pointer()->Get());
  lazy_tls_ptr.Pointer()->Set(
      new WindowManagerConnection(window_manager.Pass(), app));
}

// static
WindowManagerConnection* WindowManagerConnection::Get() {
  WindowManagerConnection* connection = lazy_tls_ptr.Pointer()->Get();
  DCHECK(connection);
  return connection;
}

mus::Window* WindowManagerConnection::NewWindow(
    const std::map<std::string, std::vector<uint8_t>>& properties) {
  mus::mojom::WindowTreeClientPtr window_tree_client;
  mojo::InterfaceRequest<mus::mojom::WindowTreeClient>
      window_tree_client_request = GetProxy(&window_tree_client);
  window_manager_->OpenWindow(
      window_tree_client.Pass(),
      mojo::Map<mojo::String, mojo::Array<uint8_t>>::From(properties));
  mus::WindowTreeConnection* window_tree_connection =
      mus::WindowTreeConnection::Create(
          this, window_tree_client_request.Pass(),
          mus::WindowTreeConnection::CreateType::WAIT_FOR_EMBED);
  DCHECK(window_tree_connection->GetRoot());
  return window_tree_connection->GetRoot();
}

WindowManagerConnection::WindowManagerConnection(
    mus::mojom::WindowManagerPtr window_manager,
    mojo::ApplicationImpl* app)
    : app_(app), window_manager_(window_manager.Pass()) {
  ui_init_.reset(new ui::mojo::UIInit(
      GetDisplaysFromWindowManager(&window_manager_)));
  ViewsDelegate::GetInstance()->set_native_widget_factory(
      base::Bind(&WindowManagerConnection::CreateNativeWidget,
                 base::Unretained(this)));
}

WindowManagerConnection::~WindowManagerConnection() {}

void WindowManagerConnection::OnEmbed(mus::Window* root) {}
void WindowManagerConnection::OnConnectionLost(
    mus::WindowTreeConnection* connection) {}

NativeWidget* WindowManagerConnection::CreateNativeWidget(
    const Widget::InitParams& init_params,
    internal::NativeWidgetDelegate* delegate) {
  std::map<std::string, std::vector<uint8_t>> properties;
  NativeWidgetMus::ConfigurePropertiesForNewWindow(init_params, &properties);
  return new NativeWidgetMus(delegate, app_->shell(), NewWindow(properties),
                             mus::mojom::SURFACE_TYPE_DEFAULT);
}

}  // namespace views
