// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/new_tab_page/promos/promo_data.h"
#include "chrome/browser/new_tab_page/promos/promo_service.h"
#include "chrome/browser/new_tab_page/promos/promo_service_factory.h"
#include "chrome/browser/new_tab_page/promos/promo_service_observer.h"
#include "chrome/browser/search/background/ntp_background_data.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_observer.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_helper.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/theme_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/search_provider_logos/logo_common.h"
#include "components/search_provider_logos/logo_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_palette.h"
#include "url/gurl.h"

namespace {

using testing::_;
using testing::DoAll;
using testing::ElementsAre;
using testing::Optional;
using testing::SaveArg;

class MockPage : public new_tab_page::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<new_tab_page::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD1(SetTheme, void(new_tab_page::mojom::ThemePtr));
  MOCK_METHOD2(SetDisabledModules, void(bool, const std::vector<std::string>&));

  mojo::Receiver<new_tab_page::mojom::Page> receiver_{this};
};

class MockLogoService : public search_provider_logos::LogoService {
 public:
  MOCK_METHOD2(GetLogo, void(search_provider_logos::LogoCallbacks, bool));
  MOCK_METHOD1(GetLogo, void(search_provider_logos::LogoObserver*));
};

class MockThemeProvider : public ui::ThemeProvider {
 public:
  MOCK_CONST_METHOD1(GetImageSkiaNamed, gfx::ImageSkia*(int));
  MOCK_CONST_METHOD1(GetColor, SkColor(int));
  MOCK_CONST_METHOD1(GetTint, color_utils::HSL(int));
  MOCK_CONST_METHOD1(GetDisplayProperty, int(int));
  MOCK_CONST_METHOD0(ShouldUseNativeFrame, bool());
  MOCK_CONST_METHOD1(HasCustomImage, bool(int));
  MOCK_CONST_METHOD1(HasCustomColor, bool(int));
  MOCK_CONST_METHOD2(GetRawData,
                     base::RefCountedMemory*(int, ui::ResourceScaleFactor));
};

class MockNtpCustomBackgroundService : public NtpCustomBackgroundService {
 public:
  explicit MockNtpCustomBackgroundService(Profile* profile)
      : NtpCustomBackgroundService(profile) {}
  MOCK_METHOD0(RefreshBackgroundIfNeeded, void());
  MOCK_METHOD0(GetCustomBackground, absl::optional<CustomBackground>());
  MOCK_METHOD1(AddObserver, void(NtpCustomBackgroundServiceObserver*));
};

class MockThemeService : public ThemeService {
 public:
  MockThemeService() : ThemeService(nullptr, theme_helper_) {}
  MOCK_CONST_METHOD0(GetThemeID, std::string());
  MOCK_CONST_METHOD0(UsingDefaultTheme, bool());
  MOCK_METHOD1(AddObserver, void(ThemeServiceObserver*));

 private:
  ThemeHelper theme_helper_;
};

class MockPromoService : public PromoService {
 public:
  MockPromoService() : PromoService(nullptr, nullptr) {}
  MOCK_METHOD(const absl::optional<PromoData>&,
              promo_data,
              (),
              (const, override));
  MOCK_METHOD(void, AddObserver, (PromoServiceObserver*), (override));
  MOCK_METHOD(void, Refresh, (), (override));
};

std::unique_ptr<TestingProfile> MakeTestingProfile(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      PromoServiceFactory::GetInstance(),
      base::BindRepeating([](content::BrowserContext* context)
                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<testing::NiceMock<MockPromoService>>();
      }));
  profile_builder.SetSharedURLLoaderFactory(url_loader_factory);
  auto profile = profile_builder.Build();
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile.get(),
      base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
  return profile;
}

}  // namespace

class NewTabPageHandlerTest : public testing::Test {
 public:
  NewTabPageHandlerTest()
      : profile_(MakeTestingProfile(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_))),
        mock_ntp_custom_background_service_(profile_.get()),
        mock_promo_service_(*static_cast<MockPromoService*>(
            PromoServiceFactory::GetForProfile(profile_.get()))),
        web_contents_(factory_.CreateWebContents(profile_.get())) {}

  ~NewTabPageHandlerTest() override = default;

  void SetUp() override {
    EXPECT_CALL(mock_theme_service_, AddObserver)
        .Times(1)
        .WillOnce(testing::SaveArg<0>(&theme_service_observer_));
    EXPECT_CALL(mock_ntp_custom_background_service_, AddObserver)
        .Times(1)
        .WillOnce(
            testing::SaveArg<0>(&ntp_custom_background_service_observer_));
    EXPECT_CALL(mock_promo_service_, AddObserver)
        .Times(1)
        .WillOnce(testing::SaveArg<0>(&promo_service_observer_));
    EXPECT_CALL(mock_page_, SetTheme).Times(1);
    EXPECT_CALL(mock_ntp_custom_background_service_, RefreshBackgroundIfNeeded)
        .Times(1);
    handler_ = std::make_unique<NewTabPageHandler>(
        mojo::PendingReceiver<new_tab_page::mojom::PageHandler>(),
        mock_page_.BindAndGetRemote(), profile_.get(),
        &mock_ntp_custom_background_service_, &mock_theme_service_,
        &mock_logo_service_, &mock_theme_provider_, web_contents_,
        base::Time::Now());
    mock_page_.FlushForTesting();
    EXPECT_EQ(handler_.get(), theme_service_observer_);
    EXPECT_EQ(handler_.get(), ntp_custom_background_service_observer_);
    testing::Mock::VerifyAndClearExpectations(&mock_page_);
    testing::Mock::VerifyAndClearExpectations(
        &mock_ntp_custom_background_service_);
  }

  new_tab_page::mojom::DoodlePtr GetDoodle(
      const search_provider_logos::EncodedLogo& logo) {
    search_provider_logos::EncodedLogoCallback on_cached_encoded_logo_available;
    EXPECT_CALL(mock_logo_service_, GetLogo(testing::_, testing::_))
        .Times(1)
        .WillOnce(
            testing::Invoke([&on_cached_encoded_logo_available](
                                search_provider_logos::LogoCallbacks callbacks,
                                bool for_webui_ntp) {
              on_cached_encoded_logo_available =
                  std::move(callbacks.on_cached_encoded_logo_available);
            }));
    base::MockCallback<NewTabPageHandler::GetDoodleCallback> callback;
    new_tab_page::mojom::DoodlePtr doodle;
    EXPECT_CALL(callback, Run(testing::_))
        .Times(1)
        .WillOnce(
            testing::Invoke([&doodle](new_tab_page::mojom::DoodlePtr arg) {
              doodle = std::move(arg);
            }));
    handler_->GetDoodle(callback.Get());

    std::move(on_cached_encoded_logo_available)
        .Run(search_provider_logos::LogoCallbackReason::DETERMINED,
             absl::make_optional(logo));

    return doodle;
  }

 protected:
  testing::NiceMock<MockPage> mock_page_;
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfile> profile_;
  testing::NiceMock<MockNtpCustomBackgroundService>
      mock_ntp_custom_background_service_;
  testing::NiceMock<MockThemeService> mock_theme_service_;
  MockLogoService mock_logo_service_;
  testing::NiceMock<MockThemeProvider> mock_theme_provider_;
  MockPromoService& mock_promo_service_;
  content::TestWebContentsFactory factory_;
  raw_ptr<content::WebContents> web_contents_;  // Weak. Owned by factory_.
  base::HistogramTester histogram_tester_;
  std::unique_ptr<NewTabPageHandler> handler_;
  ThemeServiceObserver* theme_service_observer_;
  NtpCustomBackgroundServiceObserver* ntp_custom_background_service_observer_;
  PromoServiceObserver* promo_service_observer_;
};

TEST_F(NewTabPageHandlerTest, SetTheme) {
  new_tab_page::mojom::ThemePtr theme;
  EXPECT_CALL(mock_page_, SetTheme)
      .Times(1)
      .WillOnce(testing::Invoke([&theme](new_tab_page::mojom::ThemePtr arg) {
        theme = std::move(arg);
      }));
  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(testing::Return(absl::optional<CustomBackground>()));
  ON_CALL(mock_theme_provider_, GetColor(ThemeProperties::COLOR_NTP_BACKGROUND))
      .WillByDefault(testing::Return(SkColorSetRGB(0, 0, 1)));
  ON_CALL(mock_theme_provider_, GetColor(ThemeProperties::COLOR_NTP_TEXT))
      .WillByDefault(testing::Return(SkColorSetRGB(0, 0, 2)));
  ON_CALL(mock_theme_service_, UsingDefaultTheme())
      .WillByDefault(testing::Return(false));
  ON_CALL(mock_theme_provider_,
          GetDisplayProperty(ThemeProperties::NTP_LOGO_ALTERNATE))
      .WillByDefault(testing::Return(1));
  ON_CALL(mock_theme_provider_, GetColor(ThemeProperties::COLOR_NTP_LOGO))
      .WillByDefault(testing::Return(SkColorSetRGB(0, 0, 3)));
  ON_CALL(mock_theme_service_, GetThemeID())
      .WillByDefault(testing::Return("bar"));
  ON_CALL(mock_theme_provider_,
          GetDisplayProperty(ThemeProperties::NTP_BACKGROUND_TILING))
      .WillByDefault(testing::Return(ThemeProperties::REPEAT_X));
  ON_CALL(mock_theme_provider_,
          GetDisplayProperty(ThemeProperties::NTP_BACKGROUND_ALIGNMENT))
      .WillByDefault(testing::Return(ThemeProperties::ALIGN_TOP));
  ON_CALL(mock_theme_provider_, HasCustomImage(IDR_THEME_NTP_ATTRIBUTION))
      .WillByDefault(testing::Return(true));
  ON_CALL(mock_theme_provider_, HasCustomImage(IDR_THEME_NTP_BACKGROUND))
      .WillByDefault(testing::Return(true));
  ON_CALL(mock_theme_provider_, GetColor(ThemeProperties::COLOR_NTP_SHORTCUT))
      .WillByDefault(testing::Return(SkColorSetRGB(0, 0, 4)));
  ON_CALL(mock_theme_provider_,
          GetColor(ThemeProperties::COLOR_OMNIBOX_BACKGROUND))
      .WillByDefault(testing::Return(SkColorSetRGB(0, 0, 5)));
  ON_CALL(mock_theme_provider_,
          GetColor(ThemeProperties::COLOR_OMNIBOX_RESULTS_ICON))
      .WillByDefault(testing::Return(SkColorSetRGB(0, 0, 6)));
  ON_CALL(mock_theme_provider_,
          GetColor(ThemeProperties::COLOR_OMNIBOX_RESULTS_ICON_SELECTED))
      .WillByDefault(testing::Return(SkColorSetRGB(0, 0, 7)));
  ON_CALL(mock_theme_provider_,
          GetColor(ThemeProperties::COLOR_OMNIBOX_TEXT_DIMMED))
      .WillByDefault(testing::Return(SkColorSetRGB(0, 0, 8)));
  ON_CALL(mock_theme_provider_,
          GetColor(ThemeProperties::COLOR_OMNIBOX_RESULTS_BG))
      .WillByDefault(testing::Return(SkColorSetRGB(0, 0, 9)));
  ON_CALL(mock_theme_provider_,
          GetColor(ThemeProperties::COLOR_OMNIBOX_RESULTS_BG_HOVERED))
      .WillByDefault(testing::Return(SkColorSetRGB(0, 0, 10)));
  ON_CALL(mock_theme_provider_,
          GetColor(ThemeProperties::COLOR_OMNIBOX_RESULTS_BG_SELECTED))
      .WillByDefault(testing::Return(SkColorSetRGB(0, 0, 11)));
  ON_CALL(mock_theme_provider_,
          GetColor(ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED))
      .WillByDefault(testing::Return(SkColorSetRGB(0, 0, 12)));
  ON_CALL(mock_theme_provider_,
          GetColor(ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED_SELECTED))
      .WillByDefault(testing::Return(SkColorSetRGB(0, 0, 13)));
  ON_CALL(mock_theme_provider_, GetColor(ThemeProperties::COLOR_OMNIBOX_TEXT))
      .WillByDefault(testing::Return(SkColorSetRGB(0, 0, 14)));
  ON_CALL(mock_theme_provider_,
          GetColor(ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_SELECTED))
      .WillByDefault(testing::Return(SkColorSetRGB(0, 0, 15)));
  ON_CALL(mock_theme_provider_,
          GetColor(ThemeProperties::COLOR_OMNIBOX_RESULTS_URL))
      .WillByDefault(testing::Return(SkColorSetRGB(0, 0, 16)));
  ON_CALL(mock_theme_provider_,
          GetColor(ThemeProperties::COLOR_OMNIBOX_RESULTS_URL_SELECTED))
      .WillByDefault(testing::Return(SkColorSetRGB(0, 0, 17)));

  theme_service_observer_->OnThemeChanged();
  mock_page_.FlushForTesting();

  ASSERT_TRUE(theme);
  EXPECT_EQ(SkColorSetRGB(0, 0, 1), theme->background_color);
  EXPECT_EQ(SkColorSetRGB(0, 0, 2), theme->text_color);
  EXPECT_FALSE(theme->is_default);
  EXPECT_FALSE(theme->is_custom_background);
  EXPECT_FALSE(theme->is_dark);
  EXPECT_EQ(SkColorSetRGB(0, 0, 3), theme->logo_color);
  EXPECT_FALSE(theme->daily_refresh_collection_id.has_value());
  ASSERT_TRUE(theme->background_image);
  EXPECT_EQ("chrome-untrusted://theme/IDR_THEME_NTP_BACKGROUND?bar",
            theme->background_image->url);
  EXPECT_EQ("chrome-untrusted://theme/IDR_THEME_NTP_BACKGROUND@2x?bar",
            theme->background_image->url_2x);
  EXPECT_EQ("chrome://theme/IDR_THEME_NTP_ATTRIBUTION?bar",
            theme->background_image->attribution_url);
  EXPECT_EQ("initial", theme->background_image->size);
  EXPECT_EQ("repeat", theme->background_image->repeat_x);
  EXPECT_EQ("no-repeat", theme->background_image->repeat_y);
  EXPECT_EQ("center", theme->background_image->position_x);
  EXPECT_EQ("top", theme->background_image->position_y);
  EXPECT_FALSE(theme->background_image_attribution_1.has_value());
  EXPECT_FALSE(theme->background_image_attribution_2.has_value());
  EXPECT_FALSE(theme->background_image_attribution_url.has_value());
  ASSERT_TRUE(theme->most_visited);
  EXPECT_EQ(SkColorSetRGB(0, 0, 4), theme->most_visited->background_color);
  EXPECT_TRUE(theme->most_visited->use_white_tile_icon);
  EXPECT_TRUE(theme->most_visited->use_title_pill);
  EXPECT_EQ(false, theme->most_visited->is_dark);
  EXPECT_EQ(SkColorSetRGB(0, 0, 5), theme->search_box->bg);
  EXPECT_EQ(SkColorSetRGB(0, 0, 6), theme->search_box->icon);
  EXPECT_EQ(SkColorSetRGB(0, 0, 7), theme->search_box->icon_selected);
  EXPECT_EQ(SkColorSetRGB(0, 0, 8), theme->search_box->placeholder);
  EXPECT_EQ(SkColorSetRGB(0, 0, 9), theme->search_box->results_bg);
  EXPECT_EQ(SkColorSetRGB(0, 0, 10), theme->search_box->results_bg_hovered);
  EXPECT_EQ(SkColorSetRGB(0, 0, 11), theme->search_box->results_bg_selected);
  EXPECT_EQ(SkColorSetRGB(0, 0, 12), theme->search_box->results_dim);
  EXPECT_EQ(SkColorSetRGB(0, 0, 13), theme->search_box->results_dim_selected);
  EXPECT_EQ(SkColorSetRGB(0, 0, 14), theme->search_box->results_text);
  EXPECT_EQ(SkColorSetRGB(0, 0, 15), theme->search_box->results_text_selected);
  EXPECT_EQ(SkColorSetRGB(0, 0, 16), theme->search_box->results_url);
  EXPECT_EQ(SkColorSetRGB(0, 0, 17), theme->search_box->results_url_selected);
  EXPECT_EQ(SkColorSetRGB(0, 0, 14), theme->search_box->text);
}

TEST_F(NewTabPageHandlerTest, SetCustomBackground) {
  new_tab_page::mojom::ThemePtr theme;
  EXPECT_CALL(mock_page_, SetTheme)
      .Times(1)
      .WillOnce(testing::Invoke([&theme](new_tab_page::mojom::ThemePtr arg) {
        theme = std::move(arg);
      }));
  CustomBackground custom_background;
  custom_background.custom_background_url = GURL("https://foo.com/img.png");
  custom_background.custom_background_attribution_line_1 = "foo line";
  custom_background.custom_background_attribution_line_2 = "bar line";
  custom_background.custom_background_attribution_action_url =
      GURL("https://foo.com/action");
  custom_background.collection_id = "baz collection";
  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(testing::Return(absl::make_optional(custom_background)));

  ntp_custom_background_service_observer_->OnCustomBackgroundImageUpdated();
  mock_page_.FlushForTesting();

  ASSERT_TRUE(theme);
  EXPECT_TRUE(theme->is_custom_background);
  EXPECT_EQ(gfx::kGoogleGrey050, theme->text_color);
  EXPECT_EQ(ThemeProperties::GetDefaultColor(
                ThemeProperties::COLOR_NTP_SHORTCUT, false),
            theme->most_visited->background_color);
  EXPECT_EQ(
      ThemeProperties::GetDefaultColor(ThemeProperties::COLOR_NTP_LOGO, false),
      theme->logo_color);
  EXPECT_EQ("https://foo.com/img.png", theme->background_image->url);
  EXPECT_EQ("foo line", theme->background_image_attribution_1);
  EXPECT_EQ("bar line", theme->background_image_attribution_2);
  EXPECT_EQ("https://foo.com/action", theme->background_image_attribution_url);
  EXPECT_EQ("baz collection", theme->daily_refresh_collection_id);
}

TEST_F(NewTabPageHandlerTest, Histograms) {
  histogram_tester_.ExpectTotalCount(
      NewTabPageHandler::kModuleDismissedHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      NewTabPageHandler::kModuleRestoredHistogram, 0);

  handler_->OnDismissModule("shopping_tasks");
  histogram_tester_.ExpectTotalCount(
      NewTabPageHandler::kModuleDismissedHistogram, 1);
  histogram_tester_.ExpectTotalCount(
      std::string(NewTabPageHandler::kModuleDismissedHistogram) +
          ".shopping_tasks",
      1);

  handler_->OnRestoreModule("kaleidoscope");
  histogram_tester_.ExpectTotalCount(
      NewTabPageHandler::kModuleRestoredHistogram, 1);
  histogram_tester_.ExpectTotalCount(
      std::string(NewTabPageHandler::kModuleRestoredHistogram) +
          ".kaleidoscope",
      1);
}

TEST_F(NewTabPageHandlerTest, GetAnimatedDoodle) {
  search_provider_logos::EncodedLogo logo;
  std::string encoded_image("light image");
  std::string dark_encoded_image("dark image");
  logo.encoded_image = base::RefCountedString::TakeString(&encoded_image);
  logo.dark_encoded_image =
      base::RefCountedString::TakeString(&dark_encoded_image);
  logo.metadata.type = search_provider_logos::LogoType::ANIMATED;
  logo.metadata.on_click_url = GURL("https://doodle.com/on_click_url");
  logo.metadata.alt_text = "alt text";
  logo.metadata.mime_type = "light_mime_type";
  logo.metadata.dark_mime_type = "dark_mime_type";
  logo.metadata.dark_background_color = "#000001";
  logo.metadata.animated_url = GURL("https://doodle.com/light_animation");
  logo.metadata.dark_animated_url = GURL("https://doodle.com/dark_animation");
  logo.metadata.cta_log_url = GURL("https://doodle.com/light_cta_log_url");
  logo.metadata.dark_cta_log_url = GURL("https://doodle.com/dark_cta_log_url");
  logo.metadata.log_url = GURL("https://doodle.com/light_log_url");
  logo.metadata.dark_log_url = GURL("https://doodle.com/dark_log_url");
  logo.metadata.short_link = GURL("https://doodle.com/short_link");
  logo.metadata.width_px = 1;
  logo.metadata.height_px = 2;
  logo.metadata.dark_width_px = 3;
  logo.metadata.dark_height_px = 4;
  logo.metadata.share_button_x = 5;
  logo.metadata.dark_share_button_x = 6;
  logo.metadata.share_button_y = 7;
  logo.metadata.dark_share_button_y = 8;
  logo.metadata.share_button_opacity = 0.5;
  logo.metadata.dark_share_button_opacity = 0.7;
  logo.metadata.share_button_icon = "light share button";
  logo.metadata.dark_share_button_icon = "dark share button";
  logo.metadata.share_button_bg = "#000100";
  logo.metadata.dark_share_button_bg = "#010000";

  auto doodle = GetDoodle(logo);

  ASSERT_TRUE(doodle);
  ASSERT_TRUE(doodle->image);
  ASSERT_FALSE(doodle->interactive);
  EXPECT_EQ("data:light_mime_type;base64,bGlnaHQgaW1hZ2U=",
            doodle->image->light->image_url);
  EXPECT_EQ("https://doodle.com/light_animation",
            doodle->image->light->animation_url);
  EXPECT_EQ(1u, doodle->image->light->width);
  EXPECT_EQ(2u, doodle->image->light->height);
  EXPECT_EQ(SK_ColorWHITE, doodle->image->light->background_color);
  EXPECT_EQ(5, doodle->image->light->share_button->x);
  EXPECT_EQ(7, doodle->image->light->share_button->y);
  EXPECT_EQ(SkColorSetARGB(127, 0, 1, 0),
            doodle->image->light->share_button->background_color);
  EXPECT_EQ("data:image/png;base64,light share button",
            doodle->image->light->share_button->icon_url);
  EXPECT_EQ("https://doodle.com/light_cta_log_url",
            doodle->image->light->image_impression_log_url);
  EXPECT_EQ("https://doodle.com/light_log_url",
            doodle->image->light->animation_impression_log_url);
  EXPECT_EQ("data:dark_mime_type;base64,ZGFyayBpbWFnZQ==",
            doodle->image->dark->image_url);
  EXPECT_EQ("https://doodle.com/dark_animation",
            doodle->image->dark->animation_url);
  EXPECT_EQ(3u, doodle->image->dark->width);
  EXPECT_EQ(4u, doodle->image->dark->height);
  EXPECT_EQ(SkColorSetRGB(0, 0, 1), doodle->image->dark->background_color);
  EXPECT_EQ(6, doodle->image->dark->share_button->x);
  EXPECT_EQ(8, doodle->image->dark->share_button->y);
  EXPECT_EQ(SkColorSetARGB(178, 1, 0, 0),
            doodle->image->dark->share_button->background_color);
  EXPECT_EQ("data:image/png;base64,dark share button",
            doodle->image->dark->share_button->icon_url);
  EXPECT_EQ("https://doodle.com/dark_cta_log_url",
            doodle->image->dark->image_impression_log_url);
  EXPECT_EQ("https://doodle.com/dark_log_url",
            doodle->image->dark->animation_impression_log_url);
  EXPECT_EQ("https://doodle.com/on_click_url", doodle->image->on_click_url);
  EXPECT_EQ("https://doodle.com/short_link", doodle->image->share_url);
  EXPECT_EQ("alt text", doodle->description);
}

TEST_F(NewTabPageHandlerTest, GetInteractiveDoodle) {
  search_provider_logos::EncodedLogo logo;
  logo.metadata.type = search_provider_logos::LogoType::INTERACTIVE;
  logo.metadata.full_page_url = GURL("https://doodle.com/full_page_url");
  logo.metadata.iframe_width_px = 1;
  logo.metadata.iframe_height_px = 2;
  logo.metadata.alt_text = "alt text";

  auto doodle = GetDoodle(logo);

  EXPECT_EQ("https://doodle.com/full_page_url", doodle->interactive->url);
  EXPECT_EQ(1u, doodle->interactive->width);
  EXPECT_EQ(2u, doodle->interactive->height);
  EXPECT_EQ("alt text", doodle->description);
}

TEST_F(NewTabPageHandlerTest, GetPromo) {
  PromoData promo_data;
  promo_data.promo_html = "<html/>";
  promo_data.middle_slot_json = R"({
    "part": [{
      "image": {
        "image_url": "https://image.com/image",
        "target": "https://image.com/target"
      }
    }, {
      "link": {
        "url": "https://link.com",
        "text": "bar",
        "color": "red"
      }
    }, {
      "text": {
        "text": "blub",
        "color": "green"
      }
    }]
  })";
  promo_data.promo_log_url = GURL("https://foo.com");
  promo_data.promo_id = "foo";
  auto promo_data_optional = absl::make_optional(promo_data);
  ON_CALL(mock_promo_service_, promo_data())
      .WillByDefault(testing::ReturnRef(promo_data_optional));
  EXPECT_CALL(mock_promo_service_, Refresh).Times(1);

  new_tab_page::mojom::PromoPtr promo;
  base::MockCallback<NewTabPageHandler::GetPromoCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&promo](new_tab_page::mojom::PromoPtr arg) {
        promo = std::move(arg);
      }));
  handler_->GetPromo(callback.Get());

  ASSERT_TRUE(promo);
  EXPECT_EQ("foo", promo->id);
  EXPECT_EQ("https://foo.com/", promo->log_url);
  ASSERT_EQ(3lu, promo->middle_slot_parts.size());
  ASSERT_TRUE(promo->middle_slot_parts[0]->is_image());
  const auto& image = promo->middle_slot_parts[0]->get_image();
  EXPECT_EQ("https://image.com/image", image->image_url);
  EXPECT_EQ("https://image.com/target", image->target);
  ASSERT_TRUE(promo->middle_slot_parts[1]->is_link());
  const auto& link = promo->middle_slot_parts[1]->get_link();
  EXPECT_EQ("red", link->color);
  EXPECT_EQ("bar", link->text);
  EXPECT_EQ("https://link.com/", link->url);
  ASSERT_TRUE(promo->middle_slot_parts[2]->is_text());
  const auto& text = promo->middle_slot_parts[2]->get_text();
  EXPECT_EQ("green", text->color);
  EXPECT_EQ("blub", text->text);
}

TEST_F(NewTabPageHandlerTest, OnDoodleImageClicked) {
  handler_->OnDoodleImageClicked(
      /*type=*/new_tab_page::mojom::DoodleImageType::kCta,
      /*log_url=*/GURL("https://doodle.com/log"));

  histogram_tester_.ExpectTotalCount("NewTabPage.LogoClick", 1);
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://doodle.com/log", ""));
}

TEST_F(NewTabPageHandlerTest, OnDoodleImageRendered) {
  base::MockCallback<NewTabPageHandler::OnDoodleImageRenderedCallback> callback;
  absl::optional<std::string> image_click_params;
  absl::optional<GURL> interaction_log_url;
  absl::optional<std::string> shared_id;
  EXPECT_CALL(callback, Run(_, _, _))
      .Times(1)
      .WillOnce(DoAll(SaveArg<0>(&image_click_params),
                      SaveArg<1>(&interaction_log_url),
                      SaveArg<2>(&shared_id)));

  handler_->OnDoodleImageRendered(
      /*type=*/new_tab_page::mojom::DoodleImageType::kStatic,
      /*time=*/0,
      /*log_url=*/GURL("https://doodle.com/log"), callback.Get());

  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://doodle.com/log", R"()]}'
  {
    "ddllog": {
      "target_url_params": "foo params",
      "interaction_log_url": "/bar_log",
      "encoded_ei": "baz ei"
    }
  })"));
  EXPECT_THAT(image_click_params, Optional(std::string("foo params")));
  EXPECT_THAT(interaction_log_url,
              Optional(GURL("https://www.google.com/bar_log")));
  EXPECT_THAT(shared_id, Optional(std::string("baz ei")));
  histogram_tester_.ExpectTotalCount("NewTabPage.LogoShown", 1);
  histogram_tester_.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 1);
  histogram_tester_.ExpectTotalCount("NewTabPage.LogoShownTime2", 1);
}

TEST_F(NewTabPageHandlerTest, OnDoodleShared) {
  handler_->OnDoodleShared(new_tab_page::mojom::DoodleShareChannel::kEmail,
                           "food_id", "bar_id");

  EXPECT_TRUE(test_url_loader_factory_.IsPending(
      "https://www.google.com/"
      "gen_204?atype=i&ct=doodle&ntp=2&cad=sh,5,ct:food_id&ei=bar_id"));
}

TEST_F(NewTabPageHandlerTest, GetModulesOrder) {
  std::vector<std::string> module_ids;
  base::MockCallback<NewTabPageHandler::GetModulesOrderCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(1).WillOnce(SaveArg<0>(&module_ids));
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{ntp_features::kModules,
        {{ntp_features::kNtpModulesOrderParam, "bar,baz"}}},
       {ntp_features::kNtpModulesDragAndDrop, {}}},
      {});
  base::Value module_ids_value(base::Value::Type::LIST);
  module_ids_value.Append("foo");
  module_ids_value.Append("bar");
  profile_->GetPrefs()->Set(prefs::kNtpModulesOrder, module_ids_value);

  handler_->GetModulesOrder(callback.Get());
  EXPECT_THAT(module_ids, ElementsAre("foo", "bar", "baz"));
}
