// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_DETECTION_UTILS_WIN_H_
#define SERVICES_SHAPE_DETECTION_DETECTION_UTILS_WIN_H_

#include <windows.storage.streams.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <wrl/implements.h>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
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
// upon completion, |callback_| will not be run if its object has been already
// destroyed.
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
  static HRESULT BeginAsyncOperation(Callback callback,
                                     IAsyncOperationPtr async_op_ptr) {
    auto instance =
        new AsyncOperation<RuntimeType>(std::move(callback), async_op_ptr);

    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::SequencedTaskRunnerHandle::Get();

    typedef WRL::Implements<WRL::RuntimeClassFlags<WRL::ClassicCom>,
                            IAsyncOperationCompletedHandler<RuntimeType*>,
                            WRL::FtmBase>
        AsyncCallback;
    auto async_callback = WRL::Callback<AsyncCallback>(
        [instance, task_runner](IAsyncOperation<RuntimeType*>* async_op,
                                AsyncStatus status) {
          // A reference to |async_op| is kept in |async_op_ptr_|, safe to pass
          // outside.  This is happening on an OS thread.
          task_runner->PostTask(
              FROM_HERE, base::BindOnce(&AsyncOperation::AsyncCallbackInternal,
                                        base::Owned(instance),
                                        base::Unretained(async_op), status));

          return S_OK;
        });

    return async_op_ptr->put_Completed(async_callback.Get());
  }

 private:
  AsyncOperation(Callback callback, IAsyncOperationPtr async_op_ptr)
      : async_op_ptr_(std::move(async_op_ptr)),
        callback_(std::move(callback)) {}

  void AsyncCallbackInternal(IAsyncOperation<RuntimeType*>* async_op,
                             AsyncStatus status) {
    DCHECK_EQ(async_op, async_op_ptr_.Get());

    std::move(callback_).Run((async_op && status == AsyncStatus::Completed)
                                 ? std::move(async_op_ptr_)
                                 : nullptr);
  }

  IAsyncOperationPtr async_op_ptr_;
  Callback callback_;

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
