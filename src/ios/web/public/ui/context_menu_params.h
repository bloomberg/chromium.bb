// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_WEB_PUBLIC_UI_CONTEXT_MENU_PARAMS_H_
#define IOS_WEB_PUBLIC_UI_CONTEXT_MENU_PARAMS_H_

#import <UIKit/UIKit.h>


#include "ios/web/public/navigation/referrer.h"
#include "url/gurl.h"

namespace web {

// Wraps information needed to show a context menu.
struct ContextMenuParams {
 public:
  ContextMenuParams();

  ContextMenuParams(const ContextMenuParams& other);
  ContextMenuParams& operator=(const ContextMenuParams& other);

  ContextMenuParams(ContextMenuParams&& other);
  ContextMenuParams& operator=(ContextMenuParams&& other);

  ~ContextMenuParams();

  // Whether or not the context menu was triggered from the main frame.
  bool is_main_frame;

  // The URL of the link that encloses the node the context menu was invoked on.
  GURL link_url;

  // The source URL of the element the context menu was invoked on. Example of
  // elements with source URLs are img, audio, and video.
  GURL src_url;

  // The referrer policy to use when opening the link.
  web::ReferrerPolicy referrer_policy;

  // The view in which to present the menu.
  UIView* view;

  // The location in |view| to present the menu.
  CGPoint location;

  // The text associated with the link. It is either nil or nonempty (it can not
  // be empty).
  NSString* link_text;

  // The text for the "title" attribute of the HTML element. Can be null.
  NSString* title_attribute;

  // The text for the "alt" attribute of an HTML img element. Can be null.
  NSString* alt_text;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_UI_CONTEXT_MENU_PARAMS_H_
