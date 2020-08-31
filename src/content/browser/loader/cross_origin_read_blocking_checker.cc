// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/cross_origin_read_blocking_checker.h"

#include "base/callback.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "services/network/cross_origin_read_blocking.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_reader.h"
#include "url/origin.h"

namespace content {

// The CrossOriginReadBlockingChecker lives on the UI thread, but blobs must be
// read on IO. This class handles all blob access for
// CrossOriginReadBlockingChecker.
class CrossOriginReadBlockingChecker::BlobIOState {
 public:
  BlobIOState(base::WeakPtr<CrossOriginReadBlockingChecker> checker,
              std::unique_ptr<storage::BlobDataHandle> blob_data_handle)
      : checker_(std::move(checker)),
        blob_data_handle_(std::move(blob_data_handle)) {}

  ~BlobIOState() { DCHECK_CURRENTLY_ON(BrowserThread::IO); }

  void StartSniffing() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    blob_reader_ = blob_data_handle_->CreateReader();
    const storage::BlobReader::Status size_status = blob_reader_->CalculateSize(
        base::BindOnce(&BlobIOState::DidCalculateSize, base::Unretained(this)));
    switch (size_status) {
      case storage::BlobReader::Status::NET_ERROR:
        OnNetError();
        return;
      case storage::BlobReader::Status::IO_PENDING:
        return;
      case storage::BlobReader::Status::DONE:
        DidCalculateSize(net::OK);
        return;
    }
  }

 private:
  void DidCalculateSize(int result) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    size_t buf_size = net::kMaxBytesToSniff;
    if (buf_size > blob_reader_->total_size())
      buf_size = blob_reader_->total_size();
    buffer_ = base::MakeRefCounted<net::IOBufferWithSize>(buf_size);
    int bytes_read;
    const storage::BlobReader::Status status = blob_reader_->Read(
        buffer_.get(), buf_size, &bytes_read,
        base::BindOnce(&BlobIOState::OnReadComplete, base::Unretained(this)));
    switch (status) {
      case storage::BlobReader::Status::NET_ERROR:
        OnNetError();
        return;
      case storage::BlobReader::Status::IO_PENDING:
        return;
      case storage::BlobReader::Status::DONE:
        OnReadComplete(bytes_read);
        return;
    }
  }

  void OnNetError() {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&CrossOriginReadBlockingChecker::OnNetError,
                                  checker_, blob_reader_->net_error()));
  }

  void OnReadComplete(int bytes_read) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&CrossOriginReadBlockingChecker::OnReadComplete,
                       checker_, bytes_read, buffer_,
                       blob_reader_->net_error()));
  }

  // |checker_| should only be accessed on the thread the navigation loader is
  // running on.
  base::WeakPtr<CrossOriginReadBlockingChecker> checker_;

  scoped_refptr<net::IOBufferWithSize> buffer_;
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle_;
  std::unique_ptr<storage::BlobReader> blob_reader_;
};

CrossOriginReadBlockingChecker::CrossOriginReadBlockingChecker(
    const network::ResourceRequest& request,
    const network::mojom::URLResponseHead& response,
    const url::Origin& request_initiator_site_lock,
    const storage::BlobDataHandle& blob_data_handle,
    base::OnceCallback<void(Result)> callback)
    : callback_(std::move(callback)) {
  DCHECK(!callback_.is_null());
  network::CrossOriginReadBlocking::LogAction(
      network::CrossOriginReadBlocking::Action::kResponseStarted);

  // |isolated_world_origin| and |network_service_client| are only used for UMA
  // and UKM logging related to the OOR-CORS feature.  Since OOR-CORS is not
  // used in scenarios relevant to CrossOriginReadBlockingChecker, we can just
  // use |base::nullopt| and |nullptr| here.
  const base::Optional<url::Origin> isolated_world_origin = base::nullopt;
  constexpr network::mojom::NetworkServiceClient* network_service_client =
      nullptr;

  corb_analyzer_ =
      std::make_unique<network::CrossOriginReadBlocking::ResponseAnalyzer>(
          request.url, request.request_initiator, response,
          request_initiator_site_lock, request.mode, isolated_world_origin,
          network_service_client);
  if (corb_analyzer_->ShouldBlock()) {
    OnBlocked();
    return;
  }
  if (corb_analyzer_->needs_sniffing()) {
    blob_io_state_ = std::make_unique<BlobIOState>(
        weak_factory_.GetWeakPtr(),
        std::make_unique<storage::BlobDataHandle>(blob_data_handle));
    // base::Unretained is safe because |blob_io_state_| will be deleted on
    // the IO thread.
    base::PostTask(FROM_HERE, {BrowserThread::IO},
                   base::BindOnce(&BlobIOState::StartSniffing,
                                  base::Unretained(blob_io_state_.get())));
    return;
  }
  DCHECK(corb_analyzer_->ShouldAllow());
  OnAllowed();
}

CrossOriginReadBlockingChecker::~CrossOriginReadBlockingChecker() {
  base::DeleteSoon(FROM_HERE, {BrowserThread::IO}, std::move(blob_io_state_));
}

int CrossOriginReadBlockingChecker::GetNetError() {
  return net_error_;
}

void CrossOriginReadBlockingChecker::OnAllowed() {
  corb_analyzer_->LogAllowedResponse();
  std::move(callback_).Run(Result::kAllowed);
}

void CrossOriginReadBlockingChecker::OnBlocked() {
  corb_analyzer_->LogBlockedResponse();
  std::move(callback_).Run(corb_analyzer_->ShouldReportBlockedResponse()
                               ? Result::kBlocked_ShouldReport
                               : Result::kBlocked_ShouldNotReport);
}

void CrossOriginReadBlockingChecker::OnNetError(int net_error) {
  net_error_ = net_error;
  std::move(callback_).Run(Result::kNetError);
}

void CrossOriginReadBlockingChecker::OnReadComplete(
    int bytes_read,
    scoped_refptr<net::IOBufferWithSize> buffer,
    int net_error) {
  if (bytes_read != buffer->size()) {
    OnNetError(net_error);
    return;
  }
  base::StringPiece data(buffer->data(), bytes_read);
  corb_analyzer_->SniffResponseBody(data, 0);
  if (corb_analyzer_->ShouldBlock()) {
    OnBlocked();
    return;
  }
  OnAllowed();
}

}  // namespace content
