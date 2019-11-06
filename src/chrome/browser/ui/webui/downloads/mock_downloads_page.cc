// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads/mock_downloads_page.h"

MockPage::MockPage() : binding_(this) {}
MockPage::~MockPage() = default;

downloads::mojom::PagePtr MockPage::BindAndGetPtr() {
  DCHECK(!binding_.is_bound());
  downloads::mojom::PagePtr page;
  binding_.Bind(mojo::MakeRequest(&page));
  return page;
}
