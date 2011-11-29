// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABBED_PANE_NATIVE_TABBED_PANE_GTK_H_
#define UI_VIEWS_CONTROLS_TABBED_PANE_NATIVE_TABBED_PANE_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/controls/native_control_gtk.h"
#include "ui/views/controls/tabbed_pane/native_tabbed_pane_wrapper.h"

namespace views {

class NativeTabbedPaneGtk : public NativeControlGtk,
                            public NativeTabbedPaneWrapper {
 public:
  explicit NativeTabbedPaneGtk(TabbedPane* tabbed_pane);
  virtual ~NativeTabbedPaneGtk();

  // NativeControlGtk:
  virtual void CreateNativeControl() OVERRIDE;

  // NativeTabbedPaneWrapper:
  virtual void AddTab(const string16& title, View* contents) OVERRIDE;
  virtual void AddTabAtIndex(int index,
                             const string16& title,
                             View* contents,
                             bool select_if_first_tab) OVERRIDE;
  virtual View* RemoveTabAtIndex(int index) OVERRIDE;
  virtual void SelectTabAt(int index) OVERRIDE;
  virtual int GetTabCount() OVERRIDE;
  virtual int GetSelectedTabIndex() OVERRIDE;
  virtual View* GetSelectedTab() OVERRIDE;
  virtual View* GetView() OVERRIDE;
  virtual void SetFocus() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual gfx::NativeView GetTestingHandle() const OVERRIDE;

  // View:
  virtual FocusTraversable* GetFocusTraversable() OVERRIDE;

 private:
  void DoAddTabAtIndex(int index,
                       const string16& title,
                       View* contents,
                       bool select_if_first_tab);

  // Returns the Widget containing the tab contents at |index|.
  Widget* GetWidgetAt(int index);

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

#endif  // UI_VIEWS_CONTROLS_TABBED_PANE_NATIVE_TABBED_PANE_GTK_H_
