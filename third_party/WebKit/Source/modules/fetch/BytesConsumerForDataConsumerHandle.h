// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BytesConsumerForDataConsumerHandle_h
#define BytesConsumerForDataConsumerHandle_h

#include "modules/ModulesExport.h"
#include "modules/fetch/BytesConsumer.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebDataConsumerHandle.h"

#include <memory>

namespace blink {

class ExecutionContext;

class MODULES_EXPORT BytesConsumerForDataConsumerHandle final
    : public BytesConsumer,
      public WebDataConsumerHandle::Client {
  EAGERLY_FINALIZE();
  DECLARE_EAGER_FINALIZATION_OPERATOR_NEW();

 public:
  BytesConsumerForDataConsumerHandle(ExecutionContext*,
                                     std::unique_ptr<WebDataConsumerHandle>);
  ~BytesConsumerForDataConsumerHandle() override;

  Result BeginRead(const char** buffer, size_t* available) override;
  Result EndRead(size_t read_size) override;
  void SetClient(BytesConsumer::Client*) override;
  void ClearClient() override;

  void Cancel() override;
  PublicState GetPublicState() const override;
  Error GetError() const override {
    DCHECK(state_ == InternalState::kErrored);
    return error_;
  }
  String DebugName() const override {
    return "BytesConsumerForDataConsumerHandle";
  }

  // WebDataConsumerHandle::Client
  void DidGetReadable() override;

  void Trace(blink::Visitor*);

 private:
  void Close();
  void GetError();
  void Notify();

  Member<ExecutionContext> execution_context_;
  std::unique_ptr<WebDataConsumerHandle::Reader> reader_;
  Member<BytesConsumer::Client> client_;
  InternalState state_ = InternalState::kWaiting;
  Error error_;
  bool is_in_two_phase_read_ = false;
  bool has_pending_notification_ = false;
};

}  // namespace blink

#endif  // BytesConsumerForDataConsumerHandle_h
