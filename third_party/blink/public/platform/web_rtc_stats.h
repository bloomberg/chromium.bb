// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_STATS_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_STATS_H_

#include <memory>

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/api/stats/rtc_stats.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace webrtc {
class RTCStats;
class RTCStatsCollectorCallback;
class RTCStatsMemberInterface;
class RTCStatsReport;
enum class NonStandardGroupId;
}  // namespace webrtc

namespace blink {

class WebRTCStats;
class WebRTCStatsMember;

class BLINK_PLATFORM_EXPORT WebRTCStatsReport {
 public:
  virtual ~WebRTCStatsReport();
  // Creates a new report object that is a handle to the same underlying stats
  // report (the stats are not copied). The new report's iterator is reset,
  // useful when needing multiple iterators.
  virtual std::unique_ptr<WebRTCStatsReport> CopyHandle() const = 0;

  // Gets stats object by |id|, or null if no stats with that |id| exists.
  virtual std::unique_ptr<WebRTCStats> GetStats(WebString id) const = 0;
  // The next stats object, or null if the end has been reached.
  virtual std::unique_ptr<WebRTCStats> Next() = 0;
  // The number of stats objects.
  virtual size_t Size() const = 0;
};

BLINK_PLATFORM_EXPORT
std::unique_ptr<WebRTCStatsReport> CreateRTCStatsReport(
    const scoped_refptr<const webrtc::RTCStatsReport>& stats_report,
    const WebVector<webrtc::NonStandardGroupId>& exposed_group_ids);

class BLINK_PLATFORM_EXPORT WebRTCStats {
 public:
  virtual ~WebRTCStats();

  virtual WebString Id() const = 0;
  virtual WebString GetType() const = 0;
  virtual double Timestamp() const = 0;

  virtual size_t MembersCount() const = 0;
  virtual std::unique_ptr<WebRTCStatsMember> GetMember(size_t) const = 0;
};

BLINK_PLATFORM_EXPORT
std::unique_ptr<WebRTCStats> CreateRTCStats(
    const scoped_refptr<const webrtc::RTCStatsReport>& stats_owner,
    const webrtc::RTCStats* stats,
    const WebVector<webrtc::NonStandardGroupId>& exposed_group_ids);

class BLINK_PLATFORM_EXPORT WebRTCStatsMember {
 public:
  virtual ~WebRTCStatsMember();

  virtual WebString GetName() const = 0;
  virtual webrtc::RTCStatsMemberInterface::Type GetType() const = 0;
  virtual bool IsDefined() const = 0;

  // Value getters. No conversion is performed; the function must match the
  // member's |type|.
  virtual bool ValueBool() const = 0;
  virtual int32_t ValueInt32() const = 0;
  virtual uint32_t ValueUint32() const = 0;
  virtual int64_t ValueInt64() const = 0;
  virtual uint64_t ValueUint64() const = 0;
  virtual double ValueDouble() const = 0;
  virtual WebString ValueString() const = 0;
  // |WebVector<int> because |WebVector| is incompatible with |bool|.
  virtual WebVector<int> ValueSequenceBool() const = 0;
  virtual WebVector<int32_t> ValueSequenceInt32() const = 0;
  virtual WebVector<uint32_t> ValueSequenceUint32() const = 0;
  virtual WebVector<int64_t> ValueSequenceInt64() const = 0;
  virtual WebVector<uint64_t> ValueSequenceUint64() const = 0;
  virtual WebVector<double> ValueSequenceDouble() const = 0;
  virtual WebVector<WebString> ValueSequenceString() const = 0;
};

BLINK_PLATFORM_EXPORT
std::unique_ptr<WebRTCStatsMember> CreateRTCStatsMember(
    const scoped_refptr<const webrtc::RTCStatsReport>& stats_owner,
    const webrtc::RTCStatsMemberInterface* member);

using WebRTCStatsReportCallback =
    base::OnceCallback<void(std::unique_ptr<WebRTCStatsReport>)>;

BLINK_PLATFORM_EXPORT
rtc::scoped_refptr<webrtc::RTCStatsCollectorCallback>
CreateRTCStatsCollectorCallback(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    WebRTCStatsReportCallback callback,
    const WebVector<webrtc::NonStandardGroupId>& exposed_group_ids);

BLINK_PLATFORM_EXPORT void WhitelistStatsForTesting(const char* type);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_STATS_H_
