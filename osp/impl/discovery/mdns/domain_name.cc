// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/discovery/mdns/domain_name.h"

#include <algorithm>
#include <iterator>

#include "util/stringprintf.h"

namespace openscreen {
namespace osp {

// static
DomainName DomainName::GetLocalDomain() {
  return DomainName{{5, 'l', 'o', 'c', 'a', 'l', 0}};
}

// static
ErrorOr<DomainName> DomainName::Append(const DomainName& first,
                                       const DomainName& second) {
  OSP_CHECK(first.domain_name_.size());
  OSP_CHECK(second.domain_name_.size());

  // Both vectors should represent null terminated domain names.
  OSP_DCHECK_EQ(first.domain_name_.back(), '\0');
  OSP_DCHECK_EQ(second.domain_name_.back(), '\0');
  if ((first.domain_name_.size() + second.domain_name_.size() - 1) >
      kDomainNameMaxLength) {
    return Error::Code::kDomainNameTooLong;
  }

  DomainName result;
  result.domain_name_.clear();
  result.domain_name_.insert(result.domain_name_.begin(),
                             first.domain_name_.begin(),
                             first.domain_name_.end());
  result.domain_name_.insert(result.domain_name_.end() - 1,
                             second.domain_name_.begin(),
                             second.domain_name_.end() - 1);
  return result;
}

DomainName::DomainName() : domain_name_{0u} {}
DomainName::DomainName(std::vector<uint8_t>&& domain_name)
    : domain_name_(std::move(domain_name)) {
  OSP_CHECK_LE(domain_name_.size(), kDomainNameMaxLength);
}
DomainName::DomainName(const DomainName&) = default;
DomainName::DomainName(DomainName&&) noexcept = default;
DomainName::~DomainName() = default;
DomainName& DomainName::operator=(const DomainName&) = default;
DomainName& DomainName::operator=(DomainName&&) noexcept = default;

bool DomainName::operator==(const DomainName& other) const {
  if (domain_name_.size() != other.domain_name_.size()) {
    return false;
  }
  for (size_t i = 0; i < domain_name_.size(); ++i) {
    if (tolower(domain_name_[i]) != tolower(other.domain_name_[i])) {
      return false;
    }
  }
  return true;
}

bool DomainName::operator!=(const DomainName& other) const {
  return !(*this == other);
}

bool DomainName::EndsWithLocalDomain() const {
  const DomainName local_domain = GetLocalDomain();
  if (domain_name_.size() < local_domain.domain_name_.size())
    return false;

  return std::equal(local_domain.domain_name_.begin(),
                    local_domain.domain_name_.end(),
                    domain_name_.end() - local_domain.domain_name_.size());
}

Error DomainName::Append(const DomainName& after) {
  OSP_CHECK(after.domain_name_.size());
  OSP_DCHECK_EQ(after.domain_name_.back(), 0u);

  if ((domain_name_.size() + after.domain_name_.size() - 1) >
      kDomainNameMaxLength) {
    return Error::Code::kDomainNameTooLong;
  }

  domain_name_.insert(domain_name_.end() - 1, after.domain_name_.begin(),
                      after.domain_name_.end() - 1);
  return Error::None();
}

std::vector<absl::string_view> DomainName::GetLabels() const {
  OSP_DCHECK_GT(domain_name_.size(), 0u);
  OSP_DCHECK_LT(domain_name_.size(), kDomainNameMaxLength);

  std::vector<absl::string_view> result;
  const uint8_t* data = domain_name_.data();
  while (*data != 0) {
    const size_t label_length = *data;
    OSP_DCHECK_LT(label_length, kDomainNameMaxLabelLength);

    ++data;
    result.emplace_back(reinterpret_cast<const char*>(data), label_length);
    data += label_length;
  }
  return result;
}

bool DomainNameComparator::operator()(const DomainName& a,
                                      const DomainName& b) const {
  return a.domain_name() < b.domain_name();
}

std::ostream& operator<<(std::ostream& os, const DomainName& domain_name) {
  const auto& data = domain_name.domain_name();
  OSP_DCHECK_GT(data.size(), 0u);
  auto it = data.begin();
  while (*it != 0) {
    size_t length = *it++;
    PrettyPrintAsciiHex(os, it, it + length);
    it += length;
    os << ".";
  }
  return os;
}

}  // namespace osp
}  // namespace openscreen
