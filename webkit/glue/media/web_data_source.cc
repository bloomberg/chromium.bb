// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/filters.h"
#include "webkit/glue/media/web_data_source.h"

namespace webkit_glue {

WebDataSource::WebDataSource()
    : media::DataSource() {
}

WebDataSource::~WebDataSource() {
}

}  // namespace webkit_glue
