// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABBED_PANE_NATIVE_TABBED_PANE_WRAPPER_H_
#define UI_VIEWS_CONTROLS_TABBED_PANE_NATIVE_TABBED_PANE_WRAPPER_H_
#pragma once

#include "base/string16.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Size;
}

namespace views {

class TabbedPane;
class View;

// An interface implemented by an object that provides a platform-native
// tabbed-pane.
class NativeTabbedPaneWrapper {
 public:
  // The TabbedPane calls this when it is destroyed to clean up the wrapper
  // object.
  virtual ~NativeTabbedPaneWrapper() {}

  // Adds a new tab at the end of this TabbedPane with the specified |title|.
  // |contents| is the view displayed when the tab is selected and is owned by
  // the TabbedPane.
  virtual void AddTab(const string16& title, View* contents) = 0;

  // Adds a new tab at the specified |index| with the specified |title|.
  // |contents| is the view displayed when the tab is selected and is owned by
  // the TabbedPane. If |select_if_first_tab| is true and the tabbed pane is
  // currently empty, the new tab is selected. If you pass in false for
  // |select_if_first_tab| you need to explicitly invoke SelectTabAt, otherwise
  // the tabbed pane will not have a valid selection.
  virtual void AddTabAtIndex(int index,
                             const string16& title,
                             View* contents,
                             bool select_if_first_tab) = 0;

  // Removes the tab at the specified |index| and returns the associated content
  // view.  The caller becomes the owner of the returned view.
  virtual View* RemoveTabAtIndex(int index) = 0;

  // Selects the tab at the specified |index|, which must be valid.
  virtual void SelectTabAt(int index) = 0;

  // Returns the number of tabs.
  virtual int GetTabCount() = 0;

  // Returns the index of the selected tab.
  virtual int GetSelectedTabIndex() = 0;

  // Returns the contents of the selected tab.
  virtual View* GetSelectedTab() = 0;

  // Retrieves the views::View that hosts the native control.
  virtual View* GetView() = 0;

  // Sets the focus to the tabbed pane native view.
  virtual void SetFocus() = 0;

  // Gets the preferred size of the tabbed pane.
  virtual gfx::Size GetPreferredSize() = 0;

  // Returns a handle to the underlying native view for testing.
  virtual gfx::NativeView GetTestingHandle() const = 0;

  // Creates an appropriate NativeTabbedPaneWrapper for the platform.
  static NativeTabbedPaneWrapper* CreateNativeWrapper(TabbedPane* tabbed_pane);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TABBED_PANE_NATIVE_TABBED_PANE_WRAPPER_H_
