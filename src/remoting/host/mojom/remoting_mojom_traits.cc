// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mojom/remoting_mojom_traits.h"

namespace mojo {

// static
bool mojo::StructTraits<remoting::mojom::ClipboardEventDataView,
                        ::remoting::protocol::ClipboardEvent>::
    Read(remoting::mojom::ClipboardEventDataView data_view,
         ::remoting::protocol::ClipboardEvent* out_event) {
  std::string mime_type;
  if (!data_view.ReadMimeType(&mime_type)) {
    return false;
  }
  out_event->set_mime_type(std::move(mime_type));
  std::string data;
  if (!data_view.ReadData(&data)) {
    return false;
  }
  out_event->set_data(std::move(data));
  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::DesktopCaptureOptionsDataView,
                        ::webrtc::DesktopCaptureOptions>::
    Read(remoting::mojom::DesktopCaptureOptionsDataView data_view,
         ::webrtc::DesktopCaptureOptions* out_options) {
  out_options->set_use_update_notifications(
      data_view.use_update_notifications());
  out_options->set_detect_updated_region(data_view.detect_updated_region());

#if BUILDFLAG(IS_WIN)
  out_options->set_allow_directx_capturer(data_view.allow_directx_capturer());
#endif  // BUILDFLAG(IS_WIN)

  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::DesktopEnvironmentOptionsDataView,
                        ::remoting::DesktopEnvironmentOptions>::
    Read(remoting::mojom::DesktopEnvironmentOptionsDataView data_view,
         ::remoting::DesktopEnvironmentOptions* out_options) {
  out_options->set_enable_curtaining(data_view.enable_curtaining());
  out_options->set_enable_user_interface(data_view.enable_user_interface());
  out_options->set_enable_notifications(data_view.enable_notifications());
  out_options->set_terminate_upon_input(data_view.terminate_upon_input());
  out_options->set_enable_file_transfer(data_view.enable_file_transfer());
  out_options->set_enable_remote_open_url(data_view.enable_remote_open_url());
  out_options->set_enable_remote_webauthn(data_view.enable_remote_webauthn());

  absl::optional<uint32_t> clipboard_size;
  if (!data_view.ReadClipboardSize(&clipboard_size)) {
    return false;
  }
  if (clipboard_size.has_value()) {
    out_options->set_clipboard_size(std::move(clipboard_size));
  }

  if (!data_view.ReadDesktopCaptureOptions(
          out_options->desktop_capture_options())) {
    return false;
  }

  return true;
}

// static
bool mojo::StructTraits<
    remoting::mojom::DesktopSizeDataView,
    ::webrtc::DesktopSize>::Read(remoting::mojom::DesktopSizeDataView data_view,
                                 ::webrtc::DesktopSize* out_size) {
  out_size->set(data_view.width(), data_view.height());
  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::DesktopVectorDataView,
                        ::webrtc::DesktopVector>::
    Read(remoting::mojom::DesktopVectorDataView data_view,
         ::webrtc::DesktopVector* out_vector) {
  out_vector->set(data_view.x(), data_view.y());
  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::KeyEventDataView,
                        ::remoting::protocol::KeyEvent>::
    Read(remoting::mojom::KeyEventDataView data_view,
         ::remoting::protocol::KeyEvent* out_event) {
  out_event->set_pressed(data_view.pressed());
  out_event->set_usb_keycode(data_view.usb_keycode());
  out_event->set_lock_states(data_view.lock_states());

  absl::optional<bool> caps_lock_state;
  if (!data_view.ReadCapsLockState(&caps_lock_state)) {
    return false;
  }
  if (caps_lock_state.has_value()) {
    out_event->set_caps_lock_state(caps_lock_state.value());
  }

  absl::optional<bool> num_lock_state;
  if (!data_view.ReadNumLockState(&num_lock_state)) {
    return false;
  }
  if (num_lock_state.has_value()) {
    out_event->set_num_lock_state(num_lock_state.value());
  }

  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::MouseEventDataView,
                        ::remoting::protocol::MouseEvent>::
    Read(remoting::mojom::MouseEventDataView data_view,
         ::remoting::protocol::MouseEvent* out_event) {
  absl::optional<int32_t> x;
  if (!data_view.ReadX(&x)) {
    return false;
  }
  if (x.has_value()) {
    out_event->set_x(x.value());
  }

  absl::optional<int32_t> y;
  if (!data_view.ReadY(&y)) {
    return false;
  }
  if (y.has_value()) {
    out_event->set_y(y.value());
  }

  if (data_view.button() != remoting::mojom::MouseButton::kUndefined) {
    ::remoting::protocol::MouseEvent::MouseButton mouse_button;
    if (!data_view.ReadButton(&mouse_button)) {
      return false;
    }
    out_event->set_button(mouse_button);
  }

  absl::optional<bool> button_down;
  if (!data_view.ReadButtonDown(&button_down)) {
    return false;
  }
  if (button_down.has_value()) {
    out_event->set_button_down(button_down.value());
  }

  absl::optional<float> wheel_delta_x;
  if (!data_view.ReadWheelDeltaX(&wheel_delta_x)) {
    return false;
  }
  if (wheel_delta_x.has_value()) {
    out_event->set_wheel_delta_x(wheel_delta_x.value());
  }

  absl::optional<float> wheel_delta_y;
  if (!data_view.ReadWheelDeltaY(&wheel_delta_y)) {
    return false;
  }
  if (wheel_delta_y.has_value()) {
    out_event->set_wheel_delta_y(wheel_delta_y.value());
  }

  absl::optional<float> wheel_ticks_x;
  if (!data_view.ReadWheelTicksX(&wheel_ticks_x)) {
    return false;
  }
  if (wheel_ticks_x.has_value()) {
    out_event->set_wheel_ticks_x(wheel_ticks_x.value());
  }

  absl::optional<float> wheel_ticks_y;
  if (!data_view.ReadWheelTicksY(&wheel_ticks_y)) {
    return false;
  }
  if (wheel_ticks_y.has_value()) {
    out_event->set_wheel_ticks_y(wheel_ticks_y.value());
  }

  absl::optional<int32_t> delta_x;
  if (!data_view.ReadDeltaX(&delta_x)) {
    return false;
  }
  if (delta_x.has_value()) {
    out_event->set_delta_x(delta_x.value());
  }

  absl::optional<int32_t> delta_y;
  if (!data_view.ReadDeltaY(&delta_y)) {
    return false;
  }
  if (delta_y.has_value()) {
    out_event->set_delta_y(delta_y.value());
  }

  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::ScreenResolutionDataView,
                        ::remoting::ScreenResolution>::
    Read(remoting::mojom::ScreenResolutionDataView data_view,
         ::remoting::ScreenResolution* out_resolution) {
  ::webrtc::DesktopSize desktop_size;
  if (!data_view.ReadDimensions(&desktop_size)) {
    return false;
  }

  ::webrtc::DesktopVector dpi;
  if (!data_view.ReadDpi(&dpi)) {
    return false;
  }

  *out_resolution =
      ::remoting::ScreenResolution(std::move(desktop_size), std::move(dpi));

  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::TextEventDataView,
                        ::remoting::protocol::TextEvent>::
    Read(remoting::mojom::TextEventDataView data_view,
         ::remoting::protocol::TextEvent* out_event) {
  std::string text;
  if (!data_view.ReadText(&text)) {
    return false;
  }
  out_event->set_text(std::move(text));
  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::TouchEventPointDataView,
                        ::remoting::protocol::TouchEventPoint>::
    Read(remoting::mojom::TouchEventPointDataView data_view,
         ::remoting::protocol::TouchEventPoint* out_event) {
  out_event->set_id(data_view.id());
  gfx::PointF position;
  if (!data_view.ReadPosition(&position)) {
    return false;
  }
  out_event->set_x(position.x());
  out_event->set_y(position.y());
  gfx::PointF radius;
  if (!data_view.ReadRadius(&radius)) {
    return false;
  }
  out_event->set_radius_x(radius.x());
  out_event->set_radius_y(radius.y());
  out_event->set_angle(data_view.angle());
  out_event->set_pressure(data_view.pressure());
  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::TouchEventDataView,
                        ::remoting::protocol::TouchEvent>::
    Read(remoting::mojom::TouchEventDataView data_view,
         ::remoting::protocol::TouchEvent* out_event) {
  ::remoting::protocol::TouchEvent::TouchEventType touch_event_type;
  if (!data_view.ReadEventType(&touch_event_type)) {
    return false;
  }
  out_event->set_event_type(touch_event_type);

  if (!data_view.ReadTouchPoints(out_event->mutable_touch_points())) {
    return false;
  }

  return true;
}

// static
bool mojo::StructTraits<remoting::mojom::TransportRouteDataView,
                        ::remoting::protocol::TransportRoute>::
    Read(remoting::mojom::TransportRouteDataView data_view,
         ::remoting::protocol::TransportRoute* out_transport_route) {
  if (!data_view.ReadType(&out_transport_route->type)) {
    return false;
  }

  if (!data_view.ReadRemoteAddress(&out_transport_route->remote_address)) {
    return false;
  }

  if (!data_view.ReadLocalAddress(&out_transport_route->local_address)) {
    return false;
  }

  return true;
}

}  // namespace mojo
