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

  // Fill TabStripSelectionChange with given |contents| and |selection_model|.
  // note that |new_contents| and |new_model| will be filled too so that
  // selection_changed() and active_tab_changed() won't return true.
  TabStripSelectionChange(content::WebContents* contents,
                          const ui::ListSelectionModel& model);

  bool active_tab_changed() const { return old_contents != new_contents; }

  // TODO(sangwoo.ko) Do we need something to indicate that the change
  // was made implicitly?
  bool selection_changed() const { return old_model != new_model; }

  content::WebContents* old_contents = nullptr;
  content::WebContents* new_contents = nullptr;

  ui::ListSelectionModel old_model;
  ui::ListSelectionModel new_model;

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

  // A new WebContents was inserted into the TabStripModel at the
  // specified index. |foreground| is whether or not it was opened in the
  // foreground (selected).
  // DEPRECATED, use OnTabStripChanged() above.
  // TODO(crbug.com/842194): Delete this and migrate callsites.
  virtual void TabInsertedAt(TabStripModel* tab_strip_model,
                             content::WebContents* contents,
                             int index,
                             bool foreground);

  // The specified WebContents at |index| is being closed (and eventually
  // destroyed). |tab_strip_model| is the TabStripModel that contained the tab.
  // DEPRECATED, use OnTabStripChanged() above.
  // TODO(erikchen): |index| is not used outside of tests. Do not use it. It
  // will be removed soon. https://crbug.com/842194.
  virtual void TabClosingAt(TabStripModel* tab_strip_model,
                            content::WebContents* contents,
                            int index);

  // The specified WebContents at |previous_index| has been detached, perhaps to
  // be inserted in another TabStripModel. The implementer should take whatever
  // action is necessary to deal with the WebContents no longer being present.
  // |previous_index| cannot be used to index into the current state of the
  // TabStripModel.
  // DEPRECATED, use OnTabStripChanged() above.
  // TODO(crbug.com/842194): Delete this and migrate callsites.
  virtual void TabDetachedAt(content::WebContents* contents,
                             int previous_index,
                             bool was_active);

  // The active WebContents is about to change from |old_contents|.
  // This gives observers a chance to prepare for an impending switch before it
  // happens.
  // DEPRECATED, use OnTabStripChanged() above.
  // TODO(crbug.com/842194): Delete this and migrate callsites.
  virtual void TabDeactivated(content::WebContents* contents);

  // Sent when the active tab changes. The previously active tab is identified
  // by |old_contents| and the newly active tab by |new_contents|. |index| is
  // the index of |new_contents|. If |reason| has CHANGE_REASON_REPLACED set
  // then the web contents was replaced (see TabChangedAt). If |reason| has
  // CHANGE_REASON_USER_GESTURE set then the web contents was changed due to a
  // user input event (e.g. clicking on a tab, keystroke).
  // Note: It is possible for the selection to change while the active tab
  // remains unchanged. For example, control-click may not change the active tab
  // but does change the selection. In this case |ActiveTabChanged| is not sent.
  // If you care about any changes to the selection, override
  // TabSelectionChanged.
  // Note: |old_contents| will be NULL if there was no contents previously
  // active.
  // DEPRECATED, use OnTabStripChanged() above.
  // TODO(crbug.com/842194): Delete this and migrate callsites.
  virtual void ActiveTabChanged(content::WebContents* old_contents,
                                content::WebContents* new_contents,
                                int index,
                                int reason);

  // Sent when the selection changes in |tab_strip_model|. More precisely when
  // selected tabs, anchor tab or active tab change. |old_model| is a snapshot
  // of the selection model before the change. See also ActiveTabChanged for
  // details.
  // TODO(erikchen): |old_model| is not used outside of tests. Do not use it. It
  // will be removed soon. https://crbug.com/842194.
  // DEPRECATED, use OnTabStripChanged() above.
  // TODO(crbug.com/842194): Delete this and migrate callsites.
  virtual void TabSelectionChanged(TabStripModel* tab_strip_model,
                                   const ui::ListSelectionModel& old_model);

  // The specified WebContents at |from_index| was moved to |to_index|.
  // DEPRECATED, use OnTabStripChanged() above.
  // TODO(crbug.com/842194): Delete this and migrate callsites.
  virtual void TabMoved(content::WebContents* contents,
                        int from_index,
                        int to_index);

  // The specified WebContents at |index| changed in some way. |contents|
  // may be an entirely different object and the old value is no longer
  // available by the time this message is delivered.
  //
  // See tab_change_type.h for a description of |change_type|.
  virtual void TabChangedAt(content::WebContents* contents,
                            int index,
                            TabChangeType change_type);

  // The WebContents was replaced at the specified index. This is invoked when
  // prerendering swaps in a prerendered WebContents.
  // DEPRECATED, use OnTabStripChanged() above.
  // TODO(crbug.com/842194): Delete this and migrate callsites.
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             content::WebContents* old_contents,
                             content::WebContents* new_contents,
                             int index);

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
