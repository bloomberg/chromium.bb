// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/window_manager_connection.h"

#include <utility>

#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/network/network_type_converters.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/mojo/init/ui_init.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/mus/window_manager_frame_values.h"
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
  WindowManagerFrameValues frame_values;
  std::vector<gfx::Display> displays;
  (*window_manager)
      ->GetConfig([&displays,
                   &frame_values](mus::mojom::WindowManagerConfigPtr results) {
        displays = results->displays.To<std::vector<gfx::Display>>();
        frame_values.normal_insets =
            results->normal_client_area_insets.To<gfx::Insets>();
        frame_values.maximized_insets =
            results->maximized_client_area_insets.To<gfx::Insets>();
        frame_values.max_title_bar_button_width =
            results->max_title_bar_button_width;
      });
  CHECK(window_manager->WaitForIncomingResponse());
  WindowManagerFrameValues::SetInstance(frame_values);
  return displays;
}

}  // namespace

// static
void WindowManagerConnection::Create(mojo::ApplicationImpl* app) {
  DCHECK(!lazy_tls_ptr.Pointer()->Get());
  lazy_tls_ptr.Pointer()->Set(new WindowManagerConnection(app));
}

// static
WindowManagerConnection* WindowManagerConnection::Get() {
  WindowManagerConnection* connection = lazy_tls_ptr.Pointer()->Get();
  DCHECK(connection);
  return connection;
}

mus::Window* WindowManagerConnection::NewWindow(
    const std::map<std::string, std::vector<uint8_t>>& properties) {
  if (window_tree_connection_)
    return window_tree_connection_->NewTopLevelWindow(&properties);

  mus::mojom::WindowTreeClientPtr window_tree_client;
  mojo::InterfaceRequest<mus::mojom::WindowTreeClient>
      window_tree_client_request = GetProxy(&window_tree_client);
  window_manager_->OpenWindow(
      std::move(window_tree_client),
      mojo::Map<mojo::String, mojo::Array<uint8_t>>::From(properties));

  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  window_tree_connection_.reset(mus::WindowTreeConnection::Create(
      this, std::move(window_tree_client_request),
      mus::WindowTreeConnection::CreateType::WAIT_FOR_EMBED));
  window_tree_connection_->SetDeleteOnNoRoots(false);
  DCHECK_EQ(1u, window_tree_connection_->GetRoots().size());
  return *window_tree_connection_->GetRoots().begin();
}

WindowManagerConnection::WindowManagerConnection(mojo::ApplicationImpl* app)
    : app_(app), window_tree_connection_(nullptr) {
  app->ConnectToService("mojo:mus", &window_manager_);

  ui_init_.reset(new ui::mojo::UIInit(
      GetDisplaysFromWindowManager(&window_manager_)));
  ViewsDelegate::GetInstance()->set_native_widget_factory(
      base::Bind(&WindowManagerConnection::CreateNativeWidget,
                 base::Unretained(this)));
}

WindowManagerConnection::~WindowManagerConnection() {
  // ~WindowTreeConnection calls back to us (we're the WindowTreeDelegate),
  // destroy it while we are still valid.
  window_tree_connection_.reset();
}

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
