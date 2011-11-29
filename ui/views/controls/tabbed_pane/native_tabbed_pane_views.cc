// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/tabbed_pane/native_tabbed_pane_views.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace views {

class TabStrip;

class Tab : public View {
 public:
  Tab(TabStrip* tab_strip, const string16& title, View* contents)
      : tab_strip_(tab_strip),
        title_(title),
        contents_(contents) {}
  virtual ~Tab() {}

  static int GetMinimumTabHeight() {
    return gfx::Font().GetHeight() + 10;
  }

  static View* GetContents(View* tab) {
    return static_cast<Tab*>(tab)->contents_;
  }

  // Overridden from View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->FillRect(GetTabColor(), GetLocalBounds());
    canvas->DrawStringInt(title_, gfx::Font(), SK_ColorBLACK, GetLocalBounds());
  }
  virtual bool OnMousePressed(const MouseEvent& event) OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    gfx::Size ps(gfx::Font().GetStringWidth(title_), GetMinimumTabHeight());
    ps.Enlarge(10, 0);
    return ps;
  }

 private:
  SkColor GetTabColor() const;

  TabStrip* tab_strip_;
  string16 title_;
  View* contents_;

  DISALLOW_COPY_AND_ASSIGN(Tab);
};

class TabStrip : public View {
 public:
  explicit TabStrip(NativeTabbedPaneViews* owner)
      : owner_(owner),
        selected_tab_(NULL) {
  }
  virtual ~TabStrip() {}

  void SelectTab(View* tab) {
    if (tab == selected_tab_)
      return;
    if (selected_tab_)
      selected_tab_->SchedulePaint();
    selected_tab_ = tab;
    selected_tab_->SchedulePaint();
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
    int x = 0;
    for (int i = 0; i < child_count(); ++i) {
      gfx::Size ps = child_at(i)->GetPreferredSize();
      child_at(i)->SetBounds(x, 0, ps.width(), ps.height());
      x = child_at(i)->bounds().right();
    }
  }

 private:
  NativeTabbedPaneViews* owner_;
  View* selected_tab_;

  DISALLOW_COPY_AND_ASSIGN(TabStrip);
};

bool Tab::OnMousePressed(const MouseEvent& event) {
  tab_strip_->SelectTab(this);
  return true;
}

SkColor Tab::GetTabColor() const {
  return tab_strip_->IsTabSelected(this) ? SK_ColorRED : SK_ColorBLUE;
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
    DCHECK(focus_manager);
    const View* focused_view = focus_manager->GetFocusedView();
    if (focused_view && host->Contains(focused_view) &&
        !page->Contains(focused_view))
      focus_manager->SetFocusedView(page);
  }

 private:
  // LayoutManager overrides:
  virtual void Layout(View* host) OVERRIDE {
    gfx::Rect bounds(host->GetContentsBounds());
    for (int i = 0; i < host->child_count(); ++i) {
      View* child = host->child_at(i);
      // We only layout visible children, since it may be expensive.
      if (child->IsVisible() && child->bounds() != bounds)
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
      content_window_(NULL) {
  AddChildView(tab_strip_);
}

NativeTabbedPaneViews::~NativeTabbedPaneViews() {
}

void NativeTabbedPaneViews::TabSelectionChanged(View* selected) {
  if (content_window_) {
    View* content_root = content_window_->GetRootView();
    tab_layout_manager_->SwitchToPage(content_root, Tab::GetContents(selected));
  }
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
  contents->set_parent_owned(false);
  contents->SetVisible(false);

  tab_strip_->AddChildViewAt(new Tab(tab_strip_, title, contents), index);

  // Add native tab only if the native control is alreay created.
  if (content_window_) {
    View* content_root = content_window_->GetRootView();
    content_root->AddChildView(contents);

    // Switch to the newly added tab if requested;
    if (tab_strip_->child_count() == 1 && select_if_first_tab)
      tab_layout_manager_->SwitchToPage(content_root, contents);
  }
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
    } else if (content_window_) {
      // That was the last tab. Remove the contents.
      content_window_->GetRootView()->RemoveAllChildViews(false);
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
  return tab_strip_->selected_tab();
}

View* NativeTabbedPaneViews::GetView() {
  return this;
}

void NativeTabbedPaneViews::SetFocus() {
  // Focus the associated HWND.
  OnFocus();
}

gfx::Size NativeTabbedPaneViews::GetPreferredSize() {
  return gfx::Size(0, tab_strip_->GetPreferredSize().height());
}

gfx::NativeView NativeTabbedPaneViews::GetTestingHandle() const {
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabbedPaneViews, View overrides:

void NativeTabbedPaneViews::Layout() {
  gfx::Size ps = tab_strip_->GetPreferredSize();
  tab_strip_->SetBounds(0, 0, width(), ps.height());

  gfx::Point child_origin(0, tab_strip_->bounds().bottom());
  View::ConvertPointToWidget(this, &child_origin);

  if (content_window_) {
    content_window_->SetBounds(gfx::Rect(child_origin.x(),
                                         child_origin.y(),
                                         width(),
                                         std::max(0, height() - ps.height())));
  }
}

FocusTraversable* NativeTabbedPaneViews::GetFocusTraversable() {
  return content_window_;
}

void NativeTabbedPaneViews::ViewHierarchyChanged(bool is_add,
                                                 View *parent,
                                                 View *child) {
  if (is_add && child == this)
    InitControl();
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabbedPaneViews, private:

void NativeTabbedPaneViews::InitControl() {
  content_window_ = new Widget;
  Widget::InitParams params(Widget::InitParams::TYPE_CONTROL);
  params.parent = GetWidget()->GetNativeView();
  content_window_->Init(params);
  content_window_->GetRootView()->SetLayoutManager(tab_layout_manager_);
  content_window_->GetRootView()->set_background(
      Background::CreateSolidBackground(SK_ColorLTGRAY));
  content_window_->SetFocusTraversableParentView(this);
  content_window_->SetFocusTraversableParent(
      GetWidget()->GetFocusTraversable());

  // Add tabs that are already added if any.
  if (tab_strip_->has_children())
    InitializeTabs();
}

void NativeTabbedPaneViews::InitializeTabs() {
  View* content_root = content_window_->GetRootView();
  for (std::vector<View*>::const_iterator tab = tab_strip_->children_begin();
       tab != tab_strip_->children_end(); ++tab)
    content_root->AddChildView(Tab::GetContents(*tab));
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabbedPaneWrapper, public:

// static
NativeTabbedPaneWrapper* NativeTabbedPaneWrapper::CreateNativeWrapper(
    TabbedPane* tabbed_pane) {
  return new NativeTabbedPaneViews(tabbed_pane);
}

}  // namespace views
