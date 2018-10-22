// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/chrome_favicon_client.h"

#include "base/memory/singleton.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/url_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "extensions/common/constants.h"
#include "url/gurl.h"

namespace {

void RunFaviconCallbackIfNotCanceled(
    const base::CancelableTaskTracker::IsCanceledCallback& is_canceled_cb,
    const favicon_base::FaviconResultsCallback& original_callback,
    const std::vector<favicon_base::FaviconRawBitmapResult>& results) {
  if (!is_canceled_cb.Run()) {
    original_callback.Run(results);
  }
}

}  // namespace

ChromeFaviconClient::ChromeFaviconClient(Profile* profile) : profile_(profile) {
}

ChromeFaviconClient::~ChromeFaviconClient() {
}

bool ChromeFaviconClient::IsNativeApplicationURL(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) ||
         url.SchemeIs(extensions::kExtensionScheme);
}

base::CancelableTaskTracker::TaskId
ChromeFaviconClient::GetFaviconForNativeApplicationURL(
    const GURL& url,
    const std::vector<int>& desired_sizes_in_pixel,
    const favicon_base::FaviconResultsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(tracker);
  DCHECK(IsNativeApplicationURL(url));
  base::CancelableTaskTracker::IsCanceledCallback is_canceled_cb;
  base::CancelableTaskTracker::TaskId task_id =
      tracker->NewTrackedTaskId(&is_canceled_cb);
  if (task_id != base::CancelableTaskTracker::kBadTaskId) {
    ChromeWebUIControllerFactory::GetInstance()->GetFaviconForURL(
        profile_, url, desired_sizes_in_pixel,
        base::Bind(&RunFaviconCallbackIfNotCanceled, is_canceled_cb, callback));
  }
  return task_id;
}
