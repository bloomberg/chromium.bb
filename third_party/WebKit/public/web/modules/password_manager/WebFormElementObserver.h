// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFormElementObserver_h
#define WebFormElementObserver_h

#include <memory>

#include "public/platform/WebCommon.h"

namespace blink {

class WebFormElement;
class WebFormElementObserverCallback;
class WebInputElement;

class BLINK_EXPORT WebFormElementObserver {
 public:
  // Creates a WebFormElementObserver. Delete this WebFormElementObsrver by
  // calling WebFormElementObserver::Disconnect.
  static WebFormElementObserver* Create(
      WebFormElement&,
      std::unique_ptr<WebFormElementObserverCallback>);
  static WebFormElementObserver* Create(
      WebInputElement&,
      std::unique_ptr<WebFormElementObserverCallback>);

  virtual void Disconnect() = 0;

 protected:
  WebFormElementObserver() = default;
  virtual ~WebFormElementObserver() = default;
};

}  // namespace blink

#endif  // WebFormElementObserver_h
