// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_CONTEXT_MENU_CONTENT_TYPE_WEB_VIEW_H_
#define CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_CONTEXT_MENU_CONTENT_TYPE_WEB_VIEW_H_

#include "base/macros.h"
#include "components/renderer_context_menu/context_menu_content_type.h"

namespace extensions {
class Extension;
}

// A ContextMenuContentType for <webview> guest.
// Guests are rendered inside chrome apps, but have most of the actions
// that a regular web page has. Currently actions/items that are suppressed from
// guests are: searching, printing, speech and instant.
class ContextMenuContentTypeWebView : public ContextMenuContentType {
 public:
  ~ContextMenuContentTypeWebView() override;

  // ContextMenuContentType overrides.
  bool SupportsGroup(int group) override;

 protected:
  ContextMenuContentTypeWebView(content::WebContents* web_contents,
                                const content::ContextMenuParams& params);

 private:
  friend class ContextMenuContentTypeFactory;

  const extensions::Extension* GetExtension() const;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuContentTypeWebView);
};

#endif  // CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_CONTEXT_MENU_CONTENT_TYPE_WEB_VIEW_H_
