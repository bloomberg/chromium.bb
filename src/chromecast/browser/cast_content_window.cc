// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_content_window.h"

namespace chromecast {
namespace shell {

CastContentWindow::CastContentWindow() = default;

CastContentWindow::~CastContentWindow() = default;

void CastContentWindow::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void CastContentWindow::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

}  // namespace shell
}  // namespace chromecast
