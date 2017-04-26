/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/web/WebFrame.h"

#include <stdarg.h>

#include <map>
#include <memory>

#include "SkBitmap.h"
#include "SkCanvas.h"
#include "bindings/core/v8/SerializedScriptValueFactory.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8Node.h"
#include "bindings/core/v8/serialization/V8ScriptValueSerializer.h"
#include "core/clipboard/DataTransfer.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/css/resolver/ViewportStyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentUserGestureToken.h"
#include "core/dom/Fullscreen.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/Range.h"
#include "core/editing/Editor.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/editing/spellcheck/IdleSpellCheckCallback.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/events/MouseEvent.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageDocument.h"
#include "core/input/EventHandler.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutFullScreen.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/DocumentThreadableLoader.h"
#include "core/loader/DocumentThreadableLoaderClient.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/ThreadableLoader.h"
#include "core/page/Page.h"
#include "core/page/ScopedPageSuspender.h"
#include "core/paint/PaintLayer.h"
#include "core/testing/NullExecutionContext.h"
#include "modules/mediastream/MediaStream.h"
#include "modules/mediastream/MediaStreamRegistry.h"
#include "platform/Cursor.h"
#include "platform/DragImage.h"
#include "platform/KeyboardCodes.h"
#include "platform/PlatformResourceLoader.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/UserGestureIndicator.h"
#include "platform/geometry/FloatRect.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/scroll/Scrollbar.h"
#include "platform/scroll/ScrollbarTestSuite.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/scroll/ScrollbarThemeMock.h"
#include "platform/scroll/ScrollbarThemeOverlayMock.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/weborigin/KURLHash.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/dtoa/utils.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCache.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebClipboard.h"
#include "public/platform/WebFloatRect.h"
#include "public/platform/WebKeyboardEvent.h"
#include "public/platform/WebMockClipboard.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLLoaderClient.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLResponse.h"
#include "public/web/WebConsoleMessage.h"
#include "public/web/WebContextMenuData.h"
#include "public/web/WebDataSource.h"
#include "public/web/WebDeviceEmulationParams.h"
#include "public/web/WebDocument.h"
#include "public/web/WebFindOptions.h"
#include "public/web/WebFormElement.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebFrameContentDumper.h"
#include "public/web/WebFrameWidget.h"
#include "public/web/WebHistoryItem.h"
#include "public/web/WebPrintParams.h"
#include "public/web/WebRange.h"
#include "public/web/WebRemoteFrame.h"
#include "public/web/WebScriptExecutionCallback.h"
#include "public/web/WebScriptSource.h"
#include "public/web/WebSearchableFormData.h"
#include "public/web/WebSecurityPolicy.h"
#include "public/web/WebSelection.h"
#include "public/web/WebSettings.h"
#include "public/web/WebTextCheckClient.h"
#include "public/web/WebTextCheckingCompletion.h"
#include "public/web/WebTextCheckingResult.h"
#include "public/web/WebViewClient.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"
#include "web/TextFinder.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebRemoteFrameImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include "web/tests/sim/SimDisplayItemList.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimTest.h"

using blink::URLTestHelpers::ToKURL;
using blink::testing::RunPendingTasks;
using testing::ElementsAre;
using testing::Mock;
using testing::_;

namespace blink {

::std::ostream& operator<<(::std::ostream& os, const WebFloatSize& size) {
  return os << "WebFloatSize: [" << size.width << ", " << size.height << "]";
}

::std::ostream& operator<<(::std::ostream& os, const WebFloatPoint& point) {
  return os << "WebFloatPoint: [" << point.x << ", " << point.y << "]";
}

const int kTouchPointPadding = 32;

#define EXPECT_RECT_EQ(expected, actual)           \
  do {                                             \
    EXPECT_EQ(expected.X(), actual.X());           \
    EXPECT_EQ(expected.Y(), actual.Y());           \
    EXPECT_EQ(expected.Width(), actual.Width());   \
    EXPECT_EQ(expected.Height(), actual.Height()); \
  } while (false)

#define EXPECT_SIZE_EQ(expected, actual)           \
  do {                                             \
    EXPECT_EQ(expected.Width(), actual.Width());   \
    EXPECT_EQ(expected.Height(), actual.Height()); \
  } while (false)

#define EXPECT_FLOAT_POINT_EQ(expected, actual) \
  do {                                          \
    EXPECT_FLOAT_EQ(expected.x(), actual.x());  \
    EXPECT_FLOAT_EQ(expected.y(), actual.y());  \
  } while (false)

class WebFrameTest : public ::testing::Test {
 protected:
  WebFrameTest()
      : base_url_("http://internal.test/"),
        not_base_url_("http://external.test/"),
        chrome_url_("chrome://") {}

  ~WebFrameTest() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  void RegisterMockedHttpURLLoad(const std::string& file_name) {
    RegisterMockedURLLoadFromBase(base_url_, file_name);
  }

  void RegisterMockedChromeURLLoad(const std::string& file_name) {
    RegisterMockedURLLoadFromBase(chrome_url_, file_name);
  }

  void RegisterMockedURLLoadFromBase(const std::string& base_url,
                                     const std::string& file_name) {
    URLTestHelpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url), testing::WebTestDataPath(),
        WebString::FromUTF8(file_name));
  }

  void RegisterMockedHttpURLLoadWithCSP(const std::string& file_name,
                                        const std::string& csp,
                                        bool report_only = false) {
    WebURLResponse response;
    response.SetMIMEType("text/html");
    response.AddHTTPHeaderField(
        report_only ? WebString("Content-Security-Policy-Report-Only")
                    : WebString("Content-Security-Policy"),
        WebString::FromUTF8(csp));
    std::string full_string = base_url_ + file_name;
    URLTestHelpers::RegisterMockedURLLoadWithCustomResponse(
        ToKURL(full_string),
        testing::WebTestDataPath(WebString::FromUTF8(file_name)), response);
  }

  void RegisterMockedHttpURLLoadWithMimeType(const std::string& file_name,
                                             const std::string& mime_type) {
    URLTestHelpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), testing::WebTestDataPath(),
        WebString::FromUTF8(file_name), WebString::FromUTF8(mime_type));
  }

  void ApplyViewportStyleOverride(
      FrameTestHelpers::WebViewHelper* web_view_helper) {
    web_view_helper->WebView()->GetSettings()->SetViewportStyle(
        WebViewportStyle::kMobile);
  }

  static void ConfigureCompositingWebView(WebSettings* settings) {
    settings->SetAcceleratedCompositingEnabled(true);
    settings->SetPreferCompositingToLCDTextEnabled(true);
  }

  static void ConfigureAndroid(WebSettings* settings) {
    settings->SetViewportMetaEnabled(true);
    settings->SetViewportEnabled(true);
    settings->SetMainFrameResizesAreOrientationChanges(true);
    settings->SetShrinksViewportContentToFit(true);
  }

  static void ConfigureLoadsImagesAutomatically(WebSettings* settings) {
    settings->SetLoadsImagesAutomatically(true);
  }

  void InitializeTextSelectionWebView(
      const std::string& url,
      FrameTestHelpers::WebViewHelper* web_view_helper) {
    web_view_helper->InitializeAndLoad(url, true);
    web_view_helper->WebView()->GetSettings()->SetDefaultFontSize(12);
    web_view_helper->Resize(WebSize(640, 480));
  }

  std::unique_ptr<DragImage> NodeImageTestSetup(
      FrameTestHelpers::WebViewHelper* web_view_helper,
      const std::string& testcase) {
    RegisterMockedHttpURLLoad("nodeimage.html");
    web_view_helper->InitializeAndLoad(base_url_ + "nodeimage.html");
    web_view_helper->Resize(WebSize(640, 480));
    LocalFrame* frame =
        ToLocalFrame(web_view_helper->WebView()->GetPage()->MainFrame());
    DCHECK(frame);
    Element* element = frame->GetDocument()->GetElementById(testcase.c_str());
    return frame->NodeImage(*element);
  }

  void RemoveElementById(WebLocalFrameImpl* frame, const AtomicString& id) {
    Element* element = frame->GetFrame()->GetDocument()->GetElementById(id);
    DCHECK(element);
    element->remove();
  }

  // Both sets the inner html and runs the document lifecycle.
  void InitializeWithHTML(LocalFrame& frame, const String& html_content) {
    frame.GetDocument()->body()->setInnerHTML(html_content);
    frame.GetDocument()->View()->UpdateAllLifecyclePhases();
  }

  WebFrame* LastChild(WebFrame* frame) { return frame->last_child_; }
  WebFrame* PreviousSibling(WebFrame* frame) {
    return frame->previous_sibling_;
  }
  void SwapAndVerifyFirstChildConsistency(const char* const message,
                                          WebFrame* parent,
                                          WebFrame* new_child);
  void SwapAndVerifyMiddleChildConsistency(const char* const message,
                                           WebFrame* parent,
                                           WebFrame* new_child);
  void SwapAndVerifyLastChildConsistency(const char* const message,
                                         WebFrame* parent,
                                         WebFrame* new_child);
  void SwapAndVerifySubframeConsistency(const char* const message,
                                        WebFrame* parent,
                                        WebFrame* new_child);

  std::string base_url_;
  std::string not_base_url_;
  std::string chrome_url_;
};

typedef bool TestParamRootLayerScrolling;
class ParameterizedWebFrameTest
    : public ::testing::WithParamInterface<TestParamRootLayerScrolling>,
      private ScopedRootLayerScrollingForTest,
      public WebFrameTest {
 public:
  ParameterizedWebFrameTest() : ScopedRootLayerScrollingForTest(GetParam()) {}
};

INSTANTIATE_TEST_CASE_P(All, ParameterizedWebFrameTest, ::testing::Bool());

TEST_P(ParameterizedWebFrameTest, ContentText) {
  RegisterMockedHttpURLLoad("iframes_test.html");
  RegisterMockedHttpURLLoad("visible_iframe.html");
  RegisterMockedHttpURLLoad("invisible_iframe.html");
  RegisterMockedHttpURLLoad("zero_sized_iframe.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "iframes_test.html");

  // Now retrieve the frames text and test it only includes visible elements.
  std::string content =
      WebFrameContentDumper::DumpWebViewAsText(web_view_helper.WebView(), 1024)
          .Utf8();
  EXPECT_NE(std::string::npos, content.find(" visible paragraph"));
  EXPECT_NE(std::string::npos, content.find(" visible iframe"));
  EXPECT_EQ(std::string::npos, content.find(" invisible pararaph"));
  EXPECT_EQ(std::string::npos, content.find(" invisible iframe"));
  EXPECT_EQ(std::string::npos, content.find("iframe with zero size"));
}

TEST_P(ParameterizedWebFrameTest, FrameForEnteredContext) {
  RegisterMockedHttpURLLoad("iframes_test.html");
  RegisterMockedHttpURLLoad("visible_iframe.html");
  RegisterMockedHttpURLLoad("invisible_iframe.html");
  RegisterMockedHttpURLLoad("zero_sized_iframe.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "iframes_test.html", true);

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  EXPECT_EQ(
      web_view_helper.WebView()->MainFrame(),
      WebLocalFrame::FrameForContext(
          web_view_helper.WebView()->MainFrame()->MainWorldScriptContext()));
  EXPECT_EQ(web_view_helper.WebView()->MainFrame()->FirstChild(),
            WebLocalFrame::FrameForContext(web_view_helper.WebView()
                                               ->MainFrame()
                                               ->FirstChild()
                                               ->MainWorldScriptContext()));
}

class ScriptExecutionCallbackHelper : public WebScriptExecutionCallback {
 public:
  explicit ScriptExecutionCallbackHelper(v8::Local<v8::Context> context)
      : did_complete_(false), bool_value_(false), context_(context) {}
  ~ScriptExecutionCallbackHelper() {}

  bool DidComplete() const { return did_complete_; }
  const String& StringValue() const { return string_value_; }
  bool BoolValue() { return bool_value_; }

 private:
  void Completed(const WebVector<v8::Local<v8::Value>>& values) override {
    did_complete_ = true;
    if (!values.IsEmpty()) {
      if (values[0]->IsString()) {
        string_value_ =
            ToCoreString(values[0]->ToString(context_).ToLocalChecked());
      } else if (values[0]->IsBoolean()) {
        bool_value_ = values[0].As<v8::Boolean>()->Value();
      }
    }
  }

  bool did_complete_;
  String string_value_;
  bool bool_value_;
  v8::Local<v8::Context> context_;
};

TEST_P(ParameterizedWebFrameTest, RequestExecuteScript) {
  RegisterMockedHttpURLLoad("foo.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html", true);

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  ScriptExecutionCallbackHelper callback_helper(
      web_view_helper.WebView()->MainFrame()->MainWorldScriptContext());
  web_view_helper.WebView()
      ->MainFrame()
      ->ToWebLocalFrame()
      ->RequestExecuteScriptAndReturnValue(
          WebScriptSource(WebString("'hello';")), false, &callback_helper);
  RunPendingTasks();
  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_EQ("hello", callback_helper.StringValue());
}

TEST_P(ParameterizedWebFrameTest, SuspendedRequestExecuteScript) {
  RegisterMockedHttpURLLoad("foo.html");
  RegisterMockedHttpURLLoad("bar.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html", true);

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  ScriptExecutionCallbackHelper callback_helper(
      web_view_helper.WebView()->MainFrame()->MainWorldScriptContext());

  // Suspend scheduled tasks so the script doesn't run.
  web_view_helper.WebView()
      ->MainFrameImpl()
      ->GetFrame()
      ->GetDocument()
      ->SuspendScheduledTasks();
  web_view_helper.WebView()
      ->MainFrameImpl()
      ->RequestExecuteScriptAndReturnValue(
          WebScriptSource(WebString("'hello';")), false, &callback_helper);
  RunPendingTasks();
  EXPECT_FALSE(callback_helper.DidComplete());

  // If the frame navigates, pending scripts should be removed, but the callback
  // should always be ran.
  FrameTestHelpers::LoadFrame(web_view_helper.WebView()->MainFrame(),
                              base_url_ + "bar.html");
  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_EQ(String(), callback_helper.StringValue());
}

TEST_P(ParameterizedWebFrameTest, RequestExecuteV8Function) {
  RegisterMockedHttpURLLoad("foo.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html", true);

  auto callback = [](const v8::FunctionCallbackInfo<v8::Value>& info) {
    info.GetReturnValue().Set(V8String(info.GetIsolate(), "hello"));
  };

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Context> context =
      web_view_helper.WebView()->MainFrame()->MainWorldScriptContext();
  ScriptExecutionCallbackHelper callback_helper(context);
  v8::Local<v8::Function> function =
      v8::Function::New(context, callback).ToLocalChecked();
  web_view_helper.WebView()
      ->MainFrame()
      ->ToWebLocalFrame()
      ->RequestExecuteV8Function(context, function,
                                 v8::Undefined(context->GetIsolate()), 0,
                                 nullptr, &callback_helper);
  RunPendingTasks();
  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_EQ("hello", callback_helper.StringValue());
}

TEST_P(ParameterizedWebFrameTest, RequestExecuteV8FunctionWhileSuspended) {
  RegisterMockedHttpURLLoad("foo.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html", true);

  auto callback = [](const v8::FunctionCallbackInfo<v8::Value>& info) {
    info.GetReturnValue().Set(V8String(info.GetIsolate(), "hello"));
  };

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Context> context =
      web_view_helper.WebView()->MainFrame()->MainWorldScriptContext();

  // Suspend scheduled tasks so the script doesn't run.
  WebLocalFrameImpl* main_frame = web_view_helper.WebView()->MainFrameImpl();
  main_frame->GetFrame()->GetDocument()->SuspendScheduledTasks();

  ScriptExecutionCallbackHelper callback_helper(context);
  v8::Local<v8::Function> function =
      v8::Function::New(context, callback).ToLocalChecked();
  main_frame->RequestExecuteV8Function(context, function,
                                       v8::Undefined(context->GetIsolate()), 0,
                                       nullptr, &callback_helper);
  RunPendingTasks();
  EXPECT_FALSE(callback_helper.DidComplete());

  main_frame->GetFrame()->GetDocument()->ResumeScheduledTasks();
  RunPendingTasks();
  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_EQ("hello", callback_helper.StringValue());
}

TEST_P(ParameterizedWebFrameTest,
       RequestExecuteV8FunctionWhileSuspendedWithUserGesture) {
  RegisterMockedHttpURLLoad("foo.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html", true);

  auto callback = [](const v8::FunctionCallbackInfo<v8::Value>& info) {
    info.GetReturnValue().Set(v8::Boolean::New(
        info.GetIsolate(), UserGestureIndicator::ProcessingUserGesture()));
  };

  // Suspend scheduled tasks so the script doesn't run.
  WebLocalFrameImpl* main_frame = web_view_helper.WebView()->MainFrameImpl();
  Document* document = main_frame->GetFrame()->GetDocument();
  document->SuspendScheduledTasks();

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Context> context =
      web_view_helper.WebView()->MainFrame()->MainWorldScriptContext();

  std::unique_ptr<UserGestureIndicator> indicator =
      WTF::WrapUnique(new UserGestureIndicator(DocumentUserGestureToken::Create(
          document, UserGestureToken::kNewGesture)));
  ScriptExecutionCallbackHelper callback_helper(context);
  v8::Local<v8::Function> function =
      v8::Function::New(context, callback).ToLocalChecked();
  main_frame->RequestExecuteV8Function(
      main_frame->MainWorldScriptContext(), function,
      v8::Undefined(context->GetIsolate()), 0, nullptr, &callback_helper);

  RunPendingTasks();
  EXPECT_FALSE(callback_helper.DidComplete());

  main_frame->GetFrame()->GetDocument()->ResumeScheduledTasks();
  RunPendingTasks();
  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_EQ(true, callback_helper.BoolValue());
}

TEST_P(ParameterizedWebFrameTest, IframeScriptRemovesSelf) {
  RegisterMockedHttpURLLoad("single_iframe.html");
  RegisterMockedHttpURLLoad("visible_iframe.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "single_iframe.html", true);

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  ScriptExecutionCallbackHelper callback_helper(
      web_view_helper.WebView()->MainFrame()->MainWorldScriptContext());
  web_view_helper.WebView()
      ->MainFrame()
      ->FirstChild()
      ->ToWebLocalFrame()
      ->RequestExecuteScriptAndReturnValue(
          WebScriptSource(WebString(
              "var iframe = "
              "window.top.document.getElementsByTagName('iframe')[0]; "
              "window.top.document.body.removeChild(iframe); 'hello';")),
          false, &callback_helper);
  RunPendingTasks();
  EXPECT_TRUE(callback_helper.DidComplete());
  EXPECT_EQ(String(), callback_helper.StringValue());
}

TEST_P(ParameterizedWebFrameTest, FormWithNullFrame) {
  RegisterMockedHttpURLLoad("form.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "form.html");

  WebVector<WebFormElement> forms;
  web_view_helper.WebView()->MainFrame()->GetDocument().Forms(forms);
  web_view_helper.Reset();

  EXPECT_EQ(forms.size(), 1U);

  // This test passes if this doesn't crash.
  WebSearchableFormData searchable_data_form(forms[0]);
}

TEST_P(ParameterizedWebFrameTest, ChromePageJavascript) {
  RegisterMockedChromeURLLoad("history.html");

  // Pass true to enable JavaScript.
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(chrome_url_ + "history.html", true);

  // Try to run JS against the chrome-style URL.
  FrameTestHelpers::LoadFrame(web_view_helper.WebView()->MainFrame(),
                              "javascript:document.body.appendChild(document."
                              "createTextNode('Clobbered'))");

  // Now retrieve the frame's text and ensure it was modified by running
  // javascript.
  std::string content =
      WebFrameContentDumper::DumpWebViewAsText(web_view_helper.WebView(), 1024)
          .Utf8();
  EXPECT_NE(std::string::npos, content.find("Clobbered"));
}

TEST_P(ParameterizedWebFrameTest, ChromePageNoJavascript) {
  RegisterMockedChromeURLLoad("history.html");

  /// Pass true to enable JavaScript.
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(chrome_url_ + "history.html", true);

  // Try to run JS against the chrome-style URL after prohibiting it.
  WebSecurityPolicy::RegisterURLSchemeAsNotAllowingJavascriptURLs("chrome");
  FrameTestHelpers::LoadFrame(web_view_helper.WebView()->MainFrame(),
                              "javascript:document.body.appendChild(document."
                              "createTextNode('Clobbered'))");

  // Now retrieve the frame's text and ensure it wasn't modified by running
  // javascript.
  std::string content =
      WebFrameContentDumper::DumpWebViewAsText(web_view_helper.WebView(), 1024)
          .Utf8();
  EXPECT_EQ(std::string::npos, content.find("Clobbered"));
}

TEST_P(ParameterizedWebFrameTest, LocationSetHostWithMissingPort) {
  std::string file_name = "print-location-href.html";
  RegisterMockedHttpURLLoad(file_name);
  RegisterMockedURLLoadFromBase("http://internal.test:0/", file_name);

  FrameTestHelpers::WebViewHelper web_view_helper;

  /// Pass true to enable JavaScript.
  web_view_helper.InitializeAndLoad(base_url_ + file_name, true);

  // Setting host to "hostname:" should be treated as "hostname:0".
  FrameTestHelpers::LoadFrame(
      web_view_helper.WebView()->MainFrame(),
      "javascript:location.host = 'internal.test:'; void 0;");

  FrameTestHelpers::LoadFrame(
      web_view_helper.WebView()->MainFrame(),
      "javascript:document.body.textContent = location.href; void 0;");

  std::string content =
      WebFrameContentDumper::DumpWebViewAsText(web_view_helper.WebView(), 1024)
          .Utf8();
  EXPECT_EQ("http://internal.test:0/" + file_name, content);
}

TEST_P(ParameterizedWebFrameTest, LocationSetEmptyPort) {
  std::string file_name = "print-location-href.html";
  RegisterMockedHttpURLLoad(file_name);
  RegisterMockedURLLoadFromBase("http://internal.test:0/", file_name);

  FrameTestHelpers::WebViewHelper web_view_helper;

  /// Pass true to enable JavaScript.
  web_view_helper.InitializeAndLoad(base_url_ + file_name, true);

  FrameTestHelpers::LoadFrame(web_view_helper.WebView()->MainFrame(),
                              "javascript:location.port = ''; void 0;");

  FrameTestHelpers::LoadFrame(
      web_view_helper.WebView()->MainFrame(),
      "javascript:document.body.textContent = location.href; void 0;");

  std::string content =
      WebFrameContentDumper::DumpWebViewAsText(web_view_helper.WebView(), 1024)
          .Utf8();
  EXPECT_EQ("http://internal.test:0/" + file_name, content);
}

class EvaluateOnLoadWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  EvaluateOnLoadWebFrameClient() : executing_(false), was_executed_(false) {}

  void DidClearWindowObject() override {
    EXPECT_FALSE(executing_);
    was_executed_ = true;
    executing_ = true;
    v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
    Frame()->ExecuteScriptAndReturnValue(
        WebScriptSource(WebString("window.someProperty = 42;")));
    executing_ = false;
  }

  bool executing_;
  bool was_executed_;
};

TEST_P(ParameterizedWebFrameTest, DidClearWindowObjectIsNotRecursive) {
  EvaluateOnLoadWebFrameClient web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank", true, &web_frame_client);
  EXPECT_TRUE(web_frame_client.was_executed_);
}

class CSSCallbackWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
 public:
  CSSCallbackWebFrameClient() : update_count_(0) {}
  void DidMatchCSS(
      const WebVector<WebString>& newly_matching_selectors,
      const WebVector<WebString>& stopped_matching_selectors) override;

  std::map<WebLocalFrame*, std::set<std::string>> matched_selectors_;
  int update_count_;
};

void CSSCallbackWebFrameClient::DidMatchCSS(
    const WebVector<WebString>& newly_matching_selectors,
    const WebVector<WebString>& stopped_matching_selectors) {
  ++update_count_;
  std::set<std::string>& frame_selectors = matched_selectors_[Frame()];
  for (size_t i = 0; i < newly_matching_selectors.size(); ++i) {
    std::string selector = newly_matching_selectors[i].Utf8();
    EXPECT_EQ(0U, frame_selectors.count(selector)) << selector;
    frame_selectors.insert(selector);
  }
  for (size_t i = 0; i < stopped_matching_selectors.size(); ++i) {
    std::string selector = stopped_matching_selectors[i].Utf8();
    EXPECT_EQ(1U, frame_selectors.count(selector)) << selector;
    frame_selectors.erase(selector);
  }
}

class WebFrameCSSCallbackTest : public ::testing::Test {
 protected:
  WebFrameCSSCallbackTest() {
    frame_ = helper_.InitializeAndLoad("about:blank", true, &client_)
                 ->MainFrame()
                 ->ToWebLocalFrame();
    client_.SetFrame(frame_);
  }

  ~WebFrameCSSCallbackTest() {
    EXPECT_EQ(1U, client_.matched_selectors_.size());
  }

  WebDocument Doc() const { return frame_->GetDocument(); }

  int UpdateCount() const { return client_.update_count_; }

  const std::set<std::string>& MatchedSelectors() {
    return client_.matched_selectors_[frame_];
  }

  void LoadHTML(const std::string& html) {
    FrameTestHelpers::LoadHTMLString(frame_, html, ToKURL("about:blank"));
  }

  void ExecuteScript(const WebString& code) {
    frame_->ExecuteScript(WebScriptSource(code));
    frame_->View()->UpdateAllLifecyclePhases();
    RunPendingTasks();
  }

  CSSCallbackWebFrameClient client_;
  FrameTestHelpers::WebViewHelper helper_;
  WebLocalFrame* frame_;
};

TEST_F(WebFrameCSSCallbackTest, AuthorStyleSheet) {
  LoadHTML(
      "<style>"
      // This stylesheet checks that the internal property and value can't be
      // set by a stylesheet, only WebDocument::watchCSSSelectors().
      "div.initial_on { -internal-callback: none; }"
      "div.initial_off { -internal-callback: -internal-presence; }"
      "</style>"
      "<div class=\"initial_on\"></div>"
      "<div class=\"initial_off\"></div>");

  Vector<WebString> selectors;
  selectors.push_back(WebString::FromUTF8("div.initial_on"));
  frame_->GetDocument().WatchCSSSelectors(WebVector<WebString>(selectors));
  frame_->View()->UpdateAllLifecyclePhases();
  RunPendingTasks();
  EXPECT_EQ(1, UpdateCount());
  EXPECT_THAT(MatchedSelectors(), ElementsAre("div.initial_on"));

  // Check that adding a watched selector calls back for already-present nodes.
  selectors.push_back(WebString::FromUTF8("div.initial_off"));
  Doc().WatchCSSSelectors(WebVector<WebString>(selectors));
  frame_->View()->UpdateAllLifecyclePhases();
  RunPendingTasks();
  EXPECT_EQ(2, UpdateCount());
  EXPECT_THAT(MatchedSelectors(),
              ElementsAre("div.initial_off", "div.initial_on"));

  // Check that we can turn off callbacks for certain selectors.
  Doc().WatchCSSSelectors(WebVector<WebString>());
  frame_->View()->UpdateAllLifecyclePhases();
  RunPendingTasks();
  EXPECT_EQ(3, UpdateCount());
  EXPECT_THAT(MatchedSelectors(), ElementsAre());
}

TEST_F(WebFrameCSSCallbackTest, SharedComputedStyle) {
  // Check that adding an element calls back when it matches an existing rule.
  Vector<WebString> selectors;
  selectors.push_back(WebString::FromUTF8("span"));
  Doc().WatchCSSSelectors(WebVector<WebString>(selectors));

  ExecuteScript(
      "i1 = document.createElement('span');"
      "i1.id = 'first_span';"
      "document.body.appendChild(i1)");
  EXPECT_EQ(1, UpdateCount());
  EXPECT_THAT(MatchedSelectors(), ElementsAre("span"));

  // Adding a second element that shares a ComputedStyle shouldn't call back.
  // We use <span>s to avoid default style rules that can set
  // ComputedStyle::unique().
  ExecuteScript(
      "i2 = document.createElement('span');"
      "i2.id = 'second_span';"
      "i1 = document.getElementById('first_span');"
      "i1.parentNode.insertBefore(i2, i1.nextSibling);");
  EXPECT_EQ(1, UpdateCount());
  EXPECT_THAT(MatchedSelectors(), ElementsAre("span"));

  // Removing the first element shouldn't call back.
  ExecuteScript(
      "i1 = document.getElementById('first_span');"
      "i1.parentNode.removeChild(i1);");
  EXPECT_EQ(1, UpdateCount());
  EXPECT_THAT(MatchedSelectors(), ElementsAre("span"));

  // But removing the second element *should* call back.
  ExecuteScript(
      "i2 = document.getElementById('second_span');"
      "i2.parentNode.removeChild(i2);");
  EXPECT_EQ(2, UpdateCount());
  EXPECT_THAT(MatchedSelectors(), ElementsAre());
}

TEST_F(WebFrameCSSCallbackTest, CatchesAttributeChange) {
  LoadHTML("<span></span>");

  Vector<WebString> selectors;
  selectors.push_back(WebString::FromUTF8("span[attr=\"value\"]"));
  Doc().WatchCSSSelectors(WebVector<WebString>(selectors));
  RunPendingTasks();

  EXPECT_EQ(0, UpdateCount());
  EXPECT_THAT(MatchedSelectors(), ElementsAre());

  ExecuteScript(
      "document.querySelector('span').setAttribute('attr', 'value');");
  EXPECT_EQ(1, UpdateCount());
  EXPECT_THAT(MatchedSelectors(), ElementsAre("span[attr=\"value\"]"));
}

TEST_F(WebFrameCSSCallbackTest, DisplayNone) {
  LoadHTML("<div style='display:none'><span></span></div>");

  Vector<WebString> selectors;
  selectors.push_back(WebString::FromUTF8("span"));
  Doc().WatchCSSSelectors(WebVector<WebString>(selectors));
  RunPendingTasks();

  EXPECT_EQ(0, UpdateCount()) << "Don't match elements in display:none trees.";

  ExecuteScript(
      "d = document.querySelector('div');"
      "d.style.display = 'block';");
  EXPECT_EQ(1, UpdateCount()) << "Match elements when they become displayed.";
  EXPECT_THAT(MatchedSelectors(), ElementsAre("span"));

  ExecuteScript(
      "d = document.querySelector('div');"
      "d.style.display = 'none';");
  EXPECT_EQ(2, UpdateCount())
      << "Unmatch elements when they become undisplayed.";
  EXPECT_THAT(MatchedSelectors(), ElementsAre());

  ExecuteScript(
      "s = document.querySelector('span');"
      "s.style.display = 'none';");
  EXPECT_EQ(2, UpdateCount())
      << "No effect from no-display'ing a span that's already undisplayed.";

  ExecuteScript(
      "d = document.querySelector('div');"
      "d.style.display = 'block';");
  EXPECT_EQ(2, UpdateCount())
      << "No effect from displaying a div whose span is display:none.";

  ExecuteScript(
      "s = document.querySelector('span');"
      "s.style.display = 'inline';");
  EXPECT_EQ(3, UpdateCount())
      << "Now the span is visible and produces a callback.";
  EXPECT_THAT(MatchedSelectors(), ElementsAre("span"));

  ExecuteScript(
      "s = document.querySelector('span');"
      "s.style.display = 'none';");
  EXPECT_EQ(4, UpdateCount())
      << "Undisplaying the span directly should produce another callback.";
  EXPECT_THAT(MatchedSelectors(), ElementsAre());
}

TEST_F(WebFrameCSSCallbackTest, Reparenting) {
  LoadHTML(
      "<div id='d1'><span></span></div>"
      "<div id='d2'></div>");

  Vector<WebString> selectors;
  selectors.push_back(WebString::FromUTF8("span"));
  Doc().WatchCSSSelectors(WebVector<WebString>(selectors));
  frame_->View()->UpdateAllLifecyclePhases();
  RunPendingTasks();

  EXPECT_EQ(1, UpdateCount());
  EXPECT_THAT(MatchedSelectors(), ElementsAre("span"));

  ExecuteScript(
      "s = document.querySelector('span');"
      "d2 = document.getElementById('d2');"
      "d2.appendChild(s);");
  EXPECT_EQ(1, UpdateCount()) << "Just moving an element that continues to "
                                 "match shouldn't send a spurious callback.";
  EXPECT_THAT(MatchedSelectors(), ElementsAre("span"));
}

TEST_F(WebFrameCSSCallbackTest, MultiSelector) {
  LoadHTML("<span></span>");

  // Check that selector lists match as the whole list, not as each element
  // independently.
  Vector<WebString> selectors;
  selectors.push_back(WebString::FromUTF8("span"));
  selectors.push_back(WebString::FromUTF8("span,p"));
  Doc().WatchCSSSelectors(WebVector<WebString>(selectors));
  frame_->View()->UpdateAllLifecyclePhases();
  RunPendingTasks();

  EXPECT_EQ(1, UpdateCount());
  EXPECT_THAT(MatchedSelectors(), ElementsAre("span", "span, p"));
}

TEST_F(WebFrameCSSCallbackTest, InvalidSelector) {
  LoadHTML("<p><span></span></p>");

  // Build a list with one valid selector and one invalid.
  Vector<WebString> selectors;
  selectors.push_back(WebString::FromUTF8("span"));
  selectors.push_back(WebString::FromUTF8("["));       // Invalid.
  selectors.push_back(WebString::FromUTF8("p span"));  // Not compound.
  Doc().WatchCSSSelectors(WebVector<WebString>(selectors));
  frame_->View()->UpdateAllLifecyclePhases();
  RunPendingTasks();

  EXPECT_EQ(1, UpdateCount());
  EXPECT_THAT(MatchedSelectors(), ElementsAre("span"))
      << "An invalid selector shouldn't prevent other selectors from matching.";
}

TEST_P(ParameterizedWebFrameTest, DispatchMessageEventWithOriginCheck) {
  RegisterMockedHttpURLLoad("postmessage_test.html");

  // Pass true to enable JavaScript.
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "postmessage_test.html", true);

  // Send a message with the correct origin.
  WebSecurityOrigin correct_origin(
      WebSecurityOrigin::Create(ToKURL(base_url_)));
  WebDocument document = web_view_helper.WebView()->MainFrame()->GetDocument();
  WebSerializedScriptValue data(WebSerializedScriptValue::CreateInvalid());
  WebDOMMessageEvent message(data, "http://origin.com");
  web_view_helper.WebView()->MainFrame()->DispatchMessageEventWithOriginCheck(
      correct_origin, message);

  // Send another message with incorrect origin.
  WebSecurityOrigin incorrect_origin(
      WebSecurityOrigin::Create(ToKURL(chrome_url_)));
  web_view_helper.WebView()->MainFrame()->DispatchMessageEventWithOriginCheck(
      incorrect_origin, message);

  // Verify that only the first addition is in the body of the page.
  std::string content =
      WebFrameContentDumper::DumpWebViewAsText(web_view_helper.WebView(), 1024)
          .Utf8();
  EXPECT_NE(std::string::npos, content.find("Message 1."));
  EXPECT_EQ(std::string::npos, content.find("Message 2."));
}

namespace {

RefPtr<SerializedScriptValue> SerializeString(const StringView& message,
                                              ScriptState* script_state) {
  // This is inefficient, but avoids duplicating serialization logic for the
  // sake of this test.
  NonThrowableExceptionState exception_state;
  ScriptState::Scope scope(script_state);
  V8ScriptValueSerializer serializer(script_state);
  return serializer.Serialize(V8String(script_state->GetIsolate(), message),
                              exception_state);
}

}  // namespace

TEST_P(ParameterizedWebFrameTest, PostMessageThenDetach) {
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank");

  LocalFrame* frame =
      ToLocalFrame(web_view_helper.WebView()->GetPage()->MainFrame());
  NonThrowableExceptionState exception_state;
  RefPtr<SerializedScriptValue> message =
      SerializeString("message", ToScriptStateForMainWorld(frame));
  MessagePortArray message_ports;
  frame->DomWindow()->postMessage(message, message_ports, "*",
                                  frame->DomWindow(), exception_state);
  web_view_helper.Reset();
  EXPECT_FALSE(exception_state.HadException());

  // Success is not crashing.
  RunPendingTasks();
}

namespace {

class FixedLayoutTestWebViewClient
    : public FrameTestHelpers::TestWebViewClient {
 public:
  WebScreenInfo GetScreenInfo() override { return screen_info_; }

  WebScreenInfo screen_info_;
};

class FakeCompositingWebViewClient : public FixedLayoutTestWebViewClient {};

// Viewport settings need to be set before the page gets loaded
void EnableViewportSettings(WebSettings* settings) {
  settings->SetViewportMetaEnabled(true);
  settings->SetViewportEnabled(true);
  settings->SetMainFrameResizesAreOrientationChanges(true);
  settings->SetShrinksViewportContentToFit(true);
}

// Helper function to set autosizing multipliers on a document.
bool SetTextAutosizingMultiplier(Document* document, float multiplier) {
  bool multiplier_set = false;
  for (LayoutItem layout_item = document->GetLayoutViewItem();
       !layout_item.IsNull(); layout_item = layout_item.NextInPreOrder()) {
    if (layout_item.Style()) {
      layout_item.MutableStyleRef().SetTextAutosizingMultiplier(multiplier);

      EXPECT_EQ(multiplier, layout_item.Style()->TextAutosizingMultiplier());
      multiplier_set = true;
    }
  }
  return multiplier_set;
}

// Helper function to check autosizing multipliers on a document.
bool CheckTextAutosizingMultiplier(Document* document, float multiplier) {
  bool multiplier_checked = false;
  for (LayoutItem layout_item = document->GetLayoutViewItem();
       !layout_item.IsNull(); layout_item = layout_item.NextInPreOrder()) {
    if (layout_item.Style() && layout_item.IsText()) {
      EXPECT_EQ(multiplier, layout_item.Style()->TextAutosizingMultiplier());
      multiplier_checked = true;
    }
  }
  return multiplier_checked;
}

}  // anonymous namespace

TEST_P(ParameterizedWebFrameTest,
       ChangeInFixedLayoutResetsTextAutosizingMultipliers) {
  RegisterMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebViewClient client;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fixed_layout.html", true,
                                    nullptr, &client, nullptr,
                                    EnableViewportSettings);

  Document* document =
      ToLocalFrame(web_view_helper.WebView()->GetPage()->MainFrame())
          ->GetDocument();
  document->GetSettings()->SetTextAutosizingEnabled(true);
  EXPECT_TRUE(document->GetSettings()->TextAutosizingEnabled());
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_TRUE(SetTextAutosizingMultiplier(document, 2));

  ViewportDescription description = document->GetViewportDescription();
  // Choose a width that's not going match the viewport width of the loaded
  // document.
  description.min_width = Length(100, blink::kFixed);
  description.max_width = Length(100, blink::kFixed);
  web_view_helper.WebView()->UpdatePageDefinedViewportConstraints(description);

  EXPECT_TRUE(CheckTextAutosizingMultiplier(document, 1));
}

TEST_P(ParameterizedWebFrameTest,
       WorkingTextAutosizingMultipliers_VirtualViewport) {
  const std::string html_file = "fixed_layout.html";
  RegisterMockedHttpURLLoad(html_file);

  FixedLayoutTestWebViewClient client;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + html_file, true, nullptr,
                                    &client, nullptr, ConfigureAndroid);

  Document* document =
      ToLocalFrame(web_view_helper.WebView()->GetPage()->MainFrame())
          ->GetDocument();
  document->GetSettings()->SetTextAutosizingEnabled(true);
  EXPECT_TRUE(document->GetSettings()->TextAutosizingEnabled());

  web_view_helper.Resize(WebSize(490, 800));

  // Multiplier: 980 / 490 = 2.0
  EXPECT_TRUE(CheckTextAutosizingMultiplier(document, 2.0));
}

TEST_P(ParameterizedWebFrameTest,
       VisualViewportSetSizeInvalidatesTextAutosizingMultipliers) {
  RegisterMockedHttpURLLoad("iframe_reload.html");
  RegisterMockedHttpURLLoad("visible_iframe.html");

  FixedLayoutTestWebViewClient client;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "iframe_reload.html", true,
                                    nullptr, &client, nullptr,
                                    EnableViewportSettings);

  LocalFrame* main_frame =
      ToLocalFrame(web_view_helper.WebView()->GetPage()->MainFrame());
  Document* document = main_frame->GetDocument();
  FrameView* frame_view =
      web_view_helper.WebView()->MainFrameImpl()->GetFrameView();
  document->GetSettings()->SetTextAutosizingEnabled(true);
  EXPECT_TRUE(document->GetSettings()->TextAutosizingEnabled());
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  for (Frame* frame = main_frame; frame; frame = frame->Tree().TraverseNext()) {
    if (!frame->IsLocalFrame())
      continue;
    EXPECT_TRUE(
        SetTextAutosizingMultiplier(ToLocalFrame(frame)->GetDocument(), 2));
    for (LayoutItem layout_item =
             ToLocalFrame(frame)->GetDocument()->GetLayoutViewItem();
         !layout_item.IsNull(); layout_item = layout_item.NextInPreOrder()) {
      if (layout_item.IsText())
        EXPECT_FALSE(layout_item.NeedsLayout());
    }
  }

  frame_view->GetPage()->GetVisualViewport().SetSize(IntSize(200, 200));

  for (Frame* frame = main_frame; frame; frame = frame->Tree().TraverseNext()) {
    if (!frame->IsLocalFrame())
      continue;
    for (LayoutItem layout_item =
             ToLocalFrame(frame)->GetDocument()->GetLayoutViewItem();
         !layout_item.IsNull(); layout_item = layout_item.NextInPreOrder()) {
      if (layout_item.IsText())
        EXPECT_TRUE(layout_item.NeedsLayout());
    }
  }
}

TEST_P(ParameterizedWebFrameTest, ZeroHeightPositiveWidthNotIgnored) {
  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 1280;
  int viewport_height = 0;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true, nullptr, &client, nullptr,
                             EnableViewportSettings);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_EQ(viewport_width, web_view_helper.WebView()
                                ->MainFrameImpl()
                                ->GetFrameView()
                                ->GetLayoutSize()
                                .Width());
  EXPECT_EQ(viewport_height, web_view_helper.WebView()
                                 ->MainFrameImpl()
                                 ->GetFrameView()
                                 ->GetLayoutSize()
                                 .Height());
}

TEST_P(ParameterizedWebFrameTest,
       DeviceScaleFactorUsesDefaultWithoutViewportTag) {
  RegisterMockedHttpURLLoad("no_viewport_tag.html");

  int viewport_width = 640;
  int viewport_height = 480;

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 2;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "no_viewport_tag.html", true,
                                    nullptr, &client, nullptr,
                                    EnableViewportSettings);

  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_EQ(
      2, web_view_helper.WebView()->GetPage()->DeviceScaleFactorDeprecated());

  // Device scale factor should be independent of page scale.
  web_view_helper.WebView()->SetDefaultPageScaleLimits(1, 2);
  web_view_helper.WebView()->SetPageScaleFactor(0.5);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();
  EXPECT_EQ(1, web_view_helper.WebView()->PageScaleFactor());

  // Force the layout to happen before leaving the test.
  web_view_helper.WebView()->UpdateAllLifecyclePhases();
}

TEST_P(ParameterizedWebFrameTest, FixedLayoutInitializeAtMinimumScale) {
  RegisterMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  // Make sure we initialize to minimum scale, even if the window size
  // only becomes available after the load begins.
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true, nullptr, &client, nullptr,
                             EnableViewportSettings);
  web_view_helper.WebView()->SetDefaultPageScaleLimits(0.25f, 5);
  FrameTestHelpers::LoadFrame(web_view_helper.WebView()->MainFrame(),
                              base_url_ + "fixed_layout.html");
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  int default_fixed_layout_width = 980;
  float minimum_page_scale_factor =
      viewport_width / (float)default_fixed_layout_width;
  EXPECT_EQ(minimum_page_scale_factor,
            web_view_helper.WebView()->PageScaleFactor());
  EXPECT_EQ(minimum_page_scale_factor,
            web_view_helper.WebView()->MinimumPageScaleFactor());

  // Assume the user has pinch zoomed to page scale factor 2.
  float user_pinch_page_scale_factor = 2;
  web_view_helper.WebView()->SetPageScaleFactor(user_pinch_page_scale_factor);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();

  // Make sure we don't reset to initial scale if the page continues to load.
  web_view_helper.WebView()->DidCommitLoad(false, false);
  web_view_helper.WebView()->DidChangeContentsSize();
  EXPECT_EQ(user_pinch_page_scale_factor,
            web_view_helper.WebView()->PageScaleFactor());

  // Make sure we don't reset to initial scale if the viewport size changes.
  web_view_helper.Resize(WebSize(viewport_width, viewport_height + 100));
  EXPECT_EQ(user_pinch_page_scale_factor,
            web_view_helper.WebView()->PageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, WideDocumentInitializeAtMinimumScale) {
  RegisterMockedHttpURLLoad("wide_document.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  // Make sure we initialize to minimum scale, even if the window size
  // only becomes available after the load begins.
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true, nullptr, &client, nullptr,
                             EnableViewportSettings);
  web_view_helper.WebView()->SetDefaultPageScaleLimits(0.25f, 5);
  FrameTestHelpers::LoadFrame(web_view_helper.WebView()->MainFrame(),
                              base_url_ + "wide_document.html");
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  int wide_document_width = 1500;
  float minimum_page_scale_factor = viewport_width / (float)wide_document_width;
  EXPECT_EQ(minimum_page_scale_factor,
            web_view_helper.WebView()->PageScaleFactor());
  EXPECT_EQ(minimum_page_scale_factor,
            web_view_helper.WebView()->MinimumPageScaleFactor());

  // Assume the user has pinch zoomed to page scale factor 2.
  float user_pinch_page_scale_factor = 2;
  web_view_helper.WebView()->SetPageScaleFactor(user_pinch_page_scale_factor);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();

  // Make sure we don't reset to initial scale if the page continues to load.
  web_view_helper.WebView()->DidCommitLoad(false, false);
  web_view_helper.WebView()->DidChangeContentsSize();
  EXPECT_EQ(user_pinch_page_scale_factor,
            web_view_helper.WebView()->PageScaleFactor());

  // Make sure we don't reset to initial scale if the viewport size changes.
  web_view_helper.Resize(WebSize(viewport_width, viewport_height + 100));
  EXPECT_EQ(user_pinch_page_scale_factor,
            web_view_helper.WebView()->PageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, DelayedViewportInitialScale) {
  RegisterMockedHttpURLLoad("viewport-auto-initial-scale.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-auto-initial-scale.html", true, nullptr, &client,
      nullptr, EnableViewportSettings);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_EQ(0.25f, web_view_helper.WebView()->PageScaleFactor());

  Document* document =
      ToLocalFrame(web_view_helper.WebView()->GetPage()->MainFrame())
          ->GetDocument();
  ViewportDescription description = document->GetViewportDescription();
  description.zoom = 2;
  document->SetViewportDescription(description);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();
  EXPECT_EQ(2, web_view_helper.WebView()->PageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, setLoadWithOverviewModeToFalse) {
  RegisterMockedHttpURLLoad("viewport-auto-initial-scale.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-auto-initial-scale.html", true, nullptr, &client,
      nullptr, EnableViewportSettings);
  web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
  web_view_helper.WebView()->GetSettings()->SetLoadWithOverviewMode(false);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  // The page must be displayed at 100% zoom.
  EXPECT_EQ(1.0f, web_view_helper.WebView()->PageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest,
       SetLoadWithOverviewModeToFalseAndNoWideViewport) {
  RegisterMockedHttpURLLoad("large-div.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "large-div.html", true, nullptr,
                                    &client, nullptr, EnableViewportSettings);
  web_view_helper.WebView()->GetSettings()->SetLoadWithOverviewMode(false);
  web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
  web_view_helper.WebView()->GetSettings()->SetUseWideViewport(false);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  // The page must be displayed at 100% zoom, despite that it hosts a wide div
  // element.
  EXPECT_EQ(1.0f, web_view_helper.WebView()->PageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, NoWideViewportIgnoresPageViewportWidth) {
  RegisterMockedHttpURLLoad("viewport-auto-initial-scale.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-auto-initial-scale.html", true, nullptr, &client,
      nullptr, EnableViewportSettings);
  web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
  web_view_helper.WebView()->GetSettings()->SetUseWideViewport(false);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  // The page sets viewport width to 3000, but with UseWideViewport == false is
  // must be ignored.
  EXPECT_EQ(viewport_width, web_view_helper.WebView()
                                ->MainFrameImpl()
                                ->GetFrameView()
                                ->ContentsSize()
                                .Width());
  EXPECT_EQ(viewport_height, web_view_helper.WebView()
                                 ->MainFrameImpl()
                                 ->GetFrameView()
                                 ->ContentsSize()
                                 .Height());
}

TEST_P(ParameterizedWebFrameTest,
       NoWideViewportIgnoresPageViewportWidthButAccountsScale) {
  RegisterMockedHttpURLLoad("viewport-wide-2x-initial-scale.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-wide-2x-initial-scale.html", true, nullptr, &client,
      nullptr, EnableViewportSettings);
  web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
  web_view_helper.WebView()->GetSettings()->SetUseWideViewport(false);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  // The page sets viewport width to 3000, but with UseWideViewport == false it
  // must be ignored while the initial scale specified by the page must be
  // accounted.
  EXPECT_EQ(viewport_width / 2, web_view_helper.WebView()
                                    ->MainFrameImpl()
                                    ->GetFrameView()
                                    ->ContentsSize()
                                    .Width());
  EXPECT_EQ(viewport_height / 2, web_view_helper.WebView()
                                     ->MainFrameImpl()
                                     ->GetFrameView()
                                     ->ContentsSize()
                                     .Height());
}

TEST_P(ParameterizedWebFrameTest, WideViewportSetsTo980WithoutViewportTag) {
  RegisterMockedHttpURLLoad("no_viewport_tag.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "no_viewport_tag.html", true,
                                    nullptr, &client, nullptr,
                                    EnableViewportSettings);
  ApplyViewportStyleOverride(&web_view_helper);
  web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
  web_view_helper.WebView()->GetSettings()->SetUseWideViewport(true);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_EQ(980, web_view_helper.WebView()
                     ->MainFrameImpl()
                     ->GetFrameView()
                     ->ContentsSize()
                     .Width());
  EXPECT_EQ(980.0 / viewport_width * viewport_height, web_view_helper.WebView()
                                                          ->MainFrameImpl()
                                                          ->GetFrameView()
                                                          ->ContentsSize()
                                                          .Height());
}

TEST_P(ParameterizedWebFrameTest, WideViewportSetsTo980WithXhtmlMp) {
  RegisterMockedHttpURLLoad("viewport/viewport-legacy-xhtmlmp.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true, nullptr, &client, nullptr,
                             EnableViewportSettings);
  ApplyViewportStyleOverride(&web_view_helper);
  web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
  web_view_helper.WebView()->GetSettings()->SetUseWideViewport(true);
  FrameTestHelpers::LoadFrame(
      web_view_helper.WebView()->MainFrame(),
      base_url_ + "viewport/viewport-legacy-xhtmlmp.html");

  web_view_helper.Resize(WebSize(viewport_width, viewport_height));
  EXPECT_EQ(viewport_width, web_view_helper.WebView()
                                ->MainFrameImpl()
                                ->GetFrameView()
                                ->ContentsSize()
                                .Width());
  EXPECT_EQ(viewport_height, web_view_helper.WebView()
                                 ->MainFrameImpl()
                                 ->GetFrameView()
                                 ->ContentsSize()
                                 .Height());
}

TEST_P(ParameterizedWebFrameTest, NoWideViewportAndHeightInMeta) {
  RegisterMockedHttpURLLoad("viewport-height-1000.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport-height-1000.html",
                                    true, nullptr, &client, nullptr,
                                    EnableViewportSettings);
  web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
  web_view_helper.WebView()->GetSettings()->SetUseWideViewport(false);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_EQ(viewport_width, web_view_helper.WebView()
                                ->MainFrameImpl()
                                ->GetFrameView()
                                ->ContentsSize()
                                .Width());
}

TEST_P(ParameterizedWebFrameTest, WideViewportSetsTo980WithAutoWidth) {
  RegisterMockedHttpURLLoad("viewport-2x-initial-scale.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-2x-initial-scale.html", true, nullptr, &client,
      nullptr, EnableViewportSettings);
  ApplyViewportStyleOverride(&web_view_helper);
  web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
  web_view_helper.WebView()->GetSettings()->SetUseWideViewport(true);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_EQ(980, web_view_helper.WebView()
                     ->MainFrameImpl()
                     ->GetFrameView()
                     ->ContentsSize()
                     .Width());
  EXPECT_EQ(980.0 / viewport_width * viewport_height, web_view_helper.WebView()
                                                          ->MainFrameImpl()
                                                          ->GetFrameView()
                                                          ->ContentsSize()
                                                          .Height());
}

TEST_P(ParameterizedWebFrameTest,
       PageViewportInitialScaleOverridesLoadWithOverviewMode) {
  RegisterMockedHttpURLLoad("viewport-wide-2x-initial-scale.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-wide-2x-initial-scale.html", true, nullptr, &client,
      nullptr, EnableViewportSettings);
  web_view_helper.WebView()->GetSettings()->SetLoadWithOverviewMode(false);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  // The page must be displayed at 200% zoom, as specified in its viewport meta
  // tag.
  EXPECT_EQ(2.0f, web_view_helper.WebView()->PageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, setInitialPageScaleFactorPermanently) {
  RegisterMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  float enforced_page_scale_factor = 2.0f;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fixed_layout.html", true,
                                    nullptr, &client, nullptr,
                                    EnableViewportSettings);
  ApplyViewportStyleOverride(&web_view_helper);
  web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
  web_view_helper.WebView()->GetSettings()->SetLoadWithOverviewMode(false);
  web_view_helper.WebView()->SetInitialPageScaleOverride(
      enforced_page_scale_factor);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();

  EXPECT_EQ(enforced_page_scale_factor,
            web_view_helper.WebView()->PageScaleFactor());

  int viewport_width = 640;
  int viewport_height = 480;
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_EQ(enforced_page_scale_factor,
            web_view_helper.WebView()->PageScaleFactor());

  web_view_helper.WebView()->SetInitialPageScaleOverride(-1);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();
  EXPECT_EQ(1.0, web_view_helper.WebView()->PageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest,
       PermanentInitialPageScaleFactorOverridesLoadWithOverviewMode) {
  RegisterMockedHttpURLLoad("viewport-auto-initial-scale.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;
  float enforced_page_scale_factor = 0.5f;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-auto-initial-scale.html", true, nullptr, &client,
      nullptr, EnableViewportSettings);
  web_view_helper.WebView()->GetSettings()->SetLoadWithOverviewMode(false);
  web_view_helper.WebView()->SetInitialPageScaleOverride(
      enforced_page_scale_factor);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_EQ(enforced_page_scale_factor,
            web_view_helper.WebView()->PageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest,
       PermanentInitialPageScaleFactorOverridesPageViewportInitialScale) {
  RegisterMockedHttpURLLoad("viewport-wide-2x-initial-scale.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;
  float enforced_page_scale_factor = 0.5f;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-wide-2x-initial-scale.html", true, nullptr, &client,
      nullptr, EnableViewportSettings);
  web_view_helper.WebView()->SetInitialPageScaleOverride(
      enforced_page_scale_factor);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_EQ(enforced_page_scale_factor,
            web_view_helper.WebView()->PageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest,
       SmallPermanentInitialPageScaleFactorIsClobbered) {
  const char* pages[] = {
      // These pages trigger the clobbering condition. There must be a matching
      // item in "pageScaleFactors" array.
      "viewport-device-0.5x-initial-scale.html",
      "viewport-initial-scale-1.html",
      // These ones do not.
      "viewport-auto-initial-scale.html",
      "viewport-target-densitydpi-device-and-fixed-width.html"};
  float page_scale_factors[] = {0.5f, 1.0f};
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(pages); ++i)
    RegisterMockedHttpURLLoad(pages[i]);

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 400;
  int viewport_height = 300;
  float enforced_page_scale_factor = 0.75f;

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(pages); ++i) {
    for (int quirk_enabled = 0; quirk_enabled <= 1; ++quirk_enabled) {
      FrameTestHelpers::WebViewHelper web_view_helper;
      web_view_helper.InitializeAndLoad(base_url_ + pages[i], true, nullptr,
                                        &client, nullptr,
                                        EnableViewportSettings);
      ApplyViewportStyleOverride(&web_view_helper);
      web_view_helper.WebView()
          ->GetSettings()
          ->SetClobberUserAgentInitialScaleQuirk(quirk_enabled);
      web_view_helper.WebView()->SetInitialPageScaleOverride(
          enforced_page_scale_factor);
      web_view_helper.Resize(WebSize(viewport_width, viewport_height));

      float expected_page_scale_factor =
          quirk_enabled && i < WTF_ARRAY_LENGTH(page_scale_factors)
              ? page_scale_factors[i]
              : enforced_page_scale_factor;
      EXPECT_EQ(expected_page_scale_factor,
                web_view_helper.WebView()->PageScaleFactor());
    }
  }
}

TEST_P(ParameterizedWebFrameTest,
       PermanentInitialPageScaleFactorAffectsLayoutWidth) {
  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;
  float enforced_page_scale_factor = 0.5;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank", true, nullptr, &client,
                                    nullptr, EnableViewportSettings);
  web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
  web_view_helper.WebView()->GetSettings()->SetUseWideViewport(false);
  web_view_helper.WebView()->GetSettings()->SetLoadWithOverviewMode(false);
  web_view_helper.WebView()->SetInitialPageScaleOverride(
      enforced_page_scale_factor);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_EQ(viewport_width / enforced_page_scale_factor,
            web_view_helper.WebView()
                ->MainFrameImpl()
                ->GetFrameView()
                ->ContentsSize()
                .Width());
  EXPECT_EQ(enforced_page_scale_factor,
            web_view_helper.WebView()->PageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest,
       DocumentElementClientHeightWorksWithWrapContentMode) {
  RegisterMockedHttpURLLoad("0-by-0.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "0-by-0.html", true, nullptr,
                                    &client, nullptr, ConfigureAndroid);
  web_view_helper.WebView()->GetSettings()->SetForceZeroLayoutHeight(true);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  LocalFrame* frame = web_view_helper.WebView()->MainFrameImpl()->GetFrame();
  Document* document = frame->GetDocument();
  EXPECT_EQ(viewport_height, document->documentElement()->clientHeight());
  EXPECT_EQ(viewport_width, document->documentElement()->clientWidth());
}

TEST_P(ParameterizedWebFrameTest,
       SetForceZeroLayoutHeightWorksWithWrapContentMode) {
  RegisterMockedHttpURLLoad("0-by-0.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "0-by-0.html", true, nullptr,
                                    &client, nullptr, ConfigureAndroid);
  web_view_helper.WebView()->GetSettings()->SetForceZeroLayoutHeight(true);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();

  PaintLayerCompositor* compositor = web_view_helper.WebView()->Compositor();
  GraphicsLayer* scroll_container = compositor->ContainerLayer();
  if (!scroll_container)
    scroll_container = compositor->RootGraphicsLayer();

  EXPECT_EQ(0, web_view_helper.WebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->GetLayoutSize()
                   .Width());
  EXPECT_EQ(0, web_view_helper.WebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->GetLayoutSize()
                   .Height());
  EXPECT_EQ(0.0, scroll_container->Size().Width());
  EXPECT_EQ(0.0, scroll_container->Size().Height());

  web_view_helper.Resize(WebSize(viewport_width, 0));
  EXPECT_EQ(viewport_width, web_view_helper.WebView()
                                ->MainFrameImpl()
                                ->GetFrameView()
                                ->GetLayoutSize()
                                .Width());
  EXPECT_EQ(0, web_view_helper.WebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->GetLayoutSize()
                   .Height());
  EXPECT_EQ(viewport_width, scroll_container->Size().Width());
  EXPECT_EQ(0.0, scroll_container->Size().Height());

  // The flag ForceZeroLayoutHeight will cause the following resize of viewport
  // height to be ignored by the outer viewport (the container layer of
  // LayerCompositor). The height of the visualViewport, however, is not
  // affected.
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));
  EXPECT_FALSE(web_view_helper.WebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->NeedsLayout());
  EXPECT_EQ(viewport_width, web_view_helper.WebView()
                                ->MainFrameImpl()
                                ->GetFrameView()
                                ->GetLayoutSize()
                                .Width());
  EXPECT_EQ(0, web_view_helper.WebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->GetLayoutSize()
                   .Height());
  EXPECT_EQ(viewport_width, scroll_container->Size().Width());
  EXPECT_EQ(viewport_height, scroll_container->Size().Height());

  LocalFrame* frame = web_view_helper.WebView()->MainFrameImpl()->GetFrame();
  VisualViewport& visual_viewport = frame->GetPage()->GetVisualViewport();
  EXPECT_EQ(viewport_height, visual_viewport.ContainerLayer()->Size().Height());
  EXPECT_TRUE(
      visual_viewport.ContainerLayer()->PlatformLayer()->MasksToBounds());
  EXPECT_FALSE(scroll_container->PlatformLayer()->MasksToBounds());
}

TEST_P(ParameterizedWebFrameTest, SetForceZeroLayoutHeight) {
  RegisterMockedHttpURLLoad("200-by-300.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "200-by-300.html", true,
                                    nullptr, &client, nullptr,
                                    EnableViewportSettings);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_LE(viewport_height, web_view_helper.WebView()
                                 ->MainFrameImpl()
                                 ->GetFrameView()
                                 ->GetLayoutSize()
                                 .Height());
  web_view_helper.WebView()->GetSettings()->SetForceZeroLayoutHeight(true);
  EXPECT_TRUE(web_view_helper.WebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->NeedsLayout());

  EXPECT_EQ(0, web_view_helper.WebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->GetLayoutSize()
                   .Height());

  web_view_helper.Resize(WebSize(viewport_width, viewport_height * 2));
  EXPECT_FALSE(web_view_helper.WebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->NeedsLayout());
  EXPECT_EQ(0, web_view_helper.WebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->GetLayoutSize()
                   .Height());

  web_view_helper.Resize(WebSize(viewport_width * 2, viewport_height));
  EXPECT_EQ(0, web_view_helper.WebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->GetLayoutSize()
                   .Height());

  web_view_helper.WebView()->GetSettings()->SetForceZeroLayoutHeight(false);
  EXPECT_LE(viewport_height, web_view_helper.WebView()
                                 ->MainFrameImpl()
                                 ->GetFrameView()
                                 ->GetLayoutSize()
                                 .Height());
}

TEST_F(WebFrameTest, ToggleViewportMetaOnOff) {
  RegisterMockedHttpURLLoad("viewport-device-width.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport-device-width.html",
                                    true, 0, &client);
  WebSettings* settings = web_view_helper.WebView()->GetSettings();
  settings->SetViewportMetaEnabled(false);
  settings->SetViewportEnabled(true);
  settings->SetMainFrameResizesAreOrientationChanges(true);
  settings->SetShrinksViewportContentToFit(true);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  Document* document =
      ToLocalFrame(web_view_helper.WebView()->GetPage()->MainFrame())
          ->GetDocument();
  EXPECT_FALSE(document->GetViewportDescription().IsLegacyViewportType());

  settings->SetViewportMetaEnabled(true);
  EXPECT_TRUE(document->GetViewportDescription().IsLegacyViewportType());

  settings->SetViewportMetaEnabled(false);
  EXPECT_FALSE(document->GetViewportDescription().IsLegacyViewportType());
}

TEST_F(WebFrameTest,
       SetForceZeroLayoutHeightWorksWithRelayoutsWhenHeightChanged) {
  // this unit test is an attempt to target a real world case where an app could
  // 1. call resize(width, 0) and setForceZeroLayoutHeight(true)
  // 2. load content (hoping that the viewport height would increase
  // as more content is added)
  // 3. fail to register touch events aimed at the loaded content
  // because the layout is only updated if either width or height is changed
  RegisterMockedHttpURLLoad("button.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "button.html", true, nullptr,
                                    &client, nullptr, ConfigureAndroid);
  // set view height to zero so that if the height of the view is not
  // successfully updated during later resizes touch events will fail
  // (as in not hit content included in the view)
  web_view_helper.Resize(WebSize(viewport_width, 0));

  web_view_helper.WebView()->GetSettings()->SetForceZeroLayoutHeight(true);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  IntPoint hit_point = IntPoint(30, 30);  // button size is 100x100

  WebLocalFrameImpl* frame = web_view_helper.WebView()->MainFrameImpl();
  Document* document = frame->GetFrame()->GetDocument();
  Element* element = document->GetElementById("tap_button");

  ASSERT_NE(nullptr, element);
  EXPECT_EQ(String("oldValue"), element->innerText());

  WebGestureEvent gesture_event(WebInputEvent::kGestureTap,
                                WebInputEvent::kNoModifiers,
                                WebInputEvent::kTimeStampForTesting);
  gesture_event.SetFrameScale(1);
  gesture_event.x = gesture_event.global_x = hit_point.X();
  gesture_event.y = gesture_event.global_y = hit_point.Y();
  gesture_event.source_device = kWebGestureDeviceTouchscreen;
  web_view_helper.WebView()
      ->MainFrameImpl()
      ->GetFrame()
      ->GetEventHandler()
      .HandleGestureEvent(gesture_event);
  // when pressed, the button changes its own text to "updatedValue"
  EXPECT_EQ(String("updatedValue"), element->innerText());
}

TEST_F(WebFrameTest, FrameOwnerPropertiesMargin) {
  FrameTestHelpers::TestWebViewClient view_client;
  FrameTestHelpers::TestWebRemoteFrameClient remote_client;
  WebView* view = WebView::Create(&view_client, kWebPageVisibilityStateVisible);
  view->GetSettings()->SetJavaScriptEnabled(true);
  view->SetMainFrame(remote_client.GetFrame());
  WebRemoteFrame* root = view->MainFrame()->ToWebRemoteFrame();
  root->SetReplicatedOrigin(SecurityOrigin::CreateUnique());

  WebFrameOwnerProperties properties;
  properties.margin_width = 11;
  properties.margin_height = 22;
  WebLocalFrameImpl* local_frame = FrameTestHelpers::CreateLocalChild(
      root, "frameName", nullptr, nullptr, nullptr, properties);

  RegisterMockedHttpURLLoad("frame_owner_properties.html");
  FrameTestHelpers::LoadFrame(local_frame,
                              base_url_ + "frame_owner_properties.html");

  // Check if the LocalFrame has seen the marginwidth and marginheight
  // properties.
  Document* child_document = local_frame->GetFrame()->GetDocument();
  EXPECT_EQ(11, child_document->FirstBodyElement()->GetIntegralAttribute(
                    HTMLNames::marginwidthAttr));
  EXPECT_EQ(22, child_document->FirstBodyElement()->GetIntegralAttribute(
                    HTMLNames::marginheightAttr));

  FrameView* frame_view = local_frame->GetFrameView();
  // Expect scrollbars to be enabled by default.
  EXPECT_NE(nullptr, frame_view->HorizontalScrollbar());
  EXPECT_NE(nullptr, frame_view->VerticalScrollbar());

  view->Close();
}

TEST_F(WebFrameTest, FrameOwnerPropertiesScrolling) {
  FrameTestHelpers::TestWebViewClient view_client;
  FrameTestHelpers::TestWebRemoteFrameClient remote_client;
  WebView* view = WebView::Create(&view_client, kWebPageVisibilityStateVisible);
  view->GetSettings()->SetJavaScriptEnabled(true);
  view->SetMainFrame(remote_client.GetFrame());
  WebRemoteFrame* root = view->MainFrame()->ToWebRemoteFrame();
  root->SetReplicatedOrigin(SecurityOrigin::CreateUnique());

  WebFrameOwnerProperties properties;
  // Turn off scrolling in the subframe.
  properties.scrolling_mode =
      WebFrameOwnerProperties::ScrollingMode::kAlwaysOff;
  WebLocalFrameImpl* local_frame = FrameTestHelpers::CreateLocalChild(
      root, "frameName", nullptr, nullptr, nullptr, properties);

  RegisterMockedHttpURLLoad("frame_owner_properties.html");
  FrameTestHelpers::LoadFrame(local_frame,
                              base_url_ + "frame_owner_properties.html");

  Document* child_document = local_frame->GetFrame()->GetDocument();
  EXPECT_EQ(0, child_document->FirstBodyElement()->GetIntegralAttribute(
                   HTMLNames::marginwidthAttr));
  EXPECT_EQ(0, child_document->FirstBodyElement()->GetIntegralAttribute(
                   HTMLNames::marginheightAttr));

  FrameView* frame_view =
      static_cast<WebLocalFrameImpl*>(local_frame)->GetFrameView();
  EXPECT_EQ(nullptr, frame_view->HorizontalScrollbar());
  EXPECT_EQ(nullptr, frame_view->VerticalScrollbar());

  view->Close();
}

TEST_P(ParameterizedWebFrameTest,
       SetForceZeroLayoutHeightWorksAcrossNavigations) {
  RegisterMockedHttpURLLoad("200-by-300.html");
  RegisterMockedHttpURLLoad("large-div.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "200-by-300.html", true,
                                    nullptr, &client, nullptr,
                                    EnableViewportSettings);
  web_view_helper.WebView()->GetSettings()->SetForceZeroLayoutHeight(true);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  FrameTestHelpers::LoadFrame(web_view_helper.WebView()->MainFrame(),
                              base_url_ + "large-div.html");
  web_view_helper.WebView()->UpdateAllLifecyclePhases();

  EXPECT_EQ(0, web_view_helper.WebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->GetLayoutSize()
                   .Height());
}

TEST_P(ParameterizedWebFrameTest,
       SetForceZeroLayoutHeightWithWideViewportQuirk) {
  RegisterMockedHttpURLLoad("200-by-300.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;

  web_view_helper.InitializeAndLoad(base_url_ + "200-by-300.html", true,
                                    nullptr, &client, nullptr,
                                    EnableViewportSettings);
  web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
  web_view_helper.WebView()->GetSettings()->SetUseWideViewport(true);
  web_view_helper.WebView()->GetSettings()->SetForceZeroLayoutHeight(true);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_EQ(0, web_view_helper.WebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->GetLayoutSize()
                   .Height());
}

TEST_P(ParameterizedWebFrameTest, WideViewportAndWideContentWithInitialScale) {
  RegisterMockedHttpURLLoad("wide_document_width_viewport.html");
  RegisterMockedHttpURLLoad("white-1x1.png");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 600;
  int viewport_height = 800;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank", true, nullptr, &client,
                                    nullptr, EnableViewportSettings);
  web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
  web_view_helper.WebView()->GetSettings()->SetUseWideViewport(true);
  web_view_helper.WebView()->GetSettings()->SetViewportMetaLayoutSizeQuirk(
      true);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  FrameTestHelpers::LoadFrame(web_view_helper.WebView()->MainFrame(),
                              base_url_ + "wide_document_width_viewport.html");
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  int wide_document_width = 800;
  float minimum_page_scale_factor = viewport_width / (float)wide_document_width;
  EXPECT_EQ(minimum_page_scale_factor,
            web_view_helper.WebView()->PageScaleFactor());
  EXPECT_EQ(minimum_page_scale_factor,
            web_view_helper.WebView()->MinimumPageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, WideViewportQuirkClobbersHeight) {
  RegisterMockedHttpURLLoad("viewport-height-1000.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 600;
  int viewport_height = 800;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank", true, nullptr, &client,
                                    nullptr, EnableViewportSettings);
  web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
  web_view_helper.WebView()->GetSettings()->SetUseWideViewport(false);
  web_view_helper.WebView()->GetSettings()->SetViewportMetaLayoutSizeQuirk(
      true);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  FrameTestHelpers::LoadFrame(web_view_helper.WebView()->MainFrame(),
                              base_url_ + "viewport-height-1000.html");
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_EQ(800, web_view_helper.WebView()
                     ->MainFrameImpl()
                     ->GetFrameView()
                     ->GetLayoutSize()
                     .Height());
  EXPECT_EQ(1, web_view_helper.WebView()->PageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, LayoutSize320Quirk) {
  RegisterMockedHttpURLLoad("viewport/viewport-30.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 600;
  int viewport_height = 800;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank", true, nullptr, &client,
                                    nullptr, EnableViewportSettings);
  web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
  web_view_helper.WebView()->GetSettings()->SetUseWideViewport(true);
  web_view_helper.WebView()->GetSettings()->SetViewportMetaLayoutSizeQuirk(
      true);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  FrameTestHelpers::LoadFrame(web_view_helper.WebView()->MainFrame(),
                              base_url_ + "viewport/viewport-30.html");
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_EQ(600, web_view_helper.WebView()
                     ->MainFrameImpl()
                     ->GetFrameView()
                     ->GetLayoutSize()
                     .Width());
  EXPECT_EQ(800, web_view_helper.WebView()
                     ->MainFrameImpl()
                     ->GetFrameView()
                     ->GetLayoutSize()
                     .Height());
  EXPECT_EQ(1, web_view_helper.WebView()->PageScaleFactor());

  // The magic number to snap to device-width is 320, so test that 321 is
  // respected.
  Document* document =
      ToLocalFrame(web_view_helper.WebView()->GetPage()->MainFrame())
          ->GetDocument();
  ViewportDescription description = document->GetViewportDescription();
  description.min_width = Length(321, blink::kFixed);
  description.max_width = Length(321, blink::kFixed);
  document->SetViewportDescription(description);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();
  EXPECT_EQ(321, web_view_helper.WebView()
                     ->MainFrameImpl()
                     ->GetFrameView()
                     ->GetLayoutSize()
                     .Width());

  description.min_width = Length(320, blink::kFixed);
  description.max_width = Length(320, blink::kFixed);
  document->SetViewportDescription(description);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();
  EXPECT_EQ(600, web_view_helper.WebView()
                     ->MainFrameImpl()
                     ->GetFrameView()
                     ->GetLayoutSize()
                     .Width());

  description = document->GetViewportDescription();
  description.max_height = Length(1000, blink::kFixed);
  document->SetViewportDescription(description);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();
  EXPECT_EQ(1000, web_view_helper.WebView()
                      ->MainFrameImpl()
                      ->GetFrameView()
                      ->GetLayoutSize()
                      .Height());

  description.max_height = Length(320, blink::kFixed);
  document->SetViewportDescription(description);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();
  EXPECT_EQ(800, web_view_helper.WebView()
                     ->MainFrameImpl()
                     ->GetFrameView()
                     ->GetLayoutSize()
                     .Height());
}

TEST_P(ParameterizedWebFrameTest, ZeroValuesQuirk) {
  RegisterMockedHttpURLLoad("viewport-zero-values.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true, nullptr, &client, nullptr,
                             EnableViewportSettings);
  web_view_helper.WebView()->GetSettings()->SetViewportMetaZeroValuesQuirk(
      true);
  web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
  web_view_helper.WebView()->GetSettings()->SetViewportMetaLayoutSizeQuirk(
      true);
  FrameTestHelpers::LoadFrame(web_view_helper.WebView()->MainFrame(),
                              base_url_ + "viewport-zero-values.html");
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_EQ(viewport_width, web_view_helper.WebView()
                                ->MainFrameImpl()
                                ->GetFrameView()
                                ->GetLayoutSize()
                                .Width());
  EXPECT_EQ(1.0f, web_view_helper.WebView()->PageScaleFactor());

  web_view_helper.WebView()->GetSettings()->SetUseWideViewport(true);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();
  EXPECT_EQ(viewport_width, web_view_helper.WebView()
                                ->MainFrameImpl()
                                ->GetFrameView()
                                ->GetLayoutSize()
                                .Width());
  EXPECT_EQ(1.0f, web_view_helper.WebView()->PageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, OverflowHiddenDisablesScrolling) {
  RegisterMockedHttpURLLoad("body-overflow-hidden.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true, nullptr, &client, nullptr);
  FrameTestHelpers::LoadFrame(web_view_helper.WebView()->MainFrame(),
                              base_url_ + "body-overflow-hidden.html");
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  FrameView* view = web_view_helper.WebView()->MainFrameImpl()->GetFrameView();
  EXPECT_FALSE(view->UserInputScrollable(kVerticalScrollbar));
  EXPECT_FALSE(view->UserInputScrollable(kHorizontalScrollbar));
}

TEST_P(ParameterizedWebFrameTest,
       OverflowHiddenDisablesScrollingWithSetCanHaveScrollbars) {
  RegisterMockedHttpURLLoad("body-overflow-hidden-short.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true, nullptr, &client, nullptr);
  FrameTestHelpers::LoadFrame(web_view_helper.WebView()->MainFrame(),
                              base_url_ + "body-overflow-hidden-short.html");
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  FrameView* view = web_view_helper.WebView()->MainFrameImpl()->GetFrameView();
  EXPECT_FALSE(view->UserInputScrollable(kVerticalScrollbar));
  EXPECT_FALSE(view->UserInputScrollable(kHorizontalScrollbar));

  web_view_helper.WebView()->MainFrameImpl()->SetCanHaveScrollbars(true);
  EXPECT_FALSE(view->UserInputScrollable(kVerticalScrollbar));
  EXPECT_FALSE(view->UserInputScrollable(kHorizontalScrollbar));
}

TEST_F(WebFrameTest, IgnoreOverflowHiddenQuirk) {
  RegisterMockedHttpURLLoad("body-overflow-hidden.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true, nullptr, &client, nullptr);
  web_view_helper.WebView()
      ->GetSettings()
      ->SetIgnoreMainFrameOverflowHiddenQuirk(true);
  FrameTestHelpers::LoadFrame(web_view_helper.WebView()->MainFrame(),
                              base_url_ + "body-overflow-hidden.html");
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  FrameView* view = web_view_helper.WebView()->MainFrameImpl()->GetFrameView();
  EXPECT_TRUE(view->UserInputScrollable(kVerticalScrollbar));
}

TEST_P(ParameterizedWebFrameTest, NonZeroValuesNoQuirk) {
  RegisterMockedHttpURLLoad("viewport-nonzero-values.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;
  float expected_page_scale_factor = 0.5f;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true, nullptr, &client, nullptr,
                             EnableViewportSettings);
  web_view_helper.WebView()->GetSettings()->SetViewportMetaZeroValuesQuirk(
      true);
  web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
  FrameTestHelpers::LoadFrame(web_view_helper.WebView()->MainFrame(),
                              base_url_ + "viewport-nonzero-values.html");
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_EQ(viewport_width / expected_page_scale_factor,
            web_view_helper.WebView()
                ->MainFrameImpl()
                ->GetFrameView()
                ->GetLayoutSize()
                .Width());
  EXPECT_EQ(expected_page_scale_factor,
            web_view_helper.WebView()->PageScaleFactor());

  web_view_helper.WebView()->GetSettings()->SetUseWideViewport(true);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();
  EXPECT_EQ(viewport_width / expected_page_scale_factor,
            web_view_helper.WebView()
                ->MainFrameImpl()
                ->GetFrameView()
                ->GetLayoutSize()
                .Width());
  EXPECT_EQ(expected_page_scale_factor,
            web_view_helper.WebView()->PageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, setPageScaleFactorDoesNotLayout) {
  RegisterMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  // Small viewport to ensure there are always scrollbars.
  int viewport_width = 64;
  int viewport_height = 48;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fixed_layout.html", true,
                                    nullptr, &client, nullptr,
                                    EnableViewportSettings);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  int prev_layout_count =
      web_view_helper.WebView()->MainFrameImpl()->GetFrameView()->LayoutCount();
  web_view_helper.WebView()->SetPageScaleFactor(3);
  EXPECT_FALSE(web_view_helper.WebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->NeedsLayout());
  EXPECT_EQ(prev_layout_count, web_view_helper.WebView()
                                   ->MainFrameImpl()
                                   ->GetFrameView()
                                   ->LayoutCount());
}

TEST_P(ParameterizedWebFrameTest,
       setPageScaleFactorWithOverlayScrollbarsDoesNotLayout) {
  RegisterMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fixed_layout.html", true,
                                    nullptr, &client, nullptr,
                                    EnableViewportSettings);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  int prev_layout_count =
      web_view_helper.WebView()->MainFrameImpl()->GetFrameView()->LayoutCount();
  web_view_helper.WebView()->SetPageScaleFactor(30);
  EXPECT_FALSE(web_view_helper.WebView()
                   ->MainFrameImpl()
                   ->GetFrameView()
                   ->NeedsLayout());
  EXPECT_EQ(prev_layout_count, web_view_helper.WebView()
                                   ->MainFrameImpl()
                                   ->GetFrameView()
                                   ->LayoutCount());
}

TEST_P(ParameterizedWebFrameTest, pageScaleFactorWrittenToHistoryItem) {
  RegisterMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fixed_layout.html", true,
                                    nullptr, &client, nullptr,
                                    EnableViewportSettings);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  web_view_helper.WebView()->SetPageScaleFactor(3);
  EXPECT_EQ(3, ToLocalFrame(web_view_helper.WebView()->GetPage()->MainFrame())
                   ->Loader()
                   .GetDocumentLoader()
                   ->GetHistoryItem()
                   ->PageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, initialScaleWrittenToHistoryItem) {
  RegisterMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true, nullptr, &client, nullptr,
                             EnableViewportSettings);
  web_view_helper.WebView()->SetDefaultPageScaleLimits(0.25f, 5);
  FrameTestHelpers::LoadFrame(web_view_helper.WebView()->MainFrame(),
                              base_url_ + "fixed_layout.html");
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  int default_fixed_layout_width = 980;
  float minimum_page_scale_factor =
      viewport_width / (float)default_fixed_layout_width;
  EXPECT_EQ(minimum_page_scale_factor,
            ToLocalFrame(web_view_helper.WebView()->GetPage()->MainFrame())
                ->Loader()
                .GetDocumentLoader()
                ->GetHistoryItem()
                ->PageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, pageScaleFactorDoesntShrinkFrameView) {
  RegisterMockedHttpURLLoad("large-div.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  // Small viewport to ensure there are always scrollbars.
  int viewport_width = 64;
  int viewport_height = 48;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "large-div.html", true, nullptr,
                                    &client, nullptr, EnableViewportSettings);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  FrameView* view = web_view_helper.WebView()->MainFrameImpl()->GetFrameView();
  int viewport_width_minus_scrollbar = viewport_width;
  int viewport_height_minus_scrollbar = viewport_height;

  if (view->VerticalScrollbar() &&
      !view->VerticalScrollbar()->IsOverlayScrollbar())
    viewport_width_minus_scrollbar -= 15;

  if (view->HorizontalScrollbar() &&
      !view->HorizontalScrollbar()->IsOverlayScrollbar())
    viewport_height_minus_scrollbar -= 15;

  web_view_helper.WebView()->SetPageScaleFactor(2);

  IntSize unscaled_size = view->VisibleContentSize(kIncludeScrollbars);
  EXPECT_EQ(viewport_width, unscaled_size.Width());
  EXPECT_EQ(viewport_height, unscaled_size.Height());

  IntSize unscaled_size_minus_scrollbar =
      view->VisibleContentSize(kExcludeScrollbars);
  EXPECT_EQ(viewport_width_minus_scrollbar,
            unscaled_size_minus_scrollbar.Width());
  EXPECT_EQ(viewport_height_minus_scrollbar,
            unscaled_size_minus_scrollbar.Height());

  IntSize frame_view_size = view->VisibleContentRect().Size();
  EXPECT_EQ(viewport_width_minus_scrollbar, frame_view_size.Width());
  EXPECT_EQ(viewport_height_minus_scrollbar, frame_view_size.Height());
}

TEST_P(ParameterizedWebFrameTest, pageScaleFactorDoesNotApplyCssTransform) {
  RegisterMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fixed_layout.html", true,
                                    nullptr, &client, nullptr,
                                    EnableViewportSettings);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  web_view_helper.WebView()->SetPageScaleFactor(2);

  EXPECT_EQ(980, ToLocalFrame(web_view_helper.WebView()->GetPage()->MainFrame())
                     ->ContentLayoutItem()
                     .DocumentRect()
                     .Width());
  EXPECT_EQ(980, web_view_helper.WebView()
                     ->MainFrameImpl()
                     ->GetFrameView()
                     ->ContentsSize()
                     .Width());
}

TEST_P(ParameterizedWebFrameTest, targetDensityDpiHigh) {
  RegisterMockedHttpURLLoad("viewport-target-densitydpi-high.html");

  FixedLayoutTestWebViewClient client;
  // high-dpi = 240
  float target_dpi = 240.0f;
  float device_scale_factors[] = {1.0f, 4.0f / 3.0f, 2.0f};
  int viewport_width = 640;
  int viewport_height = 480;

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(device_scale_factors); ++i) {
    float device_scale_factor = device_scale_factors[i];
    float device_dpi = device_scale_factor * 160.0f;
    client.screen_info_.device_scale_factor = device_scale_factor;

    FrameTestHelpers::WebViewHelper web_view_helper;
    web_view_helper.InitializeAndLoad(
        base_url_ + "viewport-target-densitydpi-high.html", true, nullptr,
        &client, nullptr, EnableViewportSettings);
    web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
    web_view_helper.WebView()
        ->GetSettings()
        ->SetSupportDeprecatedTargetDensityDPI(true);
    web_view_helper.Resize(WebSize(viewport_width, viewport_height));

    // We need to account for the fact that logical pixels are unconditionally
    // multiplied by deviceScaleFactor to produce physical pixels.
    float density_dpi_scale_ratio =
        device_scale_factor * target_dpi / device_dpi;
    EXPECT_NEAR(viewport_width * density_dpi_scale_ratio,
                web_view_helper.WebView()
                    ->MainFrameImpl()
                    ->GetFrameView()
                    ->GetLayoutSize()
                    .Width(),
                1.0f);
    EXPECT_NEAR(viewport_height * density_dpi_scale_ratio,
                web_view_helper.WebView()
                    ->MainFrameImpl()
                    ->GetFrameView()
                    ->GetLayoutSize()
                    .Height(),
                1.0f);
    EXPECT_NEAR(1.0f / density_dpi_scale_ratio,
                web_view_helper.WebView()->PageScaleFactor(), 0.01f);
  }
}

TEST_P(ParameterizedWebFrameTest, targetDensityDpiDevice) {
  RegisterMockedHttpURLLoad("viewport-target-densitydpi-device.html");

  float device_scale_factors[] = {1.0f, 4.0f / 3.0f, 2.0f};

  FixedLayoutTestWebViewClient client;
  int viewport_width = 640;
  int viewport_height = 480;

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(device_scale_factors); ++i) {
    client.screen_info_.device_scale_factor = device_scale_factors[i];

    FrameTestHelpers::WebViewHelper web_view_helper;
    web_view_helper.InitializeAndLoad(
        base_url_ + "viewport-target-densitydpi-device.html", true, nullptr,
        &client, nullptr, EnableViewportSettings);
    web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
    web_view_helper.WebView()
        ->GetSettings()
        ->SetSupportDeprecatedTargetDensityDPI(true);
    web_view_helper.Resize(WebSize(viewport_width, viewport_height));

    EXPECT_NEAR(viewport_width * client.screen_info_.device_scale_factor,
                web_view_helper.WebView()
                    ->MainFrameImpl()
                    ->GetFrameView()
                    ->GetLayoutSize()
                    .Width(),
                1.0f);
    EXPECT_NEAR(viewport_height * client.screen_info_.device_scale_factor,
                web_view_helper.WebView()
                    ->MainFrameImpl()
                    ->GetFrameView()
                    ->GetLayoutSize()
                    .Height(),
                1.0f);
    EXPECT_NEAR(1.0f / client.screen_info_.device_scale_factor,
                web_view_helper.WebView()->PageScaleFactor(), 0.01f);
  }
}

TEST_P(ParameterizedWebFrameTest, targetDensityDpiDeviceAndFixedWidth) {
  RegisterMockedHttpURLLoad(
      "viewport-target-densitydpi-device-and-fixed-width.html");

  float device_scale_factors[] = {1.0f, 4.0f / 3.0f, 2.0f};

  FixedLayoutTestWebViewClient client;
  int viewport_width = 640;
  int viewport_height = 480;

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(device_scale_factors); ++i) {
    client.screen_info_.device_scale_factor = device_scale_factors[i];

    FrameTestHelpers::WebViewHelper web_view_helper;
    web_view_helper.InitializeAndLoad(
        base_url_ + "viewport-target-densitydpi-device-and-fixed-width.html",
        true, nullptr, &client, nullptr, EnableViewportSettings);
    web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
    web_view_helper.WebView()
        ->GetSettings()
        ->SetSupportDeprecatedTargetDensityDPI(true);
    web_view_helper.WebView()->GetSettings()->SetUseWideViewport(true);
    web_view_helper.Resize(WebSize(viewport_width, viewport_height));

    EXPECT_NEAR(viewport_width,
                web_view_helper.WebView()
                    ->MainFrameImpl()
                    ->GetFrameView()
                    ->GetLayoutSize()
                    .Width(),
                1.0f);
    EXPECT_NEAR(viewport_height,
                web_view_helper.WebView()
                    ->MainFrameImpl()
                    ->GetFrameView()
                    ->GetLayoutSize()
                    .Height(),
                1.0f);
    EXPECT_NEAR(1.0f, web_view_helper.WebView()->PageScaleFactor(), 0.01f);
  }
}

TEST_P(ParameterizedWebFrameTest, NoWideViewportAndScaleLessThanOne) {
  RegisterMockedHttpURLLoad("viewport-initial-scale-less-than-1.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1.33f;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-initial-scale-less-than-1.html", true, nullptr,
      &client, nullptr, EnableViewportSettings);
  web_view_helper.WebView()
      ->GetSettings()
      ->SetSupportDeprecatedTargetDensityDPI(true);
  web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
  web_view_helper.WebView()->GetSettings()->SetUseWideViewport(false);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_NEAR(viewport_width * client.screen_info_.device_scale_factor,
              web_view_helper.WebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .Width(),
              1.0f);
  EXPECT_NEAR(viewport_height * client.screen_info_.device_scale_factor,
              web_view_helper.WebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .Height(),
              1.0f);
  EXPECT_NEAR(1.0f / client.screen_info_.device_scale_factor,
              web_view_helper.WebView()->PageScaleFactor(), 0.01f);
}

TEST_P(ParameterizedWebFrameTest,
       NoWideViewportAndScaleLessThanOneWithDeviceWidth) {
  RegisterMockedHttpURLLoad(
      "viewport-initial-scale-less-than-1-device-width.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1.33f;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-initial-scale-less-than-1-device-width.html", true,
      nullptr, &client, nullptr, EnableViewportSettings);
  web_view_helper.WebView()
      ->GetSettings()
      ->SetSupportDeprecatedTargetDensityDPI(true);
  web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
  web_view_helper.WebView()->GetSettings()->SetUseWideViewport(false);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  const float kPageZoom = 0.25f;
  EXPECT_NEAR(
      viewport_width * client.screen_info_.device_scale_factor / kPageZoom,
      web_view_helper.WebView()
          ->MainFrameImpl()
          ->GetFrameView()
          ->GetLayoutSize()
          .Width(),
      1.0f);
  EXPECT_NEAR(
      viewport_height * client.screen_info_.device_scale_factor / kPageZoom,
      web_view_helper.WebView()
          ->MainFrameImpl()
          ->GetFrameView()
          ->GetLayoutSize()
          .Height(),
      1.0f);
  EXPECT_NEAR(1.0f / client.screen_info_.device_scale_factor,
              web_view_helper.WebView()->PageScaleFactor(), 0.01f);
}

TEST_P(ParameterizedWebFrameTest,
       NoWideViewportAndNoViewportWithInitialPageScaleOverride) {
  RegisterMockedHttpURLLoad("large-div.html");

  FixedLayoutTestWebViewClient client;
  int viewport_width = 640;
  int viewport_height = 480;
  float enforced_page_scale_factor = 5.0f;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "large-div.html", true, nullptr,
                                    &client, nullptr, EnableViewportSettings);
  web_view_helper.WebView()->SetDefaultPageScaleLimits(0.25f, 5);
  web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
  web_view_helper.WebView()->GetSettings()->SetUseWideViewport(false);
  web_view_helper.WebView()->SetInitialPageScaleOverride(
      enforced_page_scale_factor);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_NEAR(viewport_width / enforced_page_scale_factor,
              web_view_helper.WebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .Width(),
              1.0f);
  EXPECT_NEAR(viewport_height / enforced_page_scale_factor,
              web_view_helper.WebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .Height(),
              1.0f);
  EXPECT_NEAR(enforced_page_scale_factor,
              web_view_helper.WebView()->PageScaleFactor(), 0.01f);
}

TEST_P(ParameterizedWebFrameTest, NoUserScalableQuirkIgnoresViewportScale) {
  RegisterMockedHttpURLLoad("viewport-initial-scale-and-user-scalable-no.html");

  FixedLayoutTestWebViewClient client;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-initial-scale-and-user-scalable-no.html", true,
      nullptr, &client, nullptr, EnableViewportSettings);
  web_view_helper.WebView()->GetSettings()->SetViewportMetaNonUserScalableQuirk(
      true);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_NEAR(viewport_width,
              web_view_helper.WebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .Width(),
              1.0f);
  EXPECT_NEAR(viewport_height,
              web_view_helper.WebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .Height(),
              1.0f);
  EXPECT_NEAR(1.0f, web_view_helper.WebView()->PageScaleFactor(), 0.01f);
}

TEST_P(ParameterizedWebFrameTest,
       NoUserScalableQuirkIgnoresViewportScaleForNonWideViewport) {
  RegisterMockedHttpURLLoad("viewport-initial-scale-and-user-scalable-no.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1.33f;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-initial-scale-and-user-scalable-no.html", true,
      nullptr, &client, nullptr, EnableViewportSettings);
  web_view_helper.WebView()
      ->GetSettings()
      ->SetSupportDeprecatedTargetDensityDPI(true);
  web_view_helper.WebView()->GetSettings()->SetViewportMetaNonUserScalableQuirk(
      true);
  web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
  web_view_helper.WebView()->GetSettings()->SetUseWideViewport(false);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_NEAR(viewport_width * client.screen_info_.device_scale_factor,
              web_view_helper.WebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .Width(),
              1.0f);
  EXPECT_NEAR(viewport_height * client.screen_info_.device_scale_factor,
              web_view_helper.WebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .Height(),
              1.0f);
  EXPECT_NEAR(1.0f / client.screen_info_.device_scale_factor,
              web_view_helper.WebView()->PageScaleFactor(), 0.01f);
}

TEST_P(ParameterizedWebFrameTest,
       NoUserScalableQuirkIgnoresViewportScaleForWideViewport) {
  RegisterMockedHttpURLLoad("viewport-2x-initial-scale-non-user-scalable.html");

  FixedLayoutTestWebViewClient client;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-2x-initial-scale-non-user-scalable.html", true,
      nullptr, &client, nullptr, EnableViewportSettings);
  web_view_helper.WebView()->GetSettings()->SetViewportMetaNonUserScalableQuirk(
      true);
  web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
  web_view_helper.WebView()->GetSettings()->SetUseWideViewport(true);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_NEAR(viewport_width,
              web_view_helper.WebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .Width(),
              1.0f);
  EXPECT_NEAR(viewport_height,
              web_view_helper.WebView()
                  ->MainFrameImpl()
                  ->GetFrameView()
                  ->GetLayoutSize()
                  .Height(),
              1.0f);
  EXPECT_NEAR(1.0f, web_view_helper.WebView()->PageScaleFactor(), 0.01f);
}

TEST_P(ParameterizedWebFrameTest,
       DesktopPageCanBeZoomedInWhenWideViewportIsTurnedOff) {
  RegisterMockedHttpURLLoad("no_viewport_tag.html");

  FixedLayoutTestWebViewClient client;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "no_viewport_tag.html", true,
                                    nullptr, &client, nullptr,
                                    EnableViewportSettings);
  web_view_helper.WebView()->SetDefaultPageScaleLimits(0.25f, 5);
  web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
  web_view_helper.WebView()->GetSettings()->SetUseWideViewport(false);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_NEAR(1.0f, web_view_helper.WebView()->PageScaleFactor(), 0.01f);
  EXPECT_NEAR(1.0f, web_view_helper.WebView()->MinimumPageScaleFactor(), 0.01f);
  EXPECT_NEAR(5.0f, web_view_helper.WebView()->MaximumPageScaleFactor(), 0.01f);
}

TEST_P(ParameterizedWebFrameTest, AtViewportInsideAtMediaInitialViewport) {
  RegisterMockedHttpURLLoad("viewport-inside-media.html");

  FixedLayoutTestWebViewClient client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport-inside-media.html",
                                    true, nullptr, &client, nullptr,
                                    EnableViewportSettings);
  web_view_helper.Resize(WebSize(640, 480));

  EXPECT_EQ(2000, web_view_helper.WebView()
                      ->MainFrameImpl()
                      ->GetFrameView()
                      ->GetLayoutSize()
                      .Width());

  web_view_helper.Resize(WebSize(1200, 480));

  EXPECT_EQ(1200, web_view_helper.WebView()
                      ->MainFrameImpl()
                      ->GetFrameView()
                      ->GetLayoutSize()
                      .Width());
}

TEST_P(ParameterizedWebFrameTest, AtViewportAffectingAtMediaRecalcCount) {
  RegisterMockedHttpURLLoad("viewport-and-media.html");

  FixedLayoutTestWebViewClient client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true, nullptr, &client, nullptr,
                             EnableViewportSettings);
  web_view_helper.Resize(WebSize(640, 480));
  FrameTestHelpers::LoadFrame(web_view_helper.WebView()->MainFrame(),
                              base_url_ + "viewport-and-media.html");

  Document* document =
      web_view_helper.WebView()->MainFrameImpl()->GetFrame()->GetDocument();
  EXPECT_EQ(2000, web_view_helper.WebView()
                      ->MainFrameImpl()
                      ->GetFrameView()
                      ->GetLayoutSize()
                      .Width());

  // The styleForElementCount() should match the number of elements for a single
  // pass of computed styles construction for the document.
  EXPECT_EQ(10u, document->GetStyleEngine().StyleForElementCount());
  EXPECT_EQ(Color(0, 128, 0),
            document->body()->GetComputedStyle()->VisitedDependentColor(
                CSSPropertyColor));
}

TEST_P(ParameterizedWebFrameTest, AtViewportWithViewportLengths) {
  RegisterMockedHttpURLLoad("viewport-lengths.html");

  FixedLayoutTestWebViewClient client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true, nullptr, &client, nullptr,
                             EnableViewportSettings);
  web_view_helper.Resize(WebSize(800, 600));
  FrameTestHelpers::LoadFrame(web_view_helper.WebView()->MainFrame(),
                              base_url_ + "viewport-lengths.html");

  FrameView* view = web_view_helper.WebView()->MainFrameImpl()->GetFrameView();
  EXPECT_EQ(400, view->GetLayoutSize().Width());
  EXPECT_EQ(300, view->GetLayoutSize().Height());

  web_view_helper.Resize(WebSize(1000, 400));

  EXPECT_EQ(500, view->GetLayoutSize().Width());
  EXPECT_EQ(200, view->GetLayoutSize().Height());
}

class WebFrameResizeTest : public ParameterizedWebFrameTest {
 protected:
  static FloatSize ComputeRelativeOffset(const IntPoint& absolute_offset,
                                         const LayoutRect& rect) {
    FloatSize relative_offset =
        FloatPoint(absolute_offset) - FloatPoint(rect.Location());
    relative_offset.Scale(1.f / rect.Width(), 1.f / rect.Height());
    return relative_offset;
  }

  void TestResizeYieldsCorrectScrollAndScale(
      const char* url,
      const float initial_page_scale_factor,
      const WebSize scroll_offset,
      const WebSize viewport_size,
      const bool should_scale_relative_to_viewport_width) {
    RegisterMockedHttpURLLoad(url);

    const float aspect_ratio =
        static_cast<float>(viewport_size.width) / viewport_size.height;

    FrameTestHelpers::WebViewHelper web_view_helper;
    web_view_helper.InitializeAndLoad(base_url_ + url, true, nullptr, nullptr,
                                      nullptr, EnableViewportSettings);
    web_view_helper.WebView()->SetDefaultPageScaleLimits(0.25f, 5);

    // Origin scrollOffsets preserved under resize.
    {
      web_view_helper.Resize(
          WebSize(viewport_size.width, viewport_size.height));
      web_view_helper.WebView()->SetPageScaleFactor(initial_page_scale_factor);
      ASSERT_EQ(viewport_size, web_view_helper.WebView()->Size());
      ASSERT_EQ(initial_page_scale_factor,
                web_view_helper.WebView()->PageScaleFactor());
      web_view_helper.Resize(
          WebSize(viewport_size.height, viewport_size.width));
      float expected_page_scale_factor =
          initial_page_scale_factor *
          (should_scale_relative_to_viewport_width ? 1 / aspect_ratio : 1);
      EXPECT_NEAR(expected_page_scale_factor,
                  web_view_helper.WebView()->PageScaleFactor(), 0.05f);
      EXPECT_EQ(WebSize(),
                web_view_helper.WebView()->MainFrame()->GetScrollOffset());
    }

    // Resizing just the height should not affect pageScaleFactor or
    // scrollOffset.
    {
      web_view_helper.Resize(
          WebSize(viewport_size.width, viewport_size.height));
      web_view_helper.WebView()->SetPageScaleFactor(initial_page_scale_factor);
      web_view_helper.WebView()->MainFrame()->SetScrollOffset(scroll_offset);
      web_view_helper.WebView()->UpdateAllLifecyclePhases();
      const WebSize expected_scroll_offset =
          web_view_helper.WebView()->MainFrame()->GetScrollOffset();
      web_view_helper.Resize(
          WebSize(viewport_size.width, viewport_size.height * 0.8f));
      EXPECT_EQ(initial_page_scale_factor,
                web_view_helper.WebView()->PageScaleFactor());
      EXPECT_EQ(expected_scroll_offset,
                web_view_helper.WebView()->MainFrame()->GetScrollOffset());
      web_view_helper.Resize(
          WebSize(viewport_size.width, viewport_size.height * 0.8f));
      EXPECT_EQ(initial_page_scale_factor,
                web_view_helper.WebView()->PageScaleFactor());
      EXPECT_EQ(expected_scroll_offset,
                web_view_helper.WebView()->MainFrame()->GetScrollOffset());
    }
  }
};

INSTANTIATE_TEST_CASE_P(All, WebFrameResizeTest, ::testing::Bool());

TEST_P(WebFrameResizeTest,
       ResizeYieldsCorrectScrollAndScaleForWidthEqualsDeviceWidth) {
  // With width=device-width, pageScaleFactor is preserved across resizes as
  // long as the content adjusts according to the device-width.
  const char* url = "resize_scroll_mobile.html";
  const float kInitialPageScaleFactor = 1;
  const WebSize scroll_offset(0, 50);
  const WebSize viewport_size(120, 160);
  const bool kShouldScaleRelativeToViewportWidth = true;

  TestResizeYieldsCorrectScrollAndScale(url, kInitialPageScaleFactor,
                                        scroll_offset, viewport_size,
                                        kShouldScaleRelativeToViewportWidth);
}

TEST_P(WebFrameResizeTest, ResizeYieldsCorrectScrollAndScaleForMinimumScale) {
  // This tests a scenario where minimum-scale is set to 1.0, but some element
  // on the page is slightly larger than the portrait width, so our "natural"
  // minimum-scale would be lower. In that case, we should stick to 1.0 scale
  // on rotation and not do anything strange.
  const char* url = "resize_scroll_minimum_scale.html";
  const float kInitialPageScaleFactor = 1;
  const WebSize scroll_offset(0, 0);
  const WebSize viewport_size(240, 320);
  const bool kShouldScaleRelativeToViewportWidth = false;

  TestResizeYieldsCorrectScrollAndScale(url, kInitialPageScaleFactor,
                                        scroll_offset, viewport_size,
                                        kShouldScaleRelativeToViewportWidth);
}

TEST_P(WebFrameResizeTest, ResizeYieldsCorrectScrollAndScaleForFixedWidth) {
  // With a fixed width, pageScaleFactor scales by the relative change in
  // viewport width.
  const char* url = "resize_scroll_fixed_width.html";
  const float kInitialPageScaleFactor = 2;
  const WebSize scroll_offset(0, 200);
  const WebSize viewport_size(240, 320);
  const bool kShouldScaleRelativeToViewportWidth = true;

  TestResizeYieldsCorrectScrollAndScale(url, kInitialPageScaleFactor,
                                        scroll_offset, viewport_size,
                                        kShouldScaleRelativeToViewportWidth);
}

TEST_P(WebFrameResizeTest, ResizeYieldsCorrectScrollAndScaleForFixedLayout) {
  // With a fixed layout, pageScaleFactor scales by the relative change in
  // viewport width.
  const char* url = "resize_scroll_fixed_layout.html";
  const float kInitialPageScaleFactor = 2;
  const WebSize scroll_offset(200, 400);
  const WebSize viewport_size(320, 240);
  const bool kShouldScaleRelativeToViewportWidth = true;

  TestResizeYieldsCorrectScrollAndScale(url, kInitialPageScaleFactor,
                                        scroll_offset, viewport_size,
                                        kShouldScaleRelativeToViewportWidth);
}

TEST_P(ParameterizedWebFrameTest, pageScaleFactorUpdatesScrollbars) {
  RegisterMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fixed_layout.html", true,
                                    nullptr, &client, nullptr,
                                    EnableViewportSettings);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  FrameView* view = web_view_helper.WebView()->MainFrameImpl()->GetFrameView();
  EXPECT_EQ(view->ScrollSize(kHorizontalScrollbar),
            view->ContentsSize().Width() - view->VisibleContentRect().Width());
  EXPECT_EQ(
      view->ScrollSize(kVerticalScrollbar),
      view->ContentsSize().Height() - view->VisibleContentRect().Height());

  web_view_helper.WebView()->SetPageScaleFactor(10);

  EXPECT_EQ(view->ScrollSize(kHorizontalScrollbar),
            view->ContentsSize().Width() - view->VisibleContentRect().Width());
  EXPECT_EQ(
      view->ScrollSize(kVerticalScrollbar),
      view->ContentsSize().Height() - view->VisibleContentRect().Height());
}

TEST_P(ParameterizedWebFrameTest, CanOverrideScaleLimits) {
  RegisterMockedHttpURLLoad("no_scale_for_you.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "no_scale_for_you.html", true,
                                    nullptr, &client, nullptr,
                                    EnableViewportSettings);
  web_view_helper.WebView()->SetDefaultPageScaleLimits(0.25f, 5);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  EXPECT_EQ(2.0f, web_view_helper.WebView()->MinimumPageScaleFactor());
  EXPECT_EQ(2.0f, web_view_helper.WebView()->MaximumPageScaleFactor());

  web_view_helper.WebView()->SetIgnoreViewportTagScaleLimits(true);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();

  EXPECT_EQ(1.0f, web_view_helper.WebView()->MinimumPageScaleFactor());
  EXPECT_EQ(5.0f, web_view_helper.WebView()->MaximumPageScaleFactor());

  web_view_helper.WebView()->SetIgnoreViewportTagScaleLimits(false);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();

  EXPECT_EQ(2.0f, web_view_helper.WebView()->MinimumPageScaleFactor());
  EXPECT_EQ(2.0f, web_view_helper.WebView()->MaximumPageScaleFactor());
}

// Android doesn't have scrollbars on the main FrameView
#if OS(ANDROID)
TEST_F(WebFrameTest, DISABLED_updateOverlayScrollbarLayers)
#else
TEST_F(WebFrameTest, updateOverlayScrollbarLayers)
#endif
{
  RegisterMockedHttpURLLoad("large-div.html");

  int view_width = 500;
  int view_height = 500;

  std::unique_ptr<FakeCompositingWebViewClient>
      fake_compositing_web_view_client =
          WTF::MakeUnique<FakeCompositingWebViewClient>();
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true, nullptr,
                             fake_compositing_web_view_client.get(), nullptr,
                             &ConfigureCompositingWebView);

  web_view_helper.Resize(WebSize(view_width, view_height));
  FrameTestHelpers::LoadFrame(web_view_helper.WebView()->MainFrame(),
                              base_url_ + "large-div.html");

  FrameView* view = web_view_helper.WebView()->MainFrameImpl()->GetFrameView();
  EXPECT_TRUE(
      view->GetLayoutViewItem().Compositor()->LayerForHorizontalScrollbar());
  EXPECT_TRUE(
      view->GetLayoutViewItem().Compositor()->LayerForVerticalScrollbar());

  web_view_helper.Resize(WebSize(view_width * 10, view_height * 10));
  EXPECT_FALSE(
      view->GetLayoutViewItem().Compositor()->LayerForHorizontalScrollbar());
  EXPECT_FALSE(
      view->GetLayoutViewItem().Compositor()->LayerForVerticalScrollbar());
}

void SetScaleAndScrollAndLayout(WebViewImpl* web_view,
                                WebPoint scroll,
                                float scale) {
  web_view->SetPageScaleFactor(scale);
  web_view->MainFrame()->SetScrollOffset(WebSize(scroll.x, scroll.y));
  web_view->UpdateAllLifecyclePhases();
}

void SimulatePageScale(WebViewImpl* web_view_impl, float& scale) {
  ScrollOffset scroll_delta =
      ToScrollOffset(
          web_view_impl->FakePageScaleAnimationTargetPositionForTesting()) -
      web_view_impl->MainFrameImpl()->GetFrameView()->GetScrollOffset();
  float scale_delta =
      web_view_impl->FakePageScaleAnimationPageScaleForTesting() /
      web_view_impl->PageScaleFactor();
  web_view_impl->ApplyViewportDeltas(WebFloatSize(), FloatSize(scroll_delta),
                                     WebFloatSize(), scale_delta, 0);
  scale = web_view_impl->PageScaleFactor();
}

void SimulateMultiTargetZoom(WebViewImpl* web_view_impl,
                             const WebRect& rect,
                             float& scale) {
  if (web_view_impl->ZoomToMultipleTargetsRect(rect))
    SimulatePageScale(web_view_impl, scale);
}

void SimulateDoubleTap(WebViewImpl* web_view_impl,
                       WebPoint& point,
                       float& scale) {
  web_view_impl->AnimateDoubleTapZoom(point);
  EXPECT_TRUE(web_view_impl->FakeDoubleTapAnimationPendingForTesting());
  SimulatePageScale(web_view_impl, scale);
}

TEST_P(ParameterizedWebFrameTest, DivAutoZoomParamsTest) {
  RegisterMockedHttpURLLoad("get_scale_for_auto_zoom_into_div_test.html");

  const float kDeviceScaleFactor = 2.0f;
  int viewport_width = 640 / kDeviceScaleFactor;
  int viewport_height = 1280 / kDeviceScaleFactor;
  float double_tap_zoom_already_legible_ratio = 1.2f;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "get_scale_for_auto_zoom_into_div_test.html", false, nullptr,
      nullptr, nullptr, ConfigureAndroid);
  web_view_helper.WebView()->SetDeviceScaleFactor(kDeviceScaleFactor);
  web_view_helper.WebView()->SetDefaultPageScaleLimits(0.01f, 4);
  web_view_helper.WebView()->SetPageScaleFactor(0.5f);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  WebRect wide_div(200, 100, 400, 150);
  WebRect tall_div(200, 300, 400, 800);
  WebPoint double_tap_point_wide(wide_div.x + 50, wide_div.y + 50);
  WebPoint double_tap_point_tall(tall_div.x + 50, tall_div.y + 50);
  float scale;
  WebPoint scroll;

  float double_tap_zoom_already_legible_scale =
      web_view_helper.WebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;

  // Test double-tap zooming into wide div.
  WebRect wide_block_bound = web_view_helper.WebView()->ComputeBlockBound(
      double_tap_point_wide, false);
  web_view_helper.WebView()->ComputeScaleAndScrollForBlockRect(
      WebPoint(double_tap_point_wide.x, double_tap_point_wide.y),
      wide_block_bound, kTouchPointPadding,
      double_tap_zoom_already_legible_scale, scale, scroll);
  // The div should horizontally fill the screen (modulo margins), and
  // vertically centered (modulo integer rounding).
  EXPECT_NEAR(viewport_width / (float)wide_div.width, scale, 0.1);
  EXPECT_NEAR(wide_div.x, scroll.x, 20);
  EXPECT_EQ(0, scroll.y);

  SetScaleAndScrollAndLayout(web_view_helper.WebView(), scroll, scale);

  // Test zoom out back to minimum scale.
  wide_block_bound = web_view_helper.WebView()->ComputeBlockBound(
      double_tap_point_wide, false);
  web_view_helper.WebView()->ComputeScaleAndScrollForBlockRect(
      WebPoint(double_tap_point_wide.x, double_tap_point_wide.y),
      wide_block_bound, kTouchPointPadding,
      double_tap_zoom_already_legible_scale, scale, scroll);
  // FIXME: Looks like we are missing EXPECTs here.

  scale = web_view_helper.WebView()->MinimumPageScaleFactor();
  SetScaleAndScrollAndLayout(web_view_helper.WebView(), WebPoint(0, 0), scale);

  // Test double-tap zooming into tall div.
  WebRect tall_block_bound = web_view_helper.WebView()->ComputeBlockBound(
      double_tap_point_tall, false);
  web_view_helper.WebView()->ComputeScaleAndScrollForBlockRect(
      WebPoint(double_tap_point_tall.x, double_tap_point_tall.y),
      tall_block_bound, kTouchPointPadding,
      double_tap_zoom_already_legible_scale, scale, scroll);
  // The div should start at the top left of the viewport.
  EXPECT_NEAR(viewport_width / (float)tall_div.width, scale, 0.1);
  EXPECT_NEAR(tall_div.x, scroll.x, 20);
  EXPECT_NEAR(tall_div.y, scroll.y, 20);
}

TEST_P(ParameterizedWebFrameTest, DivAutoZoomWideDivTest) {
  RegisterMockedHttpURLLoad("get_wide_div_for_auto_zoom_test.html");

  const float kDeviceScaleFactor = 2.0f;
  int viewport_width = 640 / kDeviceScaleFactor;
  int viewport_height = 1280 / kDeviceScaleFactor;
  float double_tap_zoom_already_legible_ratio = 1.2f;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "get_wide_div_for_auto_zoom_test.html", false, nullptr,
      nullptr, nullptr, ConfigureAndroid);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));
  web_view_helper.WebView()->SetDeviceScaleFactor(kDeviceScaleFactor);
  web_view_helper.WebView()->SetPageScaleFactor(1.0f);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();

  web_view_helper.WebView()->EnableFakePageScaleAnimationForTesting(true);

  float double_tap_zoom_already_legible_scale =
      web_view_helper.WebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;

  WebRect div(0, 100, viewport_width, 150);
  WebPoint point(div.x + 50, div.y + 50);
  float scale;
  SetScaleAndScrollAndLayout(
      web_view_helper.WebView(), WebPoint(0, 0),
      (web_view_helper.WebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);

  SimulateDoubleTap(web_view_helper.WebView(), point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);
  SimulateDoubleTap(web_view_helper.WebView(), point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.WebView()->MinimumPageScaleFactor(), scale);
}

TEST_P(ParameterizedWebFrameTest, DivAutoZoomVeryTallTest) {
  // When a block is taller than the viewport and a zoom targets a lower part
  // of it, then we should keep the target point onscreen instead of snapping
  // back up the top of the block.
  RegisterMockedHttpURLLoad("very_tall_div.html");

  const float kDeviceScaleFactor = 2.0f;
  int viewport_width = 640 / kDeviceScaleFactor;
  int viewport_height = 1280 / kDeviceScaleFactor;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "very_tall_div.html", true,
                                    nullptr, nullptr, nullptr,
                                    ConfigureAndroid);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));
  web_view_helper.WebView()->SetDeviceScaleFactor(kDeviceScaleFactor);
  web_view_helper.WebView()->SetPageScaleFactor(1.0f);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();

  WebRect div(200, 300, 400, 5000);
  WebPoint point(div.x + 50, div.y + 3000);
  float scale;
  WebPoint scroll;

  WebRect block_bound =
      web_view_helper.WebView()->ComputeBlockBound(point, true);
  web_view_helper.WebView()->ComputeScaleAndScrollForBlockRect(
      point, block_bound, 0, 1.0f, scale, scroll);
  EXPECT_EQ(scale, 1.0f);
  EXPECT_EQ(scroll.y, 2660);
}

TEST_F(WebFrameTest, DivAutoZoomMultipleDivsTest) {
  RegisterMockedHttpURLLoad("get_multiple_divs_for_auto_zoom_test.html");

  const float kDeviceScaleFactor = 2.0f;
  int viewport_width = 640 / kDeviceScaleFactor;
  int viewport_height = 1280 / kDeviceScaleFactor;
  float double_tap_zoom_already_legible_ratio = 1.2f;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "get_multiple_divs_for_auto_zoom_test.html", false, nullptr,
      nullptr, nullptr, ConfigureAndroid);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));
  web_view_helper.WebView()->SetDefaultPageScaleLimits(0.5f, 4);
  web_view_helper.WebView()->SetDeviceScaleFactor(kDeviceScaleFactor);
  web_view_helper.WebView()->SetPageScaleFactor(0.5f);
  web_view_helper.WebView()->SetMaximumLegibleScale(1.f);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();

  web_view_helper.WebView()->EnableFakePageScaleAnimationForTesting(true);

  WebRect top_div(200, 100, 200, 150);
  WebRect bottom_div(200, 300, 200, 150);
  WebPoint top_point(top_div.x + 50, top_div.y + 50);
  WebPoint bottom_point(bottom_div.x + 50, bottom_div.y + 50);
  float scale;
  SetScaleAndScrollAndLayout(
      web_view_helper.WebView(), WebPoint(0, 0),
      (web_view_helper.WebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);

  // Test double tap on two different divs.  After first zoom, we should go back
  // to minimum page scale with a second double tap.
  SimulateDoubleTap(web_view_helper.WebView(), top_point, scale);
  EXPECT_FLOAT_EQ(1, scale);
  SimulateDoubleTap(web_view_helper.WebView(), bottom_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.WebView()->MinimumPageScaleFactor(), scale);

  // If the user pinch zooms after double tap, a second double tap should zoom
  // back to the div.
  SimulateDoubleTap(web_view_helper.WebView(), top_point, scale);
  EXPECT_FLOAT_EQ(1, scale);
  web_view_helper.WebView()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                                 WebFloatSize(), 0.6f, 0);
  SimulateDoubleTap(web_view_helper.WebView(), bottom_point, scale);
  EXPECT_FLOAT_EQ(1, scale);
  SimulateDoubleTap(web_view_helper.WebView(), bottom_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.WebView()->MinimumPageScaleFactor(), scale);

  // If we didn't yet get an auto-zoom update and a second double-tap arrives,
  // should go back to minimum scale.
  web_view_helper.WebView()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                                 WebFloatSize(), 1.1f, 0);
  web_view_helper.WebView()->AnimateDoubleTapZoom(top_point);
  EXPECT_TRUE(
      web_view_helper.WebView()->FakeDoubleTapAnimationPendingForTesting());
  SimulateDoubleTap(web_view_helper.WebView(), bottom_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.WebView()->MinimumPageScaleFactor(), scale);
}

TEST_F(WebFrameTest, DivAutoZoomScaleBoundsTest) {
  RegisterMockedHttpURLLoad("get_scale_bounds_check_for_auto_zoom_test.html");

  int viewport_width = 320;
  int viewport_height = 480;
  float double_tap_zoom_already_legible_ratio = 1.2f;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "get_scale_bounds_check_for_auto_zoom_test.html", false,
      nullptr, nullptr, nullptr, ConfigureAndroid);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));
  web_view_helper.WebView()->SetDeviceScaleFactor(1.5f);
  web_view_helper.WebView()->SetMaximumLegibleScale(1.f);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();

  web_view_helper.WebView()->EnableFakePageScaleAnimationForTesting(true);

  WebRect div(200, 100, 200, 150);
  WebPoint double_tap_point(div.x + 50, div.y + 50);
  float scale;

  // Test double tap scale bounds.
  // minimumPageScale < doubleTapZoomAlreadyLegibleScale < 1
  web_view_helper.WebView()->SetDefaultPageScaleLimits(0.5f, 4);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();
  float double_tap_zoom_already_legible_scale =
      web_view_helper.WebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;
  SetScaleAndScrollAndLayout(
      web_view_helper.WebView(), WebPoint(0, 0),
      (web_view_helper.WebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(1, scale);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.WebView()->MinimumPageScaleFactor(), scale);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(1, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  web_view_helper.WebView()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                                 WebFloatSize(), 1.1f, 0);
  // 1 < minimumPageScale < doubleTapZoomAlreadyLegibleScale
  web_view_helper.WebView()->SetDefaultPageScaleLimits(1.1f, 4);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();
  double_tap_zoom_already_legible_scale =
      web_view_helper.WebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;
  SetScaleAndScrollAndLayout(
      web_view_helper.WebView(), WebPoint(0, 0),
      (web_view_helper.WebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.WebView()->MinimumPageScaleFactor(), scale);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  web_view_helper.WebView()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                                 WebFloatSize(), 1.1f, 0);
  // minimumPageScale < 1 < doubleTapZoomAlreadyLegibleScale
  web_view_helper.WebView()->SetDefaultPageScaleLimits(0.95f, 4);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();
  double_tap_zoom_already_legible_scale =
      web_view_helper.WebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;
  SetScaleAndScrollAndLayout(
      web_view_helper.WebView(), WebPoint(0, 0),
      (web_view_helper.WebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.WebView()->MinimumPageScaleFactor(), scale);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);
}

TEST_F(WebFrameTest, DivAutoZoomScaleLegibleScaleTest) {
  RegisterMockedHttpURLLoad("get_scale_bounds_check_for_auto_zoom_test.html");

  int viewport_width = 320;
  int viewport_height = 480;
  float double_tap_zoom_already_legible_ratio = 1.2f;
  float maximum_legible_scale_factor = 1.13f;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "get_scale_bounds_check_for_auto_zoom_test.html", false,
      nullptr, nullptr, nullptr, ConfigureAndroid);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));
  web_view_helper.WebView()->SetMaximumLegibleScale(
      maximum_legible_scale_factor);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();

  web_view_helper.WebView()->EnableFakePageScaleAnimationForTesting(true);
  web_view_helper.WebView()->GetPage()->GetSettings().SetTextAutosizingEnabled(
      true);

  WebRect div(200, 100, 200, 150);
  WebPoint double_tap_point(div.x + 50, div.y + 50);
  float scale;

  // Test double tap scale bounds.
  // minimumPageScale < doubleTapZoomAlreadyLegibleScale < 1 <
  //     maximumLegibleScaleFactor
  float legible_scale = maximum_legible_scale_factor;
  SetScaleAndScrollAndLayout(
      web_view_helper.WebView(), WebPoint(0, 0),
      (web_view_helper.WebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);
  float double_tap_zoom_already_legible_scale =
      web_view_helper.WebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;
  web_view_helper.WebView()->SetDefaultPageScaleLimits(0.5f, 4);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(legible_scale, scale);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.WebView()->MinimumPageScaleFactor(), scale);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(legible_scale, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  web_view_helper.WebView()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                                 WebFloatSize(), 1.1f, 0);
  // 1 < maximumLegibleScaleFactor < minimumPageScale <
  //     doubleTapZoomAlreadyLegibleScale
  web_view_helper.WebView()->SetDefaultPageScaleLimits(1.0f, 4);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();
  double_tap_zoom_already_legible_scale =
      web_view_helper.WebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;
  SetScaleAndScrollAndLayout(
      web_view_helper.WebView(), WebPoint(0, 0),
      (web_view_helper.WebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.WebView()->MinimumPageScaleFactor(), scale);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  web_view_helper.WebView()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                                 WebFloatSize(), 1.1f, 0);
  // minimumPageScale < 1 < maximumLegibleScaleFactor <
  //     doubleTapZoomAlreadyLegibleScale
  web_view_helper.WebView()->SetDefaultPageScaleLimits(0.95f, 4);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();
  double_tap_zoom_already_legible_scale =
      web_view_helper.WebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;
  SetScaleAndScrollAndLayout(
      web_view_helper.WebView(), WebPoint(0, 0),
      (web_view_helper.WebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.WebView()->MinimumPageScaleFactor(), scale);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  web_view_helper.WebView()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                                 WebFloatSize(), 1.1f, 0);
  // minimumPageScale < 1 < doubleTapZoomAlreadyLegibleScale <
  //     maximumLegibleScaleFactor
  web_view_helper.WebView()->SetDefaultPageScaleLimits(0.9f, 4);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();
  double_tap_zoom_already_legible_scale =
      web_view_helper.WebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;
  SetScaleAndScrollAndLayout(
      web_view_helper.WebView(), WebPoint(0, 0),
      (web_view_helper.WebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(legible_scale, scale);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.WebView()->MinimumPageScaleFactor(), scale);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(legible_scale, scale);
}

TEST_F(WebFrameTest, DivAutoZoomScaleFontScaleFactorTest) {
  RegisterMockedHttpURLLoad("get_scale_bounds_check_for_auto_zoom_test.html");

  int viewport_width = 320;
  int viewport_height = 480;
  float double_tap_zoom_already_legible_ratio = 1.2f;
  float accessibility_font_scale_factor = 1.13f;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "get_scale_bounds_check_for_auto_zoom_test.html", false,
      nullptr, nullptr, nullptr, ConfigureAndroid);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));
  web_view_helper.WebView()->SetMaximumLegibleScale(1.f);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();

  web_view_helper.WebView()->EnableFakePageScaleAnimationForTesting(true);
  web_view_helper.WebView()->GetPage()->GetSettings().SetTextAutosizingEnabled(
      true);
  web_view_helper.WebView()
      ->GetPage()
      ->GetSettings()
      .SetAccessibilityFontScaleFactor(accessibility_font_scale_factor);

  WebRect div(200, 100, 200, 150);
  WebPoint double_tap_point(div.x + 50, div.y + 50);
  float scale;

  // Test double tap scale bounds.
  // minimumPageScale < doubleTapZoomAlreadyLegibleScale < 1 <
  //     accessibilityFontScaleFactor
  float legible_scale = accessibility_font_scale_factor;
  SetScaleAndScrollAndLayout(
      web_view_helper.WebView(), WebPoint(0, 0),
      (web_view_helper.WebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);
  float double_tap_zoom_already_legible_scale =
      web_view_helper.WebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;
  web_view_helper.WebView()->SetDefaultPageScaleLimits(0.5f, 4);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(legible_scale, scale);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.WebView()->MinimumPageScaleFactor(), scale);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(legible_scale, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  web_view_helper.WebView()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                                 WebFloatSize(), 1.1f, 0);
  // 1 < accessibilityFontScaleFactor < minimumPageScale <
  //     doubleTapZoomAlreadyLegibleScale
  web_view_helper.WebView()->SetDefaultPageScaleLimits(1.0f, 4);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();
  double_tap_zoom_already_legible_scale =
      web_view_helper.WebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;
  SetScaleAndScrollAndLayout(
      web_view_helper.WebView(), WebPoint(0, 0),
      (web_view_helper.WebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.WebView()->MinimumPageScaleFactor(), scale);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  web_view_helper.WebView()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                                 WebFloatSize(), 1.1f, 0);
  // minimumPageScale < 1 < accessibilityFontScaleFactor <
  //     doubleTapZoomAlreadyLegibleScale
  web_view_helper.WebView()->SetDefaultPageScaleLimits(0.95f, 4);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();
  double_tap_zoom_already_legible_scale =
      web_view_helper.WebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;
  SetScaleAndScrollAndLayout(
      web_view_helper.WebView(), WebPoint(0, 0),
      (web_view_helper.WebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.WebView()->MinimumPageScaleFactor(), scale);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(double_tap_zoom_already_legible_scale, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  web_view_helper.WebView()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                                 WebFloatSize(), 1.1f, 0);
  // minimumPageScale < 1 < doubleTapZoomAlreadyLegibleScale <
  //     accessibilityFontScaleFactor
  web_view_helper.WebView()->SetDefaultPageScaleLimits(0.9f, 4);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();
  double_tap_zoom_already_legible_scale =
      web_view_helper.WebView()->MinimumPageScaleFactor() *
      double_tap_zoom_already_legible_ratio;
  SetScaleAndScrollAndLayout(
      web_view_helper.WebView(), WebPoint(0, 0),
      (web_view_helper.WebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(legible_scale, scale);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(web_view_helper.WebView()->MinimumPageScaleFactor(), scale);
  SimulateDoubleTap(web_view_helper.WebView(), double_tap_point, scale);
  EXPECT_FLOAT_EQ(legible_scale, scale);
}

TEST_P(ParameterizedWebFrameTest, BlockBoundTest) {
  RegisterMockedHttpURLLoad("block_bound.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "block_bound.html", false,
                                    nullptr, nullptr, nullptr,
                                    ConfigureAndroid);
  web_view_helper.Resize(WebSize(300, 300));

  IntRect rect_back = IntRect(0, 0, 200, 200);
  IntRect rect_left_top = IntRect(10, 10, 80, 80);
  IntRect rect_right_bottom = IntRect(110, 110, 80, 80);
  IntRect block_bound;

  block_bound = IntRect(
      web_view_helper.WebView()->ComputeBlockBound(WebPoint(9, 9), true));
  EXPECT_RECT_EQ(rect_back, block_bound);

  block_bound = IntRect(
      web_view_helper.WebView()->ComputeBlockBound(WebPoint(10, 10), true));
  EXPECT_RECT_EQ(rect_left_top, block_bound);

  block_bound = IntRect(
      web_view_helper.WebView()->ComputeBlockBound(WebPoint(50, 50), true));
  EXPECT_RECT_EQ(rect_left_top, block_bound);

  block_bound = IntRect(
      web_view_helper.WebView()->ComputeBlockBound(WebPoint(89, 89), true));
  EXPECT_RECT_EQ(rect_left_top, block_bound);

  block_bound = IntRect(
      web_view_helper.WebView()->ComputeBlockBound(WebPoint(90, 90), true));
  EXPECT_RECT_EQ(rect_back, block_bound);

  block_bound = IntRect(
      web_view_helper.WebView()->ComputeBlockBound(WebPoint(109, 109), true));
  EXPECT_RECT_EQ(rect_back, block_bound);

  block_bound = IntRect(
      web_view_helper.WebView()->ComputeBlockBound(WebPoint(110, 110), true));
  EXPECT_RECT_EQ(rect_right_bottom, block_bound);
}

TEST_P(ParameterizedWebFrameTest, DivMultipleTargetZoomMultipleDivsTest) {
  RegisterMockedHttpURLLoad("get_multiple_divs_for_auto_zoom_test.html");

  const float kDeviceScaleFactor = 2.0f;
  int viewport_width = 640 / kDeviceScaleFactor;
  int viewport_height = 1280 / kDeviceScaleFactor;
  float double_tap_zoom_already_legible_ratio = 1.2f;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "get_multiple_divs_for_auto_zoom_test.html");
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));
  web_view_helper.WebView()->SetDefaultPageScaleLimits(0.5f, 4);
  web_view_helper.WebView()->SetDeviceScaleFactor(kDeviceScaleFactor);
  web_view_helper.WebView()->SetPageScaleFactor(0.5f);
  web_view_helper.WebView()->SetMaximumLegibleScale(1.f);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();

  web_view_helper.WebView()->EnableFakePageScaleAnimationForTesting(true);

  WebRect viewport_rect(0, 0, viewport_width, viewport_height);
  WebRect top_div(200, 100, 200, 150);
  WebRect bottom_div(200, 300, 200, 150);
  float scale;
  SetScaleAndScrollAndLayout(
      web_view_helper.WebView(), WebPoint(0, 0),
      (web_view_helper.WebView()->MinimumPageScaleFactor()) *
          (1 + double_tap_zoom_already_legible_ratio) / 2);

  SimulateMultiTargetZoom(web_view_helper.WebView(), top_div, scale);
  EXPECT_FLOAT_EQ(1, scale);
  SimulateMultiTargetZoom(web_view_helper.WebView(), bottom_div, scale);
  EXPECT_FLOAT_EQ(1, scale);
  SimulateMultiTargetZoom(web_view_helper.WebView(), viewport_rect, scale);
  EXPECT_FLOAT_EQ(1, scale);
  web_view_helper.WebView()->SetPageScaleFactor(
      web_view_helper.WebView()->MinimumPageScaleFactor());
  SimulateMultiTargetZoom(web_view_helper.WebView(), top_div, scale);
  EXPECT_FLOAT_EQ(1, scale);
}

TEST_F(WebFrameTest, DontZoomInOnFocusedInTouchAction) {
  RegisterMockedHttpURLLoad("textbox_in_touch_action.html");

  int viewport_width = 600;
  int viewport_height = 1000;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "textbox_in_touch_action.html");
  web_view_helper.WebView()->SetDefaultPageScaleLimits(0.25f, 4);
  web_view_helper.WebView()->EnableFakePageScaleAnimationForTesting(true);
  web_view_helper.WebView()->GetPage()->GetSettings().SetTextAutosizingEnabled(
      false);
  web_view_helper.WebView()
      ->GetSettings()
      ->SetAutoZoomFocusedNodeToLegibleScale(true);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));

  float initial_scale = web_view_helper.WebView()->PageScaleFactor();

  // Focus the first textbox that's in a touch-action: pan-x ancestor, this
  // shouldn't cause an autozoom since pan-x disables pinch-zoom.
  web_view_helper.WebView()->AdvanceFocus(false);
  web_view_helper.WebView()->ScrollFocusedEditableElementIntoRect(WebRect());
  EXPECT_EQ(
      web_view_helper.WebView()->FakePageScaleAnimationPageScaleForTesting(),
      0);

  SetScaleAndScrollAndLayout(web_view_helper.WebView(), WebPoint(0, 0),
                             initial_scale);
  ASSERT_EQ(initial_scale, web_view_helper.WebView()->PageScaleFactor());

  // Focus the second textbox that's in a touch-action: manipulation ancestor,
  // this should cause an autozoom since it allows pinch-zoom.
  web_view_helper.WebView()->AdvanceFocus(false);
  web_view_helper.WebView()->ScrollFocusedEditableElementIntoRect(WebRect());
  EXPECT_GT(
      web_view_helper.WebView()->FakePageScaleAnimationPageScaleForTesting(),
      initial_scale);

  SetScaleAndScrollAndLayout(web_view_helper.WebView(), WebPoint(0, 0),
                             initial_scale);
  ASSERT_EQ(initial_scale, web_view_helper.WebView()->PageScaleFactor());

  // Focus the third textbox that has a touch-action: pan-x ancestor, this
  // should cause an autozoom since it's seperated from the node with the
  // touch-action by an overflow:scroll element.
  web_view_helper.WebView()->AdvanceFocus(false);
  web_view_helper.WebView()->ScrollFocusedEditableElementIntoRect(WebRect());
  EXPECT_GT(
      web_view_helper.WebView()->FakePageScaleAnimationPageScaleForTesting(),
      initial_scale);
}

TEST_F(WebFrameTest, DivScrollIntoEditableTest) {
  RegisterMockedHttpURLLoad("get_scale_for_zoom_into_editable_test.html");

  const bool kAutoZoomToLegibleScale = true;
  int viewport_width = 450;
  int viewport_height = 300;
  float left_box_ratio = 0.3f;
  int caret_padding = 10;
  float min_readable_caret_height = 16.0f;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "get_scale_for_zoom_into_editable_test.html");
  web_view_helper.WebView()->GetPage()->GetSettings().SetTextAutosizingEnabled(
      false);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));
  web_view_helper.WebView()->SetDefaultPageScaleLimits(0.25f, 4);

  web_view_helper.WebView()->EnableFakePageScaleAnimationForTesting(true);

  WebRect edit_box_with_text(200, 200, 250, 20);
  WebRect edit_box_with_no_text(200, 250, 250, 20);

  // Test scrolling the focused node
  // The edit box is shorter and narrower than the viewport when legible.
  web_view_helper.WebView()->AdvanceFocus(false);
  // Set the caret to the end of the input box.
  web_view_helper.WebView()
      ->MainFrame()
      ->GetDocument()
      .GetElementById("EditBoxWithText")
      .To<WebInputElement>()
      .SetSelectionRange(1000, 1000);
  SetScaleAndScrollAndLayout(web_view_helper.WebView(), WebPoint(0, 0), 1);
  WebRect rect, caret;
  web_view_helper.WebView()->SelectionBounds(caret, rect);

  // Set the page scale to be smaller than the minimal readable scale.
  float initial_scale = min_readable_caret_height / caret.height * 0.5f;
  SetScaleAndScrollAndLayout(web_view_helper.WebView(), WebPoint(0, 0),
                             initial_scale);

  float scale;
  IntPoint scroll;
  bool need_animation;
  web_view_helper.WebView()->ComputeScaleAndScrollForFocusedNode(
      web_view_helper.WebView()->FocusedElement(), kAutoZoomToLegibleScale,
      scale, scroll, need_animation);
  EXPECT_TRUE(need_animation);
  // The edit box should be left aligned with a margin for possible label.
  int h_scroll = edit_box_with_text.x - left_box_ratio * viewport_width / scale;
  EXPECT_NEAR(h_scroll, scroll.X(), 2);
  int v_scroll = edit_box_with_text.y -
                 (viewport_height / scale - edit_box_with_text.height) / 2;
  EXPECT_NEAR(v_scroll, scroll.Y(), 2);
  EXPECT_NEAR(min_readable_caret_height / caret.height, scale, 0.1);

  // The edit box is wider than the viewport when legible.
  viewport_width = 200;
  viewport_height = 150;
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));
  SetScaleAndScrollAndLayout(web_view_helper.WebView(), WebPoint(0, 0),
                             initial_scale);
  web_view_helper.WebView()->ComputeScaleAndScrollForFocusedNode(
      web_view_helper.WebView()->FocusedElement(), kAutoZoomToLegibleScale,
      scale, scroll, need_animation);
  EXPECT_TRUE(need_animation);
  // The caret should be right aligned since the caret would be offscreen when
  // the edit box is left aligned.
  h_scroll = caret.x + caret.width + caret_padding - viewport_width / scale;
  EXPECT_NEAR(h_scroll, scroll.X(), 2);
  EXPECT_NEAR(min_readable_caret_height / caret.height, scale, 0.1);

  SetScaleAndScrollAndLayout(web_view_helper.WebView(), WebPoint(0, 0),
                             initial_scale);
  // Move focus to edit box with text.
  web_view_helper.WebView()->AdvanceFocus(false);
  web_view_helper.WebView()->ComputeScaleAndScrollForFocusedNode(
      web_view_helper.WebView()->FocusedElement(), kAutoZoomToLegibleScale,
      scale, scroll, need_animation);
  EXPECT_TRUE(need_animation);
  // The edit box should be left aligned.
  h_scroll = edit_box_with_no_text.x;
  EXPECT_NEAR(h_scroll, scroll.X(), 2);
  v_scroll = edit_box_with_no_text.y -
             (viewport_height / scale - edit_box_with_no_text.height) / 2;
  EXPECT_NEAR(v_scroll, scroll.Y(), 2);
  EXPECT_NEAR(min_readable_caret_height / caret.height, scale, 0.1);

  // Move focus back to the first edit box.
  web_view_helper.WebView()->AdvanceFocus(true);
  // Zoom out slightly.
  const float within_tolerance_scale = scale * 0.9f;
  SetScaleAndScrollAndLayout(web_view_helper.WebView(), scroll,
                             within_tolerance_scale);
  // Move focus back to the second edit box.
  web_view_helper.WebView()->AdvanceFocus(false);
  web_view_helper.WebView()->ComputeScaleAndScrollForFocusedNode(
      web_view_helper.WebView()->FocusedElement(), kAutoZoomToLegibleScale,
      scale, scroll, need_animation);
  // The scale should not be adjusted as the zoomed out scale was sufficiently
  // close to the previously focused scale.
  EXPECT_FALSE(need_animation);
}

TEST_F(WebFrameTest, DivScrollIntoEditablePreservePageScaleTest) {
  RegisterMockedHttpURLLoad("get_scale_for_zoom_into_editable_test.html");

  const bool kAutoZoomToLegibleScale = true;
  const int kViewportWidth = 450;
  const int kViewportHeight = 300;
  const float kMinReadableCaretHeight = 16.0f;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "get_scale_for_zoom_into_editable_test.html");
  web_view_helper.WebView()->GetPage()->GetSettings().SetTextAutosizingEnabled(
      false);
  web_view_helper.Resize(WebSize(kViewportWidth, kViewportHeight));
  web_view_helper.WebView()->EnableFakePageScaleAnimationForTesting(true);

  const WebRect edit_box_with_text(200, 200, 250, 20);

  web_view_helper.WebView()->AdvanceFocus(false);
  // Set the caret to the begining of the input box.
  web_view_helper.WebView()
      ->MainFrame()
      ->GetDocument()
      .GetElementById("EditBoxWithText")
      .To<WebInputElement>()
      .SetSelectionRange(0, 0);
  SetScaleAndScrollAndLayout(web_view_helper.WebView(), WebPoint(0, 0), 1);
  WebRect rect, caret;
  web_view_helper.WebView()->SelectionBounds(caret, rect);

  // Set the page scale to be twice as large as the minimal readable scale.
  float new_scale = kMinReadableCaretHeight / caret.height * 2.0;
  SetScaleAndScrollAndLayout(web_view_helper.WebView(), WebPoint(0, 0),
                             new_scale);

  float scale;
  IntPoint scroll;
  bool need_animation;
  web_view_helper.WebView()->ComputeScaleAndScrollForFocusedNode(
      web_view_helper.WebView()->FocusedElement(), kAutoZoomToLegibleScale,
      scale, scroll, need_animation);
  EXPECT_TRUE(need_animation);
  // Edit box and caret should be left alinged
  int h_scroll = edit_box_with_text.x;
  EXPECT_NEAR(h_scroll, scroll.X(), 1);
  int v_scroll = edit_box_with_text.y -
                 (kViewportHeight / scale - edit_box_with_text.height) / 2;
  EXPECT_NEAR(v_scroll, scroll.Y(), 1);
  // Page scale have to be unchanged
  EXPECT_EQ(new_scale, scale);

  // Set page scale and scroll such that edit box will be under the screen
  new_scale = 3.0;
  h_scroll = 200;
  SetScaleAndScrollAndLayout(web_view_helper.WebView(), WebPoint(h_scroll, 0),
                             new_scale);
  web_view_helper.WebView()->ComputeScaleAndScrollForFocusedNode(
      web_view_helper.WebView()->FocusedElement(), kAutoZoomToLegibleScale,
      scale, scroll, need_animation);
  EXPECT_TRUE(need_animation);
  // Horizontal scroll have to be the same
  EXPECT_NEAR(h_scroll, scroll.X(), 1);
  v_scroll = edit_box_with_text.y -
             (kViewportHeight / scale - edit_box_with_text.height) / 2;
  EXPECT_NEAR(v_scroll, scroll.Y(), 1);
  // Page scale have to be unchanged
  EXPECT_EQ(new_scale, scale);
}

// Tests the scroll into view functionality when
// autoZoomeFocusedNodeToLegibleScale set to false. i.e. The path non-Android
// platforms take.
TEST_F(WebFrameTest, DivScrollIntoEditableTestZoomToLegibleScaleDisabled) {
  RegisterMockedHttpURLLoad("get_scale_for_zoom_into_editable_test.html");

  const bool kAutoZoomToLegibleScale = false;
  int viewport_width = 100;
  int viewport_height = 100;
  float left_box_ratio = 0.3f;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "get_scale_for_zoom_into_editable_test.html");
  web_view_helper.WebView()->GetPage()->GetSettings().SetTextAutosizingEnabled(
      false);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));
  web_view_helper.WebView()->SetDefaultPageScaleLimits(0.25f, 4);

  web_view_helper.WebView()->EnableFakePageScaleAnimationForTesting(true);

  WebRect edit_box_with_text(200, 200, 250, 20);
  WebRect edit_box_with_no_text(200, 250, 250, 20);

  // Test scrolling the focused node
  // Since we're zoomed out, the caret is considered too small to be legible and
  // so we'd normally zoom in. Make sure we don't change scale since the
  // auto-zoom setting is off.

  // Focus the second empty textbox.
  web_view_helper.WebView()->AdvanceFocus(false);
  web_view_helper.WebView()->AdvanceFocus(false);

  // Set the page scale to be smaller than the minimal readable scale.
  float initial_scale = 0.25f;
  SetScaleAndScrollAndLayout(web_view_helper.WebView(), WebPoint(0, 0),
                             initial_scale);

  float scale;
  IntPoint scroll;
  bool need_animation;
  web_view_helper.WebView()->ComputeScaleAndScrollForFocusedNode(
      web_view_helper.WebView()->FocusedElement(), kAutoZoomToLegibleScale,
      scale, scroll, need_animation);

  // There should be no change in page scale.
  EXPECT_EQ(initial_scale, scale);
  // The edit box should be left aligned with a margin for possible label.
  EXPECT_TRUE(need_animation);
  int h_scroll =
      edit_box_with_no_text.x - left_box_ratio * viewport_width / scale;
  EXPECT_NEAR(h_scroll, scroll.X(), 2);
  int v_scroll = edit_box_with_no_text.y -
                 (viewport_height / scale - edit_box_with_no_text.height) / 2;
  EXPECT_NEAR(v_scroll, scroll.Y(), 2);

  SetScaleAndScrollAndLayout(web_view_helper.WebView(), scroll, scale);

  // Select the first textbox.
  web_view_helper.WebView()->AdvanceFocus(true);
  WebRect rect, caret;
  web_view_helper.WebView()->SelectionBounds(caret, rect);
  web_view_helper.WebView()->ComputeScaleAndScrollForFocusedNode(
      web_view_helper.WebView()->FocusedElement(), kAutoZoomToLegibleScale,
      scale, scroll, need_animation);

  // There should be no change at all since the textbox is fully visible
  // already.
  EXPECT_EQ(initial_scale, scale);
  EXPECT_FALSE(need_animation);
}

TEST_P(ParameterizedWebFrameTest, CharacterIndexAtPointWithPinchZoom) {
  RegisterMockedHttpURLLoad("sometext.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "sometext.html");
  web_view_helper.Resize(WebSize(640, 480));

  web_view_helper.WebView()->SetPageScaleFactor(2);
  web_view_helper.WebView()->SetVisualViewportOffset(WebFloatPoint(50, 60));

  WebRect base_rect;
  WebRect extent_rect;

  WebLocalFrame* main_frame =
      web_view_helper.WebView()->MainFrame()->ToWebLocalFrame();
  size_t ix = main_frame->CharacterIndexForPoint(WebPoint(320, 388));

  EXPECT_EQ(2ul, ix);
}

TEST_P(ParameterizedWebFrameTest, FirstRectForCharacterRangeWithPinchZoom) {
  RegisterMockedHttpURLLoad("textbox.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "textbox.html", true);
  web_view_helper.Resize(WebSize(640, 480));

  WebLocalFrame* main_frame =
      web_view_helper.WebView()->MainFrame()->ToWebLocalFrame();
  main_frame->ExecuteScript(WebScriptSource("selectRange();"));

  WebRect old_rect;
  main_frame->FirstRectForCharacterRange(0, 5, old_rect);

  WebFloatPoint visual_offset(100, 130);
  float scale = 2;
  web_view_helper.WebView()->SetPageScaleFactor(scale);
  web_view_helper.WebView()->SetVisualViewportOffset(visual_offset);

  WebRect base_rect;
  WebRect extent_rect;

  WebRect rect;
  main_frame->FirstRectForCharacterRange(0, 5, rect);

  EXPECT_EQ((old_rect.x - visual_offset.x) * scale, rect.x);
  EXPECT_EQ((old_rect.y - visual_offset.y) * scale, rect.y);
  EXPECT_EQ(old_rect.width * scale, rect.width);
  EXPECT_EQ(old_rect.height * scale, rect.height);
}
class TestReloadDoesntRedirectWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  WebNavigationPolicy DecidePolicyForNavigation(
      const NavigationPolicyInfo& info) override {
    EXPECT_FALSE(info.extra_data);
    return kWebNavigationPolicyCurrentTab;
  }
};

TEST_P(ParameterizedWebFrameTest, ReloadDoesntSetRedirect) {
  // Test for case in http://crbug.com/73104. Reloading a frame very quickly
  // would sometimes call decidePolicyForNavigation with isRedirect=true
  RegisterMockedHttpURLLoad("form.html");

  TestReloadDoesntRedirectWebFrameClient web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "form.html", false,
                                    &web_frame_client);

  web_view_helper.WebView()->MainFrame()->Reload(
      WebFrameLoadType::kReloadBypassingCache);
  // start another reload before request is delivered.
  FrameTestHelpers::ReloadFrameBypassingCache(
      web_view_helper.WebView()->MainFrame());
}

class ClearScrollStateOnCommitWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  void DidCommitProvisionalLoad(const WebHistoryItem&,
                                WebHistoryCommitType) override {
    Frame()->View()->ResetScrollAndScaleState();
  }
};

TEST_F(WebFrameTest, ReloadWithOverrideURLPreservesState) {
  const std::string first_url = "200-by-300.html";
  const std::string second_url = "content-width-1000.html";
  const std::string third_url = "very_tall_div.html";
  const float kPageScaleFactor = 1.1684f;
  const int kPageWidth = 120;
  const int kPageHeight = 100;

  RegisterMockedHttpURLLoad(first_url);
  RegisterMockedHttpURLLoad(second_url);
  RegisterMockedHttpURLLoad(third_url);

  ClearScrollStateOnCommitWebFrameClient client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + first_url, true, &client);
  web_view_helper.Resize(WebSize(kPageWidth, kPageHeight));
  web_view_helper.WebView()->MainFrame()->SetScrollOffset(
      WebSize(kPageWidth / 4, kPageHeight / 4));
  web_view_helper.WebView()->SetPageScaleFactor(kPageScaleFactor);

  WebSize previous_offset =
      web_view_helper.WebView()->MainFrame()->GetScrollOffset();
  float previous_scale = web_view_helper.WebView()->PageScaleFactor();

  // Reload the page and end up at the same url. State should be propagated.
  web_view_helper.WebView()->MainFrame()->ReloadWithOverrideURL(
      ToKURL(base_url_ + first_url), WebFrameLoadType::kReload);
  FrameTestHelpers::PumpPendingRequestsForFrameToLoad(
      web_view_helper.WebView()->MainFrame());
  EXPECT_EQ(previous_offset.width,
            web_view_helper.WebView()->MainFrame()->GetScrollOffset().width);
  EXPECT_EQ(previous_offset.height,
            web_view_helper.WebView()->MainFrame()->GetScrollOffset().height);
  EXPECT_EQ(previous_scale, web_view_helper.WebView()->PageScaleFactor());

  // Reload the page using the cache. State should not be propagated.
  web_view_helper.WebView()->MainFrame()->ReloadWithOverrideURL(
      ToKURL(base_url_ + second_url), WebFrameLoadType::kReload);
  FrameTestHelpers::PumpPendingRequestsForFrameToLoad(
      web_view_helper.WebView()->MainFrame());
  EXPECT_EQ(0, web_view_helper.WebView()->MainFrame()->GetScrollOffset().width);
  EXPECT_EQ(0,
            web_view_helper.WebView()->MainFrame()->GetScrollOffset().height);
  EXPECT_EQ(1.0f, web_view_helper.WebView()->PageScaleFactor());

  // Reload the page while bypassing the cache. State should not be propagated.
  web_view_helper.WebView()->MainFrame()->ReloadWithOverrideURL(
      ToKURL(base_url_ + third_url), WebFrameLoadType::kReloadBypassingCache);
  FrameTestHelpers::PumpPendingRequestsForFrameToLoad(
      web_view_helper.WebView()->MainFrame());
  EXPECT_EQ(0, web_view_helper.WebView()->MainFrame()->GetScrollOffset().width);
  EXPECT_EQ(0,
            web_view_helper.WebView()->MainFrame()->GetScrollOffset().height);
  EXPECT_EQ(1.0f, web_view_helper.WebView()->PageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, ReloadWhileProvisional) {
  // Test that reloading while the previous load is still pending does not cause
  // the initial request to get lost.
  RegisterMockedHttpURLLoad("fixed_layout.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();
  WebURLRequest request(ToKURL(base_url_ + "fixed_layout.html"));
  web_view_helper.WebView()->MainFrame()->LoadRequest(request);
  // start reload before first request is delivered.
  FrameTestHelpers::ReloadFrameBypassingCache(
      web_view_helper.WebView()->MainFrame());

  WebDataSource* data_source =
      web_view_helper.WebView()->MainFrame()->DataSource();
  ASSERT_TRUE(data_source);
  EXPECT_EQ(ToKURL(base_url_ + "fixed_layout.html"),
            KURL(data_source->GetRequest().Url()));
}

TEST_P(ParameterizedWebFrameTest, AppendRedirects) {
  const std::string first_url = "about:blank";
  const std::string second_url = "http://internal.test";

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(first_url, true);

  WebDataSource* data_source =
      web_view_helper.WebView()->MainFrame()->DataSource();
  ASSERT_TRUE(data_source);
  data_source->AppendRedirect(ToKURL(second_url));

  WebVector<WebURL> redirects;
  data_source->RedirectChain(redirects);
  ASSERT_EQ(2U, redirects.size());
  EXPECT_EQ(ToKURL(first_url), KURL(redirects[0]));
  EXPECT_EQ(ToKURL(second_url), KURL(redirects[1]));
}

TEST_P(ParameterizedWebFrameTest, IframeRedirect) {
  RegisterMockedHttpURLLoad("iframe_redirect.html");
  RegisterMockedHttpURLLoad("visible_iframe.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "iframe_redirect.html", true);
  // Pump pending requests one more time. The test page loads script that
  // navigates.
  FrameTestHelpers::PumpPendingRequestsForFrameToLoad(
      web_view_helper.WebView()->MainFrame());

  WebFrame* iframe = web_view_helper.WebView()->FindFrameByName(
      WebString::FromUTF8("ifr"), nullptr);
  ASSERT_TRUE(iframe);
  WebDataSource* iframe_data_source = iframe->DataSource();
  ASSERT_TRUE(iframe_data_source);
  WebVector<WebURL> redirects;
  iframe_data_source->RedirectChain(redirects);
  ASSERT_EQ(2U, redirects.size());
  EXPECT_EQ(ToKURL("about:blank"), KURL(redirects[0]));
  EXPECT_EQ(ToKURL("http://internal.test/visible_iframe.html"),
            KURL(redirects[1]));
}

TEST_P(ParameterizedWebFrameTest, ClearFocusedNodeTest) {
  RegisterMockedHttpURLLoad("iframe_clear_focused_node_test.html");
  RegisterMockedHttpURLLoad("autofocus_input_field_iframe.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "iframe_clear_focused_node_test.html", true);

  // Clear the focused node.
  web_view_helper.WebView()->ClearFocusedElement();

  // Now retrieve the FocusedNode and test it should be null.
  EXPECT_EQ(0, web_view_helper.WebView()->FocusedElement());
}

class ChangedSelectionCounter : public FrameTestHelpers::TestWebFrameClient {
 public:
  ChangedSelectionCounter() : call_count_(0) {}
  void DidChangeSelection(bool isSelectionEmpty) { ++call_count_; }
  int Count() const { return call_count_; }
  void Reset() { call_count_ = 0; }

 private:
  int call_count_;
};

TEST_P(ParameterizedWebFrameTest, TabKeyCursorMoveTriggersOneSelectionChange) {
  ChangedSelectionCounter counter;
  FrameTestHelpers::WebViewHelper web_view_helper;
  RegisterMockedHttpURLLoad("editable_elements.html");
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "editable_elements.html", true, &counter);

  WebKeyboardEvent tab_down(WebInputEvent::kKeyDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::kTimeStampForTesting);
  WebKeyboardEvent tab_up(WebInputEvent::kKeyUp, WebInputEvent::kNoModifiers,
                          WebInputEvent::kTimeStampForTesting);
  tab_down.dom_key = Platform::Current()->DomKeyEnumFromString("\t");
  tab_up.dom_key = Platform::Current()->DomKeyEnumFromString("\t");
  tab_down.windows_key_code = VKEY_TAB;
  tab_up.windows_key_code = VKEY_TAB;

  // Move to the next text-field: 1 cursor change.
  counter.Reset();
  web_view->HandleInputEvent(WebCoalescedInputEvent(tab_down));
  web_view->HandleInputEvent(WebCoalescedInputEvent(tab_up));
  EXPECT_EQ(1, counter.Count());

  // Move to another text-field: 1 cursor change.
  web_view->HandleInputEvent(WebCoalescedInputEvent(tab_down));
  web_view->HandleInputEvent(WebCoalescedInputEvent(tab_up));
  EXPECT_EQ(2, counter.Count());

  // Move to a number-field: 1 cursor change.
  web_view->HandleInputEvent(WebCoalescedInputEvent(tab_down));
  web_view->HandleInputEvent(WebCoalescedInputEvent(tab_up));
  EXPECT_EQ(3, counter.Count());

  // Move to an editable element: 1 cursor change.
  web_view->HandleInputEvent(WebCoalescedInputEvent(tab_down));
  web_view->HandleInputEvent(WebCoalescedInputEvent(tab_up));
  EXPECT_EQ(4, counter.Count());

  // Move to a non-editable element: 0 cursor changes.
  web_view->HandleInputEvent(WebCoalescedInputEvent(tab_down));
  web_view->HandleInputEvent(WebCoalescedInputEvent(tab_up));
  EXPECT_EQ(4, counter.Count());
}

// Implementation of WebFrameClient that tracks the v8 contexts that are created
// and destroyed for verification.
class ContextLifetimeTestWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  struct Notification {
   public:
    Notification(WebLocalFrame* frame,
                 v8::Local<v8::Context> context,
                 int world_id)
        : frame(frame),
          context(context->GetIsolate(), context),
          world_id(world_id) {}

    ~Notification() { context.Reset(); }

    bool Equals(Notification* other) {
      return other && frame == other->frame && context == other->context &&
             world_id == other->world_id;
    }

    WebLocalFrame* frame;
    v8::Persistent<v8::Context> context;
    int world_id;
  };

  ~ContextLifetimeTestWebFrameClient() override { Reset(); }

  void Reset() {
    create_notifications.clear();
    release_notifications.clear();
  }

  Vector<std::unique_ptr<Notification>> create_notifications;
  Vector<std::unique_ptr<Notification>> release_notifications;

 private:
  void DidCreateScriptContext(WebLocalFrame* frame,
                              v8::Local<v8::Context> context,
                              int world_id) override {
    create_notifications.push_back(
        WTF::MakeUnique<Notification>(frame, context, world_id));
  }

  void WillReleaseScriptContext(WebLocalFrame* frame,
                                v8::Local<v8::Context> context,
                                int world_id) override {
    release_notifications.push_back(
        WTF::MakeUnique<Notification>(frame, context, world_id));
  }
};

TEST_P(ParameterizedWebFrameTest, ContextNotificationsLoadUnload) {
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());

  RegisterMockedHttpURLLoad("context_notifications_test.html");
  RegisterMockedHttpURLLoad("context_notifications_test_frame.html");

  // Load a frame with an iframe, make sure we get the right create
  // notifications.
  ContextLifetimeTestWebFrameClient web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "context_notifications_test.html", true, &web_frame_client);

  WebFrame* main_frame = web_view_helper.WebView()->MainFrame();
  WebFrame* child_frame = main_frame->FirstChild();

  ASSERT_EQ(2u, web_frame_client.create_notifications.size());
  EXPECT_EQ(0u, web_frame_client.release_notifications.size());

  auto& first_create_notification = web_frame_client.create_notifications[0];
  auto& second_create_notification = web_frame_client.create_notifications[1];

  EXPECT_EQ(main_frame, first_create_notification->frame);
  EXPECT_EQ(main_frame->MainWorldScriptContext(),
            first_create_notification->context);
  EXPECT_EQ(0, first_create_notification->world_id);

  EXPECT_EQ(child_frame, second_create_notification->frame);
  EXPECT_EQ(child_frame->MainWorldScriptContext(),
            second_create_notification->context);
  EXPECT_EQ(0, second_create_notification->world_id);

  // Close the view. We should get two release notifications that are exactly
  // the same as the create ones, in reverse order.
  web_view_helper.Reset();

  ASSERT_EQ(2u, web_frame_client.release_notifications.size());
  auto& first_release_notification = web_frame_client.release_notifications[0];
  auto& second_release_notification = web_frame_client.release_notifications[1];

  ASSERT_TRUE(
      first_create_notification->Equals(second_release_notification.get()));
  ASSERT_TRUE(
      second_create_notification->Equals(first_release_notification.get()));
}

TEST_P(ParameterizedWebFrameTest, ContextNotificationsReload) {
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());

  RegisterMockedHttpURLLoad("context_notifications_test.html");
  RegisterMockedHttpURLLoad("context_notifications_test_frame.html");

  ContextLifetimeTestWebFrameClient web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "context_notifications_test.html", true, &web_frame_client);

  // Refresh, we should get two release notifications and two more create
  // notifications.
  FrameTestHelpers::ReloadFrame(web_view_helper.WebView()->MainFrame());
  ASSERT_EQ(4u, web_frame_client.create_notifications.size());
  ASSERT_EQ(2u, web_frame_client.release_notifications.size());

  // The two release notifications we got should be exactly the same as the
  // first two create notifications.
  for (size_t i = 0; i < web_frame_client.release_notifications.size(); ++i) {
    EXPECT_TRUE(web_frame_client.release_notifications[i]->Equals(
        web_frame_client
            .create_notifications[web_frame_client.create_notifications.size() -
                                  3 - i]
            .get()));
  }

  // The last two create notifications should be for the current frames and
  // context.
  WebFrame* main_frame = web_view_helper.WebView()->MainFrame();
  WebFrame* child_frame = main_frame->FirstChild();
  auto& first_refresh_notification = web_frame_client.create_notifications[2];
  auto& second_refresh_notification = web_frame_client.create_notifications[3];

  EXPECT_EQ(main_frame, first_refresh_notification->frame);
  EXPECT_EQ(main_frame->MainWorldScriptContext(),
            first_refresh_notification->context);
  EXPECT_EQ(0, first_refresh_notification->world_id);

  EXPECT_EQ(child_frame, second_refresh_notification->frame);
  EXPECT_EQ(child_frame->MainWorldScriptContext(),
            second_refresh_notification->context);
  EXPECT_EQ(0, second_refresh_notification->world_id);
}

TEST_P(ParameterizedWebFrameTest, ContextNotificationsIsolatedWorlds) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);

  RegisterMockedHttpURLLoad("context_notifications_test.html");
  RegisterMockedHttpURLLoad("context_notifications_test_frame.html");

  ContextLifetimeTestWebFrameClient web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "context_notifications_test.html", true, &web_frame_client);

  // Add an isolated world.
  web_frame_client.Reset();

  int isolated_world_id = 42;
  WebScriptSource script_source("hi!");
  int num_sources = 1;
  web_view_helper.WebView()->MainFrame()->ExecuteScriptInIsolatedWorld(
      isolated_world_id, &script_source, num_sources);

  // We should now have a new create notification.
  ASSERT_EQ(1u, web_frame_client.create_notifications.size());
  auto& notification = web_frame_client.create_notifications[0];
  ASSERT_EQ(isolated_world_id, notification->world_id);
  ASSERT_EQ(web_view_helper.WebView()->MainFrame(), notification->frame);

  // We don't have an API to enumarate isolated worlds for a frame, but we can
  // at least assert that the context we got is *not* the main world's context.
  ASSERT_NE(web_view_helper.WebView()->MainFrame()->MainWorldScriptContext(),
            v8::Local<v8::Context>::New(isolate, notification->context));

  web_view_helper.Reset();

  // We should have gotten three release notifications (one for each of the
  // frames, plus one for the isolated context).
  ASSERT_EQ(3u, web_frame_client.release_notifications.size());

  // And one of them should be exactly the same as the create notification for
  // the isolated context.
  int match_count = 0;
  for (size_t i = 0; i < web_frame_client.release_notifications.size(); ++i) {
    if (web_frame_client.release_notifications[i]->Equals(
            web_frame_client.create_notifications[0].get()))
      ++match_count;
  }
  EXPECT_EQ(1, match_count);
}

TEST_P(ParameterizedWebFrameTest, FindInPage) {
  RegisterMockedHttpURLLoad("find.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "find.html");
  ASSERT_TRUE(web_view_helper.WebView()->MainFrameImpl());
  WebLocalFrame* frame = web_view_helper.WebView()->MainFrameImpl();
  const int kFindIdentifier = 12345;
  WebFindOptions options;

  // Find in a <div> element.
  EXPECT_TRUE(frame->Find(kFindIdentifier, WebString::FromUTF8("bar1"), options,
                          false));
  frame->StopFinding(WebLocalFrame::kStopFindActionKeepSelection);
  WebRange range = frame->SelectionRange();
  EXPECT_EQ(5, range.StartOffset());
  EXPECT_EQ(9, range.EndOffset());
  EXPECT_TRUE(frame->GetDocument().FocusedElement().IsNull());

  // Find in an <input> value.
  EXPECT_TRUE(frame->Find(kFindIdentifier, WebString::FromUTF8("bar2"), options,
                          false));
  // Confirm stopFinding(WebLocalFrame::StopFindActionKeepSelection) sets the
  // selection on the found text.
  frame->StopFinding(WebLocalFrame::kStopFindActionKeepSelection);
  range = frame->SelectionRange();
  ASSERT_FALSE(range.IsNull());
  EXPECT_EQ(5, range.StartOffset());
  EXPECT_EQ(9, range.EndOffset());
  EXPECT_TRUE(frame->GetDocument().FocusedElement().HasHTMLTagName("input"));

  // Find in a <textarea> content.
  EXPECT_TRUE(frame->Find(kFindIdentifier, WebString::FromUTF8("bar3"), options,
                          false));
  // Confirm stopFinding(WebLocalFrame::StopFindActionKeepSelection) sets the
  // selection on the found text.
  frame->StopFinding(WebLocalFrame::kStopFindActionKeepSelection);
  range = frame->SelectionRange();
  ASSERT_FALSE(range.IsNull());
  EXPECT_EQ(5, range.StartOffset());
  EXPECT_EQ(9, range.EndOffset());
  EXPECT_TRUE(frame->GetDocument().FocusedElement().HasHTMLTagName("textarea"));

  // Find in a contentEditable element.
  EXPECT_TRUE(frame->Find(kFindIdentifier, WebString::FromUTF8("bar4"), options,
                          false));
  // Confirm stopFinding(WebLocalFrame::StopFindActionKeepSelection) sets the
  // selection on the found text.
  frame->StopFinding(WebLocalFrame::kStopFindActionKeepSelection);
  range = frame->SelectionRange();
  ASSERT_FALSE(range.IsNull());
  EXPECT_EQ(0, range.StartOffset());
  EXPECT_EQ(4, range.EndOffset());
  // "bar4" is surrounded by <span>, but the focusable node should be the parent
  // <div>.
  EXPECT_TRUE(frame->GetDocument().FocusedElement().HasHTMLTagName("div"));

  // Find in <select> content.
  EXPECT_FALSE(frame->Find(kFindIdentifier, WebString::FromUTF8("bar5"),
                           options, false));
  // If there are any matches, stopFinding will set the selection on the found
  // text.  However, we do not expect any matches, so check that the selection
  // is null.
  frame->StopFinding(WebLocalFrame::kStopFindActionKeepSelection);
  range = frame->SelectionRange();
  ASSERT_TRUE(range.IsNull());
}

TEST_P(ParameterizedWebFrameTest, GetContentAsPlainText) {
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank", true);
  // We set the size because it impacts line wrapping, which changes the
  // resulting text value.
  web_view_helper.Resize(WebSize(640, 480));
  WebFrame* frame = web_view_helper.WebView()->MainFrame();

  // Generate a simple test case.
  const char kSimpleSource[] = "<div>Foo bar</div><div></div>baz";
  KURL test_url = ToKURL("about:blank");
  FrameTestHelpers::LoadHTMLString(frame, kSimpleSource, test_url);

  // Make sure it comes out OK.
  const std::string expected("Foo bar\nbaz");
  WebString text = WebFrameContentDumper::DumpWebViewAsText(
      web_view_helper.WebView(), std::numeric_limits<size_t>::max());
  EXPECT_EQ(expected, text.Utf8());

  // Try reading the same one with clipping of the text.
  const int kLength = 5;
  text = WebFrameContentDumper::DumpWebViewAsText(web_view_helper.WebView(),
                                                  kLength);
  EXPECT_EQ(expected.substr(0, kLength), text.Utf8());

  // Now do a new test with a subframe.
  const char kOuterFrameSource[] = "Hello<iframe></iframe> world";
  FrameTestHelpers::LoadHTMLString(frame, kOuterFrameSource, test_url);

  // Load something into the subframe.
  WebFrame* subframe = frame->FirstChild();
  ASSERT_TRUE(subframe);
  FrameTestHelpers::LoadHTMLString(subframe, "sub<p>text", test_url);

  text = WebFrameContentDumper::DumpWebViewAsText(
      web_view_helper.WebView(), std::numeric_limits<size_t>::max());
  EXPECT_EQ("Hello world\n\nsub\ntext", text.Utf8());

  // Get the frame text where the subframe separator falls on the boundary of
  // what we'll take. There used to be a crash in this case.
  text =
      WebFrameContentDumper::DumpWebViewAsText(web_view_helper.WebView(), 12);
  EXPECT_EQ("Hello world", text.Utf8());
}

TEST_P(ParameterizedWebFrameTest, GetFullHtmlOfPage) {
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank", true);
  WebLocalFrame* frame = web_view_helper.WebView()->MainFrameImpl();

  // Generate a simple test case.
  const char kSimpleSource[] = "<p>Hello</p><p>World</p>";
  KURL test_url = ToKURL("about:blank");
  FrameTestHelpers::LoadHTMLString(frame, kSimpleSource, test_url);

  WebString text = WebFrameContentDumper::DumpWebViewAsText(
      web_view_helper.WebView(), std::numeric_limits<size_t>::max());
  EXPECT_EQ("Hello\n\nWorld", text.Utf8());

  const std::string html = WebFrameContentDumper::DumpAsMarkup(frame).Utf8();

  // Load again with the output html.
  FrameTestHelpers::LoadHTMLString(frame, html, test_url);

  EXPECT_EQ(html, WebFrameContentDumper::DumpAsMarkup(frame).Utf8());

  text = WebFrameContentDumper::DumpWebViewAsText(
      web_view_helper.WebView(), std::numeric_limits<size_t>::max());
  EXPECT_EQ("Hello\n\nWorld", text.Utf8());

  // Test selection check
  EXPECT_FALSE(frame->HasSelection());
  frame->ExecuteCommand(WebString::FromUTF8("SelectAll"));
  EXPECT_TRUE(frame->HasSelection());
  frame->ExecuteCommand(WebString::FromUTF8("Unselect"));
  EXPECT_FALSE(frame->HasSelection());
  WebString selection_html = frame->SelectionAsMarkup();
  EXPECT_TRUE(selection_html.IsEmpty());
}

class TestExecuteScriptDuringDidCreateScriptContext
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  void DidCreateScriptContext(WebLocalFrame* frame,
                              v8::Local<v8::Context> context,
                              int world_id) override {
    frame->ExecuteScript(WebScriptSource("window.history = 'replaced';"));
  }
};

TEST_P(ParameterizedWebFrameTest, ExecuteScriptDuringDidCreateScriptContext) {
  RegisterMockedHttpURLLoad("hello_world.html");

  TestExecuteScriptDuringDidCreateScriptContext web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "hello_world.html", true,
                                    &web_frame_client);

  FrameTestHelpers::ReloadFrame(web_view_helper.WebView()->MainFrame());
}

class FindUpdateWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
 public:
  FindUpdateWebFrameClient()
      : find_results_are_ready_(false), count_(-1), active_index_(-1) {}

  void ReportFindInPageMatchCount(int, int count, bool final_update) override {
    count_ = count;
    if (final_update)
      find_results_are_ready_ = true;
  }

  void ReportFindInPageSelection(int,
                                 int active_match_ordinal,
                                 const WebRect&) override {
    active_index_ = active_match_ordinal;
  }

  bool FindResultsAreReady() const { return find_results_are_ready_; }
  int Count() const { return count_; }
  int ActiveIndex() const { return active_index_; }

 private:
  bool find_results_are_ready_;
  int count_;
  int active_index_;
};

TEST_P(ParameterizedWebFrameTest, FindInPageMatchRects) {
  RegisterMockedHttpURLLoad("find_in_page_frame.html");

  FindUpdateWebFrameClient client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "find_in_page_frame.html", true,
                                    &client);
  web_view_helper.Resize(WebSize(640, 480));
  web_view_helper.WebView()->SetMaximumLegibleScale(1.f);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();
  RunPendingTasks();

  // Note that the 'result 19' in the <select> element is not expected to
  // produce a match. Also, results 00 and 01 are in a different frame that is
  // not included in this test.
  const char kFindString[] = "result";
  const int kFindIdentifier = 12345;
  const int kNumResults = 17;

  WebFindOptions options;
  WebString search_text = WebString::FromUTF8(kFindString);
  WebLocalFrameImpl* main_frame = web_view_helper.WebView()->MainFrameImpl();
  EXPECT_TRUE(main_frame->Find(kFindIdentifier, search_text, options, false));

  main_frame->EnsureTextFinder().ResetMatchCount();

  for (WebLocalFrameImpl* frame = main_frame; frame;
       frame = static_cast<WebLocalFrameImpl*>(frame->TraverseNext())) {
    frame->EnsureTextFinder().StartScopingStringMatches(kFindIdentifier,
                                                        search_text, options);
  }

  RunPendingTasks();
  EXPECT_TRUE(client.FindResultsAreReady());

  WebVector<WebFloatRect> web_match_rects;
  main_frame->FindMatchRects(web_match_rects);
  ASSERT_EQ(static_cast<size_t>(kNumResults), web_match_rects.size());
  int rects_version = main_frame->FindMatchMarkersVersion();

  for (int result_index = 0; result_index < kNumResults; ++result_index) {
    FloatRect result_rect =
        static_cast<FloatRect>(web_match_rects[result_index]);

    // Select the match by the center of its rect.
    EXPECT_EQ(main_frame->SelectNearestFindMatch(result_rect.Center(), 0),
              result_index + 1);

    // Check that the find result ordering matches with our expectations.
    Range* result = main_frame->GetTextFinder()->ActiveMatch();
    ASSERT_TRUE(result);
    result->setEnd(result->endContainer(), result->endOffset() + 3);
    EXPECT_EQ(result->GetText(),
              String::Format("%s %02d", kFindString, result_index + 2));

    // Verify that the expected match rect also matches the currently active
    // match.  Compare the enclosing rects to prevent precision issues caused by
    // CSS transforms.
    FloatRect active_match = main_frame->ActiveFindMatchRect();
    EXPECT_EQ(EnclosingIntRect(active_match), EnclosingIntRect(result_rect));

    // The rects version should not have changed.
    EXPECT_EQ(main_frame->FindMatchMarkersVersion(), rects_version);
  }

  // Resizing should update the rects version.
  web_view_helper.Resize(WebSize(800, 600));
  RunPendingTasks();
  EXPECT_TRUE(main_frame->FindMatchMarkersVersion() != rects_version);
}

TEST_F(WebFrameTest, FindInPageActiveIndex) {
  RegisterMockedHttpURLLoad("find_match_count.html");

  FindUpdateWebFrameClient client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "find_match_count.html", true,
                                    &client);
  web_view_helper.WebView()->Resize(WebSize(640, 480));
  RunPendingTasks();

  const char* kFindString = "a";
  const int kFindIdentifier = 7777;
  const int kActiveIndex = 1;

  WebFindOptions options;
  WebString search_text = WebString::FromUTF8(kFindString);
  WebLocalFrameImpl* main_frame = web_view_helper.WebView()->MainFrameImpl();
  EXPECT_TRUE(main_frame->Find(kFindIdentifier, search_text, options, false));
  main_frame->EnsureTextFinder().ResetMatchCount();

  for (WebLocalFrameImpl* frame = main_frame; frame;
       frame = static_cast<WebLocalFrameImpl*>(frame->TraverseNext())) {
    frame->EnsureTextFinder().StartScopingStringMatches(kFindIdentifier,
                                                        search_text, options);
  }

  RunPendingTasks();
  EXPECT_TRUE(main_frame->Find(kFindIdentifier, search_text, options, false));
  main_frame->StopFinding(WebLocalFrame::kStopFindActionClearSelection);

  for (WebLocalFrameImpl* frame = main_frame; frame;
       frame = static_cast<WebLocalFrameImpl*>(frame->TraverseNext())) {
    frame->EnsureTextFinder().StartScopingStringMatches(kFindIdentifier,
                                                        search_text, options);
  }

  RunPendingTasks();
  EXPECT_TRUE(client.FindResultsAreReady());
  EXPECT_EQ(kActiveIndex, client.ActiveIndex());

  const char* kFindStringNew = "e";
  WebString search_text_new = WebString::FromUTF8(kFindStringNew);

  EXPECT_TRUE(
      main_frame->Find(kFindIdentifier, search_text_new, options, false));
  main_frame->EnsureTextFinder().ResetMatchCount();

  for (WebLocalFrameImpl* frame = main_frame; frame;
       frame = static_cast<WebLocalFrameImpl*>(frame->TraverseNext())) {
    frame->EnsureTextFinder().StartScopingStringMatches(
        kFindIdentifier, search_text_new, options);
  }

  RunPendingTasks();
  EXPECT_TRUE(client.FindResultsAreReady());
  EXPECT_EQ(kActiveIndex, client.ActiveIndex());
}

TEST_P(ParameterizedWebFrameTest, FindOnDetachedFrame) {
  RegisterMockedHttpURLLoad("find_in_page.html");
  RegisterMockedHttpURLLoad("find_in_page_frame.html");

  FindUpdateWebFrameClient client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "find_in_page.html", true,
                                    &client);
  web_view_helper.Resize(WebSize(640, 480));
  RunPendingTasks();

  const char kFindString[] = "result";
  const int kFindIdentifier = 12345;

  WebFindOptions options;
  WebString search_text = WebString::FromUTF8(kFindString);
  WebLocalFrameImpl* main_frame = web_view_helper.WebView()->MainFrameImpl();
  WebLocalFrameImpl* second_frame =
      ToWebLocalFrameImpl(main_frame->TraverseNext());

  // Detach the frame before finding.
  RemoveElementById(main_frame, "frame");

  EXPECT_TRUE(main_frame->Find(kFindIdentifier, search_text, options, false));
  EXPECT_FALSE(
      second_frame->Find(kFindIdentifier, search_text, options, false));

  RunPendingTasks();
  EXPECT_FALSE(client.FindResultsAreReady());

  main_frame->EnsureTextFinder().ResetMatchCount();

  for (WebLocalFrameImpl* frame = main_frame; frame;
       frame = static_cast<WebLocalFrameImpl*>(frame->TraverseNext())) {
    frame->EnsureTextFinder().StartScopingStringMatches(kFindIdentifier,
                                                        search_text, options);
  }

  RunPendingTasks();
  EXPECT_TRUE(client.FindResultsAreReady());
}

TEST_P(ParameterizedWebFrameTest, FindDetachFrameBeforeScopeStrings) {
  RegisterMockedHttpURLLoad("find_in_page.html");
  RegisterMockedHttpURLLoad("find_in_page_frame.html");

  FindUpdateWebFrameClient client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "find_in_page.html", true,
                                    &client);
  web_view_helper.Resize(WebSize(640, 480));
  RunPendingTasks();

  const char kFindString[] = "result";
  const int kFindIdentifier = 12345;

  WebFindOptions options;
  WebString search_text = WebString::FromUTF8(kFindString);
  WebLocalFrameImpl* main_frame = web_view_helper.WebView()->MainFrameImpl();

  for (WebFrame* frame = main_frame; frame; frame = frame->TraverseNext())
    EXPECT_TRUE(frame->ToWebLocalFrame()->Find(kFindIdentifier, search_text,
                                               options, false));

  RunPendingTasks();
  EXPECT_FALSE(client.FindResultsAreReady());

  // Detach the frame between finding and scoping.
  RemoveElementById(main_frame, "frame");

  main_frame->EnsureTextFinder().ResetMatchCount();

  for (WebLocalFrameImpl* frame = main_frame; frame;
       frame = static_cast<WebLocalFrameImpl*>(frame->TraverseNext())) {
    frame->EnsureTextFinder().StartScopingStringMatches(kFindIdentifier,
                                                        search_text, options);
  }

  RunPendingTasks();
  EXPECT_TRUE(client.FindResultsAreReady());
}

TEST_P(ParameterizedWebFrameTest, FindDetachFrameWhileScopingStrings) {
  RegisterMockedHttpURLLoad("find_in_page.html");
  RegisterMockedHttpURLLoad("find_in_page_frame.html");

  FindUpdateWebFrameClient client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "find_in_page.html", true,
                                    &client);
  web_view_helper.Resize(WebSize(640, 480));
  RunPendingTasks();

  const char kFindString[] = "result";
  const int kFindIdentifier = 12345;

  WebFindOptions options;
  WebString search_text = WebString::FromUTF8(kFindString);
  WebLocalFrameImpl* main_frame = web_view_helper.WebView()->MainFrameImpl();

  for (WebFrame* frame = main_frame; frame; frame = frame->TraverseNext())
    EXPECT_TRUE(frame->ToWebLocalFrame()->Find(kFindIdentifier, search_text,
                                               options, false));

  RunPendingTasks();
  EXPECT_FALSE(client.FindResultsAreReady());

  main_frame->EnsureTextFinder().ResetMatchCount();

  for (WebLocalFrameImpl* frame = main_frame; frame;
       frame = static_cast<WebLocalFrameImpl*>(frame->TraverseNext())) {
    frame->EnsureTextFinder().StartScopingStringMatches(kFindIdentifier,
                                                        search_text, options);
  }

  // The first startScopingStringMatches will have reset the state. Detach
  // before it actually scopes.
  RemoveElementById(main_frame, "frame");

  RunPendingTasks();
  EXPECT_TRUE(client.FindResultsAreReady());
}

TEST_P(ParameterizedWebFrameTest, ResetMatchCount) {
  RegisterMockedHttpURLLoad("find_in_generated_frame.html");

  FindUpdateWebFrameClient client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "find_in_generated_frame.html",
                                    true, &client);
  web_view_helper.Resize(WebSize(640, 480));
  RunPendingTasks();

  const char kFindString[] = "result";
  const int kFindIdentifier = 12345;

  WebFindOptions options;
  WebString search_text = WebString::FromUTF8(kFindString);
  WebLocalFrameImpl* main_frame = web_view_helper.WebView()->MainFrameImpl();

  // Check that child frame exists.
  EXPECT_TRUE(!!main_frame->TraverseNext());

  for (WebFrame* frame = main_frame; frame; frame = frame->TraverseNext())
    EXPECT_FALSE(frame->ToWebLocalFrame()->Find(kFindIdentifier, search_text,
                                                options, false));

  RunPendingTasks();
  EXPECT_FALSE(client.FindResultsAreReady());

  main_frame->EnsureTextFinder().ResetMatchCount();
}

TEST_P(ParameterizedWebFrameTest, SetTickmarks) {
  RegisterMockedHttpURLLoad("find.html");

  FindUpdateWebFrameClient client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "find.html", true, &client);
  web_view_helper.Resize(WebSize(640, 480));
  RunPendingTasks();

  const char kFindString[] = "foo";
  const int kFindIdentifier = 12345;

  WebFindOptions options;
  WebString search_text = WebString::FromUTF8(kFindString);
  WebLocalFrameImpl* main_frame = web_view_helper.WebView()->MainFrameImpl();
  EXPECT_TRUE(main_frame->Find(kFindIdentifier, search_text, options, false));

  main_frame->EnsureTextFinder().ResetMatchCount();
  main_frame->EnsureTextFinder().StartScopingStringMatches(
      kFindIdentifier, search_text, options);

  RunPendingTasks();
  EXPECT_TRUE(client.FindResultsAreReady());

  // Get the tickmarks for the original find request.
  FrameView* frame_view =
      web_view_helper.WebView()->MainFrameImpl()->GetFrameView();
  Scrollbar* scrollbar = frame_view->CreateScrollbar(kHorizontalScrollbar);
  Vector<IntRect> original_tickmarks;
  scrollbar->GetTickmarks(original_tickmarks);
  EXPECT_EQ(4u, original_tickmarks.size());

  // Override the tickmarks.
  Vector<IntRect> overriding_tickmarks_expected;
  overriding_tickmarks_expected.push_back(IntRect(0, 0, 100, 100));
  overriding_tickmarks_expected.push_back(IntRect(0, 20, 100, 100));
  overriding_tickmarks_expected.push_back(IntRect(0, 30, 100, 100));
  main_frame->SetTickmarks(overriding_tickmarks_expected);

  // Check the tickmarks are overriden correctly.
  Vector<IntRect> overriding_tickmarks_actual;
  scrollbar->GetTickmarks(overriding_tickmarks_actual);
  EXPECT_EQ(overriding_tickmarks_expected, overriding_tickmarks_actual);

  // Reset the tickmark behavior.
  Vector<IntRect> reset_tickmarks;
  main_frame->SetTickmarks(reset_tickmarks);

  // Check that the original tickmarks are returned
  Vector<IntRect> original_tickmarks_after_reset;
  scrollbar->GetTickmarks(original_tickmarks_after_reset);
  EXPECT_EQ(original_tickmarks, original_tickmarks_after_reset);
}

TEST_P(ParameterizedWebFrameTest, FindInPageJavaScriptUpdatesDOM) {
  RegisterMockedHttpURLLoad("find.html");

  FindUpdateWebFrameClient client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "find.html", true, &client);
  web_view_helper.Resize(WebSize(640, 480));
  RunPendingTasks();

  WebLocalFrameImpl* frame = web_view_helper.WebView()->MainFrameImpl();
  const int kFindIdentifier = 12345;
  static const char* kFindString = "foo";
  WebString search_text = WebString::FromUTF8(kFindString);
  WebFindOptions options;
  bool active_now;

  frame->EnsureTextFinder().ResetMatchCount();
  frame->EnsureTextFinder().StartScopingStringMatches(kFindIdentifier,
                                                      search_text, options);
  RunPendingTasks();
  EXPECT_TRUE(client.FindResultsAreReady());

  // Find in a <div> element.
  options.find_next = true;
  EXPECT_TRUE(
      frame->Find(kFindIdentifier, search_text, options, false, &active_now));
  EXPECT_TRUE(active_now);

  // Insert new text, which contains occurence of |searchText|.
  frame->ExecuteScript(WebScriptSource(
      "var newTextNode = document.createTextNode('bar5 foo5');"
      "var textArea = document.getElementsByTagName('textarea')[0];"
      "document.body.insertBefore(newTextNode, textArea);"));

  // Find in a <input> element.
  EXPECT_TRUE(
      frame->Find(kFindIdentifier, search_text, options, false, &active_now));
  EXPECT_TRUE(active_now);

  // Find in the inserted text node.
  EXPECT_TRUE(
      frame->Find(kFindIdentifier, search_text, options, false, &active_now));
  frame->StopFinding(WebLocalFrame::kStopFindActionKeepSelection);
  WebRange range = frame->SelectionRange();
  EXPECT_EQ(5, range.StartOffset());
  EXPECT_EQ(8, range.EndOffset());
  EXPECT_TRUE(frame->GetDocument().FocusedElement().IsNull());
  EXPECT_FALSE(active_now);
}

struct FakeTimerSetter {
  FakeTimerSetter() {
    time_elapsed_ = 0.0;
    original_time_function_ = SetTimeFunctionsForTesting(ReturnMockTime);
  }

  ~FakeTimerSetter() { SetTimeFunctionsForTesting(original_time_function_); }
  static double ReturnMockTime() {
    time_elapsed_ += 1.0;
    return time_elapsed_;
  }

 private:
  TimeFunction original_time_function_;
  static double time_elapsed_;
};
double FakeTimerSetter::time_elapsed_ = 0.;

TEST_P(ParameterizedWebFrameTest, FindInPageJavaScriptUpdatesDOMProperOrdinal) {
  FakeTimerSetter fake_timer;

  const WebString search_pattern = WebString::FromUTF8("abc");
  // We have 2 occurrences of the pattern in our text.
  const char* html =
      "foo bar foo bar foo abc bar foo bar foo bar foo bar foo bar foo bar foo "
      "bar foo bar foo bar foo bar foo bar foo bar foo bar foo bar foo bar foo "
      "bar foo bar foo abc bar <div id='new_text'></div>";

  FindUpdateWebFrameClient client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true, &client);

  WebLocalFrameImpl* frame = web_view_helper.WebView()->MainFrameImpl();
  FrameTestHelpers::LoadHTMLString(frame, html,
                                   URLTestHelpers::ToKURL(base_url_));
  web_view_helper.Resize(WebSize(640, 480));
  web_view_helper.WebView()->SetFocus(true);
  RunPendingTasks();

  const int kFindIdentifier = 12345;
  WebFindOptions options;

  // The first search that will start the scoping process.
  frame->RequestFind(kFindIdentifier, search_pattern, options);
  EXPECT_FALSE(client.FindResultsAreReady());
  EXPECT_EQ(1, client.Count());
  EXPECT_TRUE(frame->EnsureTextFinder().ScopingInProgress());

  // The scoping won't find all the entries on the first run due to the fake
  // timer.
  while (frame->EnsureTextFinder().ScopingInProgress())
    RunPendingTasks();

  EXPECT_EQ(2, client.Count());
  EXPECT_EQ(1, client.ActiveIndex());

  options.find_next = true;

  // The second search will jump to the next match without any scoping.
  frame->RequestFind(kFindIdentifier, search_pattern, options);

  EXPECT_EQ(2, client.Count());
  EXPECT_EQ(2, client.ActiveIndex());
  EXPECT_FALSE(frame->EnsureTextFinder().ScopingInProgress());

  // Insert new text, which contains occurence of |searchText|.
  frame->ExecuteScript(
      WebScriptSource("var textDiv = document.getElementById('new_text');"
                      "textDiv.innerHTML = 'foo abc';"));

  // The third search will find a new match and initiate a new scoping.
  frame->RequestFind(kFindIdentifier, search_pattern, options);

  EXPECT_TRUE(frame->EnsureTextFinder().ScopingInProgress());

  while (frame->EnsureTextFinder().ScopingInProgress())
    RunPendingTasks();

  EXPECT_EQ(3, client.Count());
  EXPECT_EQ(3, client.ActiveIndex());
}

static WebPoint TopLeft(const WebRect& rect) {
  return WebPoint(rect.x, rect.y);
}

static WebPoint BottomRightMinusOne(const WebRect& rect) {
  // FIXME: If we don't subtract 1 from the x- and y-coordinates of the
  // selection bounds, selectRange() will select the *next* element. That's
  // strictly correct, as hit-testing checks the pixel to the lower-right of
  // the input coordinate, but it's a wart on the API.
  return WebPoint(rect.x + rect.width - 1, rect.y + rect.height - 1);
}

static WebRect ElementBounds(WebFrame* frame, const WebString& id) {
  return frame->GetDocument().GetElementById(id).BoundsInViewport();
}

static std::string SelectionAsString(WebFrame* frame) {
  return frame->ToWebLocalFrame()->SelectionAsText().Utf8();
}

TEST_P(ParameterizedWebFrameTest, SelectRange) {
  WebLocalFrame* frame;
  WebRect start_web_rect;
  WebRect end_web_rect;

  RegisterMockedHttpURLLoad("select_range_basic.html");
  RegisterMockedHttpURLLoad("select_range_scroll.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "select_range_basic.html",
                                 &web_view_helper);
  frame = web_view_helper.WebView()->MainFrameImpl();
  EXPECT_EQ("Some test text for testing.", SelectionAsString(frame));
  web_view_helper.WebView()->SelectionBounds(start_web_rect, end_web_rect);
  frame->ExecuteCommand(WebString::FromUTF8("Unselect"));
  EXPECT_EQ("", SelectionAsString(frame));
  frame->SelectRange(TopLeft(start_web_rect),
                     BottomRightMinusOne(end_web_rect));
  // On some devices, the above bottomRightMinusOne() causes the ending '.' not
  // selected.
  std::string selection_string = SelectionAsString(frame);
  EXPECT_TRUE(selection_string == "Some test text for testing." ||
              selection_string == "Some test text for testing");

  InitializeTextSelectionWebView(base_url_ + "select_range_scroll.html",
                                 &web_view_helper);
  frame = web_view_helper.WebView()->MainFrameImpl();
  EXPECT_EQ("Some offscreen test text for testing.", SelectionAsString(frame));
  web_view_helper.WebView()->SelectionBounds(start_web_rect, end_web_rect);
  frame->ExecuteCommand(WebString::FromUTF8("Unselect"));
  EXPECT_EQ("", SelectionAsString(frame));
  frame->SelectRange(TopLeft(start_web_rect),
                     BottomRightMinusOne(end_web_rect));
  // On some devices, the above bottomRightMinusOne() causes the ending '.' not
  // selected.
  selection_string = SelectionAsString(frame);
  EXPECT_TRUE(selection_string == "Some offscreen test text for testing." ||
              selection_string == "Some offscreen test text for testing");
}

TEST_P(ParameterizedWebFrameTest, SelectRangeInIframe) {
  WebFrame* frame;
  WebRect start_web_rect;
  WebRect end_web_rect;

  RegisterMockedHttpURLLoad("select_range_iframe.html");
  RegisterMockedHttpURLLoad("select_range_basic.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "select_range_iframe.html",
                                 &web_view_helper);
  frame = web_view_helper.WebView()->MainFrame();
  WebLocalFrame* subframe = frame->FirstChild()->ToWebLocalFrame();
  EXPECT_EQ("Some test text for testing.", SelectionAsString(subframe));
  web_view_helper.WebView()->SelectionBounds(start_web_rect, end_web_rect);
  subframe->ExecuteCommand(WebString::FromUTF8("Unselect"));
  EXPECT_EQ("", SelectionAsString(subframe));
  subframe->SelectRange(TopLeft(start_web_rect),
                        BottomRightMinusOne(end_web_rect));
  // On some devices, the above bottomRightMinusOne() causes the ending '.' not
  // selected.
  std::string selection_string = SelectionAsString(subframe);
  EXPECT_TRUE(selection_string == "Some test text for testing." ||
              selection_string == "Some test text for testing");
}

TEST_P(ParameterizedWebFrameTest, SelectRangeDivContentEditable) {
  WebLocalFrame* frame;
  WebRect start_web_rect;
  WebRect end_web_rect;

  RegisterMockedHttpURLLoad("select_range_div_editable.html");

  // Select the middle of an editable element, then try to extend the selection
  // to the top of the document.  The selection range should be clipped to the
  // bounds of the editable element.
  FrameTestHelpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "select_range_div_editable.html",
                                 &web_view_helper);
  frame = web_view_helper.WebView()->MainFrameImpl();
  EXPECT_EQ("This text is initially selected.", SelectionAsString(frame));
  web_view_helper.WebView()->SelectionBounds(start_web_rect, end_web_rect);

  frame->SelectRange(BottomRightMinusOne(end_web_rect), WebPoint(0, 0));
  EXPECT_EQ("16-char header. This text is initially selected.",
            SelectionAsString(frame));

  // As above, but extending the selection to the bottom of the document.
  InitializeTextSelectionWebView(base_url_ + "select_range_div_editable.html",
                                 &web_view_helper);
  frame = web_view_helper.WebView()->MainFrameImpl();

  web_view_helper.WebView()->SelectionBounds(start_web_rect, end_web_rect);
  frame->SelectRange(TopLeft(start_web_rect),
                     BottomRightMinusOne(end_web_rect));
  EXPECT_EQ("This text is initially selected.", SelectionAsString(frame));
  web_view_helper.WebView()->SelectionBounds(start_web_rect, end_web_rect);

  web_view_helper.WebView()->SelectionBounds(start_web_rect, end_web_rect);
  frame->SelectRange(TopLeft(start_web_rect), WebPoint(640, 480));
  EXPECT_EQ("This text is initially selected. 16-char footer.",
            SelectionAsString(frame));
}

// positionForPoint returns the wrong values for contenteditable spans. See
// http://crbug.com/238334.
TEST_P(ParameterizedWebFrameTest, DISABLED_SelectRangeSpanContentEditable) {
  WebLocalFrame* frame;
  WebRect start_web_rect;
  WebRect end_web_rect;

  RegisterMockedHttpURLLoad("select_range_span_editable.html");

  // Select the middle of an editable element, then try to extend the selection
  // to the top of the document.
  // The selection range should be clipped to the bounds of the editable
  // element.
  FrameTestHelpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "select_range_span_editable.html",
                                 &web_view_helper);
  frame = web_view_helper.WebView()->MainFrameImpl();
  EXPECT_EQ("This text is initially selected.", SelectionAsString(frame));
  web_view_helper.WebView()->SelectionBounds(start_web_rect, end_web_rect);

  frame->SelectRange(BottomRightMinusOne(end_web_rect), WebPoint(0, 0));
  EXPECT_EQ("16-char header. This text is initially selected.",
            SelectionAsString(frame));

  // As above, but extending the selection to the bottom of the document.
  InitializeTextSelectionWebView(base_url_ + "select_range_span_editable.html",
                                 &web_view_helper);
  frame = web_view_helper.WebView()->MainFrameImpl();

  web_view_helper.WebView()->SelectionBounds(start_web_rect, end_web_rect);
  frame->SelectRange(TopLeft(start_web_rect),
                     BottomRightMinusOne(end_web_rect));
  EXPECT_EQ("This text is initially selected.", SelectionAsString(frame));
  web_view_helper.WebView()->SelectionBounds(start_web_rect, end_web_rect);

  EXPECT_EQ("This text is initially selected.", SelectionAsString(frame));
  web_view_helper.WebView()->SelectionBounds(start_web_rect, end_web_rect);
  frame->SelectRange(TopLeft(start_web_rect), WebPoint(640, 480));
  EXPECT_EQ("This text is initially selected. 16-char footer.",
            SelectionAsString(frame));
}

TEST_P(ParameterizedWebFrameTest, SelectRangeCanMoveSelectionStart) {
  RegisterMockedHttpURLLoad("text_selection.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "text_selection.html",
                                 &web_view_helper);
  WebLocalFrame* frame = web_view_helper.WebView()->MainFrameImpl();

  // Select second span. We can move the start to include the first span.
  frame->ExecuteScript(WebScriptSource("selectElement('header_2');"));
  EXPECT_EQ("Header 2.", SelectionAsString(frame));
  frame->SelectRange(BottomRightMinusOne(ElementBounds(frame, "header_2")),
                     TopLeft(ElementBounds(frame, "header_1")));
  EXPECT_EQ("Header 1. Header 2.", SelectionAsString(frame));

  // We can move the start and end together.
  frame->ExecuteScript(WebScriptSource("selectElement('header_1');"));
  EXPECT_EQ("Header 1.", SelectionAsString(frame));
  frame->SelectRange(BottomRightMinusOne(ElementBounds(frame, "header_1")),
                     BottomRightMinusOne(ElementBounds(frame, "header_1")));
  EXPECT_EQ("", SelectionAsString(frame));
  // Selection is a caret, not empty.
  EXPECT_FALSE(frame->SelectionRange().IsNull());

  // We can move the start across the end.
  frame->ExecuteScript(WebScriptSource("selectElement('header_1');"));
  EXPECT_EQ("Header 1.", SelectionAsString(frame));
  frame->SelectRange(BottomRightMinusOne(ElementBounds(frame, "header_1")),
                     BottomRightMinusOne(ElementBounds(frame, "header_2")));
  EXPECT_EQ(" Header 2.", SelectionAsString(frame));

  // Can't extend the selection part-way into an editable element.
  frame->ExecuteScript(WebScriptSource("selectElement('footer_2');"));
  EXPECT_EQ("Footer 2.", SelectionAsString(frame));
  frame->SelectRange(BottomRightMinusOne(ElementBounds(frame, "footer_2")),
                     TopLeft(ElementBounds(frame, "editable_2")));
  EXPECT_EQ(" [ Footer 1. Footer 2.", SelectionAsString(frame));

  // Can extend the selection completely across editable elements.
  frame->ExecuteScript(WebScriptSource("selectElement('footer_2');"));
  EXPECT_EQ("Footer 2.", SelectionAsString(frame));
  frame->SelectRange(BottomRightMinusOne(ElementBounds(frame, "footer_2")),
                     TopLeft(ElementBounds(frame, "header_2")));
  EXPECT_EQ("Header 2. ] [ Editable 1. Editable 2. ] [ Footer 1. Footer 2.",
            SelectionAsString(frame));

  // If the selection is editable text, we can't extend it into non-editable
  // text.
  frame->ExecuteScript(WebScriptSource("selectElement('editable_2');"));
  EXPECT_EQ("Editable 2.", SelectionAsString(frame));
  frame->SelectRange(BottomRightMinusOne(ElementBounds(frame, "editable_2")),
                     TopLeft(ElementBounds(frame, "header_2")));
  // positionForPoint returns the wrong values for contenteditable spans. See
  // http://crbug.com/238334.
  // EXPECT_EQ("[ Editable 1. Editable 2.", selectionAsString(frame));
}

TEST_P(ParameterizedWebFrameTest, SelectRangeCanMoveSelectionEnd) {
  RegisterMockedHttpURLLoad("text_selection.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "text_selection.html",
                                 &web_view_helper);
  WebLocalFrame* frame = web_view_helper.WebView()->MainFrameImpl();

  // Select first span. We can move the end to include the second span.
  frame->ExecuteScript(WebScriptSource("selectElement('header_1');"));
  EXPECT_EQ("Header 1.", SelectionAsString(frame));
  frame->SelectRange(TopLeft(ElementBounds(frame, "header_1")),
                     BottomRightMinusOne(ElementBounds(frame, "header_2")));
  EXPECT_EQ("Header 1. Header 2.", SelectionAsString(frame));

  // We can move the start and end together.
  frame->ExecuteScript(WebScriptSource("selectElement('header_2');"));
  EXPECT_EQ("Header 2.", SelectionAsString(frame));
  frame->SelectRange(TopLeft(ElementBounds(frame, "header_2")),
                     TopLeft(ElementBounds(frame, "header_2")));
  EXPECT_EQ("", SelectionAsString(frame));
  // Selection is a caret, not empty.
  EXPECT_FALSE(frame->SelectionRange().IsNull());

  // We can move the end across the start.
  frame->ExecuteScript(WebScriptSource("selectElement('header_2');"));
  EXPECT_EQ("Header 2.", SelectionAsString(frame));
  frame->SelectRange(TopLeft(ElementBounds(frame, "header_2")),
                     TopLeft(ElementBounds(frame, "header_1")));
  EXPECT_EQ("Header 1. ", SelectionAsString(frame));

  // Can't extend the selection part-way into an editable element.
  frame->ExecuteScript(WebScriptSource("selectElement('header_1');"));
  EXPECT_EQ("Header 1.", SelectionAsString(frame));
  frame->SelectRange(TopLeft(ElementBounds(frame, "header_1")),
                     BottomRightMinusOne(ElementBounds(frame, "editable_1")));
  EXPECT_EQ("Header 1. Header 2. ] ", SelectionAsString(frame));

  // Can extend the selection completely across editable elements.
  frame->ExecuteScript(WebScriptSource("selectElement('header_1');"));
  EXPECT_EQ("Header 1.", SelectionAsString(frame));
  frame->SelectRange(TopLeft(ElementBounds(frame, "header_1")),
                     BottomRightMinusOne(ElementBounds(frame, "footer_1")));
  EXPECT_EQ("Header 1. Header 2. ] [ Editable 1. Editable 2. ] [ Footer 1.",
            SelectionAsString(frame));

  // If the selection is editable text, we can't extend it into non-editable
  // text.
  frame->ExecuteScript(WebScriptSource("selectElement('editable_1');"));
  EXPECT_EQ("Editable 1.", SelectionAsString(frame));
  frame->SelectRange(TopLeft(ElementBounds(frame, "editable_1")),
                     BottomRightMinusOne(ElementBounds(frame, "footer_1")));
  // positionForPoint returns the wrong values for contenteditable spans. See
  // http://crbug.com/238334.
  // EXPECT_EQ("Editable 1. Editable 2. ]", selectionAsString(frame));
}

TEST_P(ParameterizedWebFrameTest, MoveRangeSelectionExtent) {
  WebLocalFrameImpl* frame;
  WebRect start_web_rect;
  WebRect end_web_rect;

  RegisterMockedHttpURLLoad("move_range_selection_extent.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "move_range_selection_extent.html",
                                 &web_view_helper);
  frame = web_view_helper.WebView()->MainFrameImpl();
  EXPECT_EQ("This text is initially selected.", SelectionAsString(frame));
  web_view_helper.WebView()->SelectionBounds(start_web_rect, end_web_rect);

  frame->MoveRangeSelectionExtent(WebPoint(640, 480));
  EXPECT_EQ("This text is initially selected. 16-char footer.",
            SelectionAsString(frame));

  frame->MoveRangeSelectionExtent(WebPoint(0, 0));
  EXPECT_EQ("16-char header. ", SelectionAsString(frame));

  // Reset with swapped base and extent.
  frame->SelectRange(TopLeft(end_web_rect),
                     BottomRightMinusOne(start_web_rect));
  EXPECT_EQ("This text is initially selected.", SelectionAsString(frame));

  frame->MoveRangeSelectionExtent(WebPoint(640, 480));
  EXPECT_EQ(" 16-char footer.", SelectionAsString(frame));

  frame->MoveRangeSelectionExtent(WebPoint(0, 0));
  EXPECT_EQ("16-char header. This text is initially selected.",
            SelectionAsString(frame));

  frame->ExecuteCommand(WebString::FromUTF8("Unselect"));
  EXPECT_EQ("", SelectionAsString(frame));
}

TEST_P(ParameterizedWebFrameTest, MoveRangeSelectionExtentCannotCollapse) {
  WebLocalFrameImpl* frame;
  WebRect start_web_rect;
  WebRect end_web_rect;

  RegisterMockedHttpURLLoad("move_range_selection_extent.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "move_range_selection_extent.html",
                                 &web_view_helper);
  frame = web_view_helper.WebView()->MainFrameImpl();
  EXPECT_EQ("This text is initially selected.", SelectionAsString(frame));
  web_view_helper.WebView()->SelectionBounds(start_web_rect, end_web_rect);

  frame->MoveRangeSelectionExtent(BottomRightMinusOne(start_web_rect));
  EXPECT_EQ("This text is initially selected.", SelectionAsString(frame));

  // Reset with swapped base and extent.
  frame->SelectRange(TopLeft(end_web_rect),
                     BottomRightMinusOne(start_web_rect));
  EXPECT_EQ("This text is initially selected.", SelectionAsString(frame));

  frame->MoveRangeSelectionExtent(BottomRightMinusOne(end_web_rect));
  EXPECT_EQ("This text is initially selected.", SelectionAsString(frame));
}

TEST_P(ParameterizedWebFrameTest, MoveRangeSelectionExtentScollsInputField) {
  WebLocalFrameImpl* frame;
  WebRect start_web_rect;
  WebRect end_web_rect;

  RegisterMockedHttpURLLoad("move_range_selection_extent_input_field.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(
      base_url_ + "move_range_selection_extent_input_field.html",
      &web_view_helper);
  frame = web_view_helper.WebView()->MainFrameImpl();
  EXPECT_EQ("Length", SelectionAsString(frame));
  web_view_helper.WebView()->SelectionBounds(start_web_rect, end_web_rect);

  EXPECT_EQ(0, frame->GetFrame()
                   ->Selection()
                   .ComputeVisibleSelectionInDOMTreeDeprecated()
                   .RootEditableElement()
                   ->scrollLeft());
  frame->MoveRangeSelectionExtent(
      WebPoint(end_web_rect.x + 500, end_web_rect.y));
  EXPECT_GE(frame->GetFrame()
                ->Selection()
                .ComputeVisibleSelectionInDOMTreeDeprecated()
                .RootEditableElement()
                ->scrollLeft(),
            1);
  EXPECT_EQ("Lengthy text goes here.", SelectionAsString(frame));
}

static int ComputeOffset(LayoutObject* layout_object, int x, int y) {
  return CreateVisiblePosition(
             layout_object->PositionForPoint(LayoutPoint(x, y)))
      .DeepEquivalent()
      .ComputeOffsetInContainerNode();
}

// positionForPoint returns the wrong values for contenteditable spans. See
// http://crbug.com/238334.
TEST_P(ParameterizedWebFrameTest, DISABLED_PositionForPointTest) {
  RegisterMockedHttpURLLoad("select_range_span_editable.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "select_range_span_editable.html",
                                 &web_view_helper);
  WebLocalFrameImpl* main_frame = web_view_helper.WebView()->MainFrameImpl();
  LayoutObject* layout_object =
      main_frame->GetFrame()
          ->Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated()
          .RootEditableElement()
          ->GetLayoutObject();
  EXPECT_EQ(0, ComputeOffset(layout_object, -1, -1));
  EXPECT_EQ(64, ComputeOffset(layout_object, 1000, 1000));

  RegisterMockedHttpURLLoad("select_range_div_editable.html");
  InitializeTextSelectionWebView(base_url_ + "select_range_div_editable.html",
                                 &web_view_helper);
  main_frame = web_view_helper.WebView()->MainFrameImpl();
  layout_object = main_frame->GetFrame()
                      ->Selection()
                      .ComputeVisibleSelectionInDOMTreeDeprecated()
                      .RootEditableElement()
                      ->GetLayoutObject();
  EXPECT_EQ(0, ComputeOffset(layout_object, -1, -1));
  EXPECT_EQ(64, ComputeOffset(layout_object, 1000, 1000));
}

#if !OS(MACOSX) && !OS(LINUX)
TEST_P(ParameterizedWebFrameTest,
       SelectRangeStaysHorizontallyAlignedWhenMoved) {
  RegisterMockedHttpURLLoad("move_caret.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "move_caret.html",
                                 &web_view_helper);
  WebLocalFrameImpl* frame = web_view_helper.WebView()->MainFrameImpl();

  WebRect initial_start_rect;
  WebRect initial_end_rect;
  WebRect start_rect;
  WebRect end_rect;

  frame->ExecuteScript(WebScriptSource("selectRange();"));
  web_view_helper.WebView()->SelectionBounds(initial_start_rect,
                                             initial_end_rect);
  WebPoint moved_start(TopLeft(initial_start_rect));

  moved_start.y += 40;
  frame->SelectRange(moved_start, BottomRightMinusOne(initial_end_rect));
  web_view_helper.WebView()->SelectionBounds(start_rect, end_rect);
  EXPECT_EQ(start_rect, initial_start_rect);
  EXPECT_EQ(end_rect, initial_end_rect);

  moved_start.y -= 80;
  frame->SelectRange(moved_start, BottomRightMinusOne(initial_end_rect));
  web_view_helper.WebView()->SelectionBounds(start_rect, end_rect);
  EXPECT_EQ(start_rect, initial_start_rect);
  EXPECT_EQ(end_rect, initial_end_rect);

  WebPoint moved_end(BottomRightMinusOne(initial_end_rect));

  moved_end.y += 40;
  frame->SelectRange(TopLeft(initial_start_rect), moved_end);
  web_view_helper.WebView()->SelectionBounds(start_rect, end_rect);
  EXPECT_EQ(start_rect, initial_start_rect);
  EXPECT_EQ(end_rect, initial_end_rect);

  moved_end.y -= 80;
  frame->SelectRange(TopLeft(initial_start_rect), moved_end);
  web_view_helper.WebView()->SelectionBounds(start_rect, end_rect);
  EXPECT_EQ(start_rect, initial_start_rect);
  EXPECT_EQ(end_rect, initial_end_rect);
}

TEST_P(ParameterizedWebFrameTest, MoveCaretStaysHorizontallyAlignedWhenMoved) {
  WebLocalFrameImpl* frame;
  RegisterMockedHttpURLLoad("move_caret.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "move_caret.html",
                                 &web_view_helper);
  frame = (WebLocalFrameImpl*)web_view_helper.WebView()->MainFrame();

  WebRect initial_start_rect;
  WebRect initial_end_rect;
  WebRect start_rect;
  WebRect end_rect;

  frame->ExecuteScript(WebScriptSource("selectCaret();"));
  web_view_helper.WebView()->SelectionBounds(initial_start_rect,
                                             initial_end_rect);
  WebPoint move_to(TopLeft(initial_start_rect));

  move_to.y += 40;
  frame->MoveCaretSelection(move_to);
  web_view_helper.WebView()->SelectionBounds(start_rect, end_rect);
  EXPECT_EQ(start_rect, initial_start_rect);
  EXPECT_EQ(end_rect, initial_end_rect);

  move_to.y -= 80;
  frame->MoveCaretSelection(move_to);
  web_view_helper.WebView()->SelectionBounds(start_rect, end_rect);
  EXPECT_EQ(start_rect, initial_start_rect);
  EXPECT_EQ(end_rect, initial_end_rect);
}
#endif

class CompositedSelectionBoundsTestLayerTreeView : public WebLayerTreeView {
 public:
  CompositedSelectionBoundsTestLayerTreeView() : selection_cleared_(false) {}
  ~CompositedSelectionBoundsTestLayerTreeView() override {}

  void RegisterSelection(const WebSelection& selection) override {
    selection_ = WTF::MakeUnique<WebSelection>(selection);
  }

  void ClearSelection() override {
    selection_cleared_ = true;
    selection_.reset();
  }

  bool GetAndResetSelectionCleared() {
    bool selection_cleared = selection_cleared_;
    selection_cleared_ = false;
    return selection_cleared;
  }

  const WebSelection* Selection() const { return selection_.get(); }
  const WebSelectionBound* Start() const {
    return selection_ ? &selection_->Start() : nullptr;
  }
  const WebSelectionBound* end() const {
    return selection_ ? &selection_->end() : nullptr;
  }

 private:
  bool selection_cleared_;
  std::unique_ptr<WebSelection> selection_;
};

class CompositedSelectionBoundsTestWebViewClient
    : public FrameTestHelpers::TestWebViewClient {
 public:
  ~CompositedSelectionBoundsTestWebViewClient() override {}
  WebLayerTreeView* InitializeLayerTreeView() override {
    return &test_layer_tree_view_;
  }

  CompositedSelectionBoundsTestLayerTreeView& SelectionLayerTreeView() {
    return test_layer_tree_view_;
  }

 private:
  CompositedSelectionBoundsTestLayerTreeView test_layer_tree_view_;
};

class CompositedSelectionBoundsTest : public WebFrameTest {
 protected:
  CompositedSelectionBoundsTest()
      : fake_selection_layer_tree_view_(
            fake_selection_web_view_client_.SelectionLayerTreeView()) {
    RuntimeEnabledFeatures::setCompositedSelectionUpdateEnabled(true);
    RegisterMockedHttpURLLoad("Ahem.ttf");

    web_view_helper_.Initialize(true, nullptr, &fake_selection_web_view_client_,
                                nullptr);
    web_view_helper_.WebView()->GetSettings()->SetDefaultFontSize(12);
    web_view_helper_.WebView()->SetDefaultPageScaleLimits(1, 1);
    web_view_helper_.Resize(WebSize(640, 480));
  }

  void RunTestWithNoSelection(const char* test_file) {
    RegisterMockedHttpURLLoad(test_file);
    web_view_helper_.WebView()->SetFocus(true);
    FrameTestHelpers::LoadFrame(web_view_helper_.WebView()->MainFrame(),
                                base_url_ + test_file);
    web_view_helper_.WebView()->UpdateAllLifecyclePhases();

    const WebSelection* selection = fake_selection_layer_tree_view_.Selection();
    const WebSelectionBound* select_start =
        fake_selection_layer_tree_view_.Start();
    const WebSelectionBound* select_end = fake_selection_layer_tree_view_.end();

    EXPECT_FALSE(selection);
    EXPECT_FALSE(select_start);
    EXPECT_FALSE(select_end);
  }

  void RunTest(const char* test_file) {
    RegisterMockedHttpURLLoad(test_file);
    web_view_helper_.WebView()->SetFocus(true);
    FrameTestHelpers::LoadFrame(web_view_helper_.WebView()->MainFrame(),
                                base_url_ + test_file);
    web_view_helper_.WebView()->UpdateAllLifecyclePhases();

    v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
    v8::Local<v8::Value> result =
        web_view_helper_.WebView()
            ->MainFrameImpl()
            ->ExecuteScriptAndReturnValue(WebScriptSource("expectedResult"));
    ASSERT_FALSE(result.IsEmpty() || (*result)->IsUndefined());

    ASSERT_TRUE((*result)->IsArray());
    v8::Array& expected_result = *v8::Array::Cast(*result);
    ASSERT_GE(expected_result.Length(), 10u);

    v8::Local<v8::Context> context =
        v8::Isolate::GetCurrent()->GetCurrentContext();

    const int start_edge_top_in_layer_x = expected_result.Get(context, 1)
                                              .ToLocalChecked()
                                              .As<v8::Int32>()
                                              ->Value();
    const int start_edge_top_in_layer_y = expected_result.Get(context, 2)
                                              .ToLocalChecked()
                                              .As<v8::Int32>()
                                              ->Value();
    const int start_edge_bottom_in_layer_x = expected_result.Get(context, 3)
                                                 .ToLocalChecked()
                                                 .As<v8::Int32>()
                                                 ->Value();
    const int start_edge_bottom_in_layer_y = expected_result.Get(context, 4)
                                                 .ToLocalChecked()
                                                 .As<v8::Int32>()
                                                 ->Value();

    const int end_edge_top_in_layer_x = expected_result.Get(context, 6)
                                            .ToLocalChecked()
                                            .As<v8::Int32>()
                                            ->Value();
    const int end_edge_top_in_layer_y = expected_result.Get(context, 7)
                                            .ToLocalChecked()
                                            .As<v8::Int32>()
                                            ->Value();
    const int end_edge_bottom_in_layer_x = expected_result.Get(context, 8)
                                               .ToLocalChecked()
                                               .As<v8::Int32>()
                                               ->Value();
    const int end_edge_bottom_in_layer_y = expected_result.Get(context, 9)
                                               .ToLocalChecked()
                                               .As<v8::Int32>()
                                               ->Value();

    const IntPoint hit_point =
        IntPoint((start_edge_top_in_layer_x + start_edge_bottom_in_layer_x +
                  end_edge_top_in_layer_x + end_edge_bottom_in_layer_x) /
                     4,
                 (start_edge_top_in_layer_y + start_edge_bottom_in_layer_y +
                  end_edge_top_in_layer_y + end_edge_bottom_in_layer_y) /
                         4 +
                     3);

    WebGestureEvent gesture_event(WebInputEvent::kGestureTap,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::kTimeStampForTesting);
    gesture_event.SetFrameScale(1);
    gesture_event.x = gesture_event.global_x = hit_point.X();
    gesture_event.y = gesture_event.global_y = hit_point.Y();
    gesture_event.source_device = kWebGestureDeviceTouchscreen;

    web_view_helper_.WebView()
        ->MainFrameImpl()
        ->GetFrame()
        ->GetEventHandler()
        .HandleGestureEvent(gesture_event);

    const WebSelection* selection = fake_selection_layer_tree_view_.Selection();
    const WebSelectionBound* select_start =
        fake_selection_layer_tree_view_.Start();
    const WebSelectionBound* select_end = fake_selection_layer_tree_view_.end();

    ASSERT_TRUE(selection);
    ASSERT_TRUE(select_start);
    ASSERT_TRUE(select_end);

    EXPECT_FALSE(selection->IsNone());

    blink::Node* layer_owner_node_for_start = V8Node::toImplWithTypeCheck(
        v8::Isolate::GetCurrent(), expected_result.Get(0));
    ASSERT_TRUE(layer_owner_node_for_start);
    EXPECT_EQ(layer_owner_node_for_start->GetLayoutObject()
                  ->EnclosingLayer()
                  ->EnclosingLayerForPaintInvalidation()
                  ->GraphicsLayerBacking()
                  ->PlatformLayer()
                  ->Id(),
              select_start->layer_id);

    EXPECT_EQ(start_edge_top_in_layer_x, select_start->edge_top_in_layer.x);
    EXPECT_EQ(start_edge_top_in_layer_y, select_start->edge_top_in_layer.y);
    EXPECT_EQ(start_edge_bottom_in_layer_x,
              select_start->edge_bottom_in_layer.x);

    blink::Node* layer_owner_node_for_end = V8Node::toImplWithTypeCheck(
        v8::Isolate::GetCurrent(),
        expected_result.Get(context, 5).ToLocalChecked());

    ASSERT_TRUE(layer_owner_node_for_end);
    EXPECT_EQ(layer_owner_node_for_end->GetLayoutObject()
                  ->EnclosingLayer()
                  ->EnclosingLayerForPaintInvalidation()
                  ->GraphicsLayerBacking()
                  ->PlatformLayer()
                  ->Id(),
              select_end->layer_id);

    EXPECT_EQ(end_edge_top_in_layer_x, select_end->edge_top_in_layer.x);
    EXPECT_EQ(end_edge_top_in_layer_y, select_end->edge_top_in_layer.y);
    EXPECT_EQ(end_edge_bottom_in_layer_x, select_end->edge_bottom_in_layer.x);

    // Platform differences can introduce small stylistic deviations in
    // y-axis positioning, the details of which aren't relevant to
    // selection behavior. However, such deviations from the expected value
    // should be consistent for the corresponding y coordinates.
    int y_bottom_epsilon = 0;
    if (expected_result.Length() == 13)
      y_bottom_epsilon = expected_result.Get(context, 12)
                             .ToLocalChecked()
                             .As<v8::Int32>()
                             ->Value();
    int y_bottom_deviation =
        start_edge_bottom_in_layer_y - select_start->edge_bottom_in_layer.y;
    EXPECT_GE(y_bottom_epsilon, std::abs(y_bottom_deviation));
    EXPECT_EQ(y_bottom_deviation,
              end_edge_bottom_in_layer_y - select_end->edge_bottom_in_layer.y);
  }

  void RunTestWithMultipleFiles(const char* test_file, ...) {
    va_list aux_files;
    va_start(aux_files, test_file);
    while (const char* aux_file = va_arg(aux_files, const char*))
      RegisterMockedHttpURLLoad(aux_file);
    va_end(aux_files);

    RunTest(test_file);
  }

  CompositedSelectionBoundsTestWebViewClient fake_selection_web_view_client_;
  CompositedSelectionBoundsTestLayerTreeView& fake_selection_layer_tree_view_;
  FrameTestHelpers::WebViewHelper web_view_helper_;
};

TEST_F(CompositedSelectionBoundsTest, None) {
  RunTestWithNoSelection("composited_selection_bounds_none.html");
}
TEST_F(CompositedSelectionBoundsTest, NoneReadonlyCaret) {
  RunTestWithNoSelection(
      "composited_selection_bounds_none_readonly_caret.html");
}
TEST_F(CompositedSelectionBoundsTest, DetachedFrame) {
  RunTestWithNoSelection("composited_selection_bounds_detached_frame.html");
}

TEST_F(CompositedSelectionBoundsTest, Basic) {
  RunTest("composited_selection_bounds_basic.html");
}
TEST_F(CompositedSelectionBoundsTest, Transformed) {
  RunTest("composited_selection_bounds_transformed.html");
}
TEST_F(CompositedSelectionBoundsTest, VerticalRightToLeft) {
  RunTest("composited_selection_bounds_vertical_rl.html");
}
TEST_F(CompositedSelectionBoundsTest, VerticalLeftToRight) {
  RunTest("composited_selection_bounds_vertical_lr.html");
}
TEST_F(CompositedSelectionBoundsTest, SplitLayer) {
  RunTest("composited_selection_bounds_split_layer.html");
}
TEST_F(CompositedSelectionBoundsTest, Iframe) {
  RunTestWithMultipleFiles("composited_selection_bounds_iframe.html",
                           "composited_selection_bounds_basic.html", nullptr);
}
TEST_F(CompositedSelectionBoundsTest, Editable) {
  RunTest("composited_selection_bounds_editable.html");
}
TEST_F(CompositedSelectionBoundsTest, EditableDiv) {
  RunTest("composited_selection_bounds_editable_div.html");
}

class DisambiguationPopupTestWebViewClient
    : public FrameTestHelpers::TestWebViewClient {
 public:
  bool DidTapMultipleTargets(const WebSize&,
                             const WebRect&,
                             const WebVector<WebRect>& target_rects) override {
    EXPECT_GE(target_rects.size(), 2u);
    triggered_ = true;
    return true;
  }

  bool Triggered() const { return triggered_; }
  void ResetTriggered() { triggered_ = false; }
  bool triggered_;
};

static WebCoalescedInputEvent FatTap(int x, int y) {
  WebGestureEvent event(WebInputEvent::kGestureTap, WebInputEvent::kNoModifiers,
                        WebInputEvent::kTimeStampForTesting);
  event.source_device = kWebGestureDeviceTouchscreen;
  event.x = x;
  event.y = y;
  event.data.tap.width = 50;
  event.data.tap.height = 50;
  return WebCoalescedInputEvent(event);
}

TEST_P(ParameterizedWebFrameTest, DisambiguationPopup) {
  const std::string html_file = "disambiguation_popup.html";
  RegisterMockedHttpURLLoad(html_file);

  DisambiguationPopupTestWebViewClient client;

  // Make sure we initialize to minimum scale, even if the window size
  // only becomes available after the load begins.
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + html_file, true, 0, &client);
  web_view_helper.Resize(WebSize(1000, 1000));

  client.ResetTriggered();
  web_view_helper.WebView()->HandleInputEvent(FatTap(0, 0));
  EXPECT_FALSE(client.Triggered());

  client.ResetTriggered();
  web_view_helper.WebView()->HandleInputEvent(FatTap(200, 115));
  EXPECT_FALSE(client.Triggered());

  for (int i = 0; i <= 46; i++) {
    client.ResetTriggered();
    web_view_helper.WebView()->HandleInputEvent(FatTap(120, 230 + i * 5));

    int j = i % 10;
    if (j >= 7 && j <= 9)
      EXPECT_TRUE(client.Triggered());
    else
      EXPECT_FALSE(client.Triggered());
  }

  for (int i = 0; i <= 46; i++) {
    client.ResetTriggered();
    web_view_helper.WebView()->HandleInputEvent(FatTap(10 + i * 5, 590));

    int j = i % 10;
    if (j >= 7 && j <= 9)
      EXPECT_TRUE(client.Triggered());
    else
      EXPECT_FALSE(client.Triggered());
  }

  // The same taps shouldn't trigger didTapMultipleTargets() after disabling the
  // notification for multi-target-tap.
  web_view_helper.WebView()
      ->GetSettings()
      ->SetMultiTargetTapNotificationEnabled(false);

  for (int i = 0; i <= 46; i++) {
    client.ResetTriggered();
    web_view_helper.WebView()->HandleInputEvent(FatTap(10 + i * 5, 590));
    EXPECT_FALSE(client.Triggered());
  }
}

TEST_P(ParameterizedWebFrameTest, DisambiguationPopupNoContainer) {
  RegisterMockedHttpURLLoad("disambiguation_popup_no_container.html");

  DisambiguationPopupTestWebViewClient client;

  // Make sure we initialize to minimum scale, even if the window size
  // only becomes available after the load begins.
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "disambiguation_popup_no_container.html", true, 0, &client);
  web_view_helper.Resize(WebSize(1000, 1000));

  client.ResetTriggered();
  web_view_helper.WebView()->HandleInputEvent(FatTap(50, 50));
  EXPECT_FALSE(client.Triggered());
}

TEST_P(ParameterizedWebFrameTest, DisambiguationPopupMobileSite) {
  const std::string html_file = "disambiguation_popup_mobile_site.html";
  RegisterMockedHttpURLLoad(html_file);

  DisambiguationPopupTestWebViewClient client;

  // Make sure we initialize to minimum scale, even if the window size
  // only becomes available after the load begins.
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + html_file, true, nullptr,
                                    &client, nullptr, EnableViewportSettings);
  web_view_helper.Resize(WebSize(1000, 1000));

  client.ResetTriggered();
  web_view_helper.WebView()->HandleInputEvent(FatTap(0, 0));
  EXPECT_FALSE(client.Triggered());

  client.ResetTriggered();
  web_view_helper.WebView()->HandleInputEvent(FatTap(200, 115));
  EXPECT_FALSE(client.Triggered());

  for (int i = 0; i <= 46; i++) {
    client.ResetTriggered();
    web_view_helper.WebView()->HandleInputEvent(FatTap(120, 230 + i * 5));
    EXPECT_FALSE(client.Triggered());
  }

  for (int i = 0; i <= 46; i++) {
    client.ResetTriggered();
    web_view_helper.WebView()->HandleInputEvent(FatTap(10 + i * 5, 590));
    EXPECT_FALSE(client.Triggered());
  }
}

TEST_P(ParameterizedWebFrameTest, DisambiguationPopupViewportSite) {
  const std::string html_file = "disambiguation_popup_viewport_site.html";
  RegisterMockedHttpURLLoad(html_file);

  DisambiguationPopupTestWebViewClient client;

  // Make sure we initialize to minimum scale, even if the window size
  // only becomes available after the load begins.
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + html_file, true, nullptr,
                                    &client, nullptr, EnableViewportSettings);
  web_view_helper.Resize(WebSize(1000, 1000));

  client.ResetTriggered();
  web_view_helper.WebView()->HandleInputEvent(FatTap(0, 0));
  EXPECT_FALSE(client.Triggered());

  client.ResetTriggered();
  web_view_helper.WebView()->HandleInputEvent(FatTap(200, 115));
  EXPECT_FALSE(client.Triggered());

  for (int i = 0; i <= 46; i++) {
    client.ResetTriggered();
    web_view_helper.WebView()->HandleInputEvent(FatTap(120, 230 + i * 5));
    EXPECT_FALSE(client.Triggered());
  }

  for (int i = 0; i <= 46; i++) {
    client.ResetTriggered();
    web_view_helper.WebView()->HandleInputEvent(FatTap(10 + i * 5, 590));
    EXPECT_FALSE(client.Triggered());
  }
}

TEST_F(WebFrameTest, DisambiguationPopupVisualViewport) {
  const std::string html_file = "disambiguation_popup_200_by_800.html";
  RegisterMockedHttpURLLoad(html_file);

  DisambiguationPopupTestWebViewClient client;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + html_file, true, nullptr,
                                    &client, nullptr, ConfigureAndroid);

  WebViewImpl* web_view_impl = web_view_helper.WebView();
  ASSERT_TRUE(web_view_impl);
  LocalFrame* frame = web_view_impl->MainFrameImpl()->GetFrame();
  ASSERT_TRUE(frame);

  web_view_helper.Resize(WebSize(100, 200));

  // Scroll main frame to the bottom of the document
  web_view_impl->MainFrame()->SetScrollOffset(WebSize(0, 400));
  EXPECT_SIZE_EQ(ScrollOffset(0, 400), frame->View()->GetScrollOffset());

  web_view_impl->SetPageScaleFactor(2.0);

  // Scroll visual viewport to the top of the main frame.
  VisualViewport& visual_viewport = frame->GetPage()->GetVisualViewport();
  visual_viewport.SetLocation(FloatPoint(0, 0));
  EXPECT_SIZE_EQ(ScrollOffset(0, 0), visual_viewport.GetScrollOffset());

  // Tap at the top: there is nothing there.
  client.ResetTriggered();
  web_view_helper.WebView()->HandleInputEvent(FatTap(10, 60));
  EXPECT_FALSE(client.Triggered());

  // Scroll visual viewport to the bottom of the main frame.
  visual_viewport.SetLocation(FloatPoint(0, 200));
  EXPECT_SIZE_EQ(ScrollOffset(0, 200), visual_viewport.GetScrollOffset());

  // Now the tap with the same coordinates should hit two elements.
  client.ResetTriggered();
  web_view_helper.WebView()->HandleInputEvent(FatTap(10, 60));
  EXPECT_TRUE(client.Triggered());

  // The same tap shouldn't trigger didTapMultipleTargets() after disabling the
  // notification for multi-target-tap.
  web_view_helper.WebView()
      ->GetSettings()
      ->SetMultiTargetTapNotificationEnabled(false);
  client.ResetTriggered();
  web_view_helper.WebView()->HandleInputEvent(FatTap(10, 60));
  EXPECT_FALSE(client.Triggered());
}

TEST_P(ParameterizedWebFrameTest, DisambiguationPopupBlacklist) {
  const unsigned kViewportWidth = 500;
  const unsigned kViewportHeight = 1000;
  const unsigned kDivHeight = 100;
  const std::string html_file = "disambiguation_popup_blacklist.html";
  RegisterMockedHttpURLLoad(html_file);

  DisambiguationPopupTestWebViewClient client;

  // Make sure we initialize to minimum scale, even if the window size
  // only becomes available after the load begins.
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + html_file, true, 0, &client);
  web_view_helper.Resize(WebSize(kViewportWidth, kViewportHeight));

  // Click somewhere where the popup shouldn't appear.
  client.ResetTriggered();
  web_view_helper.WebView()->HandleInputEvent(FatTap(kViewportWidth / 2, 0));
  EXPECT_FALSE(client.Triggered());

  // Click directly in between two container divs with click handlers, with
  // children that don't handle clicks.
  client.ResetTriggered();
  web_view_helper.WebView()->HandleInputEvent(
      FatTap(kViewportWidth / 2, kDivHeight));
  EXPECT_TRUE(client.Triggered());

  // The third div container should be blacklisted if you click on the link it
  // contains.
  client.ResetTriggered();
  web_view_helper.WebView()->HandleInputEvent(
      FatTap(kViewportWidth / 2, kDivHeight * 3.25));
  EXPECT_FALSE(client.Triggered());
}

TEST_P(ParameterizedWebFrameTest, DisambiguationPopupPageScale) {
  RegisterMockedHttpURLLoad("disambiguation_popup_page_scale.html");

  DisambiguationPopupTestWebViewClient client;

  // Make sure we initialize to minimum scale, even if the window size
  // only becomes available after the load begins.
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "disambiguation_popup_page_scale.html", true, 0, &client);
  web_view_helper.Resize(WebSize(1000, 1000));

  client.ResetTriggered();
  web_view_helper.WebView()->HandleInputEvent(FatTap(80, 80));
  EXPECT_TRUE(client.Triggered());

  client.ResetTriggered();
  web_view_helper.WebView()->HandleInputEvent(FatTap(230, 190));
  EXPECT_TRUE(client.Triggered());

  web_view_helper.WebView()->SetPageScaleFactor(3.0f);
  web_view_helper.WebView()->UpdateAllLifecyclePhases();

  client.ResetTriggered();
  web_view_helper.WebView()->HandleInputEvent(FatTap(240, 240));
  EXPECT_TRUE(client.Triggered());

  client.ResetTriggered();
  web_view_helper.WebView()->HandleInputEvent(FatTap(690, 570));
  EXPECT_FALSE(client.Triggered());
}

class TestSubstituteDataWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  TestSubstituteDataWebFrameClient() : commit_called_(false) {}

  virtual void DidFailProvisionalLoad(const WebURLError& error,
                                      WebHistoryCommitType) {
    Frame()->LoadHTMLString("This should appear",
                            ToKURL("data:text/html,chromewebdata"),
                            error.unreachable_url, true);
  }

  virtual void DidCommitProvisionalLoad(const WebHistoryItem&,
                                        WebHistoryCommitType) {
    if (Frame()->DataSource()->GetResponse().Url() !=
        WebURL(URLTestHelpers::ToKURL("about:blank")))
      commit_called_ = true;
  }

  bool CommitCalled() const { return commit_called_; }

 private:
  bool commit_called_;
};

TEST_P(ParameterizedWebFrameTest, ReplaceNavigationAfterHistoryNavigation) {
  TestSubstituteDataWebFrameClient web_frame_client;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank", true, &web_frame_client);
  WebFrame* frame = web_view_helper.WebView()->MainFrame();

  // Load a url as a history navigation that will return an error.
  // TestSubstituteDataWebFrameClient will start a SubstituteData load in
  // response to the load failure, which should get fully committed.  Due to
  // https://bugs.webkit.org/show_bug.cgi?id=91685,
  // FrameLoader::didReceiveData() wasn't getting called in this case, which
  // resulted in the SubstituteData document not getting displayed.
  WebURLError error;
  error.reason = 1337;
  error.domain = "WebFrameTest";
  std::string error_url = "http://0.0.0.0";
  WebURLResponse response;
  response.SetURL(URLTestHelpers::ToKURL(error_url));
  response.SetMIMEType("text/html");
  response.SetHTTPStatusCode(500);
  WebHistoryItem error_history_item;
  error_history_item.Initialize();
  error_history_item.SetURLString(
      WebString::FromUTF8(error_url.c_str(), error_url.length()));
  Platform::Current()->GetURLLoaderMockFactory()->RegisterErrorURL(
      URLTestHelpers::ToKURL(error_url), response, error);
  FrameTestHelpers::LoadHistoryItem(frame, error_history_item,
                                    kWebHistoryDifferentDocumentLoad,
                                    WebCachePolicy::kUseProtocolCachePolicy);
  WebString text = WebFrameContentDumper::DumpWebViewAsText(
      web_view_helper.WebView(), std::numeric_limits<size_t>::max());
  EXPECT_EQ("This should appear", text.Utf8());
  EXPECT_TRUE(web_frame_client.CommitCalled());
}

class TestWillInsertBodyWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  TestWillInsertBodyWebFrameClient() : num_bodies_(0), did_load_(false) {}

  void DidCommitProvisionalLoad(const WebHistoryItem&,
                                WebHistoryCommitType) override {
    num_bodies_ = 0;
    did_load_ = true;
  }

  void DidCreateDocumentElement(WebLocalFrame*) override {
    EXPECT_EQ(0, num_bodies_);
  }

  void WillInsertBody(WebLocalFrame*) override { num_bodies_++; }

  int num_bodies_;
  bool did_load_;
};

TEST_P(ParameterizedWebFrameTest, HTMLDocument) {
  RegisterMockedHttpURLLoad("clipped-body.html");

  TestWillInsertBodyWebFrameClient web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "clipped-body.html", false,
                                    &web_frame_client);

  EXPECT_TRUE(web_frame_client.did_load_);
  EXPECT_EQ(1, web_frame_client.num_bodies_);
}

TEST_P(ParameterizedWebFrameTest, EmptyDocument) {
  RegisterMockedHttpURLLoad("frameserializer/svg/green_rectangle.svg");

  TestWillInsertBodyWebFrameClient web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(false, &web_frame_client);

  EXPECT_FALSE(web_frame_client.did_load_);
  // The empty document that a new frame starts with triggers this.
  EXPECT_EQ(1, web_frame_client.num_bodies_);
}

TEST_P(ParameterizedWebFrameTest,
       MoveCaretSelectionTowardsWindowPointWithNoSelection) {
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank", true);
  WebFrame* frame = web_view_helper.WebView()->MainFrame();

  // This test passes if this doesn't crash.
  frame->ToWebLocalFrame()->MoveCaretSelection(WebPoint(0, 0));
}

class TextCheckClient : public WebTextCheckClient {
 public:
  TextCheckClient() : number_of_times_checked_(0) {}
  virtual ~TextCheckClient() {}
  void RequestCheckingOfText(const WebString&,
                             WebTextCheckingCompletion* completion) override {
    ++number_of_times_checked_;
    Vector<WebTextCheckingResult> results;
    const int kMisspellingStartOffset = 1;
    const int kMisspellingLength = 8;
    results.push_back(WebTextCheckingResult(kWebTextDecorationTypeSpelling,
                                            kMisspellingStartOffset,
                                            kMisspellingLength, WebString()));
    completion->DidFinishCheckingText(results);
  }
  int NumberOfTimesChecked() const { return number_of_times_checked_; }

 private:
  int number_of_times_checked_;
};

TEST_P(ParameterizedWebFrameTest, ReplaceMisspelledRange) {
  RegisterMockedHttpURLLoad("spell.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "spell.html", &web_view_helper);

  WebLocalFrameImpl* frame = web_view_helper.WebView()->MainFrameImpl();
  TextCheckClient textcheck;
  frame->SetTextCheckClient(&textcheck);

  Document* document = frame->GetFrame()->GetDocument();
  Element* element = document->GetElementById("data");

  web_view_helper.WebView()->GetSettings()->SetEditingBehavior(
      WebSettings::kEditingBehaviorWin);

  element->focus();
  NonThrowableExceptionState exception_state;
  document->execCommand("InsertText", false, "_wellcome_.", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  if (RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled()) {
    document->GetFrame()
        ->GetSpellChecker()
        .GetIdleSpellCheckCallback()
        .ForceInvocationForTesting();
  }

  const int kAllTextBeginOffset = 0;
  const int kAllTextLength = 11;
  frame->SelectRange(WebRange(kAllTextBeginOffset, kAllTextLength));
  EphemeralRange selection_range =
      frame->GetFrame()
          ->Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated()
          .ToNormalizedEphemeralRange();

  EXPECT_EQ(1, textcheck.NumberOfTimesChecked());
  EXPECT_EQ(1U, document->Markers()
                    .MarkersInRange(selection_range, DocumentMarker::kSpelling)
                    .size());

  frame->ReplaceMisspelledRange("welcome");
  EXPECT_EQ("_welcome_.",
            WebFrameContentDumper::DumpWebViewAsText(
                web_view_helper.WebView(), std::numeric_limits<size_t>::max())
                .Utf8());
}

TEST_P(ParameterizedWebFrameTest, RemoveSpellingMarkers) {
  RegisterMockedHttpURLLoad("spell.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "spell.html", &web_view_helper);

  WebLocalFrameImpl* frame = web_view_helper.WebView()->MainFrameImpl();
  TextCheckClient textcheck;
  frame->SetTextCheckClient(&textcheck);

  Document* document = frame->GetFrame()->GetDocument();
  Element* element = document->GetElementById("data");

  web_view_helper.WebView()->GetSettings()->SetEditingBehavior(
      WebSettings::kEditingBehaviorWin);

  element->focus();
  NonThrowableExceptionState exception_state;
  document->execCommand("InsertText", false, "_wellcome_.", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  if (RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled()) {
    document->GetFrame()
        ->GetSpellChecker()
        .GetIdleSpellCheckCallback()
        .ForceInvocationForTesting();
  }

  frame->RemoveSpellingMarkers();

  const int kAllTextBeginOffset = 0;
  const int kAllTextLength = 11;
  frame->SelectRange(WebRange(kAllTextBeginOffset, kAllTextLength));
  EphemeralRange selection_range =
      frame->GetFrame()
          ->Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated()
          .ToNormalizedEphemeralRange();

  EXPECT_EQ(0U, document->Markers()
                    .MarkersInRange(selection_range, DocumentMarker::kSpelling)
                    .size());
}

static void GetSpellingMarkerOffsets(WebVector<unsigned>* offsets,
                                     const Document& document) {
  Vector<unsigned> result;
  const DocumentMarkerVector& document_markers = document.Markers().Markers();
  for (size_t i = 0; i < document_markers.size(); ++i)
    result.push_back(document_markers[i]->StartOffset());
  offsets->Assign(result);
}

TEST_P(ParameterizedWebFrameTest, RemoveSpellingMarkersUnderWords) {
  RegisterMockedHttpURLLoad("spell.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "spell.html", &web_view_helper);

  WebLocalFrameImpl* web_frame = web_view_helper.WebView()->MainFrameImpl();
  TextCheckClient textcheck;
  web_frame->SetTextCheckClient(&textcheck);

  LocalFrame* frame = web_frame->GetFrame();
  Document* document = frame->GetDocument();
  Element* element = document->GetElementById("data");

  web_view_helper.WebView()->GetSettings()->SetEditingBehavior(
      WebSettings::kEditingBehaviorWin);

  element->focus();
  NonThrowableExceptionState exception_state;
  document->execCommand("InsertText", false, " wellcome ", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  if (RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled())
    frame->GetSpellChecker()
        .GetIdleSpellCheckCallback()
        .ForceInvocationForTesting();

  WebVector<unsigned> offsets1;
  GetSpellingMarkerOffsets(&offsets1, *frame->GetDocument());
  EXPECT_EQ(1U, offsets1.size());

  Vector<String> words;
  words.push_back("wellcome");
  frame->RemoveSpellingMarkersUnderWords(words);

  WebVector<unsigned> offsets2;
  GetSpellingMarkerOffsets(&offsets2, *frame->GetDocument());
  EXPECT_EQ(0U, offsets2.size());
}

class StubbornTextCheckClient : public WebTextCheckClient {
 public:
  StubbornTextCheckClient() : completion_(0) {}
  virtual ~StubbornTextCheckClient() {}

  virtual void RequestCheckingOfText(
      const WebString&,
      WebTextCheckingCompletion* completion) override {
    completion_ = completion;
  }

  void CancelAllPendingRequests() override {
    if (!completion_)
      return;
    completion_->DidCancelCheckingText();
    completion_ = nullptr;
  }

  void KickNoResults() { Kick(-1, -1, kWebTextDecorationTypeSpelling); }

  void Kick() { Kick(1, 8, kWebTextDecorationTypeSpelling); }

  void KickGrammar() { Kick(1, 8, kWebTextDecorationTypeGrammar); }

 private:
  void Kick(int misspelling_start_offset,
            int misspelling_length,
            WebTextDecorationType type) {
    if (!completion_)
      return;
    Vector<WebTextCheckingResult> results;
    if (misspelling_start_offset >= 0 && misspelling_length > 0)
      results.push_back(WebTextCheckingResult(type, misspelling_start_offset,
                                              misspelling_length));
    completion_->DidFinishCheckingText(results);
    completion_ = 0;
  }

  WebTextCheckingCompletion* completion_;
};

TEST_P(ParameterizedWebFrameTest, SlowSpellcheckMarkerPosition) {
  RegisterMockedHttpURLLoad("spell.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "spell.html", &web_view_helper);

  WebLocalFrameImpl* frame = web_view_helper.WebView()->MainFrameImpl();
  StubbornTextCheckClient textcheck;
  frame->SetTextCheckClient(&textcheck);

  Document* document = frame->GetFrame()->GetDocument();
  Element* element = document->GetElementById("data");

  web_view_helper.WebView()->GetSettings()->SetEditingBehavior(
      WebSettings::kEditingBehaviorWin);

  element->focus();
  NonThrowableExceptionState exception_state;
  document->execCommand("InsertText", false, "wellcome ", exception_state);
  EXPECT_FALSE(exception_state.HadException());
  document->execCommand("InsertText", false, "he", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  if (RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled()) {
    document->GetFrame()
        ->GetSpellChecker()
        .GetIdleSpellCheckCallback()
        .ForceInvocationForTesting();
  }

  textcheck.Kick();

  WebVector<unsigned> offsets;
  GetSpellingMarkerOffsets(&offsets, *frame->GetFrame()->GetDocument());
  EXPECT_EQ(0U, offsets.size());
}

// This test verifies that cancelling spelling request does not cause a
// write-after-free when there's no spellcheck client set.
TEST_P(ParameterizedWebFrameTest, CancelSpellingRequestCrash) {
  // The relevant code paths are obsolete with idle time spell checker.
  if (RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled())
    return;

  RegisterMockedHttpURLLoad("spell.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "spell.html");

  WebLocalFrameImpl* frame = web_view_helper.WebView()->MainFrameImpl();
  frame->SetTextCheckClient(0);

  Document* document = frame->GetFrame()->GetDocument();
  Element* element = document->GetElementById("data");

  web_view_helper.WebView()->GetSettings()->SetEditingBehavior(
      WebSettings::kEditingBehaviorWin);

  element->focus();
  frame->GetFrame()->GetEditor().ReplaceSelectionWithText(
      "A", false, false, InputEvent::InputType::kInsertReplacementText);
  frame->GetFrame()->GetSpellChecker().CancelCheck();
}

TEST_P(ParameterizedWebFrameTest, SpellcheckResultErasesMarkers) {
  RegisterMockedHttpURLLoad("spell.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "spell.html", &web_view_helper);

  WebLocalFrameImpl* frame = web_view_helper.WebView()->MainFrameImpl();
  StubbornTextCheckClient textcheck;
  frame->SetTextCheckClient(&textcheck);

  Document* document = frame->GetFrame()->GetDocument();
  Element* element = document->GetElementById("data");

  web_view_helper.WebView()->GetSettings()->SetEditingBehavior(
      WebSettings::kEditingBehaviorWin);

  element->focus();
  NonThrowableExceptionState exception_state;
  document->execCommand("InsertText", false, "welcome ", exception_state);

  if (RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled()) {
    document->GetFrame()
        ->GetSpellChecker()
        .GetIdleSpellCheckCallback()
        .ForceInvocationForTesting();
  }

  document->UpdateStyleAndLayout();

  EXPECT_FALSE(exception_state.HadException());
  auto range = EphemeralRange::RangeOfContents(*element);
  document->Markers().AddMarker(range.StartPosition(), range.EndPosition(),
                                DocumentMarker::kSpelling);
  document->Markers().AddMarker(range.StartPosition(), range.EndPosition(),
                                DocumentMarker::kGrammar);
  EXPECT_EQ(2U, document->Markers().Markers().size());

  textcheck.KickNoResults();
  EXPECT_EQ(0U, document->Markers().Markers().size());
}

TEST_P(ParameterizedWebFrameTest, SpellcheckResultsSavedInDocument) {
  RegisterMockedHttpURLLoad("spell.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  InitializeTextSelectionWebView(base_url_ + "spell.html", &web_view_helper);

  WebLocalFrameImpl* frame = web_view_helper.WebView()->MainFrameImpl();
  StubbornTextCheckClient textcheck;
  frame->SetTextCheckClient(&textcheck);

  Document* document = frame->GetFrame()->GetDocument();
  Element* element = document->GetElementById("data");

  web_view_helper.WebView()->GetSettings()->SetEditingBehavior(
      WebSettings::kEditingBehaviorWin);

  element->focus();
  NonThrowableExceptionState exception_state;
  document->execCommand("InsertText", false, "wellcome ", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  if (RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled()) {
    document->GetFrame()
        ->GetSpellChecker()
        .GetIdleSpellCheckCallback()
        .ForceInvocationForTesting();
  }

  textcheck.Kick();
  ASSERT_EQ(1U, document->Markers().Markers().size());
  ASSERT_NE(static_cast<DocumentMarker*>(0), document->Markers().Markers()[0]);
  EXPECT_EQ(DocumentMarker::kSpelling,
            document->Markers().Markers()[0]->GetType());

  document->execCommand("InsertText", false, "wellcome ", exception_state);
  EXPECT_FALSE(exception_state.HadException());

  if (RuntimeEnabledFeatures::idleTimeSpellCheckingEnabled()) {
    document->GetFrame()
        ->GetSpellChecker()
        .GetIdleSpellCheckCallback()
        .ForceInvocationForTesting();
  }

  textcheck.KickGrammar();
  ASSERT_EQ(1U, document->Markers().Markers().size());
  ASSERT_NE(static_cast<DocumentMarker*>(0), document->Markers().Markers()[0]);
  EXPECT_EQ(DocumentMarker::kGrammar,
            document->Markers().Markers()[0]->GetType());
}

class TestAccessInitialDocumentWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  TestAccessInitialDocumentWebFrameClient()
      : did_access_initial_document_(false) {}

  virtual void DidAccessInitialDocument() {
    did_access_initial_document_ = true;
  }

  bool did_access_initial_document_;
};

TEST_P(ParameterizedWebFrameTest, DidAccessInitialDocumentBody) {
  // FIXME: Why is this local webViewClient needed instead of the default
  // WebViewHelper one? With out it there's some mysterious crash in the
  // WebViewHelper destructor.
  FrameTestHelpers::TestWebViewClient web_view_client;
  TestAccessInitialDocumentWebFrameClient web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true, &web_frame_client, &web_view_client);
  RunPendingTasks();
  EXPECT_FALSE(web_frame_client.did_access_initial_document_);

  // Create another window that will try to access it.
  FrameTestHelpers::WebViewHelper new_web_view_helper;
  WebView* new_view = new_web_view_helper.InitializeWithOpener(
      web_view_helper.WebView()->MainFrame(), true);
  RunPendingTasks();
  EXPECT_FALSE(web_frame_client.did_access_initial_document_);

  // Access the initial document by modifying the body.
  new_view->MainFrame()->ExecuteScript(
      WebScriptSource("window.opener.document.body.innerHTML += 'Modified';"));
  RunPendingTasks();
  EXPECT_TRUE(web_frame_client.did_access_initial_document_);

  // Access the initial document again, to ensure we don't notify twice.
  new_view->MainFrame()->ExecuteScript(
      WebScriptSource("window.opener.document.body.innerHTML += 'Modified';"));
  RunPendingTasks();
  EXPECT_TRUE(web_frame_client.did_access_initial_document_);
}

TEST_P(ParameterizedWebFrameTest, DidAccessInitialDocumentNavigator) {
  // FIXME: Why is this local webViewClient needed instead of the default
  // WebViewHelper one? With out it there's some mysterious crash in the
  // WebViewHelper destructor.
  FrameTestHelpers::TestWebViewClient web_view_client;
  TestAccessInitialDocumentWebFrameClient web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true, &web_frame_client, &web_view_client);
  RunPendingTasks();
  EXPECT_FALSE(web_frame_client.did_access_initial_document_);

  // Create another window that will try to access it.
  FrameTestHelpers::WebViewHelper new_web_view_helper;
  WebView* new_view = new_web_view_helper.InitializeWithOpener(
      web_view_helper.WebView()->MainFrame(), true);
  RunPendingTasks();
  EXPECT_FALSE(web_frame_client.did_access_initial_document_);

  // Access the initial document to get to the navigator object.
  new_view->MainFrame()->ExecuteScript(
      WebScriptSource("console.log(window.opener.navigator);"));
  RunPendingTasks();
  EXPECT_TRUE(web_frame_client.did_access_initial_document_);
}

TEST_P(ParameterizedWebFrameTest, DidAccessInitialDocumentViaJavascriptUrl) {
  TestAccessInitialDocumentWebFrameClient web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true, &web_frame_client);
  RunPendingTasks();
  EXPECT_FALSE(web_frame_client.did_access_initial_document_);

  // Access the initial document from a javascript: URL.
  FrameTestHelpers::LoadFrame(web_view_helper.WebView()->MainFrame(),
                              "javascript:document.body.appendChild(document."
                              "createTextNode('Modified'))");
  EXPECT_TRUE(web_frame_client.did_access_initial_document_);
}

TEST_P(ParameterizedWebFrameTest, DidAccessInitialDocumentBodyBeforeModalDialog)
{
  // FIXME: Why is this local webViewClient needed instead of the default
  // WebViewHelper one? With out it there's some mysterious crash in the
  // WebViewHelper destructor.
  FrameTestHelpers::TestWebViewClient web_view_client;
  TestAccessInitialDocumentWebFrameClient web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true, &web_frame_client, &web_view_client);
  RunPendingTasks();
  EXPECT_FALSE(web_frame_client.did_access_initial_document_);

  // Create another window that will try to access it.
  FrameTestHelpers::WebViewHelper new_web_view_helper;
  WebView* new_view = new_web_view_helper.InitializeWithOpener(
      web_view_helper.WebView()->MainFrame(), true);
  RunPendingTasks();
  EXPECT_FALSE(web_frame_client.did_access_initial_document_);

  // Access the initial document by modifying the body.
  new_view->MainFrame()->ExecuteScript(
      WebScriptSource("window.opener.document.body.innerHTML += 'Modified';"));
  EXPECT_TRUE(web_frame_client.did_access_initial_document_);

  // Run a modal dialog, which used to run a nested message loop and require
  // a special case for notifying about the access.
  new_view->MainFrame()->ExecuteScript(
      WebScriptSource("window.opener.confirm('Modal');"));
  EXPECT_TRUE(web_frame_client.did_access_initial_document_);

  // Ensure that we don't notify again later.
  RunPendingTasks();
  EXPECT_TRUE(web_frame_client.did_access_initial_document_);
}

TEST_P(ParameterizedWebFrameTest, DidWriteToInitialDocumentBeforeModalDialog)
{
  // FIXME: Why is this local webViewClient needed instead of the default
  // WebViewHelper one? With out it there's some mysterious crash in the
  // WebViewHelper destructor.
  FrameTestHelpers::TestWebViewClient web_view_client;
  TestAccessInitialDocumentWebFrameClient web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true, &web_frame_client, &web_view_client);
  RunPendingTasks();
  EXPECT_FALSE(web_frame_client.did_access_initial_document_);

  // Create another window that will try to access it.
  FrameTestHelpers::WebViewHelper new_web_view_helper;
  WebView* new_view = new_web_view_helper.InitializeWithOpener(
      web_view_helper.WebView()->MainFrame(), true);
  RunPendingTasks();
  EXPECT_FALSE(web_frame_client.did_access_initial_document_);

  // Access the initial document with document.write, which moves us past the
  // initial empty document state of the state machine.
  new_view->MainFrame()->ExecuteScript(
      WebScriptSource("window.opener.document.write('Modified'); "
                      "window.opener.document.close();"));
  EXPECT_TRUE(web_frame_client.did_access_initial_document_);

  // Run a modal dialog, which used to run a nested message loop and require
  // a special case for notifying about the access.
  new_view->MainFrame()->ExecuteScript(
      WebScriptSource("window.opener.confirm('Modal');"));
  EXPECT_TRUE(web_frame_client.did_access_initial_document_);

  // Ensure that we don't notify again later.
  RunPendingTasks();
  EXPECT_TRUE(web_frame_client.did_access_initial_document_);
}

class TestScrolledFrameClient : public FrameTestHelpers::TestWebFrameClient {
 public:
  TestScrolledFrameClient() { Reset(); }
  void Reset() { did_scroll_frame_ = false; }
  bool WasFrameScrolled() const { return did_scroll_frame_; }

  // WebFrameClient:
  void DidChangeScrollOffset() override {
    if (Frame()->Parent())
      return;
    EXPECT_FALSE(did_scroll_frame_);
    FrameView* view = ToWebLocalFrameImpl(Frame())->GetFrameView();
    // FrameView can be scrolled in FrameView::setFixedVisibleContentRect which
    // is called from LocalFrame::createView (before the frame is associated
    // with the the view).
    if (view)
      did_scroll_frame_ = true;
  }

 private:
  bool did_scroll_frame_;
};

TEST_F(WebFrameTest, CompositorScrollIsUserScrollLongPage) {
  RegisterMockedHttpURLLoad("long_scroll.html");
  TestScrolledFrameClient client;

  // Make sure we initialize to minimum scale, even if the window size
  // only becomes available after the load begins.
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "long_scroll.html", true,
                                    &client);
  web_view_helper.Resize(WebSize(1000, 1000));

  WebLocalFrameImpl* frame_impl = web_view_helper.WebView()->MainFrameImpl();
  DocumentLoader::InitialScrollState& initial_scroll_state =
      frame_impl->GetFrame()
          ->Loader()
          .GetDocumentLoader()
          ->GetInitialScrollState();

  EXPECT_FALSE(client.WasFrameScrolled());
  EXPECT_FALSE(initial_scroll_state.was_scrolled_by_user);

  auto* scrollable_area =
      frame_impl->GetFrameView()->LayoutViewportScrollableArea();

  // Do a compositor scroll, verify that this is counted as a user scroll.
  scrollable_area->DidScroll(gfx::ScrollOffset(0, 1));
  web_view_helper.WebView()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                                 WebFloatSize(), 1.7f, 0);
  EXPECT_TRUE(client.WasFrameScrolled());
  EXPECT_TRUE(initial_scroll_state.was_scrolled_by_user);

  client.Reset();
  initial_scroll_state.was_scrolled_by_user = false;

  // The page scale 1.0f and scroll.
  scrollable_area->DidScroll(gfx::ScrollOffset(0, 2));
  web_view_helper.WebView()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                                 WebFloatSize(), 1.0f, 0);
  EXPECT_TRUE(client.WasFrameScrolled());
  EXPECT_TRUE(initial_scroll_state.was_scrolled_by_user);
  client.Reset();
  initial_scroll_state.was_scrolled_by_user = false;

  // No scroll event if there is no scroll delta.
  scrollable_area->DidScroll(gfx::ScrollOffset(0, 2));
  web_view_helper.WebView()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                                 WebFloatSize(), 1.0f, 0);
  EXPECT_FALSE(client.WasFrameScrolled());
  EXPECT_FALSE(initial_scroll_state.was_scrolled_by_user);
  client.Reset();

  // Non zero page scale and scroll.
  scrollable_area->DidScroll(gfx::ScrollOffset(9, 15));
  web_view_helper.WebView()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                                 WebFloatSize(), 0.6f, 0);
  EXPECT_TRUE(client.WasFrameScrolled());
  EXPECT_TRUE(initial_scroll_state.was_scrolled_by_user);
  client.Reset();
  initial_scroll_state.was_scrolled_by_user = false;

  // Programmatic scroll.
  frame_impl->ExecuteScript(WebScriptSource("window.scrollTo(0, 20);"));
  EXPECT_TRUE(client.WasFrameScrolled());
  EXPECT_FALSE(initial_scroll_state.was_scrolled_by_user);
  client.Reset();

  // Programmatic scroll to same offset. No scroll event should be generated.
  frame_impl->ExecuteScript(WebScriptSource("window.scrollTo(0, 20);"));
  EXPECT_FALSE(client.WasFrameScrolled());
  EXPECT_FALSE(initial_scroll_state.was_scrolled_by_user);
  client.Reset();
}

TEST_P(ParameterizedWebFrameTest, FirstPartyForCookiesForRedirect) {
  String file_path = testing::WebTestDataPath("first_party.html");

  WebURL test_url(ToKURL("http://internal.test/first_party_redirect.html"));
  char redirect[] = "http://internal.test/first_party.html";
  WebURL redirect_url(ToKURL(redirect));
  WebURLResponse redirect_response;
  redirect_response.SetMIMEType("text/html");
  redirect_response.SetHTTPStatusCode(302);
  redirect_response.SetHTTPHeaderField("Location", redirect);
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      test_url, redirect_response, file_path);

  WebURLResponse final_response;
  final_response.SetMIMEType("text/html");
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      redirect_url, final_response, file_path);

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "first_party_redirect.html",
                                    true);
  EXPECT_TRUE(web_view_helper.WebView()
                  ->MainFrame()
                  ->GetDocument()
                  .FirstPartyForCookies() == redirect_url);
}

class TestNavigationPolicyWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  void DidNavigateWithinPage(const WebHistoryItem&,
                             WebHistoryCommitType,
                             bool) override {
    EXPECT_TRUE(false);
  }
};

TEST_P(ParameterizedWebFrameTest, SimulateFragmentAnchorMiddleClick) {
  RegisterMockedHttpURLLoad("fragment_middle_click.html");
  TestNavigationPolicyWebFrameClient client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fragment_middle_click.html",
                                    true, &client);

  Document* document =
      ToLocalFrame(web_view_helper.WebView()->GetPage()->MainFrame())
          ->GetDocument();
  KURL destination = document->Url();
  destination.SetFragmentIdentifier("test");

  MouseEventInit mouse_initializer;
  mouse_initializer.setView(document->domWindow());
  mouse_initializer.setButton(1);

  Event* event =
      MouseEvent::Create(nullptr, EventTypeNames::click, mouse_initializer);
  FrameLoadRequest frame_request(document, ResourceRequest(destination));
  frame_request.SetTriggeringEvent(event);
  ToLocalFrame(web_view_helper.WebView()->GetPage()->MainFrame())
      ->Loader()
      .Load(frame_request);
}

class TestNewWindowWebViewClient : public FrameTestHelpers::TestWebViewClient {
 public:
  virtual WebView* CreateView(WebLocalFrame*,
                              const WebURLRequest&,
                              const WebWindowFeatures&,
                              const WebString&,
                              WebNavigationPolicy,
                              bool) override {
    EXPECT_TRUE(false);
    return 0;
  }
};

class TestNewWindowWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  TestNewWindowWebFrameClient() : decide_policy_call_count_(0) {}

  WebNavigationPolicy DecidePolicyForNavigation(
      const NavigationPolicyInfo& info) override {
    decide_policy_call_count_++;
    return info.default_policy;
  }

  int DecidePolicyCallCount() const { return decide_policy_call_count_; }

 private:
  int decide_policy_call_count_;
};

TEST_P(ParameterizedWebFrameTest, ModifiedClickNewWindow) {
  RegisterMockedHttpURLLoad("ctrl_click.html");
  RegisterMockedHttpURLLoad("hello_world.html");
  TestNewWindowWebViewClient web_view_client;
  TestNewWindowWebFrameClient web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "ctrl_click.html", true,
                                    &web_frame_client, &web_view_client);

  Document* document =
      ToLocalFrame(web_view_helper.WebView()->GetPage()->MainFrame())
          ->GetDocument();
  KURL destination = ToKURL(base_url_ + "hello_world.html");

  // ctrl+click event
  MouseEventInit mouse_initializer;
  mouse_initializer.setView(document->domWindow());
  mouse_initializer.setButton(1);
  mouse_initializer.setCtrlKey(true);

  Event* event =
      MouseEvent::Create(nullptr, EventTypeNames::click, mouse_initializer);
  FrameLoadRequest frame_request(document, ResourceRequest(destination));
  frame_request.SetTriggeringEvent(event);
  UserGestureIndicator gesture(DocumentUserGestureToken::Create(document));
  ToLocalFrame(web_view_helper.WebView()->GetPage()->MainFrame())
      ->Loader()
      .Load(frame_request);
  FrameTestHelpers::PumpPendingRequestsForFrameToLoad(
      web_view_helper.WebView()->MainFrame());

  // decidePolicyForNavigation should be called both for the original request
  // and the ctrl+click.
  EXPECT_EQ(2, web_frame_client.DecidePolicyCallCount());
}

TEST_P(ParameterizedWebFrameTest, BackToReload) {
  RegisterMockedHttpURLLoad("fragment_middle_click.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fragment_middle_click.html",
                                    true);
  WebFrame* frame = web_view_helper.WebView()->MainFrame();
  const FrameLoader& main_frame_loader =
      web_view_helper.WebView()->MainFrameImpl()->GetFrame()->Loader();
  Persistent<HistoryItem> first_item =
      main_frame_loader.GetDocumentLoader()->GetHistoryItem();
  EXPECT_TRUE(first_item);

  RegisterMockedHttpURLLoad("white-1x1.png");
  FrameTestHelpers::LoadFrame(frame, base_url_ + "white-1x1.png");
  EXPECT_NE(first_item.Get(),
            main_frame_loader.GetDocumentLoader()->GetHistoryItem());

  FrameTestHelpers::LoadHistoryItem(frame, WebHistoryItem(first_item.Get()),
                                    kWebHistoryDifferentDocumentLoad,
                                    WebCachePolicy::kUseProtocolCachePolicy);
  EXPECT_EQ(first_item.Get(),
            main_frame_loader.GetDocumentLoader()->GetHistoryItem());

  FrameTestHelpers::ReloadFrame(frame);
  EXPECT_EQ(WebCachePolicy::kValidatingCacheData,
            frame->DataSource()->GetRequest().GetCachePolicy());
}

TEST_P(ParameterizedWebFrameTest, BackDuringChildFrameReload) {
  RegisterMockedHttpURLLoad("page_with_blank_iframe.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "page_with_blank_iframe.html",
                                    true);
  WebLocalFrame* main_frame = web_view_helper.WebView()->MainFrameImpl();
  const FrameLoader& main_frame_loader =
      web_view_helper.WebView()->MainFrameImpl()->GetFrame()->Loader();
  WebFrame* child_frame = main_frame->FirstChild();
  ASSERT_TRUE(child_frame);

  // Start a history navigation, then have a different frame commit a
  // navigation.  In this case, reload an about:blank frame, which will commit
  // synchronously.  After the history navigation completes, both the
  // appropriate document url and the current history item should reflect the
  // history navigation.
  RegisterMockedHttpURLLoad("white-1x1.png");
  WebHistoryItem item;
  item.Initialize();
  WebURL history_url(ToKURL(base_url_ + "white-1x1.png"));
  item.SetURLString(history_url.GetString());
  WebURLRequest request = main_frame->RequestFromHistoryItem(
      item, WebCachePolicy::kUseProtocolCachePolicy);
  main_frame->Load(request, WebFrameLoadType::kBackForward, item);

  FrameTestHelpers::ReloadFrame(child_frame);
  EXPECT_EQ(item.UrlString(), main_frame->GetDocument().Url().GetString());
  EXPECT_EQ(item.UrlString(), WebString(main_frame_loader.GetDocumentLoader()
                                            ->GetHistoryItem()
                                            ->UrlString()));
}

TEST_P(ParameterizedWebFrameTest, ReloadPost) {
  RegisterMockedHttpURLLoad("reload_post.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "reload_post.html", true);
  WebFrame* frame = web_view_helper.WebView()->MainFrame();

  FrameTestHelpers::LoadFrame(web_view_helper.WebView()->MainFrame(),
                              "javascript:document.forms[0].submit()");
  // Pump requests one more time after the javascript URL has executed to
  // trigger the actual POST load request.
  FrameTestHelpers::PumpPendingRequestsForFrameToLoad(
      web_view_helper.WebView()->MainFrame());
  EXPECT_EQ(WebString::FromUTF8("POST"),
            frame->DataSource()->GetRequest().HttpMethod());

  FrameTestHelpers::ReloadFrame(frame);
  EXPECT_EQ(WebCachePolicy::kValidatingCacheData,
            frame->DataSource()->GetRequest().GetCachePolicy());
  EXPECT_EQ(kWebNavigationTypeFormResubmitted,
            frame->DataSource()->GetNavigationType());
}

TEST_P(ParameterizedWebFrameTest, LoadHistoryItemReload) {
  RegisterMockedHttpURLLoad("fragment_middle_click.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "fragment_middle_click.html",
                                    true);
  WebFrame* frame = web_view_helper.WebView()->MainFrame();
  const FrameLoader& main_frame_loader =
      web_view_helper.WebView()->MainFrameImpl()->GetFrame()->Loader();
  Persistent<HistoryItem> first_item =
      main_frame_loader.GetDocumentLoader()->GetHistoryItem();
  EXPECT_TRUE(first_item);

  RegisterMockedHttpURLLoad("white-1x1.png");
  FrameTestHelpers::LoadFrame(frame, base_url_ + "white-1x1.png");
  EXPECT_NE(first_item.Get(),
            main_frame_loader.GetDocumentLoader()->GetHistoryItem());

  // Cache policy overrides should take.
  FrameTestHelpers::LoadHistoryItem(frame, WebHistoryItem(first_item),
                                    kWebHistoryDifferentDocumentLoad,
                                    WebCachePolicy::kValidatingCacheData);
  EXPECT_EQ(first_item.Get(),
            main_frame_loader.GetDocumentLoader()->GetHistoryItem());
  EXPECT_EQ(WebCachePolicy::kValidatingCacheData,
            frame->DataSource()->GetRequest().GetCachePolicy());
}

class TestCachePolicyWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  explicit TestCachePolicyWebFrameClient(
      TestCachePolicyWebFrameClient* parent_client)
      : parent_client_(parent_client),
        policy_(WebCachePolicy::kUseProtocolCachePolicy),
        child_client_(0),
        will_send_request_call_count_(0),
        child_frame_creation_count_(0) {}

  void SetChildWebFrameClient(TestCachePolicyWebFrameClient* client) {
    child_client_ = client;
  }
  WebCachePolicy GetCachePolicy() const { return policy_; }
  int WillSendRequestCallCount() const { return will_send_request_call_count_; }
  int ChildFrameCreationCount() const { return child_frame_creation_count_; }

  WebLocalFrame* CreateChildFrame(
      WebLocalFrame* parent,
      WebTreeScopeType scope,
      const WebString&,
      const WebString&,
      WebSandboxFlags,
      const WebParsedFeaturePolicy&,
      const WebFrameOwnerProperties& frame_owner_properties) override {
    DCHECK(child_client_);
    child_frame_creation_count_++;
    WebLocalFrame* frame =
        WebLocalFrame::Create(scope, child_client_, nullptr, nullptr);
    parent->AppendChild(frame);
    return frame;
  }

  virtual void DidStartLoading(bool to_different_document) {
    if (parent_client_) {
      parent_client_->DidStartLoading(to_different_document);
      return;
    }
    TestWebFrameClient::DidStartLoading(to_different_document);
  }

  virtual void DidStopLoading() {
    if (parent_client_) {
      parent_client_->DidStopLoading();
      return;
    }
    TestWebFrameClient::DidStopLoading();
  }

  void WillSendRequest(WebURLRequest& request) override {
    policy_ = request.GetCachePolicy();
    will_send_request_call_count_++;
  }

 private:
  TestCachePolicyWebFrameClient* parent_client_;

  WebCachePolicy policy_;
  TestCachePolicyWebFrameClient* child_client_;
  int will_send_request_call_count_;
  int child_frame_creation_count_;
};

TEST_P(ParameterizedWebFrameTest, ReloadIframe) {
  RegisterMockedHttpURLLoad("iframe_reload.html");
  RegisterMockedHttpURLLoad("visible_iframe.html");
  TestCachePolicyWebFrameClient main_client(0);
  TestCachePolicyWebFrameClient child_client(&main_client);
  main_client.SetChildWebFrameClient(&child_client);

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "iframe_reload.html", true,
                                    &main_client);

  WebLocalFrameImpl* main_frame = web_view_helper.WebView()->MainFrameImpl();
  WebLocalFrameImpl* child_frame =
      ToWebLocalFrameImpl(main_frame->FirstChild());
  ASSERT_EQ(child_frame->Client(), &child_client);
  EXPECT_EQ(main_client.ChildFrameCreationCount(), 1);
  EXPECT_EQ(child_client.WillSendRequestCallCount(), 1);
  EXPECT_EQ(child_client.GetCachePolicy(),
            WebCachePolicy::kUseProtocolCachePolicy);

  FrameTestHelpers::ReloadFrame(main_frame);

  // A new WebFrame should have been created, but the child WebFrameClient
  // should be reused.
  ASSERT_NE(child_frame, ToWebLocalFrameImpl(main_frame->FirstChild()));
  ASSERT_EQ(ToWebLocalFrameImpl(main_frame->FirstChild())->Client(),
            &child_client);

  EXPECT_EQ(main_client.ChildFrameCreationCount(), 2);
  EXPECT_EQ(child_client.WillSendRequestCallCount(), 2);

  // Sub-frames should not be forcibly revalidated.
  // TODO(toyoshim): Will consider to revalidate main resources in sub-frames
  // on reloads. Or will do only for bypassingCache.
  EXPECT_EQ(child_client.GetCachePolicy(),
            WebCachePolicy::kUseProtocolCachePolicy);
}

class TestSameDocumentWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  TestSameDocumentWebFrameClient() : frame_load_type_reload_seen_(false) {}

  virtual void WillSendRequest(WebURLRequest&) {
    FrameLoader& frame_loader =
        ToWebLocalFrameImpl(Frame())->GetFrame()->Loader();
    if (frame_loader.ProvisionalDocumentLoader()->LoadType() ==
        kFrameLoadTypeReload)
      frame_load_type_reload_seen_ = true;
  }

  bool FrameLoadTypeReloadSeen() const { return frame_load_type_reload_seen_; }

 private:
  bool frame_load_type_reload_seen_;
};

TEST_P(ParameterizedWebFrameTest, NavigateToSame) {
  RegisterMockedHttpURLLoad("navigate_to_same.html");
  TestSameDocumentWebFrameClient client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "navigate_to_same.html", true,
                                    &client);
  EXPECT_FALSE(client.FrameLoadTypeReloadSeen());

  FrameLoadRequest frame_request(
      0, ResourceRequest(
             ToLocalFrame(web_view_helper.WebView()->GetPage()->MainFrame())
                 ->GetDocument()
                 ->Url()));
  ToLocalFrame(web_view_helper.WebView()->GetPage()->MainFrame())
      ->Loader()
      .Load(frame_request);
  FrameTestHelpers::PumpPendingRequestsForFrameToLoad(
      web_view_helper.WebView()->MainFrame());

  EXPECT_TRUE(client.FrameLoadTypeReloadSeen());
}

class TestSameDocumentWithImageWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  TestSameDocumentWithImageWebFrameClient() : num_of_image_requests_(0) {}

  virtual void WillSendRequest(WebURLRequest& request) {
    if (request.GetRequestContext() == WebURLRequest::kRequestContextImage) {
      num_of_image_requests_++;
      EXPECT_EQ(WebCachePolicy::kUseProtocolCachePolicy,
                request.GetCachePolicy());
    }
  }

  int NumOfImageRequests() const { return num_of_image_requests_; }

 private:
  int num_of_image_requests_;
};

TEST_P(ParameterizedWebFrameTest,
       NavigateToSameNoConditionalRequestForSubresource) {
  RegisterMockedHttpURLLoad("foo_with_image.html");
  RegisterMockedHttpURLLoad("white-1x1.png");
  TestSameDocumentWithImageWebFrameClient client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo_with_image.html", true,
                                    &client, nullptr, nullptr,
                                    &ConfigureLoadsImagesAutomatically);

  WebCache::Clear();
  FrameTestHelpers::LoadFrame(web_view_helper.WebView()->MainFrame(),
                              base_url_ + "foo_with_image.html");

  // 2 images are requested, and each triggers 2 willSendRequest() calls,
  // once for preloading and once for the real request.
  EXPECT_EQ(client.NumOfImageRequests(), 4);
}

TEST_P(ParameterizedWebFrameTest, WebNodeImageContents) {
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank", true);
  WebFrame* frame = web_view_helper.WebView()->MainFrame();

  static const char kBluePNG[] =
      "<img "
      "src=\"data:image/"
      "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAoAAAAKCAYAAACNMs+"
      "9AAAAGElEQVQYV2NkYPj/n4EIwDiqEF8oUT94AFIQE/cCn90IAAAAAElFTkSuQmCC\">";

  // Load up the image and test that we can extract the contents.
  KURL test_url = ToKURL("about:blank");
  FrameTestHelpers::LoadHTMLString(frame, kBluePNG, test_url);

  WebNode node = frame->GetDocument().Body().FirstChild();
  EXPECT_TRUE(node.IsElementNode());
  WebElement element = node.To<WebElement>();
  WebImage image = element.ImageContents();
  ASSERT_FALSE(image.IsNull());
  EXPECT_EQ(image.Size().width, 10);
  EXPECT_EQ(image.Size().height, 10);
  SkBitmap bitmap = image.GetSkBitmap();
  EXPECT_EQ(bitmap.getColor(0, 0), SK_ColorBLUE);
}

class TestStartStopCallbackWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  TestStartStopCallbackWebFrameClient()
      : start_loading_count_(0),
        stop_loading_count_(0),
        different_document_start_count_(0) {}

  void DidStartLoading(bool to_different_document) override {
    TestWebFrameClient::DidStartLoading(to_different_document);
    start_loading_count_++;
    if (to_different_document)
      different_document_start_count_++;
  }

  void DidStopLoading() override {
    TestWebFrameClient::DidStopLoading();
    stop_loading_count_++;
  }

  int StartLoadingCount() const { return start_loading_count_; }
  int StopLoadingCount() const { return stop_loading_count_; }
  int DifferentDocumentStartCount() const {
    return different_document_start_count_;
  }

 private:
  int start_loading_count_;
  int stop_loading_count_;
  int different_document_start_count_;
};

TEST_P(ParameterizedWebFrameTest, PushStateStartsAndStops) {
  RegisterMockedHttpURLLoad("push_state.html");
  TestStartStopCallbackWebFrameClient client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "push_state.html", true,
                                    &client);

  EXPECT_EQ(client.StartLoadingCount(), 2);
  EXPECT_EQ(client.StopLoadingCount(), 2);
  EXPECT_EQ(client.DifferentDocumentStartCount(), 1);
}

class TestDidNavigateCommitTypeWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  TestDidNavigateCommitTypeWebFrameClient()
      : last_commit_type_(kWebHistoryInertCommit) {}

  void DidNavigateWithinPage(const WebHistoryItem&,
                             WebHistoryCommitType type,
                             bool) override {
    last_commit_type_ = type;
  }

  WebHistoryCommitType LastCommitType() const { return last_commit_type_; }

 private:
  WebHistoryCommitType last_commit_type_;
};

TEST_P(ParameterizedWebFrameTest, SameDocumentHistoryNavigationCommitType) {
  RegisterMockedHttpURLLoad("push_state.html");
  TestDidNavigateCommitTypeWebFrameClient client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "push_state.html", true, &client);
  Persistent<HistoryItem> item =
      ToLocalFrame(web_view_impl->GetPage()->MainFrame())
          ->Loader()
          .GetDocumentLoader()
          ->GetHistoryItem();
  RunPendingTasks();

  ToLocalFrame(web_view_impl->GetPage()->MainFrame())
      ->Loader()
      .Load(FrameLoadRequest(nullptr,
                             item->GenerateResourceRequest(
                                 WebCachePolicy::kUseProtocolCachePolicy)),
            kFrameLoadTypeBackForward, item.Get(), kHistorySameDocumentLoad);
  EXPECT_EQ(kWebBackForwardCommit, client.LastCommitType());
}

class TestHistoryWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
 public:
  TestHistoryWebFrameClient() { replaces_current_history_item_ = false; }

  void DidStartProvisionalLoad(WebDataSource* data_source,
                               WebURLRequest& request) {
    replaces_current_history_item_ = data_source->ReplacesCurrentHistoryItem();
  }

  bool ReplacesCurrentHistoryItem() { return replaces_current_history_item_; }

 private:
  bool replaces_current_history_item_;
};

// Tests that the first navigation in an initially blank subframe will result in
// a history entry being replaced and not a new one being added.
TEST_P(ParameterizedWebFrameTest, FirstBlankSubframeNavigation) {
  RegisterMockedHttpURLLoad("history.html");
  RegisterMockedHttpURLLoad("find.html");

  TestHistoryWebFrameClient client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank", true, &client);

  WebFrame* frame = web_view_helper.WebView()->MainFrame();

  frame->ExecuteScript(WebScriptSource(WebString::FromUTF8(
      "document.body.appendChild(document.createElement('iframe'))")));

  WebFrame* iframe = frame->FirstChild();
  ASSERT_EQ(&client, ToWebLocalFrameImpl(iframe)->Client());

  std::string url1 = base_url_ + "history.html";
  FrameTestHelpers::LoadFrame(iframe, url1);
  EXPECT_EQ(url1, iframe->GetDocument().Url().GetString().Utf8());
  EXPECT_TRUE(client.ReplacesCurrentHistoryItem());

  std::string url2 = base_url_ + "find.html";
  FrameTestHelpers::LoadFrame(iframe, url2);
  EXPECT_EQ(url2, iframe->GetDocument().Url().GetString().Utf8());
  EXPECT_FALSE(client.ReplacesCurrentHistoryItem());
}

// Tests that a navigation in a frame with a non-blank initial URL will create
// a new history item, unlike the case above.
TEST_P(ParameterizedWebFrameTest, FirstNonBlankSubframeNavigation) {
  RegisterMockedHttpURLLoad("history.html");
  RegisterMockedHttpURLLoad("find.html");

  TestHistoryWebFrameClient client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank", true, &client);

  WebFrame* frame = web_view_helper.WebView()->MainFrame();

  std::string url1 = base_url_ + "history.html";
  FrameTestHelpers::LoadFrame(
      frame,
      "javascript:var f = document.createElement('iframe'); "
      "f.src = '" +
          url1 +
          "';"
          "document.body.appendChild(f)");

  WebFrame* iframe = frame->FirstChild();
  EXPECT_EQ(url1, iframe->GetDocument().Url().GetString().Utf8());

  std::string url2 = base_url_ + "find.html";
  FrameTestHelpers::LoadFrame(iframe, url2);
  EXPECT_EQ(url2, iframe->GetDocument().Url().GetString().Utf8());
  EXPECT_FALSE(client.ReplacesCurrentHistoryItem());
}

// Test verifies that layout will change a layer's scrollable attibutes
TEST_F(WebFrameTest, overflowHiddenRewrite) {
  RegisterMockedHttpURLLoad("non-scrollable.html");
  std::unique_ptr<FakeCompositingWebViewClient>
      fake_compositing_web_view_client =
          WTF::MakeUnique<FakeCompositingWebViewClient>();
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true, nullptr,
                             fake_compositing_web_view_client.get(), nullptr,
                             &ConfigureCompositingWebView);

  web_view_helper.Resize(WebSize(100, 100));
  FrameTestHelpers::LoadFrame(web_view_helper.WebView()->MainFrame(),
                              base_url_ + "non-scrollable.html");

  PaintLayerCompositor* compositor = web_view_helper.WebView()->Compositor();
  ASSERT_TRUE(compositor->ScrollLayer());

  // Verify that the WebLayer is not scrollable initially.
  GraphicsLayer* scroll_layer = compositor->ScrollLayer();
  WebLayer* web_scroll_layer = scroll_layer->PlatformLayer();
  ASSERT_FALSE(web_scroll_layer->UserScrollableHorizontal());
  ASSERT_FALSE(web_scroll_layer->UserScrollableVertical());

  // Call javascript to make the layer scrollable, and verify it.
  WebLocalFrameImpl* frame =
      (WebLocalFrameImpl*)web_view_helper.WebView()->MainFrame();
  frame->ExecuteScript(WebScriptSource("allowScroll();"));
  web_view_helper.WebView()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(web_scroll_layer->UserScrollableHorizontal());
  ASSERT_TRUE(web_scroll_layer->UserScrollableVertical());
}

// Test that currentHistoryItem reflects the current page, not the provisional
// load.
TEST_P(ParameterizedWebFrameTest, CurrentHistoryItem) {
  RegisterMockedHttpURLLoad("fixed_layout.html");
  std::string url = base_url_ + "fixed_layout.html";

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize();
  WebFrame* frame = web_view_helper.WebView()->MainFrame();
  const FrameLoader& main_frame_loader =
      web_view_helper.WebView()->MainFrameImpl()->GetFrame()->Loader();
  WebURLRequest request(ToKURL(url));
  frame->LoadRequest(request);

  // Before commit, there is no history item.
  EXPECT_FALSE(main_frame_loader.GetDocumentLoader()->GetHistoryItem());

  FrameTestHelpers::PumpPendingRequestsForFrameToLoad(frame);

  // After commit, there is.
  HistoryItem* item = main_frame_loader.GetDocumentLoader()->GetHistoryItem();
  ASSERT_TRUE(item);
  EXPECT_EQ(WTF::String(url.data()), item->UrlString());
}

class FailCreateChildFrame : public FrameTestHelpers::TestWebFrameClient {
 public:
  FailCreateChildFrame() : call_count_(0) {}

  WebLocalFrame* CreateChildFrame(
      WebLocalFrame* parent,
      WebTreeScopeType scope,
      const WebString& name,
      const WebString& fallback_name,
      WebSandboxFlags sandbox_flags,
      const WebParsedFeaturePolicy& container_policy,
      const WebFrameOwnerProperties& frame_owner_properties) override {
    ++call_count_;
    return nullptr;
  }

  int CallCount() const { return call_count_; }

 private:
  int call_count_;
};

// Test that we don't crash if WebFrameClient::createChildFrame() fails.
TEST_P(ParameterizedWebFrameTest, CreateChildFrameFailure) {
  RegisterMockedHttpURLLoad("create_child_frame_fail.html");
  FailCreateChildFrame client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "create_child_frame_fail.html",
                                    true, &client);

  EXPECT_EQ(1, client.CallCount());
}

TEST_P(ParameterizedWebFrameTest, fixedPositionInFixedViewport) {
  RegisterMockedHttpURLLoad("fixed-position-in-fixed-viewport.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "fixed-position-in-fixed-viewport.html", true, nullptr,
      nullptr, nullptr, EnableViewportSettings);

  WebViewImpl* web_view = web_view_helper.WebView();
  web_view_helper.Resize(WebSize(100, 100));

  Document* document = web_view->MainFrameImpl()->GetFrame()->GetDocument();
  Element* bottom_fixed = document->GetElementById("bottom-fixed");
  Element* top_bottom_fixed = document->GetElementById("top-bottom-fixed");
  Element* right_fixed = document->GetElementById("right-fixed");
  Element* left_right_fixed = document->GetElementById("left-right-fixed");

  // The layout viewport will hit the min-scale limit of 0.25, so it'll be
  // 400x800.
  web_view_helper.Resize(WebSize(100, 200));
  EXPECT_EQ(800, bottom_fixed->OffsetTop() + bottom_fixed->OffsetHeight());
  EXPECT_EQ(800, top_bottom_fixed->OffsetHeight());

  // Now the layout viewport hits the content width limit of 500px so it'll be
  // 500x500.
  web_view_helper.Resize(WebSize(200, 200));
  EXPECT_EQ(500, right_fixed->OffsetLeft() + right_fixed->OffsetWidth());
  EXPECT_EQ(500, left_right_fixed->OffsetWidth());
}

TEST_P(ParameterizedWebFrameTest, FrameViewMoveWithSetFrameRect) {
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank");
  web_view_helper.Resize(WebSize(200, 200));
  web_view_helper.WebView()->UpdateAllLifecyclePhases();

  FrameView* frame_view =
      web_view_helper.WebView()->MainFrameImpl()->GetFrameView();
  EXPECT_RECT_EQ(IntRect(0, 0, 200, 200), frame_view->FrameRect());
  frame_view->SetFrameRect(IntRect(100, 100, 200, 200));
  EXPECT_RECT_EQ(IntRect(100, 100, 200, 200), frame_view->FrameRect());
}

TEST_F(WebFrameTest, FrameViewScrollAccountsForBrowserControls) {
  FakeCompositingWebViewClient client;
  RegisterMockedHttpURLLoad("long_scroll.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "long_scroll.html", true,
                                    nullptr, &client, nullptr,
                                    ConfigureAndroid);

  WebViewImpl* web_view = web_view_helper.WebView();
  FrameView* frame_view =
      web_view_helper.WebView()->MainFrameImpl()->GetFrameView();

  float browser_controls_height = 40;
  web_view->ResizeWithBrowserControls(WebSize(100, 100),
                                      browser_controls_height, false);
  web_view->SetPageScaleFactor(2.0f);
  web_view->UpdateAllLifecyclePhases();

  web_view->MainFrame()->SetScrollOffset(WebSize(0, 2000));
  EXPECT_SIZE_EQ(ScrollOffset(0, 1900), frame_view->GetScrollOffset());

  // Simulate the browser controls showing by 20px, thus shrinking the viewport
  // and allowing it to scroll an additional 20px.
  web_view->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(),
                                1.0f, 20.0f / browser_controls_height);
  EXPECT_SIZE_EQ(ScrollOffset(0, 1920), frame_view->MaximumScrollOffset());

  // Show more, make sure the scroll actually gets clamped.
  web_view->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(),
                                1.0f, 20.0f / browser_controls_height);
  web_view->MainFrame()->SetScrollOffset(WebSize(0, 2000));
  EXPECT_SIZE_EQ(ScrollOffset(0, 1940), frame_view->GetScrollOffset());

  // Hide until there's 10px showing.
  web_view->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(),
                                1.0f, -30.0f / browser_controls_height);
  EXPECT_SIZE_EQ(ScrollOffset(0, 1910), frame_view->MaximumScrollOffset());

  // Simulate a LayoutPart::resize. The frame is resized to accomodate
  // the browser controls and Blink's view of the browser controls matches that
  // of the CC
  web_view->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(),
                                1.0f, 30.0f / browser_controls_height);
  web_view->ResizeWithBrowserControls(WebSize(100, 60), 40.0f, true);
  web_view->UpdateAllLifecyclePhases();
  EXPECT_SIZE_EQ(ScrollOffset(0, 1940), frame_view->MaximumScrollOffset());

  // Now simulate hiding.
  web_view->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(),
                                1.0f, -10.0f / browser_controls_height);
  EXPECT_SIZE_EQ(ScrollOffset(0, 1930), frame_view->MaximumScrollOffset());

  // Reset to original state: 100px widget height, browser controls fully
  // hidden.
  web_view->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(),
                                1.0f, -30.0f / browser_controls_height);
  web_view->ResizeWithBrowserControls(WebSize(100, 100),
                                      browser_controls_height, false);
  web_view->UpdateAllLifecyclePhases();
  EXPECT_SIZE_EQ(ScrollOffset(0, 1900), frame_view->MaximumScrollOffset());

  // Show the browser controls by just 1px, since we're zoomed in to 2X, that
  // should allow an extra 0.5px of scrolling in the visual viewport. Make
  // sure we're not losing any pixels when applying the adjustment on the
  // main frame.
  web_view->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(),
                                1.0f, 1.0f / browser_controls_height);
  EXPECT_SIZE_EQ(ScrollOffset(0, 1901), frame_view->MaximumScrollOffset());

  web_view->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(),
                                1.0f, 2.0f / browser_controls_height);
  EXPECT_SIZE_EQ(ScrollOffset(0, 1903), frame_view->MaximumScrollOffset());
}

TEST_F(WebFrameTest, MaximumScrollPositionCanBeNegative) {
  RegisterMockedHttpURLLoad("rtl-overview-mode.html");

  FixedLayoutTestWebViewClient client;
  client.screen_info_.device_scale_factor = 1;
  int viewport_width = 640;
  int viewport_height = 480;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "rtl-overview-mode.html", true,
                                    nullptr, &client, nullptr,
                                    EnableViewportSettings);
  web_view_helper.WebView()->SetInitialPageScaleOverride(-1);
  web_view_helper.WebView()->GetSettings()->SetWideViewportQuirkEnabled(true);
  web_view_helper.WebView()->GetSettings()->SetLoadWithOverviewMode(true);
  web_view_helper.WebView()->GetSettings()->SetUseWideViewport(true);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));
  web_view_helper.WebView()->UpdateAllLifecyclePhases();

  FrameView* frame_view =
      web_view_helper.WebView()->MainFrameImpl()->GetFrameView();
  EXPECT_LT(frame_view->MaximumScrollOffset().Width(), 0);
}

TEST_P(ParameterizedWebFrameTest, FullscreenLayerSize) {
  FakeCompositingWebViewClient client;
  RegisterMockedHttpURLLoad("fullscreen_div.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  int viewport_width = 640;
  int viewport_height = 480;
  client.screen_info_.rect.width = viewport_width;
  client.screen_info_.rect.height = viewport_height;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "fullscreen_div.html", true, nullptr, &client, nullptr,
      ConfigureAndroid);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));
  web_view_impl->UpdateAllLifecyclePhases();

  Document* document =
      web_view_impl->MainFrameImpl()->GetFrame()->GetDocument();
  UserGestureIndicator gesture(DocumentUserGestureToken::Create(document));
  Element* div_fullscreen = document->GetElementById("div1");
  Fullscreen::RequestFullscreen(*div_fullscreen);
  EXPECT_EQ(nullptr, Fullscreen::CurrentFullScreenElementFrom(*document));
  EXPECT_EQ(div_fullscreen, Fullscreen::FullscreenElementFrom(*document));
  web_view_impl->DidEnterFullscreen();
  EXPECT_EQ(div_fullscreen,
            Fullscreen::CurrentFullScreenElementFrom(*document));
  EXPECT_EQ(div_fullscreen, Fullscreen::FullscreenElementFrom(*document));
  web_view_impl->UpdateAllLifecyclePhases();
  EXPECT_EQ(div_fullscreen,
            Fullscreen::CurrentFullScreenElementFrom(*document));
  EXPECT_EQ(div_fullscreen, Fullscreen::FullscreenElementFrom(*document));

  // Verify that the element is sized to the viewport.
  LayoutFullScreen* fullscreen_layout_object =
      Fullscreen::From(*document).FullScreenLayoutObject();
  EXPECT_EQ(viewport_width, fullscreen_layout_object->LogicalWidth().ToInt());
  EXPECT_EQ(viewport_height, fullscreen_layout_object->LogicalHeight().ToInt());

  // Verify it's updated after a device rotation.
  client.screen_info_.rect.width = viewport_height;
  client.screen_info_.rect.height = viewport_width;
  web_view_helper.Resize(WebSize(viewport_height, viewport_width));
  web_view_impl->UpdateAllLifecyclePhases();
  EXPECT_EQ(viewport_height, fullscreen_layout_object->LogicalWidth().ToInt());
  EXPECT_EQ(viewport_width, fullscreen_layout_object->LogicalHeight().ToInt());
}

TEST_F(WebFrameTest, FullscreenLayerNonScrollable) {
  FakeCompositingWebViewClient client;
  RegisterMockedHttpURLLoad("fullscreen_div.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  int viewport_width = 640;
  int viewport_height = 480;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "fullscreen_div.html", true, nullptr, &client, nullptr,
      ConfigureAndroid);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));
  web_view_impl->UpdateAllLifecyclePhases();

  Document* document =
      web_view_impl->MainFrameImpl()->GetFrame()->GetDocument();
  UserGestureIndicator gesture(DocumentUserGestureToken::Create(document));
  Element* div_fullscreen = document->GetElementById("div1");
  Fullscreen::RequestFullscreen(*div_fullscreen);
  EXPECT_EQ(nullptr, Fullscreen::CurrentFullScreenElementFrom(*document));
  EXPECT_EQ(div_fullscreen, Fullscreen::FullscreenElementFrom(*document));
  web_view_impl->DidEnterFullscreen();
  EXPECT_EQ(div_fullscreen,
            Fullscreen::CurrentFullScreenElementFrom(*document));
  EXPECT_EQ(div_fullscreen, Fullscreen::FullscreenElementFrom(*document));
  web_view_impl->UpdateAllLifecyclePhases();
  EXPECT_EQ(div_fullscreen,
            Fullscreen::CurrentFullScreenElementFrom(*document));
  EXPECT_EQ(div_fullscreen, Fullscreen::FullscreenElementFrom(*document));

  // Verify that the viewports are nonscrollable.
  FrameView* frame_view =
      web_view_helper.WebView()->MainFrameImpl()->GetFrameView();
  WebLayer* layout_viewport_scroll_layer =
      web_view_impl->Compositor()->ScrollLayer()->PlatformLayer();
  WebLayer* visual_viewport_scroll_layer =
      frame_view->GetPage()->GetVisualViewport().ScrollLayer()->PlatformLayer();
  ASSERT_FALSE(layout_viewport_scroll_layer->UserScrollableHorizontal());
  ASSERT_FALSE(layout_viewport_scroll_layer->UserScrollableVertical());
  ASSERT_FALSE(visual_viewport_scroll_layer->UserScrollableHorizontal());
  ASSERT_FALSE(visual_viewport_scroll_layer->UserScrollableVertical());

  // Verify that the viewports are scrollable upon exiting fullscreen.
  EXPECT_EQ(div_fullscreen,
            Fullscreen::CurrentFullScreenElementFrom(*document));
  EXPECT_EQ(div_fullscreen, Fullscreen::FullscreenElementFrom(*document));
  web_view_impl->DidExitFullscreen();
  EXPECT_EQ(nullptr, Fullscreen::CurrentFullScreenElementFrom(*document));
  EXPECT_EQ(nullptr, Fullscreen::FullscreenElementFrom(*document));
  web_view_impl->UpdateAllLifecyclePhases();
  EXPECT_EQ(nullptr, Fullscreen::CurrentFullScreenElementFrom(*document));
  EXPECT_EQ(nullptr, Fullscreen::FullscreenElementFrom(*document));
  ASSERT_TRUE(layout_viewport_scroll_layer->UserScrollableHorizontal());
  ASSERT_TRUE(layout_viewport_scroll_layer->UserScrollableVertical());
  ASSERT_TRUE(visual_viewport_scroll_layer->UserScrollableHorizontal());
  ASSERT_TRUE(visual_viewport_scroll_layer->UserScrollableVertical());
}

TEST_P(ParameterizedWebFrameTest, FullscreenMainFrame) {
  FakeCompositingWebViewClient client;
  RegisterMockedHttpURLLoad("fullscreen_div.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  int viewport_width = 640;
  int viewport_height = 480;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "fullscreen_div.html", true, nullptr, &client, nullptr,
      ConfigureAndroid);
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));
  web_view_impl->UpdateAllLifecyclePhases();

  WebLayer* web_scroll_layer = web_view_impl->MainFrameImpl()
                                   ->GetFrame()
                                   ->View()
                                   ->LayoutViewportScrollableArea()
                                   ->LayerForScrolling()
                                   ->PlatformLayer();
  ASSERT_TRUE(web_scroll_layer->Scrollable());
  ASSERT_TRUE(web_scroll_layer->UserScrollableHorizontal());
  ASSERT_TRUE(web_scroll_layer->UserScrollableVertical());

  Document* document =
      web_view_impl->MainFrameImpl()->GetFrame()->GetDocument();
  UserGestureIndicator gesture(DocumentUserGestureToken::Create(document));
  Fullscreen::RequestFullscreen(*document->documentElement());
  EXPECT_EQ(nullptr, Fullscreen::CurrentFullScreenElementFrom(*document));
  EXPECT_EQ(document->documentElement(),
            Fullscreen::FullscreenElementFrom(*document));
  web_view_impl->DidEnterFullscreen();
  EXPECT_EQ(document->documentElement(),
            Fullscreen::CurrentFullScreenElementFrom(*document));
  EXPECT_EQ(document->documentElement(),
            Fullscreen::FullscreenElementFrom(*document));

  web_view_impl->UpdateAllLifecyclePhases();
  EXPECT_EQ(document->documentElement(),
            Fullscreen::CurrentFullScreenElementFrom(*document));
  EXPECT_EQ(document->documentElement(),
            Fullscreen::FullscreenElementFrom(*document));

  // Verify that the main frame is still scrollable.
  web_scroll_layer = web_view_impl->MainFrameImpl()
                         ->GetFrame()
                         ->View()
                         ->LayoutViewportScrollableArea()
                         ->LayerForScrolling()
                         ->PlatformLayer();
  ASSERT_TRUE(web_scroll_layer->Scrollable());
  ASSERT_TRUE(web_scroll_layer->UserScrollableHorizontal());
  ASSERT_TRUE(web_scroll_layer->UserScrollableVertical());

  // Verify the main frame still behaves correctly after a resize.
  web_view_helper.Resize(WebSize(viewport_height, viewport_width));
  ASSERT_TRUE(web_scroll_layer->Scrollable());
  ASSERT_TRUE(web_scroll_layer->UserScrollableHorizontal());
  ASSERT_TRUE(web_scroll_layer->UserScrollableVertical());
}

TEST_P(ParameterizedWebFrameTest, FullscreenSubframe) {
  FakeCompositingWebViewClient client;
  RegisterMockedHttpURLLoad("fullscreen_iframe.html");
  RegisterMockedHttpURLLoad("fullscreen_div.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "fullscreen_iframe.html", true, nullptr, &client, nullptr,
      ConfigureAndroid);
  int viewport_width = 640;
  int viewport_height = 480;
  client.screen_info_.rect.width = viewport_width;
  client.screen_info_.rect.height = viewport_height;
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));
  web_view_impl->UpdateAllLifecyclePhases();

  Document* document =
      ToWebLocalFrameImpl(web_view_helper.WebView()->MainFrame()->FirstChild())
          ->GetFrame()
          ->GetDocument();
  UserGestureIndicator gesture(DocumentUserGestureToken::Create(document));
  Element* div_fullscreen = document->GetElementById("div1");
  Fullscreen::RequestFullscreen(*div_fullscreen);
  web_view_impl->DidEnterFullscreen();
  web_view_impl->UpdateAllLifecyclePhases();

  // Verify that the element is sized to the viewport.
  LayoutFullScreen* fullscreen_layout_object =
      Fullscreen::From(*document).FullScreenLayoutObject();
  EXPECT_EQ(viewport_width, fullscreen_layout_object->LogicalWidth().ToInt());
  EXPECT_EQ(viewport_height, fullscreen_layout_object->LogicalHeight().ToInt());

  // Verify it's updated after a device rotation.
  client.screen_info_.rect.width = viewport_height;
  client.screen_info_.rect.height = viewport_width;
  web_view_helper.Resize(WebSize(viewport_height, viewport_width));
  web_view_impl->UpdateAllLifecyclePhases();
  EXPECT_EQ(viewport_height, fullscreen_layout_object->LogicalWidth().ToInt());
  EXPECT_EQ(viewport_width, fullscreen_layout_object->LogicalHeight().ToInt());
}

// Tests entering nested fullscreen and then exiting via the same code path
// that's used when the browser process exits fullscreen.
TEST_P(ParameterizedWebFrameTest, FullscreenNestedExit) {
  RegisterMockedHttpURLLoad("fullscreen_iframe.html");
  RegisterMockedHttpURLLoad("fullscreen_div.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "fullscreen_iframe.html", true);

  web_view_impl->UpdateAllLifecyclePhases();

  Document* top_doc = web_view_impl->MainFrameImpl()->GetFrame()->GetDocument();
  Element* top_body = top_doc->body();

  HTMLIFrameElement* iframe =
      toHTMLIFrameElement(top_doc->QuerySelector("iframe"));
  Document* iframe_doc = iframe->contentDocument();
  Element* iframe_body = iframe_doc->body();

  {
    UserGestureIndicator gesture(DocumentUserGestureToken::Create(top_doc));
    Fullscreen::RequestFullscreen(*top_body);
  }
  web_view_impl->DidEnterFullscreen();
  web_view_impl->UpdateAllLifecyclePhases();

  {
    UserGestureIndicator gesture(DocumentUserGestureToken::Create(iframe_doc));
    Fullscreen::RequestFullscreen(*iframe_body);
  }
  web_view_impl->DidEnterFullscreen();
  web_view_impl->UpdateAllLifecyclePhases();

  // We are now in nested fullscreen, with both documents having a non-empty
  // fullscreen element stack.
  EXPECT_EQ(top_body, Fullscreen::CurrentFullScreenElementFrom(*top_doc));
  EXPECT_EQ(iframe, Fullscreen::FullscreenElementFrom(*top_doc));
  EXPECT_EQ(iframe_body, Fullscreen::CurrentFullScreenElementFrom(*iframe_doc));
  EXPECT_EQ(iframe_body, Fullscreen::FullscreenElementFrom(*iframe_doc));

  web_view_impl->DidExitFullscreen();
  web_view_impl->UpdateAllLifecyclePhases();

  // We should now have fully exited fullscreen.
  EXPECT_EQ(nullptr, Fullscreen::CurrentFullScreenElementFrom(*top_doc));
  EXPECT_EQ(nullptr, Fullscreen::FullscreenElementFrom(*top_doc));
  EXPECT_EQ(nullptr, Fullscreen::CurrentFullScreenElementFrom(*iframe_doc));
  EXPECT_EQ(nullptr, Fullscreen::FullscreenElementFrom(*iframe_doc));
}

TEST_P(ParameterizedWebFrameTest, FullscreenWithTinyViewport) {
  FakeCompositingWebViewClient client;
  RegisterMockedHttpURLLoad("viewport-tiny.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-tiny.html", true, nullptr, &client, nullptr,
      ConfigureAndroid);
  int viewport_width = 384;
  int viewport_height = 640;
  client.screen_info_.rect.width = viewport_width;
  client.screen_info_.rect.height = viewport_height;
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));
  web_view_impl->UpdateAllLifecyclePhases();

  LayoutViewItem layout_view_item = web_view_helper.WebView()
                                        ->MainFrameImpl()
                                        ->GetFrameView()
                                        ->GetLayoutViewItem();
  EXPECT_EQ(320, layout_view_item.LogicalWidth().Floor());
  EXPECT_EQ(533, layout_view_item.LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(1.2, web_view_impl->PageScaleFactor());
  EXPECT_FLOAT_EQ(1.2, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(5.0, web_view_impl->MaximumPageScaleFactor());

  Document* document =
      web_view_impl->MainFrameImpl()->GetFrame()->GetDocument();
  UserGestureIndicator gesture(DocumentUserGestureToken::Create(document));
  Fullscreen::RequestFullscreen(*document->documentElement());
  web_view_impl->DidEnterFullscreen();
  web_view_impl->UpdateAllLifecyclePhases();
  EXPECT_EQ(384, layout_view_item.LogicalWidth().Floor());
  EXPECT_EQ(640, layout_view_item.LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->PageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MaximumPageScaleFactor());

  web_view_impl->DidExitFullscreen();
  web_view_impl->UpdateAllLifecyclePhases();
  EXPECT_EQ(320, layout_view_item.LogicalWidth().Floor());
  EXPECT_EQ(533, layout_view_item.LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(1.2, web_view_impl->PageScaleFactor());
  EXPECT_FLOAT_EQ(1.2, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(5.0, web_view_impl->MaximumPageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, FullscreenResizeWithTinyViewport) {
  FakeCompositingWebViewClient client;
  RegisterMockedHttpURLLoad("viewport-tiny.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-tiny.html", true, nullptr, &client, nullptr,
      ConfigureAndroid);
  int viewport_width = 384;
  int viewport_height = 640;
  client.screen_info_.rect.width = viewport_width;
  client.screen_info_.rect.height = viewport_height;
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));
  web_view_impl->UpdateAllLifecyclePhases();

  LayoutViewItem layout_view_item = web_view_helper.WebView()
                                        ->MainFrameImpl()
                                        ->GetFrameView()
                                        ->GetLayoutViewItem();
  Document* document =
      web_view_impl->MainFrameImpl()->GetFrame()->GetDocument();
  UserGestureIndicator gesture(DocumentUserGestureToken::Create(document));
  Fullscreen::RequestFullscreen(*document->documentElement());
  web_view_impl->DidEnterFullscreen();
  web_view_impl->UpdateAllLifecyclePhases();
  EXPECT_EQ(384, layout_view_item.LogicalWidth().Floor());
  EXPECT_EQ(640, layout_view_item.LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->PageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MaximumPageScaleFactor());

  viewport_width = 640;
  viewport_height = 384;
  client.screen_info_.rect.width = viewport_width;
  client.screen_info_.rect.height = viewport_height;
  web_view_helper.Resize(WebSize(viewport_width, viewport_height));
  web_view_impl->UpdateAllLifecyclePhases();
  EXPECT_EQ(640, layout_view_item.LogicalWidth().Floor());
  EXPECT_EQ(384, layout_view_item.LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->PageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MaximumPageScaleFactor());

  web_view_impl->DidExitFullscreen();
  web_view_impl->UpdateAllLifecyclePhases();
  EXPECT_EQ(320, layout_view_item.LogicalWidth().Floor());
  EXPECT_EQ(192, layout_view_item.LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(2, web_view_impl->PageScaleFactor());
  EXPECT_FLOAT_EQ(2, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(5.0, web_view_impl->MaximumPageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, FullscreenRestoreScaleFactorUponExiting) {
  // The purpose of this test is to more precisely simulate the sequence of
  // resize and switching fullscreen state operations on WebView, with the
  // interference from Android status bars like a real device does.
  // This verifies we handle the transition and restore states correctly.
  WebSize screen_size_minus_status_bars_minus_url_bar(598, 303);
  WebSize screen_size_minus_status_bars(598, 359);
  WebSize screen_size(640, 384);

  FakeCompositingWebViewClient client;
  RegisterMockedHttpURLLoad("fullscreen_restore_scale_factor.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "fullscreen_restore_scale_factor.html", true, nullptr,
      &client, nullptr, &ConfigureAndroid);
  client.screen_info_.rect.width =
      screen_size_minus_status_bars_minus_url_bar.width;
  client.screen_info_.rect.height =
      screen_size_minus_status_bars_minus_url_bar.height;
  web_view_helper.Resize(screen_size_minus_status_bars_minus_url_bar);
  LayoutViewItem layout_view_item = web_view_helper.WebView()
                                        ->MainFrameImpl()
                                        ->GetFrameView()
                                        ->GetLayoutViewItem();
  EXPECT_EQ(screen_size_minus_status_bars_minus_url_bar.width,
            layout_view_item.LogicalWidth().Floor());
  EXPECT_EQ(screen_size_minus_status_bars_minus_url_bar.height,
            layout_view_item.LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->PageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(5.0, web_view_impl->MaximumPageScaleFactor());

  {
    Document* document =
        web_view_impl->MainFrameImpl()->GetFrame()->GetDocument();
    UserGestureIndicator gesture(DocumentUserGestureToken::Create(document));
    Fullscreen::RequestFullscreen(*document->body());
  }

  web_view_impl->DidEnterFullscreen();
  web_view_impl->UpdateAllLifecyclePhases();
  client.screen_info_.rect.width = screen_size_minus_status_bars.width;
  client.screen_info_.rect.height = screen_size_minus_status_bars.height;
  web_view_helper.Resize(screen_size_minus_status_bars);
  client.screen_info_.rect.width = screen_size.width;
  client.screen_info_.rect.height = screen_size.height;
  web_view_helper.Resize(screen_size);
  EXPECT_EQ(screen_size.width, layout_view_item.LogicalWidth().Floor());
  EXPECT_EQ(screen_size.height, layout_view_item.LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->PageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MaximumPageScaleFactor());

  web_view_impl->DidExitFullscreen();
  web_view_impl->UpdateAllLifecyclePhases();
  client.screen_info_.rect.width = screen_size_minus_status_bars.width;
  client.screen_info_.rect.height = screen_size_minus_status_bars.height;
  web_view_helper.Resize(screen_size_minus_status_bars);
  client.screen_info_.rect.width =
      screen_size_minus_status_bars_minus_url_bar.width;
  client.screen_info_.rect.height =
      screen_size_minus_status_bars_minus_url_bar.height;
  web_view_helper.Resize(screen_size_minus_status_bars_minus_url_bar);
  EXPECT_EQ(screen_size_minus_status_bars_minus_url_bar.width,
            layout_view_item.LogicalWidth().Floor());
  EXPECT_EQ(screen_size_minus_status_bars_minus_url_bar.height,
            layout_view_item.LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->PageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(5.0, web_view_impl->MaximumPageScaleFactor());
}

// Tests that leaving fullscreen by navigating to a new page resets the
// fullscreen page scale constraints.
TEST_P(ParameterizedWebFrameTest, ClearFullscreenConstraintsOnNavigation) {
  RegisterMockedHttpURLLoad("viewport-tiny.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  int viewport_width = 100;
  int viewport_height = 200;

  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "viewport-tiny.html", true, nullptr, nullptr, nullptr,
      ConfigureAndroid);

  web_view_helper.Resize(WebSize(viewport_width, viewport_height));
  web_view_impl->UpdateAllLifecyclePhases();

  // viewport-tiny.html specifies a 320px layout width.
  LayoutViewItem layout_view_item =
      web_view_impl->MainFrameImpl()->GetFrameView()->GetLayoutViewItem();
  EXPECT_EQ(320, layout_view_item.LogicalWidth().Floor());
  EXPECT_EQ(640, layout_view_item.LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(0.3125, web_view_impl->PageScaleFactor());
  EXPECT_FLOAT_EQ(0.3125, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(5.0, web_view_impl->MaximumPageScaleFactor());

  Document* document =
      web_view_impl->MainFrameImpl()->GetFrame()->GetDocument();
  UserGestureIndicator gesture(DocumentUserGestureToken::Create(
      document, UserGestureToken::kNewGesture));
  Fullscreen::RequestFullscreen(*document->documentElement());
  web_view_impl->DidEnterFullscreen();
  web_view_impl->UpdateAllLifecyclePhases();

  // Entering fullscreen causes layout size and page scale limits to be
  // overridden.
  EXPECT_EQ(100, layout_view_item.LogicalWidth().Floor());
  EXPECT_EQ(200, layout_view_item.LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->PageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, web_view_impl->MaximumPageScaleFactor());

  const char kSource[] = "<meta name=\"viewport\" content=\"width=200\">";

  // Load a new page before exiting fullscreen.
  KURL test_url = ToKURL("about:blank");
  WebFrame* frame = web_view_helper.WebView()->MainFrame();
  FrameTestHelpers::LoadHTMLString(frame, kSource, test_url);
  web_view_impl->DidExitFullscreen();
  web_view_impl->UpdateAllLifecyclePhases();

  // Make sure the new page's layout size and scale factor limits aren't
  // overridden.
  layout_view_item =
      web_view_impl->MainFrameImpl()->GetFrameView()->GetLayoutViewItem();
  EXPECT_EQ(200, layout_view_item.LogicalWidth().Floor());
  EXPECT_EQ(400, layout_view_item.LogicalHeight().Floor());
  EXPECT_FLOAT_EQ(0.5, web_view_impl->MinimumPageScaleFactor());
  EXPECT_FLOAT_EQ(5.0, web_view_impl->MaximumPageScaleFactor());
}

namespace {

class TestFullscreenWebLayerTreeView : public WebLayerTreeView {
 public:
  void SetBackgroundColor(blink::WebColor color) override {
    has_transparent_background = SkColorGetA(color) < SK_AlphaOPAQUE;
  }
  bool has_transparent_background = false;
};

class TestFullscreenWebViewClient : public FrameTestHelpers::TestWebViewClient {
 public:
  WebLayerTreeView* InitializeLayerTreeView() override {
    return &test_fullscreen_layer_tree_view;
  }
  TestFullscreenWebLayerTreeView test_fullscreen_layer_tree_view;
};

}  // anonymous namespace

TEST_P(ParameterizedWebFrameTest, OverlayFullscreenVideo) {
  RuntimeEnabledFeatures::setForceOverlayFullscreenVideoEnabled(true);
  RegisterMockedHttpURLLoad("fullscreen_video.html");
  TestFullscreenWebViewClient web_view_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "fullscreen_video.html", true, nullptr, &web_view_client);

  const TestFullscreenWebLayerTreeView& layer_tree_view =
      web_view_client.test_fullscreen_layer_tree_view;

  Document* document =
      web_view_impl->MainFrameImpl()->GetFrame()->GetDocument();
  UserGestureIndicator gesture(DocumentUserGestureToken::Create(document));
  HTMLVideoElement* video =
      toHTMLVideoElement(document->GetElementById("video"));
  EXPECT_TRUE(video->UsesOverlayFullscreenVideo());
  EXPECT_FALSE(video->IsFullscreen());
  EXPECT_FALSE(layer_tree_view.has_transparent_background);

  video->webkitEnterFullscreen();
  web_view_impl->DidEnterFullscreen();
  web_view_impl->UpdateAllLifecyclePhases();
  EXPECT_TRUE(video->IsFullscreen());
  EXPECT_TRUE(layer_tree_view.has_transparent_background);

  web_view_impl->DidExitFullscreen();
  web_view_impl->UpdateAllLifecyclePhases();
  EXPECT_FALSE(video->IsFullscreen());
  EXPECT_FALSE(layer_tree_view.has_transparent_background);
}

TEST_P(ParameterizedWebFrameTest, LayoutBlockPercentHeightDescendants) {
  RegisterMockedHttpURLLoad("percent-height-descendants.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ +
                                    "percent-height-descendants.html");

  WebViewImpl* web_view = web_view_helper.WebView();
  web_view_helper.Resize(WebSize(800, 800));
  web_view->UpdateAllLifecyclePhases();

  Document* document = web_view->MainFrameImpl()->GetFrame()->GetDocument();
  LayoutBlock* container =
      ToLayoutBlock(document->GetElementById("container")->GetLayoutObject());
  LayoutBox* percent_height_in_anonymous =
      ToLayoutBox(document->GetElementById("percent-height-in-anonymous")
                      ->GetLayoutObject());
  LayoutBox* percent_height_direct_child =
      ToLayoutBox(document->GetElementById("percent-height-direct-child")
                      ->GetLayoutObject());

  EXPECT_TRUE(
      container->HasPercentHeightDescendant(percent_height_in_anonymous));
  EXPECT_TRUE(
      container->HasPercentHeightDescendant(percent_height_direct_child));

  ASSERT_TRUE(container->PercentHeightDescendants());
  ASSERT_TRUE(container->HasPercentHeightDescendants());
  EXPECT_EQ(2U, container->PercentHeightDescendants()->size());
  EXPECT_TRUE(container->PercentHeightDescendants()->Contains(
      percent_height_in_anonymous));
  EXPECT_TRUE(container->PercentHeightDescendants()->Contains(
      percent_height_direct_child));

  LayoutBlock* anonymous_block = percent_height_in_anonymous->ContainingBlock();
  EXPECT_TRUE(anonymous_block->IsAnonymous());
  EXPECT_FALSE(anonymous_block->HasPercentHeightDescendants());
}

TEST_P(ParameterizedWebFrameTest, HasVisibleContentOnVisibleFrames) {
  RegisterMockedHttpURLLoad("visible_frames.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl =
      web_view_helper.InitializeAndLoad(base_url_ + "visible_frames.html");
  for (WebFrame* frame = web_view_impl->MainFrameImpl()->TraverseNext(); frame;
       frame = frame->TraverseNext()) {
    EXPECT_TRUE(frame->HasVisibleContent());
  }
}

TEST_P(ParameterizedWebFrameTest, HasVisibleContentOnHiddenFrames) {
  RegisterMockedHttpURLLoad("hidden_frames.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl =
      web_view_helper.InitializeAndLoad(base_url_ + "hidden_frames.html");
  for (WebFrame* frame = web_view_impl->MainFrameImpl()->TraverseNext(); frame;
       frame = frame->TraverseNext()) {
    EXPECT_FALSE(frame->HasVisibleContent());
  }
}

class ManifestChangeWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  ManifestChangeWebFrameClient() : manifest_change_count_(0) {}
  void DidChangeManifest() override { ++manifest_change_count_; }

  int ManifestChangeCount() { return manifest_change_count_; }

 private:
  int manifest_change_count_;
};

TEST_P(ParameterizedWebFrameTest, NotifyManifestChange) {
  RegisterMockedHttpURLLoad("link-manifest-change.html");

  ManifestChangeWebFrameClient web_frame_client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "link-manifest-change.html",
                                    true, &web_frame_client);

  EXPECT_EQ(14, web_frame_client.ManifestChangeCount());
}

static Resource* FetchManifest(Document* document, const KURL& url) {
  FetchParameters fetch_parameters =
      FetchParameters(ResourceRequest(url), FetchInitiatorInfo());
  fetch_parameters.SetRequestContext(WebURLRequest::kRequestContextManifest);

  return RawResource::FetchSynchronously(fetch_parameters, document->Fetcher());
}

TEST_P(ParameterizedWebFrameTest, ManifestFetch) {
  RegisterMockedHttpURLLoad("foo.html");
  RegisterMockedHttpURLLoad("link-manifest-fetch.json");

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");
  Document* document =
      web_view_helper.WebView()->MainFrameImpl()->GetFrame()->GetDocument();

  Resource* resource =
      FetchManifest(document, ToKURL(base_url_ + "link-manifest-fetch.json"));

  EXPECT_TRUE(resource->IsLoaded());
}

TEST_P(ParameterizedWebFrameTest, ManifestCSPFetchAllow) {
  RegisterMockedURLLoadFromBase(not_base_url_, "link-manifest-fetch.json");
  RegisterMockedHttpURLLoadWithCSP("foo.html", "manifest-src *");

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");
  Document* document =
      web_view_helper.WebView()->MainFrameImpl()->GetFrame()->GetDocument();

  Resource* resource = FetchManifest(
      document, ToKURL(not_base_url_ + "link-manifest-fetch.json"));

  EXPECT_TRUE(resource->IsLoaded());
}

TEST_P(ParameterizedWebFrameTest, ManifestCSPFetchSelf) {
  RegisterMockedURLLoadFromBase(not_base_url_, "link-manifest-fetch.json");
  RegisterMockedHttpURLLoadWithCSP("foo.html", "manifest-src 'self'");

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");
  Document* document =
      web_view_helper.WebView()->MainFrameImpl()->GetFrame()->GetDocument();

  Resource* resource = FetchManifest(
      document, ToKURL(not_base_url_ + "link-manifest-fetch.json"));

  // Fetching resource wasn't allowed.
  ASSERT_TRUE(resource);
  EXPECT_TRUE(resource->ErrorOccurred());
  EXPECT_TRUE(resource->GetResourceError().IsAccessCheck());
}

TEST_P(ParameterizedWebFrameTest, ManifestCSPFetchSelfReportOnly) {
  RegisterMockedURLLoadFromBase(not_base_url_, "link-manifest-fetch.json");
  RegisterMockedHttpURLLoadWithCSP("foo.html", "manifest-src 'self'",
                                   /* report only */ true);

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html");
  Document* document =
      web_view_helper.WebView()->MainFrameImpl()->GetFrame()->GetDocument();

  Resource* resource = FetchManifest(
      document, ToKURL(not_base_url_ + "link-manifest-fetch.json"));

  EXPECT_TRUE(resource->IsLoaded());
}

TEST_P(ParameterizedWebFrameTest, ReloadBypassingCache) {
  // Check that a reload bypassing cache on a frame will result in the cache
  // policy of the request being set to ReloadBypassingCache.
  RegisterMockedHttpURLLoad("foo.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html", true);
  WebFrame* frame = web_view_helper.WebView()->MainFrame();
  FrameTestHelpers::ReloadFrameBypassingCache(frame);
  EXPECT_EQ(WebCachePolicy::kBypassingCache,
            frame->DataSource()->GetRequest().GetCachePolicy());
}

static void NodeImageTestValidation(const IntSize& reference_bitmap_size,
                                    DragImage* drag_image) {
  // Prepare the reference bitmap.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(reference_bitmap_size.Width(),
                        reference_bitmap_size.Height());
  SkCanvas canvas(bitmap);
  canvas.drawColor(SK_ColorGREEN);

  EXPECT_EQ(reference_bitmap_size.Width(), drag_image->Size().Width());
  EXPECT_EQ(reference_bitmap_size.Height(), drag_image->Size().Height());
  const SkBitmap& drag_bitmap = drag_image->Bitmap();
  EXPECT_EQ(
      0, memcmp(bitmap.getPixels(), drag_bitmap.getPixels(), bitmap.getSize()));
}

TEST_P(ParameterizedWebFrameTest, NodeImageTestCSSTransformDescendant) {
  FrameTestHelpers::WebViewHelper web_view_helper;
  std::unique_ptr<DragImage> drag_image = NodeImageTestSetup(
      &web_view_helper, std::string("case-css-3dtransform-descendant"));
  EXPECT_TRUE(drag_image);

  NodeImageTestValidation(IntSize(40, 40), drag_image.get());
}

TEST_P(ParameterizedWebFrameTest, NodeImageTestCSSTransform) {
  FrameTestHelpers::WebViewHelper web_view_helper;
  std::unique_ptr<DragImage> drag_image =
      NodeImageTestSetup(&web_view_helper, std::string("case-css-transform"));
  EXPECT_TRUE(drag_image);

  NodeImageTestValidation(IntSize(40, 40), drag_image.get());
}

TEST_P(ParameterizedWebFrameTest, NodeImageTestCSS3DTransform) {
  FrameTestHelpers::WebViewHelper web_view_helper;
  std::unique_ptr<DragImage> drag_image =
      NodeImageTestSetup(&web_view_helper, std::string("case-css-3dtransform"));
  EXPECT_TRUE(drag_image);

  NodeImageTestValidation(IntSize(40, 40), drag_image.get());
}

TEST_P(ParameterizedWebFrameTest, NodeImageTestInlineBlock) {
  FrameTestHelpers::WebViewHelper web_view_helper;
  std::unique_ptr<DragImage> drag_image =
      NodeImageTestSetup(&web_view_helper, std::string("case-inlineblock"));
  EXPECT_TRUE(drag_image);

  NodeImageTestValidation(IntSize(40, 40), drag_image.get());
}

TEST_P(ParameterizedWebFrameTest, NodeImageTestFloatLeft) {
  FrameTestHelpers::WebViewHelper web_view_helper;
  std::unique_ptr<DragImage> drag_image = NodeImageTestSetup(
      &web_view_helper, std::string("case-float-left-overflow-hidden"));
  EXPECT_TRUE(drag_image);

  NodeImageTestValidation(IntSize(40, 40), drag_image.get());
}

// Crashes on Android: http://crbug.com/403804
#if OS(ANDROID)
TEST_P(ParameterizedWebFrameTest, DISABLED_PrintingBasic)
#else
TEST_P(ParameterizedWebFrameTest, PrintingBasic)
#endif
{
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("data:text/html,Hello, world.");

  WebFrame* frame = web_view_helper.WebView()->MainFrame();

  WebPrintParams print_params;
  print_params.print_content_area.width = 500;
  print_params.print_content_area.height = 500;

  int page_count = frame->PrintBegin(print_params);
  EXPECT_EQ(1, page_count);
  frame->PrintEnd();
}

class ThemeColorTestWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  ThemeColorTestWebFrameClient() : did_notify_(false) {}

  void Reset() { did_notify_ = false; }

  bool DidNotify() const { return did_notify_; }

 private:
  virtual void DidChangeThemeColor() { did_notify_ = true; }

  bool did_notify_;
};

TEST_P(ParameterizedWebFrameTest, ThemeColor) {
  RegisterMockedHttpURLLoad("theme_color_test.html");
  ThemeColorTestWebFrameClient client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "theme_color_test.html", true,
                                    &client);
  EXPECT_TRUE(client.DidNotify());
  WebLocalFrameImpl* frame = web_view_helper.WebView()->MainFrameImpl();
  EXPECT_EQ(0xff0000ff, frame->GetDocument().ThemeColor());
  // Change color by rgb.
  client.Reset();
  frame->ExecuteScript(
      WebScriptSource("document.getElementById('tc1').setAttribute('content', "
                      "'rgb(0, 0, 0)');"));
  EXPECT_TRUE(client.DidNotify());
  EXPECT_EQ(0xff000000, frame->GetDocument().ThemeColor());
  // Change color by hsl.
  client.Reset();
  frame->ExecuteScript(
      WebScriptSource("document.getElementById('tc1').setAttribute('content', "
                      "'hsl(240,100%, 50%)');"));
  EXPECT_TRUE(client.DidNotify());
  EXPECT_EQ(0xff0000ff, frame->GetDocument().ThemeColor());
  // Change of second theme-color meta tag will not change frame's theme
  // color.
  client.Reset();
  frame->ExecuteScript(WebScriptSource(
      "document.getElementById('tc2').setAttribute('content', '#00FF00');"));
  EXPECT_TRUE(client.DidNotify());
  EXPECT_EQ(0xff0000ff, frame->GetDocument().ThemeColor());
}

// Make sure that an embedder-triggered detach with a remote frame parent
// doesn't leave behind dangling pointers.
TEST_P(ParameterizedWebFrameTest, EmbedderTriggeredDetachWithRemoteMainFrame) {
  // FIXME: Refactor some of this logic into WebViewHelper to make it easier to
  // write tests with a top-level remote frame.
  FrameTestHelpers::TestWebViewClient view_client;
  FrameTestHelpers::TestWebRemoteFrameClient remote_client;
  WebView* view = WebView::Create(&view_client, kWebPageVisibilityStateVisible);
  view->SetMainFrame(remote_client.GetFrame());
  WebLocalFrame* child_frame =
      FrameTestHelpers::CreateLocalChild(view->MainFrame()->ToWebRemoteFrame());

  // Purposely keep the LocalFrame alive so it's the last thing to be destroyed.
  Persistent<Frame> child_core_frame = child_frame->ToImplBase()->GetFrame();
  view->Close();
  child_core_frame.Clear();
}

class WebFrameSwapTest : public WebFrameTest {
 protected:
  WebFrameSwapTest() {
    RegisterMockedHttpURLLoad("frame-a-b-c.html");
    RegisterMockedHttpURLLoad("subframe-a.html");
    RegisterMockedHttpURLLoad("subframe-b.html");
    RegisterMockedHttpURLLoad("subframe-c.html");
    RegisterMockedHttpURLLoad("subframe-hello.html");

    web_view_helper_.InitializeAndLoad(base_url_ + "frame-a-b-c.html", true);
  }

  void Reset() { web_view_helper_.Reset(); }
  WebFrame* MainFrame() const {
    return web_view_helper_.WebView()->MainFrame();
  }
  WebViewImpl* WebView() const { return web_view_helper_.WebView(); }

 private:
  FrameTestHelpers::WebViewHelper web_view_helper_;
};

TEST_F(WebFrameSwapTest, SwapMainFrame) {
  WebRemoteFrame* remote_frame =
      WebRemoteFrame::Create(WebTreeScopeType::kDocument, nullptr);
  MainFrame()->Swap(remote_frame);

  FrameTestHelpers::TestWebFrameClient client;
  WebLocalFrame* local_frame = WebLocalFrame::CreateProvisional(
      &client, nullptr, nullptr, remote_frame, WebSandboxFlags::kNone);
  FrameTestHelpers::TestWebWidgetClient web_widget_client;
  WebFrameWidget::Create(&web_widget_client, local_frame);
  remote_frame->Swap(local_frame);

  // Finally, make sure an embedder triggered load in the local frame swapped
  // back in works.
  FrameTestHelpers::LoadFrame(local_frame, base_url_ + "subframe-hello.html");

  std::string content =
      WebFrameContentDumper::DumpWebViewAsText(WebView(), 1024).Utf8();
  EXPECT_EQ("hello", content);

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  Reset();
  remote_frame->Close();
}

TEST_F(WebFrameSwapTest, ValidateSizeOnRemoteToLocalMainFrameSwap) {
  WebSize size(111, 222);

  WebRemoteFrame* remote_frame =
      WebRemoteFrame::Create(WebTreeScopeType::kDocument, nullptr);
  MainFrame()->Swap(remote_frame);

  remote_frame->View()->Resize(size);

  FrameTestHelpers::TestWebFrameClient client;
  WebLocalFrame* local_frame = WebLocalFrame::CreateProvisional(
      &client, nullptr, nullptr, remote_frame, WebSandboxFlags::kNone);
  remote_frame->Swap(local_frame);

  // Verify that the size that was set with a remote main frame is correct
  // after swapping to a local frame.
  Page* page =
      ToWebViewImpl(local_frame->View())->GetPage()->MainFrame()->GetPage();
  EXPECT_EQ(size.width, page->GetVisualViewport().Size().Width());
  EXPECT_EQ(size.height, page->GetVisualViewport().Size().Height());

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  Reset();
  remote_frame->Close();
}

namespace {

class SwapMainFrameWhenTitleChangesWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  SwapMainFrameWhenTitleChangesWebFrameClient() : remote_frame_(nullptr) {}

  ~SwapMainFrameWhenTitleChangesWebFrameClient() override {
    if (remote_frame_)
      remote_frame_->Close();
  }

  void DidReceiveTitle(const WebString&, WebTextDirection) override {
    if (!Frame()->Parent()) {
      remote_frame_ =
          WebRemoteFrame::Create(WebTreeScopeType::kDocument, nullptr);
      Frame()->Swap(remote_frame_);
    }
  }

 private:
  WebRemoteFrame* remote_frame_;
};

}  // anonymous namespace

TEST_F(WebFrameTest, SwapMainFrameWhileLoading) {
  SwapMainFrameWhenTitleChangesWebFrameClient frame_client;

  FrameTestHelpers::WebViewHelper web_view_helper;
  RegisterMockedHttpURLLoad("frame-a-b-c.html");
  RegisterMockedHttpURLLoad("subframe-a.html");
  RegisterMockedHttpURLLoad("subframe-b.html");
  RegisterMockedHttpURLLoad("subframe-c.html");
  RegisterMockedHttpURLLoad("subframe-hello.html");

  web_view_helper.InitializeAndLoad(base_url_ + "frame-a-b-c.html", true,
                                    &frame_client);
}

void WebFrameTest::SwapAndVerifyFirstChildConsistency(const char* const message,
                                                      WebFrame* parent,
                                                      WebFrame* new_child) {
  SCOPED_TRACE(message);
  parent->FirstChild()->Swap(new_child);

  EXPECT_EQ(new_child, parent->FirstChild());
  EXPECT_EQ(new_child->Parent(), parent);
  EXPECT_EQ(new_child,
            parent->last_child_->previous_sibling_->previous_sibling_);
  EXPECT_EQ(new_child->NextSibling(), parent->last_child_->previous_sibling_);
}

TEST_F(WebFrameSwapTest, SwapFirstChild) {
  FrameTestHelpers::TestWebRemoteFrameClient remote_frame_client;
  WebRemoteFrame* remote_frame =
      WebRemoteFrame::Create(WebTreeScopeType::kDocument, &remote_frame_client);
  SwapAndVerifyFirstChildConsistency("local->remote", MainFrame(),
                                     remote_frame);

  FrameTestHelpers::TestWebFrameClient client;
  WebLocalFrame* local_frame = WebLocalFrame::CreateProvisional(
      &client, nullptr, nullptr, remote_frame, WebSandboxFlags::kNone);
  SwapAndVerifyFirstChildConsistency("remote->local", MainFrame(), local_frame);

  // FIXME: This almost certainly fires more load events on the iframe element
  // than it should.
  // Finally, make sure an embedder triggered load in the local frame swapped
  // back in works.
  FrameTestHelpers::LoadFrame(local_frame, base_url_ + "subframe-hello.html");
  std::string content =
      WebFrameContentDumper::DumpWebViewAsText(WebView(), 1024).Utf8();
  EXPECT_EQ("  \n\nhello\n\nb \n\na\n\nc", content);

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  Reset();
  remote_frame->Close();
}

void WebFrameTest::SwapAndVerifyMiddleChildConsistency(
    const char* const message,
    WebFrame* parent,
    WebFrame* new_child) {
  SCOPED_TRACE(message);
  parent->FirstChild()->NextSibling()->Swap(new_child);

  EXPECT_EQ(new_child, parent->FirstChild()->NextSibling());
  EXPECT_EQ(new_child, parent->last_child_->previous_sibling_);
  EXPECT_EQ(new_child->Parent(), parent);
  EXPECT_EQ(new_child, parent->FirstChild()->NextSibling());
  EXPECT_EQ(new_child->previous_sibling_, parent->FirstChild());
  EXPECT_EQ(new_child, parent->last_child_->previous_sibling_);
  EXPECT_EQ(new_child->NextSibling(), parent->last_child_);
}

TEST_F(WebFrameSwapTest, SwapMiddleChild) {
  FrameTestHelpers::TestWebRemoteFrameClient remote_frame_client;
  WebRemoteFrame* remote_frame =
      WebRemoteFrame::Create(WebTreeScopeType::kDocument, &remote_frame_client);
  SwapAndVerifyMiddleChildConsistency("local->remote", MainFrame(),
                                      remote_frame);

  FrameTestHelpers::TestWebFrameClient client;
  WebLocalFrame* local_frame = WebLocalFrame::CreateProvisional(
      &client, nullptr, nullptr, remote_frame, WebSandboxFlags::kNone);
  SwapAndVerifyMiddleChildConsistency("remote->local", MainFrame(),
                                      local_frame);

  // FIXME: This almost certainly fires more load events on the iframe element
  // than it should.
  // Finally, make sure an embedder triggered load in the local frame swapped
  // back in works.
  FrameTestHelpers::LoadFrame(local_frame, base_url_ + "subframe-hello.html");
  std::string content =
      WebFrameContentDumper::DumpWebViewAsText(WebView(), 1024).Utf8();
  EXPECT_EQ("  \n\na\n\nhello\n\nc", content);

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  Reset();
  remote_frame->Close();
}

void WebFrameTest::SwapAndVerifyLastChildConsistency(const char* const message,
                                                     WebFrame* parent,
                                                     WebFrame* new_child) {
  SCOPED_TRACE(message);
  LastChild(parent)->Swap(new_child);

  EXPECT_EQ(new_child, LastChild(parent));
  EXPECT_EQ(new_child->Parent(), parent);
  EXPECT_EQ(new_child, parent->FirstChild()->NextSibling()->NextSibling());
  EXPECT_EQ(new_child->previous_sibling_, parent->FirstChild()->NextSibling());
}

TEST_F(WebFrameSwapTest, SwapLastChild) {
  FrameTestHelpers::TestWebRemoteFrameClient remote_frame_client;
  WebRemoteFrame* remote_frame =
      WebRemoteFrame::Create(WebTreeScopeType::kDocument, &remote_frame_client);
  SwapAndVerifyLastChildConsistency("local->remote", MainFrame(), remote_frame);

  FrameTestHelpers::TestWebFrameClient client;
  WebLocalFrame* local_frame = WebLocalFrame::CreateProvisional(
      &client, nullptr, nullptr, remote_frame, WebSandboxFlags::kNone);
  SwapAndVerifyLastChildConsistency("remote->local", MainFrame(), local_frame);

  // FIXME: This almost certainly fires more load events on the iframe element
  // than it should.
  // Finally, make sure an embedder triggered load in the local frame swapped
  // back in works.
  FrameTestHelpers::LoadFrame(local_frame, base_url_ + "subframe-hello.html");
  std::string content =
      WebFrameContentDumper::DumpWebViewAsText(WebView(), 1024).Utf8();
  EXPECT_EQ("  \n\na\n\nb \n\na\n\nhello", content);

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  Reset();
  remote_frame->Close();
}

TEST_F(WebFrameSwapTest, DetachProvisionalFrame) {
  FrameTestHelpers::TestWebRemoteFrameClient remote_frame_client;
  WebRemoteFrameImpl* remote_frame = WebRemoteFrameImpl::Create(
      WebTreeScopeType::kDocument, &remote_frame_client);
  SwapAndVerifyMiddleChildConsistency("local->remote", MainFrame(),
                                      remote_frame);

  FrameTestHelpers::TestWebFrameClient client;
  WebLocalFrameImpl* provisional_frame = WebLocalFrameImpl::CreateProvisional(
      &client, nullptr, nullptr, remote_frame, WebSandboxFlags::kNone);

  // The provisional frame should have a local frame owner.
  FrameOwner* owner = provisional_frame->GetFrame()->Owner();
  ASSERT_TRUE(owner->IsLocal());

  // But the owner should point to |remoteFrame|, since the new frame is still
  // provisional.
  EXPECT_EQ(remote_frame->GetFrame(), owner->ContentFrame());

  // After detaching the provisional frame, the frame owner should still point
  // at |remoteFrame|.
  provisional_frame->Detach();

  // The owner should not be affected by detaching the provisional frame, so it
  // should still point to |remoteFrame|.
  EXPECT_EQ(remote_frame->GetFrame(), owner->ContentFrame());

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  Reset();
}

void WebFrameTest::SwapAndVerifySubframeConsistency(const char* const message,
                                                    WebFrame* old_frame,
                                                    WebFrame* new_frame) {
  SCOPED_TRACE(message);

  EXPECT_TRUE(old_frame->FirstChild());
  old_frame->Swap(new_frame);

  EXPECT_FALSE(new_frame->FirstChild());
  EXPECT_FALSE(new_frame->last_child_);
}

TEST_F(WebFrameSwapTest, SwapParentShouldDetachChildren) {
  FrameTestHelpers::TestWebRemoteFrameClient remote_frame_client1;
  WebRemoteFrame* remote_frame = WebRemoteFrame::Create(
      WebTreeScopeType::kDocument, &remote_frame_client1);
  WebFrame* target_frame = MainFrame()->FirstChild()->NextSibling();
  EXPECT_TRUE(target_frame);
  SwapAndVerifySubframeConsistency("local->remote", target_frame, remote_frame);

  target_frame = MainFrame()->FirstChild()->NextSibling();
  EXPECT_TRUE(target_frame);

  // Create child frames in the target frame before testing the swap.
  FrameTestHelpers::TestWebRemoteFrameClient remote_frame_client2;
  WebRemoteFrame* child_remote_frame =
      FrameTestHelpers::CreateRemoteChild(remote_frame, &remote_frame_client2);

  FrameTestHelpers::TestWebFrameClient client;
  WebLocalFrame* local_frame = WebLocalFrame::CreateProvisional(
      &client, nullptr, nullptr, remote_frame, WebSandboxFlags::kNone);
  SwapAndVerifySubframeConsistency("remote->local", target_frame, local_frame);

  // FIXME: This almost certainly fires more load events on the iframe element
  // than it should.
  // Finally, make sure an embedder triggered load in the local frame swapped
  // back in works.
  FrameTestHelpers::LoadFrame(local_frame, base_url_ + "subframe-hello.html");
  std::string content =
      WebFrameContentDumper::DumpWebViewAsText(WebView(), 1024).Utf8();
  EXPECT_EQ("  \n\na\n\nhello\n\nc", content);

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  Reset();
  remote_frame->Close();
  child_remote_frame->Close();
}

TEST_F(WebFrameSwapTest, SwapPreservesGlobalContext) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Value> window_top =
      MainFrame()->ExecuteScriptAndReturnValue(WebScriptSource("window"));
  ASSERT_TRUE(window_top->IsObject());
  v8::Local<v8::Value> original_window =
      MainFrame()->ExecuteScriptAndReturnValue(
          WebScriptSource("document.querySelector('#frame2').contentWindow;"));
  ASSERT_TRUE(original_window->IsObject());

  // Make sure window reference stays the same when swapping to a remote frame.
  FrameTestHelpers::TestWebRemoteFrameClient remote_client;
  WebRemoteFrame* remote_frame = remote_client.GetFrame();
  WebFrame* target_frame = MainFrame()->FirstChild()->NextSibling();
  target_frame->Swap(remote_frame);
  remote_frame->SetReplicatedOrigin(SecurityOrigin::CreateUnique());
  v8::Local<v8::Value> remote_window = MainFrame()->ExecuteScriptAndReturnValue(
      WebScriptSource("document.querySelector('#frame2').contentWindow;"));
  EXPECT_TRUE(original_window->StrictEquals(remote_window));
  // Check that its view is consistent with the world.
  v8::Local<v8::Value> remote_window_top =
      MainFrame()->ExecuteScriptAndReturnValue(WebScriptSource(
          "document.querySelector('#frame2').contentWindow.top;"));
  EXPECT_TRUE(window_top->StrictEquals(remote_window_top));

  // Now check that remote -> local works too, since it goes through a different
  // code path.
  FrameTestHelpers::TestWebFrameClient client;
  WebLocalFrame* local_frame = WebLocalFrame::CreateProvisional(
      &client, nullptr, nullptr, remote_frame, WebSandboxFlags::kNone);
  remote_frame->Swap(local_frame);
  v8::Local<v8::Value> local_window = MainFrame()->ExecuteScriptAndReturnValue(
      WebScriptSource("document.querySelector('#frame2').contentWindow;"));
  EXPECT_TRUE(original_window->StrictEquals(local_window));
  v8::Local<v8::Value> local_window_top =
      MainFrame()->ExecuteScriptAndReturnValue(WebScriptSource(
          "document.querySelector('#frame2').contentWindow.top;"));
  EXPECT_TRUE(window_top->StrictEquals(local_window_top));

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  Reset();
}

TEST_F(WebFrameSwapTest, SetTimeoutAfterSwap) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope scope(isolate);
  MainFrame()->ExecuteScript(
      WebScriptSource("savedSetTimeout = window[0].setTimeout"));

  // Swap the frame to a remote frame.
  FrameTestHelpers::TestWebRemoteFrameClient remote_client;
  WebRemoteFrame* remote_frame = remote_client.GetFrame();
  WebFrame* target_frame = MainFrame()->FirstChild();
  target_frame->Swap(remote_frame);
  remote_frame->SetReplicatedOrigin(SecurityOrigin::CreateUnique());

  // Invoking setTimeout should throw a security error.
  {
    v8::Local<v8::Value> exception = MainFrame()->ExecuteScriptAndReturnValue(
        WebScriptSource("try {\n"
                        "  savedSetTimeout.call(window[0], () => {}, 0);\n"
                        "} catch (e) { e; }"));
    ASSERT_TRUE(!exception.IsEmpty());
    EXPECT_EQ(
        "SecurityError: Blocked a frame with origin \"http://internal.test\" "
        "from accessing a cross-origin frame.",
        ToCoreString(exception
                         ->ToString(ToScriptStateForMainWorld(
                                        WebView()->MainFrameImpl()->GetFrame())
                                        ->GetContext())
                         .ToLocalChecked()));
  }

  Reset();
}

TEST_F(WebFrameSwapTest, SwapInitializesGlobal) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  v8::Local<v8::Value> window_top =
      MainFrame()->ExecuteScriptAndReturnValue(WebScriptSource("window"));
  ASSERT_TRUE(window_top->IsObject());

  v8::Local<v8::Value> last_child = MainFrame()->ExecuteScriptAndReturnValue(
      WebScriptSource("saved = window[2]"));
  ASSERT_TRUE(last_child->IsObject());

  FrameTestHelpers::TestWebRemoteFrameClient remote_client;
  WebRemoteFrame* remote_frame = remote_client.GetFrame();
  WebFrameTest::LastChild(MainFrame())->Swap(remote_frame);
  remote_frame->SetReplicatedOrigin(SecurityOrigin::CreateUnique());
  v8::Local<v8::Value> remote_window_top =
      MainFrame()->ExecuteScriptAndReturnValue(WebScriptSource("saved.top"));
  EXPECT_TRUE(remote_window_top->IsObject());
  EXPECT_TRUE(window_top->StrictEquals(remote_window_top));

  FrameTestHelpers::TestWebFrameClient client;
  WebLocalFrame* local_frame = WebLocalFrame::CreateProvisional(
      &client, nullptr, nullptr, remote_frame, WebSandboxFlags::kNone);
  remote_frame->Swap(local_frame);
  v8::Local<v8::Value> local_window_top =
      MainFrame()->ExecuteScriptAndReturnValue(WebScriptSource("saved.top"));
  EXPECT_TRUE(local_window_top->IsObject());
  EXPECT_TRUE(window_top->StrictEquals(local_window_top));

  Reset();
}

TEST_F(WebFrameSwapTest, RemoteFramesAreIndexable) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  FrameTestHelpers::TestWebRemoteFrameClient remote_client;
  WebRemoteFrame* remote_frame = remote_client.GetFrame();
  LastChild(MainFrame())->Swap(remote_frame);
  remote_frame->SetReplicatedOrigin(SecurityOrigin::CreateUnique());
  v8::Local<v8::Value> remote_window =
      MainFrame()->ExecuteScriptAndReturnValue(WebScriptSource("window[2]"));
  EXPECT_TRUE(remote_window->IsObject());
  v8::Local<v8::Value> window_length = MainFrame()->ExecuteScriptAndReturnValue(
      WebScriptSource("window.length"));
  ASSERT_TRUE(window_length->IsInt32());
  EXPECT_EQ(3, window_length.As<v8::Int32>()->Value());

  Reset();
}

TEST_F(WebFrameSwapTest, RemoteFrameLengthAccess) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  FrameTestHelpers::TestWebRemoteFrameClient remote_client;
  WebRemoteFrame* remote_frame = remote_client.GetFrame();
  LastChild(MainFrame())->Swap(remote_frame);
  remote_frame->SetReplicatedOrigin(SecurityOrigin::CreateUnique());
  v8::Local<v8::Value> remote_window_length =
      MainFrame()->ExecuteScriptAndReturnValue(
          WebScriptSource("window[2].length"));
  ASSERT_TRUE(remote_window_length->IsInt32());
  EXPECT_EQ(0, remote_window_length.As<v8::Int32>()->Value());

  Reset();
}

TEST_F(WebFrameSwapTest, RemoteWindowNamedAccess) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  // FIXME: Once OOPIF unit test infrastructure is in place, test that named
  // window access on a remote window works. For now, just test that accessing
  // a named property doesn't crash.
  FrameTestHelpers::TestWebRemoteFrameClient remote_client;
  WebRemoteFrame* remote_frame = remote_client.GetFrame();
  LastChild(MainFrame())->Swap(remote_frame);
  remote_frame->SetReplicatedOrigin(SecurityOrigin::CreateUnique());
  v8::Local<v8::Value> remote_window_property =
      MainFrame()->ExecuteScriptAndReturnValue(
          WebScriptSource("window[2].foo"));
  EXPECT_TRUE(remote_window_property.IsEmpty());

  Reset();
}

TEST_F(WebFrameSwapTest, RemoteWindowToString) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  FrameTestHelpers::TestWebRemoteFrameClient remote_client;
  WebRemoteFrame* remote_frame = remote_client.GetFrame();
  LastChild(MainFrame())->Swap(remote_frame);
  remote_frame->SetReplicatedOrigin(SecurityOrigin::CreateUnique());
  v8::Local<v8::Value> to_string_result =
      MainFrame()->ExecuteScriptAndReturnValue(
          WebScriptSource("Object.prototype.toString.call(window[2])"));
  ASSERT_FALSE(to_string_result.IsEmpty());
  EXPECT_STREQ("[object Object]", *v8::String::Utf8Value(to_string_result));

  Reset();
}

// TODO(alexmos, dcheng): This test and some other OOPIF tests use
// very little of the test fixture support in WebFrameSwapTest.  We should
// clean these tests up.
TEST_F(WebFrameSwapTest, FramesOfRemoteParentAreIndexable) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  FrameTestHelpers::TestWebRemoteFrameClient remote_client;
  WebRemoteFrame* remote_parent_frame = remote_client.GetFrame();
  MainFrame()->Swap(remote_parent_frame);
  remote_parent_frame->SetReplicatedOrigin(SecurityOrigin::CreateUnique());

  WebLocalFrame* child_frame =
      FrameTestHelpers::CreateLocalChild(remote_parent_frame);
  FrameTestHelpers::LoadFrame(child_frame, base_url_ + "subframe-hello.html");

  v8::Local<v8::Value> window =
      child_frame->ExecuteScriptAndReturnValue(WebScriptSource("window"));
  v8::Local<v8::Value> child_of_remote_parent =
      child_frame->ExecuteScriptAndReturnValue(
          WebScriptSource("parent.frames[0]"));
  EXPECT_TRUE(child_of_remote_parent->IsObject());
  EXPECT_TRUE(window->StrictEquals(child_of_remote_parent));

  v8::Local<v8::Value> window_length = child_frame->ExecuteScriptAndReturnValue(
      WebScriptSource("parent.frames.length"));
  ASSERT_TRUE(window_length->IsInt32());
  EXPECT_EQ(1, window_length.As<v8::Int32>()->Value());

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // clients.
  Reset();
}

// Check that frames with a remote parent don't crash while accessing
// window.frameElement.
TEST_F(WebFrameSwapTest, FrameElementInFramesWithRemoteParent) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  FrameTestHelpers::TestWebRemoteFrameClient remote_client;
  WebRemoteFrame* remote_parent_frame = remote_client.GetFrame();
  MainFrame()->Swap(remote_parent_frame);
  remote_parent_frame->SetReplicatedOrigin(SecurityOrigin::CreateUnique());

  WebLocalFrame* child_frame =
      FrameTestHelpers::CreateLocalChild(remote_parent_frame);
  FrameTestHelpers::LoadFrame(child_frame, base_url_ + "subframe-hello.html");

  v8::Local<v8::Value> frame_element = child_frame->ExecuteScriptAndReturnValue(
      WebScriptSource("window.frameElement"));
  // frameElement should be null if cross-origin.
  ASSERT_FALSE(frame_element.IsEmpty());
  EXPECT_TRUE(frame_element->IsNull());

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // clients.
  Reset();
}

class RemoteToLocalSwapWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  explicit RemoteToLocalSwapWebFrameClient(WebRemoteFrame* remote_frame)
      : history_commit_type_(kWebHistoryInertCommit),
        remote_frame_(remote_frame) {}

  void DidCommitProvisionalLoad(
      const WebHistoryItem&,
      WebHistoryCommitType history_commit_type) override {
    history_commit_type_ = history_commit_type;
    remote_frame_->Swap(Frame());
  }

  WebHistoryCommitType HistoryCommitType() const {
    return history_commit_type_;
  }

  WebHistoryCommitType history_commit_type_;
  WebRemoteFrame* remote_frame_;
};

// The commit type should be Initial if we are swapping a RemoteFrame to a
// LocalFrame as it is first being created.  This happens when another frame
// exists in the same process, such that we create the RemoteFrame before the
// first navigation occurs.
TEST_F(WebFrameSwapTest, HistoryCommitTypeAfterNewRemoteToLocalSwap) {
  FrameTestHelpers::TestWebRemoteFrameClient remote_frame_client;
  WebRemoteFrame* remote_frame =
      WebRemoteFrame::Create(WebTreeScopeType::kDocument, &remote_frame_client);
  WebFrame* target_frame = MainFrame()->FirstChild();
  ASSERT_TRUE(target_frame);
  target_frame->Swap(remote_frame);
  ASSERT_TRUE(MainFrame()->FirstChild());
  ASSERT_EQ(MainFrame()->FirstChild(), remote_frame);

  RemoteToLocalSwapWebFrameClient client(remote_frame);
  WebLocalFrame* local_frame = WebLocalFrame::CreateProvisional(
      &client, nullptr, nullptr, remote_frame, WebSandboxFlags::kNone);
  client.SetFrame(local_frame);
  FrameTestHelpers::LoadFrame(local_frame, base_url_ + "subframe-hello.html");
  EXPECT_EQ(kWebInitialCommitInChildFrame, client.HistoryCommitType());

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  Reset();
  remote_frame->Close();
}

// The commit type should be Standard if we are swapping a RemoteFrame to a
// LocalFrame after commits have already happened in the frame.  The browser
// process will inform us via setCommittedFirstRealLoad.
TEST_F(WebFrameSwapTest, HistoryCommitTypeAfterExistingRemoteToLocalSwap) {
  FrameTestHelpers::TestWebRemoteFrameClient remote_frame_client;
  WebRemoteFrame* remote_frame =
      WebRemoteFrame::Create(WebTreeScopeType::kDocument, &remote_frame_client);
  WebFrame* target_frame = MainFrame()->FirstChild();
  ASSERT_TRUE(target_frame);
  target_frame->Swap(remote_frame);
  ASSERT_TRUE(MainFrame()->FirstChild());
  ASSERT_EQ(MainFrame()->FirstChild(), remote_frame);

  RemoteToLocalSwapWebFrameClient client(remote_frame);
  WebLocalFrame* local_frame = WebLocalFrame::CreateProvisional(
      &client, nullptr, nullptr, remote_frame, WebSandboxFlags::kNone);
  client.SetFrame(local_frame);
  local_frame->SetCommittedFirstRealLoad();
  FrameTestHelpers::LoadFrame(local_frame, base_url_ + "subframe-hello.html");
  EXPECT_EQ(kWebStandardCommit, client.HistoryCommitType());

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  Reset();
  remote_frame->Close();
}

class RemoteNavigationClient
    : public FrameTestHelpers::TestWebRemoteFrameClient {
 public:
  void Navigate(const WebURLRequest& request,
                bool should_replace_current_entry) override {
    last_request_ = request;
  }

  const WebURLRequest& LastRequest() const { return last_request_; }

 private:
  WebURLRequest last_request_;
};

TEST_F(WebFrameSwapTest, NavigateRemoteFrameViaLocation) {
  RemoteNavigationClient client;
  WebRemoteFrame* remote_frame = client.GetFrame();
  WebFrame* target_frame = MainFrame()->FirstChild();
  ASSERT_TRUE(target_frame);
  target_frame->Swap(remote_frame);
  ASSERT_TRUE(MainFrame()->FirstChild());
  ASSERT_EQ(MainFrame()->FirstChild(), remote_frame);

  remote_frame->SetReplicatedOrigin(
      WebSecurityOrigin::CreateFromString("http://127.0.0.1"));
  MainFrame()->ExecuteScript(
      WebScriptSource("document.getElementsByTagName('iframe')[0]."
                      "contentWindow.location = 'data:text/html,hi'"));
  ASSERT_FALSE(client.LastRequest().IsNull());
  EXPECT_EQ(WebURL(ToKURL("data:text/html,hi")), client.LastRequest().Url());

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  Reset();
}

TEST_F(WebFrameSwapTest, WindowOpenOnRemoteFrame) {
  RemoteNavigationClient remote_client;
  WebRemoteFrame* remote_frame = remote_client.GetFrame();
  MainFrame()->FirstChild()->Swap(remote_frame);
  remote_frame->SetReplicatedOrigin(
      WebSecurityOrigin::CreateFromString("http://127.0.0.1"));

  ASSERT_TRUE(MainFrame()->IsWebLocalFrame());
  ASSERT_TRUE(MainFrame()->FirstChild()->IsWebRemoteFrame());
  LocalDOMWindow* main_window =
      ToWebLocalFrameImpl(MainFrame())->GetFrame()->DomWindow();

  KURL destination = ToKURL("data:text/html:destination");
  main_window->open(destination.GetString(), "frame1", "", main_window,
                    main_window);
  ASSERT_FALSE(remote_client.LastRequest().IsNull());
  EXPECT_EQ(remote_client.LastRequest().Url(), WebURL(destination));

  // Pointing a named frame to an empty URL should just return a reference to
  // the frame's window without navigating it.
  DOMWindow* result =
      main_window->open("", "frame1", "", main_window, main_window);
  EXPECT_EQ(remote_client.LastRequest().Url(), WebURL(destination));
  EXPECT_EQ(result, remote_frame->ToImplBase()->GetFrame()->DomWindow());

  Reset();
}

class RemoteWindowCloseClient : public FrameTestHelpers::TestWebViewClient {
 public:
  RemoteWindowCloseClient() : closed_(false) {}

  void CloseWidgetSoon() override { closed_ = true; }

  bool Closed() const { return closed_; }

 private:
  bool closed_;
};

TEST_F(WebFrameTest, WindowOpenRemoteClose) {
  FrameTestHelpers::WebViewHelper main_web_view;
  main_web_view.Initialize(true);

  // Create a remote window that will be closed later in the test.
  RemoteWindowCloseClient view_client;
  FrameTestHelpers::TestWebRemoteFrameClient frame_client;
  WebRemoteFrameImpl* web_remote_frame = frame_client.GetFrame();

  WebView* view = WebView::Create(&view_client, kWebPageVisibilityStateVisible);
  view->SetMainFrame(web_remote_frame);
  view->MainFrame()->SetOpener(main_web_view.WebView()->MainFrame());
  web_remote_frame->SetReplicatedOrigin(
      WebSecurityOrigin::CreateFromString("http://127.0.0.1"));

  LocalFrame* local_frame = ToLocalFrame(
      main_web_view.WebView()->MainFrame()->ToImplBase()->GetFrame());
  RemoteFrame* remote_frame = web_remote_frame->GetFrame();

  // Attempt to close the window, which should fail as it isn't opened
  // by a script.
  remote_frame->DomWindow()->close(local_frame->GetDocument());
  EXPECT_FALSE(view_client.Closed());

  // Marking it as opened by a script should now allow it to be closed.
  remote_frame->GetPage()->SetOpenedByDOM();
  remote_frame->DomWindow()->close(local_frame->GetDocument());
  EXPECT_TRUE(view_client.Closed());

  view->Close();
}

TEST_F(WebFrameTest, NavigateRemoteToLocalWithOpener) {
  FrameTestHelpers::WebViewHelper main_web_view;
  main_web_view.Initialize(true);
  WebFrame* main_frame = main_web_view.WebView()->MainFrame();

  // Create a popup with a remote frame and set its opener to the main frame.
  FrameTestHelpers::TestWebViewClient popup_view_client;
  WebView* popup_view =
      WebView::Create(&popup_view_client, kWebPageVisibilityStateVisible);
  FrameTestHelpers::TestWebRemoteFrameClient popup_remote_client;
  WebRemoteFrame* popup_remote_frame = popup_remote_client.GetFrame();
  popup_view->SetMainFrame(popup_remote_frame);
  popup_remote_frame->SetOpener(main_frame);
  popup_remote_frame->SetReplicatedOrigin(
      WebSecurityOrigin::CreateFromString("http://foo.com"));
  EXPECT_FALSE(main_frame->GetSecurityOrigin().CanAccess(
      popup_view->MainFrame()->GetSecurityOrigin()));

  // Do a remote-to-local swap in the popup.
  FrameTestHelpers::TestWebFrameClient popup_local_client;
  WebLocalFrame* popup_local_frame = WebLocalFrame::CreateProvisional(
      &popup_local_client, nullptr, nullptr, popup_remote_frame,
      WebSandboxFlags::kNone);
  popup_remote_frame->Swap(popup_local_frame);

  // The initial document created during the remote-to-local swap should have
  // inherited its opener's SecurityOrigin.
  EXPECT_TRUE(main_frame->GetSecurityOrigin().CanAccess(
      popup_view->MainFrame()->GetSecurityOrigin()));

  popup_view->Close();
}

TEST_F(WebFrameTest, SwapWithOpenerCycle) {
  // First, create a remote main frame with itself as the opener.
  FrameTestHelpers::TestWebViewClient view_client;
  WebView* view = WebView::Create(&view_client, kWebPageVisibilityStateVisible);
  FrameTestHelpers::TestWebRemoteFrameClient remote_client;
  WebRemoteFrame* remote_frame = remote_client.GetFrame();
  view->SetMainFrame(remote_frame);
  remote_frame->SetOpener(remote_frame);

  // Now swap in a local frame. It shouldn't crash.
  FrameTestHelpers::TestWebFrameClient local_client;
  WebLocalFrame* local_frame = WebLocalFrame::CreateProvisional(
      &local_client, nullptr, nullptr, remote_frame, WebSandboxFlags::kNone);
  remote_frame->Swap(local_frame);

  // And the opener cycle should still be preserved.
  EXPECT_EQ(local_frame, local_frame->Opener());

  view->Close();
}

class CommitTypeWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
 public:
  explicit CommitTypeWebFrameClient()
      : history_commit_type_(kWebHistoryInertCommit) {}

  void DidCommitProvisionalLoad(
      const WebHistoryItem&,
      WebHistoryCommitType history_commit_type) override {
    history_commit_type_ = history_commit_type;
  }

  WebHistoryCommitType HistoryCommitType() const {
    return history_commit_type_;
  }

 private:
  WebHistoryCommitType history_commit_type_;
};

TEST_P(ParameterizedWebFrameTest, RemoteFrameInitialCommitType) {
  FrameTestHelpers::TestWebViewClient view_client;
  FrameTestHelpers::TestWebRemoteFrameClient remote_client;
  WebView* view = WebView::Create(&view_client, kWebPageVisibilityStateVisible);
  view->SetMainFrame(remote_client.GetFrame());
  remote_client.GetFrame()->SetReplicatedOrigin(
      WebSecurityOrigin::CreateFromString(WebString::FromUTF8(base_url_)));

  // If an iframe has a remote main frame, ensure the inital commit is correctly
  // identified as WebInitialCommitInChildFrame.
  CommitTypeWebFrameClient child_frame_client;
  WebLocalFrame* child_frame = FrameTestHelpers::CreateLocalChild(
      view->MainFrame()->ToWebRemoteFrame(), "frameName", &child_frame_client);
  RegisterMockedHttpURLLoad("foo.html");
  FrameTestHelpers::LoadFrame(child_frame, base_url_ + "foo.html");
  EXPECT_EQ(kWebInitialCommitInChildFrame,
            child_frame_client.HistoryCommitType());
  view->Close();
}

class GestureEventTestWebWidgetClient
    : public FrameTestHelpers::TestWebWidgetClient {
 public:
  GestureEventTestWebWidgetClient() : did_handle_gesture_event_(false) {}
  void DidHandleGestureEvent(const WebGestureEvent& event,
                             bool event_cancelled) override {
    did_handle_gesture_event_ = true;
  }
  bool DidHandleGestureEvent() const { return did_handle_gesture_event_; }

 private:
  bool did_handle_gesture_event_;
};

TEST_P(ParameterizedWebFrameTest, FrameWidgetTest) {
  FrameTestHelpers::TestWebViewClient view_client;
  WebView* view = WebView::Create(&view_client, kWebPageVisibilityStateVisible);

  FrameTestHelpers::TestWebRemoteFrameClient remote_client;
  view->SetMainFrame(remote_client.GetFrame());

  GestureEventTestWebWidgetClient child_widget_client;
  WebLocalFrame* child_frame = FrameTestHelpers::CreateLocalChild(
      view->MainFrame()->ToWebRemoteFrame(), WebString(), nullptr,
      &child_widget_client);

  view->Resize(WebSize(1000, 1000));

  child_frame->FrameWidget()->HandleInputEvent(FatTap(20, 20));
  EXPECT_TRUE(child_widget_client.DidHandleGestureEvent());

  view->Close();
}

class MockDocumentThreadableLoaderClient
    : public DocumentThreadableLoaderClient {
 public:
  MockDocumentThreadableLoaderClient() : failed_(false) {}
  void DidFail(const ResourceError&) override { failed_ = true; }

  void Reset() { failed_ = false; }
  bool Failed() { return failed_; }

  bool failed_;
};

// FIXME: This would be better as a unittest on DocumentThreadableLoader but it
// requires spin-up of a frame. It may be possible to remove that requirement
// and convert it to a unittest.
TEST_P(ParameterizedWebFrameTest, LoaderOriginAccess) {
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad("about:blank");

  SchemeRegistry::RegisterURLSchemeAsDisplayIsolated("chrome");

  // Cross-origin request.
  KURL resource_url(kParsedURLString, "chrome://test.pdf");
  ResourceRequest request(resource_url);
  request.SetRequestContext(WebURLRequest::kRequestContextObject);
  RegisterMockedChromeURLLoad("test.pdf");

  LocalFrame* frame(
      ToLocalFrame(web_view_helper.WebView()->GetPage()->MainFrame()));

  MockDocumentThreadableLoaderClient client;
  ThreadableLoaderOptions options;

  // First try to load the request with regular access. Should fail.
  options.cross_origin_request_policy = kUseAccessControl;
  ResourceLoaderOptions resource_loader_options;
  DocumentThreadableLoader::LoadResourceSynchronously(
      *frame->GetDocument(), request, client, options, resource_loader_options);
  EXPECT_TRUE(client.Failed());

  client.Reset();
  // Try to load the request with cross origin access. Should succeed.
  options.cross_origin_request_policy = kAllowCrossOriginRequests;
  DocumentThreadableLoader::LoadResourceSynchronously(
      *frame->GetDocument(), request, client, options, resource_loader_options);
  EXPECT_FALSE(client.Failed());
}

TEST_P(ParameterizedWebFrameTest, DetachRemoteFrame) {
  FrameTestHelpers::TestWebViewClient view_client;
  FrameTestHelpers::TestWebRemoteFrameClient remote_client;
  WebView* view = WebView::Create(&view_client, kWebPageVisibilityStateVisible);
  view->SetMainFrame(remote_client.GetFrame());
  FrameTestHelpers::TestWebRemoteFrameClient child_frame_client;
  WebRemoteFrame* child_frame = FrameTestHelpers::CreateRemoteChild(
      view->MainFrame()->ToWebRemoteFrame(), &child_frame_client);
  child_frame->Detach();
  view->Close();
  child_frame->Close();
}

class TestConsoleMessageWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  virtual void DidAddMessageToConsole(const WebConsoleMessage& message,
                                      const WebString& source_name,
                                      unsigned source_line,
                                      const WebString& stack_trace) {
    messages.push_back(message);
  }

  Vector<WebConsoleMessage> messages;
};

TEST_P(ParameterizedWebFrameTest, CrossDomainAccessErrorsUseCallingWindow) {
  RegisterMockedHttpURLLoad("hidden_frames.html");
  RegisterMockedChromeURLLoad("hello_world.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  TestConsoleMessageWebFrameClient web_frame_client;
  FrameTestHelpers::TestWebViewClient web_view_client;
  web_view_helper.InitializeAndLoad(base_url_ + "hidden_frames.html", true,
                                    &web_frame_client, &web_view_client);

  // Create another window with a cross-origin page, and point its opener to
  // first window.
  FrameTestHelpers::WebViewHelper popup_web_view_helper;
  TestConsoleMessageWebFrameClient popup_web_frame_client;
  WebView* popup_view = popup_web_view_helper.InitializeAndLoad(
      chrome_url_ + "hello_world.html", true, &popup_web_frame_client);
  popup_view->MainFrame()->SetOpener(web_view_helper.WebView()->MainFrame());

  // Attempt a blocked navigation of an opener's subframe, and ensure that
  // the error shows up on the popup (calling) window's console, rather than
  // the target window.
  popup_view->MainFrame()->ExecuteScript(WebScriptSource(
      "try { opener.frames[1].location.href='data:text/html,foo'; } catch (e) "
      "{}"));
  EXPECT_TRUE(web_frame_client.messages.IsEmpty());
  ASSERT_EQ(1u, popup_web_frame_client.messages.size());
  EXPECT_TRUE(std::string::npos !=
              popup_web_frame_client.messages[0].text.Utf8().find(
                  "Unsafe JavaScript attempt to initiate navigation"));

  // Try setting a cross-origin iframe element's source to a javascript: URL,
  // and check that this error is also printed on the calling window.
  popup_view->MainFrame()->ExecuteScript(
      WebScriptSource("opener.document.querySelectorAll('iframe')[1].src='"
                      "javascript:alert()'"));
  EXPECT_TRUE(web_frame_client.messages.IsEmpty());
  ASSERT_EQ(2u, popup_web_frame_client.messages.size());
  EXPECT_TRUE(
      std::string::npos !=
      popup_web_frame_client.messages[1].text.Utf8().find("Blocked a frame"));

  // Manually reset to break WebViewHelpers' dependencies on the stack
  // allocated WebFrameClients.
  web_view_helper.Reset();
  popup_web_view_helper.Reset();
}

TEST_P(ParameterizedWebFrameTest, ResizeInvalidatesDeviceMediaQueries) {
  RegisterMockedHttpURLLoad("device_media_queries.html");
  FixedLayoutTestWebViewClient client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "device_media_queries.html",
                                    true, nullptr, &client, nullptr,
                                    ConfigureAndroid);
  LocalFrame* frame =
      ToLocalFrame(web_view_helper.WebView()->GetPage()->MainFrame());
  Element* element = frame->GetDocument()->GetElementById("test");
  ASSERT_TRUE(element);

  client.screen_info_.rect = WebRect(0, 0, 700, 500);
  client.screen_info_.available_rect = client.screen_info_.rect;
  web_view_helper.Resize(WebSize(700, 500));
  EXPECT_EQ(300, element->OffsetWidth());
  EXPECT_EQ(300, element->OffsetHeight());

  client.screen_info_.rect = WebRect(0, 0, 710, 500);
  client.screen_info_.available_rect = client.screen_info_.rect;
  web_view_helper.Resize(WebSize(710, 500));
  EXPECT_EQ(400, element->OffsetWidth());
  EXPECT_EQ(300, element->OffsetHeight());

  client.screen_info_.rect = WebRect(0, 0, 690, 500);
  client.screen_info_.available_rect = client.screen_info_.rect;
  web_view_helper.Resize(WebSize(690, 500));
  EXPECT_EQ(200, element->OffsetWidth());
  EXPECT_EQ(300, element->OffsetHeight());

  client.screen_info_.rect = WebRect(0, 0, 700, 510);
  client.screen_info_.available_rect = client.screen_info_.rect;
  web_view_helper.Resize(WebSize(700, 510));
  EXPECT_EQ(300, element->OffsetWidth());
  EXPECT_EQ(400, element->OffsetHeight());

  client.screen_info_.rect = WebRect(0, 0, 700, 490);
  client.screen_info_.available_rect = client.screen_info_.rect;
  web_view_helper.Resize(WebSize(700, 490));
  EXPECT_EQ(300, element->OffsetWidth());
  EXPECT_EQ(200, element->OffsetHeight());

  client.screen_info_.rect = WebRect(0, 0, 690, 510);
  client.screen_info_.available_rect = client.screen_info_.rect;
  web_view_helper.Resize(WebSize(690, 510));
  EXPECT_EQ(200, element->OffsetWidth());
  EXPECT_EQ(400, element->OffsetHeight());
}

class DeviceEmulationTest : public ParameterizedWebFrameTest {
 protected:
  DeviceEmulationTest() {
    RegisterMockedHttpURLLoad("device_emulation.html");
    client_.screen_info_.device_scale_factor = 1;
    web_view_helper_.InitializeAndLoad(base_url_ + "device_emulation.html",
                                       true, 0, &client_);
  }

  void TestResize(const WebSize size, const String& expected_size) {
    client_.screen_info_.rect = WebRect(0, 0, size.width, size.height);
    client_.screen_info_.available_rect = client_.screen_info_.rect;
    web_view_helper_.Resize(size);
    EXPECT_EQ(expected_size, DumpSize("test"));
  }

  String DumpSize(const String& id) {
    String code = "dumpSize('" + id + "')";
    v8::HandleScope scope(v8::Isolate::GetCurrent());
    ScriptExecutionCallbackHelper callback_helper(
        web_view_helper_.WebView()->MainFrame()->MainWorldScriptContext());
    web_view_helper_.WebView()
        ->MainFrameImpl()
        ->RequestExecuteScriptAndReturnValue(WebScriptSource(WebString(code)),
                                             false, &callback_helper);
    RunPendingTasks();
    EXPECT_TRUE(callback_helper.DidComplete());
    return callback_helper.StringValue();
  }

  FixedLayoutTestWebViewClient client_;
  FrameTestHelpers::WebViewHelper web_view_helper_;
};

INSTANTIATE_TEST_CASE_P(All, DeviceEmulationTest, ::testing::Bool());

TEST_P(DeviceEmulationTest, DeviceSizeInvalidatedOnResize) {
  WebDeviceEmulationParams params;
  params.screen_position = WebDeviceEmulationParams::kMobile;
  web_view_helper_.WebView()->EnableDeviceEmulation(params);

  TestResize(WebSize(700, 500), "300x300");
  TestResize(WebSize(710, 500), "400x300");
  TestResize(WebSize(690, 500), "200x300");
  TestResize(WebSize(700, 510), "300x400");
  TestResize(WebSize(700, 490), "300x200");
  TestResize(WebSize(710, 510), "400x400");
  TestResize(WebSize(690, 490), "200x200");
  TestResize(WebSize(800, 600), "400x400");

  web_view_helper_.WebView()->DisableDeviceEmulation();
}

TEST_P(DeviceEmulationTest, PointerAndHoverTypes) {
  WebDeviceEmulationParams params;
  params.screen_position = WebDeviceEmulationParams::kMobile;
  web_view_helper_.WebView()->EnableDeviceEmulation(params);
  EXPECT_EQ("20x20", DumpSize("pointer"));
  web_view_helper_.WebView()->DisableDeviceEmulation();
}

TEST_P(ParameterizedWebFrameTest, CreateLocalChildWithPreviousSibling) {
  FrameTestHelpers::TestWebViewClient view_client;
  FrameTestHelpers::TestWebRemoteFrameClient remote_client;
  WebView* view = WebView::Create(&view_client, kWebPageVisibilityStateVisible);
  view->SetMainFrame(remote_client.GetFrame());
  WebRemoteFrame* parent = view->MainFrame()->ToWebRemoteFrame();

  WebLocalFrame* second_frame(
      FrameTestHelpers::CreateLocalChild(parent, "name2"));
  WebLocalFrame* fourth_frame(FrameTestHelpers::CreateLocalChild(
      parent, "name4", nullptr, nullptr, second_frame));
  WebLocalFrame* third_frame(FrameTestHelpers::CreateLocalChild(
      parent, "name3", nullptr, nullptr, second_frame));
  WebLocalFrame* first_frame(
      FrameTestHelpers::CreateLocalChild(parent, "name1"));

  EXPECT_EQ(first_frame, parent->FirstChild());
  EXPECT_EQ(nullptr, PreviousSibling(first_frame));
  EXPECT_EQ(second_frame, first_frame->NextSibling());

  EXPECT_EQ(first_frame, PreviousSibling(second_frame));
  EXPECT_EQ(third_frame, second_frame->NextSibling());

  EXPECT_EQ(second_frame, PreviousSibling(third_frame));
  EXPECT_EQ(fourth_frame, third_frame->NextSibling());

  EXPECT_EQ(third_frame, PreviousSibling(fourth_frame));
  EXPECT_EQ(nullptr, fourth_frame->NextSibling());
  EXPECT_EQ(fourth_frame, LastChild(parent));

  EXPECT_EQ(parent, first_frame->Parent());
  EXPECT_EQ(parent, second_frame->Parent());
  EXPECT_EQ(parent, third_frame->Parent());
  EXPECT_EQ(parent, fourth_frame->Parent());

  view->Close();
}

TEST_P(ParameterizedWebFrameTest, SendBeaconFromChildWithRemoteMainFrame) {
  FrameTestHelpers::TestWebViewClient view_client;
  FrameTestHelpers::TestWebRemoteFrameClient remote_client;
  WebView* view = WebView::Create(&view_client, kWebPageVisibilityStateVisible);
  view->GetSettings()->SetJavaScriptEnabled(true);
  view->SetMainFrame(remote_client.GetFrame());
  WebRemoteFrame* root = view->MainFrame()->ToWebRemoteFrame();
  root->SetReplicatedOrigin(SecurityOrigin::CreateUnique());

  WebLocalFrame* local_frame = FrameTestHelpers::CreateLocalChild(root);

  // Finally, make sure an embedder triggered load in the local frame swapped
  // back in works.
  RegisterMockedHttpURLLoad("send_beacon.html");
  RegisterMockedHttpURLLoad("reload_post.html");  // url param to sendBeacon()
  FrameTestHelpers::LoadFrame(local_frame, base_url_ + "send_beacon.html");

  view->Close();
}

TEST_P(ParameterizedWebFrameTest,
       FirstPartyForCookiesFromChildWithRemoteMainFrame) {
  FrameTestHelpers::TestWebViewClient view_client;
  FrameTestHelpers::TestWebRemoteFrameClient remote_client;
  WebView* view = WebView::Create(&view_client, kWebPageVisibilityStateVisible);
  view->SetMainFrame(remote_client.GetFrame());
  WebRemoteFrame* root = view->MainFrame()->ToWebRemoteFrame();
  root->SetReplicatedOrigin(SecurityOrigin::Create(ToKURL(not_base_url_)));

  WebLocalFrame* local_frame = FrameTestHelpers::CreateLocalChild(root);

  RegisterMockedHttpURLLoad("foo.html");
  FrameTestHelpers::LoadFrame(local_frame, base_url_ + "foo.html");
  EXPECT_EQ(WebURL(SecurityOrigin::UrlWithUniqueSecurityOrigin()),
            local_frame->GetDocument().FirstPartyForCookies());

  SchemeRegistry::RegisterURLSchemeAsFirstPartyWhenTopLevel("http");
  EXPECT_EQ(WebURL(ToKURL(not_base_url_)),
            local_frame->GetDocument().FirstPartyForCookies());
  SchemeRegistry::RemoveURLSchemeAsFirstPartyWhenTopLevel("http");

  view->Close();
}

// See https://crbug.com/525285.
TEST_P(ParameterizedWebFrameTest,
       RemoteToLocalSwapOnMainFrameInitializesCoreFrame) {
  FrameTestHelpers::TestWebViewClient view_client;
  FrameTestHelpers::TestWebRemoteFrameClient remote_client;
  WebView* view = WebView::Create(&view_client, kWebPageVisibilityStateVisible);
  view->SetMainFrame(remote_client.GetFrame());
  WebRemoteFrame* remote_root = view->MainFrame()->ToWebRemoteFrame();
  remote_root->SetReplicatedOrigin(SecurityOrigin::CreateUnique());

  FrameTestHelpers::CreateLocalChild(remote_root);

  // Do a remote-to-local swap of the top frame.
  FrameTestHelpers::TestWebFrameClient local_client;
  WebLocalFrame* local_root = WebLocalFrame::CreateProvisional(
      &local_client, nullptr, nullptr, remote_root, WebSandboxFlags::kNone);
  FrameTestHelpers::TestWebWidgetClient web_widget_client;
  WebFrameWidget::Create(&web_widget_client, local_root);
  remote_root->Swap(local_root);

  // Load a page with a child frame in the new root to make sure this doesn't
  // crash when the child frame invokes setCoreFrame.
  RegisterMockedHttpURLLoad("single_iframe.html");
  RegisterMockedHttpURLLoad("visible_iframe.html");
  FrameTestHelpers::LoadFrame(local_root, base_url_ + "single_iframe.html");

  view->Close();
}

// See https://crbug.com/628942.
TEST_P(ParameterizedWebFrameTest, SuspendedPageLoadWithRemoteMainFrame) {
  // Prepare a page with a remote main frame.
  FrameTestHelpers::TestWebViewClient view_client;
  FrameTestHelpers::TestWebRemoteFrameClient remote_client;
  WebView* view = WebView::Create(&view_client, kWebPageVisibilityStateVisible);
  view->SetMainFrame(remote_client.GetFrame());
  WebRemoteFrame* remote_root = view->MainFrame()->ToWebRemoteFrame();
  remote_root->SetReplicatedOrigin(SecurityOrigin::CreateUnique());

  // Check that ScopedPageSuspender properly triggers deferred loading for
  // the current Page.
  Page* page = remote_root->ToImplBase()->GetFrame()->GetPage();
  EXPECT_FALSE(page->Suspended());
  {
    ScopedPageSuspender suspender;
    EXPECT_TRUE(page->Suspended());
  }
  EXPECT_FALSE(page->Suspended());

  // Repeat this for a page with a local child frame, and ensure that the
  // child frame's loads are also suspended.
  WebLocalFrame* web_local_child =
      FrameTestHelpers::CreateLocalChild(remote_root);
  RegisterMockedHttpURLLoad("foo.html");
  FrameTestHelpers::LoadFrame(web_local_child, base_url_ + "foo.html");
  LocalFrame* local_child = ToWebLocalFrameImpl(web_local_child)->GetFrame();
  EXPECT_FALSE(page->Suspended());
  EXPECT_FALSE(
      local_child->GetDocument()->Fetcher()->Context().DefersLoading());
  {
    ScopedPageSuspender suspender;
    EXPECT_TRUE(page->Suspended());
    EXPECT_TRUE(
        local_child->GetDocument()->Fetcher()->Context().DefersLoading());
  }
  EXPECT_FALSE(page->Suspended());
  EXPECT_FALSE(
      local_child->GetDocument()->Fetcher()->Context().DefersLoading());

  view->Close();
}

class OverscrollWebViewClient : public FrameTestHelpers::TestWebViewClient {
 public:
  MOCK_METHOD4(DidOverscroll,
               void(const WebFloatSize&,
                    const WebFloatSize&,
                    const WebFloatPoint&,
                    const WebFloatSize&));
};

class WebFrameOverscrollTest
    : public WebFrameTest,
      public ::testing::WithParamInterface<blink::WebGestureDevice> {
 protected:
  WebCoalescedInputEvent GenerateEvent(WebInputEvent::Type type,
                                       float delta_x = 0.0,
                                       float delta_y = 0.0) {
    WebGestureEvent event(type, WebInputEvent::kNoModifiers,
                          WebInputEvent::kTimeStampForTesting);
    // TODO(wjmaclean): Make sure that touchpad device is only ever used for
    // gesture scrolling event types.
    event.source_device = GetParam();
    event.x = 100;
    event.y = 100;
    if (type == WebInputEvent::kGestureScrollUpdate) {
      event.data.scroll_update.delta_x = delta_x;
      event.data.scroll_update.delta_y = delta_y;
    }
    return WebCoalescedInputEvent(event);
  }

  void ScrollBegin(FrameTestHelpers::WebViewHelper* web_view_helper) {
    web_view_helper->WebView()->HandleInputEvent(
        GenerateEvent(WebInputEvent::kGestureScrollBegin));
  }

  void ScrollUpdate(FrameTestHelpers::WebViewHelper* web_view_helper,
                    float delta_x,
                    float delta_y) {
    web_view_helper->WebView()->HandleInputEvent(
        GenerateEvent(WebInputEvent::kGestureScrollUpdate, delta_x, delta_y));
  }

  void ScrollEnd(FrameTestHelpers::WebViewHelper* web_view_helper) {
    web_view_helper->WebView()->HandleInputEvent(
        GenerateEvent(WebInputEvent::kGestureScrollEnd));
  }
};

INSTANTIATE_TEST_CASE_P(All,
                        WebFrameOverscrollTest,
                        ::testing::Values(kWebGestureDeviceTouchpad,
                                          kWebGestureDeviceTouchscreen));

TEST_P(WebFrameOverscrollTest,
       AccumulatedRootOverscrollAndUnsedDeltaValuesOnOverscroll) {
  OverscrollWebViewClient client;
  RegisterMockedHttpURLLoad("overscroll/overscroll.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "overscroll/overscroll.html",
                                    true, nullptr, &client, nullptr,
                                    ConfigureAndroid);
  web_view_helper.Resize(WebSize(200, 200));

  // Calculation of accumulatedRootOverscroll and unusedDelta on multiple
  // scrollUpdate.
  ScrollBegin(&web_view_helper);
  EXPECT_CALL(client, DidOverscroll(WebFloatSize(8, 16), WebFloatSize(8, 16),
                                    WebFloatPoint(100, 100), WebFloatSize()));
  ScrollUpdate(&web_view_helper, -308, -316);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client, DidOverscroll(WebFloatSize(0, 13), WebFloatSize(8, 29),
                                    WebFloatPoint(100, 100), WebFloatSize()));
  ScrollUpdate(&web_view_helper, 0, -13);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client, DidOverscroll(WebFloatSize(20, 13), WebFloatSize(28, 42),
                                    WebFloatPoint(100, 100), WebFloatSize()));
  ScrollUpdate(&web_view_helper, -20, -13);
  Mock::VerifyAndClearExpectations(&client);

  // Overscroll is not reported.
  EXPECT_CALL(client, DidOverscroll(_, _, _, _)).Times(0);
  ScrollUpdate(&web_view_helper, 0, 1);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client, DidOverscroll(_, _, _, _)).Times(0);
  ScrollUpdate(&web_view_helper, 1, 0);
  Mock::VerifyAndClearExpectations(&client);

  // Overscroll is reported.
  EXPECT_CALL(client,
              DidOverscroll(WebFloatSize(0, -701), WebFloatSize(0, -701),
                            WebFloatPoint(100, 100), WebFloatSize()));
  ScrollUpdate(&web_view_helper, 0, 1000);
  Mock::VerifyAndClearExpectations(&client);

  // Overscroll is not reported.
  EXPECT_CALL(client, DidOverscroll(_, _, _, _)).Times(0);
  ScrollEnd(&web_view_helper);
  Mock::VerifyAndClearExpectations(&client);
}

TEST_P(WebFrameOverscrollTest,
       AccumulatedOverscrollAndUnusedDeltaValuesOnDifferentAxesOverscroll) {
  OverscrollWebViewClient client;
  RegisterMockedHttpURLLoad("overscroll/div-overscroll.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "overscroll/div-overscroll.html", true, nullptr, &client,
      nullptr, ConfigureAndroid);
  web_view_helper.Resize(WebSize(200, 200));

  ScrollBegin(&web_view_helper);

  // Scroll the Div to the end.
  EXPECT_CALL(client, DidOverscroll(_, _, _, _)).Times(0);
  ScrollUpdate(&web_view_helper, 0, -316);
  Mock::VerifyAndClearExpectations(&client);

  ScrollEnd(&web_view_helper);
  ScrollBegin(&web_view_helper);

  // Now On Scrolling DIV, scroll is bubbled and root layer is over-scrolled.
  EXPECT_CALL(client, DidOverscroll(WebFloatSize(0, 100), WebFloatSize(0, 100),
                                    WebFloatPoint(100, 100), WebFloatSize()));
  ScrollUpdate(&web_view_helper, 0, -100);
  ScrollUpdate(&web_view_helper, 0, -100);
  Mock::VerifyAndClearExpectations(&client);

  // TODO(bokan): This has never worked but by the accident that this test was
  // being run in a WebView without a size. This test should be fixed along with
  // the bug, crbug.com/589320.
  // Page scrolls vertically, but over-scrolls horizontally.
  // EXPECT_CALL(client, didOverscroll(WebFloatSize(-100, 0), WebFloatSize(-100,
  // 0), WebFloatPoint(100, 100), WebFloatSize()));
  // ScrollUpdate(&webViewHelper, 100, 50);
  // Mock::VerifyAndClearExpectations(&client);

  // Scrolling up, Overscroll is not reported.
  // EXPECT_CALL(client, didOverscroll(_, _, _, _)).Times(0);
  // ScrollUpdate(&webViewHelper, 0, -50);
  // Mock::VerifyAndClearExpectations(&client);

  // Page scrolls horizontally, but over-scrolls vertically.
  // EXPECT_CALL(client, didOverscroll(WebFloatSize(0, 100), WebFloatSize(0,
  // 100), WebFloatPoint(100, 100), WebFloatSize()));
  // ScrollUpdate(&webViewHelper, -100, -100);
  // Mock::VerifyAndClearExpectations(&client);
}

TEST_P(WebFrameOverscrollTest, RootLayerOverscrolledOnInnerDivOverScroll) {
  OverscrollWebViewClient client;
  RegisterMockedHttpURLLoad("overscroll/div-overscroll.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "overscroll/div-overscroll.html", true, nullptr, &client,
      nullptr, ConfigureAndroid);
  web_view_helper.Resize(WebSize(200, 200));

  ScrollBegin(&web_view_helper);

  // Scroll the Div to the end.
  EXPECT_CALL(client, DidOverscroll(_, _, _, _)).Times(0);
  ScrollUpdate(&web_view_helper, 0, -316);
  Mock::VerifyAndClearExpectations(&client);

  ScrollEnd(&web_view_helper);
  ScrollBegin(&web_view_helper);

  // Now On Scrolling DIV, scroll is bubbled and root layer is over-scrolled.
  EXPECT_CALL(client, DidOverscroll(WebFloatSize(0, 50), WebFloatSize(0, 50),
                                    WebFloatPoint(100, 100), WebFloatSize()));
  ScrollUpdate(&web_view_helper, 0, -150);
  Mock::VerifyAndClearExpectations(&client);
}

TEST_P(WebFrameOverscrollTest, RootLayerOverscrolledOnInnerIFrameOverScroll) {
  OverscrollWebViewClient client;
  RegisterMockedHttpURLLoad("overscroll/iframe-overscroll.html");
  RegisterMockedHttpURLLoad("overscroll/scrollable-iframe.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "overscroll/iframe-overscroll.html", true, nullptr, &client,
      nullptr, ConfigureAndroid);
  web_view_helper.Resize(WebSize(200, 200));

  ScrollBegin(&web_view_helper);
  // Scroll the IFrame to the end.
  EXPECT_CALL(client, DidOverscroll(_, _, _, _)).Times(0);

  // This scroll will fully scroll the iframe but will be consumed before being
  // counted as overscroll.
  ScrollUpdate(&web_view_helper, 0, -320);

  // This scroll will again target the iframe but wont bubble further up. Make
  // sure that the unused scroll isn't handled as overscroll.
  ScrollUpdate(&web_view_helper, 0, -50);
  Mock::VerifyAndClearExpectations(&client);

  ScrollEnd(&web_view_helper);
  ScrollBegin(&web_view_helper);

  // Now On Scrolling IFrame, scroll is bubbled and root layer is over-scrolled.
  EXPECT_CALL(client, DidOverscroll(WebFloatSize(0, 50), WebFloatSize(0, 50),
                                    WebFloatPoint(100, 100), WebFloatSize()));
  ScrollUpdate(&web_view_helper, 0, -150);
  Mock::VerifyAndClearExpectations(&client);

  ScrollEnd(&web_view_helper);
}

TEST_P(WebFrameOverscrollTest, ScaledPageRootLayerOverscrolled) {
  OverscrollWebViewClient client;
  RegisterMockedHttpURLLoad("overscroll/overscroll.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "overscroll/overscroll.html", true, nullptr, &client, nullptr,
      ConfigureAndroid);
  web_view_helper.Resize(WebSize(200, 200));
  web_view_impl->SetPageScaleFactor(3.0);

  // Calculation of accumulatedRootOverscroll and unusedDelta on scaled page.
  // The point is (99, 99) because we clamp in the division by 3 to 33 so when
  // we go back to viewport coordinates it becomes (99, 99).
  ScrollBegin(&web_view_helper);
  EXPECT_CALL(client, DidOverscroll(WebFloatSize(0, -30), WebFloatSize(0, -30),
                                    WebFloatPoint(99, 99), WebFloatSize()));
  ScrollUpdate(&web_view_helper, 0, 30);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client, DidOverscroll(WebFloatSize(0, -30), WebFloatSize(0, -60),
                                    WebFloatPoint(99, 99), WebFloatSize()));
  ScrollUpdate(&web_view_helper, 0, 30);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client,
              DidOverscroll(WebFloatSize(-30, -30), WebFloatSize(-30, -90),
                            WebFloatPoint(99, 99), WebFloatSize()));
  ScrollUpdate(&web_view_helper, 30, 30);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client,
              DidOverscroll(WebFloatSize(-30, 0), WebFloatSize(-60, -90),
                            WebFloatPoint(99, 99), WebFloatSize()));
  ScrollUpdate(&web_view_helper, 30, 0);
  Mock::VerifyAndClearExpectations(&client);

  // Overscroll is not reported.
  EXPECT_CALL(client, DidOverscroll(_, _, _, _)).Times(0);
  ScrollEnd(&web_view_helper);
  Mock::VerifyAndClearExpectations(&client);
}

TEST_P(WebFrameOverscrollTest, NoOverscrollForSmallvalues) {
  OverscrollWebViewClient client;
  RegisterMockedHttpURLLoad("overscroll/overscroll.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "overscroll/overscroll.html",
                                    true, nullptr, &client, nullptr,
                                    ConfigureAndroid);
  web_view_helper.Resize(WebSize(200, 200));

  ScrollBegin(&web_view_helper);
  EXPECT_CALL(client,
              DidOverscroll(WebFloatSize(-10, -10), WebFloatSize(-10, -10),
                            WebFloatPoint(100, 100), WebFloatSize()));
  ScrollUpdate(&web_view_helper, 10, 10);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client,
              DidOverscroll(WebFloatSize(0, -0.10), WebFloatSize(-10, -10.10),
                            WebFloatPoint(100, 100), WebFloatSize()));
  ScrollUpdate(&web_view_helper, 0, 0.10);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client, DidOverscroll(WebFloatSize(-0.10, 0),
                                    WebFloatSize(-10.10, -10.10),
                                    WebFloatPoint(100, 100), WebFloatSize()));
  ScrollUpdate(&web_view_helper, 0.10, 0);
  Mock::VerifyAndClearExpectations(&client);

  // For residual values overscrollDelta should be reset and didOverscroll
  // shouldn't be called.
  EXPECT_CALL(client, DidOverscroll(_, _, _, _)).Times(0);
  ScrollUpdate(&web_view_helper, 0, 0.09);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client, DidOverscroll(_, _, _, _)).Times(0);
  ScrollUpdate(&web_view_helper, 0.09, 0.09);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client, DidOverscroll(_, _, _, _)).Times(0);
  ScrollUpdate(&web_view_helper, 0.09, 0);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client, DidOverscroll(_, _, _, _)).Times(0);
  ScrollUpdate(&web_view_helper, 0, -0.09);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client, DidOverscroll(_, _, _, _)).Times(0);
  ScrollUpdate(&web_view_helper, -0.09, -0.09);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client, DidOverscroll(_, _, _, _)).Times(0);
  ScrollUpdate(&web_view_helper, -0.09, 0);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client, DidOverscroll(_, _, _, _)).Times(0);
  ScrollEnd(&web_view_helper);
  Mock::VerifyAndClearExpectations(&client);
}

TEST_F(WebFrameTest, OrientationFrameDetach) {
  RuntimeEnabledFeatures::setOrientationEventEnabled(true);
  RegisterMockedHttpURLLoad("orientation-frame-detach.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "orientation-frame-detach.html", true);
  web_view_impl->MainFrameImpl()->SendOrientationChangeEvent();
}

TEST_F(WebFrameTest, MaxFramesDetach) {
  RegisterMockedHttpURLLoad("max-frames-detach.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.InitializeAndLoad(
      base_url_ + "max-frames-detach.html", true);
  web_view_impl->MainFrameImpl()->CollectGarbage();
}

TEST_F(WebFrameTest, ImageDocumentLoadFinishTime) {
  // Loading an image resource directly generates an ImageDocument with
  // the document loader feeding image data into the resource of a generated
  // img tag. We expect the load finish time to be the same for the document
  // and the image resource.

  RegisterMockedHttpURLLoadWithMimeType("white-1x1.png", "image/png");
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "white-1x1.png");
  WebViewImpl* web_view = web_view_helper.WebView();
  Document* document = web_view->MainFrameImpl()->GetFrame()->GetDocument();

  EXPECT_TRUE(document);
  EXPECT_TRUE(document->IsImageDocument());

  ImageDocument* img_document = ToImageDocument(document);
  ImageResource* resource = img_document->CachedImageResourceDeprecated();

  EXPECT_TRUE(resource);
  EXPECT_NE(0, resource->LoadFinishTime());

  DocumentLoader* loader = document->Loader();

  EXPECT_TRUE(loader);
  EXPECT_EQ(loader->GetTiming().ResponseEnd(), resource->LoadFinishTime());
}

class CallbackOrderingWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  CallbackOrderingWebFrameClient() : callback_count_(0) {}

  void DidStartLoading(bool to_different_document) override {
    EXPECT_EQ(0, callback_count_++);
    FrameTestHelpers::TestWebFrameClient::DidStartLoading(
        to_different_document);
  }
  void DidStartProvisionalLoad(WebDataSource*, WebURLRequest&) override {
    EXPECT_EQ(1, callback_count_++);
  }
  void DidCommitProvisionalLoad(const WebHistoryItem&,
                                WebHistoryCommitType) override {
    EXPECT_EQ(2, callback_count_++);
  }
  void DidFinishDocumentLoad(WebLocalFrame*) override {
    EXPECT_EQ(3, callback_count_++);
  }
  void DidHandleOnloadEvents() override { EXPECT_EQ(4, callback_count_++); }
  void DidFinishLoad() override { EXPECT_EQ(5, callback_count_++); }
  void DidStopLoading() override {
    EXPECT_EQ(6, callback_count_++);
    FrameTestHelpers::TestWebFrameClient::DidStopLoading();
  }

 private:
  int callback_count_;
};

TEST_F(WebFrameTest, CallbackOrdering) {
  RegisterMockedHttpURLLoad("foo.html");
  CallbackOrderingWebFrameClient client;
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "foo.html", true, &client);
}

class TestWebRemoteFrameClientForVisibility
    : public FrameTestHelpers::TestWebRemoteFrameClient {
 public:
  TestWebRemoteFrameClientForVisibility() : visible_(true) {}
  void VisibilityChanged(bool visible) override { visible_ = visible; }

  bool IsVisible() const { return visible_; }

 private:
  bool visible_;
};

class WebFrameVisibilityChangeTest : public WebFrameTest {
 public:
  WebFrameVisibilityChangeTest() {
    RegisterMockedHttpURLLoad("visible_iframe.html");
    RegisterMockedHttpURLLoad("single_iframe.html");
    frame_ = web_view_helper_
                 .InitializeAndLoad(base_url_ + "single_iframe.html", true)
                 ->MainFrame();
    web_remote_frame_ = RemoteFrameClient()->GetFrame();
  }

  ~WebFrameVisibilityChangeTest() {}

  void ExecuteScriptOnMainFrame(const WebScriptSource& script) {
    MainFrame()->ExecuteScript(script);
    MainFrame()->View()->UpdateAllLifecyclePhases();
    RunPendingTasks();
  }

  void SwapLocalFrameToRemoteFrame() {
    LastChild(MainFrame())->Swap(RemoteFrame());
    RemoteFrame()->SetReplicatedOrigin(SecurityOrigin::CreateUnique());
  }

  WebFrame* MainFrame() { return frame_; }
  WebRemoteFrameImpl* RemoteFrame() { return web_remote_frame_; }
  TestWebRemoteFrameClientForVisibility* RemoteFrameClient() {
    return &remote_frame_client_;
  }

 private:
  TestWebRemoteFrameClientForVisibility remote_frame_client_;
  FrameTestHelpers::WebViewHelper web_view_helper_;
  WebFrame* frame_;
  Persistent<WebRemoteFrameImpl> web_remote_frame_;
};

TEST_F(WebFrameVisibilityChangeTest, RemoteFrameVisibilityChange) {
  SwapLocalFrameToRemoteFrame();
  ExecuteScriptOnMainFrame(WebScriptSource(
      "document.querySelector('iframe').style.display = 'none';"));
  EXPECT_FALSE(RemoteFrameClient()->IsVisible());

  ExecuteScriptOnMainFrame(WebScriptSource(
      "document.querySelector('iframe').style.display = 'block';"));
  EXPECT_TRUE(RemoteFrameClient()->IsVisible());
}

TEST_F(WebFrameVisibilityChangeTest, RemoteFrameParentVisibilityChange) {
  SwapLocalFrameToRemoteFrame();
  ExecuteScriptOnMainFrame(
      WebScriptSource("document.querySelector('iframe').parentElement.style."
                      "display = 'none';"));
  EXPECT_FALSE(RemoteFrameClient()->IsVisible());
}

static void EnableGlobalReuseForUnownedMainFrames(WebSettings* settings) {
  settings->SetShouldReuseGlobalForUnownedMainFrame(true);
}

// A main frame with no opener should have a unique security origin. Thus, the
// global should never be reused on the initial navigation.
TEST(WebFrameGlobalReuseTest, MainFrameWithNoOpener) {
  FrameTestHelpers::WebViewHelper helper;
  helper.Initialize(true);

  WebLocalFrame* main_frame = helper.WebView()->MainFrameImpl();
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  main_frame->ExecuteScript(WebScriptSource("hello = 'world';"));
  FrameTestHelpers::LoadFrame(main_frame, "data:text/html,new page");
  v8::Local<v8::Value> result =
      main_frame->ExecuteScriptAndReturnValue(WebScriptSource("hello"));
  EXPECT_TRUE(result.IsEmpty());
}

// Child frames should never reuse the global on a cross-origin navigation, even
// if the setting is enabled. It's not safe to since the parent could have
// injected script before the initial navigation.
TEST(WebFrameGlobalReuseTest, ChildFrame) {
  FrameTestHelpers::WebViewHelper helper;
  helper.Initialize(true, nullptr, nullptr, nullptr,
                    EnableGlobalReuseForUnownedMainFrames);

  WebLocalFrame* main_frame = helper.WebView()->MainFrameImpl();
  FrameTestHelpers::LoadFrame(main_frame, "data:text/html,<iframe></iframe>");

  WebLocalFrame* child_frame = main_frame->FirstChild()->ToWebLocalFrame();
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  child_frame->ExecuteScript(WebScriptSource("hello = 'world';"));
  FrameTestHelpers::LoadFrame(child_frame, "data:text/html,new page");
  v8::Local<v8::Value> result =
      child_frame->ExecuteScriptAndReturnValue(WebScriptSource("hello"));
  EXPECT_TRUE(result.IsEmpty());
}

// A main frame with an opener should never reuse the global on a cross-origin
// navigation, even if the setting is enabled. It's not safe to since the opener
// could have injected script.
TEST(WebFrameGlobalReuseTest, MainFrameWithOpener) {
  FrameTestHelpers::TestWebViewClient opener_web_view_client;
  FrameTestHelpers::WebViewHelper opener_helper;
  opener_helper.Initialize(false, nullptr, &opener_web_view_client, nullptr);
  FrameTestHelpers::WebViewHelper helper;
  helper.InitializeWithOpener(opener_helper.WebView()->MainFrame(), true,
                              nullptr, nullptr, nullptr,
                              EnableGlobalReuseForUnownedMainFrames);

  WebLocalFrame* main_frame = helper.WebView()->MainFrameImpl();
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  main_frame->ExecuteScript(WebScriptSource("hello = 'world';"));
  FrameTestHelpers::LoadFrame(main_frame, "data:text/html,new page");
  v8::Local<v8::Value> result =
      main_frame->ExecuteScriptAndReturnValue(WebScriptSource("hello"));
  EXPECT_TRUE(result.IsEmpty());
}

// A main frame that is unrelated to any other frame /can/ reuse the global if
// the setting is enabled. In this case, it's impossible for any other frames to
// have touched the global. Only the embedder could have injected script, and
// the embedder enabling this setting is a signal that the injected script needs
// to persist on the first navigation away from the initial empty document.
TEST(WebFrameGlobalReuseTest, ReuseForMainFrameIfEnabled) {
  FrameTestHelpers::WebViewHelper helper;
  helper.Initialize(true, nullptr, nullptr, nullptr,
                    EnableGlobalReuseForUnownedMainFrames);

  WebLocalFrame* main_frame = helper.WebView()->MainFrameImpl();
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  main_frame->ExecuteScript(WebScriptSource("hello = 'world';"));
  FrameTestHelpers::LoadFrame(main_frame, "data:text/html,new page");
  v8::Local<v8::Value> result =
      main_frame->ExecuteScriptAndReturnValue(WebScriptSource("hello"));
  ASSERT_TRUE(result->IsString());
  EXPECT_EQ("world",
            ToCoreString(result->ToString(main_frame->MainWorldScriptContext())
                             .ToLocalChecked()));
}

class SaveImageFromDataURLWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  // WebFrameClient methods
  void SaveImageFromDataURL(const WebString& data_url) override {
    data_url_ = data_url;
  }

  // Local methods
  const WebString& Result() const { return data_url_; }
  void Reset() { data_url_ = WebString(); }

 private:
  WebString data_url_;
};

TEST_F(WebFrameTest, SaveImageAt) {
  std::string url = base_url_ + "image-with-data-url.html";
  RegisterMockedURLLoadFromBase(base_url_, "image-with-data-url.html");
  URLTestHelpers::RegisterMockedURLLoad(
      ToKURL("http://test"), testing::WebTestDataPath("white-1x1.png"));

  FrameTestHelpers::WebViewHelper helper;
  SaveImageFromDataURLWebFrameClient client;
  WebViewImpl* web_view = helper.InitializeAndLoad(url, true, &client);
  web_view->Resize(WebSize(400, 400));
  web_view->UpdateAllLifecyclePhases();

  WebLocalFrame* local_frame = web_view->MainFrameImpl();

  client.Reset();
  local_frame->SaveImageAt(WebPoint(1, 1));
  EXPECT_EQ(
      WebString::FromUTF8("data:image/gif;base64"
                          ",R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs="),
      client.Result());

  client.Reset();
  local_frame->SaveImageAt(WebPoint(1, 2));
  EXPECT_EQ(WebString(), client.Result());

  web_view->SetPageScaleFactor(4);
  web_view->SetVisualViewportOffset(WebFloatPoint(1, 1));

  client.Reset();
  local_frame->SaveImageAt(WebPoint(3, 3));
  EXPECT_EQ(
      WebString::FromUTF8("data:image/gif;base64"
                          ",R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs="),
      client.Result());

  // Explicitly reset to break dependency on locally scoped client.
  helper.Reset();
}

TEST_F(WebFrameTest, SaveImageWithImageMap) {
  std::string url = base_url_ + "image-map.html";
  RegisterMockedURLLoadFromBase(base_url_, "image-map.html");

  FrameTestHelpers::WebViewHelper helper;
  SaveImageFromDataURLWebFrameClient client;
  WebViewImpl* web_view = helper.InitializeAndLoad(url, true, &client);
  web_view->Resize(WebSize(400, 400));

  WebLocalFrame* local_frame = web_view->MainFrameImpl();

  client.Reset();
  local_frame->SaveImageAt(WebPoint(25, 25));
  EXPECT_EQ(
      WebString::FromUTF8("data:image/gif;base64"
                          ",R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs="),
      client.Result());

  client.Reset();
  local_frame->SaveImageAt(WebPoint(75, 25));
  EXPECT_EQ(
      WebString::FromUTF8("data:image/gif;base64"
                          ",R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs="),
      client.Result());

  client.Reset();
  local_frame->SaveImageAt(WebPoint(125, 25));
  EXPECT_EQ(WebString(), client.Result());

  // Explicitly reset to break dependency on locally scoped client.
  helper.Reset();
}

TEST_F(WebFrameTest, CopyImageAt) {
  std::string url = base_url_ + "canvas-copy-image.html";
  RegisterMockedURLLoadFromBase(base_url_, "canvas-copy-image.html");

  FrameTestHelpers::WebViewHelper helper;
  WebViewImpl* web_view = helper.InitializeAndLoad(url, true, 0);
  web_view->Resize(WebSize(400, 400));

  uint64_t sequence = Platform::Current()->Clipboard()->SequenceNumber(
      WebClipboard::kBufferStandard);

  WebLocalFrame* local_frame = web_view->MainFrameImpl();
  local_frame->CopyImageAt(WebPoint(50, 50));

  EXPECT_NE(sequence, Platform::Current()->Clipboard()->SequenceNumber(
                          WebClipboard::kBufferStandard));

  WebImage image =
      static_cast<WebMockClipboard*>(Platform::Current()->Clipboard())
          ->ReadRawImage(WebClipboard::Buffer());

  EXPECT_EQ(SkColorSetARGB(255, 255, 0, 0), image.GetSkBitmap().getColor(0, 0));
};

TEST_F(WebFrameTest, CopyImageAtWithPinchZoom) {
  std::string url = base_url_ + "canvas-copy-image.html";
  RegisterMockedURLLoadFromBase(base_url_, "canvas-copy-image.html");

  FrameTestHelpers::WebViewHelper helper;
  WebViewImpl* web_view = helper.InitializeAndLoad(url, true, 0);
  web_view->Resize(WebSize(400, 400));
  web_view->UpdateAllLifecyclePhases();
  web_view->SetPageScaleFactor(2);
  web_view->SetVisualViewportOffset(WebFloatPoint(200, 200));

  uint64_t sequence = Platform::Current()->Clipboard()->SequenceNumber(
      WebClipboard::kBufferStandard);

  WebLocalFrame* local_frame = web_view->MainFrameImpl();
  local_frame->CopyImageAt(WebPoint(0, 0));

  EXPECT_NE(sequence, Platform::Current()->Clipboard()->SequenceNumber(
                          WebClipboard::kBufferStandard));

  WebImage image =
      static_cast<WebMockClipboard*>(Platform::Current()->Clipboard())
          ->ReadRawImage(WebClipboard::Buffer());

  EXPECT_EQ(SkColorSetARGB(255, 255, 0, 0), image.GetSkBitmap().getColor(0, 0));
};

TEST_F(WebFrameTest, CopyImageWithImageMap) {
  SaveImageFromDataURLWebFrameClient client;

  std::string url = base_url_ + "image-map.html";
  RegisterMockedURLLoadFromBase(base_url_, "image-map.html");

  FrameTestHelpers::WebViewHelper helper;
  WebViewImpl* web_view = helper.InitializeAndLoad(url, true, &client);
  web_view->Resize(WebSize(400, 400));

  client.Reset();
  WebLocalFrame* local_frame = web_view->MainFrameImpl();
  local_frame->SaveImageAt(WebPoint(25, 25));
  EXPECT_EQ(
      WebString::FromUTF8("data:image/gif;base64"
                          ",R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs="),
      client.Result());

  client.Reset();
  local_frame->SaveImageAt(WebPoint(75, 25));
  EXPECT_EQ(
      WebString::FromUTF8("data:image/gif;base64"
                          ",R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs="),
      client.Result());

  client.Reset();
  local_frame->SaveImageAt(WebPoint(125, 25));
  EXPECT_EQ(WebString(), client.Result());
  // Explicitly reset to break dependency on locally scoped client.
  helper.Reset();
}

TEST_F(WebFrameTest, LoadJavascriptURLInNewFrame) {
  FrameTestHelpers::WebViewHelper helper;
  helper.Initialize(true);

  std::string redirect_url = base_url_ + "foo.html";
  URLTestHelpers::RegisterMockedURLLoad(ToKURL(redirect_url),
                                        testing::WebTestDataPath("foo.html"));
  WebURLRequest request(ToKURL("javascript:location='" + redirect_url + "'"));
  helper.WebView()->MainFrameImpl()->LoadRequest(request);

  // Normally, the result of the JS url replaces the existing contents on the
  // Document. However, if the JS triggers a navigation, the contents should
  // not be replaced.
  EXPECT_EQ("", ToLocalFrame(helper.WebView()->GetPage()->MainFrame())
                    ->GetDocument()
                    ->documentElement()
                    ->innerText());
}

class TestResourcePriorityWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  class ExpectedRequest {
   public:
    ExpectedRequest(const KURL& url, WebURLRequest::Priority priority)
        : url(url), priority(priority), seen(false) {}

    KURL url;
    WebURLRequest::Priority priority;
    bool seen;
  };

  TestResourcePriorityWebFrameClient() {}

  void WillSendRequest(WebURLRequest& request) override {
    ExpectedRequest* expected_request = expected_requests_.at(request.Url());
    DCHECK(expected_request);
    EXPECT_EQ(expected_request->priority, request.GetPriority());
    expected_request->seen = true;
  }

  void AddExpectedRequest(const KURL& url, WebURLRequest::Priority priority) {
    expected_requests_.insert(url,
                              WTF::MakeUnique<ExpectedRequest>(url, priority));
  }

  void VerifyAllRequests() {
    for (const auto& request : expected_requests_)
      EXPECT_TRUE(request.value->seen);
  }

 private:
  HashMap<KURL, std::unique_ptr<ExpectedRequest>> expected_requests_;
};

TEST_F(WebFrameTest, ChangeResourcePriority) {
  TestResourcePriorityWebFrameClient client;
  RegisterMockedHttpURLLoad("promote_img_in_viewport_priority.html");
  RegisterMockedHttpURLLoad("image_slow.pl");
  RegisterMockedHttpURLLoad("image_slow_out_of_viewport.pl");
  client.AddExpectedRequest(
      ToKURL("http://internal.test/promote_img_in_viewport_priority.html"),
      WebURLRequest::kPriorityVeryHigh);
  client.AddExpectedRequest(ToKURL("http://internal.test/image_slow.pl"),
                            WebURLRequest::kPriorityLow);
  client.AddExpectedRequest(
      ToKURL("http://internal.test/image_slow_out_of_viewport.pl"),
      WebURLRequest::kPriorityLow);

  FrameTestHelpers::WebViewHelper helper;
  helper.Initialize(true, &client);
  helper.Resize(WebSize(640, 480));
  FrameTestHelpers::LoadFrame(
      helper.WebView()->MainFrame(),
      base_url_ + "promote_img_in_viewport_priority.html");

  // Ensure the image in the viewport got promoted after the request was sent.
  Resource* image = ToWebLocalFrameImpl(helper.WebView()->MainFrame())
                        ->GetFrame()
                        ->GetDocument()
                        ->Fetcher()
                        ->AllResources()
                        .at(ToKURL("http://internal.test/image_slow.pl"));
  DCHECK(image);
  EXPECT_EQ(kResourceLoadPriorityHigh, image->GetResourceRequest().Priority());

  client.VerifyAllRequests();
}

TEST_F(WebFrameTest, ScriptPriority) {
  TestResourcePriorityWebFrameClient client;
  RegisterMockedHttpURLLoad("script_priority.html");
  RegisterMockedHttpURLLoad("priorities/defer.js");
  RegisterMockedHttpURLLoad("priorities/async.js");
  RegisterMockedHttpURLLoad("priorities/head.js");
  RegisterMockedHttpURLLoad("priorities/document-write.js");
  RegisterMockedHttpURLLoad("priorities/injected.js");
  RegisterMockedHttpURLLoad("priorities/injected-async.js");
  RegisterMockedHttpURLLoad("priorities/body.js");
  client.AddExpectedRequest(ToKURL("http://internal.test/script_priority.html"),
                            WebURLRequest::kPriorityVeryHigh);
  client.AddExpectedRequest(ToKURL("http://internal.test/priorities/defer.js"),
                            WebURLRequest::kPriorityLow);
  client.AddExpectedRequest(ToKURL("http://internal.test/priorities/async.js"),
                            WebURLRequest::kPriorityLow);
  client.AddExpectedRequest(ToKURL("http://internal.test/priorities/head.js"),
                            WebURLRequest::kPriorityHigh);
  client.AddExpectedRequest(
      ToKURL("http://internal.test/priorities/document-write.js"),
      WebURLRequest::kPriorityHigh);
  client.AddExpectedRequest(
      ToKURL("http://internal.test/priorities/injected.js"),
      WebURLRequest::kPriorityLow);
  client.AddExpectedRequest(
      ToKURL("http://internal.test/priorities/injected-async.js"),
      WebURLRequest::kPriorityLow);
  client.AddExpectedRequest(ToKURL("http://internal.test/priorities/body.js"),
                            WebURLRequest::kPriorityHigh);

  FrameTestHelpers::WebViewHelper helper;
  helper.InitializeAndLoad(base_url_ + "script_priority.html", true, &client);
  client.VerifyAllRequests();
}

class MultipleDataChunkDelegate : public WebURLLoaderTestDelegate {
 public:
  void DidReceiveData(WebURLLoaderClient* original_client,
                      const char* data,
                      int data_length) override {
    EXPECT_GT(data_length, 16);
    original_client->DidReceiveData(data, 16);
    // This didReceiveData call shouldn't crash due to a failed assertion.
    original_client->DidReceiveData(data + 16, data_length - 16);
  }
};

TEST_F(WebFrameTest, ImageDocumentDecodeError) {
  std::string url = base_url_ + "not_an_image.ico";
  URLTestHelpers::RegisterMockedURLLoad(
      ToKURL(url), testing::WebTestDataPath("not_an_image.ico"),
      "image/x-icon");
  MultipleDataChunkDelegate delegate;
  Platform::Current()->GetURLLoaderMockFactory()->SetLoaderDelegate(&delegate);
  FrameTestHelpers::WebViewHelper helper;
  helper.InitializeAndLoad(url, true);
  Platform::Current()->GetURLLoaderMockFactory()->SetLoaderDelegate(nullptr);

  Document* document =
      ToLocalFrame(helper.WebView()->GetPage()->MainFrame())->GetDocument();
  EXPECT_TRUE(document->IsImageDocument());
  EXPECT_EQ(ResourceStatus::kDecodeError,
            ToImageDocument(document)->CachedImage()->GetStatus());
}

// Ensure that the root layer -- whose size is ordinarily derived from the
// content size -- maintains a minimum height matching the viewport in cases
// where the content is smaller.
TEST_F(WebFrameTest, RootLayerMinimumHeight) {
  constexpr int kViewportWidth = 320;
  constexpr int kViewportHeight = 640;
  constexpr int kBrowserControlsHeight = 100;

  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true, nullptr, nullptr, nullptr,
                             EnableViewportSettings);
  WebViewImpl* web_view = web_view_helper.WebView();
  web_view->ResizeWithBrowserControls(
      WebSize(kViewportWidth, kViewportHeight - kBrowserControlsHeight),
      kBrowserControlsHeight, true);

  InitializeWithHTML(*web_view->MainFrameImpl()->GetFrame(),
                     "<!DOCTYPE html>"
                     "<style>"
                     "  html, body {width:100%;height:540px;margin:0px}"
                     "  #elem {"
                     "    overflow: scroll;"
                     "    width: 100px;"
                     "    height: 10px;"
                     "    position: fixed;"
                     "    left: 0px;"
                     "    bottom: 0px;"
                     "  }"
                     "</style>"
                     "<div id='elem'></div>");
  web_view->UpdateAllLifecyclePhases();

  Document* document = web_view->MainFrameImpl()->GetFrame()->GetDocument();
  FrameView* frame_view = web_view->MainFrameImpl()->GetFrameView();
  PaintLayerCompositor* compositor =
      frame_view->GetLayoutViewItem().Compositor();

  EXPECT_EQ(kViewportHeight - kBrowserControlsHeight,
            compositor->RootLayer()->BoundingBoxForCompositing().Height());

  document->View()->SetTracksPaintInvalidations(true);

  web_view->ResizeWithBrowserControls(WebSize(kViewportWidth, kViewportHeight),
                                      kBrowserControlsHeight, false);

  EXPECT_EQ(kViewportHeight,
            compositor->RootLayer()->BoundingBoxForCompositing().Height());
  EXPECT_EQ(kViewportHeight,
            compositor->RootLayer()->GraphicsLayerBacking()->Size().Height());
  EXPECT_EQ(kViewportHeight, compositor->RootGraphicsLayer()->Size().Height());

  const RasterInvalidationTracking* invalidation_tracking =
      document->GetLayoutView()
          ->Layer()
          ->GraphicsLayerBacking()
          ->GetRasterInvalidationTracking();
  ASSERT_TRUE(invalidation_tracking);
  const auto* raster_invalidations =
      &invalidation_tracking->tracked_raster_invalidations;

  // The newly revealed content at the bottom of the screen should have been
  // invalidated. There are additional invalidations for the position: fixed
  // element.
  EXPECT_GT(raster_invalidations->size(), 0u);
  EXPECT_TRUE((*raster_invalidations)[0].rect.Contains(
      IntRect(0, kViewportHeight - kBrowserControlsHeight, kViewportWidth,
              kBrowserControlsHeight)));

  document->View()->SetTracksPaintInvalidations(false);
}

// Load a page with display:none set and try to scroll it. It shouldn't crash
// due to lack of layoutObject. crbug.com/653327.
TEST_F(WebFrameTest, ScrollBeforeLayoutDoesntCrash) {
  RegisterMockedHttpURLLoad("display-none.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "display-none.html");
  WebViewImpl* web_view = web_view_helper.WebView();
  web_view_helper.Resize(WebSize(640, 480));

  Document* document = web_view->MainFrameImpl()->GetFrame()->GetDocument();
  document->documentElement()->SetLayoutObject(nullptr);

  WebGestureEvent begin_event(WebInputEvent::kGestureScrollBegin,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::kTimeStampForTesting);
  begin_event.source_device = kWebGestureDeviceTouchpad;
  WebGestureEvent update_event(WebInputEvent::kGestureScrollUpdate,
                               WebInputEvent::kNoModifiers,
                               WebInputEvent::kTimeStampForTesting);
  update_event.source_device = kWebGestureDeviceTouchpad;
  WebGestureEvent end_event(WebInputEvent::kGestureScrollEnd,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::kTimeStampForTesting);
  end_event.source_device = kWebGestureDeviceTouchpad;

  // Try GestureScrollEnd and GestureScrollUpdate first to make sure that not
  // seeing a Begin first doesn't break anything. (This currently happens).
  web_view_helper.WebView()->HandleInputEvent(
      WebCoalescedInputEvent(end_event));
  web_view_helper.WebView()->HandleInputEvent(
      WebCoalescedInputEvent(update_event));

  // Try a full Begin/Update/End cycle.
  web_view_helper.WebView()->HandleInputEvent(
      WebCoalescedInputEvent(begin_event));
  web_view_helper.WebView()->HandleInputEvent(
      WebCoalescedInputEvent(update_event));
  web_view_helper.WebView()->HandleInputEvent(
      WebCoalescedInputEvent(end_event));
}

TEST_F(WebFrameTest, HidingScrollbarsOnScrollableAreaDisablesScrollbars) {
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true);
  web_view_helper.Resize(WebSize(800, 600));
  WebViewImpl* web_view = web_view_helper.WebView();

  InitializeWithHTML(
      *web_view->MainFrameImpl()->GetFrame(),
      "<!DOCTYPE html>"
      "<style>"
      "  #scroller { overflow: scroll; width: 1000px; height: 1000px }"
      "  #spacer { width: 2000px; height: 2000px }"
      "</style>"
      "<div id='scroller'>"
      "  <div id='spacer'></div>"
      "</div>");

  Document* document = web_view->MainFrameImpl()->GetFrame()->GetDocument();
  FrameView* frame_view = web_view->MainFrameImpl()->GetFrameView();
  Element* scroller = document->GetElementById("scroller");
  ScrollableArea* scroller_area =
      ToLayoutBox(scroller->GetLayoutObject())->GetScrollableArea();

  ASSERT_TRUE(scroller_area->HorizontalScrollbar());
  ASSERT_TRUE(scroller_area->VerticalScrollbar());
  ASSERT_TRUE(frame_view->HorizontalScrollbar());
  ASSERT_TRUE(frame_view->VerticalScrollbar());

  EXPECT_FALSE(frame_view->ScrollbarsHidden());
  EXPECT_TRUE(
      frame_view->HorizontalScrollbar()->ShouldParticipateInHitTesting());
  EXPECT_TRUE(frame_view->VerticalScrollbar()->ShouldParticipateInHitTesting());

  EXPECT_FALSE(scroller_area->ScrollbarsHidden());
  EXPECT_TRUE(
      scroller_area->HorizontalScrollbar()->ShouldParticipateInHitTesting());
  EXPECT_TRUE(
      scroller_area->VerticalScrollbar()->ShouldParticipateInHitTesting());

  frame_view->SetScrollbarsHidden(true);
  EXPECT_FALSE(
      frame_view->HorizontalScrollbar()->ShouldParticipateInHitTesting());
  EXPECT_FALSE(
      frame_view->VerticalScrollbar()->ShouldParticipateInHitTesting());
  frame_view->SetScrollbarsHidden(false);
  EXPECT_TRUE(
      frame_view->HorizontalScrollbar()->ShouldParticipateInHitTesting());
  EXPECT_TRUE(frame_view->VerticalScrollbar()->ShouldParticipateInHitTesting());

  scroller_area->SetScrollbarsHidden(true);
  EXPECT_FALSE(
      scroller_area->HorizontalScrollbar()->ShouldParticipateInHitTesting());
  EXPECT_FALSE(
      scroller_area->VerticalScrollbar()->ShouldParticipateInHitTesting());
  scroller_area->SetScrollbarsHidden(false);
  EXPECT_TRUE(
      scroller_area->HorizontalScrollbar()->ShouldParticipateInHitTesting());
  EXPECT_TRUE(
      scroller_area->VerticalScrollbar()->ShouldParticipateInHitTesting());
}

TEST_F(WebFrameTest, MouseOverDifferntNodeClearsTooltip) {
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true, nullptr, nullptr, nullptr,
                             [](WebSettings* settings) {});
  web_view_helper.Resize(WebSize(200, 200));
  WebViewImpl* web_view = web_view_helper.WebView();

  InitializeWithHTML(
      *web_view->MainFrameImpl()->GetFrame(),
      "<head>"
      "  <style type='text/css'>"
      "   div"
      "    {"
      "      width: 200px;"
      "      height: 100px;"
      "      background-color: #eeeeff;"
      "    }"
      "    div:hover"
      "    {"
      "      background-color: #ddddff;"
      "    }"
      "  </style>"
      "</head>"
      "<body>"
      "  <div id='div1' title='Title Attribute Value'>Hover HERE</div>"
      "  <div id='div2' title='Title Attribute Value'>Then HERE</div>"
      "  <br><br><br>"
      "</body>");

  web_view->UpdateAllLifecyclePhases();

  Document* document = web_view->MainFrameImpl()->GetFrame()->GetDocument();
  Element* div1_tag = document->GetElementById("div1");

  HitTestResult hit_test_result = web_view->CoreHitTestResultAt(
      WebPoint(div1_tag->OffsetLeft() + 5, div1_tag->OffsetTop() + 5));

  EXPECT_TRUE(hit_test_result.InnerElement());

  // Mouse over link. Mouse cursor should be hand.
  WebMouseEvent mouse_move_over_link_event(
      WebInputEvent::kMouseMove,
      WebFloatPoint(div1_tag->OffsetLeft() + 5, div1_tag->OffsetTop() + 5),
      WebFloatPoint(div1_tag->OffsetLeft() + 5, div1_tag->OffsetTop() + 5),
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      TimeTicks::Now().InSeconds());
  mouse_move_over_link_event.SetFrameScale(1);
  document->GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_over_link_event, Vector<WebMouseEvent>());

  EXPECT_EQ(
      document->HoverElement(),
      document->GetFrame()->GetChromeClient().LastSetTooltipNodeForTesting());
  EXPECT_EQ(
      div1_tag,
      document->GetFrame()->GetChromeClient().LastSetTooltipNodeForTesting());

  Element* div2_tag = document->GetElementById("div2");

  WebMouseEvent mouse_move_event(
      WebInputEvent::kMouseMove,
      WebFloatPoint(div2_tag->OffsetLeft() + 5, div2_tag->OffsetTop() + 5),
      WebFloatPoint(div2_tag->OffsetLeft() + 5, div2_tag->OffsetTop() + 5),
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      TimeTicks::Now().InSeconds());
  mouse_move_event.SetFrameScale(1);
  document->GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>());

  EXPECT_EQ(
      document->HoverElement(),
      document->GetFrame()->GetChromeClient().LastSetTooltipNodeForTesting());
  EXPECT_EQ(
      div2_tag,
      document->GetFrame()->GetChromeClient().LastSetTooltipNodeForTesting());
}

// Makes sure that mouse hover over an overlay scrollbar doesn't activate
// elements below(except the Element that owns the scrollbar) unless the
// scrollbar is faded out.
TEST_F(WebFrameTest, MouseOverLinkAndOverlayScrollbar) {
  FrameTestHelpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(true, nullptr, nullptr, nullptr,
                             [](WebSettings* settings) {});
  web_view_helper.Resize(WebSize(20, 20));
  WebViewImpl* web_view = web_view_helper.WebView();

  InitializeWithHTML(*web_view->MainFrameImpl()->GetFrame(),
                     "<!DOCTYPE html>"
                     "<a id='a' href='javascript:void(0);'>"
                     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                     "</a>"
                     "<div style='position: absolute; top: 1000px'>end</div>");

  web_view->UpdateAllLifecyclePhases();

  Document* document = web_view->MainFrameImpl()->GetFrame()->GetDocument();
  Element* a_tag = document->GetElementById("a");

  // Ensure hittest only has scrollbar.
  HitTestResult hit_test_result =
      web_view->CoreHitTestResultAt(WebPoint(18, a_tag->OffsetTop()));

  EXPECT_FALSE(hit_test_result.URLElement());
  EXPECT_FALSE(hit_test_result.InnerElement());
  EXPECT_TRUE(hit_test_result.GetScrollbar());
  EXPECT_FALSE(hit_test_result.GetScrollbar()->IsCustomScrollbar());

  // Mouse over link. Mouse cursor should be hand.
  WebMouseEvent mouse_move_over_link_event(
      WebInputEvent::kMouseMove,
      WebFloatPoint(a_tag->OffsetLeft(), a_tag->OffsetTop()),
      WebFloatPoint(a_tag->OffsetLeft(), a_tag->OffsetTop()),
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      TimeTicks::Now().InSeconds());
  mouse_move_over_link_event.SetFrameScale(1);
  document->GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_over_link_event, Vector<WebMouseEvent>());

  EXPECT_EQ(Cursor::Type::kHand, document->GetFrame()
                                     ->GetChromeClient()
                                     .LastSetCursorForTesting()
                                     .GetType());

  // Mouse over enabled overlay scrollbar. Mouse cursor should be pointer and no
  // active hover element.
  WebMouseEvent mouse_move_event(
      WebInputEvent::kMouseMove, WebFloatPoint(18, a_tag->OffsetTop()),
      WebFloatPoint(18, a_tag->OffsetTop()),
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      TimeTicks::Now().InSeconds());
  mouse_move_event.SetFrameScale(1);
  document->GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>());

  EXPECT_EQ(Cursor::Type::kPointer, document->GetFrame()
                                        ->GetChromeClient()
                                        .LastSetCursorForTesting()
                                        .GetType());

  WebMouseEvent mouse_press_event(
      WebInputEvent::kMouseDown, WebFloatPoint(18, a_tag->OffsetTop()),
      WebFloatPoint(18, a_tag->OffsetTop()),
      WebPointerProperties::Button::kLeft, 0,
      WebInputEvent::Modifiers::kLeftButtonDown, TimeTicks::Now().InSeconds());
  mouse_press_event.SetFrameScale(1);
  document->GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_press_event);

  EXPECT_FALSE(document->ActiveHoverElement());
  EXPECT_FALSE(document->HoverElement());

  WebMouseEvent mouse_release_event(
      WebInputEvent::kMouseUp, WebFloatPoint(18, a_tag->OffsetTop()),
      WebFloatPoint(18, a_tag->OffsetTop()),
      WebPointerProperties::Button::kLeft, 0,
      WebInputEvent::Modifiers::kLeftButtonDown, TimeTicks::Now().InSeconds());
  mouse_release_event.SetFrameScale(1);
  document->GetFrame()->GetEventHandler().HandleMouseReleaseEvent(
      mouse_release_event);

  // Mouse over disabled overlay scrollbar. Mouse cursor should be hand and has
  // active hover element.
  web_view->MainFrameImpl()->GetFrameView()->SetScrollbarsHidden(true);

  // Ensure hittest only has link
  hit_test_result =
      web_view->CoreHitTestResultAt(WebPoint(18, a_tag->OffsetTop()));

  EXPECT_TRUE(hit_test_result.URLElement());
  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_FALSE(hit_test_result.GetScrollbar());

  document->GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>());

  EXPECT_EQ(Cursor::Type::kHand, document->GetFrame()
                                     ->GetChromeClient()
                                     .LastSetCursorForTesting()
                                     .GetType());

  document->GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_press_event);

  EXPECT_TRUE(document->ActiveHoverElement());
  EXPECT_TRUE(document->HoverElement());

  document->GetFrame()->GetEventHandler().HandleMouseReleaseEvent(
      mouse_release_event);
}

// Makes sure that mouse hover over an custom scrollbar doesn't change the
// activate elements.
TEST_F(WebFrameTest, MouseOverCustomScrollbar) {
  RegisterMockedHttpURLLoad("custom-scrollbar-hover.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "custom-scrollbar-hover.html");

  web_view_helper.Resize(WebSize(200, 200));

  web_view->UpdateAllLifecyclePhases();

  Document* document =
      ToLocalFrame(web_view->GetPage()->MainFrame())->GetDocument();

  Element* scrollbar_div = document->GetElementById("scrollbar");
  EXPECT_TRUE(scrollbar_div);

  // Ensure hittest only has DIV
  HitTestResult hit_test_result = web_view->CoreHitTestResultAt(WebPoint(1, 1));

  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_FALSE(hit_test_result.GetScrollbar());

  // Mouse over DIV
  WebMouseEvent mouse_move_over_div(
      WebInputEvent::kMouseMove, WebFloatPoint(1, 1), WebFloatPoint(1, 1),
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      TimeTicks::Now().InSeconds());
  mouse_move_over_div.SetFrameScale(1);
  document->GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_over_div, Vector<WebMouseEvent>());

  // DIV :hover
  EXPECT_EQ(document->HoverElement(), scrollbar_div);

  // Ensure hittest has DIV and scrollbar
  hit_test_result = web_view->CoreHitTestResultAt(WebPoint(175, 1));

  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_TRUE(hit_test_result.GetScrollbar());
  EXPECT_TRUE(hit_test_result.GetScrollbar()->IsCustomScrollbar());

  // Mouse over scrollbar
  WebMouseEvent mouse_move_over_div_and_scrollbar(
      WebInputEvent::kMouseMove, WebFloatPoint(175, 1), WebFloatPoint(175, 1),
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      TimeTicks::Now().InSeconds());
  mouse_move_over_div_and_scrollbar.SetFrameScale(1);
  document->GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_over_div_and_scrollbar, Vector<WebMouseEvent>());

  // Custom not change the DIV :hover
  EXPECT_EQ(document->HoverElement(), scrollbar_div);
  EXPECT_EQ(hit_test_result.GetScrollbar()->HoveredPart(),
            ScrollbarPart::kThumbPart);
}

// Makes sure that mouse hover over an overlay scrollbar doesn't hover iframe
// below.
TEST_F(WebFrameTest, MouseOverScrollbarAndIFrame) {
  RegisterMockedHttpURLLoad("scrollbar-and-iframe-hover.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "scrollbar-and-iframe-hover.html");

  web_view_helper.Resize(WebSize(200, 200));

  web_view->UpdateAllLifecyclePhases();

  Document* document =
      ToLocalFrame(web_view->GetPage()->MainFrame())->GetDocument();
  Element* iframe = document->GetElementById("iframe");
  DCHECK(iframe);

  // Ensure hittest only has IFRAME.
  HitTestResult hit_test_result = web_view->CoreHitTestResultAt(WebPoint(5, 5));

  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_FALSE(hit_test_result.GetScrollbar());

  // Mouse over IFRAME.
  WebMouseEvent mouse_move_over_i_frame(
      WebInputEvent::kMouseMove, WebFloatPoint(5, 5), WebFloatPoint(5, 5),
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      TimeTicks::Now().InSeconds());
  mouse_move_over_i_frame.SetFrameScale(1);
  document->GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_over_i_frame, Vector<WebMouseEvent>());

  // IFRAME hover.
  EXPECT_EQ(document->HoverElement(), iframe);

  // Ensure hittest has scrollbar.
  hit_test_result = web_view->CoreHitTestResultAt(WebPoint(195, 5));
  EXPECT_FALSE(hit_test_result.InnerElement());
  EXPECT_TRUE(hit_test_result.GetScrollbar());
  EXPECT_TRUE(hit_test_result.GetScrollbar()->Enabled());

  // Mouse over scrollbar.
  WebMouseEvent mouse_move_over_i_frame_and_scrollbar(
      WebInputEvent::kMouseMove, WebFloatPoint(195, 5), WebFloatPoint(195, 5),
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      TimeTicks::Now().InSeconds());
  mouse_move_over_i_frame_and_scrollbar.SetFrameScale(1);
  document->GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_over_i_frame_and_scrollbar, Vector<WebMouseEvent>());

  // IFRAME not hover.
  EXPECT_NE(document->HoverElement(), iframe);

  // Disable the Scrollbar.
  web_view->MainFrameImpl()->GetFrameView()->SetScrollbarsHidden(true);

  // Ensure hittest has IFRAME and no scrollbar.
  hit_test_result = web_view->CoreHitTestResultAt(WebPoint(196, 5));

  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_FALSE(hit_test_result.GetScrollbar());

  // Mouse over disabled scrollbar.
  WebMouseEvent mouse_move_over_i_frame_and_disabled_scrollbar(
      WebInputEvent::kMouseMove, WebFloatPoint(196, 5), WebFloatPoint(196, 5),
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      TimeTicks::Now().InSeconds());
  mouse_move_over_i_frame_and_disabled_scrollbar.SetFrameScale(1);
  document->GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_over_i_frame_and_disabled_scrollbar, Vector<WebMouseEvent>());

  // IFRAME hover.
  EXPECT_EQ(document->HoverElement(), iframe);
}

// Makes sure that mouse hover over a scrollbar also hover the element owns the
// scrollbar.
TEST_F(WebFrameTest, MouseOverScrollbarAndParentElement) {
  RegisterMockedHttpURLLoad("scrollbar-and-element-hover.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(false);
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "scrollbar-and-element-hover.html");

  web_view_helper.Resize(WebSize(200, 200));

  web_view->UpdateAllLifecyclePhases();

  Document* document =
      ToLocalFrame(web_view->GetPage()->MainFrame())->GetDocument();

  Element* parent_div = document->GetElementById("parent");
  Element* child_div = document->GetElementById("child");
  EXPECT_TRUE(parent_div);
  EXPECT_TRUE(child_div);

  ScrollableArea* scrollable_area =
      ToLayoutBox(parent_div->GetLayoutObject())->GetScrollableArea();

  EXPECT_TRUE(scrollable_area->VerticalScrollbar());
  EXPECT_FALSE(scrollable_area->VerticalScrollbar()->IsOverlayScrollbar());
  EXPECT_TRUE(scrollable_area->VerticalScrollbar()->GetTheme().IsMockTheme());

  // Ensure hittest only has DIV.
  HitTestResult hit_test_result = web_view->CoreHitTestResultAt(WebPoint(1, 1));

  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_FALSE(hit_test_result.GetScrollbar());

  // Mouse over DIV.
  WebMouseEvent mouse_move_over_div(
      WebInputEvent::kMouseMove, WebFloatPoint(1, 1), WebFloatPoint(1, 1),
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      TimeTicks::Now().InSeconds());
  mouse_move_over_div.SetFrameScale(1);
  document->GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_over_div, Vector<WebMouseEvent>());

  // DIV :hover.
  EXPECT_EQ(document->HoverElement(), parent_div);

  // Ensure hittest has DIV and scrollbar.
  hit_test_result = web_view->CoreHitTestResultAt(WebPoint(175, 5));

  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_TRUE(hit_test_result.GetScrollbar());
  EXPECT_FALSE(hit_test_result.GetScrollbar()->IsCustomScrollbar());
  EXPECT_TRUE(hit_test_result.GetScrollbar()->Enabled());

  // Mouse over scrollbar.
  WebMouseEvent mouse_move_over_div_and_scrollbar(
      WebInputEvent::kMouseMove, WebFloatPoint(175, 5), WebFloatPoint(175, 5),
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      TimeTicks::Now().InSeconds());
  mouse_move_over_div_and_scrollbar.SetFrameScale(1);
  document->GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_over_div_and_scrollbar, Vector<WebMouseEvent>());

  // Not change the DIV :hover.
  EXPECT_EQ(document->HoverElement(), parent_div);

  // Disable the Scrollbar by remove the childDiv.
  child_div->remove();
  web_view->UpdateAllLifecyclePhases();

  // Ensure hittest has DIV and no scrollbar.
  hit_test_result = web_view->CoreHitTestResultAt(WebPoint(175, 5));

  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_TRUE(hit_test_result.GetScrollbar());
  EXPECT_FALSE(hit_test_result.GetScrollbar()->Enabled());
  EXPECT_LT(hit_test_result.InnerElement()->clientWidth(), 180);

  // Mouse over disabled scrollbar.
  document->GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_over_div_and_scrollbar, Vector<WebMouseEvent>());

  // Not change the DIV :hover.
  EXPECT_EQ(document->HoverElement(), parent_div);
}

TEST_F(WebFrameTest, MouseReleaseUpdatesScrollbarHoveredPart) {
  RegisterMockedHttpURLLoad("custom-scrollbar-hover.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "custom-scrollbar-hover.html");

  web_view_helper.Resize(WebSize(200, 200));

  web_view->UpdateAllLifecyclePhases();

  Document* document =
      ToLocalFrame(web_view->GetPage()->MainFrame())->GetDocument();

  Element* scrollbar_div = document->GetElementById("scrollbar");
  EXPECT_TRUE(scrollbar_div);

  ScrollableArea* scrollable_area =
      ToLayoutBox(scrollbar_div->GetLayoutObject())->GetScrollableArea();

  EXPECT_TRUE(scrollable_area->VerticalScrollbar());
  Scrollbar* scrollbar = scrollable_area->VerticalScrollbar();
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kNoPart);
  EXPECT_EQ(scrollbar->HoveredPart(), ScrollbarPart::kNoPart);

  // Mouse moved over the scrollbar.
  WebMouseEvent mouse_move_over_scrollbar(
      WebInputEvent::kMouseMove, WebFloatPoint(175, 1), WebFloatPoint(175, 1),
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      TimeTicks::Now().InSeconds());
  mouse_move_over_scrollbar.SetFrameScale(1);
  document->GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_over_scrollbar, Vector<WebMouseEvent>());
  HitTestResult hit_test_result =
      web_view->CoreHitTestResultAt(WebPoint(175, 1));
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kNoPart);
  EXPECT_EQ(scrollbar->HoveredPart(), ScrollbarPart::kThumbPart);

  // Mouse pressed.
  WebMouseEvent mouse_press_event(
      WebInputEvent::kMouseDown, WebFloatPoint(175, 1), WebFloatPoint(175, 1),
      WebPointerProperties::Button::kLeft, 0,
      WebInputEvent::Modifiers::kLeftButtonDown, TimeTicks::Now().InSeconds());
  mouse_press_event.SetFrameScale(1);
  document->GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_press_event);
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kThumbPart);
  EXPECT_EQ(scrollbar->HoveredPart(), ScrollbarPart::kThumbPart);

  // Mouse moved off the scrollbar while still pressed.
  WebMouseEvent mouse_move_off_scrollbar(
      WebInputEvent::kMouseMove, WebFloatPoint(1, 1), WebFloatPoint(1, 1),
      WebPointerProperties::Button::kLeft, 0,
      WebInputEvent::Modifiers::kLeftButtonDown, TimeTicks::Now().InSeconds());
  mouse_move_off_scrollbar.SetFrameScale(1);
  document->GetFrame()->GetEventHandler().HandleMouseLeaveEvent(
      mouse_move_off_scrollbar);
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kThumbPart);
  EXPECT_EQ(scrollbar->HoveredPart(), ScrollbarPart::kThumbPart);

  // Mouse released.
  WebMouseEvent mouse_release_event(
      WebInputEvent::kMouseUp, WebFloatPoint(1, 1), WebFloatPoint(1, 1),
      WebPointerProperties::Button::kLeft, 0,
      WebInputEvent::Modifiers::kLeftButtonDown, TimeTicks::Now().InSeconds());
  mouse_release_event.SetFrameScale(1);
  document->GetFrame()->GetEventHandler().HandleMouseReleaseEvent(
      mouse_release_event);
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kNoPart);
  EXPECT_EQ(scrollbar->HoveredPart(), ScrollbarPart::kNoPart);
}

TEST_F(WebFrameTest,
       CustomScrollbarInOverlayScrollbarThemeWillNotCauseDCHECKFails) {
  RegisterMockedHttpURLLoad(
      "custom-scrollbar-dcheck-failed-when-paint-scroll-corner.html");
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ +
      "custom-scrollbar-dcheck-failed-when-paint-scroll-corner.html");

  web_view_helper.Resize(WebSize(200, 200));

  // No DCHECK Fails. Issue 676678.
  web_view->UpdateAllLifecyclePhases();
}

static void DisableCompositing(WebSettings* settings) {
  settings->SetAcceleratedCompositingEnabled(false);
  settings->SetPreferCompositingToLCDTextEnabled(false);
}

// Make sure overlay scrollbars on non-composited scrollers fade out and set
// the hidden bit as needed.
TEST_F(WebFrameTest, TestNonCompositedOverlayScrollbarsFade) {
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.Initialize(
      true, nullptr, nullptr, nullptr, &DisableCompositing);

  constexpr double kMockOverlayFadeOutDelayMs = 5.0;

  ScrollbarTheme& theme = ScrollbarTheme::GetTheme();
  // This test relies on mock overlay scrollbars.
  ASSERT_TRUE(theme.IsMockTheme());
  ASSERT_TRUE(theme.UsesOverlayScrollbars());
  ScrollbarThemeOverlayMock& mock_overlay_theme =
      (ScrollbarThemeOverlayMock&)theme;
  mock_overlay_theme.SetOverlayScrollbarFadeOutDelay(
      kMockOverlayFadeOutDelayMs / 1000.0);

  web_view_impl->ResizeWithBrowserControls(WebSize(640, 480), 0, false);

  WebURL base_url = URLTestHelpers::ToKURL("http://example.com/");
  FrameTestHelpers::LoadHTMLString(web_view_impl->MainFrame(),
                                   "<!DOCTYPE html>"
                                   "<style>"
                                   "  #space {"
                                   "    width: 1000px;"
                                   "    height: 1000px;"
                                   "  }"
                                   "  #container {"
                                   "    width: 200px;"
                                   "    height: 200px;"
                                   "    overflow: scroll;"
                                   "  }"
                                   "  div { height:1000px; width: 200px; }"
                                   "</style>"
                                   "<div id='container'>"
                                   "  <div id='space'></div>"
                                   "</div>",
                                   base_url);
  web_view_impl->UpdateAllLifecyclePhases();

  WebLocalFrameImpl* frame = web_view_helper.WebView()->MainFrameImpl();
  Document* document =
      ToLocalFrame(web_view_impl->GetPage()->MainFrame())->GetDocument();
  Element* container = document->GetElementById("container");
  ScrollableArea* scrollable_area =
      ToLayoutBox(container->GetLayoutObject())->GetScrollableArea();

  EXPECT_FALSE(scrollable_area->ScrollbarsHidden());
  testing::RunDelayedTasks(kMockOverlayFadeOutDelayMs);
  EXPECT_TRUE(scrollable_area->ScrollbarsHidden());

  scrollable_area->SetScrollOffset(ScrollOffset(10, 10), kProgrammaticScroll,
                                   kScrollBehaviorInstant);

  EXPECT_FALSE(scrollable_area->ScrollbarsHidden());
  testing::RunDelayedTasks(kMockOverlayFadeOutDelayMs);
  EXPECT_TRUE(scrollable_area->ScrollbarsHidden());

  frame->ExecuteScript(WebScriptSource(
      "document.getElementById('space').style.height = '500px';"));
  frame->View()->UpdateAllLifecyclePhases();

  EXPECT_FALSE(scrollable_area->ScrollbarsHidden());
  testing::RunDelayedTasks(kMockOverlayFadeOutDelayMs);
  EXPECT_TRUE(scrollable_area->ScrollbarsHidden());

  frame->ExecuteScript(WebScriptSource(
      "document.getElementById('container').style.height = '300px';"));
  frame->View()->UpdateAllLifecyclePhases();

  EXPECT_FALSE(scrollable_area->ScrollbarsHidden());
  testing::RunDelayedTasks(kMockOverlayFadeOutDelayMs);
  EXPECT_TRUE(scrollable_area->ScrollbarsHidden());

  // Non-composited scrollbars don't fade out while mouse is over.
  EXPECT_TRUE(scrollable_area->VerticalScrollbar());
  scrollable_area->SetScrollOffset(ScrollOffset(20, 20), kProgrammaticScroll,
                                   kScrollBehaviorInstant);
  EXPECT_FALSE(scrollable_area->ScrollbarsHidden());
  scrollable_area->MouseEnteredScrollbar(*scrollable_area->VerticalScrollbar());
  testing::RunDelayedTasks(kMockOverlayFadeOutDelayMs);
  EXPECT_FALSE(scrollable_area->ScrollbarsHidden());
  scrollable_area->MouseExitedScrollbar(*scrollable_area->VerticalScrollbar());
  testing::RunDelayedTasks(kMockOverlayFadeOutDelayMs);
  EXPECT_TRUE(scrollable_area->ScrollbarsHidden());

  mock_overlay_theme.SetOverlayScrollbarFadeOutDelay(0.0);
}

class WebFrameSimTest : public SimTest {};

TEST_F(WebFrameSimTest, DisplayNoneIFrameHasNoLayoutObjects) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/frame.html", "text/html");

  LoadURL("https://example.com/test.html");
  main_resource.Complete(
      "<!DOCTYPE html>"
      "<iframe src=frame.html style='display: none'></iframe>");
  frame_resource.Complete(
      "<!DOCTYPE html>"
      "<html><body>This is a visible iframe.</body></html>");

  Element* element = GetDocument().QuerySelector("iframe");
  HTMLFrameOwnerElement* frame_owner_element = ToHTMLFrameOwnerElement(element);
  Document* iframe_doc = frame_owner_element->contentDocument();
  EXPECT_FALSE(iframe_doc->documentElement()->GetLayoutObject());

  // Changing the display from 'none' -> 'block' should cause layout objects to
  // appear.
  element->SetInlineStyleProperty(CSSPropertyDisplay, CSSValueBlock);
  Compositor().BeginFrame();
  EXPECT_TRUE(iframe_doc->documentElement()->GetLayoutObject());

  // Changing the display from 'block' -> 'none' should cause layout objects to
  // disappear.
  element->SetInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);

  Compositor().BeginFrame();
  EXPECT_FALSE(iframe_doc->documentElement()->GetLayoutObject());
}

TEST_F(WebFrameSimTest, NormalIFrameHasLayoutObjects) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/frame.html", "text/html");

  LoadURL("https://example.com/test.html");
  main_resource.Complete(
      "<!DOCTYPE html>"
      "<iframe src=frame.html style='display: block'></iframe>");
  frame_resource.Complete(
      "<!DOCTYPE html>"
      "<html><body>This is a visible iframe.</body></html>");

  Element* element = GetDocument().QuerySelector("iframe");
  HTMLFrameOwnerElement* frame_owner_element = ToHTMLFrameOwnerElement(element);
  Document* iframe_doc = frame_owner_element->contentDocument();
  EXPECT_TRUE(iframe_doc->documentElement()->GetLayoutObject());

  // Changing the display from 'block' -> 'none' should cause layout objects to
  // disappear.
  element->SetInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);
  Compositor().BeginFrame();
  EXPECT_FALSE(iframe_doc->documentElement()->GetLayoutObject());
}

TEST_F(WebFrameSimTest, ScrollOriginChangeUpdatesLayerPositions) {
  WebView().Resize(WebSize(800, 600));
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");
  main_resource.Complete(
      "<!DOCTYPE html>"
      "<body dir='rtl'>"
      "  <div style='width:1px; height:1px; position:absolute; left:-10000px'>"
      "  </div>"
      "</body>");

  Compositor().BeginFrame();
  ScrollableArea* area = GetDocument().View()->LayoutViewportScrollableArea();
  ASSERT_EQ(10000, area->ScrollOrigin().X());
  ASSERT_EQ(10000, area->LayerForScrolling()->GetPosition().X());

  // Removing the overflowing element removes all overflow so the scroll origin
  // implicitly is reset to (0, 0).
  GetDocument().QuerySelector("div")->remove();
  Compositor().BeginFrame();

  EXPECT_EQ(0, area->ScrollOrigin().X());
  EXPECT_EQ(0, area->LayerForScrolling()->GetPosition().X());
}

TEST_F(WebFrameTest, NoLoadingCompletionCallbacksInDetach) {
  class LoadingObserverFrameClient
      : public FrameTestHelpers::TestWebFrameClient {
   public:
    void FrameDetached(WebLocalFrame*, DetachType) override {
      did_call_frame_detached_ = true;
    }

    void DidStopLoading() override {
      // TODO(dcheng): Investigate not calling this as well during frame detach.
      did_call_did_stop_loading_ = true;
    }

    void DidFailProvisionalLoad(const WebURLError&,
                                WebHistoryCommitType) override {
      EXPECT_TRUE(false) << "The load should not have failed.";
    }

    void DidFinishDocumentLoad(WebLocalFrame*) override {
      // TODO(dcheng): Investigate not calling this as well during frame detach.
      did_call_did_finish_document_load_ = true;
    }

    void DidHandleOnloadEvents() override {
      // TODO(dcheng): Investigate not calling this as well during frame detach.
      did_call_did_handle_onload_events_ = true;
    }

    void DidFinishLoad() override {
      EXPECT_TRUE(false) << "didFinishLoad() should not have been called.";
    }

    void DispatchLoad() override {
      EXPECT_TRUE(false) << "dispatchLoad() should not have been called.";
    }

    bool DidCallFrameDetached() const { return did_call_frame_detached_; }
    bool DidCallDidStopLoading() const { return did_call_did_stop_loading_; }
    bool DidCallDidFinishDocumentLoad() const {
      return did_call_did_finish_document_load_;
    }
    bool DidCallDidHandleOnloadEvents() const {
      return did_call_did_handle_onload_events_;
    }

   private:
    bool did_call_frame_detached_ = false;
    bool did_call_did_stop_loading_ = false;
    bool did_call_did_finish_document_load_ = false;
    bool did_call_did_handle_onload_events_ = false;
  };

  class MainFrameClient : public FrameTestHelpers::TestWebFrameClient {
   public:
    WebLocalFrame* CreateChildFrame(
        WebLocalFrame* parent,
        WebTreeScopeType scope,
        const WebString& name,
        const WebString& fallback_name,
        WebSandboxFlags sandbox_flags,
        const WebParsedFeaturePolicy& container_policy,
        const WebFrameOwnerProperties&) override {
      WebLocalFrame* frame =
          WebLocalFrame::Create(scope, &child_client_, nullptr, nullptr);
      parent->AppendChild(frame);
      return frame;
    }

    LoadingObserverFrameClient& ChildClient() { return child_client_; }

   private:
    LoadingObserverFrameClient child_client_;
  };

  RegisterMockedHttpURLLoad("single_iframe.html");
  URLTestHelpers::RegisterMockedURLLoad(
      ToKURL(base_url_ + "visible_iframe.html"),
      testing::WebTestDataPath("frame_with_frame.html"));
  RegisterMockedHttpURLLoad("parent_detaching_frame.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  MainFrameClient main_frame_client;
  web_view_helper.InitializeAndLoad(base_url_ + "single_iframe.html", true,
                                    &main_frame_client);

  EXPECT_TRUE(main_frame_client.ChildClient().DidCallFrameDetached());
  EXPECT_TRUE(main_frame_client.ChildClient().DidCallDidStopLoading());
  EXPECT_TRUE(main_frame_client.ChildClient().DidCallDidFinishDocumentLoad());
  EXPECT_TRUE(main_frame_client.ChildClient().DidCallDidHandleOnloadEvents());

  web_view_helper.Reset();
}

TEST_F(WebFrameTest, ClearClosedOpener) {
  FrameTestHelpers::TestWebViewClient opener_web_view_client;
  FrameTestHelpers::WebViewHelper opener_helper;
  opener_helper.Initialize(false, nullptr, &opener_web_view_client);
  FrameTestHelpers::WebViewHelper helper;
  helper.InitializeWithOpener(opener_helper.WebView()->MainFrame());

  opener_helper.Reset();
  EXPECT_EQ(nullptr, helper.WebView()->MainFrameImpl()->Opener());
}

class ShowVirtualKeyboardObserverWidgetClient
    : public FrameTestHelpers::TestWebWidgetClient {
 public:
  ShowVirtualKeyboardObserverWidgetClient()
      : did_show_virtual_keyboard_(false) {}

  void ShowVirtualKeyboardOnElementFocus() override {
    did_show_virtual_keyboard_ = true;
  }

  bool DidShowVirtualKeyboard() const { return did_show_virtual_keyboard_; }

 private:
  bool did_show_virtual_keyboard_;
};

TEST_F(WebFrameTest, ShowVirtualKeyboardOnElementFocus) {
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.Initialize(true);
  WebRemoteFrameImpl* remote_frame = static_cast<WebRemoteFrameImpl*>(
      WebRemoteFrame::Create(WebTreeScopeType::kDocument, nullptr));
  web_view->SetMainFrame(remote_frame);
  RefPtr<SecurityOrigin> unique_origin = SecurityOrigin::CreateUnique();
  remote_frame->GetFrame()->GetSecurityContext()->SetSecurityOrigin(
      unique_origin);

  ShowVirtualKeyboardObserverWidgetClient web_widget_client;
  WebLocalFrameImpl* local_frame = FrameTestHelpers::CreateLocalChild(
      remote_frame, "child", nullptr, &web_widget_client);

  RegisterMockedHttpURLLoad("input_field_default.html");
  FrameTestHelpers::LoadFrame(local_frame,
                              base_url_ + "input_field_default.html");

  // Simulate an input element focus leading to Element::focus() call with a
  // user gesture.
  local_frame->SetHasReceivedUserGesture();
  local_frame->ExecuteScript(
      WebScriptSource("window.focus();"
                      "document.querySelector('input').focus();"));

  // Verify that the right WebWidgetClient has been notified.
  EXPECT_TRUE(web_widget_client.DidShowVirtualKeyboard());

  remote_frame->Close();
  web_view_helper.Reset();
}

class ContextMenuWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
 public:
  ContextMenuWebFrameClient(){};
  // WebFrameClient methods
  void ShowContextMenu(const WebContextMenuData& data) override {
    menu_data_ = data;
  }

  WebContextMenuData GetMenuData() { return menu_data_; }

 private:
  WebContextMenuData menu_data_;
  DISALLOW_COPY_AND_ASSIGN(ContextMenuWebFrameClient);
};

bool TestSelectAll(const std::string& html) {
  ContextMenuWebFrameClient frame;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.Initialize(true, &frame);
  FrameTestHelpers::LoadHTMLString(web_view->MainFrame(), html,
                                   ToKURL("about:blank"));
  web_view->Resize(WebSize(500, 300));
  web_view->UpdateAllLifecyclePhases();
  RunPendingTasks();
  web_view->SetInitialFocus(false);
  RunPendingTasks();

  WebMouseEvent mouse_event(WebInputEvent::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::kTimeStampForTesting);

  mouse_event.button = WebMouseEvent::Button::kRight;
  mouse_event.SetPositionInWidget(8, 8);
  mouse_event.click_count = 1;
  web_view->HandleInputEvent(WebCoalescedInputEvent(mouse_event));
  RunPendingTasks();
  web_view_helper.Reset();
  return frame.GetMenuData().edit_flags & WebContextMenuData::kCanSelectAll;
}

TEST_F(WebFrameTest, ContextMenuDataSelectAll) {
  EXPECT_FALSE(TestSelectAll("<textarea></textarea>"));
  EXPECT_TRUE(TestSelectAll("<textarea>nonempty</textarea>"));
  EXPECT_FALSE(TestSelectAll("<input>"));
  EXPECT_TRUE(TestSelectAll("<input value='nonempty'>"));
  EXPECT_FALSE(TestSelectAll("<div contenteditable></div>"));
  EXPECT_TRUE(TestSelectAll("<div contenteditable>nonempty</div>"));
  EXPECT_TRUE(TestSelectAll("<div contenteditable>\n</div>"));
}

TEST_F(WebFrameTest, ContextMenuDataSelectedText) {
  ContextMenuWebFrameClient frame;
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.Initialize(true, &frame);
  const std::string& html = "<input value=' '>";
  FrameTestHelpers::LoadHTMLString(web_view->MainFrame(), html,
                                   ToKURL("about:blank"));
  web_view->Resize(WebSize(500, 300));
  web_view->UpdateAllLifecyclePhases();
  RunPendingTasks();
  web_view->SetInitialFocus(false);
  RunPendingTasks();

  web_view->MainFrameImpl()->ExecuteCommand(WebString::FromUTF8("SelectAll"));

  WebMouseEvent mouse_event(WebInputEvent::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::kTimeStampForTesting);

  mouse_event.button = WebMouseEvent::Button::kRight;
  mouse_event.SetPositionInWidget(8, 8);
  mouse_event.click_count = 1;
  web_view->HandleInputEvent(WebCoalescedInputEvent(mouse_event));
  RunPendingTasks();
  web_view_helper.Reset();
  EXPECT_EQ(frame.GetMenuData().selected_text, " ");
}

TEST_F(WebFrameTest, LocalFrameWithRemoteParentIsTransparent) {
  FrameTestHelpers::TestWebViewClient view_client;
  FrameTestHelpers::TestWebRemoteFrameClient remote_client;
  WebView* view = WebView::Create(&view_client, kWebPageVisibilityStateVisible);
  view->GetSettings()->SetJavaScriptEnabled(true);
  view->SetMainFrame(remote_client.GetFrame());
  WebRemoteFrame* root = view->MainFrame()->ToWebRemoteFrame();
  root->SetReplicatedOrigin(SecurityOrigin::CreateUnique());

  WebLocalFrameImpl* local_frame = FrameTestHelpers::CreateLocalChild(root);
  FrameTestHelpers::LoadFrame(local_frame, "data:text/html,some page");

  // Local frame with remote parent should have transparent baseBackgroundColor.
  Color color = local_frame->GetFrameView()->BaseBackgroundColor();
  EXPECT_EQ(Color::kTransparent, color);

  view->Close();
}

class TestFallbackWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
 public:
  explicit TestFallbackWebFrameClient() : child_client_(nullptr) {}

  void SetChildWebFrameClient(TestFallbackWebFrameClient* client) {
    child_client_ = client;
  }

  WebLocalFrame* CreateChildFrame(
      WebLocalFrame* parent,
      WebTreeScopeType scope,
      const WebString&,
      const WebString&,
      WebSandboxFlags,
      const WebParsedFeaturePolicy& container_policy,
      const WebFrameOwnerProperties& frameOwnerProperties) override {
    DCHECK(child_client_);
    WebLocalFrame* frame =
        WebLocalFrame::Create(scope, child_client_, nullptr, nullptr);
    parent->AppendChild(frame);
    return frame;
  }

  WebNavigationPolicy DecidePolicyForNavigation(
      const NavigationPolicyInfo& info) override {
    if (child_client_ || KURL(info.url_request.Url()) == BlankURL())
      return kWebNavigationPolicyCurrentTab;
    return kWebNavigationPolicyHandledByClient;
  }

 private:
  TestFallbackWebFrameClient* child_client_;
};

TEST_F(WebFrameTest, FallbackForNonexistentProvisionalNavigation) {
  RegisterMockedHttpURLLoad("fallback.html");
  TestFallbackWebFrameClient mainClient;
  TestFallbackWebFrameClient childClient;
  mainClient.SetChildWebFrameClient(&childClient);

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.Initialize(true, &mainClient);

  WebLocalFrameImpl* main_frame = webViewHelper.WebView()->MainFrameImpl();
  WebURLRequest request(ToKURL(base_url_ + "fallback.html"));
  main_frame->LoadRequest(request);

  // Because the child frame will be HandledByClient, the main frame will not
  // finish loading, so we cant use
  // FrameTestHelpers::pumpPendingRequestsForFrameToLoad.
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();

  // Overwrite the client-handled child frame navigation with about:blank.
  WebLocalFrame* child = main_frame->FirstChild()->ToWebLocalFrame();
  child->LoadRequest(WebURLRequest(BlankURL()));

  // Failing the original child frame navigation and trying to render fallback
  // content shouldn't crash. It should return NoLoadInProgress. This is so the
  // caller won't attempt to replace the correctly empty frame with an error
  // page.
  EXPECT_EQ(WebLocalFrame::NoLoadInProgress,
            child->MaybeRenderFallbackContent(WebURLError()));
}

}  // namespace blink
