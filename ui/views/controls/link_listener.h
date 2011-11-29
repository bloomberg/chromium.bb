// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_LINK_LISTENER_H_
#define UI_VIEWS_CONTROLS_LINK_LISTENER_H_
#pragma once

namespace views {

class Link;

// An interface implemented by an object to let it know that a link was clicked.
class LinkListener {
 public:
  virtual void LinkClicked(Link* source, int event_flags) = 0;

 protected:
  virtual ~LinkListener() {}
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_LINK_LISTENER_H_
