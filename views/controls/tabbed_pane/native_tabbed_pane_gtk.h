// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_TABBED_PANE_NATIVE_TABBED_PANE_GTK_H_
#define VIEWS_CONTROLS_TABBED_PANE_NATIVE_TABBED_PANE_GTK_H_

#include "views/controls/native_control_gtk.h"
#include "views/controls/tabbed_pane/native_tabbed_pane_wrapper.h"

namespace views {

class WidgetGtk;

class NativeTabbedPaneGtk : public NativeControlGtk,
                            public NativeTabbedPaneWrapper {
 public:
  explicit NativeTabbedPaneGtk(TabbedPane* tabbed_pane);
  virtual ~NativeTabbedPaneGtk();

  // NativeTabbedPaneWrapper implementation:
  virtual void AddTab(const std::wstring& title, View* contents);
  virtual void AddTabAtIndex(int index,
                             const std::wstring& title,
                             View* contents,
                             bool select_if_first_tab);
  virtual View* RemoveTabAtIndex(int index);
  virtual void SelectTabAt(int index);
  virtual int GetTabCount();
  virtual int GetSelectedTabIndex();
  virtual View* GetSelectedTab();
  virtual View* GetView();
  virtual void SetFocus();
  virtual gfx::NativeView GetTestingHandle() const;

  // NativeControlGtk overrides.
  virtual void CreateNativeControl();

 private:
  void DoAddTabAtIndex(int index,
                       const std::wstring& title,
                       View* contents,
                       bool select_if_first_tab);
  View* GetTabViewAt(int index);
  void OnSwitchPage(int selected_tab_index);

  static void CallSwitchPage(GtkNotebook* widget,
                             GtkNotebookPage* page,
                             guint selected_tab_index,
                             NativeTabbedPaneGtk* tabbed_pane);

  // The tabbed-pane we are bound to.
  TabbedPane* tabbed_pane_;

  DISALLOW_COPY_AND_ASSIGN(NativeTabbedPaneGtk);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_TABBED_PANE_NATIVE_TABBED_PANE_GTK_H_
