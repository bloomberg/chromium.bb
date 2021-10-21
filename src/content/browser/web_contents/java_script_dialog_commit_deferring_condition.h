// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_JAVA_SCRIPT_DIALOG_COMMIT_DEFERRING_CONDITION_H_
#define CONTENT_BROWSER_WEB_CONTENTS_JAVA_SCRIPT_DIALOG_COMMIT_DEFERRING_CONDITION_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/commit_deferring_condition.h"

namespace content {

class WebContentsImpl;
class NavigationRequest;

// Defers a navigation from committing while a JavaScript dialog is showing.
class JavaScriptDialogCommitDeferringCondition
    : public CommitDeferringCondition {
 public:
  static std::unique_ptr<CommitDeferringCondition> MaybeCreate(
      NavigationRequest& navigation_request);

  JavaScriptDialogCommitDeferringCondition(
      const JavaScriptDialogCommitDeferringCondition&) = delete;
  JavaScriptDialogCommitDeferringCondition& operator=(
      const JavaScriptDialogCommitDeferringCondition&) = delete;

  ~JavaScriptDialogCommitDeferringCondition() override;

  Result WillCommitNavigation(base::OnceClosure resume) override;

 private:
  JavaScriptDialogCommitDeferringCondition(NavigationRequest& request,
                                           WebContentsImpl& web_contents);

  // Bare reference is ok here because this class is indirectly owned by the
  // NavigationRequest which will be destroyed before the WebContentsImpl.
  WebContentsImpl& web_contents_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_JAVA_SCRIPT_DIALOG_COMMIT_DEFERRING_CONDITION_H_
