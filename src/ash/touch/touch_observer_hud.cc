// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_observer_hud.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/shell.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/transform.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

const int kPointRadius = 20;
const SkColor kColors[] = {
    SK_ColorYELLOW,
    SK_ColorGREEN,
    SK_ColorRED,
    SK_ColorBLUE,
    SK_ColorGRAY,
    SK_ColorMAGENTA,
    SK_ColorCYAN,
    SK_ColorWHITE,
    SK_ColorBLACK,
    SkColorSetRGB(0xFF, 0x8C, 0x00),
    SkColorSetRGB(0x8B, 0x45, 0x13),
    SkColorSetRGB(0xFF, 0xDE, 0xAD),
};
const int kAlpha = 0x60;
const int kMaxPaths = base::size(kColors);
const int kReducedScale = 10;

const char* GetTouchEventLabel(ui::EventType type) {
  switch (type) {
    case ui::ET_UNKNOWN:
      return " ";
    case ui::ET_TOUCH_PRESSED:
      return "P";
    case ui::ET_TOUCH_MOVED:
      return "M";
    case ui::ET_TOUCH_RELEASED:
      return "R";
    case ui::ET_TOUCH_CANCELLED:
      return "C";
    default:
      break;
  }
  return "?";
}

int GetTrackingId(const ui::TouchEvent& event) {
  return 0;
}

// A TouchPointLog represents a single touch-event of a touch point.
struct TouchPointLog {
 public:
  explicit TouchPointLog(const ui::TouchEvent& touch)
      : id(touch.pointer_details().id),
        type(touch.type()),
        location(touch.root_location()),
        timestamp((touch.time_stamp() - base::TimeTicks()).InMillisecondsF()),
        radius_x(touch.pointer_details().radius_x),
        radius_y(touch.pointer_details().radius_y),
        pressure(touch.pointer_details().force),
        tracking_id(GetTrackingId(touch)),
        source_device(touch.source_device_id()) {}

  // Populates a dictionary value with all the information about the touch
  // point.
  std::unique_ptr<base::DictionaryValue> GetAsDictionary() const {
    std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());

    value->SetInteger("id", id);
    value->SetString("type", std::string(GetTouchEventLabel(type)));
    value->SetString("location", location.ToString());
    value->SetDouble("timestamp", timestamp);
    value->SetDouble("radius_x", radius_x);
    value->SetDouble("radius_y", radius_y);
    value->SetDouble("pressure", pressure);
    value->SetInteger("tracking_id", tracking_id);
    value->SetInteger("source_device", source_device);

    return value;
  }

  int id;
  ui::EventType type;
  gfx::Point location;
  double timestamp;
  float radius_x;
  float radius_y;
  float pressure;
  int tracking_id;
  int source_device;
};

// A TouchTrace keeps track of all the touch events of a single touch point
// (starting from a touch-press and ending at a touch-release or touch-cancel).
class TouchTrace {
 public:
  using iterator = std::vector<TouchPointLog>::iterator;
  using const_iterator = std::vector<TouchPointLog>::const_iterator;
  using reverse_iterator = std::vector<TouchPointLog>::reverse_iterator;
  using const_reverse_iterator =
      std::vector<TouchPointLog>::const_reverse_iterator;

  TouchTrace() = default;

  void AddTouchPoint(const ui::TouchEvent& touch) {
    log_.push_back(TouchPointLog(touch));
  }

  const std::vector<TouchPointLog>& log() const { return log_; }

  bool active() const {
    return !log_.empty() && log_.back().type != ui::ET_TOUCH_RELEASED &&
           log_.back().type != ui::ET_TOUCH_CANCELLED;
  }

  // Returns a list containing data from all events for the touch point.
  std::unique_ptr<base::ListValue> GetAsList() const {
    std::unique_ptr<base::ListValue> list(new base::ListValue());
    for (const_iterator i = log_.begin(); i != log_.end(); ++i)
      list->Append((*i).GetAsDictionary());
    return list;
  }

  void Reset() { log_.clear(); }

 private:
  std::vector<TouchPointLog> log_;

  DISALLOW_COPY_AND_ASSIGN(TouchTrace);
};

}  // namespace

// A TouchLog keeps track of all touch events of all touch points.
class TouchLog {
 public:
  TouchLog() = default;

  void AddTouchPoint(const ui::TouchEvent& touch) {
    if (touch.type() == ui::ET_TOUCH_PRESSED)
      StartTrace(touch);
    AddToTrace(touch);
  }

  void Reset() {
    next_trace_index_ = 0;
    for (int i = 0; i < kMaxPaths; ++i)
      traces_[i].Reset();
  }

  std::unique_ptr<base::ListValue> GetAsList() const {
    std::unique_ptr<base::ListValue> list(new base::ListValue());
    for (int i = 0; i < kMaxPaths; ++i) {
      if (!traces_[i].log().empty())
        list->Append(traces_[i].GetAsList());
    }
    return list;
  }

  int GetTraceIndex(int touch_id) const {
    return touch_id_to_trace_index_.at(touch_id);
  }

  const TouchTrace* traces() const { return traces_; }

 private:
  void StartTrace(const ui::TouchEvent& touch) {
    // Find the first inactive spot; otherwise, overwrite the one
    // |next_trace_index_| is pointing to.
    int old_trace_index = next_trace_index_;
    do {
      if (!traces_[next_trace_index_].active())
        break;
      next_trace_index_ = (next_trace_index_ + 1) % kMaxPaths;
    } while (next_trace_index_ != old_trace_index);
    int touch_id = touch.pointer_details().id;
    traces_[next_trace_index_].Reset();
    touch_id_to_trace_index_[touch_id] = next_trace_index_;
    next_trace_index_ = (next_trace_index_ + 1) % kMaxPaths;
  }

  void AddToTrace(const ui::TouchEvent& touch) {
    int touch_id = touch.pointer_details().id;
    int trace_index = touch_id_to_trace_index_[touch_id];
    traces_[trace_index].AddTouchPoint(touch);
  }

  TouchTrace traces_[kMaxPaths];
  int next_trace_index_ = 0;

  std::map<int, int> touch_id_to_trace_index_;

  DISALLOW_COPY_AND_ASSIGN(TouchLog);
};

// TouchHudCanvas draws touch traces in |FULLSCREEN| and |REDUCED_SCALE| modes.
class TouchHudCanvas : public views::View {
 public:
  explicit TouchHudCanvas(const TouchLog& touch_log)
      : touch_log_(touch_log), scale_(1) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);

    flags_.setStyle(cc::PaintFlags::kFill_Style);
  }

  ~TouchHudCanvas() override = default;

  void SetScale(int scale) {
    if (scale_ == scale)
      return;
    scale_ = scale;
    gfx::Transform transform;
    transform.Scale(1. / scale_, 1. / scale_);
    layer()->SetTransform(transform);
  }

  int scale() const { return scale_; }

  void TouchPointAdded(int touch_id) {
    int trace_index = touch_log_.GetTraceIndex(touch_id);
    const TouchTrace& trace = touch_log_.traces()[trace_index];
    const TouchPointLog& point = trace.log().back();
    if (point.type == ui::ET_TOUCH_PRESSED)
      StartedTrace(trace_index);
    if (point.type != ui::ET_TOUCH_CANCELLED)
      AddedPointToTrace(trace_index);
  }

  void Clear() {
    for (int i = 0; i < kMaxPaths; ++i)
      paths_[i].reset();

    SchedulePaint();
  }

 private:
  void StartedTrace(int trace_index) {
    paths_[trace_index].reset();
    colors_[trace_index] = SkColorSetA(kColors[trace_index], kAlpha);
  }

  void AddedPointToTrace(int trace_index) {
    const TouchTrace& trace = touch_log_.traces()[trace_index];
    const TouchPointLog& point = trace.log().back();
    const gfx::Point& location = point.location;
    SkScalar x = SkIntToScalar(location.x());
    SkScalar y = SkIntToScalar(location.y());
    SkPoint last;
    if (!paths_[trace_index].getLastPt(&last) || x != last.x() ||
        y != last.y()) {
      paths_[trace_index].addCircle(x, y, SkIntToScalar(kPointRadius));
      SchedulePaint();
    }
  }

  // Overridden from views::View.
  void OnPaint(gfx::Canvas* canvas) override {
    for (int i = 0; i < kMaxPaths; ++i) {
      if (paths_[i].countPoints() == 0)
        continue;
      flags_.setColor(colors_[i]);
      canvas->DrawPath(paths_[i], flags_);
    }
  }

  cc::PaintFlags flags_;

  const TouchLog& touch_log_;
  SkPath paths_[kMaxPaths];
  SkColor colors_[kMaxPaths];

  int scale_;

  DISALLOW_COPY_AND_ASSIGN(TouchHudCanvas);
};

TouchObserverHUD::TouchObserverHUD(aura::Window* initial_root)
    : display_id_(GetRootWindowSettings(initial_root)->display_id),
      root_window_(initial_root),
      widget_(nullptr),
      mode_(FULLSCREEN),
      touch_log_(std::make_unique<TouchLog>()),
      canvas_(nullptr),
      label_container_(nullptr) {
  DCHECK(root_window_);
  const display::Display& display =
      Shell::Get()->display_manager()->GetDisplayForId(display_id_);

  views::View* content = new views::View;

  const gfx::Size& display_size = display.size();
  content->SetSize(display_size);

  widget_ = new views::Widget();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.accept_events = false;
  params.bounds = display.bounds();
  params.parent =
      Shell::GetContainer(root_window_, kShellWindowId_OverlayContainer);
  params.name = "TouchObserverHUD";
  widget_->Init(params);
  widget_->SetContentsView(content);
  widget_->StackAtTop();
  widget_->Show();

  widget_->AddObserver(this);

  // Observe changes in display size and mode to update touch HUD.
  display::Screen::GetScreen()->AddObserver(this);
  Shell::Get()->display_configurator()->AddObserver(this);
  Shell::Get()->window_tree_host_manager()->AddObserver(this);
  root_window_->AddPreTargetHandler(this);

  canvas_ = new TouchHudCanvas(*touch_log_);  // Owned by views hierarchy.
  content->AddChildView(canvas_);
  canvas_->SetSize(display_size);

  label_container_ = new views::View;
  label_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));

  for (int i = 0; i < kMaxTouchPoints; ++i) {
    touch_labels_[i] = new views::Label;
    touch_labels_[i]->SetBackgroundColor(SkColorSetARGB(0, 255, 255, 255));
    touch_labels_[i]->SetShadows(gfx::ShadowValues(
        1, gfx::ShadowValue(gfx::Vector2d(1, 1), 0, SK_ColorWHITE)));
    label_container_->AddChildView(touch_labels_[i]);
  }
  label_container_->SetX(0);
  label_container_->SetY(display_size.height() / kReducedScale);
  label_container_->SetSize(label_container_->GetPreferredSize());
  label_container_->SetVisible(false);
  content->AddChildView(label_container_);
}

TouchObserverHUD::~TouchObserverHUD() {
  Shell::Get()->window_tree_host_manager()->RemoveObserver(this);
  Shell::Get()->display_configurator()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);

  widget_->RemoveObserver(this);
}

void TouchObserverHUD::Clear() {
  if (widget_->IsVisible()) {
    canvas_->Clear();
    for (int i = 0; i < kMaxTouchPoints; ++i)
      touch_labels_[i]->SetText(base::string16());
    label_container_->SetSize(label_container_->GetPreferredSize());
  }
}

void TouchObserverHUD::Remove() {
  root_window_->RemovePreTargetHandler(this);

  RootWindowController* controller =
      RootWindowController::ForWindow(root_window_);
  controller->set_touch_observer_hud(nullptr);

  widget_->CloseNow();
}

// static
std::unique_ptr<base::DictionaryValue> TouchObserverHUD::GetAllAsDictionary() {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  aura::Window::Windows roots = Shell::Get()->GetAllRootWindows();
  for (RootWindowController* root : Shell::GetAllRootWindowControllers()) {
    TouchObserverHUD* hud = root->touch_observer_hud();
    if (hud) {
      std::unique_ptr<base::ListValue> list = hud->GetLogAsList();
      if (!list->empty())
        value->Set(base::Int64ToString(hud->display_id_), std::move(list));
    }
  }
  return value;
}

void TouchObserverHUD::ChangeToNextMode() {
  switch (mode_) {
    case FULLSCREEN:
      SetMode(REDUCED_SCALE);
      break;
    case REDUCED_SCALE:
      SetMode(INVISIBLE);
      break;
    case INVISIBLE:
      SetMode(FULLSCREEN);
      break;
  }
}

std::unique_ptr<base::ListValue> TouchObserverHUD::GetLogAsList() const {
  return touch_log_->GetAsList();
}

void TouchObserverHUD::OnTouchEvent(ui::TouchEvent* event) {
  if (event->pointer_details().id >= kMaxTouchPoints)
    return;

  touch_log_->AddTouchPoint(*event);
  canvas_->TouchPointAdded(event->pointer_details().id);
  UpdateTouchPointLabel(event->pointer_details().id);
  label_container_->SetSize(label_container_->GetPreferredSize());
}

void TouchObserverHUD::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget, widget_);
  delete this;
}

void TouchObserverHUD::OnDisplayAdded(const display::Display& new_display) {}

void TouchObserverHUD::OnDisplayRemoved(const display::Display& old_display) {
  if (old_display.id() != display_id_)
    return;
  widget_->CloseNow();
}

void TouchObserverHUD::OnDisplayMetricsChanged(const display::Display& display,
                                               uint32_t metrics) {
  if (display.id() != display_id_ || !(metrics & DISPLAY_METRIC_BOUNDS))
    return;

  widget_->SetSize(display.size());
  const gfx::Size& size = display.size();
  canvas_->SetSize(size);
  label_container_->SetY(size.height() / kReducedScale);
}

void TouchObserverHUD::OnDisplayModeChanged(
    const display::DisplayConfigurator::DisplayStateList& outputs) {
  // Clear touch HUD for any change in display mode (single, dual extended, dual
  // mirrored, ...).
  Clear();
}

void TouchObserverHUD::OnDisplaysInitialized() {
  OnDisplayConfigurationChanged();
}

void TouchObserverHUD::OnDisplayConfigurationChanging() {
  if (!root_window_)
    return;

  root_window_->RemovePreTargetHandler(this);

  RootWindowController::ForWindow(root_window_)
      ->set_touch_observer_hud(nullptr);

  views::Widget::ReparentNativeView(
      widget_->GetNativeView(),
      Shell::GetContainer(root_window_,
                          kShellWindowId_UnparentedControlContainer));

  root_window_ = NULL;
}

void TouchObserverHUD::OnDisplayConfigurationChanged() {
  if (root_window_)
    return;

  root_window_ = Shell::GetRootWindowForDisplayId(display_id_);

  views::Widget::ReparentNativeView(
      widget_->GetNativeView(),
      Shell::GetContainer(root_window_, kShellWindowId_OverlayContainer));

  RootWindowController::ForWindow(root_window_)->set_touch_observer_hud(this);

  root_window_->AddPreTargetHandler(this);
}

void TouchObserverHUD::SetMode(Mode mode) {
  if (mode_ == mode)
    return;
  mode_ = mode;
  switch (mode) {
    case FULLSCREEN:
      label_container_->SetVisible(false);
      canvas_->SetVisible(true);
      canvas_->SetScale(1);
      canvas_->SchedulePaint();
      widget_->Show();
      break;
    case REDUCED_SCALE:
      label_container_->SetVisible(true);
      canvas_->SetVisible(true);
      canvas_->SetScale(kReducedScale);
      canvas_->SchedulePaint();
      widget_->Show();
      break;
    case INVISIBLE:
      widget_->Hide();
      break;
  }
}

void TouchObserverHUD::UpdateTouchPointLabel(int index) {
  int trace_index = touch_log_->GetTraceIndex(index);
  const TouchTrace& trace = touch_log_->traces()[trace_index];
  TouchTrace::const_reverse_iterator point = trace.log().rbegin();
  ui::EventType touch_status = point->type;
  float touch_radius = std::max(point->radius_x, point->radius_y);
  while (point != trace.log().rend() && point->type == ui::ET_TOUCH_CANCELLED)
    point++;
  DCHECK(point != trace.log().rend());
  gfx::Point touch_position = point->location;

  std::string string = base::StringPrintf(
      "%2d: %s %s (%.4f)", index, GetTouchEventLabel(touch_status),
      touch_position.ToString().c_str(), touch_radius);
  touch_labels_[index]->SetText(base::UTF8ToUTF16(string));
}

}  // namespace ash
