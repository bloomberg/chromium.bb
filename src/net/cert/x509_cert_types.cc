// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_cert_types.h"

#include "net/cert/internal/parse_name.h"
#include "net/der/input.h"

namespace net {

CertPrincipal::CertPrincipal() = default;

CertPrincipal::CertPrincipal(const std::string& name) : common_name(name) {}

CertPrincipal::~CertPrincipal() = default;

bool CertPrincipal::ParseDistinguishedName(
    const void* ber_name_data,
    size_t length,
    PrintableStringHandling printable_string_handling) {
  RDNSequence rdns;
  if (!ParseName(
          der::Input(reinterpret_cast<const uint8_t*>(ber_name_data), length),
          &rdns))
    return false;

  auto string_handling =
      printable_string_handling == PrintableStringHandling::kAsUTF8Hack
          ? X509NameAttribute::PrintableStringHandling::kAsUTF8Hack
          : X509NameAttribute::PrintableStringHandling::kDefault;
  for (const RelativeDistinguishedName& rdn : rdns) {
    for (const X509NameAttribute& name_attribute : rdn) {
      if (name_attribute.type == TypeCommonNameOid()) {
        if (common_name.empty() &&
            !name_attribute.ValueAsStringWithUnsafeOptions(string_handling,
                                                           &common_name)) {
          return false;
        }
      } else if (name_attribute.type == TypeLocalityNameOid()) {
        if (locality_name.empty() &&
            !name_attribute.ValueAsStringWithUnsafeOptions(string_handling,
                                                           &locality_name)) {
          return false;
        }
      } else if (name_attribute.type == TypeStateOrProvinceNameOid()) {
        if (state_or_province_name.empty() &&
            !name_attribute.ValueAsStringWithUnsafeOptions(
                string_handling, &state_or_province_name)) {
          return false;
        }
      } else if (name_attribute.type == TypeCountryNameOid()) {
        if (country_name.empty() &&
            !name_attribute.ValueAsStringWithUnsafeOptions(string_handling,
                                                           &country_name)) {
          return false;
        }
      } else if (name_attribute.type == TypeStreetAddressOid()) {
        std::string s;
        if (!name_attribute.ValueAsStringWithUnsafeOptions(string_handling, &s))
          return false;
        street_addresses.push_back(s);
      } else if (name_attribute.type == TypeOrganizationNameOid()) {
        std::string s;
        if (!name_attribute.ValueAsStringWithUnsafeOptions(string_handling, &s))
          return false;
        organization_names.push_back(s);
      } else if (name_attribute.type == TypeOrganizationUnitNameOid()) {
        std::string s;
        if (!name_attribute.ValueAsStringWithUnsafeOptions(string_handling, &s))
          return false;
        organization_unit_names.push_back(s);
      } else if (name_attribute.type == TypeDomainComponentOid()) {
        std::string s;
        if (!name_attribute.ValueAsStringWithUnsafeOptions(string_handling, &s))
          return false;
        domain_components.push_back(s);
      }
    }
  }
  return true;
}

std::string CertPrincipal::GetDisplayName() const {
  if (!common_name.empty())
    return common_name;
  if (!organization_names.empty())
    return organization_names[0];
  if (!organization_unit_names.empty())
    return organization_unit_names[0];

  return std::string();
}

}  // namespace net
