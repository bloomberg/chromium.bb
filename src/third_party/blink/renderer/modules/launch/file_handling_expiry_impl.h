// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_FILE_HANDLING_EXPIRY_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_FILE_HANDLING_EXPIRY_IMPL_H_

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/web_launch/file_handling_expiry.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class LocalFrame;
class LocalDOMWindow;

// Implementation of FileHandlingExpiry service, to allow the browser to query
// the expiry time of the file handling origin trial for a Document.
class MODULES_EXPORT FileHandlingExpiryImpl final
    : public mojom::blink::FileHandlingExpiry {
 public:
  static void Create(
      LocalFrame* frame,
      mojo::PendingAssociatedReceiver<mojom::blink::FileHandlingExpiry>);
  explicit FileHandlingExpiryImpl(LocalDOMWindow& frame);
  ~FileHandlingExpiryImpl() override;

  // blink::mojom::FileHandlingExpiry:
  void RequestOriginTrialExpiryTime(
      RequestOriginTrialExpiryTimeCallback callback) override;

 private:
  WeakPersistent<LocalDOMWindow> window_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_FILE_HANDLING_EXPIRY_IMPL_H_
