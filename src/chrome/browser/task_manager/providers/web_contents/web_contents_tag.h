// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_WEB_CONTENTS_TAG_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_WEB_CONTENTS_TAG_H_

#include "base/macros.h"
#include "base/supports_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace task_manager {

class RendererTask;
class WebContentsTaskProvider;

// Defines a TaskManager-specific UserData type for WebContents. This is an
// abstract base class for all concrete UserData types. They all share the same
// key. We have a concrete type for each WebContents owning service that the
// task manager is interested in tracking.
//
// To instantiate a |WebContentsTag|, use the factory functions in
// |task_manager::WebContentsTags|.
class WebContentsTag : public base::SupportsUserData::Data {
 public:
  ~WebContentsTag() override;

  // Retrieves the instance of the WebContentsTag that was attached to the
  // specified WebContents and returns it. If no instance was attached, returns
  // nullptr.
  static const WebContentsTag* FromWebContents(
      const content::WebContents* contents);

  // The concrete Tags know how to instantiate a |RendererTask| that corresponds
  // to the owning WebContents and Service. This will be used by the
  // WebContentsTaskProvider to create the appropriate Tasks.
  //
  // The returned |RendererTask| is owned by the caller (in this case it will be
  // the provider, |task_provider|).
  virtual RendererTask* CreateTask(
      WebContentsTaskProvider* task_provider) const = 0;

  content::WebContents* web_contents() const { return web_contents_; }

 protected:
  friend class WebContentsTags;

  explicit WebContentsTag(content::WebContents* contents);

 private:
  // The user data key.
  static void* kTagKey;

  // The owning WebContents.
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsTag);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_WEB_CONTENTS_TAG_H_
