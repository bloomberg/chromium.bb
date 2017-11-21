// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_DETECTION_UTILS_WIN_H_
#define SERVICES_SHAPE_DETECTION_DETECTION_UTILS_WIN_H_

#include <wrl\event.h>
#include <wrl\implements.h>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::IAsyncOperationCompletedHandler;

namespace shape_detection {

namespace WRL = Microsoft::WRL;

// This template represents an asynchronous operation which returns a result
// upon completion, internal async callback will be not called if the instance
// is deleted. RuntimeType is Windows Runtime APIs that have a result.
template <typename RuntimeType>
class AsyncOperation {
 public:
  using IAsyncOperationPtr =
      Microsoft::WRL::ComPtr<IAsyncOperation<RuntimeType*>>;
  // A callback run when the asynchronous operation completes. The callback is
  // run with the IAsyncOperation that completed on success, or with an empty
  // pointer in case of failure.
  using Callback = base::OnceCallback<void(IAsyncOperationPtr)>;

  ~AsyncOperation() = default;

  // Creates an AsyncOperation instance which set Callback to be called when the
  // asynchronous action completes.
  static std::unique_ptr<AsyncOperation<RuntimeType>> Create(
      Callback callback,
      IAsyncOperationPtr async_op_ptr) {
    auto instance = base::WrapUnique(
        new AsyncOperation<RuntimeType>(std::move(callback), async_op_ptr));

    base::WeakPtr<AsyncOperation> weak_ptr =
        instance->weak_factory_.GetWeakPtr();
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::SequencedTaskRunnerHandle::Get();

    typedef WRL::Implements<WRL::RuntimeClassFlags<WRL::ClassicCom>,
                            IAsyncOperationCompletedHandler<RuntimeType*>,
                            WRL::FtmBase>
        AsyncCallback;
    auto async_callback = WRL::Callback<AsyncCallback>(
        [weak_ptr, task_runner](IAsyncOperation<RuntimeType*>* async_op,
                                AsyncStatus status) {
          // A reference to |async_op| is kept in |async_op_ptr_|, safe to pass
          // outside.  This is happening on an OS thread.
          task_runner->PostTask(
              FROM_HERE, base::Bind(&AsyncOperation::AsyncCallbackInternal,
                                    std::move(weak_ptr), async_op, status));

          return S_OK;
        });

    const HRESULT hr = async_op_ptr->put_Completed(async_callback.Get());
    if (FAILED(hr)) {
      DLOG(ERROR) << "Async put completed failed: "
                  << logging::SystemErrorCodeToString(hr);
      return nullptr;
    }

    return instance;
  }

 private:
  AsyncOperation(Callback callback, IAsyncOperationPtr async_op_ptr)
      : async_op_ptr_(std::move(async_op_ptr)),
        callback_(std::move(callback)),
        weak_factory_(this) {}

  void AsyncCallbackInternal(IAsyncOperation<RuntimeType*>* async_op,
                             AsyncStatus status) {
    DCHECK_EQ(async_op, async_op_ptr_.Get());

    std::move(callback_).Run((async_op && status == AsyncStatus::Completed)
                                 ? std::move(async_op_ptr_)
                                 : nullptr);
  }

  IAsyncOperationPtr async_op_ptr_;
  Callback callback_;
  base::WeakPtrFactory<AsyncOperation<RuntimeType>> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AsyncOperation);
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_DETECTION_UTILS_WIN_H_