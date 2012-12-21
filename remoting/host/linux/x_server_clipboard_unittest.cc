// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "remoting/base/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

// X11 headers must be #included after gtest.h, since the X11 headers define
// some macros that cause errors in gtest-type-util.h.
#include <X11/Xlib.h>
#include "remoting/host/linux/x_server_clipboard.h"

namespace remoting {

namespace {

class ClipboardTestClient {
 public:
  ClipboardTestClient() : display_(NULL) {}
  ~ClipboardTestClient() {}

  void Init(Display* display) {
    display_ = display;
    clipboard_.Init(display,
                    base::Bind(&ClipboardTestClient::OnClipboardChanged,
                               base::Unretained(this)));
  }

  void SetClipboardData(const std::string& clipboard_data) {
    clipboard_data_ = clipboard_data;
    clipboard_.SetClipboard(kMimeTypeTextUtf8, clipboard_data);
  }

  void OnClipboardChanged(const std::string& mime_type,
                          const std::string& data) {
    clipboard_data_ = data;
  }

  // Process X events on the connection, returning true if any events were
  // processed.
  bool PumpXEvents() {
    bool result = false;
    while (XPending(display_)) {
      XEvent event;
      XNextEvent(display_, &event);
      clipboard_.ProcessXEvent(&event);
      result = true;
    }
    return result;
  }

  const std::string& clipboard_data() const { return clipboard_data_; }
  Display* display() const { return display_; }

 private:
  std::string clipboard_data_;
  XServerClipboard clipboard_;
  Display* display_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardTestClient);
};

}  // namespace

class XServerClipboardTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    // XSynchronize() ensures that PumpXEvents() fully processes all X server
    // requests and responses before returning to the caller.
    Display* display1 = XOpenDisplay(NULL);
    XSynchronize(display1, True);
    client1_.Init(display1);
    Display* display2 = XOpenDisplay(NULL);
    XSynchronize(display2, True);
    client2_.Init(display2);
  }

  virtual void TearDown() OVERRIDE {
    XCloseDisplay(client1_.display());
    XCloseDisplay(client2_.display());
  }

  void PumpXEvents() {
    while (true) {
      if (!client1_.PumpXEvents() && !client2_.PumpXEvents()) {
        break;
      }
    }
  }

  ClipboardTestClient client1_;
  ClipboardTestClient client2_;
};

// http://crbug.com/163428
TEST_F(XServerClipboardTest, DISABLED_CopyPaste) {
  // Verify clipboard data can be transferred more than once. Then verify that
  // the code continues to function in the opposite direction (so client1_ will
  // send then receive, and client2_ will receive then send).
  client1_.SetClipboardData("Text1");
  PumpXEvents();
  EXPECT_EQ("Text1", client2_.clipboard_data());

  client1_.SetClipboardData("Text2");
  PumpXEvents();
  EXPECT_EQ("Text2", client2_.clipboard_data());

  client2_.SetClipboardData("Text3");
  PumpXEvents();
  EXPECT_EQ("Text3", client1_.clipboard_data());

  client2_.SetClipboardData("Text4");
  PumpXEvents();
  EXPECT_EQ("Text4", client1_.clipboard_data());
}

}  // namespace remoting
