// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/tabbed_pane/native_tabbed_pane_gtk.h"

#include <gtk/gtk.h>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/skia_utils_gtk.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget.h"
#include "views/background.h"

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
// NativeTabbedPaneGtk, NativeControlGtk implementation:

void NativeTabbedPaneGtk::CreateNativeControl() {
  GtkWidget* widget = gtk_notebook_new();
  gtk_notebook_set_tab_pos(GTK_NOTEBOOK(widget), GTK_POS_TOP);
  g_signal_connect(widget, "switch-page",
                   G_CALLBACK(CallSwitchPage), this);
  NativeControlCreated(widget);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabbedPaneGtk, NativeTabbedPaneWrapper implementation:

void NativeTabbedPaneGtk::AddTab(const string16& title, View* contents) {
  AddTabAtIndex(GetTabCount(), title, contents, true);
}

void NativeTabbedPaneGtk::AddTabAtIndex(int index,
                                        const string16& title,
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

  GtkWidget* page =
      gtk_notebook_get_nth_page(GTK_NOTEBOOK(native_view()), index);
  Widget* widget = Widget::GetWidgetForNativeView(page);

  // detach the content view from widget so that we can delete widget
  // without destroying the content view.
  View* removed_tab = GetTabViewAt(index);
  widget->GetRootView()->RemoveChildView(removed_tab);

  // widget delete itself when native_view is deleted.
  gtk_notebook_remove_page(GTK_NOTEBOOK(native_view()), index);

  // Removing a tab might change the size of the tabbed pane.
  if (GetWidget())
    GetWidget()->GetRootView()->Layout();

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
  OnFocus();
}

gfx::Size NativeTabbedPaneGtk::GetPreferredSize() {
  if (!native_view())
    return gfx::Size();

  // For some strange reason (or maybe it's a bug), the requisition is not
  // returned in the passed requisition parameter, but instead written to the
  // widget's requisition field.
  GtkRequisition requisition = { 0, 0 };
  gtk_widget_size_request(GTK_WIDGET(native_view()), &requisition);
  GtkRequisition& size(GTK_WIDGET(native_view())->requisition);
  return gfx::Size(size.width, size.height);
}

gfx::NativeView NativeTabbedPaneGtk::GetTestingHandle() const {
  return native_view();
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabbedPaneGtk, View implementation:

FocusTraversable* NativeTabbedPaneGtk::GetFocusTraversable() {
  return GetWidgetAt(GetSelectedTabIndex());
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabbedPaneGtk, private:
void NativeTabbedPaneGtk::DoAddTabAtIndex(int index,
                                          const string16& title,
                                          View* contents,
                                          bool select_if_first_tab) {
  int tab_count = GetTabCount();
  DCHECK(index <= tab_count);

  Widget* page_container = new Widget;
  page_container->Init(
      Widget::InitParams(Widget::InitParams::TYPE_CONTROL));
  page_container->SetContentsView(contents);
  page_container->SetFocusTraversableParent(GetWidget()->GetFocusTraversable());
  page_container->SetFocusTraversableParentView(this);
  page_container->Show();

  if (!contents->background()) {
    GtkStyle* window_style =
        gtk_widget_get_style(page_container->GetNativeView());
    contents->set_background(
        Background::CreateSolidBackground(
            gfx::GdkColorToSkColor(window_style->bg[GTK_STATE_NORMAL])));
  }

  GtkWidget* page = page_container->GetNativeView();
  // increment ref count not to delete on remove below
  g_object_ref(page);
  // detach parent from the page so that we can add it to notebook
  GtkWidget* parent = gtk_widget_get_parent(page);
  gtk_container_remove(GTK_CONTAINER(parent), page);

  GtkWidget* label = gtk_label_new(UTF16ToUTF8(title).c_str());
  gtk_widget_show(label);
  gtk_notebook_insert_page(GTK_NOTEBOOK(native_view()),
                           page,
                           label,
                           index);
  g_object_unref(page);

  if (tab_count == 0 && select_if_first_tab)
    gtk_notebook_set_current_page(GTK_NOTEBOOK(native_view()), 0);

  // Relayout the hierarchy, since the added tab might require more space.
  if (GetWidget())
    GetWidget()->GetRootView()->Layout();
}

Widget* NativeTabbedPaneGtk::GetWidgetAt(int index) {
  DCHECK(index <= GetTabCount());
  GtkWidget* page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(native_view()),
                                              index);
  Widget* widget = Widget::GetWidgetForNativeView(page);
  DCHECK(widget);
  return widget;
}

View* NativeTabbedPaneGtk::GetTabViewAt(int index) {
  Widget* widget = GetWidgetAt(index);
  DCHECK(widget);
  DCHECK_EQ(1, widget->GetRootView()->child_count());
  return widget->GetRootView()->child_at(0);
}

void NativeTabbedPaneGtk::OnSwitchPage(int selected_tab_index) {
  TabbedPaneListener* listener = tabbed_pane_->listener();
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
