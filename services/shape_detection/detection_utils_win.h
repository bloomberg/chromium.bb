// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_DETECTION_UTILS_WIN_H_
#define SERVICES_SHAPE_DETECTION_DETECTION_UTILS_WIN_H_

#include <windows.storage.streams.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <wrl/implements.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"

class SkBitmap;

namespace shape_detection {

namespace WRL = Microsoft::WRL;

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::IAsyncOperationCompletedHandler;
using ABI::Windows::Graphics::Imaging::ISoftwareBitmapStatics;
using ABI::Windows::Graphics::Imaging::ISoftwareBitmap;
using ABI::Windows::Graphics::Imaging::BitmapPixelFormat;

// This template represents an asynchronous operation which returns a result
// upon completion, internal async callback will be not called if the instance
// is deleted. RuntimeType is Windows Runtime APIs that has a result.
// TODO(junwei.fu): https://crbug.com/791371 consider moving the implementation
// of AsyncOperation to .cc file.
template <typename RuntimeType>
class AsyncOperation {
 public:
  using IAsyncOperationPtr = WRL::ComPtr<IAsyncOperation<RuntimeType*>>;
  // A callback run when the asynchronous operation completes. The callback is
  // run with the IAsyncOperation that completed on success, or with an empty
  // pointer in case of failure.
  using Callback = base::OnceCallback<void(IAsyncOperationPtr)>;

  ~AsyncOperation() = default;

  // Creates an AsyncOperation instance which sets |callback| to be called when
  // the asynchronous action completes.
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
              FROM_HERE, base::BindOnce(&AsyncOperation::AsyncCallbackInternal,
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
  // TODO(junwei.fu): https://crbug.com/790843 guarantee |callback_| will be
  // called instead of canceling the callback if this object is freed.
  base::WeakPtrFactory<AsyncOperation<RuntimeType>> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AsyncOperation);
};

// Creates a Windows ISoftwareBitmap from a kN32_SkColorType |bitmap|, or
// returns nullptr.
WRL::ComPtr<ISoftwareBitmap> CreateWinBitmapFromSkBitmap(
    const SkBitmap& bitmap,
    ISoftwareBitmapStatics* bitmap_factory);

// Creates a Gray8/Nv12 ISoftwareBitmap from a kN32_SkColorType |bitmap|, or
// returns nullptr.
WRL::ComPtr<ISoftwareBitmap> CreateWinBitmapWithPixelFormat(
    const SkBitmap& bitmap,
    ISoftwareBitmapStatics* bitmap_factory,
    BitmapPixelFormat pixel_format);

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_DETECTION_UTILS_WIN_H_
