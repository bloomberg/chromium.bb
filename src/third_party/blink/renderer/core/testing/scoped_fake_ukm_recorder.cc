// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/scoped_fake_ukm_recorder.h"

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/metrics/public/mojom/ukm_interface.mojom-blink.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

ScopedFakeUkmRecorder::ScopedFakeUkmRecorder()
    : recorder_(std::make_unique<ukm::TestUkmRecorder>()) {
  Platform::Current()->GetBrowserInterfaceBroker()->SetBinderForTesting(
      ukm::mojom::UkmRecorderInterface::Name_,
      WTF::BindRepeating(
          [](ScopedFakeUkmRecorder* interface,
             mojo::ScopedMessagePipeHandle handle) {
            interface->SetHandle(std::move(handle));
          },
          WTF::Unretained(this)));
}

ScopedFakeUkmRecorder::~ScopedFakeUkmRecorder() {
  Platform::Current()->GetBrowserInterfaceBroker()->SetBinderForTesting(
      ukm::mojom::UkmRecorderInterface::Name_, {});
}

void ScopedFakeUkmRecorder::AddEntry(ukm::mojom::UkmEntryPtr entry) {
  recorder_->AddEntry(std::move(entry));
}

void ScopedFakeUkmRecorder::UpdateSourceURL(int64_t source_id,
                                            const std::string& url) {
  recorder_->UpdateSourceURL(source_id, GURL(url));
}

void ScopedFakeUkmRecorder::ResetRecorder() {
  recorder_ = std::make_unique<ukm::TestUkmRecorder>();
}

void ScopedFakeUkmRecorder::SetHandle(mojo::ScopedMessagePipeHandle handle) {
  receiver_ =
      std::make_unique<mojo::Receiver<ukm::mojom::UkmRecorderInterface>>(
          this, mojo::PendingReceiver<ukm::mojom::UkmRecorderInterface>(
                    std::move(handle)));
}

}  // namespace blink
