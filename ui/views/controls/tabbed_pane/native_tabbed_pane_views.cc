// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/tabbed_pane/native_tabbed_pane_views.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace views {

const SkColor kTabTitleColor_Inactive = SkColorSetRGB(0x66, 0x66, 0x66);
const SkColor kTabTitleColor_Active = SkColorSetRGB(0x20, 0x20, 0x20);
const SkColor kTabTitleColor_Pressed = SkColorSetRGB(0x33, 0x33, 0x33);
const SkColor kTabTitleColor_Hovered = SkColorSetRGB(0x22, 0x22, 0x22);
// TODO(markusheintz): The tab background color should be provided by the
// NativeTheme.
const SkColor kTabBackgroundColor_Active = SK_ColorWHITE;
const SkColor kTabBorderColor = SkColorSetRGB(0xCC, 0xCC, 0xCC);
const SkScalar kTabBorderThickness = 1.0f;
const SkScalar kTabBorderRadius = 2.0f;

class TabStrip;

class Tab : public View {
 public:
  Tab(TabStrip* tab_strip, const string16& title, View* contents)
      : tab_strip_(tab_strip),
        title_(title),
        contents_(contents),
        title_color_(kTabTitleColor_Inactive) {}
  virtual ~Tab() {}

  static int GetMinimumTabHeight() {
    return gfx::Font().GetHeight() + 10;
  }

  static View* GetContents(View* tab) {
    return static_cast<Tab*>(tab)->contents_;
  }

  void OnSelectedStateChanged(bool selected);

  // Overridden from View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    const int kTabMinWidth = 54;
    gfx::Size ps(GetTabTitleFont().GetStringWidth(title_),
                 GetMinimumTabHeight());
    ps.Enlarge(30, 10);
    if (ps.width() < kTabMinWidth)
      ps.set_width(kTabMinWidth);
    return ps;
  }

  // Overridden from ui::EventHandler.
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

 private:
  void PaintTabBackground(gfx::Canvas* canvas) {
    // Fill the background. Note that we don't constrain to the bounds as
    // canvas is already clipped for us.
    canvas->DrawColor(kTabBackgroundColor_Active);
  }

  void PaintTabBorder(gfx::Canvas* canvas) {
    SkPath path;
    SkRect bounds = { 0, 0, SkIntToScalar(width()), SkIntToScalar(height()) };
    SkScalar radii[8] = { kTabBorderRadius, kTabBorderRadius,
                          kTabBorderRadius, kTabBorderRadius,
                          0, 0,
                          0, 0 };
    path.addRoundRect(bounds, radii, SkPath::kCW_Direction);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(kTabBorderColor);
    paint.setStrokeWidth(kTabBorderThickness * 2);

    canvas->DrawPath(path, paint);
  }

  void PaintTabTitle(gfx::Canvas* canvas, bool selected) {
    int text_width = GetTabTitleFont().GetStringWidth(title_);
    int text_height = GetTabTitleFont().GetHeight();
    int text_x = (width() - text_width) / 2;
    int text_y = 5;
    gfx::Rect text_rect(text_x, text_y, text_width, text_height);
    canvas->DrawStringInt(title_, GetTabTitleFont(), title_color_, text_rect);
  }

  void SetTitleColor(SkColor color) {
    title_color_ = color;
    SchedulePaint();
  }

  const gfx::Font& GetTabTitleFont() {
    static gfx::Font* title_font = NULL;
    if (!title_font)
      title_font = new gfx::Font(gfx::Font().DeriveFont(0, gfx::Font::BOLD));
    return *title_font;
  }

  TabStrip* tab_strip_;
  string16 title_;
  View* contents_;
  SkColor title_color_;

  DISALLOW_COPY_AND_ASSIGN(Tab);
};

class TabStrip : public View {
 public:
  explicit TabStrip(NativeTabbedPaneViews* owner)
      : owner_(owner),
        selected_tab_(NULL) {
    const int kCount = 4;
    // The gradient colors are derived from the tabbed panes used for the
    // WebUI.
    SkColor colors[] = {
        SkColorSetRGB(0xff, 0xff, 0xff),
        SkColorSetRGB(0xff, 0xff, 0xff),
        SkColorSetRGB(0xfa, 0xfa, 0xfa),
        SkColorSetRGB(0xf2, 0xf2, 0xf2)
    };
    // The relative positions of the gradient colors are derived from the
    // tabbed panes used for the WebUI.
    SkScalar pos[4] = {0.0f, 0.6f, 0.8f, 1.0f};
    set_background(Background::CreateVerticalMultiColorGradientBackground(
        colors, pos, kCount));
  }
  virtual ~TabStrip() {}

  void SelectTab(View* tab) {
    if (tab == selected_tab_)
      return;
    if (selected_tab_)
      static_cast<Tab*>(selected_tab_)->OnSelectedStateChanged(false);
    selected_tab_ = tab;
    static_cast<Tab*>(selected_tab_)->OnSelectedStateChanged(true);
    owner_->TabSelectionChanged(tab);
  }

  bool IsTabSelected(const View* tab) const {
    return tab == selected_tab_;
  }

  int GetSelectedIndex() const {
    return GetIndexOf(selected_tab_);
  }

  View* selected_tab() { return selected_tab_; }

  View* RemoveTabAt(int index) {
    View* contents = Tab::GetContents(child_at(index));
    delete child_at(index);
    return contents;
  }

  // Overridden from View:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(50, Tab::GetMinimumTabHeight());
  }
  virtual void Layout() OVERRIDE {
    const int kTabOffset = 18;
    int x = kTabOffset;  // Layout tabs with an offset to the tabstrip border.
    for (int i = 0; i < child_count(); ++i) {
      gfx::Size ps = child_at(i)->GetPreferredSize();
      child_at(i)->SetBounds(x, 0, ps.width(), ps.height());
      x = child_at(i)->bounds().right();
    }
  }
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    OnPaintBackground(canvas);

    // Draw the TabStrip border.
    SkPaint paint;
    paint.setColor(kTabBorderColor);
    paint.setStrokeWidth(kTabBorderThickness);
    SkScalar line_y = SkIntToScalar(height()) - kTabBorderThickness;
    SkScalar line_width = SkIntToScalar(width());
    canvas->sk_canvas()->drawLine(0, line_y, line_width, line_y, paint);
  }

 private:
  NativeTabbedPaneViews* owner_;
  View* selected_tab_;

  DISALLOW_COPY_AND_ASSIGN(TabStrip);
};

void Tab::OnSelectedStateChanged(bool selected) {
  SetTitleColor(selected ? kTabTitleColor_Active : kTabTitleColor_Inactive);
}

void Tab::OnPaint(gfx::Canvas* canvas) {
  bool selected = tab_strip_->IsTabSelected(this);
  if (selected) {
    PaintTabBackground(canvas);
    PaintTabBorder(canvas);
  }
  PaintTabTitle(canvas, selected);
}

bool Tab::OnMousePressed(const ui::MouseEvent& event) {
  SetTitleColor(kTabTitleColor_Pressed);
  return true;
}

void Tab::OnMouseReleased(const ui::MouseEvent& event) {
  SetTitleColor(kTabTitleColor_Hovered);
  tab_strip_->SelectTab(this);
}

void Tab::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      SetTitleColor(kTabTitleColor_Pressed);
      break;
    case ui::ET_GESTURE_TAP:
      // SelectTab also sets the right tab color.
      tab_strip_->SelectTab(this);
      break;
    case ui::ET_GESTURE_TAP_CANCEL:
      SetTitleColor(tab_strip_->IsTabSelected(this) ? kTabTitleColor_Active :
          kTabTitleColor_Inactive);
      break;
    default:
      break;
  }
  event->SetHandled();
}

void Tab::OnMouseCaptureLost() {
  SetTitleColor(kTabTitleColor_Inactive);
}

void Tab::OnMouseEntered(const ui::MouseEvent& event) {
  SetTitleColor(tab_strip_->IsTabSelected(this) ? kTabTitleColor_Active :
      kTabTitleColor_Hovered);
}

void Tab::OnMouseExited(const ui::MouseEvent& event) {
  SetTitleColor(tab_strip_->IsTabSelected(this) ? kTabTitleColor_Active :
      kTabTitleColor_Inactive);
}

// Custom layout manager that takes care of sizing and displaying the tab pages.
class TabLayout : public LayoutManager {
 public:
  TabLayout() {}

  // Switches to the tab page identified.
  void SwitchToPage(View* host, View* page) {
    for (int i = 0; i < host->child_count(); ++i) {
      View* child = host->child_at(i);
      // The child might not have been laid out yet.
      if (child == page)
        child->SetBoundsRect(host->GetContentsBounds());
      child->SetVisible(child == page);
    }

    FocusManager* focus_manager = page->GetFocusManager();
    if (focus_manager) {
      const View* focused_view = focus_manager->GetFocusedView();
      if (focused_view && host->Contains(focused_view) &&
          !page->Contains(focused_view))
        focus_manager->SetFocusedView(page);
    }
  }

 private:
  // LayoutManager overrides:
  virtual void Layout(View* host) OVERRIDE {
    gfx::Rect bounds(host->GetContentsBounds());
    for (int i = 0; i < host->child_count(); ++i) {
      View* child = host->child_at(i);
      // We only layout visible children, since it may be expensive.
      if (child->visible() && child->bounds() != bounds)
        child->SetBoundsRect(bounds);
    }
  }

  virtual gfx::Size GetPreferredSize(View* host) OVERRIDE {
    // First, query the preferred sizes to determine a good width.
    int width = 0;
    for (int i = 0; i < host->child_count(); ++i) {
      View* page = host->child_at(i);
      width = std::max(width, page->GetPreferredSize().width());
    }
    // After we know the width, decide on the height.
    return gfx::Size(width, GetPreferredHeightForWidth(host, width));
  }

  virtual int GetPreferredHeightForWidth(View* host, int width) OVERRIDE {
    int height = 0;
    for (int i = 0; i < host->child_count(); ++i) {
      View* page = host->child_at(i);
      height = std::max(height, page->GetHeightForWidth(width));
    }
    return height;
  }

  DISALLOW_COPY_AND_ASSIGN(TabLayout);
};

////////////////////////////////////////////////////////////////////////////////
// NativeTabbedPaneViews, public:

NativeTabbedPaneViews::NativeTabbedPaneViews(TabbedPane* tabbed_pane)
    : tabbed_pane_(tabbed_pane),
      tab_layout_manager_(new TabLayout),
      ALLOW_THIS_IN_INITIALIZER_LIST(tab_strip_(new TabStrip(this))),
      content_view_(new views::View) {
  AddChildView(tab_strip_);
  AddChildView(content_view_);
  InitControl();
}

NativeTabbedPaneViews::~NativeTabbedPaneViews() {
}

void NativeTabbedPaneViews::TabSelectionChanged(View* selected) {
  tab_layout_manager_->SwitchToPage(content_view_, Tab::GetContents(selected));
  if (tabbed_pane_->listener())
    tabbed_pane_->listener()->TabSelectedAt(tab_strip_->GetIndexOf(selected));
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabbedPaneViews, NativeTabbedPaneWrapper implementation:

void NativeTabbedPaneViews::AddTab(const string16& title, View* contents) {
  AddTabAtIndex(tab_strip_->child_count(), title, contents, true);
}

void NativeTabbedPaneViews::AddTabAtIndex(int index,
                                          const string16& title,
                                          View* contents,
                                          bool select_if_first_tab) {
  DCHECK(index <= static_cast<int>(tab_strip_->child_count()));
  contents->set_owned_by_client();
  contents->SetVisible(false);

  tab_strip_->AddChildViewAt(new Tab(tab_strip_, title, contents), index);

  // Add native tab only if the native control is already created.
  content_view_->AddChildViewAt(contents, index);

  // Switch to the newly added tab if requested;
  if (tab_strip_->child_count() == 1 && select_if_first_tab)
    tab_layout_manager_->SwitchToPage(content_view_, contents);
  if (!tab_strip_->selected_tab())
    tab_strip_->SelectTab(tab_strip_->child_at(0));
}

View* NativeTabbedPaneViews::RemoveTabAtIndex(int index) {
  DCHECK(index >= 0 && index < tab_strip_->child_count());

  if (index < (tab_strip_->child_count() - 1)) {
    // Select the next tab.
    SelectTabAt(index + 1);
  } else {
    // We are the last tab, select the previous one.
    if (index > 0) {
      SelectTabAt(index - 1);
    } else {
      // That was the last tab. Remove the contents.
      content_view_->RemoveAllChildViews(false);
    }
  }
  return tab_strip_->RemoveTabAt(index);
}

void NativeTabbedPaneViews::SelectTabAt(int index) {
  DCHECK((index >= 0) && (index < tab_strip_->child_count()));
  tab_strip_->SelectTab(tab_strip_->child_at(index));
}

int NativeTabbedPaneViews::GetTabCount() {
  return tab_strip_->child_count();
}

int NativeTabbedPaneViews::GetSelectedTabIndex() {
  return tab_strip_->GetSelectedIndex();
}

View* NativeTabbedPaneViews::GetSelectedTab() {
  return content_view_->child_at(GetSelectedTabIndex());
}

View* NativeTabbedPaneViews::GetView() {
  return this;
}

void NativeTabbedPaneViews::SetFocus() {
  // Focus the associated HWND.
  OnFocus();
}

gfx::Size NativeTabbedPaneViews::GetPreferredSize() {
  gfx::Size content_size = content_view_->GetPreferredSize();
  return gfx::Size(
      content_size.width(),
      tab_strip_->GetPreferredSize().height() + content_size.height());
}

gfx::NativeView NativeTabbedPaneViews::GetTestingHandle() const {
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabbedPaneViews, View overrides:

void NativeTabbedPaneViews::Layout() {
  gfx::Size ps = tab_strip_->GetPreferredSize();
  tab_strip_->SetBounds(0, 0, width(), ps.height());
  content_view_->SetBounds(0, tab_strip_->bounds().bottom(), width(),
                           std::max(0, height() - ps.height()));
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabbedPaneViews, private:

void NativeTabbedPaneViews::InitControl() {
  content_view_->SetLayoutManager(tab_layout_manager_);

  // Add tabs that are already added if any.
  if (tab_strip_->has_children())
    InitializeTabs();
}

void NativeTabbedPaneViews::InitializeTabs() {
  for (int i = 0; i < tab_strip_->child_count(); ++i)
    content_view_->AddChildView(Tab::GetContents(tab_strip_->child_at(i)));
}

}  // namespace views
