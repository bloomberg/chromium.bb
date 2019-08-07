// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEBUI_WEB_UI_IOS_MESSAGE_HANDLER_H_
#define IOS_WEB_PUBLIC_WEBUI_WEB_UI_IOS_MESSAGE_HANDLER_H_

#include "base/strings/string16.h"

namespace base {
class ListValue;
}

namespace web {

class WebUIIOS;
class WebUIIOSImpl;

// Messages sent from the DOM are forwarded via the WebUIIOS to handler
// classes. These objects are owned by WebUIIOS and destroyed when the
// host is destroyed.
class WebUIIOSMessageHandler {
 public:
  WebUIIOSMessageHandler() : web_ui_(NULL) {}
  virtual ~WebUIIOSMessageHandler() {}

 protected:
  // Helper methods:

  // Extract an integer value from a list Value.
  static bool ExtractIntegerValue(const base::ListValue* value, int* out_int);

  // Extract a floating point (double) value from a list Value.
  static bool ExtractDoubleValue(const base::ListValue* value,
                                 double* out_value);

  // Extract a string value from a list Value.
  static base::string16 ExtractStringValue(const base::ListValue* value);

  // This is where subclasses specify which messages they'd like to handle and
  // perform any additional initialization. At this point web_ui() will return
  // the associated WebUIIOS object.
  virtual void RegisterMessages() = 0;

  // Returns the attached WebUIIOS for this handler.
  WebUIIOS* web_ui() const { return web_ui_; }

  // Sets the attached WebUIIOS - exposed to subclasses for testing purposes.
  void set_web_ui(WebUIIOS* web_ui) { web_ui_ = web_ui; }

 private:
  // Provide external classes access to web_ui() and set_web_ui().
  friend class WebUIIOSImpl;

  WebUIIOS* web_ui_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEBUI_WEB_UI_IOS_MESSAGE_HANDLER_H_
