// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/public/web_content_area/java_script_dialog_source.h"

#include "base/logging.h"

JavaScriptDialogSource::JavaScriptDialogSource(const GURL& url,
                                               bool is_main_frame)
    : url_(url), is_main_frame_(is_main_frame) {
  DCHECK(url_.is_valid());
}

JavaScriptDialogSource::~JavaScriptDialogSource() = default;
