// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/discovery/mdns/mdns_responder_adapter.h"

namespace openscreen {
namespace osp {

QueryEventHeader::QueryEventHeader() = default;
QueryEventHeader::QueryEventHeader(QueryEventHeader::Type response_type,
                                   UdpSocket* socket)
    : response_type(response_type), socket(socket) {}
QueryEventHeader::QueryEventHeader(QueryEventHeader&&) noexcept = default;
QueryEventHeader::~QueryEventHeader() = default;
QueryEventHeader& QueryEventHeader::operator=(QueryEventHeader&&) noexcept =
    default;

AEvent::AEvent() = default;
AEvent::AEvent(QueryEventHeader header,
               DomainName domain_name,
               IPAddress address)
    : header(std::move(header)),
      domain_name(std::move(domain_name)),
      address(std::move(address)) {}
AEvent::AEvent(AEvent&&) noexcept = default;
AEvent::~AEvent() = default;
AEvent& AEvent::operator=(AEvent&&) noexcept = default;

AaaaEvent::AaaaEvent() = default;
AaaaEvent::AaaaEvent(QueryEventHeader header,
                     DomainName domain_name,
                     IPAddress address)
    : header(std::move(header)),
      domain_name(std::move(domain_name)),
      address(std::move(address)) {}
AaaaEvent::AaaaEvent(AaaaEvent&&) noexcept = default;
AaaaEvent::~AaaaEvent() = default;
AaaaEvent& AaaaEvent::operator=(AaaaEvent&&) noexcept = default;

PtrEvent::PtrEvent() = default;
PtrEvent::PtrEvent(QueryEventHeader header, DomainName service_instance)
    : header(std::move(header)),
      service_instance(std::move(service_instance)) {}
PtrEvent::PtrEvent(PtrEvent&&) noexcept = default;
PtrEvent::~PtrEvent() = default;
PtrEvent& PtrEvent::operator=(PtrEvent&&) noexcept = default;

SrvEvent::SrvEvent() = default;
SrvEvent::SrvEvent(QueryEventHeader header,
                   DomainName service_instance,
                   DomainName domain_name,
                   uint16_t port)
    : header(std::move(header)),
      service_instance(std::move(service_instance)),
      domain_name(std::move(domain_name)),
      port(port) {}
SrvEvent::SrvEvent(SrvEvent&&) noexcept = default;
SrvEvent::~SrvEvent() = default;
SrvEvent& SrvEvent::operator=(SrvEvent&&) noexcept = default;

TxtEvent::TxtEvent() = default;
TxtEvent::TxtEvent(QueryEventHeader header,
                   DomainName service_instance,
                   std::vector<std::string> txt_info)
    : header(std::move(header)),
      service_instance(std::move(service_instance)),
      txt_info(std::move(txt_info)) {}
TxtEvent::TxtEvent(TxtEvent&&) noexcept = default;
TxtEvent::~TxtEvent() = default;
TxtEvent& TxtEvent::operator=(TxtEvent&&) noexcept = default;

MdnsResponderAdapter::MdnsResponderAdapter() = default;
MdnsResponderAdapter::~MdnsResponderAdapter() = default;

}  // namespace osp
}  // namespace openscreen
