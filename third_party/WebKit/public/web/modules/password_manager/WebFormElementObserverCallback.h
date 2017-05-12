// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFormElementObserverCallback_h
#define WebFormElementObserverCallback_h

namespace blink {

class WebFormElementObserverCallback {
 public:
  virtual ~WebFormElementObserverCallback() {}

  // Invoked when the observed element was either removed from the DOM, it's
  // action attribute changed, or it's computed style changed to display: none.
  virtual void ElementWasHiddenOrRemoved() = 0;
};

}  // namespace blink

#endif  // WebFormElementObserverCallback_h
