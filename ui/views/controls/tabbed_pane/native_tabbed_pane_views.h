// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABBED_PANE_NATIVE_TABBED_PANE_VIEWS_H_
#define UI_VIEWS_CONTROLS_TABBED_PANE_NATIVE_TABBED_PANE_VIEWS_H_
#pragma once

#include <vector>

#include "ui/views/controls/tabbed_pane/native_tabbed_pane_wrapper.h"
#include "views/view.h"

namespace views {

class TabLayout;
class TabStrip;
class Widget;

class NativeTabbedPaneViews : public View,
                              public NativeTabbedPaneWrapper {
 public:
  explicit NativeTabbedPaneViews(TabbedPane* tabbed_pane);
  virtual ~NativeTabbedPaneViews();

  void TabSelectionChanged(View* selected);

  // Overridden from NativeTabbedPaneWrapper:
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

  // View overrides:
  virtual void Layout() OVERRIDE;
  virtual FocusTraversable* GetFocusTraversable() OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add, View *parent,
                                    View *child) OVERRIDE;

 private:
  void InitControl();

  // Called upon creation of native control to initialize tabs that are added
  // before the native control is created.
  void InitializeTabs();

  // Adds a tab with the given content to native control at the given index.
  void AddNativeTab(int index, const string16& title);

  // The tabbed-pane we are bound to.
  TabbedPane* tabbed_pane_;

  // The layout manager we use for managing our tabs.
  TabLayout* tab_layout_manager_;

  // TabStrip.
  TabStrip* tab_strip_;

  // The window displayed in the tab.
  Widget* content_window_;

  DISALLOW_COPY_AND_ASSIGN(NativeTabbedPaneViews);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TABBED_PANE_NATIVE_TABBED_PANE_VIEWS_H_
