// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_password_manager_driver.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/core/browser/logging/stub_log_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/password_manager/content/browser/form_meta_data.h"
#include "components/password_manager/core/browser/password_form_filling.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

using autofill::ParsingResult;
using autofill::PasswordFormFillData;
using base::ASCIIToUTF16;
using testing::_;
using testing::ElementsAre;
using testing::NiceMock;
using testing::Return;

namespace password_manager {

namespace {

class MockLogManager : public autofill::StubLogManager {
 public:
  MOCK_METHOD(bool, IsLoggingActive, (), (const override));
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  MockPasswordManagerClient(const MockPasswordManagerClient&) = delete;
  MockPasswordManagerClient& operator=(const MockPasswordManagerClient&) =
      delete;
  ~MockPasswordManagerClient() override = default;

  MOCK_METHOD(const autofill::LogManager*, GetLogManager, (), (const override));
  MOCK_METHOD(PasswordManager*, GetPasswordManager, (), (const override));
#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
  MOCK_METHOD(void,
              CheckSafeBrowsingReputation,
              (const GURL&, const GURL&),
              (override));
#endif
};

class FakePasswordAutofillAgent
    : public autofill::mojom::PasswordAutofillAgent {
 public:
  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<autofill::mojom::PasswordAutofillAgent>(
            std::move(handle)));
  }

  bool called_set_logging_state() { return called_set_logging_state_; }

  bool logging_state_active() { return logging_state_active_; }

  void reset_data() {
    called_set_logging_state_ = false;
    logging_state_active_ = false;
  }

  // autofill::mojom::PasswordAutofillAgent:
  MOCK_METHOD(void,
              FillPasswordForm,
              (const PasswordFormFillData&),
              (override));
  MOCK_METHOD(void, InformNoSavedCredentials, (bool), (override));
  MOCK_METHOD(void,
              FillIntoFocusedField,
              (bool, const std::u16string&),
              (override));
  MOCK_METHOD(void, TouchToFillClosed, (bool), (override));
  MOCK_METHOD(void,
              AnnotateFieldsWithParsingResult,
              (const ParsingResult&),
              (override));

 private:
  void SetLoggingState(bool active) override {
    called_set_logging_state_ = true;
    logging_state_active_ = active;
  }

  // Records whether SetLoggingState() gets called.
  bool called_set_logging_state_ = false;
  // Records data received via SetLoggingState() call.
  bool logging_state_active_ = false;

  mojo::AssociatedReceiver<autofill::mojom::PasswordAutofillAgent> receiver_{
      this};
};

class MockPasswordManager : public PasswordManager {
 public:
  explicit MockPasswordManager(PasswordManagerClient* client)
      : PasswordManager(client) {}
  ~MockPasswordManager() override = default;

  MOCK_METHOD(void,
              OnPasswordFormsParsed,
              (PasswordManagerDriver * driver,
               const std::vector<autofill::FormData>&),
              (override));
  MOCK_METHOD(void,
              OnPasswordFormsRendered,
              (PasswordManagerDriver * driver,
               const std::vector<autofill::FormData>&,
               bool),
              (override));
  MOCK_METHOD(void,
              OnPasswordFormSubmitted,
              (PasswordManagerDriver * driver, const autofill::FormData&),
              (override));
  MOCK_METHOD(void,
              OnPasswordFormCleared,
              (PasswordManagerDriver * driver, const autofill::FormData&),
              (override));
};

PasswordFormFillData GetTestPasswordFormFillData() {
  // Create the current form on the page.
  PasswordForm form_on_page;
  form_on_page.url = GURL("https://foo.com/");
  form_on_page.action = GURL("https://foo.com/login");
  form_on_page.signon_realm = "https://foo.com/";
  form_on_page.scheme = PasswordForm::Scheme::kHtml;
  form_on_page.form_data.host_frame = autofill::LocalFrameToken(
      base::UnguessableToken::Deserialize(98765, 43210));

  // Create an exact match in the database.
  PasswordForm preferred_match = form_on_page;
  preferred_match.username_element = u"username";
  preferred_match.username_value = u"test@gmail.com";
  preferred_match.password_element = u"password";
  preferred_match.password_value = u"test";

  std::vector<const PasswordForm*> matches;
  PasswordForm non_preferred_match = preferred_match;
  non_preferred_match.username_value = u"test1@gmail.com";
  non_preferred_match.password_value = u"test1";
  matches.push_back(&non_preferred_match);

  return CreatePasswordFormFillData(form_on_page, matches, preferred_match,
                                    true);
}

MATCHER(WerePasswordsCleared, "Passwords not cleared") {
  if (!arg.password_field.value.empty())
    return false;

  for (auto& credentials : arg.additional_logins)
    if (!credentials.password.empty())
      return false;

  return true;
}

MATCHER_P(FormDataEqualTo, form_data, "") {
  return autofill::FormDataEqualForTesting(arg, form_data);
}

}  // namespace

class ContentPasswordManagerDriverTest
    : public content::RenderViewHostTestHarness,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    ON_CALL(password_manager_client_, GetLogManager())
        .WillByDefault(Return(&log_manager_));

    blink::AssociatedInterfaceProvider* remote_interfaces =
        web_contents()->GetMainFrame()->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        autofill::mojom::PasswordAutofillAgent::Name_,
        base::BindRepeating(&FakePasswordAutofillAgent::BindPendingReceiver,
                            base::Unretained(&fake_agent_)));
  }

  bool WasLoggingActivationMessageSent(bool* activation_flag) {
    base::RunLoop().RunUntilIdle();
    if (!fake_agent_.called_set_logging_state())
      return false;

    if (activation_flag)
      *activation_flag = fake_agent_.logging_state_active();
    fake_agent_.reset_data();
    return true;
  }

 protected:
  NiceMock<MockLogManager> log_manager_;
  NiceMock<MockPasswordManagerClient> password_manager_client_;
  autofill::TestAutofillClient autofill_client_;

  FakePasswordAutofillAgent fake_agent_;
};

TEST_P(ContentPasswordManagerDriverTest, SendLoggingStateInCtor) {
  const bool should_allow_logging = GetParam();
  EXPECT_CALL(log_manager_, IsLoggingActive())
      .WillRepeatedly(Return(should_allow_logging));
  std::unique_ptr<ContentPasswordManagerDriver> driver(
      new ContentPasswordManagerDriver(main_rfh(), &password_manager_client_,
                                       &autofill_client_));

  if (should_allow_logging) {
    bool logging_activated = false;
    EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_activated));
    EXPECT_TRUE(logging_activated);
  } else {
    bool logging_activated = true;
    EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_activated));
    EXPECT_FALSE(logging_activated);
  }
}

TEST_P(ContentPasswordManagerDriverTest, SendLoggingStateAfterLogManagerReady) {
  const bool should_allow_logging = GetParam();
  EXPECT_CALL(password_manager_client_, GetLogManager())
      .WillOnce(Return(nullptr));
  std::unique_ptr<ContentPasswordManagerDriver> driver(
      new ContentPasswordManagerDriver(main_rfh(), &password_manager_client_,
                                       &autofill_client_));
  // Because log manager is not ready yet, should have no logging state sent.
  EXPECT_FALSE(WasLoggingActivationMessageSent(nullptr));

  // Log manager is ready, send logging state actually.
  EXPECT_CALL(password_manager_client_, GetLogManager())
      .WillOnce(Return(&log_manager_));
  EXPECT_CALL(log_manager_, IsLoggingActive())
      .WillRepeatedly(Return(should_allow_logging));
  driver->SendLoggingAvailability();
  bool logging_activated = false;
  EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_activated));
  EXPECT_EQ(should_allow_logging, logging_activated);
}

TEST_F(ContentPasswordManagerDriverTest, ClearPasswordsOnAutofill) {
  std::unique_ptr<ContentPasswordManagerDriver> driver(
      new ContentPasswordManagerDriver(main_rfh(), &password_manager_client_,
                                       &autofill_client_));

  PasswordFormFillData fill_data = GetTestPasswordFormFillData();
  fill_data.wait_for_username = true;
  EXPECT_CALL(fake_agent_, FillPasswordForm(WerePasswordsCleared()));
  driver->FillPasswordForm(fill_data);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContentPasswordManagerDriverTest, SetFrameAndFormMetaDataOfForm) {
  NavigateAndCommit(GURL("https://username:password@hostname/path?query#hash"));

  std::unique_ptr<ContentPasswordManagerDriver> driver(
      new ContentPasswordManagerDriver(main_rfh(), &password_manager_client_,
                                       &autofill_client_));
  autofill::FormData form;
  autofill::FormData form2 = GetFormWithFrameAndFormMetaData(main_rfh(), form);

  EXPECT_EQ(form2.host_frame,
            autofill::LocalFrameToken(
                web_contents()->GetMainFrame()->GetFrameToken().value()));
  EXPECT_EQ(form2.url, GURL("https://hostname/path"));
  EXPECT_EQ(form2.full_url, GURL("https://hostname/path?query#hash"));
  EXPECT_EQ(form2.main_frame_origin,
            web_contents()->GetMainFrame()->GetLastCommittedOrigin());
  EXPECT_EQ(form2.main_frame_origin,
            url::Origin::CreateFromNormalizedTuple("https", "hostname", 443));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ContentPasswordManagerDriverTest,
                         testing::Values(true, false));

class ContentPasswordManagerDriverURLTest
    : public ContentPasswordManagerDriverTest {
 public:
  void SetUp() override {
    ContentPasswordManagerDriverTest::SetUp();
    ON_CALL(password_manager_client_, GetPasswordManager())
        .WillByDefault(Return(&password_manager_));
    driver_ = std::make_unique<ContentPasswordManagerDriver>(
        main_rfh(), &password_manager_client_, &autofill_client_);
    NavigateAndCommit(
        GURL("https://username:password@hostname/path?query#hash"));
  }

  autofill::FormData ExpectedFormData() {
    autofill::FormData expected_form;
    expected_form.url = GURL("https://hostname/path");
    expected_form.full_url = GURL("https://hostname/path?query#hash");
    expected_form.main_frame_origin =
        url::Origin::CreateFromNormalizedTuple("https", "hostname", 443);
    expected_form.host_frame = autofill::LocalFrameToken(
        web_contents()->GetMainFrame()->GetFrameToken().value());
    return expected_form;
  }

  autofill::mojom::PasswordManagerDriver* driver() {
    return static_cast<autofill::mojom::PasswordManagerDriver*>(driver_.get());
  }

 protected:
  MockPasswordManager password_manager_{&password_manager_client_};
  std::unique_ptr<ContentPasswordManagerDriver> driver_;
};

TEST_F(ContentPasswordManagerDriverURLTest, PasswordFormsParsed) {
  autofill::FormData form;
  form.url = GURL("http://evil.com");
  form.full_url = GURL("http://evil.com/path");

  EXPECT_CALL(password_manager_,
              OnPasswordFormsParsed(
                  _, ElementsAre(FormDataEqualTo(ExpectedFormData()))));

  driver()->PasswordFormsParsed({form});
}

TEST_F(ContentPasswordManagerDriverURLTest, PasswordFormsRendered) {
  autofill::FormData form;
  form.url = GURL("http://evil.com");
  form.full_url = GURL("http://evil.com/path");

  EXPECT_CALL(password_manager_,
              OnPasswordFormsRendered(
                  _, ElementsAre(FormDataEqualTo(ExpectedFormData())), _));

  driver()->PasswordFormsRendered({form}, false);
}

TEST_F(ContentPasswordManagerDriverURLTest, PasswordFormSubmitted) {
  autofill::FormData form;
  form.url = GURL("http://evil.com");
  form.full_url = GURL("http://evil.com/path");

  EXPECT_CALL(password_manager_,
              OnPasswordFormSubmitted(_, FormDataEqualTo(ExpectedFormData())));

  driver()->PasswordFormSubmitted(form);
}

TEST_F(ContentPasswordManagerDriverURLTest, PasswordFormCleared) {
  autofill::FormData form;
  form.url = GURL("http://evil.com");
  form.full_url = GURL("http://evil.com/path");

  EXPECT_CALL(password_manager_,
              OnPasswordFormCleared(_, FormDataEqualTo(ExpectedFormData())));

  driver()->PasswordFormCleared(form);
}

}  // namespace password_manager
