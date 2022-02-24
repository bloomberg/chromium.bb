// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_handler.h"

#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_test_util.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_test_helpers.h"
#include "chrome/browser/media/router/providers/cast/dual_media_sink_service.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/ui/global_media_controls/test_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/cast_channel/cast_socket.h"
#include "components/cast_channel/cast_socket_service.h"
#include "components/cast_channel/cast_test_util.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/common/route_request_result.h"
#include "components/media_router/common/test/test_helper.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using DiscoveryDevice = chrome_browser_media::proto::DiscoveryDevice;
using access_code_cast::mojom::AddSinkResultCode;
using MockAddSinkCallback =
    base::MockCallback<media_router::AccessCodeCastHandler::AddSinkCallback>;
using MockCastToSinkCallback =
    base::MockCallback<media_router::AccessCodeCastHandler::CastToSinkCallback>;
using media_router::mojom::RouteRequestResultCode;
using ::testing::_;
using ::testing::Exactly;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

// TODO(b/213324920): Remove WebUI from the media_router namespace after
// expiration module has been completed.
namespace media_router {

namespace {
class MockPage : public access_code_cast::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<access_code_cast::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }
  mojo::Receiver<access_code_cast::mojom::Page> receiver_{this};
};

}  // namespace

class MockAccessCodeCastSinkService : public AccessCodeCastSinkService {
 public:
  MockAccessCodeCastSinkService(
      Profile* profile,
      MediaRouter* media_router,
      CastMediaSinkServiceImpl* cast_media_sink_service_impl)
      : AccessCodeCastSinkService(profile,
                                  media_router,
                                  cast_media_sink_service_impl) {}
  ~MockAccessCodeCastSinkService() override = default;

  MOCK_METHOD(void,
              AddSinkToMediaRouter,
              (const MediaSinkInternal& sink,
               base::OnceCallback<void(bool)> callback),
              (override));
};

class AccessCodeCastHandlerTest : public ChromeRenderViewHostTestHarness {
 protected:
  AccessCodeCastHandlerTest()
      : mock_time_task_runner_(new base::TestMockTimeTaskRunner()),
        mock_cast_socket_service_(
            new cast_channel::MockCastSocketService(mock_time_task_runner_)),
        message_handler_(mock_cast_socket_service_.get()),
        mock_cast_media_sink_service_impl_(
            new MockCastMediaSinkServiceImpl(mock_sink_discovered_cb_.Get(),
                                             mock_cast_socket_service_.get(),
                                             discovery_network_monitor_.get(),
                                             &dual_media_sink_service_)) {
    mock_cast_socket_service_->SetTaskRunnerForTest(mock_time_task_runner_);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager()->CreateTestingProfile("foo_email");

    presentation_manager_ =
        std::make_unique<NiceMock<MockWebContentsPresentationManager>>();
    WebContentsPresentationManager::SetTestInstance(
        presentation_manager_.get());

    InitializeMockMediaRouter();

    cast_sink_1_ = CreateCastSink(1);
    cast_sink_2_ = CreateCastSink(2);

    CreateSessionServiceTabHelper(web_contents());

    CreateHandler({MediaCastMode::DESKTOP_MIRROR});
  }

  void TearDown() override {
    clear_screen_capture_allowed_for_testing();
    handler_.reset();
    access_code_cast_sink_service_.reset();
    page_.reset();
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
    WebContentsPresentationManager::SetTestInstance(nullptr);
    task_environment()->RunUntilIdle();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void CreateHandler(const CastModeSet& cast_modes,
                     std::unique_ptr<StartPresentationContext>
                         start_presentation_context = nullptr) {
    page_ = std::make_unique<StrictMock<MockPage>>();

    access_code_cast_sink_service_ =
        std::make_unique<MockAccessCodeCastSinkService>(
            profile_, router_, mock_cast_media_sink_service_impl_.get());

    handler_ = std::make_unique<AccessCodeCastHandler>(
        mojo::PendingReceiver<access_code_cast::mojom::PageHandler>(),
        page_->BindAndGetRemote(), profile_, router_, cast_modes,
        web_contents(), std::move(start_presentation_context),
        access_code_cast_sink_service_.get());
  }

  AccessCodeCastHandler* handler() { return handler_.get(); }

  TestingProfileManager* profile_manager() { return profile_manager_.get(); }

  MockWebContentsPresentationManager* presentation_manager() {
    return presentation_manager_.get();
  }

  MockMediaRouter* router() { return router_; }

  MockAccessCodeCastSinkService* access_service() {
    return access_code_cast_sink_service_.get();
  }

  void set_expected_cast_result(RouteRequestResult::ResultCode code) {
    result_code_ = code;
  }

  void StartDesktopMirroring(const MediaSource& source,
                             MockCastToSinkCallback& mock_callback) {
    StartMirroring({MediaCastMode::DESKTOP_MIRROR}, source, base::Seconds(120),
                   mock_callback);
  }

  void StartTabMirroring(const MediaSource& source,
                         MockCastToSinkCallback& mock_callback) {
    StartMirroring({MediaCastMode::PRESENTATION, MediaCastMode::TAB_MIRROR},
                   source, base::Seconds(60), mock_callback);
  }

  void StartMirroring(const CastModeSet& cast_modes,
                      const MediaSource& source,
                      base::TimeDelta timeout,
                      MockCastToSinkCallback& mock_callback) {
    CreateHandler(cast_modes);
    set_screen_capture_allowed_for_testing(true);

    for (MediaSinksObserver* sinks_observer : media_sinks_observers_) {
      sinks_observer->OnSinksUpdated({cast_sink_1().sink()},
                                     std::vector<url::Origin>());
    }
    handler()->set_sink_id_for_testing(cast_sink_1().sink().id());

    EXPECT_CALL(*router(),
                CreateRouteInternal(source.id(), cast_sink_1().sink().id(), _,
                                    web_contents(), _, timeout, false));

    handler()->CastToSink(mock_callback.Get());
  }

  void StartPresentation(
      const content::PresentationRequest& request,
      std::unique_ptr<StartPresentationContext> start_presentation_context,
      MockCastToSinkCallback& mock_callback) {
    CreateHandler({MediaCastMode::PRESENTATION, MediaCastMode::TAB_MIRROR},
                  std::move(start_presentation_context));

    for (MediaSinksObserver* sinks_observer : media_sinks_observers_) {
      sinks_observer->OnSinksUpdated({cast_sink_1().sink()},
                                     {request.frame_origin});
    }
    handler()->set_sink_id_for_testing(cast_sink_1().sink().id());

    auto source =
        MediaSource::ForPresentationUrl(*(request.presentation_urls.begin()));

    EXPECT_CALL(*router(),
                CreateRouteInternal(source.id(), cast_sink_1().sink().id(),
                                    request.frame_origin, web_contents(), _,
                                    base::Seconds(20), false));
    handler()->CastToSink(mock_callback.Get());
  }

  std::unique_ptr<StartPresentationContext> CreateStartPresentationContext(
      content::PresentationRequest presentation_request,
      StartPresentationContext::PresentationConnectionCallback success_cb =
          base::DoNothing(),
      StartPresentationContext::PresentationConnectionErrorCallback error_cb =
          base::DoNothing()) {
    return std::make_unique<StartPresentationContext>(
        presentation_request, std::move(success_cb), std::move(error_cb));
  }

  const MediaSinkInternal& cast_sink_1() { return cast_sink_1_; }
  const MediaSinkInternal& cast_sink_2() { return cast_sink_2_; }

 private:
  void InitializeMockMediaRouter() {
    router_ = static_cast<MockMediaRouter*>(
        MediaRouterFactory::GetInstance()->SetTestingFactoryAndUse(
            web_contents()->GetBrowserContext(),
            base::BindRepeating(&MockMediaRouter::Create)));
    logger_ = std::make_unique<LoggerImpl>();

    ON_CALL(*router_, GetLogger()).WillByDefault(Return(logger_.get()));
    // Store sink observers so that they can be notified in tests.
    ON_CALL(*router_, RegisterMediaSinksObserver(_))
        .WillByDefault([this](MediaSinksObserver* observer) {
          media_sinks_observers_.push_back(observer);
          return true;
        });
    // Remove sink observers as appropriate (destructing handlers will cause
    // this to occur).
    ON_CALL(*router_, UnregisterMediaSinksObserver(_))
        .WillByDefault([this](MediaSinksObserver* observer) {
          auto it = std::find(media_sinks_observers_.begin(),
                              media_sinks_observers_.end(), observer);
          if (it != media_sinks_observers_.end()) {
            media_sinks_observers_.erase(it);
          }
        });

    ON_CALL(*router_, GetCurrentRoutes())
        .WillByDefault(Return(std::vector<MediaRoute>()));

    // Handler so MockMediaRouter will respond to requests to create a route.
    // Will construct a RouteRequestResult based on the set result code and
    // then call the handler's callback, which should call the page's callback.
    ON_CALL(*router_, CreateRouteInternal(_, _, _, _, _, _, _))
        .WillByDefault([this](const MediaSource::Id& source_id,
                              const MediaSink::Id& sink_id,
                              const url::Origin& origin,
                              content::WebContents* web_contents,
                              MediaRouteResponseCallback& callback,
                              base::TimeDelta timeout, bool incognito) {
          std::unique_ptr<RouteRequestResult> result;
          if (result_code_ == RouteRequestResult::ResultCode::OK) {
            MediaSource source(source_id);
            MediaRoute route;
            route.set_media_route_id(source_id + "->" + sink_id);
            route.set_media_source(source);
            route.set_media_sink_id(sink_id);
            result = RouteRequestResult::FromSuccess(route, std::string());
          } else {
            result = RouteRequestResult::FromError(std::string(), result_code_);
          }
          std::move(callback).Run(nullptr, *result);
        });
  }

  scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner_;

  raw_ptr<MockMediaRouter> router_;
  std::unique_ptr<LoggerImpl> logger_;

  static std::vector<DiscoveryNetworkInfo> GetFakeNetworkInfo() {
    return {
        DiscoveryNetworkInfo{std::string("enp0s2"), std::string("ethernet1")}};
    ;
  }

  std::unique_ptr<DiscoveryNetworkMonitor> discovery_network_monitor_ =
      DiscoveryNetworkMonitor::CreateInstanceForTest(&GetFakeNetworkInfo);

  std::unique_ptr<AccessCodeCastHandler> handler_;
  std::unique_ptr<MockAccessCodeCastSinkService> access_code_cast_sink_service_;

  base::MockCallback<OnSinksDiscoveredCallback> mock_sink_discovered_cb_;

  TestMediaSinkService dual_media_sink_service_;
  std::unique_ptr<cast_channel::MockCastSocketService>
      mock_cast_socket_service_;

  NiceMock<cast_channel::MockCastMessageHandler> message_handler_;
  std::unique_ptr<StrictMock<MockPage>> page_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<MockCastMediaSinkServiceImpl>
      mock_cast_media_sink_service_impl_;
  std::unique_ptr<MockWebContentsPresentationManager> presentation_manager_;
  std::vector<MediaSinksObserver*> media_sinks_observers_;
  RouteRequestResult::ResultCode result_code_ =
      RouteRequestResult::ResultCode::OK;
  MediaSinkInternal cast_sink_1_;
  MediaSinkInternal cast_sink_2_;
};

TEST_F(AccessCodeCastHandlerTest, DiscoveryDeviceMissingWithOk) {
  // Test to ensure that the add_sink_callback returns an EMPTY_RESPONSE if the
  // the device is missing. Since |OnAccessCodeValidated| is a public method --
  // we must check the case of an empty |discovery_device| with an OK result
  // code.
  MockAddSinkCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(AddSinkResultCode::EMPTY_RESPONSE));
  handler()->SetSinkCallbackForTesting(mock_callback.Get());
  handler()->OnAccessCodeValidated(absl::nullopt, AddSinkResultCode::OK);
}

TEST_F(AccessCodeCastHandlerTest, ValidDiscoveryDeviceAndCode) {
  // If discovery device is present, formatted correctly, and code is OK, then
  // callback should be OK.
  DiscoveryDevice discovery_device_proto =
      media_router::BuildDiscoveryDeviceProto();
  discovery_device_proto.set_id("id1");

  EXPECT_CALL(*access_service(), AddSinkToMediaRouter(_, _));
  handler()->OnAccessCodeValidated(discovery_device_proto,
                                   AddSinkResultCode::OK);

  // Validate that the sink id of the discovered device is stored for later
  // casting.
  EXPECT_EQ(cast_sink_1().sink().id(), handler()->sink_id_);
}

TEST_F(AccessCodeCastHandlerTest, OnChannelOpened) {
  // OnChannelOpened should only trigger the callback if the channel opening
  // failed.
  MockAddSinkCallback mock_callback_true;
  handler()->SetSinkCallbackForTesting(mock_callback_true.Get());

  EXPECT_CALL(mock_callback_true, Run(AddSinkResultCode::CHANNEL_OPEN_ERROR))
      .Times(0);
  handler()->OnChannelOpenedResult(true);

  MockAddSinkCallback mock_callback_false;
  handler()->SetSinkCallbackForTesting(mock_callback_false.Get());

  EXPECT_CALL(mock_callback_false, Run(AddSinkResultCode::CHANNEL_OPEN_ERROR));
  handler()->OnChannelOpenedResult(false);
}

TEST_F(AccessCodeCastHandlerTest, InvalidDiscoveryDevice) {
  // If discovery device is present, but formatted incorrectly, and code is OK,
  // then callback should be SINK_CREATION_ERROR.
  MockAddSinkCallback mock_callback;

  // Create discovery_device with an invalid port
  DiscoveryDevice discovery_device_proto =
      media_router::BuildDiscoveryDeviceProto("foo_display_name", "1234",
                                              "```````23489:1238:1239");

  EXPECT_CALL(mock_callback, Run(AddSinkResultCode::SINK_CREATION_ERROR));
  handler()->SetSinkCallbackForTesting(mock_callback.Get());
  handler()->OnAccessCodeValidated(discovery_device_proto,
                                   AddSinkResultCode::OK);
}

TEST_F(AccessCodeCastHandlerTest, NonOKResultCode) {
  // Check to see that any result code that isn't OK will return that error.
  MockAddSinkCallback mock_callback;

  EXPECT_CALL(mock_callback, Run(AddSinkResultCode::AUTH_ERROR));
  handler()->SetSinkCallbackForTesting(mock_callback.Get());
  handler()->OnAccessCodeValidated(absl::nullopt,
                                   AddSinkResultCode::AUTH_ERROR);
}

// Demonstrates that if the expected device is added to the media router,
// the page is notified of success.
TEST_F(AccessCodeCastHandlerTest, DiscoveredDeviceAdded) {
  MockAddSinkCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(AddSinkResultCode::OK));
  handler()->SetSinkCallbackForTesting(mock_callback.Get());

  MediaSinkWithCastModes sink_with_cast_modes(cast_sink_1().sink());
  sink_with_cast_modes.cast_modes = {MediaCastMode::DESKTOP_MIRROR};

  handler()->set_sink_id_for_testing(cast_sink_1().sink().id());
  handler()->OnResultsUpdated({sink_with_cast_modes});
}

// Demonstrates that if handler is notified about a device other than the
// discovered device the page is not notified.
TEST_F(AccessCodeCastHandlerTest, OtherDevicesIgnored) {
  MockAddSinkCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(_)).Times(Exactly(0));
  handler()->SetSinkCallbackForTesting(mock_callback.Get());

  handler()->set_sink_id_for_testing(cast_sink_1().sink().id());

  MediaSinkWithCastModes sink_with_cast_modes(cast_sink_2().sink());
  sink_with_cast_modes.cast_modes = {MediaCastMode::DESKTOP_MIRROR};

  handler()->OnResultsUpdated({sink_with_cast_modes});
}

// Demonstrates that desktop mirroring attempts call media router with the
// correct parameters, and that success is communicated to the dialog box.
TEST_F(AccessCodeCastHandlerTest, DesktopMirroring) {
  set_expected_cast_result(RouteRequestResult::ResultCode::OK);
  MockCastToSinkCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(RouteRequestResultCode::OK));
  StartDesktopMirroring(MediaSource::ForUnchosenDesktop(), mock_callback);
}

// Demonstrates that if casting does not start successfully that the error
// code is communicated to the dialog.
TEST_F(AccessCodeCastHandlerTest, DesktopMirroringError) {
  set_expected_cast_result(RouteRequestResult::ResultCode::ROUTE_NOT_FOUND);
  MockCastToSinkCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(RouteRequestResultCode::ROUTE_NOT_FOUND));
  StartDesktopMirroring(MediaSource::ForUnchosenDesktop(), mock_callback);
}

// Demonstrates that tab mirroring attempts call media router with the
// correct parameters, and that success is communicated to the dialog box.
TEST_F(AccessCodeCastHandlerTest, TabMirroring) {
  set_expected_cast_result(RouteRequestResult::ResultCode::OK);
  MockCastToSinkCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(RouteRequestResultCode::OK));
  MediaSource media_source = MediaSource::ForTab(
      sessions::SessionTabHelper::IdForTab(web_contents()).id());
  StartTabMirroring(media_source, mock_callback);
}

// Demonstrates that if casting does not start successfully that the error
// code is communicated to the dialog.
TEST_F(AccessCodeCastHandlerTest, TabMirroringError) {
  set_expected_cast_result(RouteRequestResult::ResultCode::INVALID_ORIGIN);
  MockCastToSinkCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(RouteRequestResultCode::INVALID_ORIGIN));
  MediaSource media_source = MediaSource::ForTab(
      sessions::SessionTabHelper::IdForTab(web_contents()).id());
  StartTabMirroring(media_source, mock_callback);
}

// Demonstrates that if a default presentation source is available,
// presentation casting will begin instead of tab casting.
TEST_F(AccessCodeCastHandlerTest, DefaultPresentation) {
  set_expected_cast_result(RouteRequestResult::ResultCode::OK);
  MockCastToSinkCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(RouteRequestResultCode::OK));

  content::PresentationRequest presentation_request(
      {0, 0}, {GURL("https://defaultpresentation.com")},
      url::Origin::Create(GURL("https://default.fakeurl")));
  presentation_manager()->SetDefaultPresentationRequest(presentation_request);
  StartPresentation(presentation_request, nullptr, mock_callback);
}

// Demonstrates that if a presentation casting does not start successfully
// that the error is propagated to the dialog.
TEST_F(AccessCodeCastHandlerTest, DefaultPresentationError) {
  set_expected_cast_result(RouteRequestResult::ResultCode::INVALID_ORIGIN);
  MockCastToSinkCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(RouteRequestResultCode::INVALID_ORIGIN));

  content::PresentationRequest presentation_request(
      {0, 0}, {GURL("https://defaultpresentation.com")},
      url::Origin::Create(GURL("https://default.fakeurl")));
  presentation_manager()->SetDefaultPresentationRequest(presentation_request);
  StartPresentation(presentation_request, nullptr, mock_callback);
}

// Demonstrates that if a StartPresentationContext is supplied to the handler,
// it will be used to start casting in preference to the default request and
// tab mirroring.
TEST_F(AccessCodeCastHandlerTest, StartPresentationContext) {
  set_expected_cast_result(RouteRequestResult::ResultCode::OK);
  MockCastToSinkCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(RouteRequestResultCode::OK));

  // Add a default presentation request. This should be ignored.
  content::PresentationRequest ignored_request(
      {0, 0}, {GURL("https://defaultpresentation.com")},
      url::Origin::Create(GURL("https://default.fakeurl")));
  presentation_manager()->SetDefaultPresentationRequest(ignored_request);

  content::PresentationRequest presentation_request(
      {0, 0}, {GURL("https://startpresentrequest.com")},
      url::Origin::Create(GURL("https://start.fakeurl")));

  auto start_presentation_context =
      CreateStartPresentationContext(presentation_request);

  StartPresentation(presentation_request, std::move(start_presentation_context),
                    mock_callback);
}

// Starting a casting session on a sink that has an existing route will cause
// the current route to be terminate before the new route is created.
TEST_F(AccessCodeCastHandlerTest, TerminateExistingRoute) {
  MediaSource existing_source = MediaSource::ForTab(
      sessions::SessionTabHelper::IdForTab(web_contents()).id());
  MediaRoute::Id route_id = MediaRoute::GetMediaRouteId(
      "presentation", cast_sink_1().id(), existing_source);
  MediaRoute existing_route(route_id, existing_source, cast_sink_1().id(),
                            "TerminateExistingRoute", false);
  EXPECT_CALL(*router(), GetCurrentRoutes())
      .WillOnce(Return(std::vector<MediaRoute>{existing_route}));
  EXPECT_CALL(*router(), TerminateRoute(route_id));

  set_expected_cast_result(RouteRequestResult::ResultCode::OK);
  MockCastToSinkCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(RouteRequestResultCode::OK));
  StartDesktopMirroring(MediaSource::ForUnchosenDesktop(), mock_callback);
}

// Starting a casting session when there are routes that exist for other sinks.
// Demonstrate that those routes aren't terminated.
TEST_F(AccessCodeCastHandlerTest, IgnoreOtherRoutes) {
  MediaSource existing_source = MediaSource::ForTab(
      sessions::SessionTabHelper::IdForTab(web_contents()).id());
  MediaRoute::Id route_id = MediaRoute::GetMediaRouteId(
      "presentation", cast_sink_2().id(), existing_source);
  MediaRoute existing_route(route_id, existing_source, cast_sink_2().id(),
                            "TerminateExistingRoute", false);
  EXPECT_CALL(*router(), GetCurrentRoutes())
      .WillOnce(Return(std::vector<MediaRoute>{existing_route}));
  EXPECT_CALL(*router(), TerminateRoute(_)).Times(0);

  set_expected_cast_result(RouteRequestResult::ResultCode::OK);
  MockCastToSinkCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(RouteRequestResultCode::OK));
  StartDesktopMirroring(MediaSource::ForUnchosenDesktop(), mock_callback);
}

}  // namespace media_router
