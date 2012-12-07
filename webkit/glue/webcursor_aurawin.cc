// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webcursor.h"

#include <windows.h>

#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"

const ui::PlatformCursor WebCursor::GetPlatformCursor() {
  // TODO(winguru): Return an appropriate platform-cursor.
  return LoadCursor(NULL, IDC_ARROW);
}

void WebCursor::SetDeviceScaleFactor(float scale_factor) {
  // TODO(winguru): Scale the cursor.
}

void WebCursor::InitPlatformData() {
}

bool WebCursor::SerializePlatformData(Pickle* pickle) const {
  return true;
}

bool WebCursor::DeserializePlatformData(PickleIterator* iter) {
  return true;
}

bool WebCursor::IsPlatformDataEqual(const WebCursor& other) const {
  return true;
}

void WebCursor::CleanupPlatformData() {
}

void WebCursor::CopyPlatformData(const WebCursor& other) {
}
