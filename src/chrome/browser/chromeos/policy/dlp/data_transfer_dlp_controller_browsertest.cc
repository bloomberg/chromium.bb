// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/shell.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/policy/core/user_policy_test_helper.h"
#include "chrome/browser/ash/policy/login/login_policy_test_base.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/dlp/data_transfer_dlp_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_event.pb.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager_test_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_impl.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_test_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "url/origin.h"

// TODO(crbug.com/1262948): Enable and modify for lacros.
namespace policy {

namespace {

constexpr char kClipboardText1[] = "Hello World";
constexpr char16_t kClipboardText116[] = u"Hello World";
constexpr char16_t kClipboardText2[] = u"abcdef";

constexpr char kMailUrl[] = "https://mail.google.com";
constexpr char kDocsUrl[] = "https://docs.google.com";
constexpr char kExampleUrl[] = "https://example.com";

class FakeClipboardNotifier : public DlpClipboardNotifier {
 public:
  views::Widget* GetWidget() { return widget_.get(); }

  void ProceedPressed(const ui::DataTransferEndpoint& data_dst) {
    DlpClipboardNotifier::ProceedPressed(data_dst, GetWidget());
  }

  void BlinkProceedPressed(const ui::DataTransferEndpoint& data_dst) {
    DlpClipboardNotifier::BlinkProceedPressed(data_dst, GetWidget());
  }

  void CancelWarningPressed(const ui::DataTransferEndpoint& data_dst) {
    DlpClipboardNotifier::CancelWarningPressed(data_dst, GetWidget());
  }
};

class FakeDlpController : public DataTransferDlpController,
                          public views::WidgetObserver {
 public:
  FakeDlpController(const DlpRulesManager& dlp_rules_manager,
                    FakeClipboardNotifier* helper)
      : DataTransferDlpController(dlp_rules_manager), helper_(helper) {
    DCHECK(helper);
  }

  ~FakeDlpController() {
    if (widget_ && widget_->HasObserver(this)) {
      widget_->RemoveObserver(this);
    }
  }

  void NotifyBlockedPaste(
      const ui::DataTransferEndpoint* const data_src,
      const ui::DataTransferEndpoint* const data_dst) override {
    helper_->NotifyBlockedAction(data_src, data_dst);
  }

  void WarnOnPaste(const ui::DataTransferEndpoint* const data_src,
                   const ui::DataTransferEndpoint* const data_dst) override {
    helper_->WarnOnPaste(data_src, data_dst);
  }

  void SetBlinkQuitCallback(base::RepeatingClosure cb) {
    blink_quit_cb_ = std::move(cb);
  }

  void WarnOnBlinkPaste(const ui::DataTransferEndpoint* const data_src,
                        const ui::DataTransferEndpoint* const data_dst,
                        content::WebContents* web_contents,
                        base::OnceCallback<void(bool)> paste_cb) override {
    blink_data_dst_.emplace(*data_dst);
    helper_->WarnOnBlinkPaste(data_src, data_dst, web_contents,
                              std::move(paste_cb));
    std::move(blink_quit_cb_).Run();
  }

  bool ShouldPasteOnWarn(
      const ui::DataTransferEndpoint* const data_dst) override {
    if (force_paste_on_warn_)
      return true;
    return helper_->DidUserApproveDst(data_dst);
  }

  bool ObserveWidget() {
    widget_ = helper_->GetWidget();
    if (widget_ && !widget_->HasObserver(this)) {
      widget_->AddObserver(this);
      return true;
    }
    return false;
  }

  MOCK_METHOD1(OnWidgetClosing, void(views::Widget* widget));
  views::Widget* widget_ = nullptr;
  FakeClipboardNotifier* helper_ = nullptr;
  absl::optional<ui::DataTransferEndpoint> blink_data_dst_;
  base::RepeatingClosure blink_quit_cb_ = base::DoNothing();
  bool force_paste_on_warn_ = false;
};

class MockDlpRulesManager : public DlpRulesManagerImpl {
 public:
  explicit MockDlpRulesManager(PrefService* local_state)
      : DlpRulesManagerImpl(local_state) {}
  ~MockDlpRulesManager() override = default;

  MOCK_CONST_METHOD0(GetReportingManager, DlpReportingManager*());
};

void SetClipboardText(std::u16string text,
                      std::unique_ptr<ui::DataTransferEndpoint> source) {
  ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste,
                                   source ? std::move(source) : nullptr);
  writer.WriteText(text);
}

// On Widget Closing, a task for NativeWidgetAura::CloseNow() gets posted. This
// task runs after the widget is destroyed which leads to a crash, that's why
// we need to flush the message loop.
void FlushMessageLoop() {
  base::RunLoop run_loop;
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace

class DataTransferDlpBrowserTest : public LoginPolicyTestBase {
 public:
  DataTransferDlpBrowserTest() = default;

  void SetDlpRulesPolicy(const base::Value& rules) {
    std::string json;
    base::JSONWriter::Write(rules, &json);

    base::DictionaryValue policy;
    policy.SetKey(key::kDataLeakPreventionRulesList, base::Value(json));
    user_policy_helper()->SetPolicyAndWait(
        policy, /*recommended=*/base::DictionaryValue(),
        ProfileManager::GetActiveUserProfile());
  }

  void SetupCrostini() {
    crostini::FakeCrostiniFeatures crostini_features;
    crostini_features.set_is_allowed_now(true);
    crostini_features.set_enabled(true);

    // Setup CrostiniManager for testing.
    crostini::CrostiniManager* crostini_manager =
        crostini::CrostiniManager::GetForProfile(GetProfileForActiveUser());
    crostini_manager->set_skip_restart_for_testing();
    crostini_manager->AddRunningVmForTesting(crostini::kCrostiniDefaultVmName);
    crostini_manager->AddRunningContainerForTesting(
        crostini::kCrostiniDefaultVmName,
        crostini::ContainerInfo(crostini::kCrostiniDefaultContainerName,
                                "testuser", "/home/testuser",
                                "PLACEHOLDER_IP"));
  }

  void SetupTextfield() {
    // Create a widget containing a single, focusable textfield.
    widget_ = std::make_unique<views::Widget>();

    views::Widget::InitParams params;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
    widget_->Init(std::move(params));
    textfield_ = widget_->SetContentsView(std::make_unique<views::Textfield>());
    textfield_->SetAccessibleName(u"Textfield");
    textfield_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

    // Show the widget.
    widget_->SetBounds(gfx::Rect(0, 0, 100, 100));
    widget_->Show();
    ASSERT_TRUE(widget_->IsActive());

    // Focus the textfield and confirm initial state.
    textfield_->RequestFocus();
    ASSERT_TRUE(textfield_->HasFocus());
    ASSERT_TRUE(textfield_->GetText().empty());

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        ash::Shell::GetPrimaryRootWindow());
  }

  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  std::unique_ptr<views::Widget> widget_;
  views::Textfield* textfield_ = nullptr;
};

// Flaky on MSan bots: http://crbug.com/1178328
#if defined(MEMORY_SANITIZER)
#define MAYBE_EmptyPolicy DISABLED_EmptyPolicy
#else
#define MAYBE_EmptyPolicy EmptyPolicy
#endif
IN_PROC_BROWSER_TEST_F(DataTransferDlpBrowserTest, MAYBE_EmptyPolicy) {
  SkipToLoginScreen();
  LogIn();

  SetClipboardText(kClipboardText116, nullptr);

  ui::DataTransferEndpoint data_dst(
      url::Origin::Create(GURL("https://google.com")));
  std::u16string result;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst, &result);
  EXPECT_EQ(kClipboardText116, result);
}

IN_PROC_BROWSER_TEST_F(DataTransferDlpBrowserTest, BlockDestination) {
  SkipToLoginScreen();
  LogIn();

  FakeClipboardNotifier helper;
  FakeDlpController dlp_controller(
      *DlpRulesManagerFactory::GetForPrimaryProfile(), &helper);

  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls1(base::Value::Type::LIST);
  src_urls1.Append(kMailUrl);
  base::Value dst_urls1(base::Value::Type::LIST);
  dst_urls1.Append("*");
  base::Value restrictions1(base::Value::Type::LIST);
  restrictions1.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Block Gmail", std::move(src_urls1), std::move(dst_urls1),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions1)));

  base::Value src_urls2(base::Value::Type::LIST);
  src_urls2.Append(kMailUrl);
  base::Value dst_urls2(base::Value::Type::LIST);
  dst_urls2.Append(kDocsUrl);
  base::Value restrictions2(base::Value::Type::LIST);
  restrictions2.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kAllowLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #2", "Allow Gmail for work purposes", std::move(src_urls2),
      std::move(dst_urls2),
      /*dst_components=*/base::Value(base::Value::Type::LIST),
      std::move(restrictions2)));

  SetDlpRulesPolicy(std::move(rules));

  SetClipboardText(kClipboardText116,
                   std::make_unique<ui::DataTransferEndpoint>(
                       url::Origin::Create(GURL(kMailUrl))));

  ui::DataTransferEndpoint data_dst1(url::Origin::Create(GURL(kMailUrl)));
  std::u16string result1;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst1, &result1);
  EXPECT_EQ(kClipboardText116, result1);

  ui::DataTransferEndpoint data_dst2(url::Origin::Create(GURL(kDocsUrl)));
  std::u16string result2;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst2, &result2);
  EXPECT_EQ(kClipboardText116, result2);

  ui::DataTransferEndpoint data_dst3(url::Origin::Create(GURL(kExampleUrl)));
  std::u16string result3;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst3, &result3);
  EXPECT_EQ(std::u16string(), result3);
  ASSERT_TRUE(dlp_controller.ObserveWidget());

  SetClipboardText(kClipboardText116,
                   std::make_unique<ui::DataTransferEndpoint>(
                       url::Origin::Create(GURL(kExampleUrl))));

  ui::DataTransferEndpoint data_dst4(url::Origin::Create(GURL(kMailUrl)));
  std::u16string result4;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst1, &result4);
  EXPECT_EQ(kClipboardText116, result4);

  FlushMessageLoop();
}

// Flaky on MSan bots: http://crbug.com/1178328
#if defined(MEMORY_SANITIZER)
#define MAYBE_BlockComponent DISABLED_BlockComponent
#else
#define MAYBE_BlockComponent BlockComponent
#endif
IN_PROC_BROWSER_TEST_F(DataTransferDlpBrowserTest, MAYBE_BlockComponent) {
  SkipToLoginScreen();
  LogIn();

  SetupCrostini();

  base::Value rules(base::Value::Type::LIST);

  base::Value src_urls(base::Value::Type::LIST);
  src_urls.Append(kMailUrl);
  base::Value dst_components(base::Value::Type::LIST);
  dst_components.Append(dlp::kArc);
  dst_components.Append(dlp::kCrostini);
  base::Value restrictions(base::Value::Type::LIST);
  restrictions.Append(dlp_test_util::CreateRestrictionWithLevel(
      dlp::kClipboardRestriction, dlp::kBlockLevel));
  rules.Append(dlp_test_util::CreateRule(
      "rule #1", "Block Gmail", std::move(src_urls),
      /*dst_urls=*/base::Value(base::Value::Type::LIST),
      std::move(dst_components), std::move(restrictions)));

  SetDlpRulesPolicy(rules);

  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste,
                                     std::make_unique<ui::DataTransferEndpoint>(
                                         url::Origin::Create(GURL(kMailUrl))));
    writer.WriteText(kClipboardText116);
  }
  ui::DataTransferEndpoint data_dst1(ui::EndpointType::kDefault);
  std::u16string result1;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst1, &result1);
  EXPECT_EQ(kClipboardText116, result1);

  ui::DataTransferEndpoint data_dst2(ui::EndpointType::kArc);
  std::u16string result2;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst2, &result2);
  EXPECT_EQ(std::u16string(), result2);

  ui::DataTransferEndpoint data_dst3(ui::EndpointType::kCrostini);
  std::u16string result3;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst3, &result3);
  EXPECT_EQ(std::u16string(), result3);
}

// Flaky on MSan bots: http://crbug.com/1178328
#if defined(MEMORY_SANITIZER)
#define MAYBE_WarnDestination DISABLED_WarnDestination
#else
#define MAYBE_WarnDestination WarnDestination
#endif
IN_PROC_BROWSER_TEST_F(DataTransferDlpBrowserTest, MAYBE_WarnDestination) {
  SkipToLoginScreen();
  LogIn();

  FakeClipboardNotifier helper;
  FakeDlpController dlp_controller(
      *DlpRulesManagerFactory::GetForPrimaryProfile(), &helper);

  {
    ListPrefUpdate update(g_browser_process->local_state(),
                          policy_prefs::kDlpRulesList);
    base::Value rule(base::Value::Type::DICTIONARY);
    base::Value src_urls(base::Value::Type::DICTIONARY);
    base::Value src_urls_list(base::Value::Type::LIST);
    src_urls_list.Append(base::Value(kMailUrl));
    src_urls.SetKey("urls", std::move(src_urls_list));
    rule.SetKey("sources", std::move(src_urls));

    base::Value dst_urls(base::Value::Type::DICTIONARY);
    base::Value dst_urls_list(base::Value::Type::LIST);
    dst_urls_list.Append(base::Value("*"));
    dst_urls.SetKey("urls", std::move(dst_urls_list));
    rule.SetKey("destinations", std::move(dst_urls));

    base::Value restrictions(base::Value::Type::DICTIONARY);
    base::Value restrictions_list(base::Value::Type::LIST);
    base::Value class_level_dict(base::Value::Type::DICTIONARY);
    class_level_dict.SetKey("class", base::Value("CLIPBOARD"));
    class_level_dict.SetKey("level", base::Value("WARN"));
    restrictions_list.Append(std::move(class_level_dict));
    rule.SetKey("restrictions", std::move(restrictions_list));

    update->Append(std::move(rule));
  }

  SetClipboardText(kClipboardText116,
                   std::make_unique<ui::DataTransferEndpoint>(
                       url::Origin::Create(GURL(kMailUrl))));

  SetupTextfield();
  // Initiate a paste on textfield_.
  event_generator_->PressKey(ui::VKEY_V, ui::EF_CONTROL_DOWN);
  event_generator_->ReleaseKey(ui::VKEY_V, ui::EF_CONTROL_DOWN);

  EXPECT_EQ("", base::UTF16ToUTF8(textfield_->GetText()));
  ASSERT_TRUE(dlp_controller.ObserveWidget());

  // Accept warning.
  EXPECT_CALL(dlp_controller, OnWidgetClosing);
  ui::DataTransferEndpoint default_endpoint(ui::EndpointType::kDefault);
  helper.ProceedPressed(default_endpoint);
  testing::Mock::VerifyAndClearExpectations(&dlp_controller);

  EXPECT_EQ(kClipboardText116, textfield_->GetText());

  SetClipboardText(kClipboardText2, std::make_unique<ui::DataTransferEndpoint>(
                                        url::Origin::Create(GURL(kMailUrl))));

  // Initiate a paste on textfield_.
  textfield_->SetText(std::u16string());
  textfield_->RequestFocus();
  event_generator_->PressKey(ui::VKEY_V, ui::EF_CONTROL_DOWN);
  event_generator_->ReleaseKey(ui::VKEY_V, ui::EF_CONTROL_DOWN);
  testing::Mock::VerifyAndClearExpectations(&dlp_controller);

  EXPECT_EQ("", base::UTF16ToUTF8(textfield_->GetText()));
  ASSERT_TRUE(dlp_controller.ObserveWidget());

  // Initiate a paste on nullptr data_dst.
  std::u16string result;
  EXPECT_CALL(dlp_controller, OnWidgetClosing);
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, nullptr, &result);
  testing::Mock::VerifyAndClearExpectations(&dlp_controller);

  EXPECT_EQ(std::u16string(), result);
  ASSERT_TRUE(dlp_controller.ObserveWidget());

  FlushMessageLoop();
}

// Flaky on MSan bots: http://crbug.com/1178328
#if defined(MEMORY_SANITIZER)
#define MAYBE_WarnComponent DISABLED_WarnComponent
#else
#define MAYBE_WarnComponent WarnComponent
#endif
IN_PROC_BROWSER_TEST_F(DataTransferDlpBrowserTest, MAYBE_WarnComponent) {
  SkipToLoginScreen();
  LogIn();

  SetupCrostini();

  {
    ListPrefUpdate update(g_browser_process->local_state(),
                          policy_prefs::kDlpRulesList);
    base::Value rule(base::Value::Type::DICTIONARY);
    base::Value src_urls(base::Value::Type::DICTIONARY);
    base::Value src_urls_list(base::Value::Type::LIST);
    src_urls_list.Append(base::Value(kMailUrl));
    src_urls.SetKey("urls", std::move(src_urls_list));
    rule.SetKey("sources", std::move(src_urls));

    base::Value dst_components(base::Value::Type::DICTIONARY);
    base::Value dst_components_list(base::Value::Type::LIST);
    dst_components_list.Append(base::Value("ARC"));
    dst_components_list.Append(base::Value("CROSTINI"));
    dst_components_list.Append(base::Value("PLUGIN_VM"));
    dst_components.SetKey("components", std::move(dst_components_list));
    rule.SetKey("destinations", std::move(dst_components));

    base::Value restrictions(base::Value::Type::DICTIONARY);
    base::Value restrictions_list(base::Value::Type::LIST);
    base::Value class_level_dict(base::Value::Type::DICTIONARY);
    class_level_dict.SetKey("class", base::Value("CLIPBOARD"));
    class_level_dict.SetKey("level", base::Value("WARN"));
    restrictions_list.Append(std::move(class_level_dict));
    rule.SetKey("restrictions", std::move(restrictions_list));

    update->Append(std::move(rule));
  }

  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste,
                                     std::make_unique<ui::DataTransferEndpoint>(
                                         url::Origin::Create(GURL(kMailUrl))));
    writer.WriteText(kClipboardText116);
  }

  ui::DataTransferEndpoint arc_endpoint(ui::EndpointType::kArc);
  std::u16string result;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &arc_endpoint, &result);
  EXPECT_EQ(kClipboardText116, result);

  ui::DataTransferEndpoint crostini_endpoint(ui::EndpointType::kCrostini);
  result.clear();
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &crostini_endpoint, &result);
  EXPECT_EQ(kClipboardText116, result);
}

class DataTransferDlpBlinkBrowserTest : public InProcessBrowserTest {
 public:
  DataTransferDlpBlinkBrowserTest() = default;
  DataTransferDlpBlinkBrowserTest(const DataTransferDlpBlinkBrowserTest&) =
      delete;
  DataTransferDlpBlinkBrowserTest& operator=(
      const DataTransferDlpBlinkBrowserTest&) = delete;
  ~DataTransferDlpBlinkBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    rules_manager_ = std::make_unique<::testing::NiceMock<MockDlpRulesManager>>(
        g_browser_process->local_state());

    reporting_manager_ = std::make_unique<DlpReportingManager>();
    SetReportQueueForReportingManager(reporting_manager_.get(), events,
                                      base::SequencedTaskRunnerHandle::Get());
    ON_CALL(*rules_manager_, GetReportingManager)
        .WillByDefault(::testing::Return(reporting_manager_.get()));

    dlp_controller_ =
        std::make_unique<FakeDlpController>(*rules_manager_, &helper);
  }

  void TearDownOnMainThread() override {
    dlp_controller_.reset();
    reporting_manager_.reset();
    rules_manager_.reset();
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  ::testing::AssertionResult ExecJs(content::WebContents* web_contents,
                                    const std::string& code) {
    return content::ExecJs(web_contents, code,
                           content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                           /*world_id=*/1);
  }

  content::EvalJsResult EvalJs(content::WebContents* web_contents,
                               const std::string& code) {
    return content::EvalJs(web_contents, code,
                           content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                           /*world_id=*/1);
  }

  std::unique_ptr<::testing::NiceMock<MockDlpRulesManager>> rules_manager_;
  std::unique_ptr<DlpReportingManager> reporting_manager_;
  std::vector<DlpPolicyEvent> events;
  FakeClipboardNotifier helper;
  std::unique_ptr<FakeDlpController> dlp_controller_;
};

// Flaky on MSan bots: crbug.com/1230617
#if defined(MEMORY_SANITIZER)
#define MAYBE_ProceedOnWarn DISABLED_ProceedOnWarn
#else
#define MAYBE_ProceedOnWarn ProceedOnWarn
#endif
IN_PROC_BROWSER_TEST_F(DataTransferDlpBlinkBrowserTest, MAYBE_ProceedOnWarn) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  // TODO(1276069): Refactor duplicated code below.
  {
    ListPrefUpdate update(g_browser_process->local_state(),
                          policy_prefs::kDlpRulesList);
    base::Value rule(base::Value::Type::DICTIONARY);
    base::Value src_urls(base::Value::Type::DICTIONARY);
    base::Value src_urls_list(base::Value::Type::LIST);
    src_urls_list.Append(base::Value(kMailUrl));
    src_urls.SetKey("urls", std::move(src_urls_list));
    rule.SetKey("sources", std::move(src_urls));

    base::Value dst_urls(base::Value::Type::DICTIONARY);
    base::Value dst_urls_list(base::Value::Type::LIST);
    dst_urls_list.Append(base::Value("*"));
    dst_urls.SetKey("urls", std::move(dst_urls_list));
    rule.SetKey("destinations", std::move(dst_urls));

    base::Value restrictions(base::Value::Type::DICTIONARY);
    base::Value restrictions_list(base::Value::Type::LIST);
    base::Value class_level_dict(base::Value::Type::DICTIONARY);
    class_level_dict.SetKey("class", base::Value("CLIPBOARD"));
    class_level_dict.SetKey("level", base::Value("WARN"));
    restrictions_list.Append(std::move(class_level_dict));
    rule.SetKey("restrictions", std::move(restrictions_list));

    update->Append(std::move(rule));
  }

  SetClipboardText(kClipboardText116,
                   std::make_unique<ui::DataTransferEndpoint>(
                       url::Origin::Create(GURL(kMailUrl))));

  EXPECT_TRUE(
      ExecJs(GetActiveWebContents(),
             "var p = new Promise((resolve, reject) => {"
             "  window.document.onpaste = async (event) => {"
             "    if (event.clipboardData.items.length !== 1) {"
             "      reject('There were ' + event.clipboardData.items.length +"
             "             ' clipboard items. Expected 1.');"
             "    }"
             "    if (event.clipboardData.items[0].kind != 'string') {"
             "      reject('The clipboard item was of kind: ' +"
             "             event.clipboardData.items[0].kind + '. Expected ' +"
             "             'string.');"
             "    }"
             "    const clipboardDataItem = event.clipboardData.items[0];"
             "    clipboardDataItem.getAsString((clipboardDataText)=> {"
             "      resolve(clipboardDataText);});"
             "  };"
             "});"));

  content::UpdateUserActivationStateInterceptor user_activation_interceptor(
      GetActiveWebContents()->GetMainFrame());
  user_activation_interceptor.UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);

  // Send paste event and wait till the notification is displayed.
  base::RunLoop run_loop;
  dlp_controller_->SetBlinkQuitCallback(run_loop.QuitClosure());
  GetActiveWebContents()->Paste();
  run_loop.Run();

  ASSERT_TRUE(dlp_controller_->ObserveWidget());
  EXPECT_EQ(events.size(), 1u);
  EXPECT_THAT(events[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kMailUrl, "*", DlpRulesManager::Restriction::kClipboard,
                  DlpRulesManager::Level::kWarn)));

  EXPECT_CALL(*dlp_controller_, OnWidgetClosing);
  ASSERT_TRUE(dlp_controller_->blink_data_dst_.has_value());
  helper.BlinkProceedPressed(dlp_controller_->blink_data_dst_.value());

  EXPECT_EQ(kClipboardText1, EvalJs(GetActiveWebContents(), "p"));
  EXPECT_EQ(events.size(), 2u);
  EXPECT_THAT(events[1],
              IsDlpPolicyEvent(CreateDlpPolicyWarningProceededEvent(
                  kMailUrl, "*", DlpRulesManager::Restriction::kClipboard)));

  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);
}

// Flaky on MSan bots: crbug.com/1230617
#if defined(MEMORY_SANITIZER)
#define MAYBE_CancelWarn DISABLED_CancelWarn
#else
#define MAYBE_CancelWarn CancelWarn
#endif
IN_PROC_BROWSER_TEST_F(DataTransferDlpBlinkBrowserTest, MAYBE_CancelWarn) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  // TODO(1276069): Refactor duplicated code below.
  {
    ListPrefUpdate update(g_browser_process->local_state(),
                          policy_prefs::kDlpRulesList);
    base::Value rule(base::Value::Type::DICTIONARY);
    base::Value src_urls(base::Value::Type::DICTIONARY);
    base::Value src_urls_list(base::Value::Type::LIST);
    src_urls_list.Append(base::Value(kMailUrl));
    src_urls.SetKey("urls", std::move(src_urls_list));
    rule.SetKey("sources", std::move(src_urls));

    base::Value dst_urls(base::Value::Type::DICTIONARY);
    base::Value dst_urls_list(base::Value::Type::LIST);
    dst_urls_list.Append(base::Value("*"));
    dst_urls.SetKey("urls", std::move(dst_urls_list));
    rule.SetKey("destinations", std::move(dst_urls));

    base::Value restrictions(base::Value::Type::DICTIONARY);
    base::Value restrictions_list(base::Value::Type::LIST);
    base::Value class_level_dict(base::Value::Type::DICTIONARY);
    class_level_dict.SetKey("class", base::Value("CLIPBOARD"));
    class_level_dict.SetKey("level", base::Value("WARN"));
    restrictions_list.Append(std::move(class_level_dict));
    rule.SetKey("restrictions", std::move(restrictions_list));

    update->Append(std::move(rule));
  }

  SetClipboardText(kClipboardText116,
                   std::make_unique<ui::DataTransferEndpoint>(
                       url::Origin::Create(GURL(kMailUrl))));

  EXPECT_TRUE(
      ExecJs(GetActiveWebContents(),
             "var p = new Promise((resolve, reject) => {"
             "  window.document.onpaste = async (event) => {"
             "    if (event.clipboardData.items.length !== 1) {"
             "      reject('There were ' + event.clipboardData.items.length +"
             "             ' clipboard items. Expected 1.');"
             "    }"
             "    if (event.clipboardData.items[0].kind != 'string') {"
             "      reject('The clipboard item was of kind: ' +"
             "             event.clipboardData.items[0].kind + '. Expected ' +"
             "             'string.');"
             "    }"
             "    const clipboardDataItem = event.clipboardData.items[0];"
             "    clipboardDataItem.getAsString((clipboardDataText)=> {"
             "      resolve(clipboardDataText);});"
             "  };"
             "});"));

  content::UpdateUserActivationStateInterceptor user_activation_interceptor(
      GetActiveWebContents()->GetMainFrame());
  user_activation_interceptor.UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);

  // Send paste event and wait till the notification is displayed.
  base::RunLoop run_loop;
  dlp_controller_->SetBlinkQuitCallback(run_loop.QuitClosure());
  GetActiveWebContents()->Paste();
  run_loop.Run();

  ASSERT_TRUE(dlp_controller_->ObserveWidget());
  ASSERT_TRUE(dlp_controller_->blink_data_dst_.has_value());
  EXPECT_EQ(events.size(), 1u);
  EXPECT_THAT(events[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kMailUrl, "*", DlpRulesManager::Restriction::kClipboard,
                  DlpRulesManager::Level::kWarn)));

  EXPECT_CALL(*dlp_controller_, OnWidgetClosing);
  helper.CancelWarningPressed(dlp_controller_->blink_data_dst_.value());

  EXPECT_EQ("", EvalJs(GetActiveWebContents(), "p"));
  EXPECT_EQ(events.size(), 1u);

  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);
}

#if defined(MEMORY_SANITIZER)
#define MAYBE_ShouldProceedWarn DISABLED_ShouldProceedWarn
#else
#define MAYBE_ShouldProceedWarn ShouldProceedWarn
#endif
IN_PROC_BROWSER_TEST_F(DataTransferDlpBlinkBrowserTest,
                       MAYBE_ShouldProceedWarn) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  // TODO(1276069): Refactor duplicated code below.
  {
    ListPrefUpdate update(g_browser_process->local_state(),
                          policy_prefs::kDlpRulesList);
    base::Value rule(base::Value::Type::DICTIONARY);
    base::Value src_urls(base::Value::Type::DICTIONARY);
    base::Value src_urls_list(base::Value::Type::LIST);
    src_urls_list.Append(base::Value(kMailUrl));
    src_urls.SetKey("urls", std::move(src_urls_list));
    rule.SetKey("sources", std::move(src_urls));

    base::Value dst_urls(base::Value::Type::DICTIONARY);
    base::Value dst_urls_list(base::Value::Type::LIST);
    dst_urls_list.Append(base::Value("*"));
    dst_urls.SetKey("urls", std::move(dst_urls_list));
    rule.SetKey("destinations", std::move(dst_urls));

    base::Value restrictions(base::Value::Type::DICTIONARY);
    base::Value restrictions_list(base::Value::Type::LIST);
    base::Value class_level_dict(base::Value::Type::DICTIONARY);
    class_level_dict.SetKey("class", base::Value("CLIPBOARD"));
    class_level_dict.SetKey("level", base::Value("WARN"));
    restrictions_list.Append(std::move(class_level_dict));
    rule.SetKey("restrictions", std::move(restrictions_list));

    update->Append(std::move(rule));
  }

  SetClipboardText(kClipboardText116,
                   std::make_unique<ui::DataTransferEndpoint>(
                       url::Origin::Create(GURL(kMailUrl))));

  EXPECT_TRUE(
      ExecJs(GetActiveWebContents(),
             "var p = new Promise((resolve, reject) => {"
             "  window.document.onpaste = async (event) => {"
             "    if (event.clipboardData.items.length !== 1) {"
             "      reject('There were ' + event.clipboardData.items.length +"
             "             ' clipboard items. Expected 1.');"
             "    }"
             "    if (event.clipboardData.items[0].kind != 'string') {"
             "      reject('The clipboard item was of kind: ' +"
             "             event.clipboardData.items[0].kind + '. Expected ' +"
             "             'string.');"
             "    }"
             "    const clipboardDataItem = event.clipboardData.items[0];"
             "    clipboardDataItem.getAsString((clipboardDataText)=> {"
             "      resolve(clipboardDataText);});"
             "  };"
             "});"));

  content::UpdateUserActivationStateInterceptor user_activation_interceptor(
      GetActiveWebContents()->GetMainFrame());
  user_activation_interceptor.UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);

  dlp_controller_->force_paste_on_warn_ = true;
  GetActiveWebContents()->Paste();
  EXPECT_FALSE(dlp_controller_->ObserveWidget());
  EXPECT_EQ(kClipboardText1, EvalJs(GetActiveWebContents(), "p"));

  EXPECT_EQ(events.size(), 1u);
  EXPECT_THAT(events[0],
              IsDlpPolicyEvent(CreateDlpPolicyWarningProceededEvent(
                  kMailUrl, "*", DlpRulesManager::Restriction::kClipboard)));

  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);
}

// Test case for crbug.com/1213143
// Flaky on MSan bots: crbug.com/1230617
// Also flaky on linux-chromeos-dbg: crbug.com/1281659
IN_PROC_BROWSER_TEST_F(DataTransferDlpBlinkBrowserTest, DISABLED_Reporting) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  // TODO(1276069): Refactor duplicated code below.
  {
    ListPrefUpdate update(g_browser_process->local_state(),
                          policy_prefs::kDlpRulesList);
    base::Value rule(base::Value::Type::DICTIONARY);
    base::Value src_urls(base::Value::Type::DICTIONARY);
    base::Value src_urls_list(base::Value::Type::LIST);
    src_urls_list.Append(base::Value(kMailUrl));
    src_urls.SetKey("urls", std::move(src_urls_list));
    rule.SetKey("sources", std::move(src_urls));

    base::Value dst_urls(base::Value::Type::DICTIONARY);
    base::Value dst_urls_list(base::Value::Type::LIST);
    dst_urls_list.Append(base::Value("*"));
    dst_urls.SetKey("urls", std::move(dst_urls_list));
    rule.SetKey("destinations", std::move(dst_urls));

    base::Value restrictions(base::Value::Type::DICTIONARY);
    base::Value restrictions_list(base::Value::Type::LIST);
    base::Value class_level_dict(base::Value::Type::DICTIONARY);
    class_level_dict.SetKey("class", base::Value("CLIPBOARD"));
    class_level_dict.SetKey("level", base::Value("REPORT"));
    restrictions_list.Append(std::move(class_level_dict));
    rule.SetKey("restrictions", std::move(restrictions_list));

    update->Append(std::move(rule));
  }

  SetClipboardText(kClipboardText116,
                   std::make_unique<ui::DataTransferEndpoint>(
                       url::Origin::Create(GURL(kMailUrl))));

  EXPECT_TRUE(
      ExecJs(GetActiveWebContents(),
             "var p = new Promise((resolve, reject) => {"
             "  window.document.onpaste = async (event) => {"
             "    if (event.clipboardData.items.length !== 1) {"
             "      reject('There were ' + event.clipboardData.items.length +"
             "             ' clipboard items. Expected 1.');"
             "    }"
             "    if (event.clipboardData.items[0].kind != 'string') {"
             "      reject('The clipboard item was of kind: ' +"
             "             event.clipboardData.items[0].kind + '. Expected ' +"
             "             'string.');"
             "    }"
             "    const clipboardDataItem = event.clipboardData.items[0];"
             "    clipboardDataItem.getAsString((clipboardDataText)=> {"
             "      resolve(clipboardDataText);});"
             "  };"
             "});"));

  content::UpdateUserActivationStateInterceptor user_activation_interceptor(
      GetActiveWebContents()->GetMainFrame());
  user_activation_interceptor.UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);

  GetActiveWebContents()->Paste();
  EXPECT_FALSE(dlp_controller_->ObserveWidget());
  EXPECT_EQ(kClipboardText1, EvalJs(GetActiveWebContents(), "p"));

  EXPECT_EQ(events.size(), 1u);
  EXPECT_THAT(events[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kMailUrl, "*", DlpRulesManager::Restriction::kClipboard,
                  DlpRulesManager::Level::kReport)));
  // TODO(1276063): This EXPECT_GE is always true, because it is compared to 0.
  // The histogram sum may not have any samples when the time difference is very
  // small (almost 0), because UmaHistogramTimes requires the time difference to
  // be >= 1.
  EXPECT_GE(
      histogram_tester.GetTotalSum(GetDlpHistogramPrefix() +
                                   dlp::kDataTransferReportingTimeDiffUMA),
      0);
}

}  // namespace policy
