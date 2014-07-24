// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/network/network_info.h"

namespace ui {

NetworkInfo::NetworkInfo() : disable(false), highlight(false) {
}

NetworkInfo::NetworkInfo(const std::string& path)
    : service_path(path), disable(false), highlight(false) {
}

NetworkInfo::~NetworkInfo() {
}

}  // namespace ui
