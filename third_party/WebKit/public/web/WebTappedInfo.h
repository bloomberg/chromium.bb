// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebTappedInfo_h
#define WebTappedInfo_h

#include "core/dom/Node.h"
#include "platform/geometry/IntPoint.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebPoint.h"
#include "public/web/WebNode.h"

namespace blink {

struct WebPoint;

/*
 * Encapsulates information needed by ShowUnhandledTapUIIfNeeded to describe
 * the tap.
 */
class WebTappedInfo {
 public:
  WebTappedInfo(bool dom_tree_changed,
                bool style_changed,
                Node* tapped_node,
                IntPoint tapped_position_in_viewport);

  BLINK_EXPORT const WebPoint& Position() const;
  BLINK_EXPORT const WebNode& GetNode() const;
  BLINK_EXPORT bool PageChanged() const;

 private:
  const WebPoint point_;
  const WebNode web_node_;
  bool page_changed_;
};

}  // namespace blink

#endif
