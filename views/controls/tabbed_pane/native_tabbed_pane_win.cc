// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/tabbed_pane/native_tabbed_pane_win.h"

#include <vssym32.h>

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/font.h"
#include "ui/gfx/native_theme_win.h"
#include "views/controls/tabbed_pane/tabbed_pane.h"
#include "views/layout/fill_layout.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_win.h"

namespace views {

// A background object that paints the tab panel background which may be
// rendered by the system visual styles system.
class TabBackground : public Background {
 public:
  explicit TabBackground() {
    // TMT_FILLCOLORHINT returns a color value that supposedly
    // approximates the texture drawn by PaintTabPanelBackground.
    SkColor tab_page_color =
        gfx::NativeTheme::instance()->GetThemeColorWithDefault(
            gfx::NativeTheme::TAB, TABP_BODY, 0, TMT_FILLCOLORHINT,
            COLOR_3DFACE);
    SetNativeControlColor(tab_page_color);
  }
  virtual ~TabBackground() {}

  virtual void Paint(gfx::Canvas* canvas, View* view) const {
    HDC dc = canvas->BeginPlatformPaint();
    RECT r = {0, 0, view->width(), view->height()};
    gfx::NativeTheme::instance()->PaintTabPanelBackground(dc, &r);
    canvas->EndPlatformPaint();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabBackground);
};

// Custom layout manager that takes care of sizing and displaying the tab pages.
class TabLayout : public LayoutManager {
 public:
  TabLayout() {}

  // Switches to the tab page identified by the given index.
  void SwitchToPage(View* host, View* page) {
    for (int i = 0; i < host->child_count(); ++i) {
      View* child = host->GetChildViewAt(i);
      // The child might not have been laid out yet.
      if (child == page)
        child->SetBoundsRect(host->GetContentsBounds());
      child->SetVisible(child == page);
    }

    FocusManager* focus_manager = page->GetFocusManager();
    DCHECK(focus_manager);
    View* focused_view = focus_manager->GetFocusedView();
    if (focused_view && host->Contains(focused_view) &&
        !page->Contains(focused_view))
      focus_manager->SetFocusedView(page);
  }

 private:
  // LayoutManager overrides:
  virtual void Layout(View* host) {
    gfx::Rect bounds(host->GetContentsBounds());
    for (int i = 0; i < host->child_count(); ++i) {
      View* child = host->GetChildViewAt(i);
      // We only layout visible children, since it may be expensive.
      if (child->IsVisible() && child->bounds() != bounds)
        child->SetBoundsRect(bounds);
    }
  }

  virtual gfx::Size GetPreferredSize(View* host) {
    // First, query the preferred sizes to determine a good width.
    int width = 0;
    for (int i = 0; i < host->child_count(); ++i) {
      View* page = host->GetChildViewAt(i);
      width = std::max(width, page->GetPreferredSize().width());
    }
    // After we know the width, decide on the height.
    return gfx::Size(width, GetPreferredHeightForWidth(host, width));
  }

  virtual int GetPreferredHeightForWidth(View* host, int width) {
    int height = 0;
    for (int i = 0; i < host->child_count(); ++i) {
      View* page = host->GetChildViewAt(i);
      height = std::max(height, page->GetHeightForWidth(width));
    }
    return height;
  }

  DISALLOW_COPY_AND_ASSIGN(TabLayout);
};

////////////////////////////////////////////////////////////////////////////////
// NativeTabbedPaneWin, public:

NativeTabbedPaneWin::NativeTabbedPaneWin(TabbedPane* tabbed_pane)
    : NativeControlWin(),
      tabbed_pane_(tabbed_pane),
      tab_layout_manager_(NULL),
      content_window_(NULL),
      selected_index_(-1) {
  // Associates the actual HWND with the tabbed-pane so the tabbed-pane is
  // the one considered as having the focus (not the wrapper) when the HWND is
  // focused directly (with a click for example).
  set_focus_view(tabbed_pane);
}

NativeTabbedPaneWin::~NativeTabbedPaneWin() {
  // We own the tab views, let's delete them.
  STLDeleteContainerPointers(tab_views_.begin(), tab_views_.end());
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabbedPaneWin, NativeTabbedPaneWrapper implementation:

void NativeTabbedPaneWin::AddTab(const std::wstring& title, View* contents) {
  AddTabAtIndex(static_cast<int>(tab_views_.size()), title, contents, true);
}

void NativeTabbedPaneWin::AddTabAtIndex(int index, const std::wstring& title,
                                        View* contents,
                                        bool select_if_first_tab) {
  DCHECK(index <= static_cast<int>(tab_views_.size()));
  contents->set_parent_owned(false);
  contents->SetVisible(false);
  tab_views_.insert(tab_views_.begin() + index, contents);
  tab_titles_.insert(tab_titles_.begin() + index, title);

  if (!contents->background())
    contents->set_background(new TabBackground);

  if (tab_views_.size() == 1 && select_if_first_tab)
    selected_index_ = 0;

  // Add native tab only if the native control is alreay created.
  if (content_window_) {
    AddNativeTab(index, title);
    // The newly added tab may have made the contents window smaller.
    ResizeContents();

    RootView* content_root = content_window_->GetRootView();
    content_root->AddChildView(contents);
    // Switch to the newly added tab if requested;
    if (tab_views_.size() == 1 && select_if_first_tab)
      tab_layout_manager_->SwitchToPage(content_root, contents);
  }
}

void NativeTabbedPaneWin::AddNativeTab(int index,
                                       const std::wstring &title) {
  TCITEM tcitem;
  tcitem.mask = TCIF_TEXT;

  // If the locale is RTL, we set the TCIF_RTLREADING so that BiDi text is
  // rendered properly on the tabs.
  if (base::i18n::IsRTL()) {
    tcitem.mask |= TCIF_RTLREADING;
  }

  tcitem.pszText = const_cast<wchar_t*>(title.c_str());
  int result = TabCtrl_InsertItem(native_view(), index, &tcitem);
  DCHECK(result != -1);
}

View* NativeTabbedPaneWin::RemoveTabAtIndex(int index) {
  int tab_count = static_cast<int>(tab_views_.size());
  DCHECK(index >= 0 && index < tab_count);

  if (index < (tab_count - 1)) {
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
  TabCtrl_DeleteItem(native_view(), index);

  // The removed tab may have made the contents window bigger.
  if (content_window_)
    ResizeContents();

  std::vector<View*>::iterator iter = tab_views_.begin() + index;
  View* removed_tab = *iter;
  if (content_window_)
    content_window_->GetRootView()->RemoveChildView(removed_tab);
  tab_views_.erase(iter);
  tab_titles_.erase(tab_titles_.begin() + index);

  return removed_tab;
}

void NativeTabbedPaneWin::SelectTabAt(int index) {
  DCHECK((index >= 0) && (index < static_cast<int>(tab_views_.size())));
  if (native_view())
    TabCtrl_SetCurSel(native_view(), index);
  DoSelectTabAt(index, true);
}

int NativeTabbedPaneWin::GetTabCount() {
  return TabCtrl_GetItemCount(native_view());
}

int NativeTabbedPaneWin::GetSelectedTabIndex() {
  return TabCtrl_GetCurSel(native_view());
}

View* NativeTabbedPaneWin::GetSelectedTab() {
  if (selected_index_ < 0)
    return NULL;
  return tab_views_[selected_index_];
}

View* NativeTabbedPaneWin::GetView() {
  return this;
}

void NativeTabbedPaneWin::SetFocus() {
  // Focus the associated HWND.
  OnFocus();
}

gfx::Size NativeTabbedPaneWin::GetPreferredSize() {
  if (!native_view())
    return gfx::Size();

  gfx::Rect contentSize(content_window_->GetRootView()->GetPreferredSize());
  RECT paneSize = { 0, 0, contentSize.width(), contentSize.height() };
  TabCtrl_AdjustRect(native_view(), TRUE, &paneSize);
  return gfx::Size(paneSize.right - paneSize.left,
                   paneSize.bottom - paneSize.top);
}

gfx::NativeView NativeTabbedPaneWin::GetTestingHandle() const {
  return native_view();
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabbedPaneWin, NativeControlWin override:

void NativeTabbedPaneWin::CreateNativeControl() {
  // Create the tab control.
  //
  // Note that we don't follow the common convention for NativeControl
  // subclasses and we don't pass the value returned from
  // NativeControl::GetAdditionalExStyle() as the dwExStyle parameter. Here is
  // why: on RTL locales, if we pass NativeControl::GetAdditionalExStyle() when
  // we basically tell Windows to create our HWND with the WS_EX_LAYOUTRTL. If
  // we do that, then the HWND we create for |content_window_| below will
  // inherit the WS_EX_LAYOUTRTL property and this will result in the contents
  // being flipped, which is not what we want (because we handle mirroring in
  // views without the use of Windows' support for mirroring). Therefore,
  // we initially create our HWND without WS_EX_LAYOUTRTL and we explicitly set
  // this property our child is created. This way, on RTL locales, our tabs
  // will be nicely rendered from right to left (by virtue of Windows doing the
  // right thing with the TabbedPane HWND) and each tab contents will use an
  // RTL layout correctly (by virtue of the mirroring infrastructure in views
  // doing the right thing with each View we put in the tab).
  DWORD style = WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | WS_CLIPCHILDREN;
  HWND tab_control = ::CreateWindowEx(0,
                                      WC_TABCONTROL,
                                      L"",
                                      style,
                                      0, 0, width(), height(),
                                      GetWidget()->GetNativeView(), NULL, NULL,
                                      NULL);

  HFONT font = ResourceBundle::GetSharedInstance().
      GetFont(ResourceBundle::BaseFont).GetNativeFont();
  SendMessage(tab_control, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);

  // Create the view container which is a child of the TabControl.
  content_window_ = new WidgetWin();
  content_window_->Init(tab_control, gfx::Rect());

  // Explicitly setting the WS_EX_LAYOUTRTL property for the HWND (see above
  // for why we waited until |content_window_| is created before we set this
  // property for the tabbed pane's HWND).
  if (base::i18n::IsRTL())
    l10n_util::HWNDSetRTLLayout(tab_control);

  RootView* root_view = content_window_->GetRootView();
  tab_layout_manager_ = new TabLayout();
  root_view->SetLayoutManager(tab_layout_manager_);
  DWORD sys_color = ::GetSysColor(COLOR_3DHILIGHT);
  SkColor color = SkColorSetRGB(GetRValue(sys_color), GetGValue(sys_color),
                                GetBValue(sys_color));
  root_view->set_background(Background::CreateSolidBackground(color));

  content_window_->SetFocusTraversableParentView(this);

  NativeControlCreated(tab_control);

  // Add tabs that are already added if any.
  if (!tab_views_.empty()) {
    InitializeTabs();
    if (selected_index_ >= 0)
      DoSelectTabAt(selected_index_, false);
  }

  ResizeContents();
}

bool NativeTabbedPaneWin::ProcessMessage(UINT message,
                                         WPARAM w_param,
                                         LPARAM l_param,
                                         LRESULT* result) {
  if (message == WM_NOTIFY &&
      reinterpret_cast<LPNMHDR>(l_param)->code == TCN_SELCHANGE) {
    int selected_tab = TabCtrl_GetCurSel(native_view());
    DCHECK(selected_tab != -1);
    DoSelectTabAt(selected_tab, true);
    return TRUE;
  }
  return NativeControlWin::ProcessMessage(message, w_param, l_param, result);
}

////////////////////////////////////////////////////////////////////////////////
// View override:

void NativeTabbedPaneWin::Layout() {
  NativeControlWin::Layout();
  ResizeContents();
}

FocusTraversable* NativeTabbedPaneWin::GetFocusTraversable() {
  return content_window_;
}

void NativeTabbedPaneWin::ViewHierarchyChanged(bool is_add,
                                               View *parent,
                                               View *child) {
  NativeControlWin::ViewHierarchyChanged(is_add, parent, child);

  if (is_add && (child == this) && content_window_) {
    // We have been added to a view hierarchy, update the FocusTraversable
    // parent.
    content_window_->SetFocusTraversableParent(
        GetWidget()->GetFocusTraversable());
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabbedPaneWin, private:

void NativeTabbedPaneWin::InitializeTabs() {
  for (size_t i = 0; i < tab_titles_.size(); ++i)
    AddNativeTab(i, tab_titles_[i]);

  RootView* content_root = content_window_->GetRootView();
  for (std::vector<View*>::iterator tab(tab_views_.begin());
       tab != tab_views_.end(); ++tab)
    content_root->AddChildView(*tab);
}

void NativeTabbedPaneWin::DoSelectTabAt(int index, boolean invoke_listener) {
  selected_index_ = index;
  if (content_window_) {
    RootView* content_root = content_window_->GetRootView();
    tab_layout_manager_->SwitchToPage(content_root, tab_views_.at(index));
  }
  if (invoke_listener && tabbed_pane_->listener())
    tabbed_pane_->listener()->TabSelectedAt(index);
}

void NativeTabbedPaneWin::ResizeContents() {
  CRect content_bounds;
  if (!GetClientRect(native_view(), &content_bounds))
    return;
  TabCtrl_AdjustRect(native_view(), FALSE, &content_bounds);
  content_window_->MoveWindow(content_bounds.left, content_bounds.top,
                              content_bounds.Width(), content_bounds.Height(),
                              TRUE);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabbedPaneWrapper, public:

// static
NativeTabbedPaneWrapper* NativeTabbedPaneWrapper::CreateNativeWrapper(
    TabbedPane* tabbed_pane) {
  return new NativeTabbedPaneWin(tabbed_pane);
}

}  // namespace views
