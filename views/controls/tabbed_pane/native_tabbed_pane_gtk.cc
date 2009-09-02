// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/tabbed_pane/native_tabbed_pane_gtk.h"

#include <gtk/gtk.h>

#include "app/gfx/canvas.h"
#include "app/gfx/font.h"
#include "app/resource_bundle.h"
#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "views/controls/tabbed_pane/tabbed_pane.h"
#include "views/fill_layout.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeTabbedPaneGtk, public:

NativeTabbedPaneGtk::NativeTabbedPaneGtk(TabbedPane* tabbed_pane)
    : NativeControlGtk(),
      tabbed_pane_(tabbed_pane) {
  set_focus_view(tabbed_pane);
}

NativeTabbedPaneGtk::~NativeTabbedPaneGtk() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabbedPaneGtk, NativeTabbedPaneWrapper implementation:

void NativeTabbedPaneGtk::AddTab(const std::wstring& title, View* contents) {
  AddTabAtIndex(GetTabCount(), title, contents, true);
}

void NativeTabbedPaneGtk::AddTabAtIndex(int index, const std::wstring& title,
                                        View* contents,
                                        bool select_if_first_tab) {
  DCHECK(native_view());
  DoAddTabAtIndex(index, title, contents, select_if_first_tab);
}

View* NativeTabbedPaneGtk::RemoveTabAtIndex(int index) {
  int tab_count = GetTabCount();
  DCHECK(index >= 0 && index < tab_count);

  if (index < (tab_count - 1)) {
    // Select the next tab.
    SelectTabAt(index + 1);
  } else {
    // We are the last tab, select the previous one.
    if (index > 0) {
      SelectTabAt(index - 1);
    } else {
      // last tab. nothing to select.
    }
  }
  View* removed_tab = GetTabViewAt(index);

  gtk_notebook_remove_page(GTK_NOTEBOOK(native_view()), index);

  return removed_tab;
}

void NativeTabbedPaneGtk::SelectTabAt(int index) {
  DCHECK((index >= 0) && (index < GetTabCount()));
  gtk_notebook_set_current_page(GTK_NOTEBOOK(native_view()), index);
}

int NativeTabbedPaneGtk::GetTabCount() {
  return gtk_notebook_get_n_pages(GTK_NOTEBOOK(native_view()));
}

int NativeTabbedPaneGtk::GetSelectedTabIndex() {
  return gtk_notebook_get_current_page(GTK_NOTEBOOK(native_view()));
}

View* NativeTabbedPaneGtk::GetSelectedTab() {
  return GetTabViewAt(GetSelectedTabIndex());
}

View* NativeTabbedPaneGtk::GetView() {
  return this;
}

void NativeTabbedPaneGtk::SetFocus() {
  Focus();
}

gfx::NativeView NativeTabbedPaneGtk::GetTestingHandle() const {
  return native_view();
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabbedPaneGtk, NativeControlGtk override:

void NativeTabbedPaneGtk::CreateNativeControl() {
  GtkWidget* widget = gtk_notebook_new();
  gtk_notebook_set_tab_pos(GTK_NOTEBOOK(widget), GTK_POS_TOP);
  g_signal_connect(G_OBJECT(widget), "switch-page",
                   G_CALLBACK(CallSwitchPage), this);
  NativeControlCreated(widget);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabbedPaneGtk, private:
void NativeTabbedPaneGtk::DoAddTabAtIndex(int index, const std::wstring& title,
                                          View* contents,
                                          bool select_if_first_tab) {
  int tab_count = GetTabCount();
  DCHECK(index <= tab_count);

  WidgetGtk* page_container = new WidgetGtk(WidgetGtk::TYPE_CHILD);
  page_container->Init(NULL, gfx::Rect());
  page_container->SetContentsView(contents);
  page_container->Show();

  GtkWidget* page = page_container->GetNativeView();

  // increment ref count not to delete on remove below
  g_object_ref(page);
  // detach parent from the page so that we can add it to notebook
  GtkWidget* parent = gtk_widget_get_parent(page);
  gtk_container_remove(GTK_CONTAINER(parent), page);

  GtkWidget* label = gtk_label_new(WideToUTF8(title).c_str());
  gtk_widget_show(label);
  gtk_notebook_insert_page(GTK_NOTEBOOK(native_view()),
                           page,
                           label,
                           index);
  g_object_unref(page);

  if (tab_count == 0 && select_if_first_tab)
    gtk_notebook_set_current_page(GTK_NOTEBOOK(native_view()), 0);
}

View* NativeTabbedPaneGtk::GetTabViewAt(int index) {
  DCHECK(index <= GetTabCount());
  GtkWidget* page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(native_view()),
                                              index);
  WidgetGtk* widget = WidgetGtk::GetViewForNative(page);
  DCHECK(widget && widget->GetRootView()->GetChildViewCount() == 1);
  return widget->GetRootView()->GetChildViewAt(0);
}

void NativeTabbedPaneGtk::OnSwitchPage(int selected_tab_index) {
  TabbedPane::Listener* listener = tabbed_pane_->listener();
  if (listener != NULL)
    listener->TabSelectedAt(selected_tab_index);
}

// static
void NativeTabbedPaneGtk::CallSwitchPage(GtkNotebook* widget,
                                         GtkNotebookPage* page,
                                         guint selected_tab_index,
                                         NativeTabbedPaneGtk* tabbed_pane) {
  tabbed_pane->OnSwitchPage(selected_tab_index);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabbedPaneWrapper, public:

// static
NativeTabbedPaneWrapper* NativeTabbedPaneWrapper::CreateNativeWrapper(
    TabbedPane* tabbed_pane) {
  return new NativeTabbedPaneGtk(tabbed_pane);
}

}  // namespace views
