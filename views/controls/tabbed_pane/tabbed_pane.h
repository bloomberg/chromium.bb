// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_TABBED_PANE_H_
#define VIEWS_CONTROLS_TABBED_PANE_H_
#pragma once

#include "views/view.h"

namespace views {

class NativeTabbedPaneWrapper;

// The TabbedPane class is a view that shows tabs.  When the user clicks on a
// tab, the associated view is displayed.
class TabbedPane : public View {
 public:
  TabbedPane();
  virtual ~TabbedPane();

  // An interface an object can implement to be notified about events within
  // the TabbedPane.
  class Listener {
   public:
    // Called when the tab at the specified |index| is selected by the user.
    virtual void TabSelectedAt(int index) = 0;
  };
  void SetListener(Listener* listener);

  // Returns the number of tabs.
  int GetTabCount();

  // Returns the index of the selected tab.
  int GetSelectedTabIndex();

  // Returns the contents of the selected tab.
  View* GetSelectedTab();

  // Adds a new tab at the end of this TabbedPane with the specified |title|.
  // |contents| is the view displayed when the tab is selected and is owned by
  // the TabbedPane.
  void AddTab(const std::wstring& title, View* contents);

  // Adds a new tab at the specified |index| with the specified |title|.
  // |contents| is the view displayed when the tab is selected and is owned by
  // the TabbedPane. If |select_if_first_tab| is true and the tabbed pane is
  // currently empty, the new tab is selected. If you pass in false for
  // |select_if_first_tab| you need to explicitly invoke SelectTabAt, otherwise
  // the tabbed pane will not have a valid selection.
  void AddTabAtIndex(int index,
                     const std::wstring& title,
                     View* contents,
                     bool select_if_first_tab);

  // Removes the tab at the specified |index| and returns the associated content
  // view.  The caller becomes the owner of the returned view.
  View* RemoveTabAtIndex(int index);

  // Selects the tab at the specified |index|, which must be valid.
  void SelectTabAt(int index);

  Listener* listener() const { return listener_; }

  void SetAccessibleName(const string16& name);

  // View overrides:
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child)
      OVERRIDE;
  // Handles Ctrl+Tab and Ctrl+Shift+Tab navigation of pages.
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator)
      OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  NativeTabbedPaneWrapper* native_wrapper() const {
    return native_tabbed_pane_;
  }

 protected:
  // The object that actually implements the tabbed-pane.
  // Protected for tests access.
  NativeTabbedPaneWrapper* native_tabbed_pane_;

 private:
  // The tabbed-pane's class name.
  static const char kViewClassName[];

  // Creates the native wrapper.
  void CreateWrapper();

  // We support Ctrl+Tab and Ctrl+Shift+Tab to navigate tabbed option pages.
  void LoadAccelerators();

  // The listener we notify about tab selection changes.
  Listener* listener_;

  // The accessible name of this view.
  string16 accessible_name_;

  DISALLOW_COPY_AND_ASSIGN(TabbedPane);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_TABBED_PANE_H_
