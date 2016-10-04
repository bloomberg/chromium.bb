// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/DataConsumerHandleUtil.h"

#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

using Result = WebDataConsumerHandle::Result;

namespace {

class WaitingHandle final : public WebDataConsumerHandle {
 private:
  class ReaderImpl final : public WebDataConsumerHandle::Reader {
   public:
    Result beginRead(const void** buffer,
                     WebDataConsumerHandle::Flags,
                     size_t* available) override {
      *available = 0;
      *buffer = nullptr;
      return ShouldWait;
    }
    Result endRead(size_t) override { return UnexpectedError; }
  };
  std::unique_ptr<Reader> obtainReader(Client*) override {
    return WTF::wrapUnique(new ReaderImpl);
  }

  const char* debugName() const override { return "WaitingHandle"; }
};

class WebToFetchDataConsumerHandleAdapter : public FetchDataConsumerHandle {
 public:
  WebToFetchDataConsumerHandleAdapter(
      std::unique_ptr<WebDataConsumerHandle> handle)
      : m_handle(std::move(handle)) {}

 private:
  class ReaderImpl final : public FetchDataConsumerHandle::Reader {
   public:
    ReaderImpl(std::unique_ptr<WebDataConsumerHandle::Reader> reader)
        : m_reader(std::move(reader)) {}
    Result read(void* data,
                size_t size,
                Flags flags,
                size_t* readSize) override {
      return m_reader->read(data, size, flags, readSize);
    }

    Result beginRead(const void** buffer,
                     Flags flags,
                     size_t* available) override {
      return m_reader->beginRead(buffer, flags, available);
    }
    Result endRead(size_t readSize) override {
      return m_reader->endRead(readSize);
    }

   private:
    std::unique_ptr<WebDataConsumerHandle::Reader> m_reader;
  };

  std::unique_ptr<Reader> obtainFetchDataReader(Client* client) override {
    return WTF::wrapUnique(new ReaderImpl(m_handle->obtainReader(client)));
  }

  const char* debugName() const override { return m_handle->debugName(); }

  std::unique_ptr<WebDataConsumerHandle> m_handle;
};

}  // namespace

std::unique_ptr<WebDataConsumerHandle> createWaitingDataConsumerHandle() {
  return wrapUnique(new WaitingHandle);
}

std::unique_ptr<FetchDataConsumerHandle>
createFetchDataConsumerHandleFromWebHandle(
    std::unique_ptr<WebDataConsumerHandle> handle) {
  return wrapUnique(new WebToFetchDataConsumerHandleAdapter(std::move(handle)));
}

}  // namespace blink
