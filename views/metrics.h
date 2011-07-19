// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_METRICS_H_
#define VIEWS_METRICS_H_
#pragma once

namespace views {

// NOTE: All times in this file are/should be expressed in milliseconds.

// The default value for how long to wait before showing a menu button on hover.
// This value is used if the OS doesn't supply one.
extern const int kDefaultMenuShowDelay;

// Returns the amount of time between double clicks.
int GetDoubleClickInterval();

// Returns the amount of time to wait from hovering over a menu button until
// showing the menu.
int GetMenuShowDelay();

}  // namespace views

#endif  // VIEWS_METRICS_H_
