// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FormDataBytesConsumer_h
#define FormDataBytesConsumer_h

#include "modules/ModulesExport.h"
#include "modules/fetch/BytesConsumer.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class DOMArrayBuffer;
class DOMArrayBufferView;
class EncodedFormData;

class FormDataBytesConsumer final : public BytesConsumer {
 public:
  explicit MODULES_EXPORT FormDataBytesConsumer(const String&);
  explicit MODULES_EXPORT FormDataBytesConsumer(DOMArrayBuffer*);
  explicit MODULES_EXPORT FormDataBytesConsumer(DOMArrayBufferView*);
  MODULES_EXPORT FormDataBytesConsumer(const void* data, size_t);
  MODULES_EXPORT FormDataBytesConsumer(ExecutionContext*,
                                       RefPtr<EncodedFormData>);
  MODULES_EXPORT static FormDataBytesConsumer* CreateForTesting(
      ExecutionContext* execution_context,
      RefPtr<EncodedFormData> form_data,
      BytesConsumer* consumer) {
    return new FormDataBytesConsumer(execution_context, std::move(form_data),
                                     consumer);
  }

  // BytesConsumer implementation
  Result BeginRead(const char** buffer, size_t* available) override {
    return impl_->BeginRead(buffer, available);
  }
  Result EndRead(size_t read_size) override {
    return impl_->EndRead(read_size);
  }
  RefPtr<BlobDataHandle> DrainAsBlobDataHandle(BlobSizePolicy policy) override {
    return impl_->DrainAsBlobDataHandle(policy);
  }
  RefPtr<EncodedFormData> DrainAsFormData() override {
    return impl_->DrainAsFormData();
  }
  void SetClient(BytesConsumer::Client* client) override {
    impl_->SetClient(client);
  }
  void ClearClient() override { impl_->ClearClient(); }
  void Cancel() override { impl_->Cancel(); }
  PublicState GetPublicState() const override {
    return impl_->GetPublicState();
  }
  Error GetError() const override { return impl_->GetError(); }
  String DebugName() const override { return impl_->DebugName(); }

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(impl_);
    BytesConsumer::Trace(visitor);
  }

 private:
  MODULES_EXPORT FormDataBytesConsumer(ExecutionContext*,
                                       RefPtr<EncodedFormData>,
                                       BytesConsumer*);

  const Member<BytesConsumer> impl_;
};

}  // namespace blink

#endif  // FormDataBytesConsumer_h
