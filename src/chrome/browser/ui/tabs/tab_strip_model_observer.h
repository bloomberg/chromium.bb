// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_OBSERVER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_OBSERVER_H_

#include <vector>

#include "base/macros.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "ui/base/models/list_selection_model.h"

class TabStripModel;

namespace content {
class WebContents;
}

////////////////////////////////////////////////////////////////////////////////
//
// TabStripModelChange / TabStripSelectionChange
//
// The following class and structures are used to inform TabStripModelObservers
// of changes to:
// 1) selection model
// 2) activated tab
// 3) inserted/removed/moved tabs.
// These changes must be bundled together because (1) and (2) consist of indices
// into a list of tabs [determined by (3)]. All three must be kept synchronized.
//
////////////////////////////////////////////////////////////////////////////////
class TabStripModelChange {
 public:
  enum Type { kSelectionOnly, kInserted, kRemoved, kMoved, kReplaced };

  // A WebContents was inserted at |index|. This implicitly changes the existing
  // selection model by calling IncrementFrom(index).
  struct Insert {
    content::WebContents* contents;
    int index;
  };

  // A WebContents was removed at |index|. This implicitly changes the existing
  // selection model by calling DecrementFrom(index).
  struct Remove {
    content::WebContents* contents;
    int index;

    // The specified WebContents at |index| is being closed (and eventually
    // destroyed). |tab_strip_model| is the TabStripModel that contained the
    // tab.
    bool will_be_deleted;
  };

  // A WebContents was moved from |from_index| to |to_index|. This implicitly
  // changes the existing selection model by calling
  // Move(from_index, to_index, 1).
  struct Move {
    content::WebContents* contents;
    int from_index;
    int to_index;
  };

  // The WebContents was replaced at the specified index. This is invoked when
  // prerendering swaps in a prerendered WebContents.
  struct Replace {
    content::WebContents* old_contents;
    content::WebContents* new_contents;
    int index;
  };

  struct Delta {
    union {
      Insert insert;
      Remove remove;
      Move move;
      Replace replace;
    };
  };

  // Convenient factory methods to create |Delta| with each member.
  static Delta CreateInsertDelta(content::WebContents* contents, int index);
  static Delta CreateRemoveDelta(content::WebContents* contents,
                                 int index,
                                 bool will_be_deleted);
  static Delta CreateMoveDelta(content::WebContents* contents,
                               int from_index,
                               int to_index);
  static Delta CreateReplaceDelta(content::WebContents* old_contents,
                                  content::WebContents* new_contents,
                                  int index);

  TabStripModelChange();
  TabStripModelChange(Type type, const Delta& delta);
  TabStripModelChange(Type type, const std::vector<Delta>& deltas);
  ~TabStripModelChange();

  TabStripModelChange(TabStripModelChange&& other);

  Type type() const { return type_; }
  const std::vector<Delta>& deltas() const { return deltas_; }

 private:
  const Type type_ = kSelectionOnly;
  const std::vector<Delta> deltas_;

  DISALLOW_COPY_AND_ASSIGN(TabStripModelChange);
};

// Struct to carry changes on selection/activation.
struct TabStripSelectionChange {
  TabStripSelectionChange();
  TabStripSelectionChange(const TabStripSelectionChange& other);
  ~TabStripSelectionChange();

  TabStripSelectionChange& operator=(const TabStripSelectionChange& other);

  // Fill TabStripSelectionChange with given |contents| and |selection_model|.
  // note that |new_contents| and |new_model| will be filled too so that
  // selection_changed() and active_tab_changed() won't return true.
  TabStripSelectionChange(content::WebContents* contents,
                          const ui::ListSelectionModel& model);

  bool active_tab_changed() const { return old_contents != new_contents; }

  // TODO(sangwoo.ko) Do we need something to indicate that the change
  // was made implicitly?
  bool selection_changed() const {
    return selected_tabs_were_removed || old_model != new_model;
  }

  content::WebContents* old_contents = nullptr;
  content::WebContents* new_contents = nullptr;

  ui::ListSelectionModel old_model;
  ui::ListSelectionModel new_model;

  bool selected_tabs_were_removed = false;

  int reason = 0;
};

////////////////////////////////////////////////////////////////////////////////
//
// TabStripModelObserver
//
//  Objects implement this interface when they wish to be notified of changes
//  to the TabStripModel.
//
//  Two major implementers are the TabStrip, which uses notifications sent
//  via this interface to update the presentation of the strip, and the Browser
//  object, which updates bookkeeping and shows/hides individual WebContentses.
//
//  Register your TabStripModelObserver with the TabStripModel using its
//  Add/RemoveObserver methods.
//
////////////////////////////////////////////////////////////////////////////////
class TabStripModelObserver {
 public:
  enum ChangeReason {
    // Used to indicate that none of the reasons below are responsible for the
    // active tab change.
    CHANGE_REASON_NONE = 0,
    // The active tab changed because the tab's web contents was replaced.
    CHANGE_REASON_REPLACED = 1 << 0,
    // The active tab changed due to a user input event.
    CHANGE_REASON_USER_GESTURE = 1 << 1,
  };

  enum CloseAllStoppedReason {
    // Used to indicate that CloseAllTab event is canceled.
    kCloseAllCanceled = 0,
    // Used to indicate that CloseAllTab event complete successfully.
    kCloseAllCompleted = 1,
  };

  // |change| is a series of changes in tabtrip model. |change| consists
  // of changes with same type and those changes may have caused selection or
  // activation changes. |selection| is determined by comparing the state of
  // TabStripModel before the |change| and after the |change| are applied.
  // When only selection/activation was changed without any change about
  // WebContents, |change| can be empty.
  virtual void OnTabStripModelChanged(TabStripModel* tab_strip_model,
                                      const TabStripModelChange& change,
                                      const TabStripSelectionChange& selection);

  // The specified WebContents at |index| changed in some way. |contents|
  // may be an entirely different object and the old value is no longer
  // available by the time this message is delivered.
  //
  // See tab_change_type.h for a description of |change_type|.
  virtual void TabChangedAt(content::WebContents* contents,
                            int index,
                            TabChangeType change_type);

  // Invoked when the pinned state of a tab changes.
  virtual void TabPinnedStateChanged(TabStripModel* tab_strip_model,
                                     content::WebContents* contents,
                                     int index);

  // Invoked when the blocked state of a tab changes.
  // NOTE: This is invoked when a tab becomes blocked/unblocked by a tab modal
  // window.
  virtual void TabBlockedStateChanged(content::WebContents* contents,
                                      int index);

  // The TabStripModel now no longer has any tabs. The implementer may
  // use this as a trigger to try and close the window containing the
  // TabStripModel, for example...
  virtual void TabStripEmpty();

  // Sent any time an attempt is made to close all the tabs. This is not
  // necessarily the result of CloseAllTabs(). For example, if the user closes
  // the last tab WillCloseAllTabs() is sent. If the close does not succeed
  // during the current event (say unload handlers block it) then
  // CloseAllTabsStopped() is sent with reason 'CANCELED'. On the other hand if
  // the close does finish then CloseAllTabsStopped() is sent with reason
  // 'COMPLETED'. Also note that if the last tab is detached
  // (DetachWebContentsAt()) then this is not sent.
  virtual void WillCloseAllTabs(TabStripModel* tab_strip_model);
  virtual void CloseAllTabsStopped(TabStripModel* tab_strip_model,
                                   CloseAllStoppedReason reason);

  // The specified tab at |index| requires the display of a UI indication to the
  // user that it needs their attention. The UI indication is set iff
  // |attention| is true.
  virtual void SetTabNeedsAttentionAt(int index, bool attention);

 protected:
  TabStripModelObserver();
  virtual ~TabStripModelObserver() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TabStripModelObserver);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_OBSERVER_H_
