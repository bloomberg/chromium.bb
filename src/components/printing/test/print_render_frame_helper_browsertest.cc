// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/renderer/print_render_frame_helper.h"

#include <stddef.h>

#include <memory>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/printing/common/print.mojom-test-utils.h"
#include "components/printing/common/print.mojom.h"
#include "components/printing/common/print_messages.h"
#include "components/printing/test/mock_printer.h"
#include "components/printing/test/print_mock_render_thread.h"
#include "components/printing/test/print_test_content_renderer_client.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/render_view_test.h"
#include "ipc/ipc_listener.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/page_range.h"
#include "printing/print_job_constants.h"
#include "printing/units.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/public/web/web_view.h"

#if defined(OS_WIN) || defined(OS_APPLE)
#include "base/files/file_util.h"
#include "printing/image.h"

using blink::WebFrame;
using blink::WebLocalFrame;
using blink::WebString;
#endif

namespace printing {

namespace {

// A simple web page.
const char kHelloWorldHTML[] = "<body><p>Hello World!</p></body>";

// Web page used for testing onbeforeprint/onafterprint.
const char kBeforeAfterPrintHtml[] =
    "<body>"
    "<script>"
    "var beforePrintCount = 0;"
    "var afterPrintCount = 0;"
    "window.onbeforeprint = () => { ++beforePrintCount; };"
    "window.onafterprint = () => { ++afterPrintCount; };"
    "</script>"
    "<button id=\"print\" onclick=\"window.print();\">Hello World!</button>"
    "</body>";

#if !defined(OS_CHROMEOS)
// A simple webpage with a button to print itself with.
const char kPrintOnUserAction[] =
    "<body>"
    "  <button id=\"print\" onclick=\"window.print();\">Hello World!</button>"
    "</body>";

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
// HTML with 3 pages.
const char kMultipageHTML[] =
    "<html><head><style>"
    ".break { page-break-after: always; }"
    "</style></head>"
    "<body>"
    "<div class='break'>page1</div>"
    "<div class='break'>page2</div>"
    "<div>page3</div>"
    "</body></html>";

// A simple web page with print page size css.
const char kHTMLWithPageSizeCss[] =
    "<html><head><style>"
    "@media print {"
    "  @page {"
    "     size: 4in 4in;"
    "  }"
    "}"
    "</style></head>"
    "<body>Lorem Ipsum:"
    "</body></html>";

// A simple web page with print page layout css.
const char kHTMLWithLandscapePageCss[] =
    "<html><head><style>"
    "@media print {"
    "  @page {"
    "     size: landscape;"
    "  }"
    "}"
    "</style></head>"
    "<body>Lorem Ipsum:"
    "</body></html>";

// A longer web page.
const char kLongPageHTML[] =
    "<body><img src=\"\" width=10 height=10000 /></body>";

// A web page to simulate the print preview page.
const char kPrintPreviewHTML[] =
    "<body><p id=\"pdf-viewer\">Hello World!</p></body>";

void CreatePrintSettingsDictionary(base::DictionaryValue* dict) {
  dict->SetBoolean(kSettingLandscape, false);
  dict->SetBoolean(kSettingCollate, false);
  dict->SetInteger(kSettingColor, static_cast<int>(mojom::ColorModel::kGray));
  dict->SetInteger(kSettingPrinterType, static_cast<int>(PrinterType::kPdf));
  dict->SetInteger(kSettingDuplexMode,
                   static_cast<int>(mojom::DuplexMode::kSimplex));
  dict->SetInteger(kSettingCopies, 1);
  dict->SetString(kSettingDeviceName, "dummy");
  dict->SetInteger(kPreviewUIID, 4);
  dict->SetInteger(kPreviewRequestID, 12345);
  dict->SetBoolean(kIsFirstRequest, true);
  dict->SetInteger(kSettingMarginsType,
                   static_cast<int>(mojom::MarginType::kDefaultMargins));
  dict->SetBoolean(kSettingPreviewModifiable, true);
  dict->SetBoolean(kSettingPreviewIsFromArc, false);
  dict->SetBoolean(kSettingPreviewIsPdf, false);
  dict->SetBoolean(kSettingHeaderFooterEnabled, false);
  dict->SetBoolean(kSettingShouldPrintBackgrounds, false);
  dict->SetBoolean(kSettingShouldPrintSelectionOnly, false);
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // !defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
// TODO(https://crbug.com/1008939): Remove DidPreviewPageListener once all IPC
// messages are moved to mojo.
class DidPreviewPageListener : public IPC::Listener {
 public:
  explicit DidPreviewPageListener(base::RunLoop* run_loop)
      : run_loop_(run_loop) {}
  DidPreviewPageListener(const DidPreviewPageListener&) = delete;
  DidPreviewPageListener& operator=(const DidPreviewPageListener&) = delete;

  bool OnMessageReceived(const IPC::Message& message) override {
    if (message.type() == PrintHostMsg_MetafileReadyForPrinting::ID)
      run_loop_->Quit();
    return false;
  }

 private:
  base::RunLoop* const run_loop_;
};

class FakePrintPreviewUI : public mojom::PrintPreviewUI {
 public:
  FakePrintPreviewUI() = default;
  ~FakePrintPreviewUI() override = default;

  mojo::PendingAssociatedRemote<mojom::PrintPreviewUI> BindReceiver() {
    return receiver_.BindNewEndpointAndPassDedicatedRemote();
  }
  void SetQuitClosure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

  bool preview_failed() const { return preview_failed_; }
  bool preview_cancelled() const { return preview_cancelled_; }
  bool invalid_printer_setting() const { return invalid_printer_setting_; }

  // mojom::PrintPreviewUI:
  void SetOptionsFromDocument(const mojom::OptionsFromDocumentParamsPtr params,
                              int32_t request_id) override {}
  void PrintPreviewFailed(int32_t document_cookie,
                          int32_t request_id) override {
    preview_failed_ = true;
    RunQuitClosure();
  }
  void PrintPreviewCancelled(int32_t document_cookie,
                             int32_t request_id) override {
    preview_cancelled_ = true;
    RunQuitClosure();
  }
  void PrinterSettingsInvalid(int32_t document_cookie,
                              int32_t request_id) override {
    invalid_printer_setting_ = true;
    RunQuitClosure();
  }

 private:
  void RunQuitClosure() {
    if (!quit_closure_)
      return;
    std::move(quit_closure_).Run();
  }

  bool preview_failed_ = false;
  bool preview_cancelled_ = false;
  bool invalid_printer_setting_ = false;
  base::OnceClosure quit_closure_;

  mojo::AssociatedReceiver<mojom::PrintPreviewUI> receiver_{this};
};
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

class TestPrintManagerHost
    : public mojom::PrintManagerHostInterceptorForTesting {
 public:
  TestPrintManagerHost(content::RenderFrame* frame, MockPrinter* printer)
      : printer_(printer) {
    Init(frame);
  }
  ~TestPrintManagerHost() override = default;

  // mojom::PrintManagerInterceptorForTesting
  mojom::PrintManagerHost* GetForwardingInterface() override { return nullptr; }
  void DidGetPrintedPagesCount(int32_t cookie, uint32_t number_pages) override {
    if (number_pages_ > 0)
      EXPECT_EQ(number_pages, number_pages_);
    printer_->SetPrintedPagesCount(cookie, number_pages);
  }
  void DidGetDocumentCookie(int32_t cookie) override {}
  void GetDefaultPrintSettings(
      GetDefaultPrintSettingsCallback callback) override {
    printing::mojom::PrintParamsPtr params =
        printer_->GetDefaultPrintSettings();
    std::move(callback).Run(std::move(params));
  }
  void UpdatePrintSettings(int32_t cookie,
                           base::Value job_settings,
                           UpdatePrintSettingsCallback callback) override {
    auto params = printing::mojom::PrintPagesParams::New();
    params->params = printing::mojom::PrintParams::New();
    bool canceled = false;

    // Check and make sure the required settings are all there.
    // We don't actually care about the values.
    base::Optional<int> margins_type =
        job_settings.FindIntKey(printing::kSettingMarginsType);
    if (!margins_type.has_value() ||
        !job_settings.FindBoolKey(printing::kSettingLandscape) ||
        !job_settings.FindBoolKey(printing::kSettingCollate) ||
        !job_settings.FindIntKey(printing::kSettingColor) ||
        !job_settings.FindIntKey(printing::kSettingPrinterType) ||
        !job_settings.FindBoolKey(printing::kIsFirstRequest) ||
        !job_settings.FindStringKey(printing::kSettingDeviceName) ||
        !job_settings.FindIntKey(printing::kSettingDuplexMode) ||
        !job_settings.FindIntKey(printing::kSettingCopies) ||
        !job_settings.FindIntKey(printing::kPreviewUIID) ||
        !job_settings.FindIntKey(printing::kPreviewRequestID)) {
      std::move(callback).Run(std::move(params), canceled);
      return;
    }

    // Just return the default settings.
    const base::Value* page_range =
        job_settings.FindListKey(printing::kSettingPageRange);
    printing::PageRanges new_ranges;
    if (page_range) {
      for (const base::Value& dict : page_range->GetList()) {
        if (!dict.is_dict())
          continue;

        base::Optional<int> range_from =
            dict.FindIntKey(printing::kSettingPageRangeFrom);
        base::Optional<int> range_to =
            dict.FindIntKey(printing::kSettingPageRangeTo);
        if (!range_from || !range_to)
          continue;

        // Page numbers are 1-based in the dictionary.
        // Page numbers are 0-based for the printing context.
        printing::PageRange range;
        range.from = range_from.value() - 1;
        range.to = range_to.value() - 1;
        new_ranges.push_back(range);
      }
    }

    // Get media size
    const base::Value* media_size_value =
        job_settings.FindDictKey(printing::kSettingMediaSize);
    gfx::Size page_size;
    if (media_size_value) {
      base::Optional<int> width_microns =
          media_size_value->FindIntKey(printing::kSettingMediaSizeWidthMicrons);
      base::Optional<int> height_microns = media_size_value->FindIntKey(
          printing::kSettingMediaSizeHeightMicrons);

      if (width_microns && height_microns) {
        float device_microns_per_unit =
            static_cast<float>(printing::kMicronsPerInch) /
            printing::kDefaultPdfDpi;
        page_size = gfx::Size(width_microns.value() / device_microns_per_unit,
                              height_microns.value() / device_microns_per_unit);
      }
    }

    // Get scaling
    base::Optional<int> setting_scale_factor =
        job_settings.FindIntKey(printing::kSettingScaleFactor);
    int scale_factor = setting_scale_factor.value_or(100);

    std::vector<uint32_t> pages(printing::PageRange::GetPages(new_ranges));
    printer_->UpdateSettings(cookie, params.get(), pages, margins_type.value(),
                             page_size, scale_factor);
    base::Optional<bool> selection_only =
        job_settings.FindBoolKey(printing::kSettingShouldPrintSelectionOnly);
    base::Optional<bool> should_print_backgrounds =
        job_settings.FindBoolKey(printing::kSettingShouldPrintBackgrounds);
    params->params->selection_only = selection_only.value();
    params->params->should_print_backgrounds = should_print_backgrounds.value();
    std::move(callback).Run(std::move(params), canceled);
  }

  void DidShowPrintDialog() override {}

  void SetExpectedPagesCount(uint32_t number_pages) {
    number_pages_ = number_pages;
  }
  void WaitUntilBinding() {
    if (receiver_.is_bound())
      return;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void Init(content::RenderFrame* frame) {
    frame->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
        mojom::PrintManagerHost::Name_,
        base::BindRepeating(&TestPrintManagerHost::BindPrintManagerReceiver,
                            base::Unretained(this)));
  }

  void BindPrintManagerReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(mojo::PendingAssociatedReceiver<mojom::PrintManagerHost>(
        std::move(handle)));

    if (!quit_closure_)
      return;
    std::move(quit_closure_).Run();
  }

  uint32_t number_pages_ = 0;
  MockPrinter* printer_;
  base::OnceClosure quit_closure_;
  mojo::AssociatedReceiver<mojom::PrintManagerHost> receiver_{this};
};

}  // namespace

class PrintRenderFrameHelperTestBase : public content::RenderViewTest {
 public:
  PrintRenderFrameHelperTestBase() = default;
  PrintRenderFrameHelperTestBase(const PrintRenderFrameHelperTestBase&) =
      delete;
  PrintRenderFrameHelperTestBase& operator=(
      const PrintRenderFrameHelperTestBase&) = delete;
  ~PrintRenderFrameHelperTestBase() override = default;

 protected:
  // content::RenderViewTest:
  content::ContentRendererClient* CreateContentRendererClient() override {
    return new PrintTestContentRendererClient();
  }

  void SetUp() override {
    render_thread_ = std::make_unique<PrintMockRenderThread>();
    print_render_thread_ =
        static_cast<PrintMockRenderThread*>(render_thread_.get());

    content::RenderViewTest::SetUp();
    BindPrintManagerHost(content::RenderFrame::FromWebFrame(GetMainFrame()));
  }

  void TearDown() override {
#if defined(LEAK_SANITIZER)
    // Do this before shutting down V8 in RenderViewTest::TearDown().
    // http://crbug.com/328552
    __lsan_do_leak_check();
#endif

    content::RenderViewTest::TearDown();
  }

  void BindPrintManagerHost(content::RenderFrame* frame) {
    auto print_manager = std::make_unique<TestPrintManagerHost>(
        frame, print_render_thread_->GetPrinter());
    GetPrintRenderFrameHelperForFrame(frame)->GetPrintManagerHost();
    print_manager->WaitUntilBinding();
    frame_to_print_manager_map_.emplace(frame, std::move(print_manager));
  }

  void ClearPrintManagerHost() { frame_to_print_manager_map_.clear(); }

  void PrintWithJavaScript() {
    ExecuteJavaScriptForTests("window.print();");
    base::RunLoop().RunUntilIdle();
  }

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  void BindToFakePrintPreviewUI() {
    PrintRenderFrameHelper* frame_helper = GetPrintRenderFrameHelper();
    frame_helper->SetPrintPreviewUI(preview_ui_.BindReceiver());
  }

  void WaitMojoMessages(base::RunLoop* run_loop) {
    preview_ui()->SetQuitClosure(run_loop->QuitClosure());
  }

  // The renderer should be done calculating the number of rendered pages
  // according to the specified settings defined in the mock render thread.
  // Verify the page count is correct.
  void VerifyPreviewPageCount(uint32_t expected_count) {
    const IPC::Message* preview_started_message =
        render_thread_->sink().GetUniqueMessageMatching(
            PrintHostMsg_DidStartPreview::ID);
    ASSERT_TRUE(preview_started_message);
    PrintHostMsg_DidStartPreview::Param param;
    PrintHostMsg_DidStartPreview::Read(preview_started_message, &param);
    EXPECT_EQ(expected_count, std::get<0>(param).page_count);
  }
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

  // Verifies whether the pages printed or not.
  void VerifyPagesPrinted(bool expect_printed) {
    const IPC::Message* print_msg =
        render_thread_->sink().GetUniqueMessageMatching(
            PrintHostMsg_DidPrintDocument::ID);
    bool did_print = !!print_msg;
    ASSERT_EQ(expect_printed, did_print);
  }

  void OnPrintPages() {
    GetPrintRenderFrameHelper()->PrintRequestedPages();
    base::RunLoop().RunUntilIdle();
  }

  void OnPrintPagesInFrame(base::StringPiece frame_name) {
    blink::WebFrame* frame = GetMainFrame()->FindFrameByName(
        blink::WebString::FromUTF8(frame_name.data(), frame_name.size()));
    ASSERT_TRUE(frame);
    content::RenderFrame* render_frame =
        content::RenderFrame::FromWebFrame(frame->ToWebLocalFrame());
    BindPrintManagerHost(render_frame);
    PrintRenderFrameHelper* helper =
        GetPrintRenderFrameHelperForFrame(render_frame);
    ASSERT_TRUE(helper);
    helper->PrintRequestedPages();
    base::RunLoop().RunUntilIdle();
  }

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  void VerifyPreviewRequest(bool expect_request) {
    const IPC::Message* print_msg =
        render_thread_->sink().GetUniqueMessageMatching(
            PrintHostMsg_SetupScriptedPrintPreview::ID);
    bool got_preview_request = !!print_msg;
    EXPECT_EQ(expect_request, got_preview_request);
  }

  void OnPrintPreview(const base::DictionaryValue& dict) {
    PrintRenderFrameHelper* print_render_frame_helper =
        GetPrintRenderFrameHelper();
    print_render_frame_helper->InitiatePrintPreview(
        mojo::NullAssociatedRemote(), false);
    base::RunLoop run_loop;
    DidPreviewPageListener filter(&run_loop);
    render_thread_->sink().AddFilter(&filter);
    print_render_frame_helper->PrintPreview(dict.Clone());
    WaitMojoMessages(&run_loop);
    run_loop.Run();
    render_thread_->sink().RemoveFilter(&filter);
  }

  void OnClosePrintPreviewDialog() {
    GetPrintRenderFrameHelper()->OnPrintPreviewDialogClosed();
  }
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

  PrintRenderFrameHelper* GetPrintRenderFrameHelper() {
    return PrintRenderFrameHelper::Get(
        content::RenderFrame::FromWebFrame(GetMainFrame()));
  }

  PrintRenderFrameHelper* GetPrintRenderFrameHelperForFrame(
      content::RenderFrame* frame) {
    return PrintRenderFrameHelper::Get(frame);
  }

  void ClickMouseButton(const gfx::Rect& bounds) {
    EXPECT_FALSE(bounds.IsEmpty());

    blink::WebMouseEvent mouse_event(
        blink::WebInputEvent::Type::kMouseDown,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    mouse_event.button = blink::WebMouseEvent::Button::kLeft;
    mouse_event.SetPositionInWidget(bounds.CenterPoint().x(),
                                    bounds.CenterPoint().y());
    mouse_event.click_count = 1;
    SendWebMouseEvent(mouse_event);
    mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
    SendWebMouseEvent(mouse_event);
  }

  void ExpectNoBeforeNoAfterPrintEvent() {
    int result;
    ASSERT_TRUE(ExecuteJavaScriptAndReturnIntValue(
        base::ASCIIToUTF16("beforePrintCount"), &result));
    EXPECT_EQ(0, result) << "beforeprint event should not be dispatched.";
    ASSERT_TRUE(ExecuteJavaScriptAndReturnIntValue(
        base::ASCIIToUTF16("afterPrintCount"), &result));
    EXPECT_EQ(0, result) << "afterprint event should not be dispatched.";
  }

  void ExpectOneBeforeNoAfterPrintEvent() {
    int result;
    ASSERT_TRUE(ExecuteJavaScriptAndReturnIntValue(
        base::ASCIIToUTF16("beforePrintCount"), &result));
    EXPECT_EQ(1, result) << "beforeprint event should be dispatched once.";
    ASSERT_TRUE(ExecuteJavaScriptAndReturnIntValue(
        base::ASCIIToUTF16("afterPrintCount"), &result));
    EXPECT_EQ(0, result) << "afterprint event should not be dispatched.";
  }

  void ExpectOneBeforeOneAfterPrintEvent() {
    int result;
    ASSERT_TRUE(ExecuteJavaScriptAndReturnIntValue(
        base::ASCIIToUTF16("beforePrintCount"), &result));
    EXPECT_EQ(1, result) << "beforeprint event should be dispatched once.";
    ASSERT_TRUE(ExecuteJavaScriptAndReturnIntValue(
        base::ASCIIToUTF16("afterPrintCount"), &result));
    EXPECT_EQ(1, result) << "afterprint event should be dispatched once.";
  }

  PrintMockRenderThread* print_render_thread() { return print_render_thread_; }
  TestPrintManagerHost* print_manager() {
    auto it = frame_to_print_manager_map_.find(
        content::RenderFrame::FromWebFrame(GetMainFrame()));
    return it->second.get();
  }
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  FakePrintPreviewUI* preview_ui() { return &preview_ui_; }
#endif

 private:
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  FakePrintPreviewUI preview_ui_;
#endif
  // Naked pointer as ownership is with
  // |content::RenderViewTest::render_thread_|.
  PrintMockRenderThread* print_render_thread_ = nullptr;
  std::map<content::RenderFrame*, std::unique_ptr<TestPrintManagerHost>>
      frame_to_print_manager_map_;
};

// RenderViewTest-based tests crash on Android
// http://crbug.com/187500
#if defined(OS_ANDROID)
#define MAYBE_PrintRenderFrameHelperTest DISABLED_PrintRenderFrameHelperTest
#else
#define MAYBE_PrintRenderFrameHelperTest PrintRenderFrameHelperTest
#endif  // defined(OS_ANDROID)

class MAYBE_PrintRenderFrameHelperTest : public PrintRenderFrameHelperTestBase {
 public:
  MAYBE_PrintRenderFrameHelperTest() = default;
  MAYBE_PrintRenderFrameHelperTest(const MAYBE_PrintRenderFrameHelperTest&) =
      delete;
  MAYBE_PrintRenderFrameHelperTest& operator=(
      const MAYBE_PrintRenderFrameHelperTest&) = delete;
  ~MAYBE_PrintRenderFrameHelperTest() override = default;
};

// This tests only for platforms without print preview.
#if !BUILDFLAG(ENABLE_PRINT_PREVIEW)
// Tests that the renderer blocks window.print() calls if they occur too
// frequently.
TEST_F(MAYBE_PrintRenderFrameHelperTest, BlockScriptInitiatedPrinting) {
  // Pretend user will cancel printing.
  print_render_thread()->set_print_dialog_user_response(false);
  // Try to print with window.print() a few times.
  PrintWithJavaScript();
  PrintWithJavaScript();
  PrintWithJavaScript();
  VerifyPagesPrinted(false);

  // Pretend user will print. (but printing is blocked.)
  print_render_thread()->set_print_dialog_user_response(true);
  PrintWithJavaScript();
  VerifyPagesPrinted(false);

  // Unblock script initiated printing and verify printing works.
  GetPrintRenderFrameHelper()->scripting_throttler_.Reset();
  print_render_thread()->printer()->ResetPrinter();
  print_manager()->SetExpectedPagesCount(1);
  PrintWithJavaScript();
  VerifyPagesPrinted(true);
}

// Tests that the renderer always allows window.print() calls if they are user
// initiated.
TEST_F(MAYBE_PrintRenderFrameHelperTest, AllowUserOriginatedPrinting) {
  // Pretend user will cancel printing.
  print_render_thread()->set_print_dialog_user_response(false);
  // Try to print with window.print() a few times.
  PrintWithJavaScript();
  PrintWithJavaScript();
  PrintWithJavaScript();
  VerifyPagesPrinted(false);

  // Pretend user will print. (but printing is blocked.)
  print_render_thread()->set_print_dialog_user_response(true);
  PrintWithJavaScript();
  VerifyPagesPrinted(false);

  // Try again as if user initiated, without resetting the print count.
  print_render_thread()->printer()->ResetPrinter();
  LoadHTML(kPrintOnUserAction);
  gfx::Size new_size(200, 100);
  Resize(new_size, false);

  print_manager()->SetExpectedPagesCount(1);
  gfx::Rect bounds = GetElementBounds("print");
  ClickMouseButton(bounds);
  base::RunLoop().RunUntilIdle();

  VerifyPagesPrinted(true);
}

// Duplicate of OnPrintPagesTest only using javascript to print.
TEST_F(MAYBE_PrintRenderFrameHelperTest, PrintWithJavascript) {
  print_manager()->SetExpectedPagesCount(1);
  PrintWithJavaScript();

  VerifyPagesPrinted(true);
}

// Regression test for https://crbug.com/912966
TEST_F(MAYBE_PrintRenderFrameHelperTest, WindowPrintBeforePrintAfterPrint) {
  LoadHTML(kBeforeAfterPrintHtml);
  ExpectNoBeforeNoAfterPrintEvent();
  print_manager()->SetExpectedPagesCount(1);

  PrintWithJavaScript();

  VerifyPagesPrinted(true);
  ExpectOneBeforeOneAfterPrintEvent();
}
#endif  // !BUILDFLAG(ENABLE_PRINT_PREVIEW)

// Tests that printing pages work and sending and receiving messages through
// that channel all works.
TEST_F(MAYBE_PrintRenderFrameHelperTest, OnPrintPages) {
  LoadHTML(kHelloWorldHTML);

  print_manager()->SetExpectedPagesCount(1);
  OnPrintPages();

  VerifyPagesPrinted(true);
}

TEST_F(MAYBE_PrintRenderFrameHelperTest, BasicBeforePrintAfterPrint) {
  LoadHTML(kBeforeAfterPrintHtml);
  ExpectNoBeforeNoAfterPrintEvent();

  print_manager()->SetExpectedPagesCount(1);
  OnPrintPages();

  VerifyPagesPrinted(true);
  ExpectOneBeforeOneAfterPrintEvent();
}

TEST_F(MAYBE_PrintRenderFrameHelperTest, BasicBeforePrintAfterPrintSubFrame) {
  static const char kCloseOnBeforeHtml[] =
      "<body>Hello"
      "<iframe name=sub srcdoc='<script>"
      "window.onbeforeprint = () => { window.frameElement.remove(); };"
      "</script>'></iframe>"
      "</body>";

  LoadHTML(kCloseOnBeforeHtml);
  OnPrintPagesInFrame("sub");
  EXPECT_EQ(nullptr, GetMainFrame()->FindFrameByName("sub"));
  VerifyPagesPrinted(false);

  ClearPrintManagerHost();

  static const char kCloseOnAfterHtml[] =
      "<body>Hello"
      "<iframe name=sub srcdoc='<script>"
      "window.onafterprint = () => { window.frameElement.remove(); };"
      "</script>'></iframe>"
      "</body>";

  LoadHTML(kCloseOnAfterHtml);
  OnPrintPagesInFrame("sub");
  EXPECT_EQ(nullptr, GetMainFrame()->FindFrameByName("sub"));
  VerifyPagesPrinted(true);
}

#if defined(OS_APPLE)
// TODO(estade): I don't think this test is worth porting to Linux. We will have
// to rip out and replace most of the IPC code if we ever plan to improve
// printing, and the comment below by sverrir suggests that it doesn't do much
// for us anyway.
TEST_F(MAYBE_PrintRenderFrameHelperTest, PrintWithIframe) {
  // Document that populates an iframe.
  static const char html[] =
      "<html><body>Lorem Ipsum:"
      "<iframe name=\"sub1\" id=\"sub1\"></iframe><script>"
      "  document.write(frames['sub1'].name);"
      "  frames['sub1'].document.write("
      "      '<p>Cras tempus ante eu felis semper luctus!</p>');"
      "  frames['sub1'].document.close();"
      "</script></body></html>";

  LoadHTML(html);

  // Find the frame and set it as the focused one.  This should mean that that
  // the printout should only contain the contents of that frame.
  auto* web_view = view_->GetWebView();
  WebFrame* sub1_frame =
      web_view->MainFrame()->ToWebLocalFrame()->FindFrameByName(
          WebString::FromUTF8("sub1"));
  ASSERT_TRUE(sub1_frame);
  web_view->SetFocusedFrame(sub1_frame);
  ASSERT_NE(web_view->FocusedFrame(), web_view->MainFrame());

  // Initiate printing.
  OnPrintPages();
  VerifyPagesPrinted(true);

  // Verify output through MockPrinter.
  const MockPrinter* printer(print_render_thread()->printer());
  ASSERT_EQ(1, printer->GetPrintedPages());
  const Image& image1(printer->GetPrintedPage(0)->image());

  // TODO(sverrir): Figure out a way to improve this test to actually print
  // only the content of the iframe.  Currently image1 will contain the full
  // page.
  EXPECT_NE(0, image1.size().width());
  EXPECT_NE(0, image1.size().height());
}
#endif  // defined(OS_APPLE)

// Tests if we can print a page and verify its results.
// This test prints HTML pages into a pseudo printer and check their outputs,
// i.e. a simplified version of the PrintingLayoutTextTest UI test.
namespace {
// Test cases used in this test.
struct TestPageData {
  const char* page;
  size_t printed_pages;
  int width;
  int height;
  const char* checksum;
  const wchar_t* file;
};

#if defined(OS_APPLE)
const TestPageData kTestPages[] = {
    {
        "<html>"
        "<head>"
        "<meta"
        "  http-equiv=\"Content-Type\""
        "  content=\"text/html; charset=utf-8\"/>"
        "<title>Test 1</title>"
        "</head>"
        "<body style=\"background-color: white;\">"
        "<p style=\"font-family: arial;\">Hello World!</p>"
        "</body>",
        1,
        // Mac printing code compensates for the WebKit scale factor while
        // generating the metafile, so we expect smaller pages. (On non-Mac
        // platforms, this would be 675x900).
        600, 780, nullptr, nullptr,
    },
};
#endif  // defined(OS_APPLE)
}  // namespace

// TODO(estade): need to port MockPrinter to get this on Linux. This involves
// hooking up Cairo to read a pdf stream, or accessing the cairo surface in the
// metafile directly.
// Same for printing via PDF on Windows.
#if defined(OS_APPLE)
TEST_F(MAYBE_PrintRenderFrameHelperTest, PrintLayoutTest) {
  bool baseline = false;

  EXPECT_TRUE(print_render_thread()->printer());
  for (size_t i = 0; i < base::size(kTestPages); ++i) {
    // Load an HTML page and print it.
    LoadHTML(kTestPages[i].page);
    OnPrintPages();
    VerifyPagesPrinted(true);

    // MockRenderThread::Send() just calls MockRenderThread::OnReceived().
    // So, all IPC messages sent in the above RenderView::OnPrintPages() call
    // has been handled by the MockPrinter object, i.e. this printing job
    // has been already finished.
    // So, we can start checking the output pages of this printing job.
    // Retrieve the number of pages actually printed.
    size_t pages = print_render_thread()->printer()->GetPrintedPages();
    EXPECT_EQ(kTestPages[i].printed_pages, pages);

    // Retrieve the width and height of the output page.
    int width = print_render_thread()->printer()->GetWidth(0);
    int height = print_render_thread()->printer()->GetHeight(0);

    // Check with margin for error.  This has been failing with a one pixel
    // offset on our buildbot.
    const int kErrorMargin = 5;  // 5%
    EXPECT_GT(kTestPages[i].width * (100 + kErrorMargin) / 100, width);
    EXPECT_LT(kTestPages[i].width * (100 - kErrorMargin) / 100, width);
    EXPECT_GT(kTestPages[i].height * (100 + kErrorMargin) / 100, height);
    EXPECT_LT(kTestPages[i].height * (100 - kErrorMargin) / 100, height);

    // Retrieve the checksum of the bitmap data from the pseudo printer and
    // compare it with the expected result.
    std::string bitmap_actual;
    EXPECT_TRUE(
        print_render_thread()->printer()->GetBitmapChecksum(0, &bitmap_actual));
    if (kTestPages[i].checksum)
      EXPECT_EQ(kTestPages[i].checksum, bitmap_actual);

    if (baseline) {
      // Save the source data and the bitmap data into temporary files to
      // create base-line results.
      base::FilePath source_path;
      base::CreateTemporaryFile(&source_path);
      print_render_thread()->printer()->SaveSource(0, source_path);

      base::FilePath bitmap_path;
      base::CreateTemporaryFile(&bitmap_path);
      print_render_thread()->printer()->SaveBitmap(0, bitmap_path);
    }
  }
}
#endif  // defined(OS_APPLE)

// These print preview tests do not work on Chrome OS yet.
#if !defined(OS_CHROMEOS)

// RenderViewTest-based tests crash on Android
// http://crbug.com/187500
#if defined(OS_ANDROID)
#define MAYBE_PrintRenderFrameHelperPreviewTest \
  DISABLED_PrintRenderFrameHelperPreviewTest
#else
#define MAYBE_PrintRenderFrameHelperPreviewTest \
  PrintRenderFrameHelperPreviewTest
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
class MAYBE_PrintRenderFrameHelperPreviewTest
    : public PrintRenderFrameHelperTestBase {
 public:
  MAYBE_PrintRenderFrameHelperPreviewTest() = default;
  MAYBE_PrintRenderFrameHelperPreviewTest(
      const MAYBE_PrintRenderFrameHelperPreviewTest&) = delete;
  MAYBE_PrintRenderFrameHelperPreviewTest& operator=(
      const MAYBE_PrintRenderFrameHelperPreviewTest&) = delete;
  ~MAYBE_PrintRenderFrameHelperPreviewTest() override = default;

  void SetUp() override {
    PrintRenderFrameHelperTestBase::SetUp();
    BindToFakePrintPreviewUI();
  }

 protected:
  void VerifyPrintPreviewCancelled(bool expect_cancel) {
    EXPECT_EQ(expect_cancel, preview_ui()->preview_cancelled());
  }

  void VerifyPrintPreviewFailed(bool expect_fail) {
    EXPECT_EQ(expect_fail, preview_ui()->preview_failed());
  }

  void VerifyPrintPreviewGenerated(bool expect_generated) {
    const IPC::Message* preview_msg =
        render_thread_->sink().GetUniqueMessageMatching(
            PrintHostMsg_MetafileReadyForPrinting::ID);
    bool got_preview_msg = !!preview_msg;
    ASSERT_EQ(expect_generated, got_preview_msg);
    if (got_preview_msg) {
      PrintHostMsg_MetafileReadyForPrinting::Param preview_param;
      PrintHostMsg_MetafileReadyForPrinting::Read(preview_msg, &preview_param);
      const auto& param = std::get<0>(preview_param);
      EXPECT_NE(0, param.document_cookie);
      EXPECT_NE(0U, param.expected_pages_count);
      EXPECT_NE(0U, param.content->metafile_data_region.GetSize());
    }
  }

  void VerifyPrintPreviewInvalidPrinterSettings(bool expect_invalid_settings) {
    EXPECT_EQ(expect_invalid_settings, preview_ui()->invalid_printer_setting());
  }

  // |page_number| is 0-based.
  void VerifyDidPreviewPage(bool expect_generated, uint32_t page_number) {
    bool msg_found = false;
    uint32_t data_size = 0;
    for (const auto& preview : print_render_thread()->print_preview_pages()) {
      if (preview.first == page_number) {
        msg_found = true;
        data_size = preview.second;
        break;
      }
    }
    EXPECT_EQ(expect_generated, msg_found) << "For page " << page_number;
    if (expect_generated)
      EXPECT_NE(0U, data_size) << "For page " << page_number;
  }

  void VerifyDefaultPageLayout(int expected_content_width,
                               int expected_content_height,
                               int expected_margin_top,
                               int expected_margin_bottom,
                               int expected_margin_left,
                               int expected_margin_right,
                               bool expected_page_has_print_css) {
    const IPC::Message* default_page_layout_msg =
        render_thread_->sink().GetUniqueMessageMatching(
            PrintHostMsg_DidGetDefaultPageLayout::ID);
    bool did_get_default_page_layout_msg = !!default_page_layout_msg;
    EXPECT_TRUE(did_get_default_page_layout_msg);
    if (!did_get_default_page_layout_msg)
      return;

    PrintHostMsg_DidGetDefaultPageLayout::Param param;
    PrintHostMsg_DidGetDefaultPageLayout::Read(default_page_layout_msg, &param);
    EXPECT_EQ(expected_content_width, std::get<0>(param).content_width);
    EXPECT_EQ(expected_content_height, std::get<0>(param).content_height);
    EXPECT_EQ(expected_margin_top, std::get<0>(param).margin_top);
    EXPECT_EQ(expected_margin_right, std::get<0>(param).margin_right);
    EXPECT_EQ(expected_margin_left, std::get<0>(param).margin_left);
    EXPECT_EQ(expected_margin_bottom, std::get<0>(param).margin_bottom);
    EXPECT_EQ(expected_page_has_print_css, std::get<2>(param));
  }
};

TEST_F(MAYBE_PrintRenderFrameHelperPreviewTest, BlockScriptInitiatedPrinting) {
  LoadHTML(kHelloWorldHTML);
  PrintRenderFrameHelper* print_render_frame_helper =
      GetPrintRenderFrameHelper();
  print_render_frame_helper->SetPrintingEnabled(false);
  PrintWithJavaScript();
  VerifyPreviewRequest(false);

  print_render_frame_helper->SetPrintingEnabled(true);
  PrintWithJavaScript();
  VerifyPreviewRequest(true);

  OnClosePrintPreviewDialog();
}

TEST_F(MAYBE_PrintRenderFrameHelperPreviewTest, PrintWithJavaScript) {
  LoadHTML(kPrintOnUserAction);
  gfx::Size new_size(200, 100);
  Resize(new_size, false);

  gfx::Rect bounds = GetElementBounds("print");
  ClickMouseButton(bounds);

  VerifyPreviewRequest(true);

  OnClosePrintPreviewDialog();
}

// Tests that print preview work and sending and receiving messages through
// that channel all works.
TEST_F(MAYBE_PrintRenderFrameHelperPreviewTest, OnPrintPreview) {
  LoadHTML(kHelloWorldHTML);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  OnPrintPreview(dict);

  EXPECT_EQ(0u, print_render_thread()->print_preview_pages_remaining());
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyDefaultPageLayout(540, 720, 36, 36, 36, 36, false);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

TEST_F(MAYBE_PrintRenderFrameHelperPreviewTest,
       PrintPreviewHTMLWithPageMarginsCss) {
  // A simple web page with print margins css.
  static const char kHTMLWithPageMarginsCss[] =
      "<html><head><style>"
      "@media print {"
      "  @page {"
      "     margin: 3in 1in 2in 0.3in;"
      "  }"
      "}"
      "</style></head>"
      "<body>Lorem Ipsum:"
      "</body></html>";
  LoadHTML(kHTMLWithPageMarginsCss);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  dict.SetInteger(kSettingPrinterType, static_cast<int>(PrinterType::kLocal));
  OnPrintPreview(dict);

  EXPECT_EQ(0u, print_render_thread()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(519, 432, 216, 144, 21, 72, false);
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

// Test to verify that print preview ignores print media css when non-default
// margin is selected.
TEST_F(MAYBE_PrintRenderFrameHelperPreviewTest,
       NonDefaultMarginsSelectedIgnorePrintCss) {
  LoadHTML(kHTMLWithPageSizeCss);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  dict.SetInteger(kSettingPrinterType, static_cast<int>(PrinterType::kLocal));
  dict.SetInteger(kSettingMarginsType,
                  static_cast<int>(mojom::MarginType::kNoMargins));
  OnPrintPreview(dict);

  EXPECT_EQ(0u, print_render_thread()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(612, 792, 0, 0, 0, 0, true);
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

// Test to verify that print preview honor print media size css when
// PRINT_TO_PDF is selected and doesn't fit to printer default paper size.
TEST_F(MAYBE_PrintRenderFrameHelperPreviewTest,
       PrintToPDFSelectedHonorPrintCss) {
  LoadHTML(kHTMLWithPageSizeCss);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  dict.SetInteger(kSettingMarginsType,
                  static_cast<int>(mojom::MarginType::kPrintableAreaMargins));
  OnPrintPreview(dict);

  EXPECT_EQ(0u, print_render_thread()->print_preview_pages_remaining());
  // Since PRINT_TO_PDF is selected, pdf page size is equal to print media page
  // size.
  VerifyDefaultPageLayout(252, 252, 18, 18, 18, 18, true);
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

TEST_F(MAYBE_PrintRenderFrameHelperPreviewTest,
       PreviewLayoutTriggeredByResize) {
  // A simple web page with print margins css.
  static const char kHTMLWithPageCss[] =
      "<!DOCTYPE html>"
      "<style>"
      "@media (min-width: 540px) {"
      "  #container {"
      "    width: 540px;"
      "  }"
      "}"
      ".testlist {"
      "  list-style-type: none;"
      "}"
      "</style>"
      "<div id='container'>"
      "  <ul class='testlist'>"
      "    <li>"
      "      <p>"
      "      'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "      bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
      "      ccccccccccccccccccccccccccccccccccccccc"
      "      ddddddddddddddddddddddddddddddddddddddd"
      "      eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
      "      fffffffffffffffffffffffffffffffffffffff"
      "      ggggggggggggggggggggggggggggggggggggggg"
      "      hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"
      "      iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii"
      "      jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj"
      "      kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk"
      "      lllllllllllllllllllllllllllllllllllllll"
      "      mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm':"
      "      </p>"
      "    </li>"
      "    <li>"
      "      <p>"
      "      'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "      bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
      "      ccccccccccccccccccccccccccccccccccccccc"
      "      ddddddddddddddddddddddddddddddddddddddd"
      "      eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
      "      fffffffffffffffffffffffffffffffffffffff"
      "      ggggggggggggggggggggggggggggggggggggggg"
      "      hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"
      "      iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii"
      "      jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj"
      "      kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk"
      "      lllllllllllllllllllllllllllllllllllllll"
      "      mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm'"
      "      </p>"
      "    </li>"
      "    <li>"
      "      <p>"
      "      'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "      bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
      "      ccccccccccccccccccccccccccccccccccccccc"
      "      ddddddddddddddddddddddddddddddddddddddd"
      "      eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
      "      fffffffffffffffffffffffffffffffffffffff"
      "      ggggggggggggggggggggggggggggggggggggggg"
      "      hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"
      "      iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii"
      "      jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj"
      "      kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk"
      "      lllllllllllllllllllllllllllllllllllllll"
      "      mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm'"
      "      </p>"
      "    </li>"
      "    <li>"
      "      <p>"
      "      'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "      bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
      "      ccccccccccccccccccccccccccccccccccccccc"
      "      ddddddddddddddddddddddddddddddddddddddd"
      "      eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
      "      fffffffffffffffffffffffffffffffffffffff"
      "      ggggggggggggggggggggggggggggggggggggggg"
      "      hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"
      "      iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii"
      "      jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj"
      "      kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk"
      "      lllllllllllllllllllllllllllllllllllllll"
      "      mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm'"
      "      </p>"
      "    </li>"
      "    <li>"
      "      <p>"
      "      'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "      bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
      "      ccccccccccccccccccccccccccccccccccccccc"
      "      ddddddddddddddddddddddddddddddddddddddd"
      "      eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
      "      fffffffffffffffffffffffffffffffffffffff"
      "      ggggggggggggggggggggggggggggggggggggggg"
      "      hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"
      "      iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii"
      "      jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj"
      "      kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk"
      "      lllllllllllllllllllllllllllllllllllllll"
      "      mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm'"
      "      </p>"
      "    </li>"
      "  </ul>"
      "</div>";
  LoadHTML(kHTMLWithPageCss);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  dict.SetInteger(kSettingPrinterType, static_cast<int>(PrinterType::kLocal));
  OnPrintPreview(dict);

  EXPECT_EQ(0u, print_render_thread()->print_preview_pages_remaining());
  VerifyDidPreviewPage(true, 0);
  VerifyDidPreviewPage(true, 1);
  VerifyPreviewPageCount(2);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

// Test to verify that print preview honor print margin css when PRINT_TO_PDF
// is selected and doesn't fit to printer default paper size.
TEST_F(MAYBE_PrintRenderFrameHelperPreviewTest,
       PrintToPDFSelectedHonorPageMarginsCss) {
  // A simple web page with print margins css.
  static const char kHTMLWithPageCss[] =
      "<html><head><style>"
      "@media print {"
      "  @page {"
      "     margin: 3in 1in 2in 0.3in;"
      "     size: 14in 14in;"
      "  }"
      "}"
      "</style></head>"
      "<body>Lorem Ipsum:"
      "</body></html>";
  LoadHTML(kHTMLWithPageCss);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  OnPrintPreview(dict);

  EXPECT_EQ(0u, print_render_thread()->print_preview_pages_remaining());
  // Since PRINT_TO_PDF is selected, pdf page size is equal to print media page
  // size.
  VerifyDefaultPageLayout(915, 648, 216, 144, 21, 72, true);
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

// Test to verify that print preview workflow center the html page contents to
// fit the page size.
TEST_F(MAYBE_PrintRenderFrameHelperPreviewTest, PrintPreviewCenterToFitPage) {
  LoadHTML(kHTMLWithPageSizeCss);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  dict.SetInteger(kSettingPrinterType, static_cast<int>(PrinterType::kLocal));
  OnPrintPreview(dict);

  EXPECT_EQ(0u, print_render_thread()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(216, 216, 288, 288, 198, 198, true);
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

// Test to verify that print preview workflow scale the html page contents to
// fit the page size.
TEST_F(MAYBE_PrintRenderFrameHelperPreviewTest, PrintPreviewShrinkToFitPage) {
  // A simple web page with print margins css.
  static const char kHTMLWithPageCss[] =
      "<html><head><style>"
      "@media print {"
      "  @page {"
      "     size: 15in 17in;"
      "  }"
      "}"
      "</style></head>"
      "<body>Lorem Ipsum:"
      "</body></html>";
  LoadHTML(kHTMLWithPageCss);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  dict.SetInteger(kSettingPrinterType, static_cast<int>(PrinterType::kLocal));
  OnPrintPreview(dict);

  EXPECT_EQ(0u, print_render_thread()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(571, 652, 69, 71, 20, 21, true);
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

// Test to verify that print preview workflow honor the orientation settings
// specified in css.
TEST_F(MAYBE_PrintRenderFrameHelperPreviewTest,
       PrintPreviewHonorsOrientationCss) {
  LoadHTML(kHTMLWithLandscapePageCss);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  dict.SetInteger(kSettingPrinterType, static_cast<int>(PrinterType::kLocal));
  dict.SetInteger(kSettingMarginsType,
                  static_cast<int>(mojom::MarginType::kNoMargins));
  OnPrintPreview(dict);

  EXPECT_EQ(0u, print_render_thread()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(792, 612, 0, 0, 0, 0, true);
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

// Test to verify that print preview workflow honors the orientation settings
// specified in css when PRINT_TO_PDF is selected.
TEST_F(MAYBE_PrintRenderFrameHelperPreviewTest,
       PrintToPDFSelectedHonorOrientationCss) {
  LoadHTML(kHTMLWithLandscapePageCss);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  dict.SetInteger(kSettingMarginsType,
                  static_cast<int>(mojom::MarginType::kCustomMargins));
  OnPrintPreview(dict);

  EXPECT_EQ(0u, print_render_thread()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(748, 568, 21, 23, 21, 23, true);
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

TEST_F(MAYBE_PrintRenderFrameHelperPreviewTest, PrintPreviewForMultiplePages) {
  LoadHTML(kMultipageHTML);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);

  OnPrintPreview(dict);

  EXPECT_EQ(0u, print_render_thread()->print_preview_pages_remaining());
  VerifyDidPreviewPage(true, 0);
  VerifyDidPreviewPage(true, 1);
  VerifyDidPreviewPage(true, 2);
  VerifyPreviewPageCount(3);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

TEST_F(MAYBE_PrintRenderFrameHelperPreviewTest, PrintPreviewForSelectedPages) {
  LoadHTML(kMultipageHTML);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);

  // Set a page range and update the dictionary to generate only the complete
  // metafile with the selected pages. Page numbers used in the dictionary
  // are 1-based.
  base::Value page_range(base::Value::Type::DICTIONARY);
  page_range.SetKey(kSettingPageRangeFrom, base::Value(2));
  page_range.SetKey(kSettingPageRangeTo, base::Value(3));
  base::Value page_range_array(base::Value::Type::LIST);
  page_range_array.Append(std::move(page_range));
  dict.SetKey(kSettingPageRange, std::move(page_range_array));

  OnPrintPreview(dict);

  // The expected page count below is 3 because the total number of pages in the
  // document, without the page range, is 3. Since only 2 pages have been
  // generated, the print_preview_pages_remaining() result is 1.
  // TODO(thestig): Fix this on the browser side to accept the number of actual
  // pages generated instead, or to take both page counts.
  EXPECT_EQ(1u, print_render_thread()->print_preview_pages_remaining());
  VerifyDidPreviewPage(false, 0);
  VerifyDidPreviewPage(true, 1);
  VerifyDidPreviewPage(true, 2);
  VerifyPreviewPageCount(3);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

// Test to verify that preview generated only for one page.
TEST_F(MAYBE_PrintRenderFrameHelperPreviewTest, PrintPreviewForSelectedText) {
  LoadHTML(kMultipageHTML);
  GetMainFrame()->SelectRange(blink::WebRange(1, 3),
                              blink::WebLocalFrame::kHideSelectionHandle,
                              blink::mojom::SelectionMenuBehavior::kHide);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  dict.SetBoolean(kSettingShouldPrintSelectionOnly, true);

  OnPrintPreview(dict);

  EXPECT_EQ(0u, print_render_thread()->print_preview_pages_remaining());
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

// Test to verify that preview generated only for two pages.
TEST_F(MAYBE_PrintRenderFrameHelperPreviewTest, PrintPreviewForSelectedText2) {
  LoadHTML(kMultipageHTML);
  GetMainFrame()->SelectRange(blink::WebRange(1, 8),
                              blink::WebLocalFrame::kHideSelectionHandle,
                              blink::mojom::SelectionMenuBehavior::kHide);

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  dict.SetBoolean(kSettingShouldPrintSelectionOnly, true);

  OnPrintPreview(dict);

  EXPECT_EQ(0u, print_render_thread()->print_preview_pages_remaining());
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(2);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

// Tests that cancelling print preview works.
TEST_F(MAYBE_PrintRenderFrameHelperPreviewTest, PrintPreviewCancel) {
  LoadHTML(kLongPageHTML);

  const uint32_t kCancelPage = 3;
  print_render_thread()->set_print_preview_cancel_page_number(kCancelPage);
  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  OnPrintPreview(dict);

  EXPECT_EQ(kCancelPage,
            print_render_thread()->print_preview_pages_remaining());
  VerifyPrintPreviewCancelled(true);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(false);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

// Tests that when default printer has invalid printer settings, print preview
// receives error message.
TEST_F(MAYBE_PrintRenderFrameHelperPreviewTest,
       OnPrintPreviewUsingInvalidPrinterSettings) {
  LoadHTML(kPrintPreviewHTML);

  // Set mock printer to provide invalid settings.
  print_render_thread()->printer()->UseInvalidSettings();

  // Fill in some dummy values.
  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  OnPrintPreview(dict);

  // We should have received invalid printer settings from |printer_|.
  VerifyPrintPreviewInvalidPrinterSettings(true);
  EXPECT_EQ(0u, print_render_thread()->print_preview_pages_remaining());

  // It should receive the invalid printer settings message only.
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(false);

  OnClosePrintPreviewDialog();
}

// Tests that when the selected printer has invalid page settings, print preview
// receives error message.
TEST_F(MAYBE_PrintRenderFrameHelperPreviewTest,
       OnPrintPreviewUsingInvalidPageSize) {
  LoadHTML(kPrintPreviewHTML);

  print_render_thread()->printer()->UseInvalidPageSize();

  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  OnPrintPreview(dict);

  VerifyPrintPreviewInvalidPrinterSettings(true);
  EXPECT_EQ(0u, print_render_thread()->print_preview_pages_remaining());

  // It should receive the invalid printer settings message only.
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(false);

  OnClosePrintPreviewDialog();
}

// Tests that when the selected printer has invalid content settings, print
// preview receives error message.
TEST_F(MAYBE_PrintRenderFrameHelperPreviewTest,
       OnPrintPreviewUsingInvalidContentSize) {
  LoadHTML(kPrintPreviewHTML);

  print_render_thread()->printer()->UseInvalidContentSize();

  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  OnPrintPreview(dict);

  VerifyPrintPreviewInvalidPrinterSettings(true);
  EXPECT_EQ(0u, print_render_thread()->print_preview_pages_remaining());

  // It should receive the invalid printer settings message only.
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(false);

  OnClosePrintPreviewDialog();
}

TEST_F(MAYBE_PrintRenderFrameHelperPreviewTest, BasicBeforePrintAfterPrint) {
  LoadHTML(kBeforeAfterPrintHtml);
  ExpectNoBeforeNoAfterPrintEvent();

  base::DictionaryValue dict;
  CreatePrintSettingsDictionary(&dict);
  OnPrintPreview(dict);

  EXPECT_EQ(0u, print_render_thread()->print_preview_pages_remaining());
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyDefaultPageLayout(540, 720, 36, 36, 36, 36, false);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);
  ExpectOneBeforeNoAfterPrintEvent();

  OnClosePrintPreviewDialog();
  ExpectOneBeforeOneAfterPrintEvent();
}

// Regression test for https://crbug.com/912966
TEST_F(MAYBE_PrintRenderFrameHelperPreviewTest,
       WindowPrintBeforePrintAfterPrint) {
  LoadHTML(kBeforeAfterPrintHtml);
  gfx::Size new_size(200, 100);
  Resize(new_size, false);
  ExpectNoBeforeNoAfterPrintEvent();

  gfx::Rect bounds = GetElementBounds("print");
  ClickMouseButton(bounds);

  VerifyPreviewRequest(true);
  ExpectOneBeforeNoAfterPrintEvent();

  OnClosePrintPreviewDialog();
  ExpectOneBeforeOneAfterPrintEvent();
}

#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#endif  // !defined(OS_CHROMEOS)

}  // namespace printing
