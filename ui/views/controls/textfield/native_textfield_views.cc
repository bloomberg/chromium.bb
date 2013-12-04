// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/textfield/native_textfield_views.h"

#include <algorithm>
#include <set>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ui_strings.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drag_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/text_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/controls/textfield/textfield_views_model.h"
#include "ui/views/drag_utils.h"
#include "ui/views/ime/input_method.h"
#include "ui/views/metrics.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/base/cursor/cursor.h"
#endif

#if defined(OS_WIN) && defined(USE_AURA)
#include "base/win/win_util.h"
#endif

namespace {

void ConvertRectToScreen(const views::View* src, gfx::Rect* r) {
  DCHECK(src);

  gfx::Point new_origin = r->origin();
  views::View::ConvertPointToScreen(src, &new_origin);
  r->set_origin(new_origin);
}

}  // namespace

namespace views {

const char NativeTextfieldViews::kViewClassName[] =
    "views/NativeTextfieldViews";

NativeTextfieldViews::NativeTextfieldViews(Textfield* parent)
    : textfield_(parent),
      model_(new TextfieldViewsModel(this)),
      text_border_(new FocusableBorder()),
      is_cursor_visible_(false),
      is_drop_cursor_visible_(false),
      skip_input_method_cancel_composition_(false),
      initiating_drag_(false),
      cursor_timer_(this),
      aggregated_clicks_(0) {
  set_border(text_border_);
  GetRenderText()->SetFontList(textfield_->font_list());
  UpdateColorsFromTheme(GetNativeTheme());
  set_context_menu_controller(this);
  parent->set_context_menu_controller(this);
  set_drag_controller(this);
}

NativeTextfieldViews::~NativeTextfieldViews() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldViews, View overrides:

bool NativeTextfieldViews::OnMousePressed(const ui::MouseEvent& event) {
  OnBeforeUserAction();
  TrackMouseClicks(event);

  TextfieldController* controller = textfield_->GetController();
  if (!(controller && controller->HandleMouseEvent(textfield_, event)) &&
      !textfield_->OnMousePressed(event)) {
    HandleMousePressEvent(event);
  }

  OnAfterUserAction();
  touch_selection_controller_.reset();
  return true;
}

bool NativeTextfieldViews::ExceededDragThresholdFromLastClickLocation(
    const ui::MouseEvent& event) {
  return ExceededDragThreshold(event.location() - last_click_location_);
}

bool NativeTextfieldViews::OnMouseDragged(const ui::MouseEvent& event) {
  // Don't adjust the cursor on a potential drag and drop, or if the mouse
  // movement from the last mouse click does not exceed the drag threshold.
  if (initiating_drag_ || !ExceededDragThresholdFromLastClickLocation(event) ||
      !event.IsOnlyLeftMouseButton()) {
    return true;
  }

  OnBeforeUserAction();
  // TODO: Remove once NativeTextfield implementations are consolidated to
  // Textfield.
  if (!textfield_->OnMouseDragged(event)) {
    MoveCursorTo(event.location(), true);
    if (aggregated_clicks_ == 1) {
      model_->SelectWord();
      // Expand the selection so the initially selected word remains selected.
      gfx::Range selection = GetRenderText()->selection();
      const size_t min = std::min(selection.GetMin(),
                                  double_click_word_.GetMin());
      const size_t max = std::max(selection.GetMax(),
                                  double_click_word_.GetMax());
      const bool reversed = selection.is_reversed();
      selection.set_start(reversed ? max : min);
      selection.set_end(reversed ? min : max);
      model_->SelectRange(selection);
    }
    SchedulePaint();
  }
  OnAfterUserAction();
  return true;
}

void NativeTextfieldViews::OnMouseReleased(const ui::MouseEvent& event) {
  OnBeforeUserAction();
  // TODO: Remove once NativeTextfield implementations are consolidated to
  // Textfield.
  textfield_->OnMouseReleased(event);
  // Cancel suspected drag initiations, the user was clicking in the selection.
  if (initiating_drag_ && MoveCursorTo(event.location(), false))
    SchedulePaint();
  initiating_drag_ = false;
  OnAfterUserAction();
}

void NativeTextfieldViews::OnGestureEvent(ui::GestureEvent* event) {
  textfield_->OnGestureEvent(event);
  if (event->handled())
    return;

  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      OnBeforeUserAction();
      textfield_->RequestFocus();
      // We don't deselect if the point is in the selection
      // because TAP_DOWN may turn into a LONG_PRESS.
      if (!GetRenderText()->IsPointInSelection(event->location()) &&
          MoveCursorTo(event->location(), false))
        SchedulePaint();
      OnAfterUserAction();
      event->SetHandled();
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      OnBeforeUserAction();
      if (MoveCursorTo(event->location(), true))
        SchedulePaint();
      OnAfterUserAction();
      event->SetHandled();
      break;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
      CreateTouchSelectionControllerAndNotifyIt();
      event->SetHandled();
      break;
    case ui::ET_GESTURE_TAP:
      if (event->details().tap_count() == 1) {
        CreateTouchSelectionControllerAndNotifyIt();
      } else {
        OnBeforeUserAction();
        SelectAll(false);
        OnAfterUserAction();
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_LONG_PRESS:
      // If long press happens outside selection, select word and show context
      // menu (If touch selection is enabled, context menu is shown by the
      // |touch_selection_controller_|, hence we mark the event handled.
      // Otherwise, the regular context menu will be shown by views).
      // If long press happens in selected text and touch drag drop is enabled,
      // we will turn off touch selection (if one exists) and let views do drag
      // drop.
      if (!GetRenderText()->IsPointInSelection(event->location())) {
        OnBeforeUserAction();
        model_->SelectWord();
        touch_selection_controller_.reset(
            ui::TouchSelectionController::create(this));
        OnCaretBoundsChanged();
        SchedulePaint();
        OnAfterUserAction();
        if (touch_selection_controller_.get())
          event->SetHandled();
      } else if (switches::IsTouchDragDropEnabled()) {
        initiating_drag_ = true;
        touch_selection_controller_.reset();
      } else {
        if (!touch_selection_controller_.get())
          CreateTouchSelectionControllerAndNotifyIt();
        if (touch_selection_controller_.get())
          event->SetHandled();
      }
      return;
    case ui::ET_GESTURE_LONG_TAP:
      if (!touch_selection_controller_.get())
        CreateTouchSelectionControllerAndNotifyIt();

      // If touch selection is enabled, the context menu on long tap will be
      // shown by the |touch_selection_controller_|, hence we mark the event
      // handled so views does not try to show context menu on it.
      if (touch_selection_controller_.get())
        event->SetHandled();
      break;
    default:
      View::OnGestureEvent(event);
      return;
  }
  PlatformGestureEventHandling(event);
}

bool NativeTextfieldViews::OnKeyPressed(const ui::KeyEvent& event) {
  // OnKeyPressed/OnKeyReleased/OnFocus/OnBlur will never be invoked on
  // NativeTextfieldViews as it will never gain focus.
  NOTREACHED();
  return false;
}

bool NativeTextfieldViews::OnKeyReleased(const ui::KeyEvent& event) {
  NOTREACHED();
  return false;
}

bool NativeTextfieldViews::GetDropFormats(
    int* formats,
    std::set<OSExchangeData::CustomFormat>* custom_formats) {
  if (!textfield_->enabled() || textfield_->read_only())
    return false;
  // TODO(msw): Can we support URL, FILENAME, etc.?
  *formats = ui::OSExchangeData::STRING;
  TextfieldController* controller = textfield_->GetController();
  if (controller)
    controller->AppendDropFormats(formats, custom_formats);
  return true;
}

bool NativeTextfieldViews::CanDrop(const OSExchangeData& data) {
  int formats;
  std::set<OSExchangeData::CustomFormat> custom_formats;
  GetDropFormats(&formats, &custom_formats);
  return textfield_->enabled() && !textfield_->read_only() &&
      data.HasAnyFormat(formats, custom_formats);
}

int NativeTextfieldViews::OnDragUpdated(const ui::DropTargetEvent& event) {
  DCHECK(CanDrop(event.data()));

  const gfx::Range& selection = GetRenderText()->selection();
  drop_cursor_position_ = GetRenderText()->FindCursorPosition(event.location());
  bool in_selection = !selection.is_empty() &&
      selection.Contains(gfx::Range(drop_cursor_position_.caret_pos()));
  is_drop_cursor_visible_ = !in_selection;
  // TODO(msw): Pan over text when the user drags to the visible text edge.
  OnCaretBoundsChanged();
  SchedulePaint();

  if (initiating_drag_) {
    if (in_selection)
      return ui::DragDropTypes::DRAG_NONE;
    return event.IsControlDown() ? ui::DragDropTypes::DRAG_COPY :
                                   ui::DragDropTypes::DRAG_MOVE;
  }
  return ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE;
}

void NativeTextfieldViews::OnDragExited() {
  is_drop_cursor_visible_ = false;
  SchedulePaint();
}

int NativeTextfieldViews::OnPerformDrop(const ui::DropTargetEvent& event) {
  DCHECK(CanDrop(event.data()));

  is_drop_cursor_visible_ = false;

  TextfieldController* controller = textfield_->GetController();
  if (controller) {
      int drag_operation = controller->OnDrop(event.data());
      if (drag_operation != ui::DragDropTypes::DRAG_NONE)
        return drag_operation;
  }

  DCHECK(!initiating_drag_ ||
         !GetRenderText()->IsPointInSelection(event.location()));
  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;

  gfx::SelectionModel drop_destination_model =
      GetRenderText()->FindCursorPosition(event.location());
  string16 text;
  event.data().GetString(&text);
  text = GetTextForDisplay(text);

  // Delete the current selection for a drag and drop within this view.
  const bool move = initiating_drag_ && !event.IsControlDown() &&
                    event.source_operations() & ui::DragDropTypes::DRAG_MOVE;
  if (move) {
    // Adjust the drop destination if it is on or after the current selection.
    size_t drop = drop_destination_model.caret_pos();
    drop -= GetSelectedRange().Intersect(gfx::Range(0, drop)).length();
    model_->DeleteSelectionAndInsertTextAt(text, drop);
  } else {
    model_->MoveCursorTo(drop_destination_model);
    // Drop always inserts text even if the textfield is not in insert mode.
    model_->InsertText(text);
  }
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(true, true);
  OnAfterUserAction();
  return move ? ui::DragDropTypes::DRAG_MOVE : ui::DragDropTypes::DRAG_COPY;
}

void NativeTextfieldViews::OnDragDone() {
  initiating_drag_ = false;
  is_drop_cursor_visible_ = false;
}

void NativeTextfieldViews::OnPaint(gfx::Canvas* canvas) {
  OnPaintBackground(canvas);
  PaintTextAndCursor(canvas);
  if (textfield_->draw_border())
    OnPaintBorder(canvas);
}

void NativeTextfieldViews::OnFocus() {
  NOTREACHED();
}

void NativeTextfieldViews::OnBlur() {
  NOTREACHED();
}

void NativeTextfieldViews::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  UpdateColorsFromTheme(theme);
}

void NativeTextfieldViews::SelectRect(const gfx::Point& start,
                                      const gfx::Point& end) {
  if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE)
    return;

  gfx::SelectionModel start_caret = GetRenderText()->FindCursorPosition(start);
  gfx::SelectionModel end_caret = GetRenderText()->FindCursorPosition(end);
  gfx::SelectionModel selection(
      gfx::Range(start_caret.caret_pos(), end_caret.caret_pos()),
      end_caret.caret_affinity());

  OnBeforeUserAction();
  model_->SelectSelectionModel(selection);
  OnCaretBoundsChanged();
  SchedulePaint();
  OnAfterUserAction();
}

void NativeTextfieldViews::MoveCaretTo(const gfx::Point& point) {
  SelectRect(point, point);
}

void NativeTextfieldViews::GetSelectionEndPoints(gfx::Rect* p1,
                                                 gfx::Rect* p2) {
  gfx::RenderText* render_text = GetRenderText();
  const gfx::SelectionModel& sel = render_text->selection_model();
  gfx::SelectionModel start_sel =
      render_text->GetSelectionModelForSelectionStart();
  *p1 = render_text->GetCursorBounds(start_sel, true);
  *p2 = render_text->GetCursorBounds(sel, true);
}

gfx::Rect NativeTextfieldViews::GetBounds() {
  return bounds();
}

gfx::NativeView NativeTextfieldViews::GetNativeView() {
  return GetWidget()->GetNativeView();
}

void NativeTextfieldViews::ConvertPointToScreen(gfx::Point* point) {
  View::ConvertPointToScreen(this, point);
}

void NativeTextfieldViews::ConvertPointFromScreen(gfx::Point* point) {
  View::ConvertPointFromScreen(this, point);
}

bool NativeTextfieldViews::DrawsHandles() {
  return false;
}

void NativeTextfieldViews::OpenContextMenu(const gfx::Point& anchor) {
  touch_selection_controller_.reset();
  ShowContextMenu(anchor, ui::MENU_SOURCE_TOUCH_EDIT_MENU);
}

gfx::NativeCursor NativeTextfieldViews::GetCursor(const ui::MouseEvent& event) {
  bool in_selection = GetRenderText()->IsPointInSelection(event.location());
  bool drag_event = event.type() == ui::ET_MOUSE_DRAGGED;
  bool text_cursor = !initiating_drag_ && (drag_event || !in_selection);
#if defined(USE_AURA)
  return text_cursor ? ui::kCursorIBeam : ui::kCursorNull;
#elif defined(OS_WIN)
  static HCURSOR ibeam = LoadCursor(NULL, IDC_IBEAM);
  static HCURSOR arrow = LoadCursor(NULL, IDC_ARROW);
  return text_cursor ? ibeam : arrow;
#endif
}

/////////////////////////////////////////////////////////////////
// NativeTextfieldViews, ContextMenuController overrides:
void NativeTextfieldViews::ShowContextMenuForView(
    View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  UpdateContextMenu();
  if (context_menu_runner_->RunMenuAt(GetWidget(), NULL,
          gfx::Rect(point, gfx::Size()), views::MenuItemView::TOPLEFT,
          source_type,
          MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU) ==
      MenuRunner::MENU_DELETED)
    return;
}

/////////////////////////////////////////////////////////////////
// NativeTextfieldViews, views::DragController overrides:
void NativeTextfieldViews::WriteDragDataForView(views::View* sender,
                                                const gfx::Point& press_pt,
                                                OSExchangeData* data) {
  DCHECK_NE(ui::DragDropTypes::DRAG_NONE,
            GetDragOperationsForView(sender, press_pt));
  data->SetString(GetSelectedText());
  scoped_ptr<gfx::Canvas> canvas(
      views::GetCanvasForDragImage(textfield_->GetWidget(), size()));
  GetRenderText()->DrawSelectedTextForDrag(canvas.get());
  drag_utils::SetDragImageOnDataObject(*canvas, size(),
                                       press_pt.OffsetFromOrigin(),
                                       data);
  TextfieldController* controller = textfield_->GetController();
  if (controller)
    controller->OnWriteDragData(data);
}

int NativeTextfieldViews::GetDragOperationsForView(views::View* sender,
                                                   const gfx::Point& p) {
  int drag_operations = ui::DragDropTypes::DRAG_COPY;
  if (!textfield_->enabled() || textfield_->IsObscured() ||
      !GetRenderText()->IsPointInSelection(p))
    drag_operations = ui::DragDropTypes::DRAG_NONE;
  else if (sender == this && !textfield_->read_only())
    drag_operations =
        ui::DragDropTypes::DRAG_MOVE | ui::DragDropTypes::DRAG_COPY;
  TextfieldController* controller = textfield_->GetController();
  if (controller)
    controller->OnGetDragOperationsForTextfield(&drag_operations);
  return drag_operations;
}

bool NativeTextfieldViews::CanStartDragForView(View* sender,
                                               const gfx::Point& press_pt,
                                               const gfx::Point& p) {
  return initiating_drag_ && GetRenderText()->IsPointInSelection(press_pt);
}

/////////////////////////////////////////////////////////////////
// NativeTextfieldViews, NativeTextifieldWrapper overrides:

string16 NativeTextfieldViews::GetText() const {
  return model_->GetText();
}

void NativeTextfieldViews::UpdateText() {
  model_->SetText(GetTextForDisplay(textfield_->text()));
  OnCaretBoundsChanged();
  SchedulePaint();
  textfield_->NotifyAccessibilityEvent(
      ui::AccessibilityTypes::EVENT_TEXT_CHANGED, true);
}

void NativeTextfieldViews::AppendText(const string16& text) {
  if (text.empty())
    return;
  model_->Append(GetTextForDisplay(text));
  OnCaretBoundsChanged();
  SchedulePaint();
}

void NativeTextfieldViews::InsertOrReplaceText(const string16& text) {
  if (text.empty())
    return;
  model_->InsertText(text);
  OnCaretBoundsChanged();
  SchedulePaint();
}

base::i18n::TextDirection NativeTextfieldViews::GetTextDirection() const {
  return GetRenderText()->GetTextDirection();
}

string16 NativeTextfieldViews::GetSelectedText() const {
  return model_->GetSelectedText();
}

void NativeTextfieldViews::SelectAll(bool reversed) {
  model_->SelectAll(reversed);
  OnCaretBoundsChanged();
  SchedulePaint();
}

void NativeTextfieldViews::ClearSelection() {
  model_->ClearSelection();
  OnCaretBoundsChanged();
  SchedulePaint();
}

void NativeTextfieldViews::UpdateBorder() {
  // By default, if a caller calls Textfield::RemoveBorder() and does not set
  // any explicit margins, they should get zero margins.  But also call
  // UpdateXXXMargins() so we respect any explicitly-set margins.
  //
  // NOTE: If someday Textfield supports toggling |draw_border_| back on, we'll
  // need to update this conditional to set the insets to their default values.
  if (!textfield_->draw_border())
    text_border_->SetInsets(0, 0, 0, 0);
  UpdateHorizontalMargins();
  UpdateVerticalMargins();
}

void NativeTextfieldViews::UpdateTextColor() {
  SetColor(textfield_->GetTextColor());
}

void NativeTextfieldViews::UpdateBackgroundColor() {
  const SkColor color = textfield_->GetBackgroundColor();
  set_background(Background::CreateSolidBackground(color));
  GetRenderText()->set_background_is_transparent(SkColorGetA(color) != 0xFF);
  SchedulePaint();
}

void NativeTextfieldViews::UpdateReadOnly() {
  OnTextInputTypeChanged();
}

void NativeTextfieldViews::UpdateFont() {
  GetRenderText()->SetFontList(textfield_->font_list());
  OnCaretBoundsChanged();
}

void NativeTextfieldViews::UpdateIsObscured() {
  GetRenderText()->SetObscured(textfield_->IsObscured());
  OnCaretBoundsChanged();
  SchedulePaint();
  OnTextInputTypeChanged();
}

void NativeTextfieldViews::UpdateEnabled() {
  SetEnabled(textfield_->enabled());
  SchedulePaint();
  OnTextInputTypeChanged();
}

gfx::Insets NativeTextfieldViews::CalculateInsets() {
  return GetInsets();
}

void NativeTextfieldViews::UpdateHorizontalMargins() {
  int left, right;
  if (!textfield_->GetHorizontalMargins(&left, &right))
    return;
  gfx::Insets inset = GetInsets();
  text_border_->SetInsets(inset.top(), left, inset.bottom(), right);
  OnBoundsChanged(GetBounds());
}

void NativeTextfieldViews::UpdateVerticalMargins() {
  int top, bottom;
  if (!textfield_->GetVerticalMargins(&top, &bottom))
    return;
  gfx::Insets inset = GetInsets();
  text_border_->SetInsets(top, inset.left(), bottom, inset.right());
  OnBoundsChanged(GetBounds());
}

bool NativeTextfieldViews::SetFocus() {
  return false;
}

View* NativeTextfieldViews::GetView() {
  return this;
}

gfx::NativeView NativeTextfieldViews::GetTestingHandle() const {
  NOTREACHED();
  return NULL;
}

bool NativeTextfieldViews::IsIMEComposing() const {
  return model_->HasCompositionText();
}

gfx::Range NativeTextfieldViews::GetSelectedRange() const {
  return GetRenderText()->selection();
}

void NativeTextfieldViews::SelectRange(const gfx::Range& range) {
  model_->SelectRange(range);
  OnCaretBoundsChanged();
  SchedulePaint();
  textfield_->NotifyAccessibilityEvent(
      ui::AccessibilityTypes::EVENT_SELECTION_CHANGED, true);
}

gfx::SelectionModel NativeTextfieldViews::GetSelectionModel() const {
  return GetRenderText()->selection_model();
}

void NativeTextfieldViews::SelectSelectionModel(
    const gfx::SelectionModel& sel) {
  model_->SelectSelectionModel(sel);
  OnCaretBoundsChanged();
  SchedulePaint();
}

size_t NativeTextfieldViews::GetCursorPosition() const {
  return model_->GetCursorPosition();
}

bool NativeTextfieldViews::GetCursorEnabled() const {
  return GetRenderText()->cursor_enabled();
}

void NativeTextfieldViews::SetCursorEnabled(bool enabled) {
  GetRenderText()->SetCursorEnabled(enabled);
}

bool NativeTextfieldViews::HandleKeyPressed(const ui::KeyEvent& e) {
  TextfieldController* controller = textfield_->GetController();
  bool handled = false;
  if (controller)
    handled = controller->HandleKeyEvent(textfield_, e);
  touch_selection_controller_.reset();
  return handled || HandleKeyEvent(e);
}

bool NativeTextfieldViews::HandleKeyReleased(const ui::KeyEvent& e) {
  return false;  // crbug.com/127520
}

void NativeTextfieldViews::HandleFocus() {
  GetRenderText()->set_focused(true);
  is_cursor_visible_ = true;
  SchedulePaint();
  GetInputMethod()->OnFocus();
  OnCaretBoundsChanged();

  const size_t caret_blink_ms = Textfield::GetCaretBlinkMs();
  if (caret_blink_ms != 0) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&NativeTextfieldViews::UpdateCursor,
                   cursor_timer_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(caret_blink_ms));
  }
}

void NativeTextfieldViews::HandleBlur() {
  GetRenderText()->set_focused(false);
  GetInputMethod()->OnBlur();
  // Stop blinking cursor.
  cursor_timer_.InvalidateWeakPtrs();
  if (is_cursor_visible_) {
    is_cursor_visible_ = false;
    RepaintCursor();
  }

  touch_selection_controller_.reset();
}

ui::TextInputClient* NativeTextfieldViews::GetTextInputClient() {
  return textfield_->read_only() ? NULL : this;
}

void NativeTextfieldViews::ClearEditHistory() {
  model_->ClearEditHistory();
}

int NativeTextfieldViews::GetFontHeight() {
  return GetRenderText()->font_list().GetHeight();
}

int NativeTextfieldViews::GetTextfieldBaseline() const {
  return GetRenderText()->GetBaseline();
}

int NativeTextfieldViews::GetWidthNeededForText() const {
  return GetRenderText()->GetContentWidth() + GetInsets().width();
}

void NativeTextfieldViews::ExecuteTextCommand(int command_id) {
  ExecuteCommand(command_id, 0);
}

bool NativeTextfieldViews::HasTextBeingDragged() {
  return initiating_drag_;
}

gfx::Point NativeTextfieldViews::GetContextMenuLocation() {
  return GetCaretBounds().bottom_right();
}

/////////////////////////////////////////////////////////////////
// NativeTextfieldViews, ui::SimpleMenuModel::Delegate overrides:

bool NativeTextfieldViews::IsCommandIdChecked(int command_id) const {
  return true;
}

bool NativeTextfieldViews::IsCommandIdEnabled(int command_id) const {
  TextfieldController* controller = textfield_->GetController();
  if (controller && controller->HandlesCommand(command_id))
    return controller->IsCommandIdEnabled(command_id);

  bool editable = !textfield_->read_only();
  string16 result;
  switch (command_id) {
    case IDS_APP_UNDO:
      return editable && model_->CanUndo();
    case IDS_APP_CUT:
      return editable && model_->HasSelection() && !textfield_->IsObscured();
    case IDS_APP_COPY:
      return model_->HasSelection() && !textfield_->IsObscured();
    case IDS_APP_PASTE:
      ui::Clipboard::GetForCurrentThread()->ReadText(
          ui::CLIPBOARD_TYPE_COPY_PASTE, &result);
      return editable && !result.empty();
    case IDS_APP_DELETE:
      return editable && model_->HasSelection();
    case IDS_APP_SELECT_ALL:
      return !model_->GetText().empty();
    default:
      return controller->IsCommandIdEnabled(command_id);
  }
}

bool NativeTextfieldViews::GetAcceleratorForCommandId(int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

bool NativeTextfieldViews::IsItemForCommandIdDynamic(int command_id) const {
  const TextfieldController* controller = textfield_->GetController();
  return controller && controller->IsItemForCommandIdDynamic(command_id);
}

string16 NativeTextfieldViews::GetLabelForCommandId(int command_id) const {
  const TextfieldController* controller = textfield_->GetController();
  return controller ? controller->GetLabelForCommandId(command_id) : string16();
}

void NativeTextfieldViews::ExecuteCommand(int command_id, int event_flags) {
  touch_selection_controller_.reset();
  if (!IsCommandIdEnabled(command_id))
    return;

  TextfieldController* controller = textfield_->GetController();
  if (controller && controller->HandlesCommand(command_id)) {
    controller->ExecuteCommand(command_id, 0);
  } else {
    bool text_changed = false;
    switch (command_id) {
      case IDS_APP_UNDO:
        OnBeforeUserAction();
        text_changed = model_->Undo();
        UpdateAfterChange(text_changed, text_changed);
        OnAfterUserAction();
        break;
      case IDS_APP_CUT:
        OnBeforeUserAction();
        text_changed = Cut();
        UpdateAfterChange(text_changed, text_changed);
        OnAfterUserAction();
        break;
      case IDS_APP_COPY:
        OnBeforeUserAction();
        Copy();
        OnAfterUserAction();
        break;
      case IDS_APP_PASTE:
        OnBeforeUserAction();
        text_changed = Paste();
        UpdateAfterChange(text_changed, text_changed);
        OnAfterUserAction();
        break;
      case IDS_APP_DELETE:
        OnBeforeUserAction();
        text_changed = model_->Delete();
        UpdateAfterChange(text_changed, text_changed);
        OnAfterUserAction();
        break;
      case IDS_APP_SELECT_ALL:
        OnBeforeUserAction();
        SelectAll(false);
        UpdateAfterChange(false, true);
        OnAfterUserAction();
        break;
      default:
        controller->ExecuteCommand(command_id, 0);
        break;
    }
  }
}

void NativeTextfieldViews::SetColor(SkColor value) {
  GetRenderText()->SetColor(value);
  SchedulePaint();
}

void NativeTextfieldViews::ApplyColor(SkColor value, const gfx::Range& range) {
  GetRenderText()->ApplyColor(value, range);
  SchedulePaint();
}

void NativeTextfieldViews::SetStyle(gfx::TextStyle style, bool value) {
  GetRenderText()->SetStyle(style, value);
  SchedulePaint();
}

void NativeTextfieldViews::ApplyStyle(gfx::TextStyle style,
                                      bool value,
                                      const gfx::Range& range) {
  GetRenderText()->ApplyStyle(style, value, range);
  SchedulePaint();
}

void NativeTextfieldViews::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // Set the RenderText display area.
  gfx::Insets insets = GetInsets();
  gfx::Rect display_rect(insets.left(),
                         insets.top(),
                         width() - insets.width(),
                         height() - insets.height());
  GetRenderText()->SetDisplayRect(display_rect);
  OnCaretBoundsChanged();
}

///////////////////////////////////////////////////////////////////////////////
// NativeTextfieldViews, ui::TextInputClient implementation, private:

void NativeTextfieldViews::SetCompositionText(
    const ui::CompositionText& composition) {
  if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE)
    return;

  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  model_->SetCompositionText(composition);
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(true, true);
  OnAfterUserAction();
}

void NativeTextfieldViews::ConfirmCompositionText() {
  if (!model_->HasCompositionText())
    return;

  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  model_->ConfirmCompositionText();
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(true, true);
  OnAfterUserAction();
}

void NativeTextfieldViews::ClearCompositionText() {
  if (!model_->HasCompositionText())
    return;

  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  model_->CancelCompositionText();
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(true, true);
  OnAfterUserAction();
}

void NativeTextfieldViews::InsertText(const string16& text) {
  // TODO(suzhe): Filter invalid characters.
  if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE || text.empty())
    return;

  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  if (GetRenderText()->insert_mode())
    model_->InsertText(GetTextForDisplay(text));
  else
    model_->ReplaceText(GetTextForDisplay(text));
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(true, true);
  OnAfterUserAction();
}

void NativeTextfieldViews::InsertChar(char16 ch, int flags) {
  if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE ||
      !ShouldInsertChar(ch, flags)) {
    return;
  }

  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  if (GetRenderText()->insert_mode())
    model_->InsertChar(ch);
  else
    model_->ReplaceChar(ch);
  skip_input_method_cancel_composition_ = false;

  model_->SetText(GetTextForDisplay(GetText()));

  UpdateAfterChange(true, true);
  OnAfterUserAction();

  if (textfield_->IsObscured()) {
    const base::TimeDelta& reveal_duration =
        textfield_->obscured_reveal_duration();
    if (reveal_duration != base::TimeDelta()) {
      const size_t change_offset = model_->GetCursorPosition();
      DCHECK_GT(change_offset, 0u);
      RevealObscuredChar(change_offset - 1, reveal_duration);
    }
  }
}

gfx::NativeWindow NativeTextfieldViews::GetAttachedWindow() const {
  // Imagine the following hierarchy.
  //   [NativeWidget A] - FocusManager
  //     [View]
  //     [NativeWidget B]
  //       [View]
  //         [View X]
  // An important thing is that [NativeWidget A] owns Win32 input focus even
  // when [View X] is logically focused by FocusManager. As a result, an Win32
  // IME may want to interact with the native view of [NativeWidget A] rather
  // than that of [NativeWidget B]. This is why we need to call
  // GetTopLevelWidget() here.
  return GetWidget()->GetTopLevelWidget()->GetNativeView();
}

ui::TextInputType NativeTextfieldViews::GetTextInputType() const {
  return textfield_->GetTextInputType();
}

ui::TextInputMode NativeTextfieldViews::GetTextInputMode() const {
  return ui::TEXT_INPUT_MODE_DEFAULT;
}

bool NativeTextfieldViews::CanComposeInline() const {
  return true;
}

gfx::Rect NativeTextfieldViews::GetCaretBounds() const {
  // TextInputClient::GetCaretBounds is expected to return a value in screen
  // coordinates.
  gfx::Rect rect = GetRenderText()->GetUpdatedCursorBounds();
  ConvertRectToScreen(this, &rect);
  return rect;
}

bool NativeTextfieldViews::GetCompositionCharacterBounds(
    uint32 index,
    gfx::Rect* rect) const {
  DCHECK(rect);
  if (!HasCompositionText())
    return false;
  const gfx::Range& composition_range = GetRenderText()->GetCompositionRange();
  DCHECK(!composition_range.is_empty());

  size_t text_index = composition_range.start() + index;
  if (composition_range.end() <= text_index)
    return false;
  if (!GetRenderText()->IsCursorablePosition(text_index)) {
    text_index = GetRenderText()->IndexOfAdjacentGrapheme(
        text_index, gfx::CURSOR_BACKWARD);
  }
  if (text_index < composition_range.start())
    return false;
  const gfx::SelectionModel caret(text_index, gfx::CURSOR_BACKWARD);
  *rect = GetRenderText()->GetCursorBounds(caret, false);
  ConvertRectToScreen(this, rect);

  return true;
}

bool NativeTextfieldViews::HasCompositionText() const {
  return model_->HasCompositionText();
}

bool NativeTextfieldViews::GetTextRange(gfx::Range* range) const {
  if (!ImeEditingAllowed())
    return false;

  model_->GetTextRange(range);
  return true;
}

bool NativeTextfieldViews::GetCompositionTextRange(gfx::Range* range) const {
  if (!ImeEditingAllowed())
    return false;

  model_->GetCompositionTextRange(range);
  return true;
}

bool NativeTextfieldViews::GetSelectionRange(gfx::Range* range) const {
  if (!ImeEditingAllowed())
    return false;
  *range = GetSelectedRange();
  return true;
}

bool NativeTextfieldViews::SetSelectionRange(const gfx::Range& range) {
  if (!ImeEditingAllowed() || !range.IsValid())
    return false;

  OnBeforeUserAction();
  SelectRange(range);
  OnAfterUserAction();
  return true;
}

bool NativeTextfieldViews::DeleteRange(const gfx::Range& range) {
  if (!ImeEditingAllowed() || range.is_empty())
    return false;

  OnBeforeUserAction();
  model_->SelectRange(range);
  if (model_->HasSelection()) {
    model_->DeleteSelection();
    UpdateAfterChange(true, true);
  }
  OnAfterUserAction();
  return true;
}

bool NativeTextfieldViews::GetTextFromRange(
    const gfx::Range& range,
    string16* text) const {
  if (!ImeEditingAllowed() || !range.IsValid())
    return false;

  gfx::Range text_range;
  if (!GetTextRange(&text_range) || !text_range.Contains(range))
    return false;

  *text = model_->GetTextFromRange(range);
  return true;
}

void NativeTextfieldViews::OnInputMethodChanged() {
  // TODO(msw): NOTIMPLEMENTED(); see http://crbug.com/140402
}

bool NativeTextfieldViews::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction) {
  // Restore text directionality mode when the indicated direction matches the
  // current forced mode; otherwise, force the mode indicated. This helps users
  // manage BiDi text layout without getting stuck in forced LTR or RTL modes.
  const gfx::DirectionalityMode mode = direction == base::i18n::RIGHT_TO_LEFT ?
      gfx::DIRECTIONALITY_FORCE_RTL : gfx::DIRECTIONALITY_FORCE_LTR;
  if (mode == GetRenderText()->directionality_mode())
    GetRenderText()->SetDirectionalityMode(gfx::DIRECTIONALITY_FROM_TEXT);
  else
    GetRenderText()->SetDirectionalityMode(mode);
  SchedulePaint();
  return true;
}

void NativeTextfieldViews::ExtendSelectionAndDelete(
    size_t before,
    size_t after) {
  gfx::Range range = GetSelectedRange();
  DCHECK_GE(range.start(), before);

  range.set_start(range.start() - before);
  range.set_end(range.end() + after);
  gfx::Range text_range;
  if (GetTextRange(&text_range) && text_range.Contains(range))
    DeleteRange(range);
}

void NativeTextfieldViews::EnsureCaretInRect(const gfx::Rect& rect) {
}

void NativeTextfieldViews::OnCandidateWindowShown() {
}

void NativeTextfieldViews::OnCandidateWindowUpdated() {
}

void NativeTextfieldViews::OnCandidateWindowHidden() {
}

void NativeTextfieldViews::OnCompositionTextConfirmedOrCleared() {
  if (skip_input_method_cancel_composition_)
    return;
  DCHECK(textfield_->GetInputMethod());
  textfield_->GetInputMethod()->CancelComposition(textfield_);
}

gfx::RenderText* NativeTextfieldViews::GetRenderText() const {
  return model_->render_text();
}

string16 NativeTextfieldViews::GetTextForDisplay(const string16& text) {
  return textfield_->style() & Textfield::STYLE_LOWERCASE ?
      base::i18n::ToLower(text) : text;
}

void NativeTextfieldViews::UpdateColorsFromTheme(const ui::NativeTheme* theme) {
  UpdateTextColor();
  UpdateBackgroundColor();
  gfx::RenderText* render_text = GetRenderText();
  render_text->set_cursor_color(textfield_->GetTextColor());
  render_text->set_selection_color(theme->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldSelectionColor));
  render_text->set_selection_background_focused_color(theme->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldSelectionBackgroundFocused));
}

void NativeTextfieldViews::UpdateCursor() {
  const size_t caret_blink_ms = Textfield::GetCaretBlinkMs();
  is_cursor_visible_ = !is_cursor_visible_ || (caret_blink_ms == 0);
  RepaintCursor();
  if (caret_blink_ms != 0) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&NativeTextfieldViews::UpdateCursor,
                   cursor_timer_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(caret_blink_ms));
  }
}

void NativeTextfieldViews::RepaintCursor() {
  gfx::Rect r(GetRenderText()->GetUpdatedCursorBounds());
  r.Inset(-1, -1, -1, -1);
  SchedulePaintInRect(r);
}

void NativeTextfieldViews::PaintTextAndCursor(gfx::Canvas* canvas) {
  TRACE_EVENT0("views", "NativeTextfieldViews::PaintTextAndCursor");
  canvas->Save();
  GetRenderText()->set_cursor_visible(!is_drop_cursor_visible_ &&
      is_cursor_visible_ && !model_->HasSelection());
  // Draw the text, cursor, and selection.
  GetRenderText()->Draw(canvas);

  // Draw the detached drop cursor that marks where the text will be dropped.
  if (is_drop_cursor_visible_)
    GetRenderText()->DrawCursor(canvas, drop_cursor_position_);

  // Draw placeholder text if needed.
  if (model_->GetText().empty() &&
      !textfield_->GetPlaceholderText().empty()) {
    canvas->DrawStringInt(
        textfield_->GetPlaceholderText(),
        GetRenderText()->GetPrimaryFont(),
        textfield_->placeholder_text_color(),
        GetRenderText()->display_rect());
  }
  canvas->Restore();
}

bool NativeTextfieldViews::HandleKeyEvent(const ui::KeyEvent& key_event) {
  // TODO(oshima): Refactor and consolidate with ExecuteCommand.
  if (key_event.type() == ui::ET_KEY_PRESSED) {
    ui::KeyboardCode key_code = key_event.key_code();
    if (key_code == ui::VKEY_TAB || key_event.IsUnicodeKeyCode())
      return false;

    OnBeforeUserAction();
    const bool editable = !textfield_->read_only();
    const bool readable = !textfield_->IsObscured();
    const bool shift = key_event.IsShiftDown();
    const bool control = key_event.IsControlDown();
    const bool alt = key_event.IsAltDown() || key_event.IsAltGrDown();
    bool text_changed = false;
    bool cursor_changed = false;
    switch (key_code) {
      case ui::VKEY_Z:
        if (control && !shift && !alt && editable)
          cursor_changed = text_changed = model_->Undo();
        else if (control && shift && !alt && editable)
          cursor_changed = text_changed = model_->Redo();
        break;
      case ui::VKEY_Y:
        if (control && !alt && editable)
          cursor_changed = text_changed = model_->Redo();
        break;
      case ui::VKEY_A:
        if (control && !alt) {
          model_->SelectAll(false);
          cursor_changed = true;
        }
        break;
      case ui::VKEY_X:
        if (control && !alt && editable && readable)
          cursor_changed = text_changed = Cut();
        break;
      case ui::VKEY_C:
        if (control && !alt && readable)
          Copy();
        break;
      case ui::VKEY_V:
        if (control && !alt && editable)
          cursor_changed = text_changed = Paste();
        break;
      case ui::VKEY_RIGHT:
      case ui::VKEY_LEFT: {
        // We should ignore the alt-left/right keys because alt key doesn't make
        // any special effects for them and they can be shortcut keys such like
        // forward/back of the browser history.
        if (alt)
          break;
        const gfx::Range selection_range = GetSelectedRange();
        model_->MoveCursor(
            control ? gfx::WORD_BREAK : gfx::CHARACTER_BREAK,
            (key_code == ui::VKEY_RIGHT) ? gfx::CURSOR_RIGHT : gfx::CURSOR_LEFT,
            shift);
        cursor_changed = GetSelectedRange() != selection_range;
        break;
      }
      case ui::VKEY_END:
      case ui::VKEY_HOME:
        if ((key_code == ui::VKEY_HOME) ==
            (GetRenderText()->GetTextDirection() == base::i18n::RIGHT_TO_LEFT))
          model_->MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, shift);
        else
          model_->MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, shift);
        cursor_changed = true;
        break;
      case ui::VKEY_BACK:
      case ui::VKEY_DELETE:
        if (!editable)
          break;
        if (!model_->HasSelection()) {
          gfx::VisualCursorDirection direction = (key_code == ui::VKEY_DELETE) ?
              gfx::CURSOR_RIGHT : gfx::CURSOR_LEFT;
          if (shift && control) {
            // If both shift and control are pressed, then erase up to the
            // beginning/end of the buffer in ChromeOS. In windows, do nothing.
#if defined(OS_WIN)
            break;
#else
            model_->MoveCursor(gfx::LINE_BREAK, direction, true);
#endif
          } else if (control) {
            // If only control is pressed, then erase the previous/next word.
            model_->MoveCursor(gfx::WORD_BREAK, direction, true);
          }
        }
        if (key_code == ui::VKEY_BACK)
          model_->Backspace();
        else if (shift && model_->HasSelection() && readable)
          Cut();
        else
          model_->Delete();

        // Consume backspace and delete keys even if the edit did nothing. This
        // prevents potential unintended side-effects of further event handling.
        text_changed = true;
        break;
      case ui::VKEY_INSERT:
        if (control && !shift && readable)
          Copy();
        else if (shift && !control && editable)
          cursor_changed = text_changed = Paste();
        break;
      default:
        break;
    }

    // We must have input method in order to support text input.
    DCHECK(textfield_->GetInputMethod());

    UpdateAfterChange(text_changed, cursor_changed);
    OnAfterUserAction();
    return (text_changed || cursor_changed);
  }
  return false;
}

bool NativeTextfieldViews::MoveCursorTo(const gfx::Point& point, bool select) {
  if (!model_->MoveCursorTo(point, select))
    return false;
  OnCaretBoundsChanged();
  return true;
}

void NativeTextfieldViews::PropagateTextChange() {
  textfield_->SyncText();
}

void NativeTextfieldViews::UpdateAfterChange(bool text_changed,
                                             bool cursor_changed) {
  if (text_changed) {
    PropagateTextChange();
    textfield_->NotifyAccessibilityEvent(
        ui::AccessibilityTypes::EVENT_TEXT_CHANGED, true);
  }
  if (cursor_changed) {
    is_cursor_visible_ = true;
    RepaintCursor();
    if (!text_changed) {
      // TEXT_CHANGED implies SELECTION_CHANGED, so we only need to fire
      // this if only the selection changed.
      textfield_->NotifyAccessibilityEvent(
          ui::AccessibilityTypes::EVENT_SELECTION_CHANGED, true);
    }
  }
  if (text_changed || cursor_changed) {
    OnCaretBoundsChanged();
    SchedulePaint();
  }
}

void NativeTextfieldViews::UpdateContextMenu() {
  if (!context_menu_contents_.get()) {
    context_menu_contents_.reset(new ui::SimpleMenuModel(this));
    context_menu_contents_->AddItemWithStringId(IDS_APP_UNDO, IDS_APP_UNDO);
    context_menu_contents_->AddSeparator(ui::NORMAL_SEPARATOR);
    context_menu_contents_->AddItemWithStringId(IDS_APP_CUT, IDS_APP_CUT);
    context_menu_contents_->AddItemWithStringId(IDS_APP_COPY, IDS_APP_COPY);
    context_menu_contents_->AddItemWithStringId(IDS_APP_PASTE, IDS_APP_PASTE);
    context_menu_contents_->AddItemWithStringId(IDS_APP_DELETE, IDS_APP_DELETE);
    context_menu_contents_->AddSeparator(ui::NORMAL_SEPARATOR);
    context_menu_contents_->AddItemWithStringId(IDS_APP_SELECT_ALL,
                                                IDS_APP_SELECT_ALL);
    TextfieldController* controller = textfield_->GetController();
    if (controller)
      controller->UpdateContextMenu(context_menu_contents_.get());

    context_menu_delegate_.reset(
        new views::MenuModelAdapter(context_menu_contents_.get()));
    context_menu_runner_.reset(
        new MenuRunner(new views::MenuItemView(context_menu_delegate_.get())));
  }

  context_menu_delegate_->BuildMenu(context_menu_runner_->GetMenu());
}

void NativeTextfieldViews::OnTextInputTypeChanged() {
  // TODO(suzhe): changed from DCHECK. See http://crbug.com/81320.
  if (textfield_->GetInputMethod())
    textfield_->GetInputMethod()->OnTextInputTypeChanged(textfield_);
}

void NativeTextfieldViews::OnCaretBoundsChanged() {
  // TODO(suzhe): changed from DCHECK. See http://crbug.com/81320.
  if (textfield_->GetInputMethod())
    textfield_->GetInputMethod()->OnCaretBoundsChanged(textfield_);

  // Notify selection controller
  if (touch_selection_controller_.get())
    touch_selection_controller_->SelectionChanged();
}

void NativeTextfieldViews::OnBeforeUserAction() {
  TextfieldController* controller = textfield_->GetController();
  if (controller)
    controller->OnBeforeUserAction(textfield_);
}

void NativeTextfieldViews::OnAfterUserAction() {
  TextfieldController* controller = textfield_->GetController();
  if (controller)
    controller->OnAfterUserAction(textfield_);
}

bool NativeTextfieldViews::Cut() {
  if (!textfield_->read_only() && !textfield_->IsObscured() && model_->Cut()) {
    TextfieldController* controller = textfield_->GetController();
    if (controller)
      controller->OnAfterCutOrCopy();
    return true;
  }
  return false;
}

bool NativeTextfieldViews::Copy() {
  if (!textfield_->IsObscured() && model_->Copy()) {
    TextfieldController* controller = textfield_->GetController();
    if (controller)
      controller->OnAfterCutOrCopy();
    return true;
  }
  return false;
}

bool NativeTextfieldViews::Paste() {
  if (textfield_->read_only())
    return false;

  const string16 original_text = GetText();
  const bool success = model_->Paste();

  if (success) {
    // As Paste is handled in model_->Paste(), the RenderText may contain
    // upper case characters. This is not consistent with other places
    // which keeps RenderText only containing lower case characters.
    string16 new_text = GetTextForDisplay(GetText());
    model_->SetText(new_text);

    TextfieldController* controller = textfield_->GetController();
    if (controller)
      controller->OnAfterPaste();
  }
  return success;
}

void NativeTextfieldViews::TrackMouseClicks(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton()) {
    base::TimeDelta time_delta = event.time_stamp() - last_click_time_;
    if (time_delta.InMilliseconds() <= GetDoubleClickInterval() &&
        !ExceededDragThresholdFromLastClickLocation(event)) {
      // Upon clicking after a triple click, the count should go back to double
      // click and alternate between double and triple. This assignment maps
      // 0 to 1, 1 to 2, 2 to 1.
      aggregated_clicks_ = (aggregated_clicks_ % 2) + 1;
    } else {
      aggregated_clicks_ = 0;
    }
    last_click_time_ = event.time_stamp();
    last_click_location_ = event.location();
  }
}

void NativeTextfieldViews::HandleMousePressEvent(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() || event.IsOnlyRightMouseButton())
    textfield_->RequestFocus();

  if (!event.IsOnlyLeftMouseButton())
    return;

  initiating_drag_ = false;
  bool can_drag = true;

  switch (aggregated_clicks_) {
    case 0:
      if (can_drag && GetRenderText()->IsPointInSelection(event.location()))
        initiating_drag_ = true;
      else
        MoveCursorTo(event.location(), event.IsShiftDown());
      break;
    case 1:
      MoveCursorTo(event.location(), false);
      model_->SelectWord();
      double_click_word_ = GetRenderText()->selection();
      OnCaretBoundsChanged();
      break;
    case 2:
      model_->SelectAll(false);
      OnCaretBoundsChanged();
      break;
    default:
      NOTREACHED();
  }
  SchedulePaint();
}

bool NativeTextfieldViews::ImeEditingAllowed() const {
  // We don't allow the input method to retrieve or delete content from a
  // password field.
  ui::TextInputType t = GetTextInputType();
  return (t != ui::TEXT_INPUT_TYPE_NONE && t != ui::TEXT_INPUT_TYPE_PASSWORD);
}

// static
bool NativeTextfieldViews::ShouldInsertChar(char16 ch, int flags) {
  // Filter out all control characters, including tab and new line characters,
  // and all characters with Alt modifier. But we need to allow characters with
  // AltGr modifier.
  // On Windows AltGr is represented by Alt+Ctrl, and on Linux it's a different
  // flag that we don't care about.
  return ((ch >= 0x20 && ch < 0x7F) || ch > 0x9F) &&
      (flags & ~(ui::EF_SHIFT_DOWN | ui::EF_CAPS_LOCK_DOWN)) != ui::EF_ALT_DOWN;
}

void NativeTextfieldViews::CreateTouchSelectionControllerAndNotifyIt() {
  if (!touch_selection_controller_) {
    touch_selection_controller_.reset(
        ui::TouchSelectionController::create(this));
  }
  if (touch_selection_controller_)
    touch_selection_controller_->SelectionChanged();
}

void NativeTextfieldViews::PlatformGestureEventHandling(
    const ui::GestureEvent* event) {
#if defined(OS_WIN) && defined(USE_AURA)
  if (event->type() == ui::ET_GESTURE_TAP && !textfield_->read_only())
    base::win::DisplayVirtualKeyboard();
#endif
}

void NativeTextfieldViews::RevealObscuredChar(int index,
                                              const base::TimeDelta& duration) {
  GetRenderText()->SetObscuredRevealIndex(index);
  SchedulePaint();

  if (index != -1) {
    obscured_reveal_timer_.Start(
        FROM_HERE,
        duration,
        base::Bind(&NativeTextfieldViews::RevealObscuredChar,
                   base::Unretained(this), -1, base::TimeDelta()));
  }
}

}  // namespace views
