// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/querier_impl.h"

#include <memory>

#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/types/optional.h"
#include "discovery/mdns/mdns_records.h"
#include "discovery/mdns/testing/mdns_test_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/logging.h"

namespace openscreen {
namespace discovery {
namespace {

class MockCallback : public DnsSdQuerier::Callback {
 public:
  MOCK_METHOD1(OnInstanceCreated, void(const DnsSdInstanceRecord&));
  MOCK_METHOD1(OnInstanceUpdated, void(const DnsSdInstanceRecord&));
  MOCK_METHOD1(OnInstanceDeleted, void(const DnsSdInstanceRecord&));
};

class MockMdnsService : public MdnsService {
 public:
  MOCK_METHOD4(
      StartQuery,
      void(const DomainName&, DnsType, DnsClass, MdnsRecordChangedCallback*));

  MOCK_METHOD4(
      StopQuery,
      void(const DomainName&, DnsType, DnsClass, MdnsRecordChangedCallback*));

  void RegisterRecord(const MdnsRecord& record) override { FAIL(); }

  void DeregisterRecord(const MdnsRecord& record) override { FAIL(); }
};

SrvRecordRdata CreateSrvRecord() {
  DomainName kDomain({"label"});
  constexpr uint16_t kPort{8080};
  return SrvRecordRdata(0, 0, kPort, std::move(kDomain));
}

ARecordRdata CreateARecord() {
  return ARecordRdata(IPAddress{192, 168, 0, 0});
}

AAAARecordRdata CreateAAAARecord() {
  return AAAARecordRdata(
      IPAddress{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
}

MdnsRecord CreatePtrRecord(const std::string& instance,
                           const std::string& service,
                           const std::string& domain) {
  std::vector<std::string> ptr_labels;
  std::vector<std::string> instance_labels{instance};

  std::vector<std::string> service_labels = absl::StrSplit(service, '.');
  ptr_labels.insert(ptr_labels.end(), service_labels.begin(),
                    service_labels.end());
  instance_labels.insert(instance_labels.end(), service_labels.begin(),
                         service_labels.end());

  std::vector<std::string> domain_labels = absl::StrSplit(domain, '.');
  ptr_labels.insert(ptr_labels.end(), domain_labels.begin(),
                    domain_labels.end());
  instance_labels.insert(instance_labels.end(), domain_labels.begin(),
                         domain_labels.end());

  DomainName ptr_domain(ptr_labels);
  DomainName inner_domain(instance_labels);

  PtrRecordRdata data(std::move(inner_domain));

  // TTL specified by RFC 6762 section 10.
  constexpr std::chrono::seconds kTtl(120);
  return MdnsRecord(std::move(ptr_domain), DnsType::kPTR, DnsClass::kIN,
                    RecordType::kShared, kTtl, std::move(data));
}

}  // namespace

using testing::_;
using testing::ByMove;
using testing::Return;
using testing::StrictMock;

class DnsDataAccessor {
 public:
  DnsDataAccessor(DnsData* data) : data_(data) {}

  void set_srv(absl::optional<SrvRecordRdata> record) { data_->srv_ = record; }
  void set_txt(absl::optional<TxtRecordRdata> record) { data_->txt_ = record; }
  void set_a(absl::optional<ARecordRdata> record) { data_->a_ = record; }
  void set_aaaa(absl::optional<AAAARecordRdata> record) {
    data_->aaaa_ = record;
  }

  absl::optional<SrvRecordRdata>* srv() { return &data_->srv_; }
  absl::optional<TxtRecordRdata>* txt() { return &data_->txt_; }
  absl::optional<ARecordRdata>* a() { return &data_->a_; }
  absl::optional<AAAARecordRdata>* aaaa() { return &data_->aaaa_; }

  bool CanCreateInstance() { return data_->CreateRecord().is_value(); }

 private:
  DnsData* data_;
};

class QuerierImplTesting : public QuerierImpl {
 public:
  QuerierImplTesting() : QuerierImpl(&mock_service_) {}

  MockMdnsService* service() { return &mock_service_; }

  DnsDataAccessor CreateDnsData(const std::string& instance,
                                const std::string& service,
                                const std::string& domain) {
    InstanceKey key{instance, service, domain};
    auto it = received_records_.emplace(key, DnsData(key)).first;
    return DnsDataAccessor(&it->second);
  }

  absl::optional<DnsDataAccessor> GetDnsData(const std::string& instance,
                                             const std::string& service,
                                             const std::string& domain) {
    InstanceKey key{instance, service, domain};
    auto it = received_records_.find(key);
    if (it == received_records_.end()) {
      return absl::nullopt;
    }
    return DnsDataAccessor(&it->second);
  }

 private:
  StrictMock<MockMdnsService> mock_service_;
};

class DnsSdQuerierImplTest : public testing::Test {
 public:
  DnsSdQuerierImplTest() {
    EXPECT_FALSE(querier.IsQueryRunning(service));

    EXPECT_CALL(*querier.service(),
                StartQuery(_, DnsType::kPTR, DnsClass::kANY, _))
        .Times(1);
    querier.StartQuery(service, &callback);
    EXPECT_TRUE(querier.IsQueryRunning(service));

    EXPECT_CALL(*querier.service(),
                StartQuery(_, DnsType::kPTR, DnsClass::kANY, _))
        .Times(0);
    EXPECT_TRUE(querier.IsQueryRunning(service));
  }

 protected:
  std::string instance = "instance";
  std::string service = "_service._udp";
  std::string domain = "local";
  StrictMock<MockCallback> callback;
  QuerierImplTesting querier;
};

TEST_F(DnsSdQuerierImplTest, TestStartStopQueryCallsMdnsQueries) {
  StrictMock<MockCallback> callback2;

  querier.StartQuery(service, &callback2);
  querier.StopQuery(service, &callback);
  EXPECT_TRUE(querier.IsQueryRunning(service));

  EXPECT_CALL(*querier.service(),
              StopQuery(_, DnsType::kPTR, DnsClass::kANY, _))
      .Times(1);
  querier.StopQuery(service, &callback2);
  EXPECT_FALSE(querier.IsQueryRunning(service));
}

TEST_F(DnsSdQuerierImplTest, TestStartDuplicateQueryFiresCallbacksWhenAble) {
  StrictMock<MockCallback> callback2;

  DnsDataAccessor dns_data = querier.CreateDnsData(instance, service, domain);
  dns_data.set_srv(CreateSrvRecord());
  dns_data.set_txt(MakeTxtRecord({}));
  dns_data.set_a(CreateARecord());
  dns_data.set_aaaa(CreateAAAARecord());

  EXPECT_CALL(callback2, OnInstanceCreated(_)).Times(1);
  querier.StartQuery(service, &callback2);
}

TEST_F(DnsSdQuerierImplTest, TestStopQueryClearsRecords) {
  querier.CreateDnsData(instance, service, domain);

  EXPECT_CALL(*querier.service(),
              StopQuery(_, DnsType::kPTR, DnsClass::kANY, _))
      .Times(1);
  querier.StopQuery(service, &callback);
  EXPECT_FALSE(querier.GetDnsData(instance, service, domain).has_value());
}

TEST_F(DnsSdQuerierImplTest, TestStopNonexistantQueryHasNoEffect) {
  StrictMock<MockCallback> callback2;
  querier.CreateDnsData(instance, service, domain);

  querier.StopQuery(service, &callback2);
  EXPECT_TRUE(querier.GetDnsData(instance, service, domain).has_value());
}

TEST_F(DnsSdQuerierImplTest, TestCreateDeletePtrRecord) {
  const auto ptr = CreatePtrRecord(instance, service, domain);
  const auto ptr2 = CreatePtrRecord(instance, service, domain);

  EXPECT_CALL(*querier.service(),
              StartQuery(_, DnsType::kANY, DnsClass::kANY, _))
      .Times(1);
  querier.OnRecordChanged(ptr, RecordChangedEvent::kCreated);

  EXPECT_CALL(*querier.service(),
              StopQuery(_, DnsType::kANY, DnsClass::kANY, _))
      .Times(1);
  querier.OnRecordChanged(ptr2, RecordChangedEvent::kExpired);
}

TEST_F(DnsSdQuerierImplTest, CallbackCalledWhenPtrDeleted) {
  auto ptr = CreatePtrRecord(instance, service, domain);
  DnsDataAccessor dns_data = querier.CreateDnsData(instance, service, domain);
  dns_data.set_srv(CreateSrvRecord());
  dns_data.set_txt(MakeTxtRecord({}));
  dns_data.set_a(CreateARecord());
  dns_data.set_aaaa(CreateAAAARecord());
  ASSERT_TRUE(dns_data.CanCreateInstance());

  EXPECT_CALL(*querier.service(),
              StartQuery(_, DnsType::kANY, DnsClass::kANY, _))
      .Times(1);
  querier.OnRecordChanged(ptr, RecordChangedEvent::kCreated);

  EXPECT_CALL(callback, OnInstanceDeleted(_)).Times(1);
  EXPECT_CALL(*querier.service(),
              StopQuery(_, DnsType::kANY, DnsClass::kANY, _))
      .Times(1);
  querier.OnRecordChanged(ptr, RecordChangedEvent::kExpired);
  EXPECT_FALSE(querier.GetDnsData(instance, service, domain).has_value());
}

TEST_F(DnsSdQuerierImplTest, NeitherNewNorOldValidRecords) {
  DnsDataAccessor dns_data = querier.CreateDnsData(instance, service, domain);
  dns_data.set_a(CreateARecord());
  dns_data.set_aaaa(CreateAAAARecord());

  auto srv_rdata = CreateSrvRecord();
  DomainName kDomainName{"instance", "_service", "_udp", "local"};
  MdnsRecord srv_record(std::move(kDomainName), DnsType::kSRV, DnsClass::kIN,
                        RecordType::kUnique, std::chrono::seconds(0),
                        srv_rdata);
  querier.OnRecordChanged(srv_record, RecordChangedEvent::kCreated);
}

TEST_F(DnsSdQuerierImplTest, BothNewAndOldValidRecords) {
  DnsDataAccessor dns_data = querier.CreateDnsData(instance, service, domain);
  dns_data.set_srv(CreateSrvRecord());
  dns_data.set_txt(MakeTxtRecord({}));
  dns_data.set_aaaa(CreateAAAARecord());

  auto a_rdata = CreateARecord();
  const DomainName kDomainName{"instance", "_service", "_udp", "local"};
  MdnsRecord a_record(kDomainName, DnsType::kA, DnsClass::kIN,
                      RecordType::kUnique, std::chrono::seconds(0), a_rdata);

  EXPECT_CALL(callback, OnInstanceUpdated(_)).Times(1);
  querier.OnRecordChanged(a_record, RecordChangedEvent::kCreated);

  EXPECT_CALL(callback, OnInstanceUpdated(_)).Times(1);
  querier.OnRecordChanged(a_record, RecordChangedEvent::kUpdated);

  auto aaaa_rdata = CreateAAAARecord();
  MdnsRecord aaaa_record(kDomainName, DnsType::kAAAA, DnsClass::kIN,
                         RecordType::kUnique, std::chrono::seconds(0),
                         aaaa_rdata);

  EXPECT_CALL(callback, OnInstanceUpdated(_)).Times(1);
  querier.OnRecordChanged(aaaa_record, RecordChangedEvent::kUpdated);

  EXPECT_CALL(callback, OnInstanceUpdated(_)).Times(1);
  querier.OnRecordChanged(a_record, RecordChangedEvent::kExpired);
}

TEST_F(DnsSdQuerierImplTest, OnlyNewRecordValid) {
  DnsDataAccessor dns_data = querier.CreateDnsData(instance, service, domain);
  dns_data.set_srv(CreateSrvRecord());
  dns_data.set_txt(MakeTxtRecord({}));

  auto a_rdata = CreateARecord();
  DomainName kDomainName{"instance", "_service", "_udp", "local"};
  MdnsRecord a_record(std::move(kDomainName), DnsType::kA, DnsClass::kIN,
                      RecordType::kUnique, std::chrono::seconds(0), a_rdata);

  EXPECT_CALL(callback, OnInstanceCreated(_)).Times(1);
  querier.OnRecordChanged(a_record, RecordChangedEvent::kCreated);
}

TEST_F(DnsSdQuerierImplTest, OnlyOldRecordValid) {
  DnsDataAccessor dns_data = querier.CreateDnsData(instance, service, domain);
  dns_data.set_srv(CreateSrvRecord());
  dns_data.set_txt(MakeTxtRecord({}));
  dns_data.set_a(CreateARecord());

  auto a_rdata = CreateARecord();
  DomainName kDomainName{"instance", "_service", "_udp", "local"};
  MdnsRecord a_record(std::move(kDomainName), DnsType::kA, DnsClass::kIN,
                      RecordType::kUnique, std::chrono::seconds(0), a_rdata);

  EXPECT_CALL(callback, OnInstanceDeleted(_)).Times(1);
  querier.OnRecordChanged(a_record, RecordChangedEvent::kExpired);
}

}  // namespace discovery
}  // namespace openscreen
