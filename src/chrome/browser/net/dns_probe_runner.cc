// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_probe_runner.h"

#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace chrome_browser_net {

const char DnsProbeRunner::kKnownGoodHostname[] = "google.com";

namespace {

DnsProbeRunner::Result EvaluateResponse(
    int net_error,
    const base::Optional<net::AddressList>& resolved_addresses) {
  switch (net_error) {
    case net::OK:
      break;

    case net::ERR_FAILED:
    // ERR_DNS_CACHE_MISS means HostResolver was not able to attempt DNS, e.g.
    // due to limitations from Chrome build configuration or because of
    // incompatibilities with the DNS configuration from the system. The result
    // Chrome would have gotten from DNS if attempted is therefore unknown.
    case net::ERR_DNS_CACHE_MISS:
      return DnsProbeRunner::UNKNOWN;

    // ERR_NAME_NOT_RESOLVED maps to NXDOMAIN, which means the server is working
    // but returned a wrong answer.
    case net::ERR_NAME_NOT_RESOLVED:
      return DnsProbeRunner::INCORRECT;

    // These results mean we heard *something* from the DNS server, but it was
    // unsuccessful (SERVFAIL) or malformed.
    case net::ERR_DNS_MALFORMED_RESPONSE:
    case net::ERR_DNS_SERVER_REQUIRES_TCP:  // Shouldn't happen; DnsTransaction
                                            // will retry with TCP.
    case net::ERR_DNS_SERVER_FAILED:
    case net::ERR_DNS_SORT_ERROR:  // Can only happen if the server responds.
      return DnsProbeRunner::FAILING;

    // Any other error means we never reached the DNS server in the first place.
    case net::ERR_DNS_TIMED_OUT:
    default:
      // Something else happened, probably at a network level.
      return DnsProbeRunner::UNREACHABLE;
  }

  if (!resolved_addresses) {
    // If net_error is OK, resolved_addresses should be set. The binding is not
    // closed here since it will be closed by the caller anyway.
    mojo::ReportBadMessage("resolved_addresses not set when net_error=OK");
    return DnsProbeRunner::UNKNOWN;
  } else if (resolved_addresses.value().empty()) {
    return DnsProbeRunner::INCORRECT;
  } else {
    return DnsProbeRunner::CORRECT;
  }
}

}  // namespace

DnsProbeRunner::DnsProbeRunner(
    net::DnsConfigOverrides dns_config_overrides,
    const NetworkContextGetter& network_context_getter)
    : binding_(this),
      dns_config_overrides_(dns_config_overrides),
      network_context_getter_(network_context_getter),
      result_(UNKNOWN) {
  CreateHostResolver();
}

DnsProbeRunner::~DnsProbeRunner() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DnsProbeRunner::RunProbe(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  DCHECK(host_resolver_);
  DCHECK(callback_.is_null());
  DCHECK(!binding_);

  network::mojom::ResolveHostClientPtr client_ptr;
  binding_.Bind(mojo::MakeRequest(&client_ptr));
  binding_.set_connection_error_handler(base::BindOnce(
      &DnsProbeRunner::OnMojoConnectionError, base::Unretained(this)));

  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
  parameters->dns_query_type = net::DnsQueryType::A;
  parameters->source = net::HostResolverSource::DNS;
  parameters->allow_cached_response = false;

  host_resolver_->ResolveHost(net::HostPortPair(kKnownGoodHostname, 80),
                              std::move(parameters), std::move(client_ptr));

  callback_ = std::move(callback);
}

bool DnsProbeRunner::IsRunning() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !callback_.is_null();
}

void DnsProbeRunner::OnComplete(
    int32_t result,
    const base::Optional<net::AddressList>& resolved_addresses) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback_.is_null());

  result_ = EvaluateResponse(result, resolved_addresses);
  binding_.Close();

  // ResolveHost will call OnComplete asynchronously, so callback_ can be
  // invoked directly here.  Clear callback in case it starts a new probe
  // immediately.
  std::move(callback_).Run();
}

void DnsProbeRunner::CreateHostResolver() {
  host_resolver_.reset();
  network_context_getter_.Run()->CreateHostResolver(
      dns_config_overrides_, mojo::MakeRequest(&host_resolver_));
}

void DnsProbeRunner::OnMojoConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateHostResolver();
  OnComplete(net::ERR_FAILED, base::nullopt);
}

}  // namespace chrome_browser_net
