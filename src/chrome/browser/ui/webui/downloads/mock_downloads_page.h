// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOADS_MOCK_DOWNLOADS_PAGE_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOADS_MOCK_DOWNLOADS_PAGE_H_

#include <vector>

#include "chrome/browser/ui/webui/downloads/downloads.mojom.h"

#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockPage : public downloads::mojom::Page {
 public:
  MockPage();
  ~MockPage() override;

  downloads::mojom::PagePtr BindAndGetPtr();

  MOCK_METHOD1(RemoveItem, void(int));
  MOCK_METHOD2(UpdateItem, void(int, downloads::mojom::DataPtr));
  MOCK_METHOD2(InsertItems, void(int, std::vector<downloads::mojom::DataPtr>));
  MOCK_METHOD0(ClearAll, void());

  mojo::Binding<downloads::mojom::Page> binding_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOADS_MOCK_DOWNLOADS_PAGE_H_
