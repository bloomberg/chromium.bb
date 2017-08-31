// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/copy_output_request_struct_traits.h"

#include <utility>

#include "base/bind.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace {

// When we're sending a CopyOutputRequest, we keep the result_callback_ in a
// CopyOutputResultSenderImpl and send a CopyOutputResultSenderPtr to the other
// process. When SendResult is called, we run the stored result_callback_.
class CopyOutputResultSenderImpl : public viz::mojom::CopyOutputResultSender {
 public:
  CopyOutputResultSenderImpl(
      viz::CopyOutputRequest::CopyOutputRequestCallback result_callback)
      : result_callback_(std::move(result_callback)) {
    DCHECK(result_callback_);
  }

  ~CopyOutputResultSenderImpl() override {
    if (result_callback_) {
      std::move(result_callback_)
          .Run(viz::CopyOutputResult::CreateEmptyResult());
    }
  }

  // mojom::TextureMailboxReleaser implementation:
  void SendResult(std::unique_ptr<viz::CopyOutputResult> result) override {
    if (!result_callback_)
      return;
    std::move(result_callback_).Run(std::move(result));
  }

 private:
  viz::CopyOutputRequest::CopyOutputRequestCallback result_callback_;
};

void SendResult(viz::mojom::CopyOutputResultSenderPtr ptr,
                std::unique_ptr<viz::CopyOutputResult> result) {
  ptr->SendResult(std::move(result));
}

}  // namespace

namespace mojo {

// static
viz::mojom::CopyOutputResultSenderPtr
StructTraits<viz::mojom::CopyOutputRequestDataView,
             std::unique_ptr<viz::CopyOutputRequest>>::
    result_sender(const std::unique_ptr<viz::CopyOutputRequest>& request) {
  viz::mojom::CopyOutputResultSenderPtr result_sender;
  auto impl = std::make_unique<CopyOutputResultSenderImpl>(
      std::move(request->result_callback_));
  MakeStrongBinding(std::move(impl), MakeRequest(&result_sender));
  return result_sender;
}

// static
bool StructTraits<viz::mojom::CopyOutputRequestDataView,
                  std::unique_ptr<viz::CopyOutputRequest>>::
    Read(viz::mojom::CopyOutputRequestDataView data,
         std::unique_ptr<viz::CopyOutputRequest>* out_p) {
  auto request = viz::CopyOutputRequest::CreateEmptyRequest();

  request->force_bitmap_result_ = data.force_bitmap_result();

  if (!data.ReadSource(&request->source_))
    return false;

  if (!data.ReadArea(&request->area_))
    return false;

  if (!data.ReadTextureMailbox(&request->texture_mailbox_))
    return false;

  auto result_sender =
      data.TakeResultSender<viz::mojom::CopyOutputResultSenderPtr>();
  request->result_callback_ =
      base::BindOnce(SendResult, base::Passed(&result_sender));

  *out_p = std::move(request);

  return true;
}

}  // namespace mojo
