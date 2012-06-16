// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/url_fetcher.h"

#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"

namespace remoting {

const int kBufferSize = 4096;

class UrlFetcher::Core : public base::RefCountedThreadSafe<Core>,
                         public net::URLRequest::Delegate {
 public:
  Core(const GURL& url, Method method);

  void SetRequestContext(
      net::URLRequestContextGetter* request_context_getter);
  void SetUploadData(const std::string& upload_content_type,
                     const std::string& upload_content);
  void SetHeader(const std::string& key, const std::string& value);
  void Start(const UrlFetcher::DoneCallback& done_callback);

  void Detach();

  // net::UrlRequest::Delegate interface.
  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE;
  virtual void OnReadCompleted(net::URLRequest* request,
                               int bytes_read) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<Core>;
  virtual ~Core();

  // Helper methods called on the IO thread.
  void DoStart();
  void ReadResponse();
  void CancelRequest();

  // Helper called on the delegate thread to invoke |done_callback_|.
  void CallCallback(const net::URLRequestStatus& status,
                    int response_code);

  GURL url_;
  Method method_;

  scoped_refptr<base::MessageLoopProxy> delegate_message_loop_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  std::string upload_content_;
  std::string upload_content_type_;

  net::HttpRequestHeaders request_headers_;

  scoped_ptr<net::URLRequest> request_;

  scoped_refptr<net::IOBuffer> buffer_;
  std::string data_;

  UrlFetcher::DoneCallback done_callback_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

UrlFetcher::Core::Core(const GURL& url, Method method)
    : url_(url),
      method_(method),
      delegate_message_loop_(base::MessageLoopProxy::current()),
      buffer_(new net::IOBuffer(kBufferSize)) {
  CHECK(url_.is_valid());
}

UrlFetcher::Core::~Core() {
}

void UrlFetcher::Core::SetRequestContext(
    net::URLRequestContextGetter* request_context_getter) {
  DCHECK(!request_context_getter_);
  request_context_getter_ = request_context_getter;
}

void UrlFetcher::Core::SetUploadData(const std::string& upload_content_type,
                                     const std::string& upload_content) {
  upload_content_type_ = upload_content_type;
  upload_content_ = upload_content;
}

void UrlFetcher::Core::SetHeader(const std::string& key,
                                 const std::string& value) {
  request_headers_.SetHeader(key, value);
}

void UrlFetcher::Core::Start(const UrlFetcher::DoneCallback& done_callback) {
  done_callback_ = done_callback;
  network_task_runner_ = request_context_getter_->GetNetworkTaskRunner();
  DCHECK(network_task_runner_);
  network_task_runner_->PostTask(FROM_HERE, base::Bind(
      &UrlFetcher::Core::DoStart, this));
}

void UrlFetcher::Core::Detach() {
  DCHECK(delegate_message_loop_->BelongsToCurrentThread());
  network_task_runner_->PostTask(
      FROM_HERE, base::Bind(&UrlFetcher::Core::CancelRequest, this));
  done_callback_.Reset();
}

void UrlFetcher::Core::OnResponseStarted(net::URLRequest* request) {
  DCHECK_EQ(request, request_.get());
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  ReadResponse();
}

void UrlFetcher::Core::OnReadCompleted(net::URLRequest* request,
                                       int bytes_read) {
  DCHECK_EQ(request, request_.get());
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  do {
    if (!request_->status().is_success() || bytes_read <= 0)
      break;

    data_.append(buffer_->data(), bytes_read);
  } while (request_->Read(buffer_, kBufferSize, &bytes_read));

  const net::URLRequestStatus status = request_->status();
  if (!status.is_io_pending()) {
    // We are done. Post a task to call |done_callback_|.
    delegate_message_loop_->PostTask(
        FROM_HERE, base::Bind(&UrlFetcher::Core::CallCallback, this, status,
                              request_->GetResponseCode()));
  }
}

void UrlFetcher::Core::DoStart() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  request_.reset(new net::URLRequest(url_, this));
  request_->set_context(request_context_getter_->GetURLRequestContext());

  switch (method_) {
    case GET:
      break;

    case POST:
      DCHECK(!upload_content_.empty());
      DCHECK(!upload_content_type_.empty());
      request_->set_method("POST");

      request_headers_.SetHeader(net::HttpRequestHeaders::kContentType,
                                 upload_content_type_);

      request_->AppendBytesToUpload(
          upload_content_.data(), static_cast<int>(upload_content_.length()));
      break;
  }

  request_->SetExtraRequestHeaders(request_headers_);

  request_->Start();
}

void UrlFetcher::Core::ReadResponse() {
  int bytes_read = 0;
  if (request_->status().is_success()) {
    request_->Read(buffer_, kBufferSize, &bytes_read);
  }
  OnReadCompleted(request_.get(), bytes_read);
}

void UrlFetcher::Core::CallCallback(const net::URLRequestStatus& status,
                                    int response_code) {
  DCHECK(delegate_message_loop_->BelongsToCurrentThread());
  if (!done_callback_.is_null()) {
    done_callback_.Run(status, response_code, data_);
  }
}

void UrlFetcher::Core::CancelRequest() {
  if (request_.get()) {
    request_->Cancel();
    request_.reset();
  }
}

UrlFetcher::UrlFetcher(const GURL& url, Method method)
    : core_ (new Core(url, method)) {
}

UrlFetcher::~UrlFetcher() {
  core_->Detach();
}

void UrlFetcher::SetRequestContext(
    net::URLRequestContextGetter* request_context_getter) {
  core_->SetRequestContext(request_context_getter);
}

void UrlFetcher::SetUploadData(const std::string& upload_content_type,
                               const std::string& upload_content) {
  core_->SetUploadData(upload_content_type, upload_content);
}

void UrlFetcher::SetHeader(const std::string& key, const std::string& value) {
  core_->SetHeader(key, value);
}

void UrlFetcher::Start(const DoneCallback& done_callback) {
  core_->Start(done_callback);
}

}  // namespace remoting
