// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebTappedInfo.h"

namespace blink {

WebTappedInfo::WebTappedInfo(bool dom_tree_changed,
                             bool style_changed,
                             Node* tapped_node,
                             IntPoint tapped_position_in_viewport)
    : point_(WebPoint(tapped_position_in_viewport.X(),
                      tapped_position_in_viewport.Y())),
      web_node_(tapped_node),
      page_changed_(dom_tree_changed || style_changed) {}

const WebPoint& WebTappedInfo::Position() const {
  return point_;
}

const WebNode& WebTappedInfo::GetNode() const {
  return web_node_;
}

bool WebTappedInfo::PageChanged() const {
  return page_changed_;
}

}  // namespace blink
