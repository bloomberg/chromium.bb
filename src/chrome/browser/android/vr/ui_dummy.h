// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_UI_DUMMY_H_
#define CHROME_BROWSER_ANDROID_VR_UI_DUMMY_H_

namespace vr {

extern "C" {
// Dummy function that returns 42.
__attribute__((visibility("default"))) int CreateUi();
}

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_UI_DUMMY_H_
