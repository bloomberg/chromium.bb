// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/resolve_host_request.h"

#include <utility>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"

namespace network {

ResolveHostRequest::ResolveHostRequest(
    net::HostResolver* resolver,
    const net::HostPortPair& host,
    const base::Optional<net::HostResolver::ResolveHostParameters>&
        optional_parameters,
    net::NetLog* net_log) {
  DCHECK(resolver);
  DCHECK(net_log);

  internal_request_ = resolver->CreateRequest(
      host, net::NetLogWithSource::Make(net_log, net::NetLogSourceType::NONE),
      optional_parameters);
}

ResolveHostRequest::~ResolveHostRequest() {
  if (control_handle_binding_.is_bound())
    control_handle_binding_.Close();

  if (response_client_.is_bound()) {
    response_client_->OnComplete(net::ERR_FAILED, base::nullopt);
    response_client_ = nullptr;
  }
}

int ResolveHostRequest::Start(
    mojom::ResolveHostHandleRequest control_handle_request,
    mojom::ResolveHostClientPtr response_client,
    net::CompletionOnceCallback callback) {
  DCHECK(internal_request_);
  DCHECK(!control_handle_binding_.is_bound());
  DCHECK(!response_client_.is_bound());

  // Unretained |this| reference is safe because if |internal_request_| goes out
  // of scope, it will cancel the request and ResolveHost() will not call the
  // callback.
  int rv = internal_request_->Start(
      base::BindOnce(&ResolveHostRequest::OnComplete, base::Unretained(this)));
  if (rv != net::ERR_IO_PENDING) {
    response_client->OnComplete(rv, GetAddressResults());
    return rv;
  }

  if (control_handle_request)
    control_handle_binding_.Bind(std::move(control_handle_request));

  response_client_ = std::move(response_client);
  // Unretained |this| reference is safe because connection error cannot occur
  // if |response_client_| goes out of scope.
  response_client_.set_connection_error_handler(base::BindOnce(
      &ResolveHostRequest::Cancel, base::Unretained(this), net::ERR_FAILED));

  callback_ = std::move(callback);

  return net::ERR_IO_PENDING;
}

void ResolveHostRequest::Cancel(int error) {
  DCHECK_NE(net::OK, error);

  if (cancelled_)
    return;

  internal_request_ = nullptr;
  cancelled_ = true;
  OnComplete(error);
}

void ResolveHostRequest::OnComplete(int error) {
  DCHECK(response_client_.is_bound());
  DCHECK(callback_);

  control_handle_binding_.Close();
  response_client_->OnComplete(error, GetAddressResults());
  response_client_ = nullptr;

  // Invoke completion callback last as it may delete |this|.
  std::move(callback_).Run(error);
}

const base::Optional<net::AddressList>& ResolveHostRequest::GetAddressResults()
    const {
  if (cancelled_) {
    static base::NoDestructor<base::Optional<net::AddressList>>
        cancelled_result(base::nullopt);
    return *cancelled_result;
  }

  DCHECK(internal_request_);
  return internal_request_->GetAddressResults();
}

}  // namespace network
