// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_WEB_RTC_CROSS_THREAD_COPIER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_WEB_RTC_CROSS_THREAD_COPIER_H_

// This file defines specializations for the CrossThreadCopier that allow WebRTC
// types to be passed across threads using their copy constructors.

#include <set>
#include <vector>

#include "third_party/blink/renderer/platform/cross_thread_copier.h"

namespace cricket {
class Candidate;
struct IceParameters;
struct RelayServerConfig;
}  // namespace cricket

namespace rtc {
class SocketAddress;
}

namespace blink {

template <>
struct CrossThreadCopier<cricket::IceParameters>
    : public CrossThreadCopierPassThrough<cricket::IceParameters> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<std::set<rtc::SocketAddress>>
    : public CrossThreadCopierPassThrough<std::set<rtc::SocketAddress>> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<std::vector<cricket::RelayServerConfig>>
    : public CrossThreadCopierPassThrough<
          std::vector<cricket::RelayServerConfig>> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<std::vector<cricket::Candidate>>
    : public CrossThreadCopierPassThrough<std::vector<cricket::Candidate>> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<cricket::Candidate>
    : public CrossThreadCopierPassThrough<cricket::Candidate> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_WEB_RTC_CROSS_THREAD_COPIER_H_
