// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_clipboard_instance.h"
#include "base/bind_helpers.h"

namespace arc {

FakeClipboardInstance::FakeClipboardInstance() = default;

FakeClipboardInstance::~FakeClipboardInstance() = default;

void FakeClipboardInstance::Init(mojom::ClipboardHostPtr host_ptr,
                                 InitCallback callback) {
  std::move(callback).Run();
}

void FakeClipboardInstance::InitDeprecated(mojom::ClipboardHostPtr host_ptr) {
  Init(std::move(host_ptr), base::DoNothing());
}

void FakeClipboardInstance::OnHostClipboardUpdated() {
  num_host_clipboard_updated_++;
}

}  // namespace arc
