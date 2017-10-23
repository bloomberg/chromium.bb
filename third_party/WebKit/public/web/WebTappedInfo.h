// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebTappedInfo_h
#define WebTappedInfo_h

#ifdef INSIDE_BLINK
#include "platform/geometry/IntPoint.h"
#endif  // INSIDE_BLINK
#include "public/platform/WebCommon.h"
#include "public/platform/WebPoint.h"
#include "public/web/WebNode.h"

namespace blink {

struct WebPoint;

#ifdef INSIDE_BLINK
class Node;
#endif  // INSIDE_BLINK

/*
 * Encapsulates information needed by ShowUnhandledTapUIIfNeeded to describe
 * the tap.
 */
class WebTappedInfo {
 public:
#ifdef INSIDE_BLINK
  WebTappedInfo(bool dom_tree_changed,
                bool style_changed,
                Node* tapped_node,
                IntPoint tapped_position_in_viewport);
#endif  // INSIDE_BLINK

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
