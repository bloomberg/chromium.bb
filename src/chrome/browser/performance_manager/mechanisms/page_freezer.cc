// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/page_freezer.h"

#include "base/bind.h"
#include "base/task/task_traits.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/web_contents_proxy.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_result.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager {
namespace mechanism {
namespace {

// Try to freeze a page on the UI thread.
void MaybeFreezePageOnUIThread(const WebContentsProxy& contents_proxy) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* const contents = contents_proxy.Get();
  if (!contents)
    return;

  const GURL last_committed_origin =
      permissions::PermissionUtil::GetLastCommittedOriginAsURL(contents);

  // Page with the notification permission shouldn't be frozen as this is a
  // strong signal that the user wants to receive updates from this page while
  // it's in background. This information isn't available in the PM graph, this
  // has to be checked on the UI thread.
  auto notif_permission =
      PermissionManagerFactory::GetForProfile(
          Profile::FromBrowserContext(contents->GetBrowserContext()))
          ->GetPermissionStatus(ContentSettingsType::NOTIFICATIONS,
                                last_committed_origin, last_committed_origin);
  if (notif_permission.content_setting == CONTENT_SETTING_ALLOW)
    return;

  contents->SetPageFrozen(true);
}

void UnfreezePageOnUIThread(const WebContentsProxy& contents_proxy) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* const content = contents_proxy.Get();
  if (!content)
    return;

  // A visible page is automatically unfrozen.
  if (content->GetVisibility() == content::Visibility::VISIBLE)
    return;

  content->SetPageFrozen(false);
}

}  // namespace

void PageFreezer::MaybeFreezePageNode(const PageNode* page_node) {
  DCHECK(page_node);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&MaybeFreezePageOnUIThread,
                                page_node->GetContentsProxy()));
}

void PageFreezer::UnfreezePageNode(const PageNode* page_node) {
  DCHECK(page_node);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&UnfreezePageOnUIThread, page_node->GetContentsProxy()));
}

}  // namespace mechanism
}  // namespace performance_manager
