// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_BUFFERING_BYTES_CONSUMER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_BUFFERING_BYTES_CONSUMER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// BufferingBytesConsumer is a BytesConsumer. It takes a BytesConsumer
// ("the original BytesConsumer") as a constructor parameter, and results read
// from the BufferingBytesConsumer are as same as results which would be read
// from the original BytesConsumer.
// BufferingBytesConsumer buffers reads chunks from the original BytesConsumer
// and store it until they are read, before read requests are issued from the
// client.
class PLATFORM_EXPORT BufferingBytesConsumer final
    : public BytesConsumer,
      private BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(BufferingBytesConsumer);

 public:
  // Creates a BufferingBytesConsumer. |bytes_consumer| is the original
  // BytesConsumer.
  // |bytes_consumer| must not have a client.
  explicit BufferingBytesConsumer(BytesConsumer* bytes_consumer);
  ~BufferingBytesConsumer() override;

  // After this function is called, |this| will not do buffering. Already
  // buffered data still waits to be consumed, but after all the buffered data
  // is consumed, BeginRead and EndRead will result in BeginRead and EndRead
  // calls to the original BytesConsumer.
  void StopBuffering() { is_buffering_ = false; }

  // BufferingBytesConsumer
  Result BeginRead(const char** buffer, size_t* available) override;
  Result EndRead(size_t read_size) override;
  scoped_refptr<BlobDataHandle> DrainAsBlobDataHandle(BlobSizePolicy) override;
  scoped_refptr<EncodedFormData> DrainAsFormData() override;
  mojo::ScopedDataPipeConsumerHandle DrainAsDataPipe() override;
  void SetClient(BytesConsumer::Client*) override;
  void ClearClient() override;
  void Cancel() override;
  PublicState GetPublicState() const override;
  Error GetError() const override;
  String DebugName() const override { return "BufferingBytesConsumer"; }

  void Trace(blink::Visitor*) override;

 private:
  // BufferingBytesConsumer::Client
  void OnStateChange() override;
  void BufferData();

  const Member<BytesConsumer> bytes_consumer_;
  Deque<Vector<char>> buffer_;
  size_t offset_for_first_chunk_ = 0;
  bool is_buffering_ = true;
  bool has_seen_end_of_data_ = false;
  bool has_seen_error_ = false;
  Member<BytesConsumer::Client> client_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_BUFFERING_BYTES_CONSUMER_H_
