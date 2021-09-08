// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/service_worker/service_worker_storage_test_utils.h"

#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

namespace {

void ReadDataPipeInternal(mojo::DataPipeConsumerHandle handle,
                          std::string* result,
                          base::OnceClosure quit_closure) {
  while (true) {
    uint32_t num_bytes;
    const void* buffer = nullptr;
    MojoResult rv =
        handle.BeginReadData(&buffer, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
    switch (rv) {
      case MOJO_RESULT_BUSY:
      case MOJO_RESULT_INVALID_ARGUMENT:
        NOTREACHED();
        return;
      case MOJO_RESULT_FAILED_PRECONDITION:
        std::move(quit_closure).Run();
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(&ReadDataPipeInternal, handle, result,
                                      std::move(quit_closure)));
        return;
      case MOJO_RESULT_OK:
        ASSERT_NE(buffer, nullptr);
        ASSERT_GT(num_bytes, 0u);
        uint32_t before_size = result->size();
        result->append(static_cast<const char*>(buffer), num_bytes);
        uint32_t read_size = result->size() - before_size;
        EXPECT_EQ(num_bytes, read_size);
        rv = handle.EndReadData(read_size);
        EXPECT_EQ(rv, MOJO_RESULT_OK);
        break;
    }
  }
  NOTREACHED();
  return;
}

}  // namespace

FakeServiceWorkerDataPipeStateNotifier::
    FakeServiceWorkerDataPipeStateNotifier() = default;

FakeServiceWorkerDataPipeStateNotifier::
    ~FakeServiceWorkerDataPipeStateNotifier() = default;

mojo::PendingRemote<mojom::ServiceWorkerDataPipeStateNotifier>
FakeServiceWorkerDataPipeStateNotifier::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

int32_t FakeServiceWorkerDataPipeStateNotifier::WaitUntilComplete() {
  if (!complete_status_.has_value()) {
    base::RunLoop loop;
    on_complete_callback_ = loop.QuitClosure();
    loop.Run();
    DCHECK(complete_status_.has_value());
  }
  return *complete_status_;
}

void FakeServiceWorkerDataPipeStateNotifier::OnComplete(int32_t status) {
  complete_status_ = status;
  if (on_complete_callback_)
    std::move(on_complete_callback_).Run();
}

namespace test {

std::string ReadDataPipeViaRunLoop(mojo::ScopedDataPipeConsumerHandle handle) {
  EXPECT_TRUE(handle.is_valid());
  std::string result;
  base::RunLoop loop;
  ReadDataPipeInternal(handle.get(), &result, loop.QuitClosure());
  loop.Run();
  return result;
}

}  // namespace test
}  // namespace storage
