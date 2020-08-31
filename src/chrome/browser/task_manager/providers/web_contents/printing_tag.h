// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_PRINTING_TAG_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_PRINTING_TAG_H_

#include "base/macros.h"
#include "chrome/browser/task_manager/providers/web_contents/printing_task.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tag.h"

namespace task_manager {

// Defines a concrete UserData type for WebContents created for print previews
// and background printing.
class PrintingTag : public WebContentsTag {
 public:
  ~PrintingTag() override;

  // task_manager::WebContentsTag:
  PrintingTask* CreateTask(WebContentsTaskProvider*) const override;

 private:
  friend class WebContentsTags;

  explicit PrintingTag(content::WebContents* web_contents);

  DISALLOW_COPY_AND_ASSIGN(PrintingTag);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_PRINTING_TAG_H_
