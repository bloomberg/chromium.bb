// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/tabbed_pane/tabbed_pane.h"

#include "base/logging.h"
#include "base/macros.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/default_style.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/widget/widget.h"

namespace views {

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

const int kHarmonyTabStripTabHeight = 32;
constexpr int kBorderThickness = 2;
constexpr int kBorderThicknessHorizontal = 16;

// The View containing the text for each tab in the tab strip.
class TabLabel : public Label {
 public:
  explicit TabLabel(const base::string16& tab_title)
      : Label(tab_title, style::CONTEXT_LABEL, style::STYLE_TAB_ACTIVE) {}

  // Label:
  void GetAccessibleNodeData(ui::AXNodeData* data) override {
    // views::Tab shouldn't expose any of its children in the a11y tree.
    // Instead, it should provide the a11y information itself. Normally,
    // non-keyboard-focusable children of keyboard-focusable parents are
    // ignored, but Tabs only mark the currently selected tab as
    // keyboard-focusable. This means all unselected Tabs expose their children
    // to the a11y tree. To fix, manually ignore the children.
    data->role = ui::AX_ROLE_IGNORED;
  }
};

}  // namespace

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
  gfx::Size CalculatePreferredSize() const override;
  void OnFocus() override;
  void OnBlur() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MdTab);
};

// A subclass of TabStrip that implements the Harmony visual styling. This
// class uses a BoxLayout to position tabs.
class MdTabStrip : public TabStrip, public gfx::AnimationDelegate {
 public:
  explicit MdTabStrip(TabbedPane::Orientation orientation);
  ~MdTabStrip() override;

  // Overridden from TabStrip:
  void OnSelectedTabChanged(Tab* from_tab, Tab* to_tab) override;

  // Overridden from View:
  void OnPaintBorder(gfx::Canvas* canvas) override;

  // Overridden from AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

 private:
  // Animations for expanding and contracting the selection bar. When changing
  // selections, the selection bar first grows to encompass both the old and new
  // selections, then shrinks to encompass only the new selection. The rates of
  // expansion and contraction each follow the cubic bezier curves used in
  // gfx::Tween; see MdTabStrip::OnPaintBorder for details.
  std::unique_ptr<gfx::LinearAnimation> expand_animation_;
  std::unique_ptr<gfx::LinearAnimation> contract_animation_;

  // The x-coordinate ranges of the old selection and the new selection.
  gfx::Range animating_from_;
  gfx::Range animating_to_;

  DISALLOW_COPY_AND_ASSIGN(MdTabStrip);
};

// static
const char Tab::kViewClassName[] = "Tab";

Tab::Tab(TabbedPane* tabbed_pane, const base::string16& title, View* contents)
    : tabbed_pane_(tabbed_pane),
      title_(new TabLabel(title)),
      tab_state_(TAB_ACTIVE),
      contents_(contents) {
  // Calculate this now while the font list is guaranteed to be bold.
  preferred_title_size_ = title_->GetPreferredSize();

  const int kTabVerticalPadding = 5;
  const int kTabHorizontalPadding = 10;

  if (!tabbed_pane_->IsHorizontal())
    title_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  SetBorder(CreateEmptyBorder(
      gfx::Insets(kTabVerticalPadding, kTabHorizontalPadding)));
  SetLayoutManager(std::make_unique<FillLayout>());

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

gfx::Size Tab::CalculatePreferredSize() const {
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

void Tab::GetAccessibleNodeData(ui::AXNodeData* data) {
  data->role = ui::AX_ROLE_TAB;
  data->SetName(title()->text());
  data->AddState(ui::AX_STATE_SELECTABLE);
  if (selected())
    data->AddState(ui::AX_STATE_SELECTED);
}

bool Tab::HandleAccessibleAction(const ui::AXActionData& action_data) {
  if (action_data.action != ui::AX_ACTION_SET_SELECTION || !enabled())
    return false;

  // It's not clear what should happen if a tab is 'deselected', so the
  // AX_ACTION_SET_SELECTION action will always select the tab.
  tabbed_pane_->SelectTab(this);
  return true;
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
  if (tabbed_pane->IsHorizontal()) {
    SetBorder(CreateEmptyBorder(gfx::Insets(kBorderThickness)));
  } else {
    SetBorder(CreateEmptyBorder(
        gfx::Insets(kBorderThickness, kBorderThicknessHorizontal)));
  }

  OnStateChanged();
}

MdTab::~MdTab() {}

void MdTab::OnStateChanged() {
  ui::NativeTheme* theme = GetNativeTheme();

  SkColor font_color =
      selected()
          ? theme->GetSystemColor(ui::NativeTheme::kColorId_TabTitleColorActive)
          : theme->GetSystemColor(
                ui::NativeTheme::kColorId_TabTitleColorInactive);
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

gfx::Size MdTab::CalculatePreferredSize() const {
  return gfx::Size(Tab::CalculatePreferredSize().width(),
                   kHarmonyTabStripTabHeight);
}

void MdTab::OnFocus() {
  SetBorder(CreateSolidBorder(
      GetInsets().top(),
      SkColorSetA(GetNativeTheme()->GetSystemColor(
                      ui::NativeTheme::kColorId_FocusedBorderColor),
                  0x66)));
  SchedulePaint();
}

void MdTab::OnBlur() {
  SetBorder(CreateEmptyBorder(GetInsets()));
  SchedulePaint();
}

// static
const char TabStrip::kViewClassName[] = "TabStrip";

TabStrip::TabStrip(TabbedPane::Orientation orientation)
    : orientation_(orientation) {
  const int kTabStripLeadingEdgePadding = 9;
  std::unique_ptr<BoxLayout> layout;
  if (IsHorizontal()) {
    layout = std::make_unique<BoxLayout>(
        BoxLayout::kHorizontal, gfx::Insets(0, kTabStripLeadingEdgePadding));
    layout->set_cross_axis_alignment(BoxLayout::CROSS_AXIS_ALIGNMENT_END);
  } else {
    layout = std::make_unique<BoxLayout>(
        BoxLayout::kVertical, gfx::Insets(kTabStripLeadingEdgePadding, 0));
    layout->set_cross_axis_alignment(BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  }
  layout->set_main_axis_alignment(BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  layout->SetDefaultFlex(0);
  SetLayoutManager(std::move(layout));
}

TabStrip::~TabStrip() {}

void TabStrip::OnSelectedTabChanged(Tab* from_tab, Tab* to_tab) {}

const char* TabStrip::GetClassName() const {
  return kViewClassName;
}

void TabStrip::OnPaintBorder(gfx::Canvas* canvas) {
  cc::PaintFlags fill_flags;
  fill_flags.setColor(kTabBorderColor);
  fill_flags.setStrokeWidth(kTabBorderThickness);
  SkScalar line_center_cross_axis;
  SkScalar line_end_main_axis;

  if (IsHorizontal()) {
    line_center_cross_axis =
        SkIntToScalar(height()) - (kTabBorderThickness / 2);
    line_end_main_axis = SkIntToScalar(width());
  } else {
    line_center_cross_axis = SkIntToScalar(width()) - (kTabBorderThickness / 2);
    line_end_main_axis = SkIntToScalar(height());
  }

  int selected_tab_index = GetSelectedTabIndex();
  if (selected_tab_index >= 0) {
    Tab* selected_tab = GetTabAtIndex(selected_tab_index);
    SkPath path;
    SkScalar tab_height =
        SkIntToScalar(selected_tab->height()) - kTabBorderThickness;
    SkScalar tab_width;

    SkScalar tab_start_x = SkIntToScalar(selected_tab->GetMirroredX());
    SkScalar tab_start_y = SkIntToScalar(selected_tab->bounds().y());
    if (IsHorizontal()) {
      tab_width = SkIntToScalar(selected_tab->width()) - kTabBorderThickness;
      path.moveTo(0, line_center_cross_axis);
      path.rLineTo(tab_start_x, 0);
      path.rLineTo(0, -tab_height);
      path.rLineTo(tab_width, 0);
      path.rLineTo(0, tab_height);
      path.lineTo(line_end_main_axis, line_center_cross_axis);
    } else {
      tab_width =
          SkIntToScalar(width() - selected_tab->GetInsets().left() / 2) -
          kTabBorderThickness;
      path.moveTo(line_center_cross_axis, 0);
      path.rLineTo(0, tab_start_y);
      path.rLineTo(-tab_width, 0);
      path.rLineTo(0, tab_height);
      path.rLineTo(tab_width, 0);
      path.lineTo(line_center_cross_axis, line_end_main_axis);
    }

    fill_flags.setStyle(cc::PaintFlags::kStroke_Style);
    canvas->DrawPath(path, fill_flags);
  } else {
    if (IsHorizontal())
      canvas->sk_canvas()->drawLine(0, line_center_cross_axis,
                                    line_end_main_axis, line_center_cross_axis,
                                    fill_flags);
    else
      canvas->sk_canvas()->drawLine(line_center_cross_axis, 0,
                                    line_center_cross_axis, line_end_main_axis,
                                    fill_flags);
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

MdTabStrip::MdTabStrip(TabbedPane::Orientation orientation)
    : TabStrip(orientation) {
  auto layout = std::make_unique<BoxLayout>(
      IsHorizontal() ? BoxLayout::kHorizontal : BoxLayout::kVertical);
  layout->set_main_axis_alignment(BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  layout->set_cross_axis_alignment(BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
  layout->SetDefaultFlex(1);
  SetLayoutManager(std::move(layout));

  // These durations are taken from the Paper Tabs source:
  // https://github.com/PolymerElements/paper-tabs/blob/master/paper-tabs.html
  // See |selectionBar.expand| and |selectionBar.contract|.
  expand_animation_.reset(new gfx::LinearAnimation(this));
  expand_animation_->SetDuration(base::TimeDelta::FromMilliseconds(150));

  contract_animation_.reset(new gfx::LinearAnimation(this));
  contract_animation_->SetDuration(base::TimeDelta::FromMilliseconds(180));
}

MdTabStrip::~MdTabStrip() {}

void MdTabStrip::OnSelectedTabChanged(Tab* from_tab, Tab* to_tab) {
  DCHECK(!from_tab->selected());
  DCHECK(to_tab->selected());

  if (IsHorizontal()) {
    animating_from_ = gfx::Range(from_tab->GetMirroredX(),
                                 from_tab->GetMirroredX() + from_tab->width());
    animating_to_ = gfx::Range(to_tab->GetMirroredX(),
                               to_tab->GetMirroredX() + to_tab->width());
  } else {
    animating_from_ = gfx::Range(from_tab->bounds().y(),
                                 from_tab->bounds().y() + from_tab->height());
    animating_to_ = gfx::Range(to_tab->bounds().y(),
                               to_tab->bounds().y() + to_tab->height());
  }

  contract_animation_->Stop();
  expand_animation_->Start();
}

void MdTabStrip::OnPaintBorder(gfx::Canvas* canvas) {
  const int kUnselectedBorderThickness = 1;
  const int kSelectedBorderThickness = 2;

  int max_cross_axis;

  // First, draw the unselected border across the TabStrip's entire width or
  // height, depending on the orientation of the tab alignment. The area
  // underneath or on the right of the selected tab will be overdrawn later.
  gfx::Rect rect;
  if (IsHorizontal()) {
    max_cross_axis = child_at(0)->y() + child_at(0)->height();
    rect = gfx::Rect(0, max_cross_axis - kUnselectedBorderThickness, width(),
                     kUnselectedBorderThickness);
  } else {
    max_cross_axis = child_at(0)->x() + child_at(0)->width();
    rect = gfx::Rect(max_cross_axis - kUnselectedBorderThickness, 0,
                     kUnselectedBorderThickness, height());
  }
  canvas->FillRect(rect, GetNativeTheme()->GetSystemColor(
                             ui::NativeTheme::kColorId_TabBottomBorder));

  int min_main_axis = 0;
  int max_main_axis = 0;

  // Now, figure out the range to draw the selection marker underneath. There
  // are three states here:
  // 1) Expand animation is running: use FAST_OUT_LINEAR_IN to grow the
  //    selection marker until it encompasses both the previously selected tab
  //    and the currently selected tab;
  // 2) Contract animation is running: use LINEAR_OUT_SLOW_IN to shrink the
  //    selection marker until it encompasses only the currently selected tab;
  // 3) No animations running: the selection marker is only under the currently
  //    selected tab.
  Tab* tab = GetSelectedTab();
  if (!tab)
    return;
  if (expand_animation_->is_animating()) {
    bool animating_leading = animating_to_.start() < animating_from_.start();
    double anim_value = gfx::Tween::CalculateValue(
        gfx::Tween::FAST_OUT_LINEAR_IN, expand_animation_->GetCurrentValue());

    if (animating_leading) {
      min_main_axis = gfx::Tween::IntValueBetween(
          anim_value, animating_from_.start(), animating_to_.start());
      max_main_axis = animating_from_.end();
    } else {
      min_main_axis = animating_from_.start();
      max_main_axis = gfx::Tween::IntValueBetween(
          anim_value, animating_from_.end(), animating_to_.end());
    }
  } else if (contract_animation_->is_animating()) {
    bool animating_leading = animating_to_.start() < animating_from_.start();
    double anim_value = gfx::Tween::CalculateValue(
        gfx::Tween::LINEAR_OUT_SLOW_IN, contract_animation_->GetCurrentValue());
    if (animating_leading) {
      min_main_axis = animating_to_.start();
      max_main_axis = gfx::Tween::IntValueBetween(
          anim_value, animating_from_.end(), animating_to_.end());
    } else {
      min_main_axis = gfx::Tween::IntValueBetween(
          anim_value, animating_from_.start(), animating_to_.start());
      max_main_axis = animating_to_.end();
    }
  } else if (tab) {
    if (IsHorizontal()) {
      min_main_axis = tab->GetMirroredX();
      max_main_axis = tab->GetMirroredX() + tab->width();
    } else {
      min_main_axis = tab->bounds().y();
      max_main_axis = tab->bounds().y() + tab->height();
    }
  }

  DCHECK(min_main_axis != max_main_axis);
  // Draw over the unselected border from above.
  if (IsHorizontal()) {
    rect = gfx::Rect(min_main_axis, max_cross_axis - kSelectedBorderThickness,
                     max_main_axis - min_main_axis, kSelectedBorderThickness);
  } else {
    rect = gfx::Rect(max_cross_axis - kSelectedBorderThickness, min_main_axis,
                     kSelectedBorderThickness, max_main_axis - min_main_axis);
  }
  canvas->FillRect(rect, GetNativeTheme()->GetSystemColor(
                             ui::NativeTheme::kColorId_FocusedBorderColor));
}

void MdTabStrip::AnimationProgressed(const gfx::Animation* animation) {
  SchedulePaint();
}

void MdTabStrip::AnimationEnded(const gfx::Animation* animation) {
  if (animation == expand_animation_.get())
    contract_animation_->Start();
}

TabbedPane::TabbedPane(TabbedPane::Orientation orientation)
    : listener_(NULL), contents_(new View()) {
  tab_strip_ = ui::MaterialDesignController::IsSecondaryUiMaterial()
                   ? new MdTabStrip(orientation)
                   : new TabStrip(orientation);
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
    tab_strip_->OnSelectedTabChanged(old_selected_tab, new_selected_tab);
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

gfx::Size TabbedPane::CalculatePreferredSize() const {
  gfx::Size size;
  for (int i = 0; i < contents_->child_count(); ++i)
    size.SetToMax(contents_->child_at(i)->GetPreferredSize());
  if (IsHorizontal())
    size.Enlarge(0, tab_strip_->GetPreferredSize().height());
  else
    size.Enlarge(tab_strip_->GetPreferredSize().width(), 0);
  return size;
}

bool TabbedPane::IsHorizontal() const {
  return tab_strip_->IsHorizontal();
}

Tab* TabbedPane::GetSelectedTab() {
  return tab_strip_->GetSelectedTab();
}

View* TabbedPane::GetSelectedTabContentView() {
  return GetSelectedTab() ? GetSelectedTab()->contents() : nullptr;
}

bool TabbedPane::MoveSelectionBy(int delta) {
  if (contents_->child_count() <= 1)
    return false;
  SelectTab(tab_strip_->GetTabAtDeltaFromSelected(delta));
  return true;
}

void TabbedPane::Layout() {
  const gfx::Size size = tab_strip_->GetPreferredSize();
  if (IsHorizontal()) {
    tab_strip_->SetBounds(0, 0, width(), size.height());
    contents_->SetBounds(0, tab_strip_->bounds().bottom(), width(),
                         std::max(0, height() - size.height()));
  } else {
    tab_strip_->SetBounds(0, 0, size.width(), height());
    contents_->SetBounds(tab_strip_->bounds().width(), 0,
                         std::max(0, width() - size.width()), height());
  }
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

void TabbedPane::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ui::AX_ROLE_TAB_LIST;
}

}  // namespace views
