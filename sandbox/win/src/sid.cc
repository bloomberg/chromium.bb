// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sid.h"

#include <sddl.h>

#include "base/logging.h"
#include "base/win/windows_version.h"

namespace sandbox {

namespace {

typedef decltype(
    ::DeriveCapabilitySidsFromName) DeriveCapabilitySidsFromNameFunc;

class SidArray {
 public:
  SidArray() : count_(0), sids_(nullptr) {}

  ~SidArray() {
    if (sids_) {
      for (size_t index = 0; index < count_; ++index) {
        ::LocalFree(sids_[index]);
      }
      ::LocalFree(sids_);
    }
  }

  DWORD count() { return count_; }
  PSID* sids() { return sids_; }
  PDWORD count_ptr() { return &count_; }
  PSID** sids_ptr() { return &sids_; }

 private:
  DWORD count_;
  PSID* sids_;
};

const wchar_t* WellKnownCapabilityToName(WellKnownCapabilities capability) {
  switch (capability) {
    case kInternetClient:
      return L"internetClient";
    case kInternetClientServer:
      return L"internetClientServer";
    case kRegistryRead:
      return L"registryRead";
    case kLpacCryptoServices:
      return L"lpacCryptoServices";
    case kEnterpriseAuthentication:
      return L"enterpriseAuthentication";
    case kPrivateNetworkClientServer:
      return L"privateNetworkClientServer";
    default:
      return nullptr;
  }
}

}  // namespace

Sid::Sid() : sid_() {}

Sid::Sid(PSID sid) : sid_() {
  ::CopySid(SECURITY_MAX_SID_SIZE, sid_, sid);
}

Sid::Sid(const SID* sid) : sid_() {
  ::CopySid(SECURITY_MAX_SID_SIZE, sid_, const_cast<SID*>(sid));
}

Sid::Sid(WELL_KNOWN_SID_TYPE type) {
  DWORD size_sid = SECURITY_MAX_SID_SIZE;
  bool result = ::CreateWellKnownSid(type, nullptr, sid_, &size_sid);
  DCHECK(result);
  (void)result;
}

Sid Sid::FromKnownCapability(WellKnownCapabilities capability) {
  return Sid::FromNamedCapability(WellKnownCapabilityToName(capability));
}

Sid Sid::FromNamedCapability(const wchar_t* capability_name) {
  DeriveCapabilitySidsFromNameFunc* derive_capablity_sids =
      (DeriveCapabilitySidsFromNameFunc*)GetProcAddress(
          GetModuleHandle(L"kernelbase"), "DeriveCapabilitySidsFromName");
  if (!derive_capablity_sids)
    return Sid();

  if (!capability_name || ::wcslen(capability_name) == 0)
    return Sid();

  SidArray capability_group_sids;
  SidArray capability_sids;

  if (!derive_capablity_sids(capability_name, capability_group_sids.sids_ptr(),
                             capability_group_sids.count_ptr(),
                             capability_sids.sids_ptr(),
                             capability_sids.count_ptr())) {
    return Sid();
  }

  if (capability_sids.count() < 1)
    return Sid();

  return Sid(capability_sids.sids()[0]);
}

Sid Sid::FromSddlString(const wchar_t* sddl_sid) {
  PSID converted_sid;
  if (!::ConvertStringSidToSid(sddl_sid, &converted_sid))
    return Sid();

  return Sid(converted_sid);
}

PSID Sid::GetPSID() const {
  return const_cast<BYTE*>(sid_);
}

bool Sid::IsValid() const {
  return !!::IsValidSid(GetPSID());
}

}  // namespace sandbox
