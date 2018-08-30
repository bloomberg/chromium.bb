// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_window.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/fuchsia/fuchsia_logging.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/platform/scenic/scenic_window_manager.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

namespace {

const uint32_t kUsbHidKeyboardPage = 0x07;

int KeyModifiersToFlags(int modifiers) {
  int flags = 0;
  if (modifiers & fuchsia::ui::input::kModifierShift)
    flags |= EF_SHIFT_DOWN;
  if (modifiers & fuchsia::ui::input::kModifierControl)
    flags |= EF_CONTROL_DOWN;
  if (modifiers & fuchsia::ui::input::kModifierAlt)
    flags |= EF_ALT_DOWN;
  // TODO(crbug.com/850697): Add AltGraph support.
  return flags;
}

}  // namespace

ScenicWindow::ScenicWindow(
    ScenicWindowManager* window_manager,
    PlatformWindowDelegate* delegate,
    fidl::InterfaceRequest<fuchsia::ui::viewsv1token::ViewOwner>
        view_owner_request)
    : manager_(window_manager),
      delegate_(delegate),
      window_id_(manager_->AddWindow(this)),
      view_listener_binding_(this),
      scenic_session_(manager_->GetScenic(), this),
      input_listener_binding_(this) {
  // Create event pair to import parent view node to Scenic. One end is passed
  // directly to Scenic in ImportResource command and the second one is passed
  // to ViewManager::CreateView(). ViewManager will passes it to Scenic when the
  // view is added to a container.
  zx::eventpair parent_export_token;
  zx::eventpair parent_import_token;
  zx_status_t status =
      zx::eventpair::create(0u, &parent_import_token, &parent_export_token);
  ZX_CHECK(status == ZX_OK, status) << "zx_eventpair_create()";

  // Create a new node and add it as a child to the parent.
  parent_node_id_ = scenic_session_.ImportResource(
      fuchsia::ui::gfx::ImportSpec::NODE, std::move(parent_import_token));
  node_id_ = scenic_session_.CreateEntityNode();
  scenic_session_.AddNodeChild(parent_node_id_, node_id_);

  // Subscribe to metrics events from the parent node. These events are used to
  // get |device_pixel_ratio_| for the screen.
  scenic_session_.SetEventMask(parent_node_id_,
                               fuchsia::ui::gfx::kMetricsEventMask);

  // Create the view.
  manager_->GetViewManager()->CreateView(
      view_.NewRequest(), std::move(view_owner_request),
      view_listener_binding_.NewBinding(), std::move(parent_export_token),
      "Chromium");
  view_.set_error_handler(fit::bind_member(this, &ScenicWindow::OnViewError));
  view_listener_binding_.set_error_handler(
      fit::bind_member(this, &ScenicWindow::OnViewError));

  // Setup input event listener.
  fuchsia::sys::ServiceProviderPtr view_service_provider;
  view_->GetServiceProvider(view_service_provider.NewRequest());
  view_service_provider->ConnectToService(
      fuchsia::ui::input::InputConnection::Name_,
      input_connection_.NewRequest().TakeChannel());
  input_connection_->SetEventListener(input_listener_binding_.NewBinding());

  // Add shape node for window.
  shape_id_ = scenic_session_.CreateShapeNode();
  scenic_session_.AddNodeChild(node_id_, shape_id_);
  material_id_ = scenic_session_.CreateMaterial();
  scenic_session_.SetNodeMaterial(shape_id_, material_id_);

  // Call Present() to ensure that the scenic session commands are processed,
  // which is necessary to receive metrics event from Scenic.
  scenic_session_.Present();

  delegate_->OnAcceleratedWidgetAvailable(window_id_);
}

ScenicWindow::~ScenicWindow() {
  scenic_session_.ReleaseResource(node_id_);
  scenic_session_.ReleaseResource(parent_node_id_);
  scenic_session_.ReleaseResource(shape_id_);
  scenic_session_.ReleaseResource(material_id_);

  manager_->RemoveWindow(window_id_, this);
  view_.Unbind();
}

void ScenicWindow::SetTexture(ScenicSession::ResourceId texture) {
  scenic_session_.SetMaterialTexture(material_id_, texture);
}

gfx::Rect ScenicWindow::GetBounds() {
  return gfx::Rect(size_pixels_);
}

void ScenicWindow::SetBounds(const gfx::Rect& bounds) {
  // View dimensions are controlled by the containing view, it's not possible to
  // set them here.
}

void ScenicWindow::SetTitle(const base::string16& title) {
  NOTIMPLEMENTED();
}

void ScenicWindow::Show() {
  NOTIMPLEMENTED();
}

void ScenicWindow::Hide() {
  NOTIMPLEMENTED();
}

void ScenicWindow::Close() {
  NOTIMPLEMENTED();
}

void ScenicWindow::PrepareForShutdown() {
  NOTIMPLEMENTED();
}

void ScenicWindow::SetCapture() {
  NOTIMPLEMENTED();
}

void ScenicWindow::ReleaseCapture() {
  NOTIMPLEMENTED();
}

bool ScenicWindow::HasCapture() const {
  NOTIMPLEMENTED();
  return false;
}

void ScenicWindow::ToggleFullscreen() {
  NOTIMPLEMENTED();
}

void ScenicWindow::Maximize() {
  NOTIMPLEMENTED();
}

void ScenicWindow::Minimize() {
  NOTIMPLEMENTED();
}

void ScenicWindow::Restore() {
  NOTIMPLEMENTED();
}

PlatformWindowState ScenicWindow::GetPlatformWindowState() const {
  return PLATFORM_WINDOW_STATE_NORMAL;
}

void ScenicWindow::SetCursor(PlatformCursor cursor) {
  NOTIMPLEMENTED();
}

void ScenicWindow::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void ScenicWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

PlatformImeController* ScenicWindow::GetPlatformImeController() {
  NOTIMPLEMENTED();
  return nullptr;
}

void ScenicWindow::SetRestoredBoundsInPixels(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

gfx::Rect ScenicWindow::GetRestoredBoundsInPixels() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void ScenicWindow::UpdateSize() {
  gfx::SizeF scaled = ScaleSize(size_dips_, device_pixel_ratio_);
  size_pixels_ = gfx::Size(ceilf(scaled.width()), ceilf(scaled.height()));
  gfx::Rect size_rect(size_pixels_);

  // Update this window's Screen's dimensions to match the new size.
  ScenicScreen* screen = manager_->screen();
  if (screen)
    screen->OnWindowBoundsChanged(window_id_, size_rect);

  // Translate the node by half of the view dimensions to put it in the center
  // of the view.
  const float translation[] = {size_dips_.width() / 2.0,
                               size_dips_.height() / 2.0, 0.f};

  // Set node shape to rectangle that matches size of the view.
  ScenicSession::ResourceId rect_id =
      scenic_session_.CreateRectangle(size_dips_.width(), size_dips_.height());
  scenic_session_.SetNodeShape(shape_id_, rect_id);
  scenic_session_.SetNodeTranslation(shape_id_, translation);
  scenic_session_.ReleaseResource(rect_id);

  delegate_->OnBoundsChanged(size_rect);
}

void ScenicWindow::OnPropertiesChanged(
    fuchsia::ui::viewsv1::ViewProperties properties,
    OnPropertiesChangedCallback callback) {
  if (properties.view_layout) {
    size_dips_.SetSize(properties.view_layout->size.width,
                       properties.view_layout->size.height);
    if (device_pixel_ratio_ > 0.0)
      UpdateSize();
  }

  callback();
}

void ScenicWindow::OnScenicError(const std::string& error) {
  LOG(ERROR) << "ScenicSession failed: " << error;
  delegate_->OnClosed();
}

void ScenicWindow::OnScenicEvents(
    const std::vector<fuchsia::ui::scenic::Event>& events) {
  for (const auto& event : events) {
    if (!event.is_gfx() || !event.gfx().is_metrics())
      continue;

    auto& metrics = event.gfx().metrics();
    if (metrics.node_id != parent_node_id_)
      continue;

    device_pixel_ratio_ =
        std::max(metrics.metrics.scale_x, metrics.metrics.scale_y);

    ScenicScreen* screen = manager_->screen();
    if (screen)
      screen->OnWindowMetrics(window_id_, device_pixel_ratio_);

    if (!size_dips_.IsEmpty())
      UpdateSize();
  }
}

void ScenicWindow::OnEvent(fuchsia::ui::input::InputEvent event,
                           OnEventCallback callback) {
  bool result = false;

  switch (event.Which()) {
    case fuchsia::ui::input::InputEvent::Tag::kPointer:
      switch (event.pointer().type) {
        case fuchsia::ui::input::PointerEventType::MOUSE:
          result = OnMouseEvent(event.pointer());
          break;
        case fuchsia::ui::input::PointerEventType::TOUCH:
          result = OnTouchEvent(event.pointer());
          break;
        case fuchsia::ui::input::PointerEventType::STYLUS:
        case fuchsia::ui::input::PointerEventType::INVERTED_STYLUS:
          NOTIMPLEMENTED() << "Stylus input is not yet supported.";
          break;
      }
      break;

    case fuchsia::ui::input::InputEvent::Tag::kKeyboard:
      result = OnKeyboardEvent(event.keyboard());
      break;

    case fuchsia::ui::input::InputEvent::Tag::kFocus:
      result = OnFocusEvent(event.focus());
      break;

    case fuchsia::ui::input::InputEvent::Tag::Invalid:
      break;
  }

  callback(result);
}

void ScenicWindow::OnViewError() {
  VLOG(1) << "viewsv1::View connection was closed.";
  delegate_->OnClosed();
}

bool ScenicWindow::OnMouseEvent(const fuchsia::ui::input::PointerEvent& event) {
  int flags = 0;
  if (event.buttons & 1)
    flags |= EF_LEFT_MOUSE_BUTTON;
  if (event.buttons & 2)
    flags |= EF_RIGHT_MOUSE_BUTTON;
  if (event.buttons & 4)
    flags |= EF_MIDDLE_MOUSE_BUTTON;

  EventType event_type;

  switch (event.phase) {
    case fuchsia::ui::input::PointerEventPhase::DOWN:
      event_type = ET_MOUSE_PRESSED;
      break;
    case fuchsia::ui::input::PointerEventPhase::MOVE:
      event_type = flags ? ET_MOUSE_DRAGGED : ET_MOUSE_MOVED;
      break;
    case fuchsia::ui::input::PointerEventPhase::UP:
      event_type = ET_MOUSE_RELEASED;
      break;

    // Following phases are not expected for mouse events.
    case fuchsia::ui::input::PointerEventPhase::HOVER:
    case fuchsia::ui::input::PointerEventPhase::CANCEL:
    case fuchsia::ui::input::PointerEventPhase::ADD:
    case fuchsia::ui::input::PointerEventPhase::REMOVE:
      NOTREACHED() << "Unexpected mouse phase "
                   << fidl::ToUnderlying(event.phase);
      return false;
  }

  gfx::Point location =
      gfx::Point(event.x * device_pixel_ratio_, event.y * device_pixel_ratio_);
  ui::MouseEvent mouse_event(event_type, location, location,
                             base::TimeTicks::FromZxTime(event.event_time),
                             flags, 0);
  delegate_->DispatchEvent(&mouse_event);
  return true;
}

bool ScenicWindow::OnTouchEvent(const fuchsia::ui::input::PointerEvent& event) {
  EventType event_type;

  switch (event.phase) {
    case fuchsia::ui::input::PointerEventPhase::DOWN:
      event_type = ET_TOUCH_PRESSED;
      break;
    case fuchsia::ui::input::PointerEventPhase::MOVE:
      event_type = ET_TOUCH_MOVED;
      break;
    case fuchsia::ui::input::PointerEventPhase::CANCEL:
      event_type = ET_TOUCH_CANCELLED;
      break;
    case fuchsia::ui::input::PointerEventPhase::UP:
      event_type = ET_TOUCH_RELEASED;
      break;
    case fuchsia::ui::input::PointerEventPhase::ADD:
    case fuchsia::ui::input::PointerEventPhase::REMOVE:
    case fuchsia::ui::input::PointerEventPhase::HOVER:
      return false;
  }

  // TODO(crbug.com/876933): Add more detailed fields such as
  // force/orientation/tilt once they are added to PointerEvent.
  ui::PointerDetails pointer_details(ui::EventPointerType::POINTER_TYPE_TOUCH,
                                     event.pointer_id);

  gfx::Point location =
      gfx::Point(event.x * device_pixel_ratio_, event.y * device_pixel_ratio_);
  ui::TouchEvent touch_event(event_type, location,
                             base::TimeTicks::FromZxTime(event.event_time),
                             pointer_details);

  delegate_->DispatchEvent(&touch_event);
  return true;
}

bool ScenicWindow::OnKeyboardEvent(
    const fuchsia::ui::input::KeyboardEvent& event) {
  EventType event_type;

  switch (event.phase) {
    case fuchsia::ui::input::KeyboardEventPhase::PRESSED:
    case fuchsia::ui::input::KeyboardEventPhase::REPEAT:
      event_type = ET_KEY_PRESSED;
      break;

    case fuchsia::ui::input::KeyboardEventPhase::RELEASED:
      event_type = ET_KEY_RELEASED;
      break;

    case fuchsia::ui::input::KeyboardEventPhase::CANCELLED:
      NOTIMPLEMENTED() << "Key event cancellation is not supported.";
      event_type = ET_KEY_RELEASED;
      break;
  }

  // Currently KeyboardEvent doesn't specify HID Usage page. |hid_usage|
  // field always contains values from the Keyboard page. See
  // https://fuchsia.atlassian.net/browse/SCN-762 .
  DomCode dom_code = KeycodeConverter::UsbKeycodeToDomCode(
      (kUsbHidKeyboardPage << 16) | event.hid_usage);
  DomKey dom_key;
  KeyboardCode key_code;
  if (!DomCodeToUsLayoutDomKey(dom_code, KeyModifiersToFlags(event.modifiers),
                               &dom_key, &key_code)) {
    LOG(ERROR) << "DomCodeToUsLayoutDomKey() failed for usb_key: "
               << event.hid_usage;
    key_code = VKEY_UNKNOWN;
  }

  if (event.code_point)
    dom_key = DomKey::FromCharacter(event.code_point);

  KeyEvent key_event(event_type, key_code, dom_code,
                     KeyModifiersToFlags(event.modifiers), dom_key,
                     base::TimeTicks::FromZxTime(event.event_time));
  delegate_->DispatchEvent(&key_event);
  return true;
}

bool ScenicWindow::OnFocusEvent(const fuchsia::ui::input::FocusEvent& event) {
  delegate_->OnActivationChanged(event.focused);
  return true;
}

}  // namespace ui
