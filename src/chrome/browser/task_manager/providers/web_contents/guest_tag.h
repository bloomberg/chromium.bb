// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_GUEST_TAG_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_GUEST_TAG_H_

#include "base/macros.h"
#include "chrome/browser/task_manager/providers/web_contents/guest_task.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tag.h"

namespace task_manager {

// Defines a concrete UserData type for WebContents owned by the GuestViewBase,
// which represents browser <*view> tag plugin guests.
class GuestTag : public WebContentsTag {
 public:
  ~GuestTag() override;

  // task_manager::WebContentsTag:
  GuestTask* CreateTask(WebContentsTaskProvider*) const override;

 private:
  friend class WebContentsTags;

  explicit GuestTag(content::WebContents* web_contents);

  DISALLOW_COPY_AND_ASSIGN(GuestTag);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_GUEST_TAG_H_
