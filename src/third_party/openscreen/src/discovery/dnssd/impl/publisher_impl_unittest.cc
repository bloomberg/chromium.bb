// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/publisher_impl.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace discovery {

using testing::_;
using testing::Return;

class MockMdnsService : public MdnsService {
 public:
  void StartQuery(const DomainName& name,
                  DnsType dns_type,
                  DnsClass dns_class,
                  MdnsRecordChangedCallback* callback) override {
    OSP_UNIMPLEMENTED();
  }

  void StopQuery(const DomainName& name,
                 DnsType dns_type,
                 DnsClass dns_class,
                 MdnsRecordChangedCallback* callback) override {
    OSP_UNIMPLEMENTED();
  }

  MOCK_METHOD1(RegisterRecord, void(const MdnsRecord& record));
  MOCK_METHOD1(DeregisterRecord, void(const MdnsRecord& record));
};

class PublisherTesting : public PublisherImpl {
 public:
  PublisherTesting() : PublisherImpl(&mock_service_) {}

  MockMdnsService& mdns_service() { return mock_service_; }

 private:
  MockMdnsService mock_service_;
};

TEST(DnsSdPublisherImplTests, TestRegisterAndDeregister) {
  PublisherTesting publisher;
  IPAddress address = IPAddress(std::array<uint8_t, 4>{{192, 168, 0, 0}});
  DnsSdInstanceRecord record("instance", "_service._udp", "domain",
                             {address, 80}, {});

  int seen = 0;
  EXPECT_CALL(publisher.mdns_service(), RegisterRecord(_))
      .Times(4)
      .WillRepeatedly([&seen, &address](const MdnsRecord& record) mutable {
        if (record.dns_type() == DnsType::kA) {
          const ARecordRdata& data = absl::get<ARecordRdata>(record.rdata());
          if (data.ipv4_address() == address) {
            seen++;
          }
        } else if (record.dns_type() == DnsType::kSRV) {
          const SrvRecordRdata& data =
              absl::get<SrvRecordRdata>(record.rdata());
          if (data.port() == 80) {
            seen++;
          }
        };
      });
  publisher.Register(record);
  EXPECT_EQ(seen, 2);

  seen = 0;
  EXPECT_CALL(publisher.mdns_service(), DeregisterRecord(_))
      .Times(4)
      .WillRepeatedly([&seen, &address](const MdnsRecord& record) mutable {
        if (record.dns_type() == DnsType::kA) {
          const ARecordRdata& data = absl::get<ARecordRdata>(record.rdata());
          if (data.ipv4_address() == address) {
            seen++;
          }
        } else if (record.dns_type() == DnsType::kSRV) {
          const SrvRecordRdata& data =
              absl::get<SrvRecordRdata>(record.rdata());
          if (data.port() == 80) {
            seen++;
          }
        };
      });
  publisher.DeregisterAll("_service._udp");
  EXPECT_EQ(seen, 2);
}

}  // namespace discovery
}  // namespace openscreen
