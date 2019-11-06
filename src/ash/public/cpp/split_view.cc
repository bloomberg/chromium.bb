// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/split_view.h"

#include "base/logging.h"

namespace ash {

namespace {

SplitViewNotifier* g_instance = nullptr;

}  // namespace

SplitViewObserver::SplitViewObserver() = default;

SplitViewObserver::~SplitViewObserver() = default;

// static
SplitViewNotifier* SplitViewNotifier::Get() {
  return g_instance;
}

SplitViewNotifier::SplitViewNotifier() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

SplitViewNotifier::~SplitViewNotifier() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
