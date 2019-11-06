// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_reader.h"

#include "cast/common/mdns/mdns_constants.h"
#include "platform/api/logging.h"

namespace cast {
namespace mdns {

bool MdnsReader::Read(absl::string_view* out) {
  Cursor cursor(this);
  uint8_t string_length;
  if (!Read(&string_length)) {
    return false;
  }
  const char* string_begin = reinterpret_cast<const char*>(current());
  if (!Skip(string_length)) {
    return false;
  }
  *out = absl::string_view(string_begin, string_length);
  cursor.Commit();
  return true;
}

// RFC 1035: https://www.ietf.org/rfc/rfc1035.txt
// See section 4.1.4. Message compression
bool MdnsReader::Read(DomainName* out) {
  OSP_DCHECK(out);
  const uint8_t* position = current();
  // The number of bytes consumed reading from the starting position to either
  // the first label pointer or the final termination byte, including the
  // pointer or the termination byte. This is equal to the actual wire size of
  // the DomainName accounting for compression.
  size_t bytes_consumed = 0;
  // The number of bytes that was processed when reading the DomainName,
  // including all label pointers and direct labels. It is used to detect
  // circular compression. The number of processed bytes cannot be possibly
  // greater than the length of the buffer.
  size_t bytes_processed = 0;
  size_t domain_name_length = 0;
  std::vector<absl::string_view> labels;
  // If we are pointing before the beginning or past the end of the buffer, we
  // hit a malformed pointer. If we have processed more bytes than there are in
  // the buffer, we are in a circular compression loop.
  while (position >= begin() && position < end() &&
         bytes_processed <= length()) {
    const uint8_t label_type = openscreen::ReadBigEndian<uint8_t>(position);
    if (IsTerminationLabel(label_type)) {
      *out = DomainName(labels);
      if (!bytes_consumed) {
        bytes_consumed = position + sizeof(uint8_t) - current();
      }
      return Skip(bytes_consumed);
    } else if (IsPointerLabel(label_type)) {
      if (position + sizeof(uint16_t) > end()) {
        return false;
      }
      const uint16_t label_offset =
          GetPointerLabelOffset(openscreen::ReadBigEndian<uint16_t>(position));
      if (!bytes_consumed) {
        bytes_consumed = position + sizeof(uint16_t) - current();
      }
      bytes_processed += sizeof(uint16_t);
      position = begin() + label_offset;
    } else if (IsDirectLabel(label_type)) {
      const uint8_t label_length = GetDirectLabelLength(label_type);
      OSP_DCHECK_GT(label_length, 0);
      bytes_processed += sizeof(uint8_t);
      position += sizeof(uint8_t);
      if (position + label_length >= end()) {
        return false;
      }
      const absl::string_view label(reinterpret_cast<const char*>(position),
                                    label_length);
      domain_name_length += label_length + 1;  // including the length byte
      if (!IsValidDomainLabel(label) ||
          domain_name_length > kMaxDomainNameLength) {
        return false;
      }
      labels.push_back(label);
      bytes_processed += label_length;
      position += label_length;
    } else {
      return false;
    }
  }
  return false;
}

bool MdnsReader::Read(RawRecordRdata* out) {
  OSP_DCHECK(out);
  Cursor cursor(this);
  uint16_t record_length;
  if (Read(&record_length)) {
    std::vector<uint8_t> buffer(record_length);
    if (Read(buffer.size(), buffer.data())) {
      *out = RawRecordRdata(std::move(buffer));
      cursor.Commit();
      return true;
    }
  }
  return false;
}

bool MdnsReader::Read(SrvRecordRdata* out) {
  OSP_DCHECK(out);
  Cursor cursor(this);
  uint16_t record_length;
  uint16_t priority;
  uint16_t weight;
  uint16_t port;
  DomainName target;
  if (Read(&record_length) && Read(&priority) && Read(&weight) && Read(&port) &&
      Read(&target) &&
      (cursor.delta() == sizeof(record_length) + record_length)) {
    *out = SrvRecordRdata(priority, weight, port, std::move(target));
    cursor.Commit();
    return true;
  }
  return false;
}

bool MdnsReader::Read(ARecordRdata* out) {
  OSP_DCHECK(out);
  Cursor cursor(this);
  uint16_t record_length;
  IPAddress address;
  if (Read(&record_length) && (record_length == IPAddress::kV4Size) &&
      Read(IPAddress::Version::kV4, &address)) {
    *out = ARecordRdata(address);
    cursor.Commit();
    return true;
  }
  return false;
}

bool MdnsReader::Read(AAAARecordRdata* out) {
  OSP_DCHECK(out);
  Cursor cursor(this);
  uint16_t record_length;
  IPAddress address;
  if (Read(&record_length) && (record_length == IPAddress::kV6Size) &&
      Read(IPAddress::Version::kV6, &address)) {
    *out = AAAARecordRdata(address);
    cursor.Commit();
    return true;
  }
  return false;
}

bool MdnsReader::Read(PtrRecordRdata* out) {
  OSP_DCHECK(out);
  Cursor cursor(this);
  uint16_t record_length;
  DomainName ptr_domain;
  if (Read(&record_length) && Read(&ptr_domain) &&
      (cursor.delta() == sizeof(record_length) + record_length)) {
    *out = PtrRecordRdata(std::move(ptr_domain));
    cursor.Commit();
    return true;
  }
  return false;
}

bool MdnsReader::Read(TxtRecordRdata* out) {
  OSP_DCHECK(out);
  Cursor cursor(this);
  uint16_t record_length;
  if (!Read(&record_length)) {
    return false;
  }
  std::vector<absl::string_view> texts;
  while (cursor.delta() < sizeof(record_length) + record_length) {
    absl::string_view entry;
    if (!Read(&entry)) {
      return false;
    }
    OSP_DCHECK(entry.length() <= kTXTMaxEntrySize);
    if (!entry.empty()) {
      texts.push_back(entry);
    }
  }
  if (cursor.delta() != sizeof(record_length) + record_length) {
    return false;
  }
  *out = TxtRecordRdata(texts);
  cursor.Commit();
  return true;
}

bool MdnsReader::Read(MdnsRecord* out) {
  OSP_DCHECK(out);
  Cursor cursor(this);
  DomainName name;
  uint16_t type;
  uint16_t rrclass;
  uint32_t ttl;
  Rdata rdata;
  if (Read(&name) && Read(&type) && Read(&rrclass) && Read(&ttl) &&
      Read(static_cast<DnsType>(type), &rdata)) {
    *out = MdnsRecord(std::move(name), static_cast<DnsType>(type),
                      GetDnsClass(rrclass), GetRecordType(rrclass), ttl,
                      std::move(rdata));
    cursor.Commit();
    return true;
  }
  return false;
}

bool MdnsReader::Read(MdnsQuestion* out) {
  OSP_DCHECK(out);
  Cursor cursor(this);
  DomainName name;
  uint16_t type;
  uint16_t rrclass;
  if (Read(&name) && Read(&type) && Read(&rrclass)) {
    *out = MdnsQuestion(std::move(name), static_cast<DnsType>(type),
                        GetDnsClass(rrclass), GetResponseType(rrclass));
    cursor.Commit();
    return true;
  }
  return false;
}

bool MdnsReader::Read(MdnsMessage* out) {
  OSP_DCHECK(out);
  Cursor cursor(this);
  Header header;
  std::vector<MdnsQuestion> questions;
  std::vector<MdnsRecord> answers;
  std::vector<MdnsRecord> authority_records;
  std::vector<MdnsRecord> additional_records;
  if (Read(&header) && Read(header.question_count, &questions) &&
      Read(header.answer_count, &answers) &&
      Read(header.authority_record_count, &authority_records) &&
      Read(header.additional_record_count, &additional_records)) {
    // TODO(yakimakha): Skip messages with non-zero opcode and rcode.
    // One way to do this is to change the method signature to return
    // ErrorOr<MdnsMessage> and return different error codes for failure to read
    // and for messages that were read successfully but are non-conforming.
    *out = MdnsMessage(header.id, GetMessageType(header.flags), questions,
                       answers, authority_records, additional_records);
    cursor.Commit();
    return true;
  }
  return false;
}

bool MdnsReader::Read(IPAddress::Version version, IPAddress* out) {
  OSP_DCHECK(out);
  size_t ipaddress_size = (version == IPAddress::Version::kV6)
                              ? IPAddress::kV6Size
                              : IPAddress::kV4Size;
  const uint8_t* const address_bytes = current();
  if (Skip(ipaddress_size)) {
    *out = IPAddress(version, address_bytes);
    return true;
  }
  return false;
}

bool MdnsReader::Read(DnsType type, Rdata* out) {
  OSP_DCHECK(out);
  switch (type) {
    case DnsType::kSRV:
      return Read<SrvRecordRdata>(out);
    case DnsType::kA:
      return Read<ARecordRdata>(out);
    case DnsType::kAAAA:
      return Read<AAAARecordRdata>(out);
    case DnsType::kPTR:
      return Read<PtrRecordRdata>(out);
    case DnsType::kTXT:
      return Read<TxtRecordRdata>(out);
    default:
      return Read<RawRecordRdata>(out);
  }
}

bool MdnsReader::Read(Header* out) {
  OSP_DCHECK(out);
  Cursor cursor(this);
  if (Read(&out->id) && Read(&out->flags) && Read(&out->question_count) &&
      Read(&out->answer_count) && Read(&out->authority_record_count) &&
      Read(&out->additional_record_count)) {
    cursor.Commit();
    return true;
  }
  return false;
}

}  // namespace mdns
}  // namespace cast
