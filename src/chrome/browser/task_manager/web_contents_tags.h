// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_WEB_CONTENTS_TAGS_H_
#define CHROME_BROWSER_TASK_MANAGER_WEB_CONTENTS_TAGS_H_

#include "base/macros.h"
#include "extensions/common/view_type.h"

class BackgroundContents;

namespace content {
class WebContents;
}  // namespace content

namespace task_manager {

// Defines a factory class for creating the TaskManager-specific Tags for the
// WebContents that are owned by various types of services.
//
// Any service or feature that creates WebContents instances (via
// WebContents::Create) needs to make sure that they are tagged using this
// mechanism, otherwise the associated render processes will not show up in the
// task manager.
class WebContentsTags {
 public:
  // Tag a BackgroundContents so that it shows up in the task manager. Calling
  // this function creates a BackgroundContentsTag, and attaches it to
  // |web_contents|. If an instance is already attached, this does nothing. The
  // resulting tag does not have to be cleaned up by the caller, as it is owned
  // by |web_contents|.
  static void CreateForBackgroundContents(
      content::WebContents* web_contents,
      BackgroundContents* background_contents);

  // Tag a DevTools WebContents so that it shows up in the task manager. Calling
  // this function creates a DevToolsTag, and attaches it to |web_contents|. If
  // an instance is already attached, this does nothing. The resulting tag does
  // not have to be cleaned up by the caller, as it is owned by |web_contents|.
  static void CreateForDevToolsContents(content::WebContents* web_contents);

  // Tag a WebContents owned by the PrerenderManager so that it shows up in the
  // task manager. Calling this function creates a PrerenderTag, and attaches it
  // to |web_contents|. If an instance is already attached, this does nothing.
  // The resulting tag does not have to be cleaned up by the caller, as it is
  // owned by |web_contents|.
  static void CreateForPrerenderContents(content::WebContents* web_contents);

  // Tag a WebContents owned by the TabStripModel so that it shows up in the
  // task manager. Calling this function creates a TabContentsTag, and attaches
  // it to |web_contents|. If an instance is already attached, this does
  // nothing. The resulting tag does not have to be cleaned up by the caller, as
  // it is owned by |web_contents|.
  static void CreateForTabContents(content::WebContents* web_contents);

  // Tag a WebContents created for a print preview or background printing so
  // that it shows up in the task manager. Calling this function creates a
  // PrintingTag, and attaches it to |web_contents|. If an instance is already
  // attached, this does nothing. The resulting tag does not have to be cleaned
  // up by the caller, as it is owned by |web_contents|.
  static void CreateForPrintingContents(content::WebContents* web_contents);

  // Tag a WebContents owned by a GuestViewBase so that it shows up in the
  // task manager. Calling this function creates a GuestTag, and attaches it to
  // |web_contents|. If an instance is already attached, this does nothing. The
  // resulting tag does not have to be cleaned up by the caller, as it is owned
  // by |web_contents|.
  static void CreateForGuestContents(content::WebContents* web_contents);

  // Tag a WebContents that belongs to |extension| so that it shows up in the
  // task manager. Calling this function creates a ExtensionTag, and attaches
  // it to |web_contents|. If an instance is already attached, this does
  // nothing. The resulting tag does not have to be cleaned up by the caller,
  // as it is owned by |web_contents|.
  // |web_contents| must be of a non-tab, non-guest view, or
  // non-background contents Extension.
  static void CreateForExtension(content::WebContents* web_contents,
                                 extensions::ViewType view_type);

  // Tag a WebContents created for a Portal so that it shows up in the task
  // manager. Calling this function creates a PortalTag, and attaches it to
  // |web_contents|. If an instance is already attached, this does nothing.
  // The resulting tag does not have to be cleaned up by the caller, as it is
  // owned by |web_contents|.
  static void CreateForPortal(content::WebContents* web_contents);

  // Clears the task-manager tag, created by any of the above functions, from
  // the given |web_contents| if any.
  // Clearing the tag is necessary only when you need to re-tag an existing
  // WebContents, to indicate a change in ownership.
  static void ClearTag(content::WebContents* web_contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentsTags);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_WEB_CONTENTS_TAGS_H_
