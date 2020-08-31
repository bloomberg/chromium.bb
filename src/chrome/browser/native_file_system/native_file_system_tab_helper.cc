// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/native_file_system/native_file_system_tab_helper.h"

#include "chrome/browser/native_file_system/chrome_native_file_system_permission_context.h"
#include "chrome/browser/native_file_system/native_file_system_permission_context_factory.h"
#include "content/public/browser/navigation_handle.h"

NativeFileSystemTabHelper::~NativeFileSystemTabHelper() = default;

void NativeFileSystemTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation) {
  // We only care about top-level navigations that actually committed.
  if (!navigation->IsInMainFrame() || !navigation->HasCommitted())
    return;

  auto src_origin = url::Origin::Create(navigation->GetPreviousURL());
  auto dest_origin = url::Origin::Create(navigation->GetURL());

  if (src_origin == dest_origin)
    return;

  // Navigated away from |src_origin|, tell permission context to check if
  // permissions need to be revoked.
  auto* context =
      NativeFileSystemPermissionContextFactory::GetForProfileIfExists(
          web_contents()->GetBrowserContext());
  if (context)
    context->NavigatedAwayFromOrigin(src_origin);
}

void NativeFileSystemTabHelper::WebContentsDestroyed() {
  auto src_origin = url::Origin::Create(web_contents()->GetLastCommittedURL());

  // Navigated away from |src_origin|, tell permission context to check if
  // permissions need to be revoked.
  auto* context =
      NativeFileSystemPermissionContextFactory::GetForProfileIfExists(
          web_contents()->GetBrowserContext());
  if (context)
    context->NavigatedAwayFromOrigin(src_origin);
}

NativeFileSystemTabHelper::NativeFileSystemTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(NativeFileSystemTabHelper)
