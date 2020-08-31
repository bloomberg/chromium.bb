// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"

namespace blink {

namespace {

class ErroredBytesConsumer final : public BytesConsumer {
 public:
  explicit ErroredBytesConsumer(const Error& error) : error_(error) {}

  Result BeginRead(const char** buffer, size_t* available) override {
    *buffer = nullptr;
    *available = 0;
    return Result::kError;
  }
  Result EndRead(size_t read_size) override {
    NOTREACHED();
    return Result::kError;
  }
  void SetClient(BytesConsumer::Client*) override {}
  void ClearClient() override {}

  void Cancel() override {}
  PublicState GetPublicState() const override { return PublicState::kErrored; }
  Error GetError() const override { return error_; }
  String DebugName() const override { return "ErroredBytesConsumer"; }

 private:
  const Error error_;
};

class ClosedBytesConsumer final : public BytesConsumer {
 public:
  Result BeginRead(const char** buffer, size_t* available) override {
    *buffer = nullptr;
    *available = 0;
    return Result::kDone;
  }
  Result EndRead(size_t read_size) override {
    NOTREACHED();
    return Result::kError;
  }
  void SetClient(BytesConsumer::Client*) override {}
  void ClearClient() override {}

  void Cancel() override {}
  PublicState GetPublicState() const override { return PublicState::kClosed; }
  Error GetError() const override {
    NOTREACHED();
    return Error();
  }
  String DebugName() const override { return "ClosedBytesConsumer"; }
};

}  // namespace

BytesConsumer* BytesConsumer::CreateErrored(const BytesConsumer::Error& error) {
  return MakeGarbageCollected<ErroredBytesConsumer>(error);
}

BytesConsumer* BytesConsumer::CreateClosed() {
  return MakeGarbageCollected<ClosedBytesConsumer>();
}

}  // namespace blink
