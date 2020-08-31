// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_capabilities/media_capabilities.h"

#include <math.h>

#include <algorithm>

#include "base/strings/string_number_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/learning/common/media_learning_tasks.h"
#include "media/learning/common/target_histogram.h"
#include "media/learning/mojo/public/mojom/learning_task_controller.mojom-blink.h"
#include "media/mojo/mojom/media_metrics_provider.mojom-blink.h"
#include "media/mojo/mojom/media_types.mojom-blink.h"
#include "media/mojo/mojom/video_decode_perf_history.mojom-blink.h"
#include "media/mojo/mojom/watch_time_recorder.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_capabilities_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_decoding_configuration.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock-actions.h"
#include "ui/gfx/geometry/size.h"

using ::media::learning::FeatureValue;
using ::media::learning::ObservationCompletion;
using ::media::learning::TargetValue;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Unused;

namespace blink {

namespace {

// Simulating the browser-side service.
class MockPerfHistoryService
    : public media::mojom::blink::VideoDecodePerfHistory {
 public:
  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(
        mojo::PendingReceiver<media::mojom::blink::VideoDecodePerfHistory>(
            std::move(handle)));
    receiver_.set_disconnect_handler(base::BindOnce(
        &MockPerfHistoryService::OnConnectionError, base::Unretained(this)));
  }

  void OnConnectionError() { receiver_.reset(); }

  // media::mojom::blink::VideoDecodePerfHistory implementation:
  MOCK_METHOD2(GetPerfInfo,
               void(media::mojom::blink::PredictionFeaturesPtr features,
                    GetPerfInfoCallback got_info_cb));

 private:
  mojo::Receiver<media::mojom::blink::VideoDecodePerfHistory> receiver_{this};
};

class MockLearningTaskControllerService
    : public media::learning::mojom::blink::LearningTaskController {
 public:
  void BindRequest(mojo::PendingReceiver<
                   media::learning::mojom::blink::LearningTaskController>
                       pending_receiver) {
    receiver_.Bind(std::move(pending_receiver));
    receiver_.set_disconnect_handler(
        base::BindOnce(&MockLearningTaskControllerService::OnConnectionError,
                       base::Unretained(this)));
  }

  void OnConnectionError() { receiver_.reset(); }

  bool is_bound() const { return receiver_.is_bound(); }

  // media::mojom::blink::LearningTaskController implementation:
  MOCK_METHOD3(BeginObservation,
               void(const base::UnguessableToken& id,
                    const WTF::Vector<FeatureValue>& features,
                    const base::Optional<TargetValue>& default_target));
  MOCK_METHOD2(CompleteObservation,
               void(const base::UnguessableToken& id,
                    const ObservationCompletion& completion));
  MOCK_METHOD1(CancelObservation, void(const base::UnguessableToken& id));
  MOCK_METHOD2(UpdateDefaultTarget,
               void(const base::UnguessableToken& id,
                    const base::Optional<TargetValue>& default_target));
  MOCK_METHOD2(PredictDistribution,
               void(const WTF::Vector<FeatureValue>& features,
                    PredictDistributionCallback callback));

 private:
  mojo::Receiver<media::learning::mojom::blink::LearningTaskController>
      receiver_{this};
};

class FakeMediaMetricsProvider
    : public media::mojom::blink::MediaMetricsProvider {
 public:
  // Raw pointers to services owned by the test.
  FakeMediaMetricsProvider(
      MockLearningTaskControllerService* bad_window_service,
      MockLearningTaskControllerService* nnr_service)
      : bad_window_service_(bad_window_service), nnr_service_(nnr_service) {}

  ~FakeMediaMetricsProvider() override = default;

  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(
        mojo::PendingReceiver<media::mojom::blink::MediaMetricsProvider>(
            std::move(handle)));
    receiver_.set_disconnect_handler(base::BindOnce(
        &FakeMediaMetricsProvider::OnConnectionError, base::Unretained(this)));
  }

  void OnConnectionError() { receiver_.reset(); }

  // mojom::WatchTimeRecorderProvider implementation:
  void AcquireWatchTimeRecorder(
      media::mojom::blink::PlaybackPropertiesPtr properties,
      mojo::PendingReceiver<media::mojom::blink::WatchTimeRecorder> receiver)
      override {
    FAIL();
  }
  void AcquireVideoDecodeStatsRecorder(
      mojo::PendingReceiver<media::mojom::blink::VideoDecodeStatsRecorder>
          receiver) override {
    FAIL();
  }
  void AcquireLearningTaskController(
      const WTF::String& taskName,
      mojo::PendingReceiver<
          media::learning::mojom::blink::LearningTaskController>
          pending_receiver) override {
    if (taskName == media::learning::tasknames::kConsecutiveBadWindows) {
      bad_window_service_->BindRequest(std::move(pending_receiver));
      return;
    }

    if (taskName == media::learning::tasknames::kConsecutiveNNRs) {
      nnr_service_->BindRequest(std::move(pending_receiver));
      return;
    }
    FAIL();
  }
  void AcquirePlaybackEventsRecorder(
      mojo::PendingReceiver<media::mojom::blink::PlaybackEventsRecorder>
          receiver) override {
    FAIL();
  }
  void Initialize(bool is_mse,
                  media::mojom::MediaURLScheme url_scheme) override {}
  void OnError(media::mojom::PipelineStatus status) override {}
  void SetIsEME() override {}
  void SetTimeToMetadata(base::TimeDelta elapsed) override {}
  void SetTimeToFirstFrame(base::TimeDelta elapsed) override {}
  void SetTimeToPlayReady(base::TimeDelta elapsed) override {}
  void SetContainerName(
      media::mojom::blink::MediaContainerName container_name) override {}
  void SetHasPlayed() override {}
  void SetHaveEnough() override {}
  void SetHasAudio(media::mojom::AudioCodec audio_codec) override {}
  void SetHasVideo(media::mojom::VideoCodec video_codec) override {}
  void SetVideoPipelineInfo(
      media::mojom::blink::PipelineDecoderInfoPtr info) override {}
  void SetAudioPipelineInfo(
      media::mojom::blink::PipelineDecoderInfoPtr info) override {}

 private:
  mojo::Receiver<media::mojom::blink::MediaMetricsProvider> receiver_{this};
  MockLearningTaskControllerService* bad_window_service_;
  MockLearningTaskControllerService* nnr_service_;
};

// Simple helper for saving back-end callbacks for pending decodingInfo() calls.
// Callers can then manually fire the callbacks, gaining fine-grain control of
// the timing and order of their arrival.
class CallbackSaver {
 public:
  void SavePerfHistoryCallback(
      media::mojom::blink::PredictionFeaturesPtr features,
      MockPerfHistoryService::GetPerfInfoCallback got_info_cb) {
    perf_history_cb_ = std::move(got_info_cb);
  }

  void SaveBadWindowCallback(
      Vector<media::learning::FeatureValue> features,
      MockLearningTaskControllerService::PredictDistributionCallback
          predict_cb) {
    bad_window_cb_ = std::move(predict_cb);
  }

  void SaveNnrCallback(
      Vector<media::learning::FeatureValue> features,
      MockLearningTaskControllerService::PredictDistributionCallback
          predict_cb) {
    nnr_cb_ = std::move(predict_cb);
  }

  MockPerfHistoryService::GetPerfInfoCallback& perf_history_cb() {
    return perf_history_cb_;
  }

  MockLearningTaskControllerService::PredictDistributionCallback&
  bad_window_cb() {
    return bad_window_cb_;
  }

  MockLearningTaskControllerService::PredictDistributionCallback& nnr_cb() {
    return nnr_cb_;
  }

 private:
  MockPerfHistoryService::GetPerfInfoCallback perf_history_cb_;
  MockLearningTaskControllerService::PredictDistributionCallback bad_window_cb_;
  MockLearningTaskControllerService::PredictDistributionCallback nnr_cb_;
};

// This would typically be a test fixture, but we need it to be
// STACK_ALLOCATED() in order to use V8TestingScope, and we can't force that on
// whatever gtest class instantiates the fixture.
class MediaCapabilitiesTestContext {
  STACK_ALLOCATED();

 public:
  MediaCapabilitiesTestContext() {
    perf_history_service_ = std::make_unique<MockPerfHistoryService>();
    bad_window_service_ = std::make_unique<MockLearningTaskControllerService>();
    nnr_service_ = std::make_unique<MockLearningTaskControllerService>();
    fake_metrics_provider_ = std::make_unique<FakeMediaMetricsProvider>(
        bad_window_service_.get(), nnr_service_.get());

    CHECK(v8_scope_.GetExecutionContext()
              ->GetBrowserInterfaceBroker()
              .SetBinderForTesting(
                  media::mojom::blink::MediaMetricsProvider::Name_,
                  base::BindRepeating(
                      &FakeMediaMetricsProvider::BindRequest,
                      base::Unretained(fake_metrics_provider_.get()))));

    CHECK(v8_scope_.GetExecutionContext()
              ->GetBrowserInterfaceBroker()
              .SetBinderForTesting(
                  media::mojom::blink::VideoDecodePerfHistory::Name_,
                  base::BindRepeating(
                      &MockPerfHistoryService::BindRequest,
                      base::Unretained(perf_history_service_.get()))));

    media_capabilities_ = MakeGarbageCollected<MediaCapabilities>(
        v8_scope_.GetExecutionContext());
  }

  ~MediaCapabilitiesTestContext() {
    CHECK(v8_scope_.GetExecutionContext()
              ->GetBrowserInterfaceBroker()
              .SetBinderForTesting(
                  media::mojom::blink::MediaMetricsProvider::Name_, {}));

    CHECK(v8_scope_.GetExecutionContext()
              ->GetBrowserInterfaceBroker()
              .SetBinderForTesting(
                  media::mojom::blink::VideoDecodePerfHistory::Name_, {}));
  }

  ExceptionState& GetExceptionState() { return v8_scope_.GetExceptionState(); }

  ScriptState* GetScriptState() const { return v8_scope_.GetScriptState(); }

  v8::Isolate* GetIsolate() const { return GetScriptState()->GetIsolate(); }

  MediaCapabilities* GetMediaCapabilities() const {
    return media_capabilities_.Get();
  }

  MockPerfHistoryService* GetPerfHistoryService() const {
    return perf_history_service_.get();
  }

  MockLearningTaskControllerService* GetBadWindowService() const {
    return bad_window_service_.get();
  }

  MockLearningTaskControllerService* GetNnrService() const {
    return nnr_service_.get();
  }

  void VerifyAndClearMockExpectations() {
    testing::Mock::VerifyAndClearExpectations(GetPerfHistoryService());
    testing::Mock::VerifyAndClearExpectations(GetNnrService());
    testing::Mock::VerifyAndClearExpectations(GetBadWindowService());
  }

 private:
  V8TestingScope v8_scope_;
  std::unique_ptr<MockPerfHistoryService> perf_history_service_;
  std::unique_ptr<FakeMediaMetricsProvider> fake_metrics_provider_;
  Persistent<MediaCapabilities> media_capabilities_;
  std::unique_ptr<MockLearningTaskControllerService> bad_window_service_;
  std::unique_ptr<MockLearningTaskControllerService> nnr_service_;
};

// |kContentType|, |kCodec|, and |kCodecProfile| must match.
const char kContentType[] = "video/webm; codecs=\"vp09.00.10.08\"";
const media::VideoCodecProfile kCodecProfile = media::VP9PROFILE_PROFILE0;
const media::VideoCodec kCodec = media::kCodecVP9;
const double kFramerate = 20.5;
const int kWidth = 3840;
const int kHeight = 2160;
const int kBitrate = 2391000;

// Construct VideoConfig using the constants above.
MediaDecodingConfiguration* CreateDecodingConfig() {
  auto* video_config = MakeGarbageCollected<VideoConfiguration>();
  video_config->setFramerate(kFramerate);
  video_config->setContentType(kContentType);
  video_config->setWidth(kWidth);
  video_config->setHeight(kHeight);
  video_config->setBitrate(kBitrate);
  auto* decoding_config = MakeGarbageCollected<MediaDecodingConfiguration>();
  decoding_config->setType("media-source");
  decoding_config->setVideo(video_config);
  return decoding_config;
}

// Construct PredicitonFeatures matching the CreateDecodingConfig, using the
// constants above.
media::mojom::blink::PredictionFeatures CreateFeatures() {
  media::mojom::blink::PredictionFeatures features;
  features.profile =
      static_cast<media::mojom::blink::VideoCodecProfile>(kCodecProfile);
  features.video_size = gfx::Size(kWidth, kHeight);
  features.frames_per_sec = kFramerate;

  // Not set by any tests so far. Choosing sane defaults to mirror production
  // code.
  features.key_system = "";
  features.use_hw_secure_codecs = false;

  return features;
}

Vector<media::learning::FeatureValue> CreateFeaturesML() {
  media::mojom::blink::PredictionFeatures features = CreateFeatures();

  // FRAGILE: Order here MUST match order in
  // WebMediaPlayerImpl::UpdateSmoothnessHelper().
  // TODO(chcunningham): refactor into something more robust.
  Vector<media::learning::FeatureValue> ml_features(
      {media::learning::FeatureValue(kCodec),
       media::learning::FeatureValue(kCodecProfile),
       media::learning::FeatureValue(kWidth),
       media::learning::FeatureValue(kFramerate)});

  return ml_features;
}

// Types of smoothness predictions.
enum class PredictionType {
  kDB,
  kBadWindow,
  kNnr,
};

// Makes a TargetHistogram with single count at |target_value|.
media::learning::TargetHistogram MakeHistogram(double target_value) {
  media::learning::TargetHistogram histogram;
  histogram += media::learning::TargetValue(target_value);
  return histogram;
}

// Makes DB (PerfHistoryService) callback for use with gtest WillOnce().
// Callback will verify |features| matches |expected_features| and run with
// provided values for |is_smooth| and |is_power_efficient|.
testing::Action<void(media::mojom::blink::PredictionFeaturesPtr,
                     MockPerfHistoryService::GetPerfInfoCallback)>
DbCallback(const media::mojom::blink::PredictionFeatures& expected_features,
           bool is_smooth,
           bool is_power_efficient) {
  return [=](media::mojom::blink::PredictionFeaturesPtr features,
             MockPerfHistoryService::GetPerfInfoCallback got_info_cb) {
    EXPECT_TRUE(features->Equals(expected_features));
    std::move(got_info_cb).Run(is_smooth, is_power_efficient);
  };
}

// Makes ML (LearningTaskControllerService) callback for use with gtest
// WillOnce(). Callback will verify |features| matches |expected_features| and
// run a TargetHistogram containing a single count for |histogram_target|.
testing::Action<void(
    const Vector<media::learning::FeatureValue>&,
    MockLearningTaskControllerService::PredictDistributionCallback predict_cb)>
MlCallback(const Vector<media::learning::FeatureValue>& expected_features,
           double histogram_target) {
  return [=](const Vector<media::learning::FeatureValue>& features,
             MockLearningTaskControllerService::PredictDistributionCallback
                 predict_cb) {
    EXPECT_EQ(features, expected_features);
    std::move(predict_cb).Run(MakeHistogram(histogram_target));
  };
}

// Helper to constructs field trial params with given ML prediction thresholds.
base::FieldTrialParams MakeMlParams(double bad_window_threshold,
                                    double nnr_threshold) {
  base::FieldTrialParams params;
  params[MediaCapabilities::kLearningBadWindowThresholdParamName] =
      base::NumberToString(bad_window_threshold);
  params[MediaCapabilities::kLearningNnrThresholdParamName] =
      base::NumberToString(nnr_threshold);
  return params;
}

// Wrapping deocdingInfo() call for readability. Await resolution of the promise
// and return its info.
MediaCapabilitiesInfo* DecodingInfo(
    const MediaDecodingConfiguration* decoding_config,
    MediaCapabilitiesTestContext* context) {
  ScriptPromise promise = context->GetMediaCapabilities()->decodingInfo(
      context->GetScriptState(), decoding_config, context->GetExceptionState());

  ScriptPromiseTester tester(context->GetScriptState(), promise);
  tester.WaitUntilSettled();

  CHECK(!tester.IsRejected()) << " Cant get info from rejected promise.";

  return NativeValueTraits<MediaCapabilitiesInfo>::NativeValue(
      context->GetIsolate(), tester.Value().V8Value(),
      context->GetExceptionState());
}

}  // namespace

// Other tests will assume these match. Test to be sure they stay in sync.
TEST(MediaCapabilitiesTests, ConfigMatchesFeatures) {
  const MediaDecodingConfiguration* kDecodingConfig = CreateDecodingConfig();
  const media::mojom::blink::PredictionFeatures kFeatures = CreateFeatures();

  EXPECT_TRUE(kDecodingConfig->video()->contentType().Contains("vp09.00"));
  EXPECT_EQ(static_cast<media::VideoCodecProfile>(kFeatures.profile),
            media::VP9PROFILE_PROFILE0);
  EXPECT_EQ(kCodecProfile, media::VP9PROFILE_PROFILE0);

  EXPECT_EQ(kDecodingConfig->video()->framerate(), kFeatures.frames_per_sec);
  EXPECT_EQ(kDecodingConfig->video()->width(),
            static_cast<uint32_t>(kFeatures.video_size.width()));
  EXPECT_EQ(kDecodingConfig->video()->height(),
            static_cast<uint32_t>(kFeatures.video_size.height()));
}

// Test that non-integer framerate isn't truncated by IPC.
// https://crbug.com/1024399
TEST(MediaCapabilitiesTests, NonIntegerFramerate) {
  MediaCapabilitiesTestContext context;

  const auto* kDecodingConfig = CreateDecodingConfig();
  const media::mojom::blink::PredictionFeatures kFeatures = CreateFeatures();

  // FPS for this test must not be a whole number. Assert to ensure the default
  // config meets that condition.
  ASSERT_NE(fmod(kDecodingConfig->video()->framerate(), 1), 0);

  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce([&](media::mojom::blink::PredictionFeaturesPtr features,
                    MockPerfHistoryService::GetPerfInfoCallback got_info_cb) {
        // Explicitly check for frames_per_sec equality.
        // PredictionFeatures::Equals() will not catch loss of precision if
        // frames_per_sec is made to be int (currently a double).
        EXPECT_EQ(features->frames_per_sec, kFramerate);

        // Check that other things match as well.
        EXPECT_TRUE(features->Equals(kFeatures));

        std::move(got_info_cb).Run(/*smooth*/ true, /*power_efficient*/ true);
      });

  MediaCapabilitiesInfo* info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_TRUE(info->smooth());
  EXPECT_TRUE(info->powerEfficient());
}

// Test smoothness predictions from DB (PerfHistoryService).
TEST(MediaCapabilitiesTests, PredictWithJustDB) {
  // Disable ML predictions (may/may not be disabled by default).
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      media::kMediaLearningSmoothnessExperiment);

  MediaCapabilitiesTestContext context;
  const auto* kDecodingConfig = CreateDecodingConfig();
  const media::mojom::blink::PredictionFeatures kFeatures = CreateFeatures();

  // ML services should not be called for prediction.
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .Times(0);
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _)).Times(0);

  // DB alone (PerfHistoryService) should be called. Signal smooth=true and
  // power_efficient = false.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ true, /*power_eff*/ false));
  MediaCapabilitiesInfo* info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_TRUE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());

  // Verify DB call was made. ML services should not even be bound.
  testing::Mock::VerifyAndClearExpectations(context.GetPerfHistoryService());
  EXPECT_FALSE(context.GetBadWindowService()->is_bound());
  EXPECT_FALSE(context.GetNnrService()->is_bound());

  // Repeat test with inverted smooth and power_efficient results.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ false, /*power_eff*/ true));
  info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_FALSE(info->smooth());
  EXPECT_TRUE(info->powerEfficient());
}

// Test with smoothness predictions coming solely from "bad window" ML service.
TEST(MediaCapabilitiesTests, PredictWithBadWindowMLService) {
  // Enable ML predictions with thresholds. -1 disables the NNR predictor.
  const double kBadWindowThreshold = 2;
  const double kNnrThreshold = -1;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      media::kMediaLearningSmoothnessExperiment,
      MakeMlParams(kBadWindowThreshold, kNnrThreshold));

  MediaCapabilitiesTestContext context;
  const auto* kDecodingConfig = CreateDecodingConfig();
  const media::mojom::blink::PredictionFeatures kFeatures = CreateFeatures();
  const Vector<media::learning::FeatureValue> kFeaturesML = CreateFeaturesML();

  // ML is enabled, but DB should still be called for power efficiency (false).
  // Its smoothness value (true) should be ignored in favor of ML prediction.
  // Only bad window service should be asked for a prediction. We exceed the
  // bad window threshold to trigger smooth=false.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ true, /*efficient*/ false));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kBadWindowThreshold + 0.5));
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _)).Times(0);
  MediaCapabilitiesInfo* info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_FALSE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());
  // NNR service should not be bound when NNR predictions disabled.
  EXPECT_FALSE(context.GetNnrService()->is_bound());
  context.VerifyAndClearMockExpectations();

  // Same as above, but invert all signals. Expect smooth=true because bad
  // window prediction is now = its threshold.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ false, /*efficient*/ true));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kBadWindowThreshold));
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _)).Times(0);
  info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_TRUE(info->smooth());
  EXPECT_TRUE(info->powerEfficient());
  EXPECT_FALSE(context.GetNnrService()->is_bound());
  context.VerifyAndClearMockExpectations();

  // Same as above, but predict zero bad windows. Expect smooth=true because
  // zero is below the threshold.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ false, /*efficient*/ true));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, /* bad windows */ 0));
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _)).Times(0);
  info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_TRUE(info->smooth());
  EXPECT_TRUE(info->powerEfficient());
  EXPECT_FALSE(context.GetNnrService()->is_bound());
  context.VerifyAndClearMockExpectations();
}

// Test with smoothness predictions coming solely from "NNR" ML service.
TEST(MediaCapabilitiesTests, PredictWithNnrMLService) {
  // Enable ML predictions with thresholds. -1 disables the bad window
  // predictor.
  const double kBadWindowThreshold = -1;
  const double kNnrThreshold = 5;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      media::kMediaLearningSmoothnessExperiment,
      MakeMlParams(kBadWindowThreshold, kNnrThreshold));

  MediaCapabilitiesTestContext context;
  const auto* kDecodingConfig = CreateDecodingConfig();
  const media::mojom::blink::PredictionFeatures kFeatures = CreateFeatures();
  const Vector<media::learning::FeatureValue> kFeaturesML = CreateFeaturesML();

  // ML is enabled, but DB should still be called for power efficiency (false).
  // Its smoothness value (true) should be ignored in favor of ML prediction.
  // Only NNR service should be asked for a prediction. We exceed the NNR
  // threshold to trigger smooth=false.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ true, /*efficient*/ false));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .Times(0);
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kNnrThreshold + 0.5));
  MediaCapabilitiesInfo* info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_FALSE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());
  // Bad window service should not be bound when NNR predictions disabled.
  EXPECT_FALSE(context.GetBadWindowService()->is_bound());
  context.VerifyAndClearMockExpectations();

  // Same as above, but invert all signals. Expect smooth=true because NNR
  // prediction is now = its threshold.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ false, /*efficient*/ true));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .Times(0);
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kNnrThreshold));
  info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_TRUE(info->smooth());
  EXPECT_TRUE(info->powerEfficient());
  EXPECT_FALSE(context.GetBadWindowService()->is_bound());
  context.VerifyAndClearMockExpectations();

  // Same as above, but predict zero NNRs. Expect smooth=true because zero is
  // below the threshold.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ false, /*efficient*/ true));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .Times(0);
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, /* NNRs */ 0));
  info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_TRUE(info->smooth());
  EXPECT_TRUE(info->powerEfficient());
  EXPECT_FALSE(context.GetBadWindowService()->is_bound());
  context.VerifyAndClearMockExpectations();
}

// Test with combined smoothness predictions from both ML services.
TEST(MediaCapabilitiesTests, PredictWithBothMLServices) {
  // Enable ML predictions with thresholds.
  const double kBadWindowThreshold = 2;
  const double kNnrThreshold = 1;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      media::kMediaLearningSmoothnessExperiment,
      MakeMlParams(kBadWindowThreshold, kNnrThreshold));

  MediaCapabilitiesTestContext context;
  const auto* kDecodingConfig = CreateDecodingConfig();
  const media::mojom::blink::PredictionFeatures kFeatures = CreateFeatures();
  const Vector<media::learning::FeatureValue> kFeaturesML = CreateFeaturesML();

  // ML is enabled, but DB should still be called for power efficiency (false).
  // Its smoothness value (true) should be ignored in favor of ML predictions.
  // Both ML services should be called for prediction. In both cases we exceed
  // the threshold, such that smooth=false.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ true, /*efficient*/ false));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kBadWindowThreshold + 0.5));
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kNnrThreshold + 0.5));
  MediaCapabilitiesInfo* info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_FALSE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());
  context.VerifyAndClearMockExpectations();

  // Make another call to DecodingInfo with one "bad window" prediction
  // indicating smooth=false, while nnr prediction indicates smooth=true. Verify
  // resulting info predicts false, as the logic should OR the false signals.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ true, /*efficient*/ false));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kBadWindowThreshold + 0.5));
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kNnrThreshold / 2));
  info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_FALSE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());
  context.VerifyAndClearMockExpectations();

  // Same as above, but invert predictions from ML services. Outcome should
  // still be smooth=false (logic is ORed).
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ true, /*efficient*/ false));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kBadWindowThreshold / 2));
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kNnrThreshold + 0.5));
  info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_FALSE(info->smooth());
  EXPECT_FALSE(info->powerEfficient());
  context.VerifyAndClearMockExpectations();

  // This time both ML services agree smooth=true while DB predicts
  // smooth=false. Expect info->smooth() = true, as only ML predictions matter
  // when ML experiment enabled.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ false, /*efficient*/ true));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kBadWindowThreshold / 2));
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kNnrThreshold / 2));
  info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_TRUE(info->smooth());
  EXPECT_TRUE(info->powerEfficient());
  context.VerifyAndClearMockExpectations();

  // Same as above, but with ML services predicting exactly their respective
  // thresholds. Still expect info->smooth() = true - matching the threshold is
  // still considered smooth.
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(DbCallback(kFeatures, /*smooth*/ false, /*efficient*/ true));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kBadWindowThreshold / 2));
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _))
      .WillOnce(MlCallback(kFeaturesML, kNnrThreshold / 2));
  info = DecodingInfo(kDecodingConfig, &context);
  EXPECT_TRUE(info->smooth());
  EXPECT_TRUE(info->powerEfficient());
  context.VerifyAndClearMockExpectations();
}

// Simulate a call to DecodingInfo with smoothness predictions arriving in the
// specified |callback_order|. Ensure that promise resolves correctly only after
// all callbacks have arrived.
void RunCallbackPermutationTest(std::vector<PredictionType> callback_order) {
  // Enable ML predictions with thresholds.
  const double kBadWindowThreshold = 2;
  const double kNnrThreshold = 3;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      media::kMediaLearningSmoothnessExperiment,
      MakeMlParams(kBadWindowThreshold, kNnrThreshold));

  MediaCapabilitiesTestContext context;
  const auto* kDecodingConfig = CreateDecodingConfig();

  // DB and both ML services should be called. Save their callbacks.
  CallbackSaver cb_saver;
  EXPECT_CALL(*context.GetPerfHistoryService(), GetPerfInfo(_, _))
      .WillOnce(Invoke(&cb_saver, &CallbackSaver::SavePerfHistoryCallback));
  EXPECT_CALL(*context.GetBadWindowService(), PredictDistribution(_, _))
      .WillOnce(Invoke(&cb_saver, &CallbackSaver::SaveBadWindowCallback));
  EXPECT_CALL(*context.GetNnrService(), PredictDistribution(_, _))
      .WillOnce(Invoke(&cb_saver, &CallbackSaver::SaveNnrCallback));

  // Call decodingInfo() to kick off the calls to prediction services.
  ScriptPromise promise = context.GetMediaCapabilities()->decodingInfo(
      context.GetScriptState(), kDecodingConfig, context.GetExceptionState());
  ScriptPromiseTester tester(context.GetScriptState(), promise);

  // Callbacks should all be saved after mojo's pending tasks have run.
  test::RunPendingTasks();
  ASSERT_TRUE(cb_saver.perf_history_cb() && cb_saver.bad_window_cb() &&
              cb_saver.nnr_cb());

  // Complete callbacks in whatever order.
  for (size_t i = 0; i < callback_order.size(); ++i) {
    switch (callback_order[i]) {
      case PredictionType::kDB:
        std::move(cb_saver.perf_history_cb()).Run(true, true);
        break;
      case PredictionType::kBadWindow:
        std::move(cb_saver.bad_window_cb())
            .Run(MakeHistogram(kBadWindowThreshold - 0.25));
        break;
      case PredictionType::kNnr:
        std::move(cb_saver.nnr_cb()).Run(MakeHistogram(kNnrThreshold + 0.5));
        break;
    }

    // Give mojo callbacks a chance to run.
    test::RunPendingTasks();

    // Promise should only be resolved once the final callback has run.
    if (i < callback_order.size() - 1) {
      ASSERT_FALSE(tester.IsFulfilled());
    } else {
      ASSERT_TRUE(tester.IsFulfilled());
    }
  }

  ASSERT_FALSE(tester.IsRejected()) << " Cant get info from rejected promise.";
  MediaCapabilitiesInfo* info =
      NativeValueTraits<MediaCapabilitiesInfo>::NativeValue(
          context.GetIsolate(), tester.Value().V8Value(),
          context.GetExceptionState());

  // Smooth=false because NNR prediction exceeds threshold.
  EXPECT_FALSE(info->smooth());
  // DB predicted power_efficient = true.
  EXPECT_TRUE(info->powerEfficient());
}

// Test that decodingInfo() behaves correctly for all orderings/timings of the
// underlying prediction services.
TEST(MediaCapabilitiesTests, PredictionCallbackPermutations) {
  std::vector<PredictionType> callback_order(
      {PredictionType::kDB, PredictionType::kBadWindow, PredictionType::kNnr});
  do {
    RunCallbackPermutationTest(callback_order);
  } while (std::next_permutation(callback_order.begin(), callback_order.end()));
}

}  // namespace blink
