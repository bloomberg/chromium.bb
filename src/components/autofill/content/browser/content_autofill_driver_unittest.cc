// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/content/browser/content_autofill_driver_test_api.h"
#include "components/autofill/content/browser/content_autofill_router.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace autofill {

using ::testing::_;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Return;
using ::testing::SaveArg;

namespace {

const char kAppLocale[] = "en-US";
const BrowserAutofillManager::AutofillDownloadManagerState kDownloadState =
    BrowserAutofillManager::DISABLE_AUTOFILL_DOWNLOAD_MANAGER;

class FakeAutofillAgent : public mojom::AutofillAgent {
 public:
  FakeAutofillAgent()
      : fill_form_id_(-1),
        preview_form_id_(-1),
        called_clear_section_(false),
        called_clear_previewed_form_(false) {}

  ~FakeAutofillAgent() override {}

  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(this, mojo::PendingAssociatedReceiver<mojom::AutofillAgent>(
                             std::move(handle)));
  }

  void SetQuitLoopClosure(base::OnceClosure closure) {
    quit_closure_ = std::move(closure);
  }

  // Returns the id and formdata received via
  // mojo interface method mojom::AutofillAgent::FillOrPreviewForm().
  bool GetAutofillFillFormMessage(int* page_id, FormData* results) {
    if (fill_form_id_ == -1)
      return false;
    if (!fill_form_form_)
      return false;

    if (page_id)
      *page_id = fill_form_id_;
    if (results)
      *results = *fill_form_form_;
    return true;
  }

  // Returns the id and formdata received via
  // mojo interface method mojom::AutofillAgent::PreviewForm().
  bool GetAutofillPreviewFormMessage(int* page_id, FormData* results) {
    if (preview_form_id_ == -1)
      return false;
    if (!preview_form_form_)
      return false;

    if (page_id)
      *page_id = preview_form_id_;
    if (results)
      *results = *preview_form_form_;
    return true;
  }

  // Returns data received via mojo interface method
  // mojom::AutofillAent::FieldTypePredictionsAvailable().
  bool GetFieldTypePredictionsAvailable(
      std::vector<FormDataPredictions>* predictions) {
    if (!predictions_)
      return false;
    if (predictions)
      *predictions = *predictions_;
    return true;
  }

  // Returns whether mojo interface method mojom::AutofillAgent::ClearForm() got
  // called.
  bool GetCalledClearSection() { return called_clear_section_; }

  // Returns whether mojo interface method
  // mojom::AutofillAgent::ClearPreviewedForm() got called.
  bool GetCalledClearPreviewedForm() { return called_clear_previewed_form_; }

  // Returns data received via mojo interface method
  // mojom::AutofillAent::FillFieldWithValue().
  bool GetString16FillFieldWithValue(const FieldGlobalId& field,
                                     std::u16string* value) {
    if (!value_fill_field_ || value_renderer_id_ != field.renderer_id)
      return false;
    if (value)
      *value = *value_fill_field_;
    return true;
  }

  // Returns data received via mojo interface method
  // mojom::AutofillAent::PreviewFieldWithValue().
  bool GetString16PreviewFieldWithValue(const FieldGlobalId field,
                                        std::u16string* value) {
    if (!value_preview_field_ || value_renderer_id_ != field.renderer_id)
      return false;
    if (value)
      *value = *value_preview_field_;
    return true;
  }

  // Returns data received via mojo interface method
  // mojom::AutofillAent::AcceptDataListSuggestion().
  bool GetString16AcceptDataListSuggestion(FieldGlobalId field,
                                           std::u16string* value) {
    if (!value_accept_data_ || value_renderer_id_ != field.renderer_id)
      return false;
    if (value)
      *value = *value_accept_data_;
    return true;
  }

  // Mocked mojom::AutofillAgent methods:
  MOCK_METHOD(void, EnableHeavyFormDataScraping, (), (override));

 private:
  void CallDone() {
    if (!quit_closure_.is_null())
      std::move(quit_closure_).Run();
  }

  // mojom::AutofillAgent:
  void TriggerReparse() override {}

  void FillOrPreviewForm(int32_t id,
                         const FormData& form,
                         mojom::RendererFormDataAction action) override {
    if (action == mojom::RendererFormDataAction::kPreview) {
      preview_form_id_ = id;
      preview_form_form_ = form;
    } else {
      fill_form_id_ = id;
      fill_form_form_ = form;
    }
    CallDone();
  }

  void FieldTypePredictionsAvailable(
      const std::vector<FormDataPredictions>& forms) override {
    predictions_ = forms;
    CallDone();
  }

  void ClearSection() override {
    called_clear_section_ = true;
    CallDone();
  }

  void ClearPreviewedForm() override {
    called_clear_previewed_form_ = true;
    CallDone();
  }

  void FillFieldWithValue(FieldRendererId field,
                          const std::u16string& value) override {
    value_renderer_id_ = field;
    value_fill_field_ = value;
    CallDone();
  }

  void PreviewFieldWithValue(FieldRendererId field,
                             const std::u16string& value) override {
    value_renderer_id_ = field;
    value_preview_field_ = value;
    CallDone();
  }

  void SetSuggestionAvailability(FieldRendererId field,
                                 const mojom::AutofillState state) override {
    value_renderer_id_ = field;
    if (state == mojom::AutofillState::kAutofillAvailable)
      suggestions_available_ = true;
    else if (state == mojom::AutofillState::kNoSuggestions)
      suggestions_available_ = false;
    CallDone();
  }

  void AcceptDataListSuggestion(FieldRendererId field,
                                const std::u16string& value) override {
    value_renderer_id_ = field;
    value_accept_data_ = value;
    CallDone();
  }

  void FillPasswordSuggestion(const std::u16string& username,
                              const std::u16string& password) override {}

  void PreviewPasswordSuggestion(const std::u16string& username,
                                 const std::u16string& password) override {}

  void SetUserGestureRequired(bool required) override {}

  void SetSecureContextRequired(bool required) override {}

  void SetFocusRequiresScroll(bool require) override {}

  void SetQueryPasswordSuggestion(bool query) override {}

  void GetElementFormAndFieldDataForDevToolsNodeId(
      int backend_node_id,
      GetElementFormAndFieldDataForDevToolsNodeIdCallback callback) override {}

  void SetAssistantKeyboardSuppressState(bool suppress) override {}

  void SetFieldsEligibleForManualFilling(
      const std::vector<FieldRendererId>& fields) override {}

  mojo::AssociatedReceiverSet<mojom::AutofillAgent> receivers_;

  base::OnceClosure quit_closure_;

  // Records data received from FillOrPreviewForm() call.
  int32_t fill_form_id_;
  absl::optional<FormData> fill_form_form_;
  int32_t preview_form_id_;
  absl::optional<FormData> preview_form_form_;
  // Records data received from FieldTypePredictionsAvailable() call.
  absl::optional<std::vector<FormDataPredictions>> predictions_;
  // Records whether ClearSection() got called.
  bool called_clear_section_;
  // Records whether ClearPreviewedForm() got called.
  bool called_clear_previewed_form_;
  // Records the ID received from FillFieldWithValue(), PreviewFieldWithValue(),
  // SetSuggestionAvailability(), or AcceptDataListSuggestion().
  absl::optional<FieldRendererId> value_renderer_id_;
  // Records string received from FillFieldWithValue() call.
  absl::optional<std::u16string> value_fill_field_;
  // Records string received from PreviewFieldWithValue() call.
  absl::optional<std::u16string> value_preview_field_;
  // Records string received from AcceptDataListSuggestion() call.
  absl::optional<std::u16string> value_accept_data_;
  // Records bool received from SetSuggestionAvailability() call.
  bool suggestions_available_;
};

}  // namespace

class MockBrowserAutofillManager : public BrowserAutofillManager {
 public:
  MockBrowserAutofillManager(AutofillDriver* driver, AutofillClient* client)
      : BrowserAutofillManager(driver, client, kAppLocale, kDownloadState) {}
  ~MockBrowserAutofillManager() override {}

  MOCK_METHOD(void, Reset, (), (override));
  MOCK_METHOD(bool, ShouldParseForms, (const std::vector<FormData>&), ());
};

class MockAutofillClient : public TestAutofillClient {};

class TestContentAutofillDriver : public ContentAutofillDriver {
 public:
  TestContentAutofillDriver(content::RenderFrameHost* rfh,
                            AutofillClient* client,
                            ContentAutofillRouter* router)
      : ContentAutofillDriver(
            rfh,
            client,
            kAppLocale,
            router,
            kDownloadState,
            AutofillManager::AutofillManagerFactoryCallback()) {
    std::unique_ptr<MockBrowserAutofillManager> autofill_manager(
        new MockBrowserAutofillManager(this, client));
    SetBrowserAutofillManager(std::move(autofill_manager));
  }
  ~TestContentAutofillDriver() override = default;

  virtual MockBrowserAutofillManager* mock_browser_autofill_manager() {
    return static_cast<MockBrowserAutofillManager*>(browser_autofill_manager());
  }

  using ContentAutofillDriver::DidNavigateFrame;
};

class ContentAutofillDriverTest : public content::RenderViewHostTestHarness,
                                  public ::testing::WithParamInterface<bool> {
 public:
  enum class NavigationType {
    kNormal,
    kSameDocument,
    kServedFromBackForwardCache,
    kPrerenderedPageActivation,
  };

  ContentAutofillDriverTest() : autofill_across_iframes_(GetParam()) {
    std::vector<base::Feature> enabled;
    std::vector<base::Feature> disabled;
    (autofill_across_iframes_ ? enabled : disabled)
        .push_back(features::kAutofillAcrossIframes);
    scoped_feature_list_.InitWithFeatures(enabled, disabled);
  }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    // This needed to keep the WebContentsObserverConsistencyChecker checks
    // happy for when AppendChild is called.
    NavigateAndCommit(GURL("about:blank"));

    test_autofill_client_ = std::make_unique<MockAutofillClient>();
    router_ = std::make_unique<ContentAutofillRouter>();
    driver_ = std::make_unique<TestContentAutofillDriver>(
        web_contents()->GetMainFrame(), test_autofill_client_.get(),
        router_.get());

    blink::AssociatedInterfaceProvider* remote_interfaces =
        web_contents()->GetMainFrame()->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        mojom::AutofillAgent::Name_,
        base::BindRepeating(&FakeAutofillAgent::BindPendingReceiver,
                            base::Unretained(&fake_agent_)));
  }

  void TearDown() override {
    // Reset the driver now to cause all pref observers to be removed and avoid
    // crashes that otherwise occur in the destructor.
    driver_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  void Navigate(NavigationType type) {
    content::MockNavigationHandle navigation_handle(GURL(), main_rfh());
    navigation_handle.set_has_committed(true);
    switch (type) {
      case NavigationType::kNormal:
        break;
      case NavigationType::kSameDocument:
        navigation_handle.set_is_same_document(true);
        break;
      case NavigationType::kServedFromBackForwardCache:
        navigation_handle.set_is_served_from_bfcache(true);
        break;
      case NavigationType::kPrerenderedPageActivation:
        navigation_handle.set_is_prerendered_page_activation(true);
        break;
    }
    driver_->DidNavigateFrame(&navigation_handle);
  }

 protected:
  FormData SeeAddressFormData() {
    FormData form;
    test::CreateTestAddressFormData(&form);
    std::vector<FormData> augmented_forms;
    EXPECT_CALL(*driver_->mock_browser_autofill_manager(), ShouldParseForms(_))
        .WillOnce(DoAll(SaveArg<0>(&augmented_forms), Return(false)));
    driver_->FormsSeen(/*updated_forms=*/{form}, /*removed_forms=*/{});
    return augmented_forms.front();
  }

  bool autofill_across_iframes_ = false;
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<MockAutofillClient> test_autofill_client_;
  std::unique_ptr<ContentAutofillRouter> router_;
  std::unique_ptr<TestContentAutofillDriver> driver_;

  FakeAutofillAgent fake_agent_;
};

TEST_P(ContentAutofillDriverTest, NavigatedMainFrameDifferentDocument) {
  EXPECT_CALL(*driver_->mock_browser_autofill_manager(), Reset());
  Navigate(NavigationType::kNormal);
}

TEST_P(ContentAutofillDriverTest, NavigatedMainFrameSameDocument) {
  EXPECT_CALL(*driver_->mock_browser_autofill_manager(), Reset()).Times(0);
  Navigate(NavigationType::kSameDocument);
}

TEST_P(ContentAutofillDriverTest, NavigatedMainFrameFromBackForwardCache) {
  EXPECT_CALL(*driver_->mock_browser_autofill_manager(), Reset()).Times(0);
  Navigate(NavigationType::kServedFromBackForwardCache);
}

TEST_P(ContentAutofillDriverTest, NavigatedMainFramePrerenderedPageActivation) {
  EXPECT_CALL(*driver_->mock_browser_autofill_manager(), Reset()).Times(0);
  Navigate(NavigationType::kPrerenderedPageActivation);
}

TEST_P(ContentAutofillDriverTest, SetFrameAndFormMetaDataOfForm) {
  NavigateAndCommit(GURL("https://username:password@hostname/path?query#hash"));
  FormData form;
  form.fields.push_back(FormFieldData());
  FormData form2 = ContentAutofillDriverTestApi(driver_.get())
                       .GetFormWithFrameAndFormMetaData(form);
  ContentAutofillDriverTestApi(driver_.get())
      .SetFrameAndFormMetaData(form, nullptr);

  EXPECT_EQ(
      form.host_frame,
      LocalFrameToken(web_contents()->GetMainFrame()->GetFrameToken().value()));
  EXPECT_EQ(form.url, GURL("https://hostname/path"));
  EXPECT_EQ(form.full_url, GURL());
  EXPECT_EQ(form.main_frame_origin,
            web_contents()->GetMainFrame()->GetLastCommittedOrigin());
  EXPECT_EQ(form.main_frame_origin,
            url::Origin::CreateFromNormalizedTuple("https", "hostname", 443));
  ASSERT_EQ(form.fields.size(), 1u);
  EXPECT_EQ(
      form.fields.front().host_frame,
      LocalFrameToken(web_contents()->GetMainFrame()->GetFrameToken().value()));

  EXPECT_EQ(form2.host_frame, form.host_frame);
  EXPECT_EQ(form2.url, form.url);
  EXPECT_EQ(form2.full_url, form.full_url);
  EXPECT_EQ(form2.main_frame_origin, form.main_frame_origin);
  ASSERT_EQ(form2.fields.size(), 1u);
  EXPECT_EQ(form2.fields.front().host_frame, form2.fields.front().host_frame);
}

// Test that forms in "about:" without parents have an empty FormData::url.
TEST_P(ContentAutofillDriverTest, SetFrameAndFormMetaDataOfForm_AboutScheme) {
  NavigateAndCommit(GURL("about:blank"));
  ASSERT_TRUE(main_rfh()->GetLastCommittedURL().IsAboutBlank());

  FormData form;
  ContentAutofillDriverTestApi(driver_.get())
      .SetFrameAndFormMetaData(form, nullptr);

  EXPECT_TRUE(form.url.is_empty());
}

// Test that forms in "about:" subframes inherit the URL of their next
// non-"about:" ancestor in FormData::url.
TEST_P(ContentAutofillDriverTest,
       SetFrameAndFormMetaDataOfForm_AboutSchemeInheritsFromGrandParent) {
  NavigateAndCommit(GURL("https://username:password@hostname/path?query#hash"));
  content::RenderFrameHost* child_rfh =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("about:blank"), content::RenderFrameHostTester::For(main_rfh())
                                   ->AppendChild("child"));
  content::RenderFrameHost* grandchild_rfh =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("about:blank"),
          content::RenderFrameHostTester::For(child_rfh)->AppendChild(
              "grandchild"));
  auto grandchild_driver = std::make_unique<TestContentAutofillDriver>(
      grandchild_rfh, test_autofill_client_.get(), router_.get());
  ASSERT_TRUE(child_rfh->GetLastCommittedURL().IsAboutBlank());
  ASSERT_TRUE(grandchild_rfh->GetLastCommittedURL().IsAboutBlank());

  FormData form;
  ContentAutofillDriverTestApi(grandchild_driver.get())
      .SetFrameAndFormMetaData(form, nullptr);

  EXPECT_EQ(form.url, GURL("https://hostname"));
}

TEST_P(ContentAutofillDriverTest, SetFrameAndFormMetaDataOfField) {
  NavigateAndCommit(GURL("https://username:password@hostname/path?query#hash"));
  // We test that `SetFrameAndFormMetaData(form, &field) sets the meta data not
  // just of |form|'s fields but also of an additional individual |field|.
  FormData form;
  form.fields.push_back(FormFieldData());
  FormFieldData field = form.fields.back();
  FormSignature signature_without_meta_data = CalculateFormSignature(form);
  ContentAutofillDriverTestApi(driver_.get())
      .SetFrameAndFormMetaData(form, &field);

  EXPECT_NE(signature_without_meta_data, CalculateFormSignature(form));
  EXPECT_EQ(
      field.host_frame,
      LocalFrameToken(web_contents()->GetMainFrame()->GetFrameToken().value()));
  EXPECT_EQ(field.host_form_id, form.unique_renderer_id);
  EXPECT_EQ(field.host_form_signature, CalculateFormSignature(form));

  EXPECT_EQ(field.host_frame, form.fields.front().host_frame);
  EXPECT_EQ(field.host_form_id, form.fields.front().host_form_id);
  EXPECT_EQ(field.host_form_signature, form.fields.front().host_form_signature);
}

TEST_P(ContentAutofillDriverTest, FormDataSentToRenderer_FillForm) {
  int input_page_id = autofill_across_iframes_ ? kCrossFrameFill : 42;
  url::Origin triggered_origin;
  FormData input_form_data = SeeAddressFormData();
  for (FormFieldData& field : input_form_data.fields) {
    field.origin = triggered_origin;
    field.value = u"dummy_value";
  }
  base::RunLoop run_loop;
  fake_agent_.SetQuitLoopClosure(run_loop.QuitClosure());
  driver_->FillOrPreviewForm(input_page_id,
                             mojom::RendererFormDataAction::kFill,
                             input_form_data, triggered_origin, {});

  run_loop.RunUntilIdle();

  int output_page_id = 0;
  FormData output_form_data;
  EXPECT_FALSE(fake_agent_.GetAutofillPreviewFormMessage(&output_page_id,
                                                         &output_form_data));
  EXPECT_TRUE(fake_agent_.GetAutofillFillFormMessage(&output_page_id,
                                                     &output_form_data));
  EXPECT_EQ(input_page_id, output_page_id);
  EXPECT_TRUE(test::WithoutUnserializedData(input_form_data)
                  .SameFormAs(output_form_data));
}

TEST_P(ContentAutofillDriverTest, FormDataSentToRenderer_PreviewForm) {
  int input_page_id = autofill_across_iframes_ ? kCrossFrameFill : 42;
  url::Origin triggered_origin;
  FormData input_form_data = SeeAddressFormData();
  for (FormFieldData& field : input_form_data.fields) {
    field.origin = triggered_origin;
    field.value = u"dummy_value";
  }
  ASSERT_TRUE(base::ranges::all_of(input_form_data.fields,
                                   base::not_fn(&std::u16string::empty),
                                   &FormFieldData::value));
  base::RunLoop run_loop;
  fake_agent_.SetQuitLoopClosure(run_loop.QuitClosure());
  driver_->FillOrPreviewForm(input_page_id,
                             mojom::RendererFormDataAction::kPreview,
                             input_form_data, triggered_origin, {});

  run_loop.RunUntilIdle();

  int output_page_id = 0;
  FormData output_form_data;
  EXPECT_FALSE(fake_agent_.GetAutofillFillFormMessage(&output_page_id,
                                                      &output_form_data));
  EXPECT_TRUE(fake_agent_.GetAutofillPreviewFormMessage(&output_page_id,
                                                        &output_form_data));
  EXPECT_EQ(input_page_id, output_page_id);
  EXPECT_TRUE(test::WithoutUnserializedData(input_form_data)
                  .SameFormAs(output_form_data));
}

TEST_P(ContentAutofillDriverTest, TypePredictionsSentToRendererWhenEnabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kShowAutofillTypePredictions);

  FormData form;
  test::CreateTestAddressFormData(&form);

  std::vector<FormData> augmented_forms;
  EXPECT_CALL(*driver_->mock_browser_autofill_manager(), ShouldParseForms(_))
      .WillOnce(DoAll(SaveArg<0>(&augmented_forms), Return(false)));
  driver_->FormsSeen(/*updated_forms=*/{form}, /*removed_forms=*/{});

  ContentAutofillDriverTestApi(driver_.get())
      .SetFrameAndFormMetaData(form, nullptr);
  ASSERT_EQ(augmented_forms.size(), 1u);
  EXPECT_TRUE(augmented_forms.front().SameFormAs(form));

  FormStructure form_structure(form);
  std::vector<FormStructure*> form_structures(1, &form_structure);
  std::vector<FormDataPredictions> expected_type_predictions =
      FormStructure::GetFieldTypePredictions(form_structures);

  base::RunLoop run_loop;
  fake_agent_.SetQuitLoopClosure(run_loop.QuitClosure());
  driver_->SendAutofillTypePredictionsToRenderer(form_structures);
  run_loop.RunUntilIdle();

  std::vector<FormDataPredictions> output_type_predictions;
  EXPECT_TRUE(
      fake_agent_.GetFieldTypePredictionsAvailable(&output_type_predictions));
  EXPECT_EQ(expected_type_predictions, output_type_predictions);
}

TEST_P(ContentAutofillDriverTest, AcceptDataListSuggestion) {
  FieldGlobalId field = SeeAddressFormData().fields.front().global_id();
  std::u16string input_value(u"barfoo");
  std::u16string output_value;

  base::RunLoop run_loop;
  fake_agent_.SetQuitLoopClosure(run_loop.QuitClosure());
  driver_->RendererShouldAcceptDataListSuggestion(field, input_value);
  run_loop.RunUntilIdle();

  EXPECT_TRUE(
      fake_agent_.GetString16AcceptDataListSuggestion(field, &output_value));
  EXPECT_EQ(input_value, output_value);
}

TEST_P(ContentAutofillDriverTest, ClearFilledSectionSentToRenderer) {
  SeeAddressFormData();
  base::RunLoop run_loop;
  fake_agent_.SetQuitLoopClosure(run_loop.QuitClosure());
  driver_->RendererShouldClearFilledSection();
  run_loop.RunUntilIdle();

  EXPECT_TRUE(fake_agent_.GetCalledClearSection());
}

TEST_P(ContentAutofillDriverTest, ClearPreviewedFormSentToRenderer) {
  SeeAddressFormData();
  base::RunLoop run_loop;
  fake_agent_.SetQuitLoopClosure(run_loop.QuitClosure());
  driver_->RendererShouldClearPreviewedForm();
  run_loop.RunUntilIdle();

  EXPECT_TRUE(fake_agent_.GetCalledClearPreviewedForm());
}

TEST_P(ContentAutofillDriverTest, FillFieldWithValue) {
  FieldGlobalId field = SeeAddressFormData().fields.front().global_id();
  std::u16string input_value(u"barqux");
  std::u16string output_value;

  base::RunLoop run_loop;
  fake_agent_.SetQuitLoopClosure(run_loop.QuitClosure());
  driver_->RendererShouldFillFieldWithValue(field, input_value);
  run_loop.RunUntilIdle();

  EXPECT_TRUE(fake_agent_.GetString16FillFieldWithValue(field, &output_value));
  EXPECT_EQ(input_value, output_value);
}

TEST_P(ContentAutofillDriverTest, PreviewFieldWithValue) {
  FieldGlobalId field = SeeAddressFormData().fields.front().global_id();
  std::u16string input_value(u"barqux");
  std::u16string output_value;

  base::RunLoop run_loop;
  fake_agent_.SetQuitLoopClosure(run_loop.QuitClosure());
  driver_->RendererShouldPreviewFieldWithValue(field, input_value);
  run_loop.RunUntilIdle();

  EXPECT_TRUE(
      fake_agent_.GetString16PreviewFieldWithValue(field, &output_value));
  EXPECT_EQ(input_value, output_value);
}

TEST_P(ContentAutofillDriverTest, EnableHeavyFormDataScraping) {
  struct TestCase {
    version_info::Channel channel;
    bool heavy_scraping_enabled;
  } kTestCases[] = {{version_info::Channel::CANARY, true},
                    {version_info::Channel::DEV, true},
                    {version_info::Channel::UNKNOWN, false},
                    {version_info::Channel::BETA, false},
                    {version_info::Channel::STABLE, false}};

  for (auto test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "channel: "
                 << version_info::GetChannelString(test_case.channel));
    test_autofill_client_->set_channel_for_testing(test_case.channel);
    EXPECT_CALL(fake_agent_, EnableHeavyFormDataScraping())
        .Times(test_case.heavy_scraping_enabled ? 1 : 0);

    std::unique_ptr<ContentAutofillDriver> driver(new TestContentAutofillDriver(
        web_contents()->GetMainFrame(), test_autofill_client_.get(),
        router_.get()));

    base::RunLoop().RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(&fake_agent_);
  }
}

INSTANTIATE_TEST_SUITE_P(ContentAutofillDriverTest,
                         ContentAutofillDriverTest,
                         testing::Bool());

}  // namespace autofill
