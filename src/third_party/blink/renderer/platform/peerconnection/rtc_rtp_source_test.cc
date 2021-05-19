#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/rtp_headers.h"
#include "third_party/webrtc/api/transport/rtp/rtp_source.h"
#include "third_party/webrtc/rtc_base/time_utils.h"

namespace blink {
namespace {

constexpr int64_t kTimestampMs = 12345678;
constexpr uint32_t kSourceId = 5;
constexpr webrtc::RtpSourceType kSourceType = webrtc::RtpSourceType::SSRC;
constexpr uint32_t kRtpTimestamp = 112233;

// Q32x32 formatted timestamps.
constexpr uint64_t kUint64One = 1;
constexpr uint64_t kQ32x32Time1000ms = kUint64One << 32;

}  // namespace

TEST(RtcRtpSource, BasicPropertiesAreSetAndReturned) {
  webrtc::RtpSource rtp_source(kTimestampMs, kSourceId, kSourceType,
                               kRtpTimestamp, webrtc::RtpSource::Extensions());

  RTCRtpSource rtc_rtp_source(rtp_source);

  EXPECT_EQ((rtc_rtp_source.Timestamp() - base::TimeTicks()).InMilliseconds(),
            kTimestampMs);
  EXPECT_EQ(rtc_rtp_source.Source(), kSourceId);
  EXPECT_EQ(rtc_rtp_source.SourceType(), RTCRtpSource::Type::kSSRC);
  EXPECT_EQ(rtc_rtp_source.RtpTimestamp(), kRtpTimestamp);
}

// The Timestamp() function relies on the fact that Base::TimeTicks() and
// rtc::TimeMicros() share the same implementation.
TEST(RtcRtpSource, BaseTimeTicksAndRtcMicrosAreTheSame) {
  base::TimeTicks first_chromium_timestamp = base::TimeTicks::Now();
  base::TimeTicks webrtc_timestamp =
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(rtc::TimeMicros());
  base::TimeTicks second_chromium_timestamp = base::TimeTicks::Now();

  // Test that the timestamps are correctly ordered, which they can only be if
  // the clocks are the same (assuming at least one of the clocks is functioning
  // correctly).
  EXPECT_GE((webrtc_timestamp - first_chromium_timestamp).InMillisecondsF(),
            0.0f);
  EXPECT_GE((second_chromium_timestamp - webrtc_timestamp).InMillisecondsF(),
            0.0f);
}

TEST(RtcRtpSource, AbsoluteCaptureTimeSetAndReturnedNoOffset) {
  constexpr webrtc::AbsoluteCaptureTime kAbsCaptureTime{
      .absolute_capture_timestamp = kQ32x32Time1000ms};
  webrtc::RtpSource rtp_source(
      kTimestampMs, kSourceId, kSourceType, kRtpTimestamp,
      /*extensions=*/{.absolute_capture_time = kAbsCaptureTime});
  RTCRtpSource rtc_rtp_source(rtp_source);
  EXPECT_EQ(rtc_rtp_source.CaptureTimestamp(), 1000);
}

}  // namespace blink
