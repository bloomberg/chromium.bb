// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/web_contents_tags.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/task_manager/providers/web_contents/background_contents_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/devtools_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/extension_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/guest_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/portal_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/prerender_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/printing_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/tab_contents_tag.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tags_manager.h"
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "components/guest_view/browser/guest_view_base.h"
#include "extensions/browser/process_manager.h"
#endif

namespace task_manager {

#if !defined(OS_ANDROID)
namespace {

// Adds the |tag| to |contents|. It also adds the |tag| to the
// |WebContentsTagsManager|.
// Note: This will fail if |contents| is already tagged by |tag|.
void TagWebContents(content::WebContents* contents,
                    std::unique_ptr<WebContentsTag> tag,
                    void* tag_key) {
  DCHECK(contents);
  DCHECK(tag);
  DCHECK(WebContentsTag::FromWebContents(contents) == nullptr);
  WebContentsTag* tag_ptr = tag.get();
  contents->SetUserData(tag_key, std::move(tag));
  WebContentsTagsManager::GetInstance()->AddTag(tag_ptr);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)

bool IsExtensionWebContents(content::WebContents* contents) {
  DCHECK(contents);

  if (guest_view::GuestViewBase::IsGuest(contents))
    return false;

  extensions::ViewType view_type = extensions::GetViewType(contents);
  return (view_type != extensions::VIEW_TYPE_INVALID &&
          view_type != extensions::VIEW_TYPE_TAB_CONTENTS &&
          view_type != extensions::VIEW_TYPE_BACKGROUND_CONTENTS);
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace

#endif  // !defined(OS_ANDROID)

// static
void WebContentsTags::CreateForBackgroundContents(
    content::WebContents* web_contents,
    BackgroundContents* background_contents) {
#if !defined(OS_ANDROID)
  if (!WebContentsTag::FromWebContents(web_contents)) {
    TagWebContents(web_contents,
                   base::WrapUnique(new BackgroundContentsTag(
                       web_contents, background_contents)),
                   WebContentsTag::kTagKey);
  }
#endif  // !defined(OS_ANDROID)
}

// static
void WebContentsTags::CreateForDevToolsContents(
    content::WebContents* web_contents) {
#if !defined(OS_ANDROID)
  if (!WebContentsTag::FromWebContents(web_contents)) {
    TagWebContents(web_contents,
                   base::WrapUnique(new DevToolsTag(web_contents)),
                   WebContentsTag::kTagKey);
  }
#endif  // !defined(OS_ANDROID)
}

// static
void WebContentsTags::CreateForPrerenderContents(
    content::WebContents* web_contents) {
#if !defined(OS_ANDROID)
  if (!WebContentsTag::FromWebContents(web_contents)) {
    TagWebContents(web_contents,
                   base::WrapUnique(new PrerenderTag(web_contents)),
                   WebContentsTag::kTagKey);
  }
#endif  // !defined(OS_ANDROID)
}

// static
void WebContentsTags::CreateForTabContents(content::WebContents* web_contents) {
#if !defined(OS_ANDROID)
  if (!WebContentsTag::FromWebContents(web_contents)) {
    TagWebContents(web_contents,
                   base::WrapUnique(new TabContentsTag(web_contents)),
                   WebContentsTag::kTagKey);
  }
#endif  // !defined(OS_ANDROID)
}

// static
void WebContentsTags::CreateForPrintingContents(
    content::WebContents* web_contents) {
#if !defined(OS_ANDROID) && BUILDFLAG(ENABLE_PRINT_PREVIEW)
  if (!WebContentsTag::FromWebContents(web_contents)) {
    TagWebContents(web_contents,
                   base::WrapUnique(new PrintingTag(web_contents)),
                   WebContentsTag::kTagKey);
  }
#endif  // !defined(OS_ANDROID) && BUILDFLAG(ENABLE_PRINT_PREVIEW)
}

// static
void WebContentsTags::CreateForGuestContents(
    content::WebContents* web_contents) {
#if !defined(OS_ANDROID)
  DCHECK(guest_view::GuestViewBase::IsGuest(web_contents));
  if (!WebContentsTag::FromWebContents(web_contents)) {
    TagWebContents(web_contents, base::WrapUnique(new GuestTag(web_contents)),
                   WebContentsTag::kTagKey);
  }
#endif  // !defined(OS_ANDROID)
}

// static
void WebContentsTags::CreateForExtension(content::WebContents* web_contents,
                                         extensions::ViewType view_type) {
#if !defined(OS_ANDROID) && BUILDFLAG(ENABLE_EXTENSIONS)
  DCHECK(IsExtensionWebContents(web_contents));

  if (!WebContentsTag::FromWebContents(web_contents)) {
    TagWebContents(web_contents,
                   base::WrapUnique(new ExtensionTag(web_contents, view_type)),
                   WebContentsTag::kTagKey);
  }
#endif  // !defined(OS_ANDROID) && BUILDFLAG(ENABLE_EXTENSIONS)
}

// static
void WebContentsTags::CreateForPortal(content::WebContents* web_contents) {
#if !defined(OS_ANDROID)
  if (!WebContentsTag::FromWebContents(web_contents)) {
    TagWebContents(web_contents, base::WrapUnique(new PortalTag(web_contents)),
                   WebContentsTag::kTagKey);
  }
#endif  // !defined(OS_ANDROID)
}

// static
void WebContentsTags::ClearTag(content::WebContents* web_contents) {
#if !defined(OS_ANDROID)
  // Some callers may clear the tag of a contents that is currently untagged
  // (for example, it may have previously been cleared). Doing so is a no-op.
  const WebContentsTag* tag = WebContentsTag::FromWebContents(web_contents);
  if (!tag)
    return;
  WebContentsTagsManager::GetInstance()->ClearFromProvider(tag);
  web_contents->RemoveUserData(WebContentsTag::kTagKey);
#endif  // !defined(OS_ANDROID)
}

}  // namespace task_manager

