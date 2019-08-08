// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/dns_blackhole_checker.h"

#include <utility>

#include "base/bind.h"
#include "remoting/base/logging.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace remoting {

// Default prefix added to the base talkgadget URL.
const char kDefaultHostTalkGadgetPrefix[] = "chromoting-host";

// The base talkgadget URL.
const char kTalkGadgetUrl[] = ".talkgadget.google.com/talkgadget/"
                              "oauth/chrome-remote-desktop-host";

DnsBlackholeChecker::DnsBlackholeChecker(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::string talkgadget_prefix)
    : url_loader_factory_(url_loader_factory),
      talkgadget_prefix_(talkgadget_prefix) {}

DnsBlackholeChecker::~DnsBlackholeChecker() = default;

// This is called in response to the TalkGadget http request initiated from
// CheckStatus().
void DnsBlackholeChecker::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  bool allow = false;
  if (response_body) {
    HOST_LOG << "Successfully connected to host talkgadget.";
    allow = true;
  } else {
    int response_code = -1;
    if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
      response_code = url_loader_->ResponseInfo()->headers->response_code();
    }
    HOST_LOG << "Unable to connect to host talkgadget (" << response_code
             << ")";
  }
  url_loader_.reset();
  std::move(callback_).Run(allow);
}

void DnsBlackholeChecker::CheckForDnsBlackhole(
    const base::Callback<void(bool)>& callback) {
  // Make sure we're not currently in the middle of a connection check.
  if (!url_loader_) {
    DCHECK(callback_.is_null());
    callback_ = callback;
    std::string talkgadget_url("https://");
    if (talkgadget_prefix_.empty()) {
      talkgadget_url += kDefaultHostTalkGadgetPrefix;
    } else {
      talkgadget_url += talkgadget_prefix_;
    }
    talkgadget_url += kTalkGadgetUrl;
    HOST_LOG << "Verifying connection to " << talkgadget_url;

    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("CRD_dns_blackhole_checker",
                                            R"(
          semantics {
            sender: "CRD Dns Blackhole Checker"
            description: "Checks if this machine is allowed to access the "
              "chromoting host talkgadget and block startup if the talkgadget "
              "is not reachable. This permits admins to DNS block the "
              "talkgadget to disable hosts from sharing out from their "
              "network."
            trigger:
              "Manually triggered running <out>/remoting_me2me_host ."
            data: "No user data."
            destination: OTHER
            destination_other:
              "The Chrome Remote Desktop client/host the user is connecting to."
          }
          policy {
            cookies_allowed: NO
            setting:
              "This request cannot be stopped in settings, but will not be "
              "sent if user does not use Chrome Remote Desktop."
            policy_exception_justification:
              "Not implemented."
          })");

    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = GURL(talkgadget_url);

    url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                   traffic_annotation);
    // TODO(crbug.com/879719): Investigate the use of
    // SimpleURLLoader::DownloadHeadersOnly here.
    url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory_.get(),
        base::BindOnce(&DnsBlackholeChecker::OnURLLoadComplete,
                       base::Unretained(this)));
  } else {
    HOST_LOG << "Pending connection check";
  }
}

}  // namespace remoting
