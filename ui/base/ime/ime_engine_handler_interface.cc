// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "ui/base/ime/ime_engine_handler_interface.h"

namespace ui {

IMEEngineHandlerInterface::KeyboardEvent::KeyboardEvent()
    : alt_key(false), ctrl_key(false), shift_key(false), caps_lock(false) {}

IMEEngineHandlerInterface::KeyboardEvent::~KeyboardEvent() {}

// ChromeOS only APIs.
#if defined(OS_CHROMEOS)

IMEEngineHandlerInterface::MenuItem::MenuItem() {}

IMEEngineHandlerInterface::MenuItem::~MenuItem() {}

IMEEngineHandlerInterface::Candidate::Candidate() {}

IMEEngineHandlerInterface::Candidate::~Candidate() {}

namespace {
// The default entry number of a page in CandidateWindowProperty.
const int kDefaultPageSize = 9;
}  // namespace

// When the default values are changed, please modify
// CandidateWindow::CandidateWindowProperty defined in chromeos/ime/ too.
IMEEngineHandlerInterface::CandidateWindowProperty::CandidateWindowProperty()
    : page_size(kDefaultPageSize),
      is_cursor_visible(true),
      is_vertical(false),
      show_window_at_composition(false) {}

IMEEngineHandlerInterface::CandidateWindowProperty::~CandidateWindowProperty() {
}

#endif

}  // namespace ui
