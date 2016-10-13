// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/tabbed_pane/tabbed_pane.h"

#include "base/logging.h"
#include "base/macros.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/default_style.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/widget/widget.h"

namespace {

// TODO(markusheintz|msw): Use NativeTheme colors.
const SkColor kTabTitleColor_Inactive = SkColorSetRGB(0x64, 0x64, 0x64);
const SkColor kTabTitleColor_Active = SK_ColorBLACK;
const SkColor kTabTitleColor_Hovered = SK_ColorBLACK;
const SkColor kTabBorderColor = SkColorSetRGB(0xC8, 0xC8, 0xC8);
const SkScalar kTabBorderThickness = 1.0f;

const gfx::Font::Weight kHoverWeight = gfx::Font::Weight::NORMAL;
const gfx::Font::Weight kActiveWeight = gfx::Font::Weight::BOLD;
const gfx::Font::Weight kInactiveWeight = gfx::Font::Weight::NORMAL;

const int kHarmonyTabStripVerticalPad = 16;
const int kHarmonyTabStripTabHeight = 40;

}  // namespace

namespace views {

// static
const char TabbedPane::kViewClassName[] = "TabbedPane";

// A subclass of Tab that implements the Harmony visual styling.
class MdTab : public Tab {
 public:
  MdTab(TabbedPane* tabbed_pane, const base::string16& title, View* contents);
  ~MdTab() override;

  // Overridden from Tab:
  void OnStateChanged() override;

  // Overridden from View:
  gfx::Size GetPreferredSize() const override;
  void OnPaintBorder(gfx::Canvas* canvas) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MdTab);
};

// The tab strip shown above the tab contents.
class TabStrip : public View {
 public:
  // Internal class name.
  static const char kViewClassName[];

  TabStrip();
  ~TabStrip() override;

  // Overridden from View:
  const char* GetClassName() const override;
  void OnPaintBorder(gfx::Canvas* canvas) override;

  Tab* GetSelectedTab() const;
  Tab* GetTabAtDeltaFromSelected(int delta) const;
  Tab* GetTabAtIndex(int index) const;
  int GetSelectedTabIndex() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabStrip);
};

// A subclass of TabStrip that implements the Harmony visual styling. This
// class uses a BoxLayout to position tabs.
class MdTabStrip : public TabStrip {
 public:
  MdTabStrip();
  ~MdTabStrip() override;

  // Overridden from View:
  void OnPaintBorder(gfx::Canvas* canvas) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MdTabStrip);
};

// static
const char Tab::kViewClassName[] = "Tab";

Tab::Tab(TabbedPane* tabbed_pane, const base::string16& title, View* contents)
    : tabbed_pane_(tabbed_pane),
      title_(new Label(
          title,
          ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(
              ui::kLabelFontSizeDelta,
              gfx::Font::NORMAL,
              kActiveWeight))),
      tab_state_(TAB_ACTIVE),
      contents_(contents) {
  // Calculate this now while the font list is guaranteed to be bold.
  preferred_title_size_ = title_->GetPreferredSize();

  const int kTabVerticalPadding = 5;
  const int kTabHorizontalPadding = 10;

  SetBorder(Border::CreateEmptyBorder(
      gfx::Insets(kTabVerticalPadding, kTabHorizontalPadding)));
  SetLayoutManager(new FillLayout);

  SetState(TAB_INACTIVE);
  AddChildView(title_);
}

Tab::~Tab() {}

void Tab::SetSelected(bool selected) {
  contents_->SetVisible(selected);
  SetState(selected ? TAB_ACTIVE : TAB_INACTIVE);
#if defined(OS_MACOSX)
  SetFocusBehavior(selected ? FocusBehavior::ACCESSIBLE_ONLY
                            : FocusBehavior::NEVER);
#else
  SetFocusBehavior(selected ? FocusBehavior::ALWAYS : FocusBehavior::NEVER);
#endif
}

void Tab::OnStateChanged() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  switch (tab_state_) {
    case TAB_INACTIVE:
      title_->SetEnabledColor(kTabTitleColor_Inactive);
      title_->SetFontList(rb.GetFontListWithDelta(
          ui::kLabelFontSizeDelta, gfx::Font::NORMAL, kInactiveWeight));
      break;
    case TAB_ACTIVE:
      title_->SetEnabledColor(kTabTitleColor_Active);
      title_->SetFontList(rb.GetFontListWithDelta(
          ui::kLabelFontSizeDelta, gfx::Font::NORMAL, kActiveWeight));
      break;
    case TAB_HOVERED:
      title_->SetEnabledColor(kTabTitleColor_Hovered);
      title_->SetFontList(rb.GetFontListWithDelta(
          ui::kLabelFontSizeDelta, gfx::Font::NORMAL, kHoverWeight));
      break;
  }
}

bool Tab::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() &&
      GetLocalBounds().Contains(event.location()))
    tabbed_pane_->SelectTab(this);
  return true;
}

void Tab::OnMouseEntered(const ui::MouseEvent& event) {
  SetState(selected() ? TAB_ACTIVE : TAB_HOVERED);
}

void Tab::OnMouseExited(const ui::MouseEvent& event) {
  SetState(selected() ? TAB_ACTIVE : TAB_INACTIVE);
}

void Tab::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      // Fallthrough.
    case ui::ET_GESTURE_TAP:
      // SelectTab also sets the right tab color.
      tabbed_pane_->SelectTab(this);
      break;
    case ui::ET_GESTURE_TAP_CANCEL:
      SetState(selected() ? TAB_ACTIVE : TAB_INACTIVE);
      break;
    default:
      break;
  }
  event->SetHandled();
}

gfx::Size Tab::GetPreferredSize() const {
  gfx::Size size(preferred_title_size_);
  size.Enlarge(GetInsets().width(), GetInsets().height());
  return size;
}

const char* Tab::GetClassName() const {
  return kViewClassName;
}

void Tab::SetState(TabState tab_state) {
  if (tab_state == tab_state_)
    return;
  tab_state_ = tab_state;
  OnStateChanged();
  SchedulePaint();
}

void Tab::OnFocus() {
  OnStateChanged();
  // When the tab gains focus, send an accessibility event indicating that the
  // contents are focused. When the tab loses focus, whichever new View ends up
  // with focus will send an AX_EVENT_FOCUS of its own, so there's no need to
  // send one in OnBlur().
  if (contents())
    contents()->NotifyAccessibilityEvent(ui::AX_EVENT_FOCUS, true);
  SchedulePaint();
}

void Tab::OnBlur() {
  OnStateChanged();
  SchedulePaint();
}

bool Tab::OnKeyPressed(const ui::KeyEvent& event) {
  ui::KeyboardCode key = event.key_code();
  if (key != ui::VKEY_LEFT && key != ui::VKEY_RIGHT)
    return false;
  return tabbed_pane_->MoveSelectionBy(key == ui::VKEY_RIGHT ? 1 : -1);
}

MdTab::MdTab(TabbedPane* tabbed_pane,
             const base::string16& title,
             View* contents)
    : Tab(tabbed_pane, title, contents) {
  OnStateChanged();
}

MdTab::~MdTab() {}

void MdTab::OnStateChanged() {
  ui::NativeTheme* theme = GetNativeTheme();

  SkColor font_color = selected()
      ? theme->GetSystemColor(ui::NativeTheme::kColorId_ProminentButtonColor)
      : theme->GetSystemColor(ui::NativeTheme::kColorId_ButtonEnabledColor);
  title()->SetEnabledColor(font_color);

  gfx::Font::Weight font_weight = gfx::Font::Weight::MEDIUM;
#if defined(OS_WIN)
  if (selected())
    font_weight = gfx::Font::Weight::BOLD;
#endif

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  title()->SetFontList(rb.GetFontListWithDelta(ui::kLabelFontSizeDelta,
                                               gfx::Font::NORMAL, font_weight));
}

void MdTab::OnPaintBorder(gfx::Canvas* canvas) {
  const int kBorderStrokeWidth = 2;
  if (!HasFocus()) {
    SkColor color = GetNativeTheme()->GetSystemColor(
        selected() ? ui::NativeTheme::kColorId_FocusedBorderColor
                   : ui::NativeTheme::kColorId_UnfocusedBorderColor);
    int thickness = selected() ? kBorderStrokeWidth : kBorderStrokeWidth / 2;
    canvas->FillRect(gfx::Rect(0, height() - thickness, width(), thickness),
                     color);
    return;
  }

  // TODO(ellyjones): should this 0x66 be part of NativeTheme somehow?
  SkColor base_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_FocusedBorderColor);
  SkColor light_color = SkColorSetA(base_color, 0x66);

  SkPaint paint;
  paint.setColor(light_color);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(kBorderStrokeWidth);

  gfx::RectF bounds = gfx::RectF(GetLocalBounds());
  bounds.Inset(gfx::Insets(kBorderStrokeWidth / 2.f));

  // Draw the lighter-colored stroke first, then draw the heavier stroke over
  // the bottom of it. This is fine because the heavier stroke has 1.0 alpha, so
  // the lighter stroke won't show through.
  canvas->DrawRect(bounds, paint);
  canvas->FillRect(
      gfx::Rect(0, height() - kBorderStrokeWidth, width(), kBorderStrokeWidth),
      base_color);
}

gfx::Size MdTab::GetPreferredSize() const {
  return gfx::Size(Tab::GetPreferredSize().width(), kHarmonyTabStripTabHeight);
}

// static
const char TabStrip::kViewClassName[] = "TabStrip";

TabStrip::TabStrip() {
  const int kTabStripLeadingEdgePadding = 9;
  BoxLayout* layout =
      new BoxLayout(BoxLayout::kHorizontal, kTabStripLeadingEdgePadding, 0, 0);
  layout->set_main_axis_alignment(BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  layout->set_cross_axis_alignment(BoxLayout::CROSS_AXIS_ALIGNMENT_END);
  layout->SetDefaultFlex(0);
  SetLayoutManager(layout);
}

TabStrip::~TabStrip() {}

const char* TabStrip::GetClassName() const {
  return kViewClassName;
}

void TabStrip::OnPaintBorder(gfx::Canvas* canvas) {
  SkPaint paint;
  paint.setColor(kTabBorderColor);
  paint.setStrokeWidth(kTabBorderThickness);
  SkScalar line_y = SkIntToScalar(height()) - (kTabBorderThickness / 2);
  SkScalar line_end = SkIntToScalar(width());
  int selected_tab_index = GetSelectedTabIndex();
  if (selected_tab_index >= 0) {
    Tab* selected_tab = GetTabAtIndex(selected_tab_index);
    SkPath path;
    SkScalar tab_height =
        SkIntToScalar(selected_tab->height()) - kTabBorderThickness;
    SkScalar tab_width =
        SkIntToScalar(selected_tab->width()) - kTabBorderThickness;
    SkScalar tab_start = SkIntToScalar(selected_tab->GetMirroredX());
    path.moveTo(0, line_y);
    path.rLineTo(tab_start, 0);
    path.rLineTo(0, -tab_height);
    path.rLineTo(tab_width, 0);
    path.rLineTo(0, tab_height);
    path.lineTo(line_end, line_y);

    SkPaint paint;
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(kTabBorderColor);
    paint.setStrokeWidth(kTabBorderThickness);
    canvas->DrawPath(path, paint);
  } else {
    canvas->sk_canvas()->drawLine(0, line_y, line_end, line_y, paint);
  }
}

Tab* TabStrip::GetTabAtIndex(int index) const {
  return static_cast<Tab*>(const_cast<View*>(child_at(index)));
}

int TabStrip::GetSelectedTabIndex() const {
  for (int i = 0; i < child_count(); ++i)
    if (GetTabAtIndex(i)->selected())
      return i;
  return -1;
}

Tab* TabStrip::GetSelectedTab() const {
  int index = GetSelectedTabIndex();
  return index >= 0 ? GetTabAtIndex(index) : nullptr;
}

Tab* TabStrip::GetTabAtDeltaFromSelected(int delta) const {
  int index = (GetSelectedTabIndex() + delta) % child_count();
  if (index < 0)
    index += child_count();
  return GetTabAtIndex(index);
}

MdTabStrip::MdTabStrip() {
  BoxLayout* layout =
      new BoxLayout(BoxLayout::kHorizontal, 0, kHarmonyTabStripVerticalPad, 0);
  layout->set_main_axis_alignment(BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  layout->set_cross_axis_alignment(BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
  layout->SetDefaultFlex(1);
  SetLayoutManager(layout);
}

MdTabStrip::~MdTabStrip() {}

// The tab strip "border" is drawn as part of the tabs.
void MdTabStrip::OnPaintBorder(gfx::Canvas* canvas) {}

TabbedPane::TabbedPane()
    : listener_(NULL),
      tab_strip_(ui::MaterialDesignController::IsSecondaryUiMaterial()
                     ? new MdTabStrip
                     : new TabStrip),
      contents_(new View()) {
  AddChildView(tab_strip_);
  AddChildView(contents_);
}

TabbedPane::~TabbedPane() {}

int TabbedPane::GetSelectedTabIndex() const {
  return tab_strip_->GetSelectedTabIndex();
}

int TabbedPane::GetTabCount() {
  DCHECK_EQ(tab_strip_->child_count(), contents_->child_count());
  return contents_->child_count();
}

void TabbedPane::AddTab(const base::string16& title, View* contents) {
  AddTabAtIndex(tab_strip_->child_count(), title, contents);
}

void TabbedPane::AddTabAtIndex(int index,
                               const base::string16& title,
                               View* contents) {
  DCHECK(index >= 0 && index <= GetTabCount());
  contents->SetVisible(false);

  tab_strip_->AddChildViewAt(
      ui::MaterialDesignController::IsSecondaryUiMaterial()
          ? new MdTab(this, title, contents)
          : new Tab(this, title, contents),
      index);
  contents_->AddChildViewAt(contents, index);
  if (!GetSelectedTab())
    SelectTabAt(index);

  PreferredSizeChanged();
}

void TabbedPane::SelectTab(Tab* new_selected_tab) {
  Tab* old_selected_tab = tab_strip_->GetSelectedTab();
  if (old_selected_tab == new_selected_tab)
    return;

  new_selected_tab->SetSelected(true);
  if (old_selected_tab) {
    if (old_selected_tab->HasFocus())
      new_selected_tab->RequestFocus();
    old_selected_tab->SetSelected(false);
  }
  tab_strip_->SchedulePaint();

  FocusManager* focus_manager = new_selected_tab->contents()->GetFocusManager();
  if (focus_manager) {
    const View* focused_view = focus_manager->GetFocusedView();
    if (focused_view && contents_->Contains(focused_view) &&
        !new_selected_tab->contents()->Contains(focused_view))
      focus_manager->SetFocusedView(new_selected_tab->contents());
  }

  if (listener())
    listener()->TabSelectedAt(tab_strip_->GetIndexOf(new_selected_tab));
}

void TabbedPane::SelectTabAt(int index) {
  Tab* tab = tab_strip_->GetTabAtIndex(index);
  if (tab)
    SelectTab(tab);
}

gfx::Size TabbedPane::GetPreferredSize() const {
  gfx::Size size;
  for (int i = 0; i < contents_->child_count(); ++i)
    size.SetToMax(contents_->child_at(i)->GetPreferredSize());
  size.Enlarge(0, tab_strip_->GetPreferredSize().height());
  return size;
}

Tab* TabbedPane::GetSelectedTab() {
  return tab_strip_->GetSelectedTab();
}

bool TabbedPane::MoveSelectionBy(int delta) {
  if (contents_->child_count() <= 1)
    return false;
  SelectTab(tab_strip_->GetTabAtDeltaFromSelected(delta));
  return true;
}

void TabbedPane::Layout() {
  const gfx::Size size = tab_strip_->GetPreferredSize();
  tab_strip_->SetBounds(0, 0, width(), size.height());
  contents_->SetBounds(0, tab_strip_->bounds().bottom(), width(),
                       std::max(0, height() - size.height()));
  for (int i = 0; i < contents_->child_count(); ++i)
    contents_->child_at(i)->SetSize(contents_->size());
}

void TabbedPane::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add) {
    // Support navigating tabs by Ctrl+Tab and Ctrl+Shift+Tab.
    AddAccelerator(ui::Accelerator(ui::VKEY_TAB,
                                   ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN));
    AddAccelerator(ui::Accelerator(ui::VKEY_TAB, ui::EF_CONTROL_DOWN));
  }
}

bool TabbedPane::AcceleratorPressed(const ui::Accelerator& accelerator) {
  // Handle Ctrl+Tab and Ctrl+Shift+Tab navigation of pages.
  DCHECK(accelerator.key_code() == ui::VKEY_TAB && accelerator.IsCtrlDown());
  return MoveSelectionBy(accelerator.IsShiftDown() ? -1 : 1);
}

const char* TabbedPane::GetClassName() const {
  return kViewClassName;
}

View* TabbedPane::GetSelectedTabContentView() {
  return GetSelectedTab() ? GetSelectedTab()->contents() : nullptr;
}

void TabbedPane::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_TAB_LIST;
}

}  // namespace views
