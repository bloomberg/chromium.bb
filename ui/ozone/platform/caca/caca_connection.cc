// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/caca/caca_connection.h"

#include "base/logging.h"

namespace ui {

namespace {

// Size of initial cnavas (in characters).
const int kDefaultCanvasWidth = 160;
const int kDefaultCanvasHeight = 48;

}  // namespace

CacaConnection::CacaConnection() {
}

CacaConnection::~CacaConnection() {
}

bool CacaConnection::Initialize() {
  if (display_)
    return true;

  canvas_.reset(caca_create_canvas(kDefaultCanvasWidth, kDefaultCanvasHeight));
  if (!canvas_) {
    PLOG(ERROR) << "failed to create libcaca canvas";
    return false;
  }

  display_.reset(caca_create_display(canvas_.get()));
  if (!display_) {
    PLOG(ERROR) << "failed to initialize libcaca display";
    return false;
  }

  caca_set_cursor(display_.get(), 1);
  caca_set_display_title(display_.get(), "Ozone Content Shell");

  UpdateDisplaySize();

  return true;
}

void CacaConnection::UpdateDisplaySize() {
  physical_size_.SetSize(caca_get_canvas_width(canvas_.get()),
                         caca_get_canvas_height(canvas_.get()));

  bitmap_size_.SetSize(caca_get_display_width(display_.get()),
                       caca_get_display_height(display_.get()));
}

}  // namespace ui
