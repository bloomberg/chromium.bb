// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/testing/fake_mdns_responder_adapter.h"

#include <algorithm>

#include "platform/base/error.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace osp {

constexpr char kLocalDomain[] = "local";

PtrEvent MakePtrEvent(const std::string& service_instance,
                      const std::string& service_type,
                      const std::string& service_protocol,
                      UdpSocket* socket) {
  const auto labels = std::vector<std::string>{service_instance, service_type,
                                               service_protocol, kLocalDomain};
  ErrorOr<DomainName> full_instance_name =
      DomainName::FromLabels(labels.begin(), labels.end());
  OSP_CHECK(full_instance_name);
  PtrEvent result{QueryEventHeader{QueryEventHeader::Type::kAdded, socket},
                  full_instance_name.value()};
  return result;
}

SrvEvent MakeSrvEvent(const std::string& service_instance,
                      const std::string& service_type,
                      const std::string& service_protocol,
                      const std::string& hostname,
                      uint16_t port,
                      UdpSocket* socket) {
  const auto instance_labels = std::vector<std::string>{
      service_instance, service_type, service_protocol, kLocalDomain};
  ErrorOr<DomainName> full_instance_name =
      DomainName::FromLabels(instance_labels.begin(), instance_labels.end());
  OSP_CHECK(full_instance_name);

  const auto host_labels = std::vector<std::string>{hostname, kLocalDomain};
  ErrorOr<DomainName> domain_name =
      DomainName::FromLabels(host_labels.begin(), host_labels.end());
  OSP_CHECK(domain_name);

  SrvEvent result{QueryEventHeader{QueryEventHeader::Type::kAdded, socket},
                  full_instance_name.value(), domain_name.value(), port};
  return result;
}

TxtEvent MakeTxtEvent(const std::string& service_instance,
                      const std::string& service_type,
                      const std::string& service_protocol,
                      const std::vector<std::string>& txt_lines,
                      UdpSocket* socket) {
  const auto labels = std::vector<std::string>{service_instance, service_type,
                                               service_protocol, kLocalDomain};
  ErrorOr<DomainName> domain_name =
      DomainName::FromLabels(labels.begin(), labels.end());
  OSP_CHECK(domain_name);
  TxtEvent result{QueryEventHeader{QueryEventHeader::Type::kAdded, socket},
                  domain_name.value(), txt_lines};
  return result;
}

AEvent MakeAEvent(const std::string& hostname,
                  IPAddress address,
                  UdpSocket* socket) {
  const auto labels = std::vector<std::string>{hostname, kLocalDomain};
  ErrorOr<DomainName> domain_name =
      DomainName::FromLabels(labels.begin(), labels.end());
  OSP_CHECK(domain_name);
  AEvent result{QueryEventHeader{QueryEventHeader::Type::kAdded, socket},
                domain_name.value(), address};
  return result;
}

AaaaEvent MakeAaaaEvent(const std::string& hostname,
                        IPAddress address,
                        UdpSocket* socket) {
  const auto labels = std::vector<std::string>{hostname, kLocalDomain};
  ErrorOr<DomainName> domain_name =
      DomainName::FromLabels(labels.begin(), labels.end());
  OSP_CHECK(domain_name);
  AaaaEvent result{QueryEventHeader{QueryEventHeader::Type::kAdded, socket},
                   domain_name.value(), address};
  return result;
}

void AddEventsForNewService(FakeMdnsResponderAdapter* mdns_responder,
                            const std::string& service_instance,
                            const std::string& service_name,
                            const std::string& service_protocol,
                            const std::string& hostname,
                            uint16_t port,
                            const std::vector<std::string>& txt_lines,
                            const IPAddress& address,
                            UdpSocket* socket) {
  mdns_responder->AddPtrEvent(
      MakePtrEvent(service_instance, service_name, service_protocol, socket));
  mdns_responder->AddSrvEvent(MakeSrvEvent(service_instance, service_name,
                                           service_protocol, hostname, port,
                                           socket));
  mdns_responder->AddTxtEvent(MakeTxtEvent(
      service_instance, service_name, service_protocol, txt_lines, socket));
  mdns_responder->AddAEvent(MakeAEvent(hostname, address, socket));
}

FakeMdnsResponderAdapter::~FakeMdnsResponderAdapter() {
  if (observer_) {
    observer_->OnDestroyed();
  }
}

void FakeMdnsResponderAdapter::AddPtrEvent(PtrEvent&& ptr_event) {
  if (running_)
    ptr_events_.push_back(std::move(ptr_event));
}

void FakeMdnsResponderAdapter::AddSrvEvent(SrvEvent&& srv_event) {
  if (running_)
    srv_events_.push_back(std::move(srv_event));
}

void FakeMdnsResponderAdapter::AddTxtEvent(TxtEvent&& txt_event) {
  if (running_)
    txt_events_.push_back(std::move(txt_event));
}

void FakeMdnsResponderAdapter::AddAEvent(AEvent&& a_event) {
  if (running_)
    a_events_.push_back(std::move(a_event));
}

void FakeMdnsResponderAdapter::AddAaaaEvent(AaaaEvent&& aaaa_event) {
  if (running_)
    aaaa_events_.push_back(std::move(aaaa_event));
}

bool FakeMdnsResponderAdapter::ptr_queries_empty() const {
  for (const auto& queries : queries_) {
    if (!queries.second.ptr_queries.empty())
      return false;
  }
  return true;
}

bool FakeMdnsResponderAdapter::srv_queries_empty() const {
  for (const auto& queries : queries_) {
    if (!queries.second.srv_queries.empty())
      return false;
  }
  return true;
}

bool FakeMdnsResponderAdapter::txt_queries_empty() const {
  for (const auto& queries : queries_) {
    if (!queries.second.txt_queries.empty())
      return false;
  }
  return true;
}

bool FakeMdnsResponderAdapter::a_queries_empty() const {
  for (const auto& queries : queries_) {
    if (!queries.second.a_queries.empty())
      return false;
  }
  return true;
}

bool FakeMdnsResponderAdapter::aaaa_queries_empty() const {
  for (const auto& queries : queries_) {
    if (!queries.second.aaaa_queries.empty())
      return false;
  }
  return true;
}

Error FakeMdnsResponderAdapter::Init() {
  OSP_CHECK(!running_);
  running_ = true;
  return Error::None();
}

void FakeMdnsResponderAdapter::Close() {
  queries_.clear();
  ptr_events_.clear();
  srv_events_.clear();
  txt_events_.clear();
  a_events_.clear();
  aaaa_events_.clear();
  registered_interfaces_.clear();
  registered_services_.clear();
  running_ = false;
}

Error FakeMdnsResponderAdapter::SetHostLabel(const std::string& host_label) {
  return Error::Code::kNotImplemented;
}

Error FakeMdnsResponderAdapter::RegisterInterface(
    const InterfaceInfo& interface_info,
    const IPSubnet& interface_address,
    UdpSocket* socket) {
  if (!running_)
    return Error::Code::kOperationInvalid;

  if (std::find_if(registered_interfaces_.begin(), registered_interfaces_.end(),
                   [&socket](const RegisteredInterface& interface) {
                     return interface.socket == socket;
                   }) != registered_interfaces_.end()) {
    return Error::Code::kItemNotFound;
  }
  registered_interfaces_.push_back({interface_info, interface_address, socket});
  return Error::None();
}

Error FakeMdnsResponderAdapter::DeregisterInterface(UdpSocket* socket) {
  auto it =
      std::find_if(registered_interfaces_.begin(), registered_interfaces_.end(),
                   [&socket](const RegisteredInterface& interface) {
                     return interface.socket == socket;
                   });
  if (it == registered_interfaces_.end())
    return Error::Code::kItemNotFound;

  registered_interfaces_.erase(it);
  return Error::None();
}

void FakeMdnsResponderAdapter::OnRead(UdpSocket* socket,
                                      ErrorOr<UdpPacket> packet) {
  OSP_NOTREACHED() << "Tests should not drive this class with packets";
}

void FakeMdnsResponderAdapter::OnSendError(UdpSocket* socket, Error error) {
  OSP_NOTREACHED() << "Tests should not drive this class with packets";
}

void FakeMdnsResponderAdapter::OnError(UdpSocket* socket, Error error) {
  OSP_NOTREACHED() << "Tests should not drive this class with packets";
}

Clock::duration FakeMdnsResponderAdapter::RunTasks() {
  return std::chrono::seconds(1);
}

std::vector<PtrEvent> FakeMdnsResponderAdapter::TakePtrResponses() {
  std::vector<PtrEvent> result;
  for (auto& queries : queries_) {
    const auto query_it = std::stable_partition(
        ptr_events_.begin(), ptr_events_.end(),
        [&queries](const PtrEvent& ptr_event) {
          const auto instance_labels = ptr_event.service_instance.GetLabels();
          for (const auto& query : queries.second.ptr_queries) {
            const auto query_labels = query.GetLabels();
            // TODO(btolsch): Just use qname if it's added to PtrEvent.
            if (ptr_event.header.socket == queries.first &&
                std::equal(instance_labels.begin() + 1, instance_labels.end(),
                           query_labels.begin())) {
              return false;
            }
          }
          return true;
        });
    for (auto it = query_it; it != ptr_events_.end(); ++it) {
      result.push_back(std::move(*it));
    }
    ptr_events_.erase(query_it, ptr_events_.end());
  }
  OSP_LOG << "taking " << result.size() << " ptr response(s)";
  return result;
}

std::vector<SrvEvent> FakeMdnsResponderAdapter::TakeSrvResponses() {
  std::vector<SrvEvent> result;
  for (auto& queries : queries_) {
    const auto query_it = std::stable_partition(
        srv_events_.begin(), srv_events_.end(),
        [&queries](const SrvEvent& srv_event) {
          for (const auto& query : queries.second.srv_queries) {
            if (srv_event.header.socket == queries.first &&
                srv_event.service_instance == query)
              return false;
          }
          return true;
        });
    for (auto it = query_it; it != srv_events_.end(); ++it) {
      result.push_back(std::move(*it));
    }
    srv_events_.erase(query_it, srv_events_.end());
  }
  OSP_LOG << "taking " << result.size() << " srv response(s)";
  return result;
}

std::vector<TxtEvent> FakeMdnsResponderAdapter::TakeTxtResponses() {
  std::vector<TxtEvent> result;
  for (auto& queries : queries_) {
    const auto query_it = std::stable_partition(
        txt_events_.begin(), txt_events_.end(),
        [&queries](const TxtEvent& txt_event) {
          for (const auto& query : queries.second.txt_queries) {
            if (txt_event.header.socket == queries.first &&
                txt_event.service_instance == query) {
              return false;
            }
          }
          return true;
        });
    for (auto it = query_it; it != txt_events_.end(); ++it) {
      result.push_back(std::move(*it));
    }
    txt_events_.erase(query_it, txt_events_.end());
  }
  OSP_LOG << "taking " << result.size() << " txt response(s)";
  return result;
}

std::vector<AEvent> FakeMdnsResponderAdapter::TakeAResponses() {
  std::vector<AEvent> result;
  for (auto& queries : queries_) {
    const auto query_it = std::stable_partition(
        a_events_.begin(), a_events_.end(), [&queries](const AEvent& a_event) {
          for (const auto& query : queries.second.a_queries) {
            if (a_event.header.socket == queries.first &&
                a_event.domain_name == query) {
              return false;
            }
          }
          return true;
        });
    for (auto it = query_it; it != a_events_.end(); ++it) {
      result.push_back(std::move(*it));
    }
    a_events_.erase(query_it, a_events_.end());
  }
  OSP_LOG << "taking " << result.size() << " a response(s)";
  return result;
}

std::vector<AaaaEvent> FakeMdnsResponderAdapter::TakeAaaaResponses() {
  std::vector<AaaaEvent> result;
  for (auto& queries : queries_) {
    const auto query_it = std::stable_partition(
        aaaa_events_.begin(), aaaa_events_.end(),
        [&queries](const AaaaEvent& aaaa_event) {
          for (const auto& query : queries.second.aaaa_queries) {
            if (aaaa_event.header.socket == queries.first &&
                aaaa_event.domain_name == query) {
              return false;
            }
          }
          return true;
        });
    for (auto it = query_it; it != aaaa_events_.end(); ++it) {
      result.push_back(std::move(*it));
    }
    aaaa_events_.erase(query_it, aaaa_events_.end());
  }
  OSP_LOG << "taking " << result.size() << " a response(s)";
  return result;
}

MdnsResponderErrorCode FakeMdnsResponderAdapter::StartPtrQuery(
    UdpSocket* socket,
    const DomainName& service_type) {
  if (!running_)
    return MdnsResponderErrorCode::kUnknownError;

  auto canonical_service_type = service_type;
  if (!canonical_service_type.EndsWithLocalDomain())
    OSP_CHECK(canonical_service_type.Append(DomainName::GetLocalDomain()).ok());

  auto maybe_inserted =
      queries_[socket].ptr_queries.insert(canonical_service_type);
  if (maybe_inserted.second) {
    return MdnsResponderErrorCode::kNoError;
  } else {
    return MdnsResponderErrorCode::kUnknownError;
  }
}

MdnsResponderErrorCode FakeMdnsResponderAdapter::StartSrvQuery(
    UdpSocket* socket,
    const DomainName& service_instance) {
  if (!running_)
    return MdnsResponderErrorCode::kUnknownError;

  auto maybe_inserted = queries_[socket].srv_queries.insert(service_instance);
  if (maybe_inserted.second) {
    return MdnsResponderErrorCode::kNoError;
  } else {
    return MdnsResponderErrorCode::kUnknownError;
  }
}

MdnsResponderErrorCode FakeMdnsResponderAdapter::StartTxtQuery(
    UdpSocket* socket,
    const DomainName& service_instance) {
  if (!running_)
    return MdnsResponderErrorCode::kUnknownError;

  auto maybe_inserted = queries_[socket].txt_queries.insert(service_instance);
  if (maybe_inserted.second) {
    return MdnsResponderErrorCode::kNoError;
  } else {
    return MdnsResponderErrorCode::kUnknownError;
  }
}

MdnsResponderErrorCode FakeMdnsResponderAdapter::StartAQuery(
    UdpSocket* socket,
    const DomainName& domain_name) {
  if (!running_)
    return MdnsResponderErrorCode::kUnknownError;

  auto maybe_inserted = queries_[socket].a_queries.insert(domain_name);
  if (maybe_inserted.second) {
    return MdnsResponderErrorCode::kNoError;
  } else {
    return MdnsResponderErrorCode::kUnknownError;
  }
}

MdnsResponderErrorCode FakeMdnsResponderAdapter::StartAaaaQuery(
    UdpSocket* socket,
    const DomainName& domain_name) {
  if (!running_)
    return MdnsResponderErrorCode::kUnknownError;

  auto maybe_inserted = queries_[socket].aaaa_queries.insert(domain_name);
  if (maybe_inserted.second) {
    return MdnsResponderErrorCode::kNoError;
  } else {
    return MdnsResponderErrorCode::kUnknownError;
  }
}

MdnsResponderErrorCode FakeMdnsResponderAdapter::StopPtrQuery(
    UdpSocket* socket,
    const DomainName& service_type) {
  auto interface_entry = queries_.find(socket);
  if (interface_entry == queries_.end())
    return MdnsResponderErrorCode::kUnknownError;
  auto& ptr_queries = interface_entry->second.ptr_queries;
  auto canonical_service_type = service_type;
  if (!canonical_service_type.EndsWithLocalDomain())
    OSP_CHECK(canonical_service_type.Append(DomainName::GetLocalDomain()).ok());

  auto it = ptr_queries.find(canonical_service_type);
  if (it == ptr_queries.end())
    return MdnsResponderErrorCode::kUnknownError;

  ptr_queries.erase(it);
  return MdnsResponderErrorCode::kNoError;
}

MdnsResponderErrorCode FakeMdnsResponderAdapter::StopSrvQuery(
    UdpSocket* socket,
    const DomainName& service_instance) {
  auto interface_entry = queries_.find(socket);
  if (interface_entry == queries_.end())
    return MdnsResponderErrorCode::kUnknownError;
  auto& srv_queries = interface_entry->second.srv_queries;
  auto it = srv_queries.find(service_instance);
  if (it == srv_queries.end())
    return MdnsResponderErrorCode::kUnknownError;

  srv_queries.erase(it);
  return MdnsResponderErrorCode::kNoError;
}

MdnsResponderErrorCode FakeMdnsResponderAdapter::StopTxtQuery(
    UdpSocket* socket,
    const DomainName& service_instance) {
  auto interface_entry = queries_.find(socket);
  if (interface_entry == queries_.end())
    return MdnsResponderErrorCode::kUnknownError;
  auto& txt_queries = interface_entry->second.txt_queries;
  auto it = txt_queries.find(service_instance);
  if (it == txt_queries.end())
    return MdnsResponderErrorCode::kUnknownError;

  txt_queries.erase(it);
  return MdnsResponderErrorCode::kNoError;
}

MdnsResponderErrorCode FakeMdnsResponderAdapter::StopAQuery(
    UdpSocket* socket,
    const DomainName& domain_name) {
  auto interface_entry = queries_.find(socket);
  if (interface_entry == queries_.end())
    return MdnsResponderErrorCode::kUnknownError;
  auto& a_queries = interface_entry->second.a_queries;
  auto it = a_queries.find(domain_name);
  if (it == a_queries.end())
    return MdnsResponderErrorCode::kUnknownError;

  a_queries.erase(it);
  return MdnsResponderErrorCode::kNoError;
}

MdnsResponderErrorCode FakeMdnsResponderAdapter::StopAaaaQuery(
    UdpSocket* socket,
    const DomainName& domain_name) {
  auto interface_entry = queries_.find(socket);
  if (interface_entry == queries_.end())
    return MdnsResponderErrorCode::kUnknownError;
  auto& aaaa_queries = interface_entry->second.aaaa_queries;
  auto it = aaaa_queries.find(domain_name);
  if (it == aaaa_queries.end())
    return MdnsResponderErrorCode::kUnknownError;

  aaaa_queries.erase(it);
  return MdnsResponderErrorCode::kNoError;
}

MdnsResponderErrorCode FakeMdnsResponderAdapter::RegisterService(
    const std::string& service_instance,
    const std::string& service_name,
    const std::string& service_protocol,
    const DomainName& target_host,
    uint16_t target_port,
    const std::map<std::string, std::string>& txt_data) {
  if (!running_)
    return MdnsResponderErrorCode::kUnknownError;

  if (std::find_if(registered_services_.begin(), registered_services_.end(),
                   [&service_instance, &service_name,
                    &service_protocol](const RegisteredService& service) {
                     return service.service_instance == service_instance &&
                            service.service_name == service_name &&
                            service.service_protocol == service_protocol;
                   }) != registered_services_.end()) {
    return MdnsResponderErrorCode::kUnknownError;
  }
  registered_services_.push_back({service_instance, service_name,
                                  service_protocol, target_host, target_port,
                                  txt_data});
  return MdnsResponderErrorCode::kNoError;
}

MdnsResponderErrorCode FakeMdnsResponderAdapter::DeregisterService(
    const std::string& service_instance,
    const std::string& service_name,
    const std::string& service_protocol) {
  if (!running_)
    return MdnsResponderErrorCode::kUnknownError;

  auto it =
      std::find_if(registered_services_.begin(), registered_services_.end(),
                   [&service_instance, &service_name,
                    &service_protocol](const RegisteredService& service) {
                     return service.service_instance == service_instance &&
                            service.service_name == service_name &&
                            service.service_protocol == service_protocol;
                   });
  if (it == registered_services_.end())
    return MdnsResponderErrorCode::kUnknownError;

  registered_services_.erase(it);
  return MdnsResponderErrorCode::kNoError;
}

MdnsResponderErrorCode FakeMdnsResponderAdapter::UpdateTxtData(
    const std::string& service_instance,
    const std::string& service_name,
    const std::string& service_protocol,
    const std::map<std::string, std::string>& txt_data) {
  if (!running_)
    return MdnsResponderErrorCode::kUnknownError;

  auto it =
      std::find_if(registered_services_.begin(), registered_services_.end(),
                   [&service_instance, &service_name,
                    &service_protocol](const RegisteredService& service) {
                     return service.service_instance == service_instance &&
                            service.service_name == service_name &&
                            service.service_protocol == service_protocol;
                   });
  if (it == registered_services_.end())
    return MdnsResponderErrorCode::kUnknownError;

  it->txt_data = txt_data;
  return MdnsResponderErrorCode::kNoError;
}

}  // namespace osp
}  // namespace openscreen
