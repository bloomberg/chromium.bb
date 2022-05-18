// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/dictation.h"

#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/hash/hash.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/accessibility/dictation_bubble_test_helper.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/browser/ash/input_method/textinput_test_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/speech_recognition_constants.h"
#include "chrome/browser/speech/speech_recognition_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/soda/soda_installer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fake_speech_recognition_manager.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/mock_ime_input_context_handler.h"
#include "ui/base/ime/fake_text_input_client.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {

constexpr int kPrintErrorMessageDelayMs = 3500;

const char kFirstSpeechResult[] = "help";
const char16_t kFirstSpeechResult16[] = u"help";
const char kFinalSpeechResult[] = "hello world";
const char16_t kFinalSpeechResult16[] = u"hello world";
const char16_t kTrySaying[] = u"Try saying:";
const char16_t kType[] = u"\"Type [word / phrase]\"";
const char16_t kHelp[] = u"\"Help\"";
const char16_t kUndo[] = u"\"Undo\"";
const char16_t kDelete[] = u"\"Delete\"";
const char16_t kSelectAll[] = u"\"Select all\"";
const char16_t kUnselect[] = u"\"Unselect\"";
const char16_t kCopy[] = u"\"Copy\"";
const char* kOnDeviceListeningDurationMetric =
    "Accessibility.CrosDictation.ListeningDuration.OnDeviceRecognition";
const char* kNetworkListeningDurationMetric =
    "Accessibility.CrosDictation.ListeningDuration.NetworkRecognition";
const char* kLocaleMetric = "Accessibility.CrosDictation.Language";
const char* kOnDeviceSpeechMetric =
    "Accessibility.CrosDictation.UsedOnDeviceSpeech";
const char* kMacroRecognizedMetric =
    "Accessibility.CrosDictation.MacroRecognized";
const char* kMacroSucceededMetric =
    "Accessibility.CrosDictation.MacroSucceeded";
const char* kMacroFailedMetric = "Accessibility.CrosDictation.MacroFailed";
const int kInputTextViewMetricValue = 1;

static const char* kEnglishDictationCommands[] = {
    "delete",
    "move to the previous character",
    "move to the next character",
    "move to the previous line",
    "move to the next line",
    "copy",
    "paste",
    "cut",
    "undo",
    "redo",
    "select all",
    "unselect",
    "help",
    "new line"};

PrefService* GetActiveUserPrefs() {
  return ProfileManager::GetActiveUserProfile()->GetPrefs();
}

AccessibilityManager* GetManager() {
  return AccessibilityManager::Get();
}

void EnableChromeVox() {
  GetManager()->EnableSpokenFeedback(true);
}

std::string ToString(DictationBubbleIconType icon) {
  switch (icon) {
    case DictationBubbleIconType::kHidden:
      return "hidden";
    case DictationBubbleIconType::kStandby:
      return "standby";
    case DictationBubbleIconType::kMacroSuccess:
      return "macro success";
    case DictationBubbleIconType::kMacroFail:
      return "macro fail";
  }
}

// Listens for changes to the histogram provided at construction. This class
// only allows `Wait()` to be called once. If you need to call `Wait()` multiple
// times, create multiple instances of this class.
class HistogramWaiter {
 public:
  explicit HistogramWaiter(const char* metric_name) {
    histogram_observer_ = std::make_unique<
        base::StatisticsRecorder::ScopedHistogramSampleObserver>(
        metric_name, base::BindRepeating(&HistogramWaiter::OnHistogramCallback,
                                         base::Unretained(this)));
  }
  ~HistogramWaiter() { histogram_observer_.reset(); }

  HistogramWaiter(const HistogramWaiter&) = delete;
  HistogramWaiter& operator=(const HistogramWaiter&) = delete;

  // Waits for the next update to the observed histogram.
  void Wait() { run_loop_.Run(); }

  void OnHistogramCallback(const char* metric_name,
                           uint64_t name_hash,
                           base::HistogramBase::Sample sample) {
    run_loop_.Quit();
    histogram_observer_.reset();
  }

 private:
  std::unique_ptr<base::StatisticsRecorder::ScopedHistogramSampleObserver>
      histogram_observer_;
  base::RunLoop run_loop_;
};

// A class that repeatedly runs a function, which is supplied at construction,
// until it evaluates to true.
class SuccessWaiter {
 public:
  SuccessWaiter(const base::RepeatingCallback<bool()>& is_success,
                const std::string& error_message)
      : is_success_(is_success), error_message_(error_message) {}
  ~SuccessWaiter() = default;
  SuccessWaiter(const SuccessWaiter&) = delete;
  SuccessWaiter& operator=(const SuccessWaiter&) = delete;

  void Wait() {
    timer_.Start(FROM_HERE, base::Milliseconds(200), this,
                 &SuccessWaiter::OnTimer);
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SuccessWaiter::MaybePrintErrorMessage,
                       weak_factory_.GetWeakPtr()),
        base::Milliseconds(kPrintErrorMessageDelayMs));
    run_loop_.Run();
    ASSERT_TRUE(is_success_.Run());
  }

  void OnTimer() {
    if (is_success_.Run()) {
      timer_.Stop();
      run_loop_.Quit();
    }
  }

  void MaybePrintErrorMessage() {
    if (!timer_.IsRunning() || run_loop_.AnyQuitCalled() || is_success_.Run())
      return;

    LOG(ERROR) << "Still waiting for SuccessWaiter\n"
               << "SuccessWaiter error message: " << error_message_ << "\n";
  }

 private:
  base::RepeatingCallback<bool()> is_success_;
  std::string error_message_;
  base::RepeatingTimer timer_;
  base::RunLoop run_loop_;
  base::WeakPtrFactory<SuccessWaiter> weak_factory_{this};
};

class CaretBoundsChangedWaiter : public ui::InputMethodObserver {
 public:
  explicit CaretBoundsChangedWaiter(ui::InputMethod* input_method)
      : input_method_(input_method) {
    input_method_->AddObserver(this);
  }
  CaretBoundsChangedWaiter(const CaretBoundsChangedWaiter&) = delete;
  CaretBoundsChangedWaiter& operator=(const CaretBoundsChangedWaiter&) = delete;
  ~CaretBoundsChangedWaiter() override { input_method_->RemoveObserver(this); }

  void Wait() { run_loop_.Run(); }

 private:
  // ui::InputMethodObserver:
  void OnFocus() override {}
  void OnBlur() override {}
  void OnTextInputStateChanged(const ui::TextInputClient* client) override {}
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {
    run_loop_.Quit();
  }

  ui::InputMethod* input_method_;
  base::RunLoop run_loop_;
};

// Listens for changes to the clipboard. This class only allows `Wait()` to be
// called once. If you need to call `Wait()` multiple times, create multiple
// instances of this class.
class ClipboardChangedWaiter : public ui::ClipboardObserver {
 public:
  ClipboardChangedWaiter() {
    ui::ClipboardMonitor::GetInstance()->AddObserver(this);
  }
  ClipboardChangedWaiter(const ClipboardChangedWaiter&) = delete;
  ClipboardChangedWaiter& operator=(const ClipboardChangedWaiter&) = delete;
  ~ClipboardChangedWaiter() override {
    ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

 private:
  // ui::ClipboardObserver:
  void OnClipboardDataChanged() override { run_loop_.Quit(); }

  base::RunLoop run_loop_;
};

}  // namespace

class DictationTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<speech::SpeechRecognitionType> {
 protected:
  DictationTest() : test_helper_(GetParam()) {}
  ~DictationTest() override = default;
  DictationTest(const DictationTest&) = delete;
  DictationTest& operator=(const DictationTest&) = delete;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::vector<base::Feature> enabled_features =
        test_helper_.GetEnabledFeatures();
    std::vector<base::Feature> disabled_features =
        test_helper_.GetDisabledFeatures();
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    test_helper_.SetUp(browser()->profile());
    // Ensure that the Dictation locale preference is set.
    GetActiveUserPrefs()->SetString(prefs::kAccessibilityDictationLocale,
                                    "en-US");

    ASSERT_FALSE(AccessibilityManager::Get()->IsDictationEnabled());
    console_observer_ = std::make_unique<ExtensionConsoleErrorObserver>(
        browser()->profile(), extension_misc::kAccessibilityCommonExtensionId);
    browser()->profile()->GetPrefs()->SetBoolean(
        ash::prefs::kDictationAcceleratorDialogHasBeenAccepted, true);

    extensions::ExtensionHostTestHelper host_helper(
        browser()->profile(), extension_misc::kAccessibilityCommonExtensionId);
    AccessibilityManager::Get()->SetDictationEnabled(true);
    host_helper.WaitForHostCompletedFirstLoad();

    aura::Window* root_window = Shell::Get()->GetPrimaryRootWindow();
    generator_ = std::make_unique<ui::test::EventGenerator>(root_window);

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        GURL(
            "data:text/html;charset=utf-8,<textarea id=textarea></textarea>")));
    // Put focus in the text box.
    ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        nullptr, ui::KeyboardCode::VKEY_TAB, false, false, false, false)));
  }

  void TearDownOnMainThread() override {
    if (GetParam() == speech::SpeechRecognitionType::kNetwork)
      content::SpeechRecognitionManager::SetManagerForTesting(nullptr);

    InProcessBrowserTest::TearDownOnMainThread();
  }

  // Routers to SpeechRecognitionTestHelper methods.
  void WaitForRecognitionStarted() { test_helper_.WaitForRecognitionStarted(); }

  void WaitForRecognitionStopped() { test_helper_.WaitForRecognitionStopped(); }

  void SendInterimResultAndWait(const std::string& transcript) {
    test_helper_.SendInterimResultAndWait(transcript);
  }

  void SendFinalResultAndWait(const std::string& transcript) {
    test_helper_.SendFinalResultAndWait(transcript);
  }

  void SendErrorAndWait() { test_helper_.SendErrorAndWait(); }

  void SendFinalResultAndWaitForTextAreaValue(const std::string& result,
                                              const std::string& value) {
    // Ensure that the accessibility tree and the text area value are updated.
    content::AccessibilityNotificationWaiter waiter(
        browser()->tab_strip_model()->GetActiveWebContents(),
        ui::kAXModeComplete, ax::mojom::Event::kValueChanged);
    SendFinalResultAndWait(result);
    waiter.WaitForNotification();
    WaitForTextAreaValue(value);
  }

  void SendFinalResultAndWaitForSelectionChanged(
      const std::string& result,
      content::WebContents* web_contents) {
    content::AccessibilityNotificationWaiter selection_waiter(
        browser()->tab_strip_model()->GetActiveWebContents(),
        ui::kAXModeComplete, ax::mojom::Event::kTextSelectionChanged);
    content::BoundingBoxUpdateWaiter bounding_box_waiter(web_contents);
    SendFinalResultAndWait(result);
    bounding_box_waiter.Wait();
    selection_waiter.WaitForNotification();
  }

  void SendFinalResultAndWaitForCaretBoundsChanged(const std::string& result) {
    content::AccessibilityNotificationWaiter selection_waiter(
        browser()->tab_strip_model()->GetActiveWebContents(),
        ui::kAXModeComplete, ax::mojom::Event::kTextSelectionChanged);
    CaretBoundsChangedWaiter caret_waiter(
        browser()->window()->GetNativeWindow()->GetHost()->GetInputMethod());
    SendFinalResultAndWait(result);
    caret_waiter.Wait();
    selection_waiter.WaitForNotification();
  }

  void SendFinalResultAndWaitForClipboardChanged(const std::string& result) {
    ClipboardChangedWaiter waiter;
    SendFinalResultAndWait(result);
    waiter.Wait();
  }

  std::string GetTextAreaValue() {
    std::string output;
    std::string script =
        "window.domAutomationController.send("
        "document.getElementById('textarea').value)";
    CHECK(ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetWebContentsAt(0), script, &output));
    return output;
  }

  void WaitForTextAreaValue(const std::string& value) {
    std::string error_message = "Still waiting for text area value: " + value;
    SuccessWaiter(base::BindLambdaForTesting(
                      [&]() { return value == GetTextAreaValue(); }),
                  error_message)
        .Wait();
  }

  void ToggleDictationWithKeystroke() {
    ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        nullptr, ui::KeyboardCode::VKEY_D, false, false, false, true)));
  }

  void InstallMockInputContextHandler() {
    input_context_handler_ = std::make_unique<ui::MockIMEInputContextHandler>();
    ui::IMEBridge::Get()->SetInputContextHandler(input_context_handler_.get());
  }

  // Retrieves the number of times commit text is updated.
  int GetCommitTextCallCount() {
    EXPECT_TRUE(input_context_handler_);
    return input_context_handler_->commit_text_call_count();
  }

  void WaitForCommitText(const std::u16string& value) {
    std::string error_message =
        base::UTF16ToUTF8(u"Still waiting for commit text: " + value);
    EXPECT_TRUE(input_context_handler_);
    SuccessWaiter(base::BindLambdaForTesting([&]() {
                    return value == input_context_handler_->last_commit_text();
                  }),
                  error_message)
        .Wait();
  }

  const base::flat_map<std::string, Dictation::LocaleData>
  GetAllSupportedLocales() {
    return Dictation::GetAllSupportedLocales();
  }

 private:
  SpeechRecognitionTestHelper test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ui::MockIMEInputContextHandler> input_context_handler_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  std::unique_ptr<ExtensionConsoleErrorObserver> console_observer_;
};

INSTANTIATE_TEST_SUITE_P(
    Network,
    DictationTest,
    ::testing::Values(speech::SpeechRecognitionType::kNetwork));

INSTANTIATE_TEST_SUITE_P(
    OnDevice,
    DictationTest,
    ::testing::Values(speech::SpeechRecognitionType::kOnDevice));

// Tests the behavior of the GetAllSupportedLocales method, specifically how
// it sets locale data.
IN_PROC_BROWSER_TEST_P(DictationTest, GetAllSupportedLocales) {
  auto locales = GetAllSupportedLocales();
  for (auto& it : locales) {
    const std::string locale = it.first;
    bool works_offline = it.second.works_offline;
    bool installed = it.second.installed;
    if (GetParam() == speech::SpeechRecognitionType::kOnDevice &&
        locale == speech::kUsEnglishLocale) {
      // Currently, the only locale supported by SODA is en-US. It should work
      // offline and be installed.
      EXPECT_TRUE(works_offline);
      EXPECT_TRUE(installed);
    } else {
      EXPECT_FALSE(works_offline);
      EXPECT_FALSE(installed);
    }
  }

  if (GetParam() == speech::SpeechRecognitionType::kOnDevice) {
    // Uninstall SODA and all language packs.
    speech::SodaInstaller::GetInstance()->UninstallSodaForTesting();
  } else {
    return;
  }

  locales = GetAllSupportedLocales();
  for (auto& it : locales) {
    const std::string locale = it.first;
    bool works_offline = it.second.works_offline;
    bool installed = it.second.installed;
    if (locale == speech::kUsEnglishLocale) {
      // en-US should be marked as "works offline", but it shouldn't be
      // installed.
      EXPECT_TRUE(works_offline);
      EXPECT_FALSE(installed);
    } else {
      EXPECT_FALSE(works_offline);
      EXPECT_FALSE(installed);
    }
  }
}

IN_PROC_BROWSER_TEST_P(DictationTest, StartsAndStopsRecognition) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_P(DictationTest, EntersFinalizedSpeech) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalResultAndWaitForTextAreaValue(kFinalSpeechResult,
                                         kFinalSpeechResult);
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

// Tests that multiple finalized strings can be committed to the text area.
// Also ensures that spaces are added between finalized utterances.
IN_PROC_BROWSER_TEST_P(DictationTest, EntersMultipleFinalizedStrings) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalResultAndWaitForTextAreaValue("The rain in Spain",
                                         "The rain in Spain");
  SendFinalResultAndWaitForTextAreaValue(
      "falls mainly on the plain.",
      "The rain in Spain falls mainly on the plain.");
  SendFinalResultAndWaitForTextAreaValue(
      "Vega is a star.",
      "The rain in Spain falls mainly on the plain. Vega is a star.");
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_P(DictationTest, OnlyAddSpaceWhenNecessary) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalResultAndWaitForTextAreaValue("The rain in Spain",
                                         "The rain in Spain");
  // Artificially add a space to this utterance. Verify that only one space is
  // added.
  SendFinalResultAndWaitForTextAreaValue(
      " falls mainly on the plain.",
      "The rain in Spain falls mainly on the plain.");
  // Artificially add a space to this utterance. Verify that only one space is
  // added.
  SendFinalResultAndWaitForTextAreaValue(
      " Vega is a star.",
      "The rain in Spain falls mainly on the plain. Vega is a star.");
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_P(DictationTest, RecognitionEndsWhenInputFieldLosesFocus) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendFinalResultAndWaitForTextAreaValue("Vega is a star", "Vega is a star");
  ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, ui::KeyboardCode::VKEY_TAB, false, false, false, false)));
  WaitForRecognitionStopped();
  EXPECT_EQ("Vega is a star", GetTextAreaValue());
}

IN_PROC_BROWSER_TEST_P(DictationTest, UserEndsDictationWhenChromeVoxEnabled) {
  EnableChromeVox();
  EXPECT_TRUE(GetManager()->IsSpokenFeedbackEnabled());
  InstallMockInputContextHandler();

  GetManager()->ToggleDictation();
  WaitForRecognitionStarted();
  SendInterimResultAndWait(kFinalSpeechResult);
  GetManager()->ToggleDictation();
  WaitForRecognitionStopped();

  WaitForCommitText(kFinalSpeechResult16);
}

IN_PROC_BROWSER_TEST_P(DictationTest, EntersInterimSpeechWhenToggledOff) {
  InstallMockInputContextHandler();

  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SendInterimResultAndWait(kFirstSpeechResult);
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
  WaitForCommitText(kFirstSpeechResult16);
}

// Tests that commit text is not updated if the user toggles dictation and no
// speech results are processed.
IN_PROC_BROWSER_TEST_P(DictationTest, UserEndsDictationBeforeSpeech) {
  InstallMockInputContextHandler();
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
  EXPECT_EQ(0, GetCommitTextCallCount());
}

// Ensures that the correct metrics are recorded when Dictation is toggled.
IN_PROC_BROWSER_TEST_P(DictationTest, Metrics) {
  base::HistogramTester histogram_tester_;
  bool on_device = GetParam() == speech::SpeechRecognitionType::kOnDevice;
  const char* metric_name = on_device ? kOnDeviceListeningDurationMetric
                                      : kNetworkListeningDurationMetric;
  HistogramWaiter waiter(metric_name);
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
  waiter.Wait();
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // Ensure that we recorded the correct locale.
  histogram_tester_.ExpectUniqueSample(/*name=*/kLocaleMetric,
                                       /*sample=*/base::PersistentHash("en-US"),
                                       /*expected_bucket_count=*/1);
  // Ensure that we recorded the type of speech recognition and listening
  // duration.
  if (on_device) {
    histogram_tester_.ExpectUniqueSample(/*name=*/kOnDeviceSpeechMetric,
                                         /*sample=*/true,
                                         /*expected_bucket_count=*/1);
    ASSERT_EQ(1u,
              histogram_tester_.GetAllSamples(kOnDeviceListeningDurationMetric)
                  .size());
    // Ensure there are no metrics for the other type of speech recognition.
    ASSERT_EQ(0u,
              histogram_tester_.GetAllSamples(kNetworkListeningDurationMetric)
                  .size());
  } else {
    histogram_tester_.ExpectUniqueSample(/*name=*/kOnDeviceSpeechMetric,
                                         /*sample=*/false,
                                         /*expected_bucket_count=*/1);
    ASSERT_EQ(1u,
              histogram_tester_.GetAllSamples(kNetworkListeningDurationMetric)
                  .size());
    // Ensure there are no metrics for the other type of speech recognition.
    ASSERT_EQ(0u,
              histogram_tester_.GetAllSamples(kOnDeviceListeningDurationMetric)
                  .size());
  }
}

IN_PROC_BROWSER_TEST_P(DictationTest,
                       DictationStopsWhenSystemTrayBecomesVisible) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  SystemTrayTestApi::Create()->ShowBubble();
  WaitForRecognitionStopped();
}

class DictationCommandsTest : public DictationTest {
 protected:
  DictationCommandsTest() {}
  ~DictationCommandsTest() override = default;
  DictationCommandsTest(const DictationCommandsTest&) = delete;
  DictationCommandsTest& operator=(const DictationCommandsTest&) = delete;

  void SetUpOnMainThread() override {
    DictationTest::SetUpOnMainThread();
    ToggleDictationWithKeystroke();
    WaitForRecognitionStarted();
  }

  void TearDownOnMainThread() override {
    ToggleDictationWithKeystroke();
    WaitForRecognitionStopped();
    DictationTest::TearDownOnMainThread();
  }

  std::string GetClipboardText() {
    std::u16string text;
    ui::Clipboard::GetForCurrentThread()->ReadText(
        ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr, &text);
    return base::UTF16ToUTF8(text);
  }
};

INSTANTIATE_TEST_SUITE_P(
    Network,
    DictationCommandsTest,
    ::testing::Values(speech::SpeechRecognitionType::kNetwork));

INSTANTIATE_TEST_SUITE_P(
    OnDevice,
    DictationCommandsTest,
    ::testing::Values(speech::SpeechRecognitionType::kOnDevice));

IN_PROC_BROWSER_TEST_P(DictationCommandsTest, TypesCommands) {
  std::string expected_text = "";
  int i = 0;
  for (const char* command : kEnglishDictationCommands) {
    std::string type_command = "type ";
    if (i == 0) {
      expected_text += command;
    } else {
      expected_text += " ";
      expected_text += command;
    }
    SendFinalResultAndWaitForTextAreaValue(type_command + command,
                                           expected_text);
    ++i;
  }
}

IN_PROC_BROWSER_TEST_P(DictationCommandsTest, TypesNonCommands) {
  // The phrase should be entered without the word "type".
  SendFinalResultAndWaitForTextAreaValue("Type this is a test",
                                         "this is a test");
}

IN_PROC_BROWSER_TEST_P(DictationCommandsTest, DeleteCharacter) {
  SendFinalResultAndWaitForTextAreaValue("Vega", "Vega");
  // Capitalization and whitespace shouldn't matter.
  SendFinalResultAndWaitForTextAreaValue(" Delete", "Veg");
  SendFinalResultAndWaitForTextAreaValue("delete ", "Ve");
  SendFinalResultAndWaitForTextAreaValue("  delete ", "V");
  SendFinalResultAndWaitForTextAreaValue("DELETE", "");
}

IN_PROC_BROWSER_TEST_P(DictationCommandsTest, MoveByCharacter) {
  SendFinalResultAndWaitForTextAreaValue("Lyra", "Lyra");
  SendFinalResultAndWaitForCaretBoundsChanged("Move to the Previous character");
  // White space is added to the text on the left of the text caret, but not
  // to the right of the text caret.
  SendFinalResultAndWaitForTextAreaValue("inserted", "Lyr inserteda");
  SendFinalResultAndWaitForCaretBoundsChanged("move TO the next character ");
  SendFinalResultAndWaitForTextAreaValue("is a constellation",
                                         "Lyr inserteda is a constellation");
}

IN_PROC_BROWSER_TEST_P(DictationCommandsTest, NewLineAndMoveByLine) {
  SendFinalResultAndWaitForTextAreaValue("Line 1", "Line 1");
  SendFinalResultAndWaitForTextAreaValue("new line", "Line 1\n");
  SendFinalResultAndWaitForTextAreaValue("Line 2", "Line 1\nLine 2");
  SendFinalResultAndWaitForCaretBoundsChanged("Move to the previous line ");
  SendFinalResultAndWaitForTextAreaValue("up", "Line 1 up\nLine 2");
  SendFinalResultAndWaitForCaretBoundsChanged("Move to the next line");
  SendFinalResultAndWaitForTextAreaValue("down", "Line 1 up\nLine 2 down");
}

IN_PROC_BROWSER_TEST_P(DictationCommandsTest, UndoAndRedo) {
  SendFinalResultAndWaitForTextAreaValue("The constellation",
                                         "The constellation");
  SendFinalResultAndWaitForTextAreaValue(" Myra", "The constellation Myra");
  SendFinalResultAndWaitForTextAreaValue("undo", "The constellation");
  SendFinalResultAndWaitForTextAreaValue(" Lyra", "The constellation Lyra");
  SendFinalResultAndWaitForTextAreaValue("undo", "The constellation");
  SendFinalResultAndWaitForTextAreaValue("redo", "The constellation Lyra");
}

// Flaky, https://crbug.com/1296811
IN_PROC_BROWSER_TEST_P(DictationCommandsTest, DISABLED_SelectAllAndUnselect) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SendFinalResultAndWaitForTextAreaValue("Vega is the brightest star in Lyra",
                                         "Vega is the brightest star in Lyra");
  SendFinalResultAndWaitForSelectionChanged("Select all", web_contents);
  SendFinalResultAndWaitForTextAreaValue("delete", "");
  SendFinalResultAndWaitForTextAreaValue(
      "Vega is the fifth brightest star in the sky",
      "Vega is the fifth brightest star in the sky");
  SendFinalResultAndWaitForSelectionChanged("Select all", web_contents);
  SendFinalResultAndWaitForSelectionChanged("Unselect", web_contents);
  SendFinalResultAndWaitForTextAreaValue(
      "!", "Vega is the fifth brightest star in the sky!");
}

IN_PROC_BROWSER_TEST_P(DictationCommandsTest, CutCopyPaste) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SendFinalResultAndWaitForTextAreaValue("Star", "Star");
  SendFinalResultAndWaitForSelectionChanged("Select all", web_contents);
  SendFinalResultAndWaitForClipboardChanged("Copy");
  EXPECT_EQ("Star", GetClipboardText());
  SendFinalResultAndWaitForSelectionChanged("unselect", web_contents);
  SendFinalResultAndWaitForTextAreaValue("paste", "StarStar");
  SendFinalResultAndWaitForSelectionChanged("select ALL ", web_contents);
  SendFinalResultAndWaitForClipboardChanged("cut");
  EXPECT_EQ("StarStar", GetClipboardText());
  WaitForTextAreaValue("");
  SendFinalResultAndWaitForTextAreaValue("  PaStE ", "StarStar");
}

// Ensures that a metric is recorded when a macro succeeds.
// TODO(crbug.com/1288964): Add a test to ensure that a metric is recorded when
// a macro fails.
IN_PROC_BROWSER_TEST_P(DictationCommandsTest, MacroSucceededMetric) {
  base::HistogramTester histogram_tester_;
  SendFinalResultAndWaitForTextAreaValue("Vega is the brightest star in Lyra",
                                         "Vega is the brightest star in Lyra");
  histogram_tester_.ExpectUniqueSample(/*name=*/kMacroSucceededMetric,
                                       /*sample=*/kInputTextViewMetricValue,
                                       /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(/*name=*/kMacroFailedMetric,
                                       /*sample=*/kInputTextViewMetricValue,
                                       /*expected_bucket_count=*/0);
  histogram_tester_.ExpectUniqueSample(/*name=*/kMacroRecognizedMetric,
                                       /*sample=*/kInputTextViewMetricValue,
                                       /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_P(DictationCommandsTest, Help) {
  SendFinalResultAndWait("help");

  // Wait for the help URL to load.
  SuccessWaiter(
      base::BindLambdaForTesting([&]() {
        content::WebContents* web_contents =
            browser()->tab_strip_model()->GetActiveWebContents();
        return web_contents->GetVisibleURL() ==
               "https://support.google.com/chromebook?p=text_dictation_m100";
      }),
      "Still waiting for help URL to load")
      .Wait();

  // Opening a new tab with the help center article toggles Dictation off.
  WaitForRecognitionStopped();
}

// Tests the behavior of the Dictation bubble UI.
class DictationUITest : public DictationTest {
 protected:
  DictationUITest() = default;
  ~DictationUITest() override = default;
  DictationUITest(const DictationUITest&) = delete;
  DictationUITest& operator=(const DictationUITest&) = delete;

  void SetUpOnMainThread() override {
    DictationTest::SetUpOnMainThread();
    dictation_bubble_test_helper_ =
        std::make_unique<DictationBubbleTestHelper>();
  }

  void WaitForProperties(
      bool visible,
      DictationBubbleIconType icon,
      const absl::optional<std::u16string>& text,
      const absl::optional<std::vector<std::u16string>>& hints) {
    WaitForVisibility(visible);
    WaitForVisibleIcon(icon);
    if (text.has_value())
      WaitForVisibleText(text.value());
    if (hints.has_value())
      WaitForVisibleHints(hints.value());
  }

 private:
  void WaitForVisibility(bool visible) {
    std::string error_message = "Still waiting for UI visibility: ";
    error_message += visible ? "true" : "false";
    SuccessWaiter(base::BindLambdaForTesting([&]() {
                    return dictation_bubble_test_helper_->IsVisible() ==
                           visible;
                  }),
                  error_message)
        .Wait();
  }

  void WaitForVisibleIcon(DictationBubbleIconType icon) {
    std::string error_message = "Still waiting for UI icon: " + ToString(icon);
    SuccessWaiter(base::BindLambdaForTesting([&]() {
                    return dictation_bubble_test_helper_->GetVisibleIcon() ==
                           icon;
                  }),
                  error_message)
        .Wait();
  }

  void WaitForVisibleText(const std::u16string& text) {
    std::string error_message =
        "Still waiting for UI text: " + base::UTF16ToUTF8(text);
    SuccessWaiter(base::BindLambdaForTesting([&]() {
                    return dictation_bubble_test_helper_->GetText() == text;
                  }),
                  error_message)
        .Wait();
  }

  void WaitForVisibleHints(const std::vector<std::u16string>& hints) {
    std::string error_message = base::UTF16ToUTF8(
        u"Still waiting for UI hints: " + base::JoinString(hints, u","));
    SuccessWaiter(
        base::BindLambdaForTesting([&]() {
          return dictation_bubble_test_helper_->HasVisibleHints(hints);
        }),
        error_message)
        .Wait();
  }

  std::unique_ptr<DictationBubbleTestHelper> dictation_bubble_test_helper_;
};

// Consistently failing on Linux ChromiumOS MSan (https://crbug.com/1302688).
#if defined(MEMORY_SANITIZER)
#define MAYBE_ShownWhenSpeechRecognitionStarts \
  DISABLED_ShownWhenSpeechRecognitionStarts
#define MAYBE_DisplaysInterimSpeechResults DISABLED_DisplaysInterimSpeechResults
#define MAYBE_DisplaysMacroSuccess DISABLED_DisplaysMacroSuccess
#define MAYBE_ResetsToStandbyModeAfterFinalSpeechResult \
  DISABLED_ResetsToStandbyModeAfterFinalSpeechResult
#define MAYBE_DisplaysMacroSuccess DISABLED_DisplaysMacroSuccess
#define MAYBE_HiddenWhenDictationDeactivates \
  DISABLED_HiddenWhenDictationDeactivates
#define MAYBE_StandbyHints DISABLED_StandbyHints
#define MAYBE_HintsShownWhenTextCommitted DISABLED_HintsShownWhenTextCommitted
#define MAYBE_HintsShownAfterTextSelected DISABLED_HintsShownAfterTextSelected
#define MAYBE_HintsShownAfterCommandExecuted \
  DISABLED_HintsShownAfterCommandExecuted
#else
#define MAYBE_ShownWhenSpeechRecognitionStarts ShownWhenSpeechRecognitionStarts
#define MAYBE_DisplaysInterimSpeechResults DisplaysInterimSpeechResults
#define MAYBE_DisplaysMacroSuccess DisplaysMacroSuccess
#define MAYBE_ResetsToStandbyModeAfterFinalSpeechResult \
  ResetsToStandbyModeAfterFinalSpeechResult
#define MAYBE_DisplaysMacroSuccess DisplaysMacroSuccess
#define MAYBE_HiddenWhenDictationDeactivates HiddenWhenDictationDeactivates
#define MAYBE_StandbyHints StandbyHints
#define MAYBE_HintsShownWhenTextCommitted HintsShownWhenTextCommitted
#define MAYBE_HintsShownAfterTextSelected HintsShownAfterTextSelected
#define MAYBE_HintsShownAfterCommandExecuted HintsShownAfterCommandExecuted
#endif

INSTANTIATE_TEST_SUITE_P(
    Network,
    DictationUITest,
    ::testing::Values(speech::SpeechRecognitionType::kNetwork));

INSTANTIATE_TEST_SUITE_P(
    OnDevice,
    DictationUITest,
    ::testing::Values(speech::SpeechRecognitionType::kOnDevice));

IN_PROC_BROWSER_TEST_P(DictationUITest,
                       MAYBE_ShownWhenSpeechRecognitionStarts) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kStandby,
                    /*text=*/absl::optional<std::u16string>(),
                    /*hints=*/absl::optional<std::vector<std::u16string>>());
}

IN_PROC_BROWSER_TEST_P(DictationUITest, MAYBE_DisplaysInterimSpeechResults) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  // Send an interim speech result.
  SendInterimResultAndWait("Testing");
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kHidden,
                    /*text=*/u"Testing",
                    /*hints=*/absl::optional<std::vector<std::u16string>>());
}

IN_PROC_BROWSER_TEST_P(DictationUITest, MAYBE_DisplaysMacroSuccess) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  // Perform a command.
  SendFinalResultAndWait("Select all");
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kMacroSuccess,
                    /*text=*/u"Select all",
                    /*hints=*/absl::optional<std::vector<std::u16string>>());
  // UI should return to standby mode after a timeout.
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kStandby,
                    /*text=*/std::u16string(),
                    /*hints=*/absl::optional<std::vector<std::u16string>>());
}

IN_PROC_BROWSER_TEST_P(DictationUITest,
                       MAYBE_ResetsToStandbyModeAfterFinalSpeechResult) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kStandby,
                    /*text=*/absl::optional<std::u16string>(),
                    /*hints=*/absl::optional<std::vector<std::u16string>>());
  // Send an interim speech result.
  SendInterimResultAndWait("Testing");
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kHidden,
                    /*text=*/u"Testing",
                    /*hints=*/absl::optional<std::vector<std::u16string>>());
  // Send a final speech result. UI should return to standby mode.
  SendFinalResultAndWait("Testing 123");
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kStandby,
                    /*text=*/std::u16string(),
                    /*hints=*/absl::optional<std::vector<std::u16string>>());
}

IN_PROC_BROWSER_TEST_P(DictationUITest, MAYBE_HiddenWhenDictationDeactivates) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kStandby,
                    /*text=*/absl::optional<std::u16string>(),
                    /*hints=*/absl::optional<std::vector<std::u16string>>());
  // The UI should be hidden when Dictation deactivates.
  ToggleDictationWithKeystroke();
  WaitForRecognitionStopped();
  WaitForProperties(/*visible=*/false,
                    /*icon=*/DictationBubbleIconType::kHidden,
                    /*text=*/std::u16string(),
                    /*hints=*/absl::optional<std::vector<std::u16string>>());
}

IN_PROC_BROWSER_TEST_P(DictationUITest, MAYBE_StandbyHints) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kStandby,
                    /*text=*/absl::optional<std::u16string>(),
                    /*hints=*/absl::optional<std::vector<std::u16string>>());
  // Hints should show up after a few seconds without speech.
  WaitForProperties(
      /*visible=*/true,
      /*icon=*/DictationBubbleIconType::kStandby,
      /*text=*/absl::optional<std::u16string>(),
      /*hints=*/std::vector<std::u16string>{kTrySaying, kType, kHelp});
}

// Ensures that Search + D can be used to toggle Dictation when ChromeVox is
// active. Also verifies that ChromeVox announces hints when they are shown in
// the Dictation UI.

// TODO(crbug.com/1296810): DictationUITest.ChromeVoxAnnouncesHints is flaky.
IN_PROC_BROWSER_TEST_P(DictationUITest, DISABLED_ChromeVoxAnnouncesHints) {
  // Setup ChromeVox first.
  test::SpeechMonitor sm;
  EXPECT_FALSE(GetManager()->IsSpokenFeedbackEnabled());
  extensions::ExtensionHostTestHelper host_helper(
      browser()->profile(), extension_misc::kChromeVoxExtensionId);
  EnableChromeVox();
  host_helper.WaitForHostCompletedFirstLoad();
  EXPECT_TRUE(GetManager()->IsSpokenFeedbackEnabled());

  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();

  // Hints should show up after a few seconds without speech.
  WaitForProperties(
      /*visible=*/true,
      /*icon=*/DictationBubbleIconType::kStandby,
      /*text=*/absl::optional<std::u16string>(),
      /*hints=*/std::vector<std::u16string>{kTrySaying, kType, kHelp});

  // Assert speech from ChromeVox.
  sm.ExpectSpeechPattern("Try saying*Type*Help*");
  sm.Replay();
}

IN_PROC_BROWSER_TEST_P(DictationUITest, MAYBE_HintsShownWhenTextCommitted) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();

  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kStandby,
                    /*text=*/absl::optional<std::u16string>(),
                    /*hints=*/absl::optional<std::vector<std::u16string>>());

  // Send a final speech result. UI should return to standby mode.
  SendFinalResultAndWait("Testing");
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kStandby,
                    /*text=*/std::u16string(),
                    /*hints=*/absl::optional<std::vector<std::u16string>>());

  // Hints should show up after a few seconds without speech.
  WaitForProperties(
      /*visible=*/true,
      /*icon=*/DictationBubbleIconType::kStandby,
      /*text=*/absl::optional<std::u16string>(),
      /*hints=*/
      std::vector<std::u16string>{kTrySaying, kUndo, kDelete, kSelectAll,
                                  kHelp});
}

IN_PROC_BROWSER_TEST_P(DictationUITest, MAYBE_HintsShownAfterTextSelected) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();

  // Perform a select all command.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SendFinalResultAndWaitForTextAreaValue("Vega is the brightest star in Lyra",
                                         "Vega is the brightest star in Lyra");
  SendFinalResultAndWaitForSelectionChanged("Select all", web_contents);
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kMacroSuccess,
                    /*text=*/u"Select all",
                    /*hints=*/absl::optional<std::vector<std::u16string>>());

  // UI should return to standby mode with hints after a few seconds without
  // speech.
  WaitForProperties(
      /*visible=*/true,
      /*icon=*/DictationBubbleIconType::kStandby,
      /*text=*/absl::optional<std::u16string>(),
      /*hints=*/
      std::vector<std::u16string>{kTrySaying, kUnselect, kCopy, kDelete,
                                  kHelp});
}

IN_PROC_BROWSER_TEST_P(DictationUITest, MAYBE_HintsShownAfterCommandExecuted) {
  ToggleDictationWithKeystroke();
  WaitForRecognitionStarted();

  // Perform a command.
  SendFinalResultAndWait("Move to the previous character");
  WaitForProperties(/*visible=*/true,
                    /*icon=*/DictationBubbleIconType::kMacroSuccess,
                    /*text=*/u"Move to the previous character",
                    /*hints=*/absl::optional<std::vector<std::u16string>>());

  // UI should return to standby mode with hints after a few seconds without
  // speech.
  WaitForProperties(
      /*visible=*/true,
      /*icon=*/DictationBubbleIconType::kStandby,
      /*text=*/absl::optional<std::u16string>(),
      /*hints=*/std::vector<std::u16string>{kTrySaying, kUndo, kHelp});
}

// TODO(crbug.com/1264544): Test looking at gn args has pumpkin and does
// repeats.

}  // namespace ash
