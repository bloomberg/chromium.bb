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

#include "SkBitmap.h"
#include "SkCanvas.h"
#include "bindings/core/v8/SerializedScriptValueFactory.h"
#include "bindings/core/v8/V8Node.h"
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
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/events/MouseEvent.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLMediaElement.h"
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
#include "platform/PlatformMouseEvent.h"
#include "platform/PlatformResourceLoader.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/UserGestureIndicator.h"
#include "platform/geometry/FloatRect.h"
#include "platform/network/ResourceError.h"
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
#include "public/platform/Platform.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebClipboard.h"
#include "public/platform/WebFloatRect.h"
#include "public/platform/WebMockClipboard.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLLoaderClient.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLResponse.h"
#include "public/web/WebCache.h"
#include "public/web/WebConsoleMessage.h"
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
#include "public/web/WebSpellCheckClient.h"
#include "public/web/WebTextCheckingCompletion.h"
#include "public/web/WebTextCheckingResult.h"
#include "public/web/WebViewClient.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/TextFinder.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebRemoteFrameImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include "wtf/Forward.h"
#include "wtf/PtrUtil.h"
#include "wtf/dtoa/utils.h"
#include <map>
#include <memory>
#include <stdarg.h>
#include <v8.h>

using blink::URLTestHelpers::toKURL;
using blink::testing::runPendingTasks;
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

const int touchPointPadding = 32;

#define EXPECT_RECT_EQ(expected, actual)           \
  do {                                             \
    EXPECT_EQ(expected.x(), actual.x());           \
    EXPECT_EQ(expected.y(), actual.y());           \
    EXPECT_EQ(expected.width(), actual.width());   \
    EXPECT_EQ(expected.height(), actual.height()); \
  } while (false)

#define EXPECT_SIZE_EQ(expected, actual)           \
  do {                                             \
    EXPECT_EQ(expected.width(), actual.width());   \
    EXPECT_EQ(expected.height(), actual.height()); \
  } while (false)

#define EXPECT_FLOAT_POINT_EQ(expected, actual) \
  do {                                          \
    EXPECT_FLOAT_EQ(expected.x(), actual.x());  \
    EXPECT_FLOAT_EQ(expected.y(), actual.y());  \
  } while (false)

class WebFrameTest : public ::testing::Test {
 protected:
  WebFrameTest()
      : m_baseURL("http://internal.test/"),
        m_notBaseURL("http://external.test/"),
        m_chromeURL("chrome://") {}

  ~WebFrameTest() override {
    Platform::current()->getURLLoaderMockFactory()->unregisterAllURLs();
    WebCache::clear();
  }

  void registerMockedHttpURLLoad(const std::string& fileName) {
    URLTestHelpers::registerMockedURLFromBaseURL(
        WebString::fromUTF8(m_baseURL.c_str()),
        WebString::fromUTF8(fileName.c_str()));
  }

  void registerMockedChromeURLLoad(const std::string& fileName) {
    URLTestHelpers::registerMockedURLFromBaseURL(
        WebString::fromUTF8(m_chromeURL.c_str()),
        WebString::fromUTF8(fileName.c_str()));
  }

  void registerMockedHttpURLLoadWithCSP(const std::string& fileName,
                                        const std::string& csp,
                                        bool reportOnly = false) {
    WebURLResponse response;
    response.setMIMEType("text/html");
    response.addHTTPHeaderField(
        reportOnly ? WebString("Content-Security-Policy-Report-Only")
                   : WebString("Content-Security-Policy"),
        WebString::fromUTF8(csp));
    std::string fullString = m_baseURL + fileName;
    URLTestHelpers::registerMockedURLLoadWithCustomResponse(
        toKURL(fullString.c_str()), WebString::fromUTF8(fileName.c_str()),
        WebString::fromUTF8(""), response);
  }

  void registerMockedHttpURLLoadWithMimeType(const std::string& fileName,
                                             const std::string& mimeType) {
    URLTestHelpers::registerMockedURLFromBaseURL(
        WebString::fromUTF8(m_baseURL.c_str()),
        WebString::fromUTF8(fileName.c_str()), WebString::fromUTF8(mimeType));
  }

  void applyViewportStyleOverride(
      FrameTestHelpers::WebViewHelper* webViewHelper) {
    webViewHelper->webView()->settings()->setViewportStyle(
        WebViewportStyle::Mobile);
  }

  static void configureCompositingWebView(WebSettings* settings) {
    settings->setAcceleratedCompositingEnabled(true);
    settings->setPreferCompositingToLCDTextEnabled(true);
  }

  static void configureAndroid(WebSettings* settings) {
    settings->setViewportMetaEnabled(true);
    settings->setViewportEnabled(true);
    settings->setMainFrameResizesAreOrientationChanges(true);
    settings->setShrinksViewportContentToFit(true);
  }

  static void configureLoadsImagesAutomatically(WebSettings* settings) {
    settings->setLoadsImagesAutomatically(true);
  }

  void initializeTextSelectionWebView(
      const std::string& url,
      FrameTestHelpers::WebViewHelper* webViewHelper) {
    webViewHelper->initializeAndLoad(url, true);
    webViewHelper->webView()->settings()->setDefaultFontSize(12);
    webViewHelper->resize(WebSize(640, 480));
  }

  std::unique_ptr<DragImage> nodeImageTestSetup(
      FrameTestHelpers::WebViewHelper* webViewHelper,
      const std::string& testcase) {
    registerMockedHttpURLLoad("nodeimage.html");
    webViewHelper->initializeAndLoad(m_baseURL + "nodeimage.html");
    webViewHelper->resize(WebSize(640, 480));
    LocalFrame* frame =
        toLocalFrame(webViewHelper->webView()->page()->mainFrame());
    DCHECK(frame);
    Element* element = frame->document()->getElementById(testcase.c_str());
    return frame->nodeImage(*element);
  }

  void removeElementById(WebLocalFrameImpl* frame, const AtomicString& id) {
    Element* element = frame->frame()->document()->getElementById(id);
    DCHECK(element);
    element->remove();
  }

  // Both sets the inner html and runs the document lifecycle.
  void initializeWithHTML(LocalFrame& frame, const String& htmlContent) {
    frame.document()->body()->setInnerHTML(htmlContent);
    frame.document()->view()->updateAllLifecyclePhases();
  }

  WebFrame* lastChild(WebFrame* frame) { return frame->m_lastChild; }
  WebFrame* previousSibling(WebFrame* frame) {
    return frame->m_previousSibling;
  }
  void swapAndVerifyFirstChildConsistency(const char* const message,
                                          WebFrame* parent,
                                          WebFrame* newChild);
  void swapAndVerifyMiddleChildConsistency(const char* const message,
                                           WebFrame* parent,
                                           WebFrame* newChild);
  void swapAndVerifyLastChildConsistency(const char* const message,
                                         WebFrame* parent,
                                         WebFrame* newChild);
  void swapAndVerifySubframeConsistency(const char* const message,
                                        WebFrame* parent,
                                        WebFrame* newChild);

  std::string m_baseURL;
  std::string m_notBaseURL;
  std::string m_chromeURL;
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
  registerMockedHttpURLLoad("iframes_test.html");
  registerMockedHttpURLLoad("visible_iframe.html");
  registerMockedHttpURLLoad("invisible_iframe.html");
  registerMockedHttpURLLoad("zero_sized_iframe.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "iframes_test.html");

  // Now retrieve the frames text and test it only includes visible elements.
  std::string content =
      WebFrameContentDumper::dumpWebViewAsText(webViewHelper.webView(), 1024)
          .utf8();
  EXPECT_NE(std::string::npos, content.find(" visible paragraph"));
  EXPECT_NE(std::string::npos, content.find(" visible iframe"));
  EXPECT_EQ(std::string::npos, content.find(" invisible pararaph"));
  EXPECT_EQ(std::string::npos, content.find(" invisible iframe"));
  EXPECT_EQ(std::string::npos, content.find("iframe with zero size"));
}

TEST_P(ParameterizedWebFrameTest, FrameForEnteredContext) {
  registerMockedHttpURLLoad("iframes_test.html");
  registerMockedHttpURLLoad("visible_iframe.html");
  registerMockedHttpURLLoad("invisible_iframe.html");
  registerMockedHttpURLLoad("zero_sized_iframe.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "iframes_test.html", true);

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  EXPECT_EQ(
      webViewHelper.webView()->mainFrame(),
      WebLocalFrame::frameForContext(
          webViewHelper.webView()->mainFrame()->mainWorldScriptContext()));
  EXPECT_EQ(webViewHelper.webView()->mainFrame()->firstChild(),
            WebLocalFrame::frameForContext(webViewHelper.webView()
                                               ->mainFrame()
                                               ->firstChild()
                                               ->mainWorldScriptContext()));
}

class ScriptExecutionCallbackHelper : public WebScriptExecutionCallback {
 public:
  explicit ScriptExecutionCallbackHelper(v8::Local<v8::Context> context)
      : m_didComplete(false), m_boolValue(false), m_context(context) {}
  ~ScriptExecutionCallbackHelper() {}

  bool didComplete() const { return m_didComplete; }
  const String& stringValue() const { return m_stringValue; }
  bool boolValue() { return m_boolValue; }

 private:
  void completed(const WebVector<v8::Local<v8::Value>>& values) override {
    m_didComplete = true;
    if (!values.isEmpty()) {
      if (values[0]->IsString()) {
        m_stringValue =
            toCoreString(values[0]->ToString(m_context).ToLocalChecked());
      } else if (values[0]->IsBoolean()) {
        m_boolValue = values[0].As<v8::Boolean>()->Value();
      }
    }
  }

  bool m_didComplete;
  String m_stringValue;
  bool m_boolValue;
  v8::Local<v8::Context> m_context;
};

TEST_P(ParameterizedWebFrameTest, RequestExecuteScript) {
  registerMockedHttpURLLoad("foo.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "foo.html", true);

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  ScriptExecutionCallbackHelper callbackHelper(
      webViewHelper.webView()->mainFrame()->mainWorldScriptContext());
  webViewHelper.webView()
      ->mainFrame()
      ->toWebLocalFrame()
      ->requestExecuteScriptAndReturnValue(
          WebScriptSource(WebString("'hello';")), false, &callbackHelper);
  runPendingTasks();
  EXPECT_TRUE(callbackHelper.didComplete());
  EXPECT_EQ("hello", callbackHelper.stringValue());
}

TEST_P(ParameterizedWebFrameTest, SuspendedRequestExecuteScript) {
  registerMockedHttpURLLoad("foo.html");
  registerMockedHttpURLLoad("bar.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "foo.html", true);

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  ScriptExecutionCallbackHelper callbackHelper(
      webViewHelper.webView()->mainFrame()->mainWorldScriptContext());

  // Suspend scheduled tasks so the script doesn't run.
  webViewHelper.webView()
      ->mainFrameImpl()
      ->frame()
      ->document()
      ->suspendScheduledTasks();
  webViewHelper.webView()->mainFrameImpl()->requestExecuteScriptAndReturnValue(
      WebScriptSource(WebString("'hello';")), false, &callbackHelper);
  runPendingTasks();
  EXPECT_FALSE(callbackHelper.didComplete());

  // If the frame navigates, pending scripts should be removed, but the callback
  // should always be ran.
  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              m_baseURL + "bar.html");
  EXPECT_TRUE(callbackHelper.didComplete());
  EXPECT_EQ(String(), callbackHelper.stringValue());
}

TEST_P(ParameterizedWebFrameTest, RequestExecuteV8Function) {
  registerMockedHttpURLLoad("foo.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "foo.html", true);

  auto callback = [](const v8::FunctionCallbackInfo<v8::Value>& info) {
    info.GetReturnValue().Set(v8String(info.GetIsolate(), "hello"));
  };

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Context> context =
      webViewHelper.webView()->mainFrame()->mainWorldScriptContext();
  ScriptExecutionCallbackHelper callbackHelper(context);
  v8::Local<v8::Function> function =
      v8::Function::New(context, callback).ToLocalChecked();
  webViewHelper.webView()
      ->mainFrame()
      ->toWebLocalFrame()
      ->requestExecuteV8Function(context, function,
                                 v8::Undefined(context->GetIsolate()), 0,
                                 nullptr, &callbackHelper);
  runPendingTasks();
  EXPECT_TRUE(callbackHelper.didComplete());
  EXPECT_EQ("hello", callbackHelper.stringValue());
}

TEST_P(ParameterizedWebFrameTest, RequestExecuteV8FunctionWhileSuspended) {
  registerMockedHttpURLLoad("foo.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "foo.html", true);

  auto callback = [](const v8::FunctionCallbackInfo<v8::Value>& info) {
    info.GetReturnValue().Set(v8String(info.GetIsolate(), "hello"));
  };

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Context> context =
      webViewHelper.webView()->mainFrame()->mainWorldScriptContext();

  // Suspend scheduled tasks so the script doesn't run.
  WebLocalFrameImpl* mainFrame = webViewHelper.webView()->mainFrameImpl();
  mainFrame->frame()->document()->suspendScheduledTasks();

  ScriptExecutionCallbackHelper callbackHelper(context);
  v8::Local<v8::Function> function =
      v8::Function::New(context, callback).ToLocalChecked();
  mainFrame->requestExecuteV8Function(context, function,
                                      v8::Undefined(context->GetIsolate()), 0,
                                      nullptr, &callbackHelper);
  runPendingTasks();
  EXPECT_FALSE(callbackHelper.didComplete());

  mainFrame->frame()->document()->resumeScheduledTasks();
  runPendingTasks();
  EXPECT_TRUE(callbackHelper.didComplete());
  EXPECT_EQ("hello", callbackHelper.stringValue());
}

TEST_P(ParameterizedWebFrameTest,
       RequestExecuteV8FunctionWhileSuspendedWithUserGesture) {
  registerMockedHttpURLLoad("foo.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "foo.html", true);

  auto callback = [](const v8::FunctionCallbackInfo<v8::Value>& info) {
    info.GetReturnValue().Set(v8::Boolean::New(
        info.GetIsolate(), UserGestureIndicator::processingUserGesture()));
  };

  // Suspend scheduled tasks so the script doesn't run.
  WebLocalFrameImpl* mainFrame = webViewHelper.webView()->mainFrameImpl();
  Document* document = mainFrame->frame()->document();
  document->suspendScheduledTasks();

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Context> context =
      webViewHelper.webView()->mainFrame()->mainWorldScriptContext();

  std::unique_ptr<UserGestureIndicator> indicator =
      wrapUnique(new UserGestureIndicator(DocumentUserGestureToken::create(
          document, UserGestureToken::NewGesture)));
  ScriptExecutionCallbackHelper callbackHelper(context);
  v8::Local<v8::Function> function =
      v8::Function::New(context, callback).ToLocalChecked();
  mainFrame->requestExecuteV8Function(
      mainFrame->mainWorldScriptContext(), function,
      v8::Undefined(context->GetIsolate()), 0, nullptr, &callbackHelper);

  runPendingTasks();
  EXPECT_FALSE(callbackHelper.didComplete());

  mainFrame->frame()->document()->resumeScheduledTasks();
  runPendingTasks();
  EXPECT_TRUE(callbackHelper.didComplete());
  EXPECT_EQ(true, callbackHelper.boolValue());
}

TEST_P(ParameterizedWebFrameTest, IframeScriptRemovesSelf) {
  registerMockedHttpURLLoad("single_iframe.html");
  registerMockedHttpURLLoad("visible_iframe.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "single_iframe.html", true);

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  ScriptExecutionCallbackHelper callbackHelper(
      webViewHelper.webView()->mainFrame()->mainWorldScriptContext());
  webViewHelper.webView()
      ->mainFrame()
      ->firstChild()
      ->toWebLocalFrame()
      ->requestExecuteScriptAndReturnValue(
          WebScriptSource(WebString(
              "var iframe = "
              "window.top.document.getElementsByTagName('iframe')[0]; "
              "window.top.document.body.removeChild(iframe); 'hello';")),
          false, &callbackHelper);
  runPendingTasks();
  EXPECT_TRUE(callbackHelper.didComplete());
  EXPECT_EQ(String(), callbackHelper.stringValue());
}

TEST_P(ParameterizedWebFrameTest, FormWithNullFrame) {
  registerMockedHttpURLLoad("form.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "form.html");

  WebVector<WebFormElement> forms;
  webViewHelper.webView()->mainFrame()->document().forms(forms);
  webViewHelper.reset();

  EXPECT_EQ(forms.size(), 1U);

  // This test passes if this doesn't crash.
  WebSearchableFormData searchableDataForm(forms[0]);
}

TEST_P(ParameterizedWebFrameTest, ChromePageJavascript) {
  registerMockedChromeURLLoad("history.html");

  // Pass true to enable JavaScript.
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_chromeURL + "history.html", true);

  // Try to run JS against the chrome-style URL.
  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              "javascript:document.body.appendChild(document."
                              "createTextNode('Clobbered'))");

  // Now retrieve the frame's text and ensure it was modified by running
  // javascript.
  std::string content =
      WebFrameContentDumper::dumpWebViewAsText(webViewHelper.webView(), 1024)
          .utf8();
  EXPECT_NE(std::string::npos, content.find("Clobbered"));
}

TEST_P(ParameterizedWebFrameTest, ChromePageNoJavascript) {
  registerMockedChromeURLLoad("history.html");

  /// Pass true to enable JavaScript.
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_chromeURL + "history.html", true);

  // Try to run JS against the chrome-style URL after prohibiting it.
  WebSecurityPolicy::registerURLSchemeAsNotAllowingJavascriptURLs("chrome");
  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              "javascript:document.body.appendChild(document."
                              "createTextNode('Clobbered'))");

  // Now retrieve the frame's text and ensure it wasn't modified by running
  // javascript.
  std::string content =
      WebFrameContentDumper::dumpWebViewAsText(webViewHelper.webView(), 1024)
          .utf8();
  EXPECT_EQ(std::string::npos, content.find("Clobbered"));
}

TEST_P(ParameterizedWebFrameTest, LocationSetHostWithMissingPort) {
  std::string fileName = "print-location-href.html";
  registerMockedHttpURLLoad(fileName);
  URLTestHelpers::registerMockedURLLoad(
      toKURL("http://internal.test:0/" + fileName),
      WebString::fromUTF8(fileName));

  FrameTestHelpers::WebViewHelper webViewHelper;

  /// Pass true to enable JavaScript.
  webViewHelper.initializeAndLoad(m_baseURL + fileName, true);

  // Setting host to "hostname:" should be treated as "hostname:0".
  FrameTestHelpers::loadFrame(
      webViewHelper.webView()->mainFrame(),
      "javascript:location.host = 'internal.test:'; void 0;");

  FrameTestHelpers::loadFrame(
      webViewHelper.webView()->mainFrame(),
      "javascript:document.body.textContent = location.href; void 0;");

  std::string content =
      WebFrameContentDumper::dumpWebViewAsText(webViewHelper.webView(), 1024)
          .utf8();
  EXPECT_EQ("http://internal.test:0/" + fileName, content);
}

TEST_P(ParameterizedWebFrameTest, LocationSetEmptyPort) {
  std::string fileName = "print-location-href.html";
  registerMockedHttpURLLoad(fileName);
  URLTestHelpers::registerMockedURLLoad(
      toKURL("http://internal.test:0/" + fileName),
      WebString::fromUTF8(fileName));

  FrameTestHelpers::WebViewHelper webViewHelper;

  /// Pass true to enable JavaScript.
  webViewHelper.initializeAndLoad(m_baseURL + fileName, true);

  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              "javascript:location.port = ''; void 0;");

  FrameTestHelpers::loadFrame(
      webViewHelper.webView()->mainFrame(),
      "javascript:document.body.textContent = location.href; void 0;");

  std::string content =
      WebFrameContentDumper::dumpWebViewAsText(webViewHelper.webView(), 1024)
          .utf8();
  EXPECT_EQ("http://internal.test:0/" + fileName, content);
}

class EvaluateOnLoadWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  EvaluateOnLoadWebFrameClient() : m_executing(false), m_wasExecuted(false) {}

  void didClearWindowObject(WebLocalFrame* frame) override {
    EXPECT_FALSE(m_executing);
    m_wasExecuted = true;
    m_executing = true;
    v8::HandleScope handleScope(v8::Isolate::GetCurrent());
    frame->executeScriptAndReturnValue(
        WebScriptSource(WebString("window.someProperty = 42;")));
    m_executing = false;
  }

  bool m_executing;
  bool m_wasExecuted;
};

TEST_P(ParameterizedWebFrameTest, DidClearWindowObjectIsNotRecursive) {
  EvaluateOnLoadWebFrameClient webFrameClient;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad("about:blank", true, &webFrameClient);
  EXPECT_TRUE(webFrameClient.m_wasExecuted);
}

class CSSCallbackWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
 public:
  CSSCallbackWebFrameClient() : m_updateCount(0) {}
  void didMatchCSS(
      WebLocalFrame*,
      const WebVector<WebString>& newlyMatchingSelectors,
      const WebVector<WebString>& stoppedMatchingSelectors) override;

  std::map<WebLocalFrame*, std::set<std::string>> m_matchedSelectors;
  int m_updateCount;
};

void CSSCallbackWebFrameClient::didMatchCSS(
    WebLocalFrame* frame,
    const WebVector<WebString>& newlyMatchingSelectors,
    const WebVector<WebString>& stoppedMatchingSelectors) {
  ++m_updateCount;
  std::set<std::string>& frameSelectors = m_matchedSelectors[frame];
  for (size_t i = 0; i < newlyMatchingSelectors.size(); ++i) {
    std::string selector = newlyMatchingSelectors[i].utf8();
    EXPECT_EQ(0U, frameSelectors.count(selector)) << selector;
    frameSelectors.insert(selector);
  }
  for (size_t i = 0; i < stoppedMatchingSelectors.size(); ++i) {
    std::string selector = stoppedMatchingSelectors[i].utf8();
    EXPECT_EQ(1U, frameSelectors.count(selector)) << selector;
    frameSelectors.erase(selector);
  }
}

class WebFrameCSSCallbackTest : public ::testing::Test {
 protected:
  WebFrameCSSCallbackTest() {
    m_frame = m_helper.initializeAndLoad("about:blank", true, &m_client)
                  ->mainFrame()
                  ->toWebLocalFrame();
  }

  ~WebFrameCSSCallbackTest() {
    EXPECT_EQ(1U, m_client.m_matchedSelectors.size());
  }

  WebDocument doc() const { return m_frame->document(); }

  int updateCount() const { return m_client.m_updateCount; }

  const std::set<std::string>& matchedSelectors() {
    return m_client.m_matchedSelectors[m_frame];
  }

  void loadHTML(const std::string& html) {
    FrameTestHelpers::loadHTMLString(m_frame, html, toKURL("about:blank"));
  }

  void executeScript(const WebString& code) {
    m_frame->executeScript(WebScriptSource(code));
    m_frame->view()->updateAllLifecyclePhases();
    runPendingTasks();
  }

  CSSCallbackWebFrameClient m_client;
  FrameTestHelpers::WebViewHelper m_helper;
  WebLocalFrame* m_frame;
};

TEST_F(WebFrameCSSCallbackTest, AuthorStyleSheet) {
  loadHTML(
      "<style>"
      // This stylesheet checks that the internal property and value can't be
      // set by a stylesheet, only WebDocument::watchCSSSelectors().
      "div.initial_on { -internal-callback: none; }"
      "div.initial_off { -internal-callback: -internal-presence; }"
      "</style>"
      "<div class=\"initial_on\"></div>"
      "<div class=\"initial_off\"></div>");

  Vector<WebString> selectors;
  selectors.append(WebString::fromUTF8("div.initial_on"));
  m_frame->document().watchCSSSelectors(WebVector<WebString>(selectors));
  m_frame->view()->updateAllLifecyclePhases();
  runPendingTasks();
  EXPECT_EQ(1, updateCount());
  EXPECT_THAT(matchedSelectors(), ElementsAre("div.initial_on"));

  // Check that adding a watched selector calls back for already-present nodes.
  selectors.append(WebString::fromUTF8("div.initial_off"));
  doc().watchCSSSelectors(WebVector<WebString>(selectors));
  m_frame->view()->updateAllLifecyclePhases();
  runPendingTasks();
  EXPECT_EQ(2, updateCount());
  EXPECT_THAT(matchedSelectors(),
              ElementsAre("div.initial_off", "div.initial_on"));

  // Check that we can turn off callbacks for certain selectors.
  doc().watchCSSSelectors(WebVector<WebString>());
  m_frame->view()->updateAllLifecyclePhases();
  runPendingTasks();
  EXPECT_EQ(3, updateCount());
  EXPECT_THAT(matchedSelectors(), ElementsAre());
}

TEST_F(WebFrameCSSCallbackTest, SharedComputedStyle) {
  // Check that adding an element calls back when it matches an existing rule.
  Vector<WebString> selectors;
  selectors.append(WebString::fromUTF8("span"));
  doc().watchCSSSelectors(WebVector<WebString>(selectors));

  executeScript(
      "i1 = document.createElement('span');"
      "i1.id = 'first_span';"
      "document.body.appendChild(i1)");
  EXPECT_EQ(1, updateCount());
  EXPECT_THAT(matchedSelectors(), ElementsAre("span"));

  // Adding a second element that shares a ComputedStyle shouldn't call back.
  // We use <span>s to avoid default style rules that can set
  // ComputedStyle::unique().
  executeScript(
      "i2 = document.createElement('span');"
      "i2.id = 'second_span';"
      "i1 = document.getElementById('first_span');"
      "i1.parentNode.insertBefore(i2, i1.nextSibling);");
  EXPECT_EQ(1, updateCount());
  EXPECT_THAT(matchedSelectors(), ElementsAre("span"));

  // Removing the first element shouldn't call back.
  executeScript(
      "i1 = document.getElementById('first_span');"
      "i1.parentNode.removeChild(i1);");
  EXPECT_EQ(1, updateCount());
  EXPECT_THAT(matchedSelectors(), ElementsAre("span"));

  // But removing the second element *should* call back.
  executeScript(
      "i2 = document.getElementById('second_span');"
      "i2.parentNode.removeChild(i2);");
  EXPECT_EQ(2, updateCount());
  EXPECT_THAT(matchedSelectors(), ElementsAre());
}

TEST_F(WebFrameCSSCallbackTest, CatchesAttributeChange) {
  loadHTML("<span></span>");

  Vector<WebString> selectors;
  selectors.append(WebString::fromUTF8("span[attr=\"value\"]"));
  doc().watchCSSSelectors(WebVector<WebString>(selectors));
  runPendingTasks();

  EXPECT_EQ(0, updateCount());
  EXPECT_THAT(matchedSelectors(), ElementsAre());

  executeScript(
      "document.querySelector('span').setAttribute('attr', 'value');");
  EXPECT_EQ(1, updateCount());
  EXPECT_THAT(matchedSelectors(), ElementsAre("span[attr=\"value\"]"));
}

TEST_F(WebFrameCSSCallbackTest, DisplayNone) {
  loadHTML("<div style='display:none'><span></span></div>");

  Vector<WebString> selectors;
  selectors.append(WebString::fromUTF8("span"));
  doc().watchCSSSelectors(WebVector<WebString>(selectors));
  runPendingTasks();

  EXPECT_EQ(0, updateCount()) << "Don't match elements in display:none trees.";

  executeScript(
      "d = document.querySelector('div');"
      "d.style.display = 'block';");
  EXPECT_EQ(1, updateCount()) << "Match elements when they become displayed.";
  EXPECT_THAT(matchedSelectors(), ElementsAre("span"));

  executeScript(
      "d = document.querySelector('div');"
      "d.style.display = 'none';");
  EXPECT_EQ(2, updateCount())
      << "Unmatch elements when they become undisplayed.";
  EXPECT_THAT(matchedSelectors(), ElementsAre());

  executeScript(
      "s = document.querySelector('span');"
      "s.style.display = 'none';");
  EXPECT_EQ(2, updateCount())
      << "No effect from no-display'ing a span that's already undisplayed.";

  executeScript(
      "d = document.querySelector('div');"
      "d.style.display = 'block';");
  EXPECT_EQ(2, updateCount())
      << "No effect from displaying a div whose span is display:none.";

  executeScript(
      "s = document.querySelector('span');"
      "s.style.display = 'inline';");
  EXPECT_EQ(3, updateCount())
      << "Now the span is visible and produces a callback.";
  EXPECT_THAT(matchedSelectors(), ElementsAre("span"));

  executeScript(
      "s = document.querySelector('span');"
      "s.style.display = 'none';");
  EXPECT_EQ(4, updateCount())
      << "Undisplaying the span directly should produce another callback.";
  EXPECT_THAT(matchedSelectors(), ElementsAre());
}

TEST_F(WebFrameCSSCallbackTest, Reparenting) {
  loadHTML(
      "<div id='d1'><span></span></div>"
      "<div id='d2'></div>");

  Vector<WebString> selectors;
  selectors.append(WebString::fromUTF8("span"));
  doc().watchCSSSelectors(WebVector<WebString>(selectors));
  m_frame->view()->updateAllLifecyclePhases();
  runPendingTasks();

  EXPECT_EQ(1, updateCount());
  EXPECT_THAT(matchedSelectors(), ElementsAre("span"));

  executeScript(
      "s = document.querySelector('span');"
      "d2 = document.getElementById('d2');"
      "d2.appendChild(s);");
  EXPECT_EQ(1, updateCount()) << "Just moving an element that continues to "
                                 "match shouldn't send a spurious callback.";
  EXPECT_THAT(matchedSelectors(), ElementsAre("span"));
}

TEST_F(WebFrameCSSCallbackTest, MultiSelector) {
  loadHTML("<span></span>");

  // Check that selector lists match as the whole list, not as each element
  // independently.
  Vector<WebString> selectors;
  selectors.append(WebString::fromUTF8("span"));
  selectors.append(WebString::fromUTF8("span,p"));
  doc().watchCSSSelectors(WebVector<WebString>(selectors));
  m_frame->view()->updateAllLifecyclePhases();
  runPendingTasks();

  EXPECT_EQ(1, updateCount());
  EXPECT_THAT(matchedSelectors(), ElementsAre("span", "span, p"));
}

TEST_F(WebFrameCSSCallbackTest, InvalidSelector) {
  loadHTML("<p><span></span></p>");

  // Build a list with one valid selector and one invalid.
  Vector<WebString> selectors;
  selectors.append(WebString::fromUTF8("span"));
  selectors.append(WebString::fromUTF8("["));       // Invalid.
  selectors.append(WebString::fromUTF8("p span"));  // Not compound.
  doc().watchCSSSelectors(WebVector<WebString>(selectors));
  m_frame->view()->updateAllLifecyclePhases();
  runPendingTasks();

  EXPECT_EQ(1, updateCount());
  EXPECT_THAT(matchedSelectors(), ElementsAre("span"))
      << "An invalid selector shouldn't prevent other selectors from matching.";
}

TEST_P(ParameterizedWebFrameTest, DispatchMessageEventWithOriginCheck) {
  registerMockedHttpURLLoad("postmessage_test.html");

  // Pass true to enable JavaScript.
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "postmessage_test.html", true);

  // Send a message with the correct origin.
  WebSecurityOrigin correctOrigin(WebSecurityOrigin::create(toKURL(m_baseURL)));
  WebDocument document = webViewHelper.webView()->mainFrame()->document();
  WebSerializedScriptValue data(WebSerializedScriptValue::fromString("foo"));
  WebDOMMessageEvent message(data, "http://origin.com");
  webViewHelper.webView()->mainFrame()->dispatchMessageEventWithOriginCheck(
      correctOrigin, message);

  // Send another message with incorrect origin.
  WebSecurityOrigin incorrectOrigin(
      WebSecurityOrigin::create(toKURL(m_chromeURL)));
  webViewHelper.webView()->mainFrame()->dispatchMessageEventWithOriginCheck(
      incorrectOrigin, message);

  // Verify that only the first addition is in the body of the page.
  std::string content =
      WebFrameContentDumper::dumpWebViewAsText(webViewHelper.webView(), 1024)
          .utf8();
  EXPECT_NE(std::string::npos, content.find("Message 1."));
  EXPECT_EQ(std::string::npos, content.find("Message 2."));
}

TEST_P(ParameterizedWebFrameTest, PostMessageThenDetach) {
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad("about:blank");

  LocalFrame* frame =
      toLocalFrame(webViewHelper.webView()->page()->mainFrame());
  NonThrowableExceptionState exceptionState;
  MessagePortArray messagePorts;
  frame->domWindow()->postMessage(SerializedScriptValue::serialize("message"),
                                  messagePorts, "*", frame->localDOMWindow(),
                                  exceptionState);
  webViewHelper.reset();
  EXPECT_FALSE(exceptionState.hadException());

  // Success is not crashing.
  runPendingTasks();
}

namespace {

class FixedLayoutTestWebViewClient
    : public FrameTestHelpers::TestWebViewClient {
 public:
  WebScreenInfo screenInfo() override { return m_screenInfo; }

  WebScreenInfo m_screenInfo;
};

class FakeCompositingWebViewClient : public FixedLayoutTestWebViewClient {};

// Viewport settings need to be set before the page gets loaded
void enableViewportSettings(WebSettings* settings) {
  settings->setViewportMetaEnabled(true);
  settings->setViewportEnabled(true);
  settings->setMainFrameResizesAreOrientationChanges(true);
  settings->setShrinksViewportContentToFit(true);
}

// Helper function to set autosizing multipliers on a document.
bool setTextAutosizingMultiplier(Document* document, float multiplier) {
  bool multiplierSet = false;
  for (LayoutItem layoutItem = document->layoutViewItem(); !layoutItem.isNull();
       layoutItem = layoutItem.nextInPreOrder()) {
    if (layoutItem.style()) {
      layoutItem.mutableStyleRef().setTextAutosizingMultiplier(multiplier);

      EXPECT_EQ(multiplier, layoutItem.style()->textAutosizingMultiplier());
      multiplierSet = true;
    }
  }
  return multiplierSet;
}

// Helper function to check autosizing multipliers on a document.
bool checkTextAutosizingMultiplier(Document* document, float multiplier) {
  bool multiplierChecked = false;
  for (LayoutItem layoutItem = document->layoutViewItem(); !layoutItem.isNull();
       layoutItem = layoutItem.nextInPreOrder()) {
    if (layoutItem.style() && layoutItem.isText()) {
      EXPECT_EQ(multiplier, layoutItem.style()->textAutosizingMultiplier());
      multiplierChecked = true;
    }
  }
  return multiplierChecked;
}

}  // anonymous namespace

TEST_P(ParameterizedWebFrameTest,
       ChangeInFixedLayoutResetsTextAutosizingMultipliers) {
  registerMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebViewClient client;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "fixed_layout.html", true,
                                  nullptr, &client, nullptr,
                                  enableViewportSettings);

  Document* document =
      toLocalFrame(webViewHelper.webView()->page()->mainFrame())->document();
  document->settings()->setTextAutosizingEnabled(true);
  EXPECT_TRUE(document->settings()->textAutosizingEnabled());
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_TRUE(setTextAutosizingMultiplier(document, 2));

  ViewportDescription description = document->viewportDescription();
  // Choose a width that's not going match the viewport width of the loaded
  // document.
  description.minWidth = Length(100, blink::Fixed);
  description.maxWidth = Length(100, blink::Fixed);
  webViewHelper.webView()->updatePageDefinedViewportConstraints(description);

  EXPECT_TRUE(checkTextAutosizingMultiplier(document, 1));
}

TEST_P(ParameterizedWebFrameTest,
       WorkingTextAutosizingMultipliers_VirtualViewport) {
  const std::string htmlFile = "fixed_layout.html";
  registerMockedHttpURLLoad(htmlFile);

  FixedLayoutTestWebViewClient client;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + htmlFile, true, nullptr, &client,
                                  nullptr, configureAndroid);

  Document* document =
      toLocalFrame(webViewHelper.webView()->page()->mainFrame())->document();
  document->settings()->setTextAutosizingEnabled(true);
  EXPECT_TRUE(document->settings()->textAutosizingEnabled());

  webViewHelper.resize(WebSize(490, 800));

  // Multiplier: 980 / 490 = 2.0
  EXPECT_TRUE(checkTextAutosizingMultiplier(document, 2.0));
}

TEST_P(ParameterizedWebFrameTest,
       VisualViewportSetSizeInvalidatesTextAutosizingMultipliers) {
  registerMockedHttpURLLoad("iframe_reload.html");
  registerMockedHttpURLLoad("visible_iframe.html");

  FixedLayoutTestWebViewClient client;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "iframe_reload.html", true,
                                  nullptr, &client, nullptr,
                                  enableViewportSettings);

  LocalFrame* mainFrame =
      toLocalFrame(webViewHelper.webView()->page()->mainFrame());
  Document* document = mainFrame->document();
  FrameView* frameView = webViewHelper.webView()->mainFrameImpl()->frameView();
  document->settings()->setTextAutosizingEnabled(true);
  EXPECT_TRUE(document->settings()->textAutosizingEnabled());
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  for (Frame* frame = mainFrame; frame; frame = frame->tree().traverseNext()) {
    if (!frame->isLocalFrame())
      continue;
    EXPECT_TRUE(
        setTextAutosizingMultiplier(toLocalFrame(frame)->document(), 2));
    for (LayoutItem layoutItem =
             toLocalFrame(frame)->document()->layoutViewItem();
         !layoutItem.isNull(); layoutItem = layoutItem.nextInPreOrder()) {
      if (layoutItem.isText())
        EXPECT_FALSE(layoutItem.needsLayout());
    }
  }

  frameView->page()->frameHost().visualViewport().setSize(IntSize(200, 200));

  for (Frame* frame = mainFrame; frame; frame = frame->tree().traverseNext()) {
    if (!frame->isLocalFrame())
      continue;
    for (LayoutItem layoutItem =
             toLocalFrame(frame)->document()->layoutViewItem();
         !layoutItem.isNull(); layoutItem = layoutItem.nextInPreOrder()) {
      if (layoutItem.isText())
        EXPECT_TRUE(layoutItem.needsLayout());
    }
  }
}

TEST_P(ParameterizedWebFrameTest, ZeroHeightPositiveWidthNotIgnored) {
  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 1280;
  int viewportHeight = 0;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(true, nullptr, &client, nullptr,
                           enableViewportSettings);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_EQ(viewportWidth, webViewHelper.webView()
                               ->mainFrameImpl()
                               ->frameView()
                               ->layoutSize()
                               .width());
  EXPECT_EQ(viewportHeight, webViewHelper.webView()
                                ->mainFrameImpl()
                                ->frameView()
                                ->layoutSize()
                                .height());
}

TEST_P(ParameterizedWebFrameTest,
       DeviceScaleFactorUsesDefaultWithoutViewportTag) {
  registerMockedHttpURLLoad("no_viewport_tag.html");

  int viewportWidth = 640;
  int viewportHeight = 480;

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 2;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "no_viewport_tag.html", true,
                                  nullptr, &client, nullptr,
                                  enableViewportSettings);

  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_EQ(2, webViewHelper.webView()->page()->deviceScaleFactor());

  // Device scale factor should be independent of page scale.
  webViewHelper.webView()->setDefaultPageScaleLimits(1, 2);
  webViewHelper.webView()->setPageScaleFactor(0.5);
  webViewHelper.webView()->updateAllLifecyclePhases();
  EXPECT_EQ(1, webViewHelper.webView()->pageScaleFactor());

  // Force the layout to happen before leaving the test.
  webViewHelper.webView()->updateAllLifecyclePhases();
}

TEST_P(ParameterizedWebFrameTest, FixedLayoutInitializeAtMinimumScale) {
  registerMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  // Make sure we initialize to minimum scale, even if the window size
  // only becomes available after the load begins.
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(true, nullptr, &client, nullptr,
                           enableViewportSettings);
  webViewHelper.webView()->setDefaultPageScaleLimits(0.25f, 5);
  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              m_baseURL + "fixed_layout.html");
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  int defaultFixedLayoutWidth = 980;
  float minimumPageScaleFactor = viewportWidth / (float)defaultFixedLayoutWidth;
  EXPECT_EQ(minimumPageScaleFactor, webViewHelper.webView()->pageScaleFactor());
  EXPECT_EQ(minimumPageScaleFactor,
            webViewHelper.webView()->minimumPageScaleFactor());

  // Assume the user has pinch zoomed to page scale factor 2.
  float userPinchPageScaleFactor = 2;
  webViewHelper.webView()->setPageScaleFactor(userPinchPageScaleFactor);
  webViewHelper.webView()->updateAllLifecyclePhases();

  // Make sure we don't reset to initial scale if the page continues to load.
  webViewHelper.webView()->didCommitLoad(false, false);
  webViewHelper.webView()->didChangeContentsSize();
  EXPECT_EQ(userPinchPageScaleFactor,
            webViewHelper.webView()->pageScaleFactor());

  // Make sure we don't reset to initial scale if the viewport size changes.
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight + 100));
  EXPECT_EQ(userPinchPageScaleFactor,
            webViewHelper.webView()->pageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, WideDocumentInitializeAtMinimumScale) {
  registerMockedHttpURLLoad("wide_document.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  // Make sure we initialize to minimum scale, even if the window size
  // only becomes available after the load begins.
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(true, nullptr, &client, nullptr,
                           enableViewportSettings);
  webViewHelper.webView()->setDefaultPageScaleLimits(0.25f, 5);
  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              m_baseURL + "wide_document.html");
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  int wideDocumentWidth = 1500;
  float minimumPageScaleFactor = viewportWidth / (float)wideDocumentWidth;
  EXPECT_EQ(minimumPageScaleFactor, webViewHelper.webView()->pageScaleFactor());
  EXPECT_EQ(minimumPageScaleFactor,
            webViewHelper.webView()->minimumPageScaleFactor());

  // Assume the user has pinch zoomed to page scale factor 2.
  float userPinchPageScaleFactor = 2;
  webViewHelper.webView()->setPageScaleFactor(userPinchPageScaleFactor);
  webViewHelper.webView()->updateAllLifecyclePhases();

  // Make sure we don't reset to initial scale if the page continues to load.
  webViewHelper.webView()->didCommitLoad(false, false);
  webViewHelper.webView()->didChangeContentsSize();
  EXPECT_EQ(userPinchPageScaleFactor,
            webViewHelper.webView()->pageScaleFactor());

  // Make sure we don't reset to initial scale if the viewport size changes.
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight + 100));
  EXPECT_EQ(userPinchPageScaleFactor,
            webViewHelper.webView()->pageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, DelayedViewportInitialScale) {
  registerMockedHttpURLLoad("viewport-auto-initial-scale.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(
      m_baseURL + "viewport-auto-initial-scale.html", true, nullptr, &client,
      nullptr, enableViewportSettings);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_EQ(0.25f, webViewHelper.webView()->pageScaleFactor());

  Document* document =
      toLocalFrame(webViewHelper.webView()->page()->mainFrame())->document();
  ViewportDescription description = document->viewportDescription();
  description.zoom = 2;
  document->setViewportDescription(description);
  webViewHelper.webView()->updateAllLifecyclePhases();
  EXPECT_EQ(2, webViewHelper.webView()->pageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, setLoadWithOverviewModeToFalse) {
  registerMockedHttpURLLoad("viewport-auto-initial-scale.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(
      m_baseURL + "viewport-auto-initial-scale.html", true, nullptr, &client,
      nullptr, enableViewportSettings);
  webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
  webViewHelper.webView()->settings()->setLoadWithOverviewMode(false);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  // The page must be displayed at 100% zoom.
  EXPECT_EQ(1.0f, webViewHelper.webView()->pageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest,
       SetLoadWithOverviewModeToFalseAndNoWideViewport) {
  registerMockedHttpURLLoad("large-div.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "large-div.html", true, nullptr,
                                  &client, nullptr, enableViewportSettings);
  webViewHelper.webView()->settings()->setLoadWithOverviewMode(false);
  webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
  webViewHelper.webView()->settings()->setUseWideViewport(false);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  // The page must be displayed at 100% zoom, despite that it hosts a wide div
  // element.
  EXPECT_EQ(1.0f, webViewHelper.webView()->pageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, NoWideViewportIgnoresPageViewportWidth) {
  registerMockedHttpURLLoad("viewport-auto-initial-scale.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(
      m_baseURL + "viewport-auto-initial-scale.html", true, nullptr, &client,
      nullptr, enableViewportSettings);
  webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
  webViewHelper.webView()->settings()->setUseWideViewport(false);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  // The page sets viewport width to 3000, but with UseWideViewport == false is
  // must be ignored.
  EXPECT_EQ(viewportWidth, webViewHelper.webView()
                               ->mainFrameImpl()
                               ->frameView()
                               ->contentsSize()
                               .width());
  EXPECT_EQ(viewportHeight, webViewHelper.webView()
                                ->mainFrameImpl()
                                ->frameView()
                                ->contentsSize()
                                .height());
}

TEST_P(ParameterizedWebFrameTest,
       NoWideViewportIgnoresPageViewportWidthButAccountsScale) {
  registerMockedHttpURLLoad("viewport-wide-2x-initial-scale.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(
      m_baseURL + "viewport-wide-2x-initial-scale.html", true, nullptr, &client,
      nullptr, enableViewportSettings);
  webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
  webViewHelper.webView()->settings()->setUseWideViewport(false);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  // The page sets viewport width to 3000, but with UseWideViewport == false it
  // must be ignored while the initial scale specified by the page must be
  // accounted.
  EXPECT_EQ(viewportWidth / 2, webViewHelper.webView()
                                   ->mainFrameImpl()
                                   ->frameView()
                                   ->contentsSize()
                                   .width());
  EXPECT_EQ(viewportHeight / 2, webViewHelper.webView()
                                    ->mainFrameImpl()
                                    ->frameView()
                                    ->contentsSize()
                                    .height());
}

TEST_P(ParameterizedWebFrameTest, WideViewportSetsTo980WithoutViewportTag) {
  registerMockedHttpURLLoad("no_viewport_tag.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "no_viewport_tag.html", true,
                                  nullptr, &client, nullptr,
                                  enableViewportSettings);
  applyViewportStyleOverride(&webViewHelper);
  webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
  webViewHelper.webView()->settings()->setUseWideViewport(true);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_EQ(980, webViewHelper.webView()
                     ->mainFrameImpl()
                     ->frameView()
                     ->contentsSize()
                     .width());
  EXPECT_EQ(980.0 / viewportWidth * viewportHeight, webViewHelper.webView()
                                                        ->mainFrameImpl()
                                                        ->frameView()
                                                        ->contentsSize()
                                                        .height());
}

TEST_P(ParameterizedWebFrameTest, WideViewportSetsTo980WithXhtmlMp) {
  registerMockedHttpURLLoad("viewport/viewport-legacy-xhtmlmp.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(true, nullptr, &client, nullptr,
                           enableViewportSettings);
  applyViewportStyleOverride(&webViewHelper);
  webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
  webViewHelper.webView()->settings()->setUseWideViewport(true);
  FrameTestHelpers::loadFrame(
      webViewHelper.webView()->mainFrame(),
      m_baseURL + "viewport/viewport-legacy-xhtmlmp.html");

  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));
  EXPECT_EQ(viewportWidth, webViewHelper.webView()
                               ->mainFrameImpl()
                               ->frameView()
                               ->contentsSize()
                               .width());
  EXPECT_EQ(viewportHeight, webViewHelper.webView()
                                ->mainFrameImpl()
                                ->frameView()
                                ->contentsSize()
                                .height());
}

TEST_P(ParameterizedWebFrameTest, NoWideViewportAndHeightInMeta) {
  registerMockedHttpURLLoad("viewport-height-1000.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "viewport-height-1000.html", true,
                                  nullptr, &client, nullptr,
                                  enableViewportSettings);
  webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
  webViewHelper.webView()->settings()->setUseWideViewport(false);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_EQ(viewportWidth, webViewHelper.webView()
                               ->mainFrameImpl()
                               ->frameView()
                               ->contentsSize()
                               .width());
}

TEST_P(ParameterizedWebFrameTest, WideViewportSetsTo980WithAutoWidth) {
  registerMockedHttpURLLoad("viewport-2x-initial-scale.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "viewport-2x-initial-scale.html",
                                  true, nullptr, &client, nullptr,
                                  enableViewportSettings);
  applyViewportStyleOverride(&webViewHelper);
  webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
  webViewHelper.webView()->settings()->setUseWideViewport(true);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_EQ(980, webViewHelper.webView()
                     ->mainFrameImpl()
                     ->frameView()
                     ->contentsSize()
                     .width());
  EXPECT_EQ(980.0 / viewportWidth * viewportHeight, webViewHelper.webView()
                                                        ->mainFrameImpl()
                                                        ->frameView()
                                                        ->contentsSize()
                                                        .height());
}

TEST_P(ParameterizedWebFrameTest,
       PageViewportInitialScaleOverridesLoadWithOverviewMode) {
  registerMockedHttpURLLoad("viewport-wide-2x-initial-scale.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(
      m_baseURL + "viewport-wide-2x-initial-scale.html", true, nullptr, &client,
      nullptr, enableViewportSettings);
  webViewHelper.webView()->settings()->setLoadWithOverviewMode(false);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  // The page must be displayed at 200% zoom, as specified in its viewport meta
  // tag.
  EXPECT_EQ(2.0f, webViewHelper.webView()->pageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, setInitialPageScaleFactorPermanently) {
  registerMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  float enforcedPageScaleFactor = 2.0f;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "fixed_layout.html", true,
                                  nullptr, &client, nullptr,
                                  enableViewportSettings);
  applyViewportStyleOverride(&webViewHelper);
  webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
  webViewHelper.webView()->settings()->setLoadWithOverviewMode(false);
  webViewHelper.webView()->setInitialPageScaleOverride(enforcedPageScaleFactor);
  webViewHelper.webView()->updateAllLifecyclePhases();

  EXPECT_EQ(enforcedPageScaleFactor,
            webViewHelper.webView()->pageScaleFactor());

  int viewportWidth = 640;
  int viewportHeight = 480;
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_EQ(enforcedPageScaleFactor,
            webViewHelper.webView()->pageScaleFactor());

  webViewHelper.webView()->setInitialPageScaleOverride(-1);
  webViewHelper.webView()->updateAllLifecyclePhases();
  EXPECT_EQ(1.0, webViewHelper.webView()->pageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest,
       PermanentInitialPageScaleFactorOverridesLoadWithOverviewMode) {
  registerMockedHttpURLLoad("viewport-auto-initial-scale.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;
  float enforcedPageScaleFactor = 0.5f;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(
      m_baseURL + "viewport-auto-initial-scale.html", true, nullptr, &client,
      nullptr, enableViewportSettings);
  webViewHelper.webView()->settings()->setLoadWithOverviewMode(false);
  webViewHelper.webView()->setInitialPageScaleOverride(enforcedPageScaleFactor);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_EQ(enforcedPageScaleFactor,
            webViewHelper.webView()->pageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest,
       PermanentInitialPageScaleFactorOverridesPageViewportInitialScale) {
  registerMockedHttpURLLoad("viewport-wide-2x-initial-scale.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;
  float enforcedPageScaleFactor = 0.5f;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(
      m_baseURL + "viewport-wide-2x-initial-scale.html", true, nullptr, &client,
      nullptr, enableViewportSettings);
  webViewHelper.webView()->setInitialPageScaleOverride(enforcedPageScaleFactor);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_EQ(enforcedPageScaleFactor,
            webViewHelper.webView()->pageScaleFactor());
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
  float pageScaleFactors[] = {0.5f, 1.0f};
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(pages); ++i)
    registerMockedHttpURLLoad(pages[i]);

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 400;
  int viewportHeight = 300;
  float enforcedPageScaleFactor = 0.75f;

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(pages); ++i) {
    for (int quirkEnabled = 0; quirkEnabled <= 1; ++quirkEnabled) {
      FrameTestHelpers::WebViewHelper webViewHelper;
      webViewHelper.initializeAndLoad(m_baseURL + pages[i], true, nullptr,
                                      &client, nullptr, enableViewportSettings);
      applyViewportStyleOverride(&webViewHelper);
      webViewHelper.webView()->settings()->setClobberUserAgentInitialScaleQuirk(
          quirkEnabled);
      webViewHelper.webView()->setInitialPageScaleOverride(
          enforcedPageScaleFactor);
      webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

      float expectedPageScaleFactor =
          quirkEnabled && i < WTF_ARRAY_LENGTH(pageScaleFactors)
              ? pageScaleFactors[i]
              : enforcedPageScaleFactor;
      EXPECT_EQ(expectedPageScaleFactor,
                webViewHelper.webView()->pageScaleFactor());
    }
  }
}

TEST_P(ParameterizedWebFrameTest,
       PermanentInitialPageScaleFactorAffectsLayoutWidth) {
  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;
  float enforcedPageScaleFactor = 0.5;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad("about:blank", true, nullptr, &client,
                                  nullptr, enableViewportSettings);
  webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
  webViewHelper.webView()->settings()->setUseWideViewport(false);
  webViewHelper.webView()->settings()->setLoadWithOverviewMode(false);
  webViewHelper.webView()->setInitialPageScaleOverride(enforcedPageScaleFactor);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_EQ(viewportWidth / enforcedPageScaleFactor, webViewHelper.webView()
                                                         ->mainFrameImpl()
                                                         ->frameView()
                                                         ->contentsSize()
                                                         .width());
  EXPECT_EQ(enforcedPageScaleFactor,
            webViewHelper.webView()->pageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest,
       DocumentElementClientHeightWorksWithWrapContentMode) {
  registerMockedHttpURLLoad("0-by-0.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;

  webViewHelper.initializeAndLoad(m_baseURL + "0-by-0.html", true, nullptr,
                                  &client, nullptr, configureAndroid);
  webViewHelper.webView()->settings()->setForceZeroLayoutHeight(true);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  LocalFrame* frame = webViewHelper.webView()->mainFrameImpl()->frame();
  Document* document = frame->document();
  EXPECT_EQ(viewportHeight, document->documentElement()->clientHeight());
  EXPECT_EQ(viewportWidth, document->documentElement()->clientWidth());
}

TEST_P(ParameterizedWebFrameTest,
       SetForceZeroLayoutHeightWorksWithWrapContentMode) {
  registerMockedHttpURLLoad("0-by-0.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;

  webViewHelper.initializeAndLoad(m_baseURL + "0-by-0.html", true, nullptr,
                                  &client, nullptr, configureAndroid);
  webViewHelper.webView()->settings()->setForceZeroLayoutHeight(true);
  PaintLayerCompositor* compositor = webViewHelper.webView()->compositor();
  EXPECT_EQ(0, webViewHelper.webView()
                   ->mainFrameImpl()
                   ->frameView()
                   ->layoutSize()
                   .width());
  EXPECT_EQ(0, webViewHelper.webView()
                   ->mainFrameImpl()
                   ->frameView()
                   ->layoutSize()
                   .height());
  EXPECT_EQ(0.0, compositor->containerLayer()->size().width());
  EXPECT_EQ(0.0, compositor->containerLayer()->size().height());

  webViewHelper.resize(WebSize(viewportWidth, 0));
  EXPECT_EQ(viewportWidth, webViewHelper.webView()
                               ->mainFrameImpl()
                               ->frameView()
                               ->layoutSize()
                               .width());
  EXPECT_EQ(0, webViewHelper.webView()
                   ->mainFrameImpl()
                   ->frameView()
                   ->layoutSize()
                   .height());
  EXPECT_EQ(viewportWidth, compositor->containerLayer()->size().width());
  EXPECT_EQ(0.0, compositor->containerLayer()->size().height());

  // The flag ForceZeroLayoutHeight will cause the following resize of viewport
  // height to be ignored by the outer viewport (the container layer of
  // LayerCompositor). The height of the visualViewport, however, is not
  // affected.
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));
  EXPECT_FALSE(
      webViewHelper.webView()->mainFrameImpl()->frameView()->needsLayout());
  EXPECT_EQ(viewportWidth, webViewHelper.webView()
                               ->mainFrameImpl()
                               ->frameView()
                               ->layoutSize()
                               .width());
  EXPECT_EQ(0, webViewHelper.webView()
                   ->mainFrameImpl()
                   ->frameView()
                   ->layoutSize()
                   .height());
  EXPECT_EQ(viewportWidth, compositor->containerLayer()->size().width());
  EXPECT_EQ(viewportHeight, compositor->containerLayer()->size().height());

  LocalFrame* frame = webViewHelper.webView()->mainFrameImpl()->frame();
  VisualViewport& visualViewport = frame->page()->frameHost().visualViewport();
  EXPECT_EQ(viewportHeight, visualViewport.containerLayer()->size().height());
  EXPECT_TRUE(
      visualViewport.containerLayer()->platformLayer()->masksToBounds());
  EXPECT_FALSE(compositor->containerLayer()->platformLayer()->masksToBounds());
}

TEST_P(ParameterizedWebFrameTest, SetForceZeroLayoutHeight) {
  registerMockedHttpURLLoad("200-by-300.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;

  webViewHelper.initializeAndLoad(m_baseURL + "200-by-300.html", true, nullptr,
                                  &client, nullptr, enableViewportSettings);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_LE(viewportHeight, webViewHelper.webView()
                                ->mainFrameImpl()
                                ->frameView()
                                ->layoutSize()
                                .height());
  webViewHelper.webView()->settings()->setForceZeroLayoutHeight(true);
  EXPECT_TRUE(
      webViewHelper.webView()->mainFrameImpl()->frameView()->needsLayout());

  EXPECT_EQ(0, webViewHelper.webView()
                   ->mainFrameImpl()
                   ->frameView()
                   ->layoutSize()
                   .height());

  webViewHelper.resize(WebSize(viewportWidth, viewportHeight * 2));
  EXPECT_FALSE(
      webViewHelper.webView()->mainFrameImpl()->frameView()->needsLayout());
  EXPECT_EQ(0, webViewHelper.webView()
                   ->mainFrameImpl()
                   ->frameView()
                   ->layoutSize()
                   .height());

  webViewHelper.resize(WebSize(viewportWidth * 2, viewportHeight));
  EXPECT_EQ(0, webViewHelper.webView()
                   ->mainFrameImpl()
                   ->frameView()
                   ->layoutSize()
                   .height());

  webViewHelper.webView()->settings()->setForceZeroLayoutHeight(false);
  EXPECT_LE(viewportHeight, webViewHelper.webView()
                                ->mainFrameImpl()
                                ->frameView()
                                ->layoutSize()
                                .height());
}

TEST_F(WebFrameTest, ToggleViewportMetaOnOff) {
  registerMockedHttpURLLoad("viewport-device-width.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "viewport-device-width.html",
                                  true, 0, &client);
  WebSettings* settings = webViewHelper.webView()->settings();
  settings->setViewportMetaEnabled(false);
  settings->setViewportEnabled(true);
  settings->setMainFrameResizesAreOrientationChanges(true);
  settings->setShrinksViewportContentToFit(true);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  Document* document =
      toLocalFrame(webViewHelper.webView()->page()->mainFrame())->document();
  EXPECT_FALSE(document->viewportDescription().isLegacyViewportType());

  settings->setViewportMetaEnabled(true);
  EXPECT_TRUE(document->viewportDescription().isLegacyViewportType());

  settings->setViewportMetaEnabled(false);
  EXPECT_FALSE(document->viewportDescription().isLegacyViewportType());
}

TEST_F(WebFrameTest,
       SetForceZeroLayoutHeightWorksWithRelayoutsWhenHeightChanged) {
  // this unit test is an attempt to target a real world case where an app could
  // 1. call resize(width, 0) and setForceZeroLayoutHeight(true)
  // 2. load content (hoping that the viewport height would increase
  // as more content is added)
  // 3. fail to register touch events aimed at the loaded content
  // because the layout is only updated if either width or height is changed
  registerMockedHttpURLLoad("button.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;

  webViewHelper.initializeAndLoad(m_baseURL + "button.html", true, nullptr,
                                  &client, nullptr, configureAndroid);
  // set view height to zero so that if the height of the view is not
  // successfully updated during later resizes touch events will fail
  // (as in not hit content included in the view)
  webViewHelper.resize(WebSize(viewportWidth, 0));

  webViewHelper.webView()->settings()->setForceZeroLayoutHeight(true);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  IntPoint hitPoint = IntPoint(30, 30);  // button size is 100x100

  WebLocalFrameImpl* frame = webViewHelper.webView()->mainFrameImpl();
  Document* document = frame->frame()->document();
  Element* element = document->getElementById("tap_button");

  ASSERT_NE(nullptr, element);
  EXPECT_EQ(String("oldValue"), element->innerText());

  PlatformGestureEvent gestureEvent(
      PlatformEvent::EventType::GestureTap, hitPoint, hitPoint, IntSize(0, 0),
      0, PlatformEvent::NoModifiers, PlatformGestureSourceTouchscreen);
  webViewHelper.webView()
      ->mainFrameImpl()
      ->frame()
      ->eventHandler()
      .handleGestureEvent(gestureEvent);
  // when pressed, the button changes its own text to "updatedValue"
  EXPECT_EQ(String("updatedValue"), element->innerText());
}

TEST_F(WebFrameTest, FrameOwnerPropertiesMargin) {
  FrameTestHelpers::TestWebViewClient viewClient;
  FrameTestHelpers::TestWebRemoteFrameClient remoteClient;
  WebView* view = WebView::create(&viewClient, WebPageVisibilityStateVisible);
  view->settings()->setJavaScriptEnabled(true);
  view->setMainFrame(remoteClient.frame());
  WebRemoteFrame* root = view->mainFrame()->toWebRemoteFrame();
  root->setReplicatedOrigin(SecurityOrigin::createUnique());

  WebFrameOwnerProperties properties;
  properties.marginWidth = 11;
  properties.marginHeight = 22;
  WebLocalFrameImpl* localFrame = FrameTestHelpers::createLocalChild(
      root, "frameName", nullptr, nullptr, nullptr, properties);

  registerMockedHttpURLLoad("frame_owner_properties.html");
  FrameTestHelpers::loadFrame(localFrame,
                              m_baseURL + "frame_owner_properties.html");

  // Check if the LocalFrame has seen the marginwidth and marginheight
  // properties.
  Document* childDocument = localFrame->frame()->document();
  EXPECT_EQ(11, childDocument->firstBodyElement()->getIntegralAttribute(
                    HTMLNames::marginwidthAttr));
  EXPECT_EQ(22, childDocument->firstBodyElement()->getIntegralAttribute(
                    HTMLNames::marginheightAttr));

  FrameView* frameView = localFrame->frameView();
  // Expect scrollbars to be enabled by default.
  EXPECT_NE(nullptr, frameView->horizontalScrollbar());
  EXPECT_NE(nullptr, frameView->verticalScrollbar());

  view->close();
}

TEST_F(WebFrameTest, FrameOwnerPropertiesScrolling) {
  FrameTestHelpers::TestWebViewClient viewClient;
  FrameTestHelpers::TestWebRemoteFrameClient remoteClient;
  WebView* view = WebView::create(&viewClient, WebPageVisibilityStateVisible);
  view->settings()->setJavaScriptEnabled(true);
  view->setMainFrame(remoteClient.frame());
  WebRemoteFrame* root = view->mainFrame()->toWebRemoteFrame();
  root->setReplicatedOrigin(SecurityOrigin::createUnique());

  WebFrameOwnerProperties properties;
  // Turn off scrolling in the subframe.
  properties.scrollingMode = WebFrameOwnerProperties::ScrollingMode::AlwaysOff;
  WebLocalFrameImpl* localFrame = FrameTestHelpers::createLocalChild(
      root, "frameName", nullptr, nullptr, nullptr, properties);

  registerMockedHttpURLLoad("frame_owner_properties.html");
  FrameTestHelpers::loadFrame(localFrame,
                              m_baseURL + "frame_owner_properties.html");

  Document* childDocument = localFrame->frame()->document();
  EXPECT_EQ(0, childDocument->firstBodyElement()->getIntegralAttribute(
                   HTMLNames::marginwidthAttr));
  EXPECT_EQ(0, childDocument->firstBodyElement()->getIntegralAttribute(
                   HTMLNames::marginheightAttr));

  FrameView* frameView =
      static_cast<WebLocalFrameImpl*>(localFrame)->frameView();
  EXPECT_EQ(nullptr, frameView->horizontalScrollbar());
  EXPECT_EQ(nullptr, frameView->verticalScrollbar());

  view->close();
}

TEST_P(ParameterizedWebFrameTest,
       SetForceZeroLayoutHeightWorksAcrossNavigations) {
  registerMockedHttpURLLoad("200-by-300.html");
  registerMockedHttpURLLoad("large-div.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;

  webViewHelper.initializeAndLoad(m_baseURL + "200-by-300.html", true, nullptr,
                                  &client, nullptr, enableViewportSettings);
  webViewHelper.webView()->settings()->setForceZeroLayoutHeight(true);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              m_baseURL + "large-div.html");
  webViewHelper.webView()->updateAllLifecyclePhases();

  EXPECT_EQ(0, webViewHelper.webView()
                   ->mainFrameImpl()
                   ->frameView()
                   ->layoutSize()
                   .height());
}

TEST_P(ParameterizedWebFrameTest,
       SetForceZeroLayoutHeightWithWideViewportQuirk) {
  registerMockedHttpURLLoad("200-by-300.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;

  webViewHelper.initializeAndLoad(m_baseURL + "200-by-300.html", true, nullptr,
                                  &client, nullptr, enableViewportSettings);
  webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
  webViewHelper.webView()->settings()->setUseWideViewport(true);
  webViewHelper.webView()->settings()->setForceZeroLayoutHeight(true);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_EQ(0, webViewHelper.webView()
                   ->mainFrameImpl()
                   ->frameView()
                   ->layoutSize()
                   .height());
}

TEST_P(ParameterizedWebFrameTest, WideViewportAndWideContentWithInitialScale) {
  registerMockedHttpURLLoad("wide_document_width_viewport.html");
  registerMockedHttpURLLoad("white-1x1.png");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 600;
  int viewportHeight = 800;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad("about:blank", true, nullptr, &client,
                                  nullptr, enableViewportSettings);
  webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
  webViewHelper.webView()->settings()->setUseWideViewport(true);
  webViewHelper.webView()->settings()->setViewportMetaLayoutSizeQuirk(true);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              m_baseURL + "wide_document_width_viewport.html");
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  int wideDocumentWidth = 800;
  float minimumPageScaleFactor = viewportWidth / (float)wideDocumentWidth;
  EXPECT_EQ(minimumPageScaleFactor, webViewHelper.webView()->pageScaleFactor());
  EXPECT_EQ(minimumPageScaleFactor,
            webViewHelper.webView()->minimumPageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, WideViewportQuirkClobbersHeight) {
  registerMockedHttpURLLoad("viewport-height-1000.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 600;
  int viewportHeight = 800;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad("about:blank", true, nullptr, &client,
                                  nullptr, enableViewportSettings);
  webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
  webViewHelper.webView()->settings()->setUseWideViewport(false);
  webViewHelper.webView()->settings()->setViewportMetaLayoutSizeQuirk(true);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              m_baseURL + "viewport-height-1000.html");
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_EQ(800, webViewHelper.webView()
                     ->mainFrameImpl()
                     ->frameView()
                     ->layoutSize()
                     .height());
  EXPECT_EQ(1, webViewHelper.webView()->pageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, LayoutSize320Quirk) {
  registerMockedHttpURLLoad("viewport/viewport-30.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 600;
  int viewportHeight = 800;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad("about:blank", true, nullptr, &client,
                                  nullptr, enableViewportSettings);
  webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
  webViewHelper.webView()->settings()->setUseWideViewport(true);
  webViewHelper.webView()->settings()->setViewportMetaLayoutSizeQuirk(true);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              m_baseURL + "viewport/viewport-30.html");
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_EQ(600, webViewHelper.webView()
                     ->mainFrameImpl()
                     ->frameView()
                     ->layoutSize()
                     .width());
  EXPECT_EQ(800, webViewHelper.webView()
                     ->mainFrameImpl()
                     ->frameView()
                     ->layoutSize()
                     .height());
  EXPECT_EQ(1, webViewHelper.webView()->pageScaleFactor());

  // The magic number to snap to device-width is 320, so test that 321 is
  // respected.
  Document* document =
      toLocalFrame(webViewHelper.webView()->page()->mainFrame())->document();
  ViewportDescription description = document->viewportDescription();
  description.minWidth = Length(321, blink::Fixed);
  description.maxWidth = Length(321, blink::Fixed);
  document->setViewportDescription(description);
  webViewHelper.webView()->updateAllLifecyclePhases();
  EXPECT_EQ(321, webViewHelper.webView()
                     ->mainFrameImpl()
                     ->frameView()
                     ->layoutSize()
                     .width());

  description.minWidth = Length(320, blink::Fixed);
  description.maxWidth = Length(320, blink::Fixed);
  document->setViewportDescription(description);
  webViewHelper.webView()->updateAllLifecyclePhases();
  EXPECT_EQ(600, webViewHelper.webView()
                     ->mainFrameImpl()
                     ->frameView()
                     ->layoutSize()
                     .width());

  description = document->viewportDescription();
  description.maxHeight = Length(1000, blink::Fixed);
  document->setViewportDescription(description);
  webViewHelper.webView()->updateAllLifecyclePhases();
  EXPECT_EQ(1000, webViewHelper.webView()
                      ->mainFrameImpl()
                      ->frameView()
                      ->layoutSize()
                      .height());

  description.maxHeight = Length(320, blink::Fixed);
  document->setViewportDescription(description);
  webViewHelper.webView()->updateAllLifecyclePhases();
  EXPECT_EQ(800, webViewHelper.webView()
                     ->mainFrameImpl()
                     ->frameView()
                     ->layoutSize()
                     .height());
}

TEST_P(ParameterizedWebFrameTest, ZeroValuesQuirk) {
  registerMockedHttpURLLoad("viewport-zero-values.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(true, nullptr, &client, nullptr,
                           enableViewportSettings);
  webViewHelper.webView()->settings()->setViewportMetaZeroValuesQuirk(true);
  webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
  webViewHelper.webView()->settings()->setViewportMetaLayoutSizeQuirk(true);
  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              m_baseURL + "viewport-zero-values.html");
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_EQ(viewportWidth, webViewHelper.webView()
                               ->mainFrameImpl()
                               ->frameView()
                               ->layoutSize()
                               .width());
  EXPECT_EQ(1.0f, webViewHelper.webView()->pageScaleFactor());

  webViewHelper.webView()->settings()->setUseWideViewport(true);
  webViewHelper.webView()->updateAllLifecyclePhases();
  EXPECT_EQ(viewportWidth, webViewHelper.webView()
                               ->mainFrameImpl()
                               ->frameView()
                               ->layoutSize()
                               .width());
  EXPECT_EQ(1.0f, webViewHelper.webView()->pageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, OverflowHiddenDisablesScrolling) {
  registerMockedHttpURLLoad("body-overflow-hidden.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(true, nullptr, &client, nullptr);
  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              m_baseURL + "body-overflow-hidden.html");
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  FrameView* view = webViewHelper.webView()->mainFrameImpl()->frameView();
  EXPECT_FALSE(view->userInputScrollable(VerticalScrollbar));
  EXPECT_FALSE(view->userInputScrollable(HorizontalScrollbar));
}

TEST_P(ParameterizedWebFrameTest,
       OverflowHiddenDisablesScrollingWithSetCanHaveScrollbars) {
  registerMockedHttpURLLoad("body-overflow-hidden-short.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(true, nullptr, &client, nullptr);
  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              m_baseURL + "body-overflow-hidden-short.html");
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  FrameView* view = webViewHelper.webView()->mainFrameImpl()->frameView();
  EXPECT_FALSE(view->userInputScrollable(VerticalScrollbar));
  EXPECT_FALSE(view->userInputScrollable(HorizontalScrollbar));

  webViewHelper.webView()->mainFrameImpl()->setCanHaveScrollbars(true);
  EXPECT_FALSE(view->userInputScrollable(VerticalScrollbar));
  EXPECT_FALSE(view->userInputScrollable(HorizontalScrollbar));
}

TEST_F(WebFrameTest, IgnoreOverflowHiddenQuirk) {
  registerMockedHttpURLLoad("body-overflow-hidden.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(true, nullptr, &client, nullptr);
  webViewHelper.webView()->settings()->setIgnoreMainFrameOverflowHiddenQuirk(
      true);
  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              m_baseURL + "body-overflow-hidden.html");
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  FrameView* view = webViewHelper.webView()->mainFrameImpl()->frameView();
  EXPECT_TRUE(view->userInputScrollable(VerticalScrollbar));
}

TEST_P(ParameterizedWebFrameTest, NonZeroValuesNoQuirk) {
  registerMockedHttpURLLoad("viewport-nonzero-values.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;
  float expectedPageScaleFactor = 0.5f;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(true, nullptr, &client, nullptr,
                           enableViewportSettings);
  webViewHelper.webView()->settings()->setViewportMetaZeroValuesQuirk(true);
  webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              m_baseURL + "viewport-nonzero-values.html");
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_EQ(viewportWidth / expectedPageScaleFactor, webViewHelper.webView()
                                                         ->mainFrameImpl()
                                                         ->frameView()
                                                         ->layoutSize()
                                                         .width());
  EXPECT_EQ(expectedPageScaleFactor,
            webViewHelper.webView()->pageScaleFactor());

  webViewHelper.webView()->settings()->setUseWideViewport(true);
  webViewHelper.webView()->updateAllLifecyclePhases();
  EXPECT_EQ(viewportWidth / expectedPageScaleFactor, webViewHelper.webView()
                                                         ->mainFrameImpl()
                                                         ->frameView()
                                                         ->layoutSize()
                                                         .width());
  EXPECT_EQ(expectedPageScaleFactor,
            webViewHelper.webView()->pageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, setPageScaleFactorDoesNotLayout) {
  registerMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  // Small viewport to ensure there are always scrollbars.
  int viewportWidth = 64;
  int viewportHeight = 48;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "fixed_layout.html", true,
                                  nullptr, &client, nullptr,
                                  enableViewportSettings);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  int prevLayoutCount =
      webViewHelper.webView()->mainFrameImpl()->frameView()->layoutCount();
  webViewHelper.webView()->setPageScaleFactor(3);
  EXPECT_FALSE(
      webViewHelper.webView()->mainFrameImpl()->frameView()->needsLayout());
  EXPECT_EQ(
      prevLayoutCount,
      webViewHelper.webView()->mainFrameImpl()->frameView()->layoutCount());
}

TEST_P(ParameterizedWebFrameTest,
       setPageScaleFactorWithOverlayScrollbarsDoesNotLayout) {
  registerMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "fixed_layout.html", true,
                                  nullptr, &client, nullptr,
                                  enableViewportSettings);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  int prevLayoutCount =
      webViewHelper.webView()->mainFrameImpl()->frameView()->layoutCount();
  webViewHelper.webView()->setPageScaleFactor(30);
  EXPECT_FALSE(
      webViewHelper.webView()->mainFrameImpl()->frameView()->needsLayout());
  EXPECT_EQ(
      prevLayoutCount,
      webViewHelper.webView()->mainFrameImpl()->frameView()->layoutCount());
}

TEST_P(ParameterizedWebFrameTest, pageScaleFactorWrittenToHistoryItem) {
  registerMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "fixed_layout.html", true,
                                  nullptr, &client, nullptr,
                                  enableViewportSettings);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  webViewHelper.webView()->setPageScaleFactor(3);
  EXPECT_EQ(3, toLocalFrame(webViewHelper.webView()->page()->mainFrame())
                   ->loader()
                   .currentItem()
                   ->pageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, initialScaleWrittenToHistoryItem) {
  registerMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(true, nullptr, &client, nullptr,
                           enableViewportSettings);
  webViewHelper.webView()->setDefaultPageScaleLimits(0.25f, 5);
  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              m_baseURL + "fixed_layout.html");
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  int defaultFixedLayoutWidth = 980;
  float minimumPageScaleFactor = viewportWidth / (float)defaultFixedLayoutWidth;
  EXPECT_EQ(minimumPageScaleFactor,
            toLocalFrame(webViewHelper.webView()->page()->mainFrame())
                ->loader()
                .currentItem()
                ->pageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, pageScaleFactorDoesntShrinkFrameView) {
  registerMockedHttpURLLoad("large-div.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  // Small viewport to ensure there are always scrollbars.
  int viewportWidth = 64;
  int viewportHeight = 48;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "large-div.html", true, nullptr,
                                  &client, nullptr, enableViewportSettings);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  FrameView* view = webViewHelper.webView()->mainFrameImpl()->frameView();
  int viewportWidthMinusScrollbar = viewportWidth;
  int viewportHeightMinusScrollbar = viewportHeight;

  if (view->verticalScrollbar() &&
      !view->verticalScrollbar()->isOverlayScrollbar())
    viewportWidthMinusScrollbar -= 15;

  if (view->horizontalScrollbar() &&
      !view->horizontalScrollbar()->isOverlayScrollbar())
    viewportHeightMinusScrollbar -= 15;

  webViewHelper.webView()->setPageScaleFactor(2);

  IntSize unscaledSize = view->visibleContentSize(IncludeScrollbars);
  EXPECT_EQ(viewportWidth, unscaledSize.width());
  EXPECT_EQ(viewportHeight, unscaledSize.height());

  IntSize unscaledSizeMinusScrollbar =
      view->visibleContentSize(ExcludeScrollbars);
  EXPECT_EQ(viewportWidthMinusScrollbar, unscaledSizeMinusScrollbar.width());
  EXPECT_EQ(viewportHeightMinusScrollbar, unscaledSizeMinusScrollbar.height());

  IntSize frameViewSize = view->visibleContentRect().size();
  EXPECT_EQ(viewportWidthMinusScrollbar, frameViewSize.width());
  EXPECT_EQ(viewportHeightMinusScrollbar, frameViewSize.height());
}

TEST_P(ParameterizedWebFrameTest, pageScaleFactorDoesNotApplyCssTransform) {
  registerMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "fixed_layout.html", true,
                                  nullptr, &client, nullptr,
                                  enableViewportSettings);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  webViewHelper.webView()->setPageScaleFactor(2);

  EXPECT_EQ(980, toLocalFrame(webViewHelper.webView()->page()->mainFrame())
                     ->contentLayoutItem()
                     .documentRect()
                     .width());
  EXPECT_EQ(980, webViewHelper.webView()
                     ->mainFrameImpl()
                     ->frameView()
                     ->contentsSize()
                     .width());
}

TEST_P(ParameterizedWebFrameTest, targetDensityDpiHigh) {
  registerMockedHttpURLLoad("viewport-target-densitydpi-high.html");

  FixedLayoutTestWebViewClient client;
  // high-dpi = 240
  float targetDpi = 240.0f;
  float deviceScaleFactors[] = {1.0f, 4.0f / 3.0f, 2.0f};
  int viewportWidth = 640;
  int viewportHeight = 480;

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(deviceScaleFactors); ++i) {
    float deviceScaleFactor = deviceScaleFactors[i];
    float deviceDpi = deviceScaleFactor * 160.0f;
    client.m_screenInfo.deviceScaleFactor = deviceScaleFactor;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(
        m_baseURL + "viewport-target-densitydpi-high.html", true, nullptr,
        &client, nullptr, enableViewportSettings);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    webViewHelper.webView()->settings()->setSupportDeprecatedTargetDensityDPI(
        true);
    webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

    // We need to account for the fact that logical pixels are unconditionally
    // multiplied by deviceScaleFactor to produce physical pixels.
    float densityDpiScaleRatio = deviceScaleFactor * targetDpi / deviceDpi;
    EXPECT_NEAR(viewportWidth * densityDpiScaleRatio, webViewHelper.webView()
                                                          ->mainFrameImpl()
                                                          ->frameView()
                                                          ->layoutSize()
                                                          .width(),
                1.0f);
    EXPECT_NEAR(viewportHeight * densityDpiScaleRatio, webViewHelper.webView()
                                                           ->mainFrameImpl()
                                                           ->frameView()
                                                           ->layoutSize()
                                                           .height(),
                1.0f);
    EXPECT_NEAR(1.0f / densityDpiScaleRatio,
                webViewHelper.webView()->pageScaleFactor(), 0.01f);
  }
}

TEST_P(ParameterizedWebFrameTest, targetDensityDpiDevice) {
  registerMockedHttpURLLoad("viewport-target-densitydpi-device.html");

  float deviceScaleFactors[] = {1.0f, 4.0f / 3.0f, 2.0f};

  FixedLayoutTestWebViewClient client;
  int viewportWidth = 640;
  int viewportHeight = 480;

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(deviceScaleFactors); ++i) {
    client.m_screenInfo.deviceScaleFactor = deviceScaleFactors[i];

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(
        m_baseURL + "viewport-target-densitydpi-device.html", true, nullptr,
        &client, nullptr, enableViewportSettings);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    webViewHelper.webView()->settings()->setSupportDeprecatedTargetDensityDPI(
        true);
    webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

    EXPECT_NEAR(viewportWidth * client.m_screenInfo.deviceScaleFactor,
                webViewHelper.webView()
                    ->mainFrameImpl()
                    ->frameView()
                    ->layoutSize()
                    .width(),
                1.0f);
    EXPECT_NEAR(viewportHeight * client.m_screenInfo.deviceScaleFactor,
                webViewHelper.webView()
                    ->mainFrameImpl()
                    ->frameView()
                    ->layoutSize()
                    .height(),
                1.0f);
    EXPECT_NEAR(1.0f / client.m_screenInfo.deviceScaleFactor,
                webViewHelper.webView()->pageScaleFactor(), 0.01f);
  }
}

TEST_P(ParameterizedWebFrameTest, targetDensityDpiDeviceAndFixedWidth) {
  registerMockedHttpURLLoad(
      "viewport-target-densitydpi-device-and-fixed-width.html");

  float deviceScaleFactors[] = {1.0f, 4.0f / 3.0f, 2.0f};

  FixedLayoutTestWebViewClient client;
  int viewportWidth = 640;
  int viewportHeight = 480;

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(deviceScaleFactors); ++i) {
    client.m_screenInfo.deviceScaleFactor = deviceScaleFactors[i];

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(
        m_baseURL + "viewport-target-densitydpi-device-and-fixed-width.html",
        true, nullptr, &client, nullptr, enableViewportSettings);
    webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
    webViewHelper.webView()->settings()->setSupportDeprecatedTargetDensityDPI(
        true);
    webViewHelper.webView()->settings()->setUseWideViewport(true);
    webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

    EXPECT_NEAR(viewportWidth, webViewHelper.webView()
                                   ->mainFrameImpl()
                                   ->frameView()
                                   ->layoutSize()
                                   .width(),
                1.0f);
    EXPECT_NEAR(viewportHeight, webViewHelper.webView()
                                    ->mainFrameImpl()
                                    ->frameView()
                                    ->layoutSize()
                                    .height(),
                1.0f);
    EXPECT_NEAR(1.0f, webViewHelper.webView()->pageScaleFactor(), 0.01f);
  }
}

TEST_P(ParameterizedWebFrameTest, NoWideViewportAndScaleLessThanOne) {
  registerMockedHttpURLLoad("viewport-initial-scale-less-than-1.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1.33f;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(
      m_baseURL + "viewport-initial-scale-less-than-1.html", true, nullptr,
      &client, nullptr, enableViewportSettings);
  webViewHelper.webView()->settings()->setSupportDeprecatedTargetDensityDPI(
      true);
  webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
  webViewHelper.webView()->settings()->setUseWideViewport(false);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_NEAR(viewportWidth * client.m_screenInfo.deviceScaleFactor,
              webViewHelper.webView()
                  ->mainFrameImpl()
                  ->frameView()
                  ->layoutSize()
                  .width(),
              1.0f);
  EXPECT_NEAR(viewportHeight * client.m_screenInfo.deviceScaleFactor,
              webViewHelper.webView()
                  ->mainFrameImpl()
                  ->frameView()
                  ->layoutSize()
                  .height(),
              1.0f);
  EXPECT_NEAR(1.0f / client.m_screenInfo.deviceScaleFactor,
              webViewHelper.webView()->pageScaleFactor(), 0.01f);
}

TEST_P(ParameterizedWebFrameTest,
       NoWideViewportAndScaleLessThanOneWithDeviceWidth) {
  registerMockedHttpURLLoad(
      "viewport-initial-scale-less-than-1-device-width.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1.33f;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(
      m_baseURL + "viewport-initial-scale-less-than-1-device-width.html", true,
      nullptr, &client, nullptr, enableViewportSettings);
  webViewHelper.webView()->settings()->setSupportDeprecatedTargetDensityDPI(
      true);
  webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
  webViewHelper.webView()->settings()->setUseWideViewport(false);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  const float pageZoom = 0.25f;
  EXPECT_NEAR(viewportWidth * client.m_screenInfo.deviceScaleFactor / pageZoom,
              webViewHelper.webView()
                  ->mainFrameImpl()
                  ->frameView()
                  ->layoutSize()
                  .width(),
              1.0f);
  EXPECT_NEAR(viewportHeight * client.m_screenInfo.deviceScaleFactor / pageZoom,
              webViewHelper.webView()
                  ->mainFrameImpl()
                  ->frameView()
                  ->layoutSize()
                  .height(),
              1.0f);
  EXPECT_NEAR(1.0f / client.m_screenInfo.deviceScaleFactor,
              webViewHelper.webView()->pageScaleFactor(), 0.01f);
}

TEST_P(ParameterizedWebFrameTest,
       NoWideViewportAndNoViewportWithInitialPageScaleOverride) {
  registerMockedHttpURLLoad("large-div.html");

  FixedLayoutTestWebViewClient client;
  int viewportWidth = 640;
  int viewportHeight = 480;
  float enforcedPageScaleFactor = 5.0f;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "large-div.html", true, nullptr,
                                  &client, nullptr, enableViewportSettings);
  webViewHelper.webView()->setDefaultPageScaleLimits(0.25f, 5);
  webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
  webViewHelper.webView()->settings()->setUseWideViewport(false);
  webViewHelper.webView()->setInitialPageScaleOverride(enforcedPageScaleFactor);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_NEAR(viewportWidth / enforcedPageScaleFactor, webViewHelper.webView()
                                                           ->mainFrameImpl()
                                                           ->frameView()
                                                           ->layoutSize()
                                                           .width(),
              1.0f);
  EXPECT_NEAR(viewportHeight / enforcedPageScaleFactor, webViewHelper.webView()
                                                            ->mainFrameImpl()
                                                            ->frameView()
                                                            ->layoutSize()
                                                            .height(),
              1.0f);
  EXPECT_NEAR(enforcedPageScaleFactor,
              webViewHelper.webView()->pageScaleFactor(), 0.01f);
}

TEST_P(ParameterizedWebFrameTest, NoUserScalableQuirkIgnoresViewportScale) {
  registerMockedHttpURLLoad("viewport-initial-scale-and-user-scalable-no.html");

  FixedLayoutTestWebViewClient client;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(
      m_baseURL + "viewport-initial-scale-and-user-scalable-no.html", true,
      nullptr, &client, nullptr, enableViewportSettings);
  webViewHelper.webView()->settings()->setViewportMetaNonUserScalableQuirk(
      true);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_NEAR(viewportWidth, webViewHelper.webView()
                                 ->mainFrameImpl()
                                 ->frameView()
                                 ->layoutSize()
                                 .width(),
              1.0f);
  EXPECT_NEAR(viewportHeight, webViewHelper.webView()
                                  ->mainFrameImpl()
                                  ->frameView()
                                  ->layoutSize()
                                  .height(),
              1.0f);
  EXPECT_NEAR(1.0f, webViewHelper.webView()->pageScaleFactor(), 0.01f);
}

TEST_P(ParameterizedWebFrameTest,
       NoUserScalableQuirkIgnoresViewportScaleForNonWideViewport) {
  registerMockedHttpURLLoad("viewport-initial-scale-and-user-scalable-no.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1.33f;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(
      m_baseURL + "viewport-initial-scale-and-user-scalable-no.html", true,
      nullptr, &client, nullptr, enableViewportSettings);
  webViewHelper.webView()->settings()->setSupportDeprecatedTargetDensityDPI(
      true);
  webViewHelper.webView()->settings()->setViewportMetaNonUserScalableQuirk(
      true);
  webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
  webViewHelper.webView()->settings()->setUseWideViewport(false);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_NEAR(viewportWidth * client.m_screenInfo.deviceScaleFactor,
              webViewHelper.webView()
                  ->mainFrameImpl()
                  ->frameView()
                  ->layoutSize()
                  .width(),
              1.0f);
  EXPECT_NEAR(viewportHeight * client.m_screenInfo.deviceScaleFactor,
              webViewHelper.webView()
                  ->mainFrameImpl()
                  ->frameView()
                  ->layoutSize()
                  .height(),
              1.0f);
  EXPECT_NEAR(1.0f / client.m_screenInfo.deviceScaleFactor,
              webViewHelper.webView()->pageScaleFactor(), 0.01f);
}

TEST_P(ParameterizedWebFrameTest,
       NoUserScalableQuirkIgnoresViewportScaleForWideViewport) {
  registerMockedHttpURLLoad("viewport-2x-initial-scale-non-user-scalable.html");

  FixedLayoutTestWebViewClient client;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(
      m_baseURL + "viewport-2x-initial-scale-non-user-scalable.html", true,
      nullptr, &client, nullptr, enableViewportSettings);
  webViewHelper.webView()->settings()->setViewportMetaNonUserScalableQuirk(
      true);
  webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
  webViewHelper.webView()->settings()->setUseWideViewport(true);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_NEAR(viewportWidth, webViewHelper.webView()
                                 ->mainFrameImpl()
                                 ->frameView()
                                 ->layoutSize()
                                 .width(),
              1.0f);
  EXPECT_NEAR(viewportHeight, webViewHelper.webView()
                                  ->mainFrameImpl()
                                  ->frameView()
                                  ->layoutSize()
                                  .height(),
              1.0f);
  EXPECT_NEAR(1.0f, webViewHelper.webView()->pageScaleFactor(), 0.01f);
}

TEST_P(ParameterizedWebFrameTest,
       DesktopPageCanBeZoomedInWhenWideViewportIsTurnedOff) {
  registerMockedHttpURLLoad("no_viewport_tag.html");

  FixedLayoutTestWebViewClient client;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "no_viewport_tag.html", true,
                                  nullptr, &client, nullptr,
                                  enableViewportSettings);
  webViewHelper.webView()->setDefaultPageScaleLimits(0.25f, 5);
  webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
  webViewHelper.webView()->settings()->setUseWideViewport(false);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_NEAR(1.0f, webViewHelper.webView()->pageScaleFactor(), 0.01f);
  EXPECT_NEAR(1.0f, webViewHelper.webView()->minimumPageScaleFactor(), 0.01f);
  EXPECT_NEAR(5.0f, webViewHelper.webView()->maximumPageScaleFactor(), 0.01f);
}

TEST_P(ParameterizedWebFrameTest, AtViewportInsideAtMediaInitialViewport) {
  registerMockedHttpURLLoad("viewport-inside-media.html");

  FixedLayoutTestWebViewClient client;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "viewport-inside-media.html",
                                  true, nullptr, &client, nullptr,
                                  enableViewportSettings);
  webViewHelper.resize(WebSize(640, 480));

  EXPECT_EQ(2000, webViewHelper.webView()
                      ->mainFrameImpl()
                      ->frameView()
                      ->layoutSize()
                      .width());

  webViewHelper.resize(WebSize(1200, 480));

  EXPECT_EQ(1200, webViewHelper.webView()
                      ->mainFrameImpl()
                      ->frameView()
                      ->layoutSize()
                      .width());
}

TEST_P(ParameterizedWebFrameTest, AtViewportAffectingAtMediaRecalcCount) {
  registerMockedHttpURLLoad("viewport-and-media.html");

  FixedLayoutTestWebViewClient client;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(true, nullptr, &client, nullptr,
                           enableViewportSettings);
  webViewHelper.resize(WebSize(640, 480));
  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              m_baseURL + "viewport-and-media.html");

  Document* document =
      webViewHelper.webView()->mainFrameImpl()->frame()->document();
  EXPECT_EQ(2000, webViewHelper.webView()
                      ->mainFrameImpl()
                      ->frameView()
                      ->layoutSize()
                      .width());

  // The styleForElementCount() should match the number of elements for a single
  // pass of computed styles construction for the document.
  EXPECT_EQ(10u, document->styleEngine().styleForElementCount());
  EXPECT_EQ(Color(0, 128, 0),
            document->body()->computedStyle()->visitedDependentColor(
                CSSPropertyColor));
}

TEST_P(ParameterizedWebFrameTest, AtViewportWithViewportLengths) {
  registerMockedHttpURLLoad("viewport-lengths.html");

  FixedLayoutTestWebViewClient client;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(true, nullptr, &client, nullptr,
                           enableViewportSettings);
  webViewHelper.resize(WebSize(800, 600));
  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              m_baseURL + "viewport-lengths.html");

  FrameView* view = webViewHelper.webView()->mainFrameImpl()->frameView();
  EXPECT_EQ(400, view->layoutSize().width());
  EXPECT_EQ(300, view->layoutSize().height());

  webViewHelper.resize(WebSize(1000, 400));

  EXPECT_EQ(500, view->layoutSize().width());
  EXPECT_EQ(200, view->layoutSize().height());
}

class WebFrameResizeTest : public ParameterizedWebFrameTest {
 protected:
  static FloatSize computeRelativeOffset(const IntPoint& absoluteOffset,
                                         const LayoutRect& rect) {
    FloatSize relativeOffset =
        FloatPoint(absoluteOffset) - FloatPoint(rect.location());
    relativeOffset.scale(1.f / rect.width(), 1.f / rect.height());
    return relativeOffset;
  }

  void testResizeYieldsCorrectScrollAndScale(
      const char* url,
      const float initialPageScaleFactor,
      const WebSize scrollOffset,
      const WebSize viewportSize,
      const bool shouldScaleRelativeToViewportWidth) {
    registerMockedHttpURLLoad(url);

    const float aspectRatio =
        static_cast<float>(viewportSize.width) / viewportSize.height;

    FrameTestHelpers::WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(m_baseURL + url, true, nullptr, nullptr,
                                    nullptr, enableViewportSettings);
    webViewHelper.webView()->setDefaultPageScaleLimits(0.25f, 5);

    // Origin scrollOffsets preserved under resize.
    {
      webViewHelper.resize(WebSize(viewportSize.width, viewportSize.height));
      webViewHelper.webView()->setPageScaleFactor(initialPageScaleFactor);
      ASSERT_EQ(viewportSize, webViewHelper.webView()->size());
      ASSERT_EQ(initialPageScaleFactor,
                webViewHelper.webView()->pageScaleFactor());
      webViewHelper.resize(WebSize(viewportSize.height, viewportSize.width));
      float expectedPageScaleFactor =
          initialPageScaleFactor *
          (shouldScaleRelativeToViewportWidth ? 1 / aspectRatio : 1);
      EXPECT_NEAR(expectedPageScaleFactor,
                  webViewHelper.webView()->pageScaleFactor(), 0.05f);
      EXPECT_EQ(WebSize(),
                webViewHelper.webView()->mainFrame()->scrollOffset());
    }

    // Resizing just the height should not affect pageScaleFactor or
    // scrollOffset.
    {
      webViewHelper.resize(WebSize(viewportSize.width, viewportSize.height));
      webViewHelper.webView()->setPageScaleFactor(initialPageScaleFactor);
      webViewHelper.webView()->mainFrame()->setScrollOffset(scrollOffset);
      webViewHelper.webView()->updateAllLifecyclePhases();
      const WebSize expectedScrollOffset =
          webViewHelper.webView()->mainFrame()->scrollOffset();
      webViewHelper.resize(
          WebSize(viewportSize.width, viewportSize.height * 0.8f));
      EXPECT_EQ(initialPageScaleFactor,
                webViewHelper.webView()->pageScaleFactor());
      EXPECT_EQ(expectedScrollOffset,
                webViewHelper.webView()->mainFrame()->scrollOffset());
      webViewHelper.resize(
          WebSize(viewportSize.width, viewportSize.height * 0.8f));
      EXPECT_EQ(initialPageScaleFactor,
                webViewHelper.webView()->pageScaleFactor());
      EXPECT_EQ(expectedScrollOffset,
                webViewHelper.webView()->mainFrame()->scrollOffset());
    }
  }
};

INSTANTIATE_TEST_CASE_P(All, WebFrameResizeTest, ::testing::Bool());

TEST_P(WebFrameResizeTest,
       ResizeYieldsCorrectScrollAndScaleForWidthEqualsDeviceWidth) {
  // With width=device-width, pageScaleFactor is preserved across resizes as
  // long as the content adjusts according to the device-width.
  const char* url = "resize_scroll_mobile.html";
  const float initialPageScaleFactor = 1;
  const WebSize scrollOffset(0, 50);
  const WebSize viewportSize(120, 160);
  const bool shouldScaleRelativeToViewportWidth = true;

  testResizeYieldsCorrectScrollAndScale(url, initialPageScaleFactor,
                                        scrollOffset, viewportSize,
                                        shouldScaleRelativeToViewportWidth);
}

TEST_P(WebFrameResizeTest, ResizeYieldsCorrectScrollAndScaleForMinimumScale) {
  // This tests a scenario where minimum-scale is set to 1.0, but some element
  // on the page is slightly larger than the portrait width, so our "natural"
  // minimum-scale would be lower. In that case, we should stick to 1.0 scale
  // on rotation and not do anything strange.
  const char* url = "resize_scroll_minimum_scale.html";
  const float initialPageScaleFactor = 1;
  const WebSize scrollOffset(0, 0);
  const WebSize viewportSize(240, 320);
  const bool shouldScaleRelativeToViewportWidth = false;

  testResizeYieldsCorrectScrollAndScale(url, initialPageScaleFactor,
                                        scrollOffset, viewportSize,
                                        shouldScaleRelativeToViewportWidth);
}

TEST_P(WebFrameResizeTest, ResizeYieldsCorrectScrollAndScaleForFixedWidth) {
  // With a fixed width, pageScaleFactor scales by the relative change in
  // viewport width.
  const char* url = "resize_scroll_fixed_width.html";
  const float initialPageScaleFactor = 2;
  const WebSize scrollOffset(0, 200);
  const WebSize viewportSize(240, 320);
  const bool shouldScaleRelativeToViewportWidth = true;

  testResizeYieldsCorrectScrollAndScale(url, initialPageScaleFactor,
                                        scrollOffset, viewportSize,
                                        shouldScaleRelativeToViewportWidth);
}

TEST_P(WebFrameResizeTest, ResizeYieldsCorrectScrollAndScaleForFixedLayout) {
  // With a fixed layout, pageScaleFactor scales by the relative change in
  // viewport width.
  const char* url = "resize_scroll_fixed_layout.html";
  const float initialPageScaleFactor = 2;
  const WebSize scrollOffset(200, 400);
  const WebSize viewportSize(320, 240);
  const bool shouldScaleRelativeToViewportWidth = true;

  testResizeYieldsCorrectScrollAndScale(url, initialPageScaleFactor,
                                        scrollOffset, viewportSize,
                                        shouldScaleRelativeToViewportWidth);
}

TEST_P(ParameterizedWebFrameTest, pageScaleFactorUpdatesScrollbars) {
  registerMockedHttpURLLoad("fixed_layout.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "fixed_layout.html", true,
                                  nullptr, &client, nullptr,
                                  enableViewportSettings);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  FrameView* view = webViewHelper.webView()->mainFrameImpl()->frameView();
  EXPECT_EQ(view->scrollSize(HorizontalScrollbar),
            view->contentsSize().width() - view->visibleContentRect().width());
  EXPECT_EQ(
      view->scrollSize(VerticalScrollbar),
      view->contentsSize().height() - view->visibleContentRect().height());

  webViewHelper.webView()->setPageScaleFactor(10);

  EXPECT_EQ(view->scrollSize(HorizontalScrollbar),
            view->contentsSize().width() - view->visibleContentRect().width());
  EXPECT_EQ(
      view->scrollSize(VerticalScrollbar),
      view->contentsSize().height() - view->visibleContentRect().height());
}

TEST_P(ParameterizedWebFrameTest, CanOverrideScaleLimits) {
  registerMockedHttpURLLoad("no_scale_for_you.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "no_scale_for_you.html", true,
                                  nullptr, &client, nullptr,
                                  enableViewportSettings);
  webViewHelper.webView()->setDefaultPageScaleLimits(0.25f, 5);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  EXPECT_EQ(2.0f, webViewHelper.webView()->minimumPageScaleFactor());
  EXPECT_EQ(2.0f, webViewHelper.webView()->maximumPageScaleFactor());

  webViewHelper.webView()->setIgnoreViewportTagScaleLimits(true);
  webViewHelper.webView()->updateAllLifecyclePhases();

  EXPECT_EQ(1.0f, webViewHelper.webView()->minimumPageScaleFactor());
  EXPECT_EQ(5.0f, webViewHelper.webView()->maximumPageScaleFactor());

  webViewHelper.webView()->setIgnoreViewportTagScaleLimits(false);
  webViewHelper.webView()->updateAllLifecyclePhases();

  EXPECT_EQ(2.0f, webViewHelper.webView()->minimumPageScaleFactor());
  EXPECT_EQ(2.0f, webViewHelper.webView()->maximumPageScaleFactor());
}

// Android doesn't have scrollbars on the main FrameView
#if OS(ANDROID)
TEST_F(WebFrameTest, DISABLED_updateOverlayScrollbarLayers)
#else
TEST_F(WebFrameTest, updateOverlayScrollbarLayers)
#endif
{
  registerMockedHttpURLLoad("large-div.html");

  int viewWidth = 500;
  int viewHeight = 500;

  std::unique_ptr<FakeCompositingWebViewClient> fakeCompositingWebViewClient =
      makeUnique<FakeCompositingWebViewClient>();
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(true, nullptr, fakeCompositingWebViewClient.get(),
                           nullptr, &configureCompositingWebView);

  webViewHelper.resize(WebSize(viewWidth, viewHeight));
  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              m_baseURL + "large-div.html");

  FrameView* view = webViewHelper.webView()->mainFrameImpl()->frameView();
  EXPECT_TRUE(
      view->layoutViewItem().compositor()->layerForHorizontalScrollbar());
  EXPECT_TRUE(view->layoutViewItem().compositor()->layerForVerticalScrollbar());

  webViewHelper.resize(WebSize(viewWidth * 10, viewHeight * 10));
  EXPECT_FALSE(
      view->layoutViewItem().compositor()->layerForHorizontalScrollbar());
  EXPECT_FALSE(
      view->layoutViewItem().compositor()->layerForVerticalScrollbar());
}

void setScaleAndScrollAndLayout(WebViewImpl* webView,
                                WebPoint scroll,
                                float scale) {
  webView->setPageScaleFactor(scale);
  webView->mainFrame()->setScrollOffset(WebSize(scroll.x, scroll.y));
  webView->updateAllLifecyclePhases();
}

void simulatePageScale(WebViewImpl* webViewImpl, float& scale) {
  ScrollOffset scrollDelta =
      toScrollOffset(
          webViewImpl->fakePageScaleAnimationTargetPositionForTesting()) -
      webViewImpl->mainFrameImpl()->frameView()->scrollOffset();
  float scaleDelta = webViewImpl->fakePageScaleAnimationPageScaleForTesting() /
                     webViewImpl->pageScaleFactor();
  webViewImpl->applyViewportDeltas(WebFloatSize(), FloatSize(scrollDelta),
                                   WebFloatSize(), scaleDelta, 0);
  scale = webViewImpl->pageScaleFactor();
}

void simulateMultiTargetZoom(WebViewImpl* webViewImpl,
                             const WebRect& rect,
                             float& scale) {
  if (webViewImpl->zoomToMultipleTargetsRect(rect))
    simulatePageScale(webViewImpl, scale);
}

void simulateDoubleTap(WebViewImpl* webViewImpl,
                       WebPoint& point,
                       float& scale) {
  webViewImpl->animateDoubleTapZoom(point);
  EXPECT_TRUE(webViewImpl->fakeDoubleTapAnimationPendingForTesting());
  simulatePageScale(webViewImpl, scale);
}

TEST_P(ParameterizedWebFrameTest, DivAutoZoomParamsTest) {
  registerMockedHttpURLLoad("get_scale_for_auto_zoom_into_div_test.html");

  const float deviceScaleFactor = 2.0f;
  int viewportWidth = 640 / deviceScaleFactor;
  int viewportHeight = 1280 / deviceScaleFactor;
  float doubleTapZoomAlreadyLegibleRatio = 1.2f;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(
      m_baseURL + "get_scale_for_auto_zoom_into_div_test.html", false, nullptr,
      nullptr, nullptr, configureAndroid);
  webViewHelper.webView()->setDeviceScaleFactor(deviceScaleFactor);
  webViewHelper.webView()->setDefaultPageScaleLimits(0.01f, 4);
  webViewHelper.webView()->setPageScaleFactor(0.5f);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  WebRect wideDiv(200, 100, 400, 150);
  WebRect tallDiv(200, 300, 400, 800);
  WebPoint doubleTapPointWide(wideDiv.x + 50, wideDiv.y + 50);
  WebPoint doubleTapPointTall(tallDiv.x + 50, tallDiv.y + 50);
  float scale;
  WebPoint scroll;

  float doubleTapZoomAlreadyLegibleScale =
      webViewHelper.webView()->minimumPageScaleFactor() *
      doubleTapZoomAlreadyLegibleRatio;

  // Test double-tap zooming into wide div.
  WebRect wideBlockBound =
      webViewHelper.webView()->computeBlockBound(doubleTapPointWide, false);
  webViewHelper.webView()->computeScaleAndScrollForBlockRect(
      WebPoint(doubleTapPointWide.x, doubleTapPointWide.y), wideBlockBound,
      touchPointPadding, doubleTapZoomAlreadyLegibleScale, scale, scroll);
  // The div should horizontally fill the screen (modulo margins), and
  // vertically centered (modulo integer rounding).
  EXPECT_NEAR(viewportWidth / (float)wideDiv.width, scale, 0.1);
  EXPECT_NEAR(wideDiv.x, scroll.x, 20);
  EXPECT_EQ(0, scroll.y);

  setScaleAndScrollAndLayout(webViewHelper.webView(), scroll, scale);

  // Test zoom out back to minimum scale.
  wideBlockBound =
      webViewHelper.webView()->computeBlockBound(doubleTapPointWide, false);
  webViewHelper.webView()->computeScaleAndScrollForBlockRect(
      WebPoint(doubleTapPointWide.x, doubleTapPointWide.y), wideBlockBound,
      touchPointPadding, doubleTapZoomAlreadyLegibleScale, scale, scroll);
  // FIXME: Looks like we are missing EXPECTs here.

  scale = webViewHelper.webView()->minimumPageScaleFactor();
  setScaleAndScrollAndLayout(webViewHelper.webView(), WebPoint(0, 0), scale);

  // Test double-tap zooming into tall div.
  WebRect tallBlockBound =
      webViewHelper.webView()->computeBlockBound(doubleTapPointTall, false);
  webViewHelper.webView()->computeScaleAndScrollForBlockRect(
      WebPoint(doubleTapPointTall.x, doubleTapPointTall.y), tallBlockBound,
      touchPointPadding, doubleTapZoomAlreadyLegibleScale, scale, scroll);
  // The div should start at the top left of the viewport.
  EXPECT_NEAR(viewportWidth / (float)tallDiv.width, scale, 0.1);
  EXPECT_NEAR(tallDiv.x, scroll.x, 20);
  EXPECT_NEAR(tallDiv.y, scroll.y, 20);
}

TEST_P(ParameterizedWebFrameTest, DivAutoZoomWideDivTest) {
  registerMockedHttpURLLoad("get_wide_div_for_auto_zoom_test.html");

  const float deviceScaleFactor = 2.0f;
  int viewportWidth = 640 / deviceScaleFactor;
  int viewportHeight = 1280 / deviceScaleFactor;
  float doubleTapZoomAlreadyLegibleRatio = 1.2f;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(
      m_baseURL + "get_wide_div_for_auto_zoom_test.html", false, nullptr,
      nullptr, nullptr, configureAndroid);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));
  webViewHelper.webView()->setDeviceScaleFactor(deviceScaleFactor);
  webViewHelper.webView()->setPageScaleFactor(1.0f);
  webViewHelper.webView()->updateAllLifecyclePhases();

  webViewHelper.webView()->enableFakePageScaleAnimationForTesting(true);

  float doubleTapZoomAlreadyLegibleScale =
      webViewHelper.webView()->minimumPageScaleFactor() *
      doubleTapZoomAlreadyLegibleRatio;

  WebRect div(0, 100, viewportWidth, 150);
  WebPoint point(div.x + 50, div.y + 50);
  float scale;
  setScaleAndScrollAndLayout(
      webViewHelper.webView(), WebPoint(0, 0),
      (webViewHelper.webView()->minimumPageScaleFactor()) *
          (1 + doubleTapZoomAlreadyLegibleRatio) / 2);

  simulateDoubleTap(webViewHelper.webView(), point, scale);
  EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
  simulateDoubleTap(webViewHelper.webView(), point, scale);
  EXPECT_FLOAT_EQ(webViewHelper.webView()->minimumPageScaleFactor(), scale);
}

TEST_P(ParameterizedWebFrameTest, DivAutoZoomVeryTallTest) {
  // When a block is taller than the viewport and a zoom targets a lower part
  // of it, then we should keep the target point onscreen instead of snapping
  // back up the top of the block.
  registerMockedHttpURLLoad("very_tall_div.html");

  const float deviceScaleFactor = 2.0f;
  int viewportWidth = 640 / deviceScaleFactor;
  int viewportHeight = 1280 / deviceScaleFactor;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "very_tall_div.html", true,
                                  nullptr, nullptr, nullptr, configureAndroid);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));
  webViewHelper.webView()->setDeviceScaleFactor(deviceScaleFactor);
  webViewHelper.webView()->setPageScaleFactor(1.0f);
  webViewHelper.webView()->updateAllLifecyclePhases();

  WebRect div(200, 300, 400, 5000);
  WebPoint point(div.x + 50, div.y + 3000);
  float scale;
  WebPoint scroll;

  WebRect blockBound = webViewHelper.webView()->computeBlockBound(point, true);
  webViewHelper.webView()->computeScaleAndScrollForBlockRect(
      point, blockBound, 0, 1.0f, scale, scroll);
  EXPECT_EQ(scale, 1.0f);
  EXPECT_EQ(scroll.y, 2660);
}

TEST_F(WebFrameTest, DivAutoZoomMultipleDivsTest) {
  registerMockedHttpURLLoad("get_multiple_divs_for_auto_zoom_test.html");

  const float deviceScaleFactor = 2.0f;
  int viewportWidth = 640 / deviceScaleFactor;
  int viewportHeight = 1280 / deviceScaleFactor;
  float doubleTapZoomAlreadyLegibleRatio = 1.2f;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(
      m_baseURL + "get_multiple_divs_for_auto_zoom_test.html", false, nullptr,
      nullptr, nullptr, configureAndroid);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));
  webViewHelper.webView()->setDefaultPageScaleLimits(0.5f, 4);
  webViewHelper.webView()->setDeviceScaleFactor(deviceScaleFactor);
  webViewHelper.webView()->setPageScaleFactor(0.5f);
  webViewHelper.webView()->setMaximumLegibleScale(1.f);
  webViewHelper.webView()->updateAllLifecyclePhases();

  webViewHelper.webView()->enableFakePageScaleAnimationForTesting(true);

  WebRect topDiv(200, 100, 200, 150);
  WebRect bottomDiv(200, 300, 200, 150);
  WebPoint topPoint(topDiv.x + 50, topDiv.y + 50);
  WebPoint bottomPoint(bottomDiv.x + 50, bottomDiv.y + 50);
  float scale;
  setScaleAndScrollAndLayout(
      webViewHelper.webView(), WebPoint(0, 0),
      (webViewHelper.webView()->minimumPageScaleFactor()) *
          (1 + doubleTapZoomAlreadyLegibleRatio) / 2);

  // Test double tap on two different divs.  After first zoom, we should go back
  // to minimum page scale with a second double tap.
  simulateDoubleTap(webViewHelper.webView(), topPoint, scale);
  EXPECT_FLOAT_EQ(1, scale);
  simulateDoubleTap(webViewHelper.webView(), bottomPoint, scale);
  EXPECT_FLOAT_EQ(webViewHelper.webView()->minimumPageScaleFactor(), scale);

  // If the user pinch zooms after double tap, a second double tap should zoom
  // back to the div.
  simulateDoubleTap(webViewHelper.webView(), topPoint, scale);
  EXPECT_FLOAT_EQ(1, scale);
  webViewHelper.webView()->applyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                               WebFloatSize(), 0.6f, 0);
  simulateDoubleTap(webViewHelper.webView(), bottomPoint, scale);
  EXPECT_FLOAT_EQ(1, scale);
  simulateDoubleTap(webViewHelper.webView(), bottomPoint, scale);
  EXPECT_FLOAT_EQ(webViewHelper.webView()->minimumPageScaleFactor(), scale);

  // If we didn't yet get an auto-zoom update and a second double-tap arrives,
  // should go back to minimum scale.
  webViewHelper.webView()->applyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                               WebFloatSize(), 1.1f, 0);
  webViewHelper.webView()->animateDoubleTapZoom(topPoint);
  EXPECT_TRUE(
      webViewHelper.webView()->fakeDoubleTapAnimationPendingForTesting());
  simulateDoubleTap(webViewHelper.webView(), bottomPoint, scale);
  EXPECT_FLOAT_EQ(webViewHelper.webView()->minimumPageScaleFactor(), scale);
}

TEST_F(WebFrameTest, DivAutoZoomScaleBoundsTest) {
  registerMockedHttpURLLoad("get_scale_bounds_check_for_auto_zoom_test.html");

  int viewportWidth = 320;
  int viewportHeight = 480;
  float doubleTapZoomAlreadyLegibleRatio = 1.2f;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(
      m_baseURL + "get_scale_bounds_check_for_auto_zoom_test.html", false,
      nullptr, nullptr, nullptr, configureAndroid);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));
  webViewHelper.webView()->setDeviceScaleFactor(1.5f);
  webViewHelper.webView()->setMaximumLegibleScale(1.f);
  webViewHelper.webView()->updateAllLifecyclePhases();

  webViewHelper.webView()->enableFakePageScaleAnimationForTesting(true);

  WebRect div(200, 100, 200, 150);
  WebPoint doubleTapPoint(div.x + 50, div.y + 50);
  float scale;

  // Test double tap scale bounds.
  // minimumPageScale < doubleTapZoomAlreadyLegibleScale < 1
  webViewHelper.webView()->setDefaultPageScaleLimits(0.5f, 4);
  webViewHelper.webView()->updateAllLifecyclePhases();
  float doubleTapZoomAlreadyLegibleScale =
      webViewHelper.webView()->minimumPageScaleFactor() *
      doubleTapZoomAlreadyLegibleRatio;
  setScaleAndScrollAndLayout(
      webViewHelper.webView(), WebPoint(0, 0),
      (webViewHelper.webView()->minimumPageScaleFactor()) *
          (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(1, scale);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(webViewHelper.webView()->minimumPageScaleFactor(), scale);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(1, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  webViewHelper.webView()->applyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                               WebFloatSize(), 1.1f, 0);
  // 1 < minimumPageScale < doubleTapZoomAlreadyLegibleScale
  webViewHelper.webView()->setDefaultPageScaleLimits(1.1f, 4);
  webViewHelper.webView()->updateAllLifecyclePhases();
  doubleTapZoomAlreadyLegibleScale =
      webViewHelper.webView()->minimumPageScaleFactor() *
      doubleTapZoomAlreadyLegibleRatio;
  setScaleAndScrollAndLayout(
      webViewHelper.webView(), WebPoint(0, 0),
      (webViewHelper.webView()->minimumPageScaleFactor()) *
          (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(webViewHelper.webView()->minimumPageScaleFactor(), scale);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  webViewHelper.webView()->applyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                               WebFloatSize(), 1.1f, 0);
  // minimumPageScale < 1 < doubleTapZoomAlreadyLegibleScale
  webViewHelper.webView()->setDefaultPageScaleLimits(0.95f, 4);
  webViewHelper.webView()->updateAllLifecyclePhases();
  doubleTapZoomAlreadyLegibleScale =
      webViewHelper.webView()->minimumPageScaleFactor() *
      doubleTapZoomAlreadyLegibleRatio;
  setScaleAndScrollAndLayout(
      webViewHelper.webView(), WebPoint(0, 0),
      (webViewHelper.webView()->minimumPageScaleFactor()) *
          (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(webViewHelper.webView()->minimumPageScaleFactor(), scale);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
}

TEST_F(WebFrameTest, DivAutoZoomScaleLegibleScaleTest) {
  registerMockedHttpURLLoad("get_scale_bounds_check_for_auto_zoom_test.html");

  int viewportWidth = 320;
  int viewportHeight = 480;
  float doubleTapZoomAlreadyLegibleRatio = 1.2f;
  float maximumLegibleScaleFactor = 1.13f;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(
      m_baseURL + "get_scale_bounds_check_for_auto_zoom_test.html", false,
      nullptr, nullptr, nullptr, configureAndroid);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));
  webViewHelper.webView()->setMaximumLegibleScale(maximumLegibleScaleFactor);
  webViewHelper.webView()->updateAllLifecyclePhases();

  webViewHelper.webView()->enableFakePageScaleAnimationForTesting(true);
  webViewHelper.webView()->page()->settings().setTextAutosizingEnabled(true);

  WebRect div(200, 100, 200, 150);
  WebPoint doubleTapPoint(div.x + 50, div.y + 50);
  float scale;

  // Test double tap scale bounds.
  // minimumPageScale < doubleTapZoomAlreadyLegibleScale < 1 <
  //     maximumLegibleScaleFactor
  float legibleScale = maximumLegibleScaleFactor;
  setScaleAndScrollAndLayout(
      webViewHelper.webView(), WebPoint(0, 0),
      (webViewHelper.webView()->minimumPageScaleFactor()) *
          (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
  float doubleTapZoomAlreadyLegibleScale =
      webViewHelper.webView()->minimumPageScaleFactor() *
      doubleTapZoomAlreadyLegibleRatio;
  webViewHelper.webView()->setDefaultPageScaleLimits(0.5f, 4);
  webViewHelper.webView()->updateAllLifecyclePhases();
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(legibleScale, scale);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(webViewHelper.webView()->minimumPageScaleFactor(), scale);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(legibleScale, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  webViewHelper.webView()->applyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                               WebFloatSize(), 1.1f, 0);
  // 1 < maximumLegibleScaleFactor < minimumPageScale <
  //     doubleTapZoomAlreadyLegibleScale
  webViewHelper.webView()->setDefaultPageScaleLimits(1.0f, 4);
  webViewHelper.webView()->updateAllLifecyclePhases();
  doubleTapZoomAlreadyLegibleScale =
      webViewHelper.webView()->minimumPageScaleFactor() *
      doubleTapZoomAlreadyLegibleRatio;
  setScaleAndScrollAndLayout(
      webViewHelper.webView(), WebPoint(0, 0),
      (webViewHelper.webView()->minimumPageScaleFactor()) *
          (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(webViewHelper.webView()->minimumPageScaleFactor(), scale);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  webViewHelper.webView()->applyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                               WebFloatSize(), 1.1f, 0);
  // minimumPageScale < 1 < maximumLegibleScaleFactor <
  //     doubleTapZoomAlreadyLegibleScale
  webViewHelper.webView()->setDefaultPageScaleLimits(0.95f, 4);
  webViewHelper.webView()->updateAllLifecyclePhases();
  doubleTapZoomAlreadyLegibleScale =
      webViewHelper.webView()->minimumPageScaleFactor() *
      doubleTapZoomAlreadyLegibleRatio;
  setScaleAndScrollAndLayout(
      webViewHelper.webView(), WebPoint(0, 0),
      (webViewHelper.webView()->minimumPageScaleFactor()) *
          (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(webViewHelper.webView()->minimumPageScaleFactor(), scale);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  webViewHelper.webView()->applyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                               WebFloatSize(), 1.1f, 0);
  // minimumPageScale < 1 < doubleTapZoomAlreadyLegibleScale <
  //     maximumLegibleScaleFactor
  webViewHelper.webView()->setDefaultPageScaleLimits(0.9f, 4);
  webViewHelper.webView()->updateAllLifecyclePhases();
  doubleTapZoomAlreadyLegibleScale =
      webViewHelper.webView()->minimumPageScaleFactor() *
      doubleTapZoomAlreadyLegibleRatio;
  setScaleAndScrollAndLayout(
      webViewHelper.webView(), WebPoint(0, 0),
      (webViewHelper.webView()->minimumPageScaleFactor()) *
          (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(legibleScale, scale);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(webViewHelper.webView()->minimumPageScaleFactor(), scale);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(legibleScale, scale);
}

TEST_F(WebFrameTest, DivAutoZoomScaleFontScaleFactorTest) {
  registerMockedHttpURLLoad("get_scale_bounds_check_for_auto_zoom_test.html");

  int viewportWidth = 320;
  int viewportHeight = 480;
  float doubleTapZoomAlreadyLegibleRatio = 1.2f;
  float accessibilityFontScaleFactor = 1.13f;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(
      m_baseURL + "get_scale_bounds_check_for_auto_zoom_test.html", false,
      nullptr, nullptr, nullptr, configureAndroid);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));
  webViewHelper.webView()->setMaximumLegibleScale(1.f);
  webViewHelper.webView()->updateAllLifecyclePhases();

  webViewHelper.webView()->enableFakePageScaleAnimationForTesting(true);
  webViewHelper.webView()->page()->settings().setTextAutosizingEnabled(true);
  webViewHelper.webView()->page()->settings().setAccessibilityFontScaleFactor(
      accessibilityFontScaleFactor);

  WebRect div(200, 100, 200, 150);
  WebPoint doubleTapPoint(div.x + 50, div.y + 50);
  float scale;

  // Test double tap scale bounds.
  // minimumPageScale < doubleTapZoomAlreadyLegibleScale < 1 <
  //     accessibilityFontScaleFactor
  float legibleScale = accessibilityFontScaleFactor;
  setScaleAndScrollAndLayout(
      webViewHelper.webView(), WebPoint(0, 0),
      (webViewHelper.webView()->minimumPageScaleFactor()) *
          (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
  float doubleTapZoomAlreadyLegibleScale =
      webViewHelper.webView()->minimumPageScaleFactor() *
      doubleTapZoomAlreadyLegibleRatio;
  webViewHelper.webView()->setDefaultPageScaleLimits(0.5f, 4);
  webViewHelper.webView()->updateAllLifecyclePhases();
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(legibleScale, scale);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(webViewHelper.webView()->minimumPageScaleFactor(), scale);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(legibleScale, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  webViewHelper.webView()->applyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                               WebFloatSize(), 1.1f, 0);
  // 1 < accessibilityFontScaleFactor < minimumPageScale <
  //     doubleTapZoomAlreadyLegibleScale
  webViewHelper.webView()->setDefaultPageScaleLimits(1.0f, 4);
  webViewHelper.webView()->updateAllLifecyclePhases();
  doubleTapZoomAlreadyLegibleScale =
      webViewHelper.webView()->minimumPageScaleFactor() *
      doubleTapZoomAlreadyLegibleRatio;
  setScaleAndScrollAndLayout(
      webViewHelper.webView(), WebPoint(0, 0),
      (webViewHelper.webView()->minimumPageScaleFactor()) *
          (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(webViewHelper.webView()->minimumPageScaleFactor(), scale);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  webViewHelper.webView()->applyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                               WebFloatSize(), 1.1f, 0);
  // minimumPageScale < 1 < accessibilityFontScaleFactor <
  //     doubleTapZoomAlreadyLegibleScale
  webViewHelper.webView()->setDefaultPageScaleLimits(0.95f, 4);
  webViewHelper.webView()->updateAllLifecyclePhases();
  doubleTapZoomAlreadyLegibleScale =
      webViewHelper.webView()->minimumPageScaleFactor() *
      doubleTapZoomAlreadyLegibleRatio;
  setScaleAndScrollAndLayout(
      webViewHelper.webView(), WebPoint(0, 0),
      (webViewHelper.webView()->minimumPageScaleFactor()) *
          (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(webViewHelper.webView()->minimumPageScaleFactor(), scale);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);

  // Zoom in to reset double_tap_zoom_in_effect flag.
  webViewHelper.webView()->applyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                               WebFloatSize(), 1.1f, 0);
  // minimumPageScale < 1 < doubleTapZoomAlreadyLegibleScale <
  //     accessibilityFontScaleFactor
  webViewHelper.webView()->setDefaultPageScaleLimits(0.9f, 4);
  webViewHelper.webView()->updateAllLifecyclePhases();
  doubleTapZoomAlreadyLegibleScale =
      webViewHelper.webView()->minimumPageScaleFactor() *
      doubleTapZoomAlreadyLegibleRatio;
  setScaleAndScrollAndLayout(
      webViewHelper.webView(), WebPoint(0, 0),
      (webViewHelper.webView()->minimumPageScaleFactor()) *
          (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(legibleScale, scale);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(webViewHelper.webView()->minimumPageScaleFactor(), scale);
  simulateDoubleTap(webViewHelper.webView(), doubleTapPoint, scale);
  EXPECT_FLOAT_EQ(legibleScale, scale);
}

TEST_P(ParameterizedWebFrameTest, BlockBoundTest) {
  registerMockedHttpURLLoad("block_bound.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "block_bound.html", false,
                                  nullptr, nullptr, nullptr, configureAndroid);

  IntRect rectBack = IntRect(0, 0, 200, 200);
  IntRect rectLeftTop = IntRect(10, 10, 80, 80);
  IntRect rectRightBottom = IntRect(110, 110, 80, 80);
  IntRect blockBound;

  blockBound =
      IntRect(webViewHelper.webView()->computeBlockBound(WebPoint(9, 9), true));
  EXPECT_RECT_EQ(rectBack, blockBound);

  blockBound = IntRect(
      webViewHelper.webView()->computeBlockBound(WebPoint(10, 10), true));
  EXPECT_RECT_EQ(rectLeftTop, blockBound);

  blockBound = IntRect(
      webViewHelper.webView()->computeBlockBound(WebPoint(50, 50), true));
  EXPECT_RECT_EQ(rectLeftTop, blockBound);

  blockBound = IntRect(
      webViewHelper.webView()->computeBlockBound(WebPoint(89, 89), true));
  EXPECT_RECT_EQ(rectLeftTop, blockBound);

  blockBound = IntRect(
      webViewHelper.webView()->computeBlockBound(WebPoint(90, 90), true));
  EXPECT_RECT_EQ(rectBack, blockBound);

  blockBound = IntRect(
      webViewHelper.webView()->computeBlockBound(WebPoint(109, 109), true));
  EXPECT_RECT_EQ(rectBack, blockBound);

  blockBound = IntRect(
      webViewHelper.webView()->computeBlockBound(WebPoint(110, 110), true));
  EXPECT_RECT_EQ(rectRightBottom, blockBound);
}

TEST_P(ParameterizedWebFrameTest, DivMultipleTargetZoomMultipleDivsTest) {
  registerMockedHttpURLLoad("get_multiple_divs_for_auto_zoom_test.html");

  const float deviceScaleFactor = 2.0f;
  int viewportWidth = 640 / deviceScaleFactor;
  int viewportHeight = 1280 / deviceScaleFactor;
  float doubleTapZoomAlreadyLegibleRatio = 1.2f;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL +
                                  "get_multiple_divs_for_auto_zoom_test.html");
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));
  webViewHelper.webView()->setDefaultPageScaleLimits(0.5f, 4);
  webViewHelper.webView()->setDeviceScaleFactor(deviceScaleFactor);
  webViewHelper.webView()->setPageScaleFactor(0.5f);
  webViewHelper.webView()->setMaximumLegibleScale(1.f);
  webViewHelper.webView()->updateAllLifecyclePhases();

  webViewHelper.webView()->enableFakePageScaleAnimationForTesting(true);

  WebRect viewportRect(0, 0, viewportWidth, viewportHeight);
  WebRect topDiv(200, 100, 200, 150);
  WebRect bottomDiv(200, 300, 200, 150);
  float scale;
  setScaleAndScrollAndLayout(
      webViewHelper.webView(), WebPoint(0, 0),
      (webViewHelper.webView()->minimumPageScaleFactor()) *
          (1 + doubleTapZoomAlreadyLegibleRatio) / 2);

  simulateMultiTargetZoom(webViewHelper.webView(), topDiv, scale);
  EXPECT_FLOAT_EQ(1, scale);
  simulateMultiTargetZoom(webViewHelper.webView(), bottomDiv, scale);
  EXPECT_FLOAT_EQ(1, scale);
  simulateMultiTargetZoom(webViewHelper.webView(), viewportRect, scale);
  EXPECT_FLOAT_EQ(1, scale);
  webViewHelper.webView()->setPageScaleFactor(
      webViewHelper.webView()->minimumPageScaleFactor());
  simulateMultiTargetZoom(webViewHelper.webView(), topDiv, scale);
  EXPECT_FLOAT_EQ(1, scale);
}

TEST_F(WebFrameTest, DontZoomInOnFocusedInTouchAction) {
  registerMockedHttpURLLoad("textbox_in_touch_action.html");

  int viewportWidth = 600;
  int viewportHeight = 1000;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "textbox_in_touch_action.html");
  webViewHelper.webView()->setDefaultPageScaleLimits(0.25f, 4);
  webViewHelper.webView()->enableFakePageScaleAnimationForTesting(true);
  webViewHelper.webView()->page()->settings().setTextAutosizingEnabled(false);
  webViewHelper.webView()->settings()->setAutoZoomFocusedNodeToLegibleScale(
      true);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  float initialScale = webViewHelper.webView()->pageScaleFactor();

  // Focus the first textbox that's in a touch-action: pan-x ancestor, this
  // shouldn't cause an autozoom since pan-x disables pinch-zoom.
  webViewHelper.webView()->advanceFocus(false);
  webViewHelper.webView()->scrollFocusedEditableElementIntoRect(WebRect());
  EXPECT_EQ(
      webViewHelper.webView()->fakePageScaleAnimationPageScaleForTesting(), 0);

  setScaleAndScrollAndLayout(webViewHelper.webView(), WebPoint(0, 0),
                             initialScale);
  ASSERT_EQ(initialScale, webViewHelper.webView()->pageScaleFactor());

  // Focus the second textbox that's in a touch-action: manipulation ancestor,
  // this should cause an autozoom since it allows pinch-zoom.
  webViewHelper.webView()->advanceFocus(false);
  webViewHelper.webView()->scrollFocusedEditableElementIntoRect(WebRect());
  EXPECT_GT(
      webViewHelper.webView()->fakePageScaleAnimationPageScaleForTesting(),
      initialScale);

  setScaleAndScrollAndLayout(webViewHelper.webView(), WebPoint(0, 0),
                             initialScale);
  ASSERT_EQ(initialScale, webViewHelper.webView()->pageScaleFactor());

  // Focus the third textbox that has a touch-action: pan-x ancestor, this
  // should cause an autozoom since it's seperated from the node with the
  // touch-action by an overflow:scroll element.
  webViewHelper.webView()->advanceFocus(false);
  webViewHelper.webView()->scrollFocusedEditableElementIntoRect(WebRect());
  EXPECT_GT(
      webViewHelper.webView()->fakePageScaleAnimationPageScaleForTesting(),
      initialScale);
}

TEST_F(WebFrameTest, DivScrollIntoEditableTest) {
  registerMockedHttpURLLoad("get_scale_for_zoom_into_editable_test.html");

  const bool autoZoomToLegibleScale = true;
  int viewportWidth = 450;
  int viewportHeight = 300;
  float leftBoxRatio = 0.3f;
  int caretPadding = 10;
  float minReadableCaretHeight = 16.0f;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL +
                                  "get_scale_for_zoom_into_editable_test.html");
  webViewHelper.webView()->page()->settings().setTextAutosizingEnabled(false);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));
  webViewHelper.webView()->setDefaultPageScaleLimits(0.25f, 4);

  webViewHelper.webView()->enableFakePageScaleAnimationForTesting(true);

  WebRect editBoxWithText(200, 200, 250, 20);
  WebRect editBoxWithNoText(200, 250, 250, 20);

  // Test scrolling the focused node
  // The edit box is shorter and narrower than the viewport when legible.
  webViewHelper.webView()->advanceFocus(false);
  // Set the caret to the end of the input box.
  webViewHelper.webView()
      ->mainFrame()
      ->document()
      .getElementById("EditBoxWithText")
      .to<WebInputElement>()
      .setSelectionRange(1000, 1000);
  setScaleAndScrollAndLayout(webViewHelper.webView(), WebPoint(0, 0), 1);
  WebRect rect, caret;
  webViewHelper.webView()->selectionBounds(caret, rect);

  // Set the page scale to be smaller than the minimal readable scale.
  float initialScale = minReadableCaretHeight / caret.height * 0.5f;
  setScaleAndScrollAndLayout(webViewHelper.webView(), WebPoint(0, 0),
                             initialScale);

  float scale;
  IntPoint scroll;
  bool needAnimation;
  webViewHelper.webView()->computeScaleAndScrollForFocusedNode(
      webViewHelper.webView()->focusedElement(), autoZoomToLegibleScale, scale,
      scroll, needAnimation);
  EXPECT_TRUE(needAnimation);
  // The edit box should be left aligned with a margin for possible label.
  int hScroll = editBoxWithText.x - leftBoxRatio * viewportWidth / scale;
  EXPECT_NEAR(hScroll, scroll.x(), 2);
  int vScroll =
      editBoxWithText.y - (viewportHeight / scale - editBoxWithText.height) / 2;
  EXPECT_NEAR(vScroll, scroll.y(), 2);
  EXPECT_NEAR(minReadableCaretHeight / caret.height, scale, 0.1);

  // The edit box is wider than the viewport when legible.
  viewportWidth = 200;
  viewportHeight = 150;
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));
  setScaleAndScrollAndLayout(webViewHelper.webView(), WebPoint(0, 0),
                             initialScale);
  webViewHelper.webView()->computeScaleAndScrollForFocusedNode(
      webViewHelper.webView()->focusedElement(), autoZoomToLegibleScale, scale,
      scroll, needAnimation);
  EXPECT_TRUE(needAnimation);
  // The caret should be right aligned since the caret would be offscreen when
  // the edit box is left aligned.
  hScroll = caret.x + caret.width + caretPadding - viewportWidth / scale;
  EXPECT_NEAR(hScroll, scroll.x(), 2);
  EXPECT_NEAR(minReadableCaretHeight / caret.height, scale, 0.1);

  setScaleAndScrollAndLayout(webViewHelper.webView(), WebPoint(0, 0),
                             initialScale);
  // Move focus to edit box with text.
  webViewHelper.webView()->advanceFocus(false);
  webViewHelper.webView()->computeScaleAndScrollForFocusedNode(
      webViewHelper.webView()->focusedElement(), autoZoomToLegibleScale, scale,
      scroll, needAnimation);
  EXPECT_TRUE(needAnimation);
  // The edit box should be left aligned.
  hScroll = editBoxWithNoText.x;
  EXPECT_NEAR(hScroll, scroll.x(), 2);
  vScroll = editBoxWithNoText.y -
            (viewportHeight / scale - editBoxWithNoText.height) / 2;
  EXPECT_NEAR(vScroll, scroll.y(), 2);
  EXPECT_NEAR(minReadableCaretHeight / caret.height, scale, 0.1);

  // Move focus back to the first edit box.
  webViewHelper.webView()->advanceFocus(true);
  // Zoom out slightly.
  const float withinToleranceScale = scale * 0.9f;
  setScaleAndScrollAndLayout(webViewHelper.webView(), scroll,
                             withinToleranceScale);
  // Move focus back to the second edit box.
  webViewHelper.webView()->advanceFocus(false);
  webViewHelper.webView()->computeScaleAndScrollForFocusedNode(
      webViewHelper.webView()->focusedElement(), autoZoomToLegibleScale, scale,
      scroll, needAnimation);
  // The scale should not be adjusted as the zoomed out scale was sufficiently
  // close to the previously focused scale.
  EXPECT_FALSE(needAnimation);
}

TEST_F(WebFrameTest, DivScrollIntoEditablePreservePageScaleTest) {
  registerMockedHttpURLLoad("get_scale_for_zoom_into_editable_test.html");

  const bool autoZoomToLegibleScale = true;
  const int viewportWidth = 450;
  const int viewportHeight = 300;
  const float minReadableCaretHeight = 16.0f;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL +
                                  "get_scale_for_zoom_into_editable_test.html");
  webViewHelper.webView()->page()->settings().setTextAutosizingEnabled(false);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));
  webViewHelper.webView()->enableFakePageScaleAnimationForTesting(true);

  const WebRect editBoxWithText(200, 200, 250, 20);

  webViewHelper.webView()->advanceFocus(false);
  // Set the caret to the begining of the input box.
  webViewHelper.webView()
      ->mainFrame()
      ->document()
      .getElementById("EditBoxWithText")
      .to<WebInputElement>()
      .setSelectionRange(0, 0);
  setScaleAndScrollAndLayout(webViewHelper.webView(), WebPoint(0, 0), 1);
  WebRect rect, caret;
  webViewHelper.webView()->selectionBounds(caret, rect);

  // Set the page scale to be twice as large as the minimal readable scale.
  float newScale = minReadableCaretHeight / caret.height * 2.0;
  setScaleAndScrollAndLayout(webViewHelper.webView(), WebPoint(0, 0), newScale);

  float scale;
  IntPoint scroll;
  bool needAnimation;
  webViewHelper.webView()->computeScaleAndScrollForFocusedNode(
      webViewHelper.webView()->focusedElement(), autoZoomToLegibleScale, scale,
      scroll, needAnimation);
  EXPECT_TRUE(needAnimation);
  // Edit box and caret should be left alinged
  int hScroll = editBoxWithText.x;
  EXPECT_NEAR(hScroll, scroll.x(), 1);
  int vScroll =
      editBoxWithText.y - (viewportHeight / scale - editBoxWithText.height) / 2;
  EXPECT_NEAR(vScroll, scroll.y(), 1);
  // Page scale have to be unchanged
  EXPECT_EQ(newScale, scale);

  // Set page scale and scroll such that edit box will be under the screen
  newScale = 3.0;
  hScroll = 200;
  setScaleAndScrollAndLayout(webViewHelper.webView(), WebPoint(hScroll, 0),
                             newScale);
  webViewHelper.webView()->computeScaleAndScrollForFocusedNode(
      webViewHelper.webView()->focusedElement(), autoZoomToLegibleScale, scale,
      scroll, needAnimation);
  EXPECT_TRUE(needAnimation);
  // Horizontal scroll have to be the same
  EXPECT_NEAR(hScroll, scroll.x(), 1);
  vScroll =
      editBoxWithText.y - (viewportHeight / scale - editBoxWithText.height) / 2;
  EXPECT_NEAR(vScroll, scroll.y(), 1);
  // Page scale have to be unchanged
  EXPECT_EQ(newScale, scale);
}

// Tests the scroll into view functionality when
// autoZoomeFocusedNodeToLegibleScale set to false. i.e. The path non-Android
// platforms take.
TEST_F(WebFrameTest, DivScrollIntoEditableTestZoomToLegibleScaleDisabled) {
  registerMockedHttpURLLoad("get_scale_for_zoom_into_editable_test.html");

  const bool autoZoomToLegibleScale = false;
  int viewportWidth = 100;
  int viewportHeight = 100;
  float leftBoxRatio = 0.3f;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL +
                                  "get_scale_for_zoom_into_editable_test.html");
  webViewHelper.webView()->page()->settings().setTextAutosizingEnabled(false);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));
  webViewHelper.webView()->setDefaultPageScaleLimits(0.25f, 4);

  webViewHelper.webView()->enableFakePageScaleAnimationForTesting(true);

  WebRect editBoxWithText(200, 200, 250, 20);
  WebRect editBoxWithNoText(200, 250, 250, 20);

  // Test scrolling the focused node
  // Since we're zoomed out, the caret is considered too small to be legible and
  // so we'd normally zoom in. Make sure we don't change scale since the
  // auto-zoom setting is off.

  // Focus the second empty textbox.
  webViewHelper.webView()->advanceFocus(false);
  webViewHelper.webView()->advanceFocus(false);

  // Set the page scale to be smaller than the minimal readable scale.
  float initialScale = 0.25f;
  setScaleAndScrollAndLayout(webViewHelper.webView(), WebPoint(0, 0),
                             initialScale);

  float scale;
  IntPoint scroll;
  bool needAnimation;
  webViewHelper.webView()->computeScaleAndScrollForFocusedNode(
      webViewHelper.webView()->focusedElement(), autoZoomToLegibleScale, scale,
      scroll, needAnimation);

  // There should be no change in page scale.
  EXPECT_EQ(initialScale, scale);
  // The edit box should be left aligned with a margin for possible label.
  EXPECT_TRUE(needAnimation);
  int hScroll = editBoxWithNoText.x - leftBoxRatio * viewportWidth / scale;
  EXPECT_NEAR(hScroll, scroll.x(), 2);
  int vScroll = editBoxWithNoText.y -
                (viewportHeight / scale - editBoxWithNoText.height) / 2;
  EXPECT_NEAR(vScroll, scroll.y(), 2);

  setScaleAndScrollAndLayout(webViewHelper.webView(), scroll, scale);

  // Select the first textbox.
  webViewHelper.webView()->advanceFocus(true);
  WebRect rect, caret;
  webViewHelper.webView()->selectionBounds(caret, rect);
  webViewHelper.webView()->computeScaleAndScrollForFocusedNode(
      webViewHelper.webView()->focusedElement(), autoZoomToLegibleScale, scale,
      scroll, needAnimation);

  // There should be no change at all since the textbox is fully visible
  // already.
  EXPECT_EQ(initialScale, scale);
  EXPECT_FALSE(needAnimation);
}

TEST_P(ParameterizedWebFrameTest, CharacterIndexAtPointWithPinchZoom) {
  registerMockedHttpURLLoad("sometext.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "sometext.html");
  webViewHelper.resize(WebSize(640, 480));

  webViewHelper.webView()->setPageScaleFactor(2);
  webViewHelper.webView()->setVisualViewportOffset(WebFloatPoint(50, 60));

  WebRect baseRect;
  WebRect extentRect;

  WebLocalFrame* mainFrame =
      webViewHelper.webView()->mainFrame()->toWebLocalFrame();
  size_t ix = mainFrame->characterIndexForPoint(WebPoint(320, 388));

  EXPECT_EQ(2ul, ix);
}

TEST_P(ParameterizedWebFrameTest, FirstRectForCharacterRangeWithPinchZoom) {
  registerMockedHttpURLLoad("textbox.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "textbox.html", true);
  webViewHelper.resize(WebSize(640, 480));

  WebLocalFrame* mainFrame =
      webViewHelper.webView()->mainFrame()->toWebLocalFrame();
  mainFrame->executeScript(WebScriptSource("selectRange();"));

  WebRect oldRect;
  mainFrame->firstRectForCharacterRange(0, 5, oldRect);

  WebFloatPoint visualOffset(100, 130);
  float scale = 2;
  webViewHelper.webView()->setPageScaleFactor(scale);
  webViewHelper.webView()->setVisualViewportOffset(visualOffset);

  WebRect baseRect;
  WebRect extentRect;

  WebRect rect;
  mainFrame->firstRectForCharacterRange(0, 5, rect);

  EXPECT_EQ((oldRect.x - visualOffset.x) * scale, rect.x);
  EXPECT_EQ((oldRect.y - visualOffset.y) * scale, rect.y);
  EXPECT_EQ(oldRect.width * scale, rect.width);
  EXPECT_EQ(oldRect.height * scale, rect.height);
}
class TestReloadDoesntRedirectWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  WebNavigationPolicy decidePolicyForNavigation(
      const NavigationPolicyInfo& info) override {
    EXPECT_FALSE(info.extraData);
    return WebNavigationPolicyCurrentTab;
  }
};

TEST_P(ParameterizedWebFrameTest, ReloadDoesntSetRedirect) {
  // Test for case in http://crbug.com/73104. Reloading a frame very quickly
  // would sometimes call decidePolicyForNavigation with isRedirect=true
  registerMockedHttpURLLoad("form.html");

  TestReloadDoesntRedirectWebFrameClient webFrameClient;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "form.html", false,
                                  &webFrameClient);

  webViewHelper.webView()->mainFrame()->reload(
      WebFrameLoadType::ReloadBypassingCache);
  // start another reload before request is delivered.
  FrameTestHelpers::reloadFrameIgnoringCache(
      webViewHelper.webView()->mainFrame());
}

class ClearScrollStateOnCommitWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  void didCommitProvisionalLoad(WebLocalFrame* frame,
                                const WebHistoryItem&,
                                WebHistoryCommitType) override {
    frame->view()->resetScrollAndScaleState();
  }
};

TEST_F(WebFrameTest, ReloadWithOverrideURLPreservesState) {
  const std::string firstURL = "200-by-300.html";
  const std::string secondURL = "content-width-1000.html";
  const std::string thirdURL = "very_tall_div.html";
  const float pageScaleFactor = 1.1684f;
  const int pageWidth = 120;
  const int pageHeight = 100;

  registerMockedHttpURLLoad(firstURL);
  registerMockedHttpURLLoad(secondURL);
  registerMockedHttpURLLoad(thirdURL);

  FrameTestHelpers::WebViewHelper webViewHelper;
  ClearScrollStateOnCommitWebFrameClient client;
  webViewHelper.initializeAndLoad(m_baseURL + firstURL, true, &client);
  webViewHelper.resize(WebSize(pageWidth, pageHeight));
  webViewHelper.webView()->mainFrame()->setScrollOffset(
      WebSize(pageWidth / 4, pageHeight / 4));
  webViewHelper.webView()->setPageScaleFactor(pageScaleFactor);

  WebSize previousOffset = webViewHelper.webView()->mainFrame()->scrollOffset();
  float previousScale = webViewHelper.webView()->pageScaleFactor();

  // Reload the page and end up at the same url. State should be propagated.
  webViewHelper.webView()->mainFrame()->reloadWithOverrideURL(
      toKURL(m_baseURL + firstURL), WebFrameLoadType::Reload);
  FrameTestHelpers::pumpPendingRequestsForFrameToLoad(
      webViewHelper.webView()->mainFrame());
  EXPECT_EQ(previousOffset.width,
            webViewHelper.webView()->mainFrame()->scrollOffset().width);
  EXPECT_EQ(previousOffset.height,
            webViewHelper.webView()->mainFrame()->scrollOffset().height);
  EXPECT_EQ(previousScale, webViewHelper.webView()->pageScaleFactor());

  // Reload the page using the cache. State should not be propagated.
  webViewHelper.webView()->mainFrame()->reloadWithOverrideURL(
      toKURL(m_baseURL + secondURL), WebFrameLoadType::Reload);
  FrameTestHelpers::pumpPendingRequestsForFrameToLoad(
      webViewHelper.webView()->mainFrame());
  EXPECT_EQ(0, webViewHelper.webView()->mainFrame()->scrollOffset().width);
  EXPECT_EQ(0, webViewHelper.webView()->mainFrame()->scrollOffset().height);
  EXPECT_EQ(1.0f, webViewHelper.webView()->pageScaleFactor());

  // Reload the page while ignoring the cache. State should not be propagated.
  webViewHelper.webView()->mainFrame()->reloadWithOverrideURL(
      toKURL(m_baseURL + thirdURL), WebFrameLoadType::ReloadBypassingCache);
  FrameTestHelpers::pumpPendingRequestsForFrameToLoad(
      webViewHelper.webView()->mainFrame());
  EXPECT_EQ(0, webViewHelper.webView()->mainFrame()->scrollOffset().width);
  EXPECT_EQ(0, webViewHelper.webView()->mainFrame()->scrollOffset().height);
  EXPECT_EQ(1.0f, webViewHelper.webView()->pageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, ReloadWhileProvisional) {
  // Test that reloading while the previous load is still pending does not cause
  // the initial request to get lost.
  registerMockedHttpURLLoad("fixed_layout.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize();
  WebURLRequest request(toKURL(m_baseURL + "fixed_layout.html"));
  webViewHelper.webView()->mainFrame()->loadRequest(request);
  // start reload before first request is delivered.
  FrameTestHelpers::reloadFrameIgnoringCache(
      webViewHelper.webView()->mainFrame());

  WebDataSource* dataSource =
      webViewHelper.webView()->mainFrame()->dataSource();
  ASSERT_TRUE(dataSource);
  EXPECT_EQ(toKURL(m_baseURL + "fixed_layout.html"),
            KURL(dataSource->request().url()));
}

TEST_P(ParameterizedWebFrameTest, AppendRedirects) {
  const std::string firstURL = "about:blank";
  const std::string secondURL = "http://internal.test";

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(firstURL, true);

  WebDataSource* dataSource =
      webViewHelper.webView()->mainFrame()->dataSource();
  ASSERT_TRUE(dataSource);
  dataSource->appendRedirect(toKURL(secondURL));

  WebVector<WebURL> redirects;
  dataSource->redirectChain(redirects);
  ASSERT_EQ(2U, redirects.size());
  EXPECT_EQ(toKURL(firstURL), KURL(redirects[0]));
  EXPECT_EQ(toKURL(secondURL), KURL(redirects[1]));
}

TEST_P(ParameterizedWebFrameTest, IframeRedirect) {
  registerMockedHttpURLLoad("iframe_redirect.html");
  registerMockedHttpURLLoad("visible_iframe.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "iframe_redirect.html", true);
  // Pump pending requests one more time. The test page loads script that
  // navigates.
  FrameTestHelpers::pumpPendingRequestsForFrameToLoad(
      webViewHelper.webView()->mainFrame());

  WebFrame* iframe = webViewHelper.webView()->findFrameByName(
      WebString::fromUTF8("ifr"), nullptr);
  ASSERT_TRUE(iframe);
  WebDataSource* iframeDataSource = iframe->dataSource();
  ASSERT_TRUE(iframeDataSource);
  WebVector<WebURL> redirects;
  iframeDataSource->redirectChain(redirects);
  ASSERT_EQ(2U, redirects.size());
  EXPECT_EQ(toKURL("about:blank"), KURL(redirects[0]));
  EXPECT_EQ(toKURL("http://internal.test/visible_iframe.html"),
            KURL(redirects[1]));
}

TEST_P(ParameterizedWebFrameTest, ClearFocusedNodeTest) {
  registerMockedHttpURLLoad("iframe_clear_focused_node_test.html");
  registerMockedHttpURLLoad("autofocus_input_field_iframe.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(
      m_baseURL + "iframe_clear_focused_node_test.html", true);

  // Clear the focused node.
  webViewHelper.webView()->clearFocusedElement();

  // Now retrieve the FocusedNode and test it should be null.
  EXPECT_EQ(0, webViewHelper.webView()->focusedElement());
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
                 int worldId)
        : frame(frame),
          context(context->GetIsolate(), context),
          worldId(worldId) {}

    ~Notification() { context.Reset(); }

    bool Equals(Notification* other) {
      return other && frame == other->frame && context == other->context &&
             worldId == other->worldId;
    }

    WebLocalFrame* frame;
    v8::Persistent<v8::Context> context;
    int worldId;
  };

  ~ContextLifetimeTestWebFrameClient() override { reset(); }

  void reset() {
    createNotifications.clear();
    releaseNotifications.clear();
  }

  Vector<std::unique_ptr<Notification>> createNotifications;
  Vector<std::unique_ptr<Notification>> releaseNotifications;

 private:
  void didCreateScriptContext(WebLocalFrame* frame,
                              v8::Local<v8::Context> context,
                              int extensionGroup,
                              int worldId) override {
    createNotifications.append(
        makeUnique<Notification>(frame, context, worldId));
  }

  void willReleaseScriptContext(WebLocalFrame* frame,
                                v8::Local<v8::Context> context,
                                int worldId) override {
    releaseNotifications.append(
        makeUnique<Notification>(frame, context, worldId));
  }
};

TEST_P(ParameterizedWebFrameTest, ContextNotificationsLoadUnload) {
  v8::HandleScope handleScope(v8::Isolate::GetCurrent());

  registerMockedHttpURLLoad("context_notifications_test.html");
  registerMockedHttpURLLoad("context_notifications_test_frame.html");

  // Load a frame with an iframe, make sure we get the right create
  // notifications.
  ContextLifetimeTestWebFrameClient webFrameClient;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "context_notifications_test.html",
                                  true, &webFrameClient);

  WebFrame* mainFrame = webViewHelper.webView()->mainFrame();
  WebFrame* childFrame = mainFrame->firstChild();

  ASSERT_EQ(2u, webFrameClient.createNotifications.size());
  EXPECT_EQ(0u, webFrameClient.releaseNotifications.size());

  auto& firstCreateNotification = webFrameClient.createNotifications[0];
  auto& secondCreateNotification = webFrameClient.createNotifications[1];

  EXPECT_EQ(mainFrame, firstCreateNotification->frame);
  EXPECT_EQ(mainFrame->mainWorldScriptContext(),
            firstCreateNotification->context);
  EXPECT_EQ(0, firstCreateNotification->worldId);

  EXPECT_EQ(childFrame, secondCreateNotification->frame);
  EXPECT_EQ(childFrame->mainWorldScriptContext(),
            secondCreateNotification->context);
  EXPECT_EQ(0, secondCreateNotification->worldId);

  // Close the view. We should get two release notifications that are exactly
  // the same as the create ones, in reverse order.
  webViewHelper.reset();

  ASSERT_EQ(2u, webFrameClient.releaseNotifications.size());
  auto& firstReleaseNotification = webFrameClient.releaseNotifications[0];
  auto& secondReleaseNotification = webFrameClient.releaseNotifications[1];

  ASSERT_TRUE(firstCreateNotification->Equals(secondReleaseNotification.get()));
  ASSERT_TRUE(secondCreateNotification->Equals(firstReleaseNotification.get()));
}

TEST_P(ParameterizedWebFrameTest, ContextNotificationsReload) {
  v8::HandleScope handleScope(v8::Isolate::GetCurrent());

  registerMockedHttpURLLoad("context_notifications_test.html");
  registerMockedHttpURLLoad("context_notifications_test_frame.html");

  ContextLifetimeTestWebFrameClient webFrameClient;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "context_notifications_test.html",
                                  true, &webFrameClient);

  // Refresh, we should get two release notifications and two more create
  // notifications.
  FrameTestHelpers::reloadFrame(webViewHelper.webView()->mainFrame());
  ASSERT_EQ(4u, webFrameClient.createNotifications.size());
  ASSERT_EQ(2u, webFrameClient.releaseNotifications.size());

  // The two release notifications we got should be exactly the same as the
  // first two create notifications.
  for (size_t i = 0; i < webFrameClient.releaseNotifications.size(); ++i) {
    EXPECT_TRUE(webFrameClient.releaseNotifications[i]->Equals(
        webFrameClient
            .createNotifications[webFrameClient.createNotifications.size() - 3 -
                                 i]
            .get()));
  }

  // The last two create notifications should be for the current frames and
  // context.
  WebFrame* mainFrame = webViewHelper.webView()->mainFrame();
  WebFrame* childFrame = mainFrame->firstChild();
  auto& firstRefreshNotification = webFrameClient.createNotifications[2];
  auto& secondRefreshNotification = webFrameClient.createNotifications[3];

  EXPECT_EQ(mainFrame, firstRefreshNotification->frame);
  EXPECT_EQ(mainFrame->mainWorldScriptContext(),
            firstRefreshNotification->context);
  EXPECT_EQ(0, firstRefreshNotification->worldId);

  EXPECT_EQ(childFrame, secondRefreshNotification->frame);
  EXPECT_EQ(childFrame->mainWorldScriptContext(),
            secondRefreshNotification->context);
  EXPECT_EQ(0, secondRefreshNotification->worldId);
}

TEST_P(ParameterizedWebFrameTest, ContextNotificationsIsolatedWorlds) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handleScope(isolate);

  registerMockedHttpURLLoad("context_notifications_test.html");
  registerMockedHttpURLLoad("context_notifications_test_frame.html");

  ContextLifetimeTestWebFrameClient webFrameClient;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "context_notifications_test.html",
                                  true, &webFrameClient);

  // Add an isolated world.
  webFrameClient.reset();

  int isolatedWorldId = 42;
  WebScriptSource scriptSource("hi!");
  int numSources = 1;
  int extensionGroup = 0;
  webViewHelper.webView()->mainFrame()->executeScriptInIsolatedWorld(
      isolatedWorldId, &scriptSource, numSources, extensionGroup);

  // We should now have a new create notification.
  ASSERT_EQ(1u, webFrameClient.createNotifications.size());
  auto& notification = webFrameClient.createNotifications[0];
  ASSERT_EQ(isolatedWorldId, notification->worldId);
  ASSERT_EQ(webViewHelper.webView()->mainFrame(), notification->frame);

  // We don't have an API to enumarate isolated worlds for a frame, but we can
  // at least assert that the context we got is *not* the main world's context.
  ASSERT_NE(webViewHelper.webView()->mainFrame()->mainWorldScriptContext(),
            v8::Local<v8::Context>::New(isolate, notification->context));

  webViewHelper.reset();

  // We should have gotten three release notifications (one for each of the
  // frames, plus one for the isolated context).
  ASSERT_EQ(3u, webFrameClient.releaseNotifications.size());

  // And one of them should be exactly the same as the create notification for
  // the isolated context.
  int matchCount = 0;
  for (size_t i = 0; i < webFrameClient.releaseNotifications.size(); ++i) {
    if (webFrameClient.releaseNotifications[i]->Equals(
            webFrameClient.createNotifications[0].get()))
      ++matchCount;
  }
  EXPECT_EQ(1, matchCount);
}

TEST_P(ParameterizedWebFrameTest, FindInPage) {
  registerMockedHttpURLLoad("find.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "find.html");
  ASSERT_TRUE(webViewHelper.webView()->mainFrameImpl());
  WebLocalFrame* frame = webViewHelper.webView()->mainFrameImpl();
  const int findIdentifier = 12345;
  WebFindOptions options;

  // Find in a <div> element.
  EXPECT_TRUE(
      frame->find(findIdentifier, WebString::fromUTF8("bar1"), options, false));
  frame->stopFinding(WebLocalFrame::StopFindActionKeepSelection);
  WebRange range = frame->selectionRange();
  EXPECT_EQ(5, range.startOffset());
  EXPECT_EQ(9, range.endOffset());
  EXPECT_TRUE(frame->document().focusedElement().isNull());

  // Find in an <input> value.
  EXPECT_TRUE(
      frame->find(findIdentifier, WebString::fromUTF8("bar2"), options, false));
  // Confirm stopFinding(WebLocalFrame::StopFindActionKeepSelection) sets the
  // selection on the found text.
  frame->stopFinding(WebLocalFrame::StopFindActionKeepSelection);
  range = frame->selectionRange();
  ASSERT_FALSE(range.isNull());
  EXPECT_EQ(5, range.startOffset());
  EXPECT_EQ(9, range.endOffset());
  EXPECT_TRUE(frame->document().focusedElement().hasHTMLTagName("input"));

  // Find in a <textarea> content.
  EXPECT_TRUE(
      frame->find(findIdentifier, WebString::fromUTF8("bar3"), options, false));
  // Confirm stopFinding(WebLocalFrame::StopFindActionKeepSelection) sets the
  // selection on the found text.
  frame->stopFinding(WebLocalFrame::StopFindActionKeepSelection);
  range = frame->selectionRange();
  ASSERT_FALSE(range.isNull());
  EXPECT_EQ(5, range.startOffset());
  EXPECT_EQ(9, range.endOffset());
  EXPECT_TRUE(frame->document().focusedElement().hasHTMLTagName("textarea"));

  // Find in a contentEditable element.
  EXPECT_TRUE(
      frame->find(findIdentifier, WebString::fromUTF8("bar4"), options, false));
  // Confirm stopFinding(WebLocalFrame::StopFindActionKeepSelection) sets the
  // selection on the found text.
  frame->stopFinding(WebLocalFrame::StopFindActionKeepSelection);
  range = frame->selectionRange();
  ASSERT_FALSE(range.isNull());
  EXPECT_EQ(0, range.startOffset());
  EXPECT_EQ(4, range.endOffset());
  // "bar4" is surrounded by <span>, but the focusable node should be the parent
  // <div>.
  EXPECT_TRUE(frame->document().focusedElement().hasHTMLTagName("div"));

  // Find in <select> content.
  EXPECT_FALSE(
      frame->find(findIdentifier, WebString::fromUTF8("bar5"), options, false));
  // If there are any matches, stopFinding will set the selection on the found
  // text.  However, we do not expect any matches, so check that the selection
  // is null.
  frame->stopFinding(WebLocalFrame::StopFindActionKeepSelection);
  range = frame->selectionRange();
  ASSERT_TRUE(range.isNull());
}

TEST_P(ParameterizedWebFrameTest, GetContentAsPlainText) {
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad("about:blank", true);
  // We set the size because it impacts line wrapping, which changes the
  // resulting text value.
  webViewHelper.resize(WebSize(640, 480));
  WebFrame* frame = webViewHelper.webView()->mainFrame();

  // Generate a simple test case.
  const char simpleSource[] = "<div>Foo bar</div><div></div>baz";
  KURL testURL = toKURL("about:blank");
  FrameTestHelpers::loadHTMLString(frame, simpleSource, testURL);

  // Make sure it comes out OK.
  const std::string expected("Foo bar\nbaz");
  WebString text = WebFrameContentDumper::dumpWebViewAsText(
      webViewHelper.webView(), std::numeric_limits<size_t>::max());
  EXPECT_EQ(expected, text.utf8());

  // Try reading the same one with clipping of the text.
  const int length = 5;
  text =
      WebFrameContentDumper::dumpWebViewAsText(webViewHelper.webView(), length);
  EXPECT_EQ(expected.substr(0, length), text.utf8());

  // Now do a new test with a subframe.
  const char outerFrameSource[] = "Hello<iframe></iframe> world";
  FrameTestHelpers::loadHTMLString(frame, outerFrameSource, testURL);

  // Load something into the subframe.
  WebFrame* subframe = frame->firstChild();
  ASSERT_TRUE(subframe);
  FrameTestHelpers::loadHTMLString(subframe, "sub<p>text", testURL);

  text = WebFrameContentDumper::dumpWebViewAsText(
      webViewHelper.webView(), std::numeric_limits<size_t>::max());
  EXPECT_EQ("Hello world\n\nsub\ntext", text.utf8());

  // Get the frame text where the subframe separator falls on the boundary of
  // what we'll take. There used to be a crash in this case.
  text = WebFrameContentDumper::dumpWebViewAsText(webViewHelper.webView(), 12);
  EXPECT_EQ("Hello world", text.utf8());
}

TEST_P(ParameterizedWebFrameTest, GetFullHtmlOfPage) {
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad("about:blank", true);
  WebLocalFrame* frame = webViewHelper.webView()->mainFrameImpl();

  // Generate a simple test case.
  const char simpleSource[] = "<p>Hello</p><p>World</p>";
  KURL testURL = toKURL("about:blank");
  FrameTestHelpers::loadHTMLString(frame, simpleSource, testURL);

  WebString text = WebFrameContentDumper::dumpWebViewAsText(
      webViewHelper.webView(), std::numeric_limits<size_t>::max());
  EXPECT_EQ("Hello\n\nWorld", text.utf8());

  const std::string html = WebFrameContentDumper::dumpAsMarkup(frame).utf8();

  // Load again with the output html.
  FrameTestHelpers::loadHTMLString(frame, html, testURL);

  EXPECT_EQ(html, WebFrameContentDumper::dumpAsMarkup(frame).utf8());

  text = WebFrameContentDumper::dumpWebViewAsText(
      webViewHelper.webView(), std::numeric_limits<size_t>::max());
  EXPECT_EQ("Hello\n\nWorld", text.utf8());

  // Test selection check
  EXPECT_FALSE(frame->hasSelection());
  frame->executeCommand(WebString::fromUTF8("SelectAll"));
  EXPECT_TRUE(frame->hasSelection());
  frame->executeCommand(WebString::fromUTF8("Unselect"));
  EXPECT_FALSE(frame->hasSelection());
  WebString selectionHtml = frame->selectionAsMarkup();
  EXPECT_TRUE(selectionHtml.isEmpty());
}

class TestExecuteScriptDuringDidCreateScriptContext
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  void didCreateScriptContext(WebLocalFrame* frame,
                              v8::Local<v8::Context> context,
                              int extensionGroup,
                              int worldId) override {
    frame->executeScript(WebScriptSource("window.history = 'replaced';"));
  }
};

TEST_P(ParameterizedWebFrameTest, ExecuteScriptDuringDidCreateScriptContext) {
  registerMockedHttpURLLoad("hello_world.html");

  TestExecuteScriptDuringDidCreateScriptContext webFrameClient;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "hello_world.html", true,
                                  &webFrameClient);

  FrameTestHelpers::reloadFrame(webViewHelper.webView()->mainFrame());
}

class FindUpdateWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
 public:
  FindUpdateWebFrameClient()
      : m_findResultsAreReady(false), m_count(-1), m_activeIndex(-1) {}

  void reportFindInPageMatchCount(int, int count, bool finalUpdate) override {
    m_count = count;
    if (finalUpdate)
      m_findResultsAreReady = true;
  }

  void reportFindInPageSelection(int,
                                 int activeMatchOrdinal,
                                 const WebRect&) override {
    m_activeIndex = activeMatchOrdinal;
  }

  bool findResultsAreReady() const { return m_findResultsAreReady; }
  int count() const { return m_count; }
  int activeIndex() const { return m_activeIndex; }

 private:
  bool m_findResultsAreReady;
  int m_count;
  int m_activeIndex;
};

TEST_P(ParameterizedWebFrameTest, FindInPageMatchRects) {
  registerMockedHttpURLLoad("find_in_page_frame.html");

  FindUpdateWebFrameClient client;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "find_in_page_frame.html", true,
                                  &client);
  webViewHelper.resize(WebSize(640, 480));
  webViewHelper.webView()->setMaximumLegibleScale(1.f);
  webViewHelper.webView()->updateAllLifecyclePhases();
  runPendingTasks();

  // Note that the 'result 19' in the <select> element is not expected to
  // produce a match. Also, results 00 and 01 are in a different frame that is
  // not included in this test.
  const char kFindString[] = "result";
  const int kFindIdentifier = 12345;
  const int kNumResults = 17;

  WebFindOptions options;
  WebString searchText = WebString::fromUTF8(kFindString);
  WebLocalFrameImpl* mainFrame = webViewHelper.webView()->mainFrameImpl();
  EXPECT_TRUE(mainFrame->find(kFindIdentifier, searchText, options, false));

  mainFrame->ensureTextFinder().resetMatchCount();

  for (WebLocalFrameImpl* frame = mainFrame; frame;
       frame = static_cast<WebLocalFrameImpl*>(frame->traverseNext()))
    frame->ensureTextFinder().scopeStringMatches(kFindIdentifier, searchText,
                                                 options, true);

  runPendingTasks();
  EXPECT_TRUE(client.findResultsAreReady());

  WebVector<WebFloatRect> webMatchRects;
  mainFrame->findMatchRects(webMatchRects);
  ASSERT_EQ(static_cast<size_t>(kNumResults), webMatchRects.size());
  int rectsVersion = mainFrame->findMatchMarkersVersion();

  for (int resultIndex = 0; resultIndex < kNumResults; ++resultIndex) {
    FloatRect resultRect = static_cast<FloatRect>(webMatchRects[resultIndex]);

    // Select the match by the center of its rect.
    EXPECT_EQ(mainFrame->selectNearestFindMatch(resultRect.center(), 0),
              resultIndex + 1);

    // Check that the find result ordering matches with our expectations.
    Range* result = mainFrame->textFinder()->activeMatch();
    ASSERT_TRUE(result);
    result->setEnd(result->endContainer(), result->endOffset() + 3);
    EXPECT_EQ(result->text(),
              String::format("%s %02d", kFindString, resultIndex + 2));

    // Verify that the expected match rect also matches the currently active
    // match.  Compare the enclosing rects to prevent precision issues caused by
    // CSS transforms.
    FloatRect activeMatch = mainFrame->activeFindMatchRect();
    EXPECT_EQ(enclosingIntRect(activeMatch), enclosingIntRect(resultRect));

    // The rects version should not have changed.
    EXPECT_EQ(mainFrame->findMatchMarkersVersion(), rectsVersion);
  }

  // Resizing should update the rects version.
  webViewHelper.resize(WebSize(800, 600));
  runPendingTasks();
  EXPECT_TRUE(mainFrame->findMatchMarkersVersion() != rectsVersion);
}

TEST_F(WebFrameTest, FindInPageActiveIndex) {
  registerMockedHttpURLLoad("find_match_count.html");

  FindUpdateWebFrameClient client;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "find_match_count.html", true,
                                  &client);
  webViewHelper.webView()->resize(WebSize(640, 480));
  runPendingTasks();

  const char* kFindString = "a";
  const int kFindIdentifier = 7777;
  const int kActiveIndex = 1;

  WebFindOptions options;
  WebString searchText = WebString::fromUTF8(kFindString);
  WebLocalFrameImpl* mainFrame = webViewHelper.webView()->mainFrameImpl();
  EXPECT_TRUE(mainFrame->find(kFindIdentifier, searchText, options, false));
  mainFrame->ensureTextFinder().resetMatchCount();

  for (WebLocalFrameImpl* frame = mainFrame; frame;
       frame = static_cast<WebLocalFrameImpl*>(frame->traverseNext()))
    frame->ensureTextFinder().scopeStringMatches(kFindIdentifier, searchText,
                                                 options, true);

  runPendingTasks();
  EXPECT_TRUE(mainFrame->find(kFindIdentifier, searchText, options, false));
  mainFrame->stopFinding(WebLocalFrame::StopFindActionClearSelection);

  for (WebLocalFrameImpl* frame = mainFrame; frame;
       frame = static_cast<WebLocalFrameImpl*>(frame->traverseNext()))
    frame->ensureTextFinder().scopeStringMatches(kFindIdentifier, searchText,
                                                 options, true);

  runPendingTasks();
  EXPECT_TRUE(client.findResultsAreReady());
  EXPECT_EQ(kActiveIndex, client.activeIndex());

  const char* kFindStringNew = "e";
  WebString searchTextNew = WebString::fromUTF8(kFindStringNew);

  EXPECT_TRUE(mainFrame->find(kFindIdentifier, searchTextNew, options, false));
  mainFrame->ensureTextFinder().resetMatchCount();

  for (WebLocalFrameImpl* frame = mainFrame; frame;
       frame = static_cast<WebLocalFrameImpl*>(frame->traverseNext()))
    frame->ensureTextFinder().scopeStringMatches(kFindIdentifier, searchTextNew,
                                                 options, true);

  runPendingTasks();
  EXPECT_TRUE(client.findResultsAreReady());
  EXPECT_EQ(kActiveIndex, client.activeIndex());
}

TEST_P(ParameterizedWebFrameTest, FindOnDetachedFrame) {
  registerMockedHttpURLLoad("find_in_page.html");
  registerMockedHttpURLLoad("find_in_page_frame.html");

  FindUpdateWebFrameClient client;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "find_in_page.html", true,
                                  &client);
  webViewHelper.resize(WebSize(640, 480));
  runPendingTasks();

  const char kFindString[] = "result";
  const int kFindIdentifier = 12345;

  WebFindOptions options;
  WebString searchText = WebString::fromUTF8(kFindString);
  WebLocalFrameImpl* mainFrame = webViewHelper.webView()->mainFrameImpl();
  WebLocalFrameImpl* secondFrame =
      toWebLocalFrameImpl(mainFrame->traverseNext());

  // Detach the frame before finding.
  removeElementById(mainFrame, "frame");

  EXPECT_TRUE(mainFrame->find(kFindIdentifier, searchText, options, false));
  EXPECT_FALSE(secondFrame->find(kFindIdentifier, searchText, options, false));

  runPendingTasks();
  EXPECT_FALSE(client.findResultsAreReady());

  mainFrame->ensureTextFinder().resetMatchCount();

  for (WebLocalFrameImpl* frame = mainFrame; frame;
       frame = static_cast<WebLocalFrameImpl*>(frame->traverseNext()))
    frame->ensureTextFinder().scopeStringMatches(kFindIdentifier, searchText,
                                                 options, true);

  runPendingTasks();
  EXPECT_TRUE(client.findResultsAreReady());
}

TEST_P(ParameterizedWebFrameTest, FindDetachFrameBeforeScopeStrings) {
  registerMockedHttpURLLoad("find_in_page.html");
  registerMockedHttpURLLoad("find_in_page_frame.html");

  FindUpdateWebFrameClient client;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "find_in_page.html", true,
                                  &client);
  webViewHelper.resize(WebSize(640, 480));
  runPendingTasks();

  const char kFindString[] = "result";
  const int kFindIdentifier = 12345;

  WebFindOptions options;
  WebString searchText = WebString::fromUTF8(kFindString);
  WebLocalFrameImpl* mainFrame = webViewHelper.webView()->mainFrameImpl();

  for (WebFrame* frame = mainFrame; frame; frame = frame->traverseNext())
    EXPECT_TRUE(frame->toWebLocalFrame()->find(kFindIdentifier, searchText,
                                               options, false));

  runPendingTasks();
  EXPECT_FALSE(client.findResultsAreReady());

  // Detach the frame between finding and scoping.
  removeElementById(mainFrame, "frame");

  mainFrame->ensureTextFinder().resetMatchCount();

  for (WebLocalFrameImpl* frame = mainFrame; frame;
       frame = static_cast<WebLocalFrameImpl*>(frame->traverseNext()))
    frame->ensureTextFinder().scopeStringMatches(kFindIdentifier, searchText,
                                                 options, true);

  runPendingTasks();
  EXPECT_TRUE(client.findResultsAreReady());
}

TEST_P(ParameterizedWebFrameTest, FindDetachFrameWhileScopingStrings) {
  registerMockedHttpURLLoad("find_in_page.html");
  registerMockedHttpURLLoad("find_in_page_frame.html");

  FindUpdateWebFrameClient client;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "find_in_page.html", true,
                                  &client);
  webViewHelper.resize(WebSize(640, 480));
  runPendingTasks();

  const char kFindString[] = "result";
  const int kFindIdentifier = 12345;

  WebFindOptions options;
  WebString searchText = WebString::fromUTF8(kFindString);
  WebLocalFrameImpl* mainFrame = webViewHelper.webView()->mainFrameImpl();

  for (WebFrame* frame = mainFrame; frame; frame = frame->traverseNext())
    EXPECT_TRUE(frame->toWebLocalFrame()->find(kFindIdentifier, searchText,
                                               options, false));

  runPendingTasks();
  EXPECT_FALSE(client.findResultsAreReady());

  mainFrame->ensureTextFinder().resetMatchCount();

  for (WebLocalFrameImpl* frame = mainFrame; frame;
       frame = static_cast<WebLocalFrameImpl*>(frame->traverseNext()))
    frame->ensureTextFinder().scopeStringMatches(kFindIdentifier, searchText,
                                                 options, true);

  // The first scopeStringMatches will have reset the state. Detach before it
  // actually scopes.
  removeElementById(mainFrame, "frame");

  runPendingTasks();
  EXPECT_TRUE(client.findResultsAreReady());
}

TEST_P(ParameterizedWebFrameTest, ResetMatchCount) {
  registerMockedHttpURLLoad("find_in_generated_frame.html");

  FindUpdateWebFrameClient client;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "find_in_generated_frame.html",
                                  true, &client);
  webViewHelper.resize(WebSize(640, 480));
  runPendingTasks();

  const char kFindString[] = "result";
  const int kFindIdentifier = 12345;

  WebFindOptions options;
  WebString searchText = WebString::fromUTF8(kFindString);
  WebLocalFrameImpl* mainFrame = webViewHelper.webView()->mainFrameImpl();

  // Check that child frame exists.
  EXPECT_TRUE(!!mainFrame->traverseNext());

  for (WebFrame* frame = mainFrame; frame; frame = frame->traverseNext())
    EXPECT_FALSE(frame->toWebLocalFrame()->find(kFindIdentifier, searchText,
                                                options, false));

  runPendingTasks();
  EXPECT_FALSE(client.findResultsAreReady());

  mainFrame->ensureTextFinder().resetMatchCount();
}

TEST_P(ParameterizedWebFrameTest, SetTickmarks) {
  registerMockedHttpURLLoad("find.html");

  FindUpdateWebFrameClient client;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "find.html", true, &client);
  webViewHelper.resize(WebSize(640, 480));
  runPendingTasks();

  const char kFindString[] = "foo";
  const int kFindIdentifier = 12345;

  WebFindOptions options;
  WebString searchText = WebString::fromUTF8(kFindString);
  WebLocalFrameImpl* mainFrame = webViewHelper.webView()->mainFrameImpl();
  EXPECT_TRUE(mainFrame->find(kFindIdentifier, searchText, options, false));

  mainFrame->ensureTextFinder().resetMatchCount();
  mainFrame->ensureTextFinder().scopeStringMatches(kFindIdentifier, searchText,
                                                   options, true);

  runPendingTasks();
  EXPECT_TRUE(client.findResultsAreReady());

  // Get the tickmarks for the original find request.
  FrameView* frameView = webViewHelper.webView()->mainFrameImpl()->frameView();
  Scrollbar* scrollbar = frameView->createScrollbar(HorizontalScrollbar);
  Vector<IntRect> originalTickmarks;
  scrollbar->getTickmarks(originalTickmarks);
  EXPECT_EQ(4u, originalTickmarks.size());

  // Override the tickmarks.
  Vector<IntRect> overridingTickmarksExpected;
  overridingTickmarksExpected.append(IntRect(0, 0, 100, 100));
  overridingTickmarksExpected.append(IntRect(0, 20, 100, 100));
  overridingTickmarksExpected.append(IntRect(0, 30, 100, 100));
  mainFrame->setTickmarks(overridingTickmarksExpected);

  // Check the tickmarks are overriden correctly.
  Vector<IntRect> overridingTickmarksActual;
  scrollbar->getTickmarks(overridingTickmarksActual);
  EXPECT_EQ(overridingTickmarksExpected, overridingTickmarksActual);

  // Reset the tickmark behavior.
  Vector<IntRect> resetTickmarks;
  mainFrame->setTickmarks(resetTickmarks);

  // Check that the original tickmarks are returned
  Vector<IntRect> originalTickmarksAfterReset;
  scrollbar->getTickmarks(originalTickmarksAfterReset);
  EXPECT_EQ(originalTickmarks, originalTickmarksAfterReset);
}

TEST_P(ParameterizedWebFrameTest, FindInPageJavaScriptUpdatesDOM) {
  registerMockedHttpURLLoad("find.html");

  FindUpdateWebFrameClient client;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "find.html", true, &client);
  webViewHelper.resize(WebSize(640, 480));
  runPendingTasks();

  WebLocalFrameImpl* frame = webViewHelper.webView()->mainFrameImpl();
  const int findIdentifier = 12345;
  static const char* kFindString = "foo";
  WebString searchText = WebString::fromUTF8(kFindString);
  WebFindOptions options;
  bool activeNow;

  frame->ensureTextFinder().resetMatchCount();
  frame->ensureTextFinder().scopeStringMatches(findIdentifier, searchText,
                                               options, true);
  runPendingTasks();
  EXPECT_TRUE(client.findResultsAreReady());

  // Find in a <div> element.
  options.findNext = true;
  EXPECT_TRUE(
      frame->find(findIdentifier, searchText, options, false, &activeNow));
  EXPECT_TRUE(activeNow);

  // Insert new text, which contains occurence of |searchText|.
  frame->executeScript(WebScriptSource(
      "var newTextNode = document.createTextNode('bar5 foo5');"
      "var textArea = document.getElementsByTagName('textarea')[0];"
      "document.body.insertBefore(newTextNode, textArea);"));

  // Find in a <input> element.
  EXPECT_TRUE(
      frame->find(findIdentifier, searchText, options, false, &activeNow));
  EXPECT_TRUE(activeNow);

  // Find in the inserted text node.
  EXPECT_TRUE(
      frame->find(findIdentifier, searchText, options, false, &activeNow));
  frame->stopFinding(WebLocalFrame::StopFindActionKeepSelection);
  WebRange range = frame->selectionRange();
  EXPECT_EQ(5, range.startOffset());
  EXPECT_EQ(8, range.endOffset());
  EXPECT_TRUE(frame->document().focusedElement().isNull());
  EXPECT_FALSE(activeNow);
}

static WebPoint topLeft(const WebRect& rect) {
  return WebPoint(rect.x, rect.y);
}

static WebPoint bottomRightMinusOne(const WebRect& rect) {
  // FIXME: If we don't subtract 1 from the x- and y-coordinates of the
  // selection bounds, selectRange() will select the *next* element. That's
  // strictly correct, as hit-testing checks the pixel to the lower-right of
  // the input coordinate, but it's a wart on the API.
  return WebPoint(rect.x + rect.width - 1, rect.y + rect.height - 1);
}

static WebRect elementBounds(WebFrame* frame, const WebString& id) {
  return frame->document().getElementById(id).boundsInViewport();
}

static std::string selectionAsString(WebFrame* frame) {
  return frame->toWebLocalFrame()->selectionAsText().utf8();
}

TEST_P(ParameterizedWebFrameTest, SelectRange) {
  WebLocalFrame* frame;
  WebRect startWebRect;
  WebRect endWebRect;

  registerMockedHttpURLLoad("select_range_basic.html");
  registerMockedHttpURLLoad("select_range_scroll.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  initializeTextSelectionWebView(m_baseURL + "select_range_basic.html",
                                 &webViewHelper);
  frame = webViewHelper.webView()->mainFrameImpl();
  EXPECT_EQ("Some test text for testing.", selectionAsString(frame));
  webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);
  frame->executeCommand(WebString::fromUTF8("Unselect"));
  EXPECT_EQ("", selectionAsString(frame));
  frame->selectRange(topLeft(startWebRect), bottomRightMinusOne(endWebRect));
  // On some devices, the above bottomRightMinusOne() causes the ending '.' not
  // selected.
  std::string selectionString = selectionAsString(frame);
  EXPECT_TRUE(selectionString == "Some test text for testing." ||
              selectionString == "Some test text for testing");

  initializeTextSelectionWebView(m_baseURL + "select_range_scroll.html",
                                 &webViewHelper);
  frame = webViewHelper.webView()->mainFrameImpl();
  EXPECT_EQ("Some offscreen test text for testing.", selectionAsString(frame));
  webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);
  frame->executeCommand(WebString::fromUTF8("Unselect"));
  EXPECT_EQ("", selectionAsString(frame));
  frame->selectRange(topLeft(startWebRect), bottomRightMinusOne(endWebRect));
  // On some devices, the above bottomRightMinusOne() causes the ending '.' not
  // selected.
  selectionString = selectionAsString(frame);
  EXPECT_TRUE(selectionString == "Some offscreen test text for testing." ||
              selectionString == "Some offscreen test text for testing");
}

TEST_P(ParameterizedWebFrameTest, SelectRangeInIframe) {
  WebFrame* frame;
  WebRect startWebRect;
  WebRect endWebRect;

  registerMockedHttpURLLoad("select_range_iframe.html");
  registerMockedHttpURLLoad("select_range_basic.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  initializeTextSelectionWebView(m_baseURL + "select_range_iframe.html",
                                 &webViewHelper);
  frame = webViewHelper.webView()->mainFrame();
  WebLocalFrame* subframe = frame->firstChild()->toWebLocalFrame();
  EXPECT_EQ("Some test text for testing.", selectionAsString(subframe));
  webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);
  subframe->executeCommand(WebString::fromUTF8("Unselect"));
  EXPECT_EQ("", selectionAsString(subframe));
  subframe->selectRange(topLeft(startWebRect), bottomRightMinusOne(endWebRect));
  // On some devices, the above bottomRightMinusOne() causes the ending '.' not
  // selected.
  std::string selectionString = selectionAsString(subframe);
  EXPECT_TRUE(selectionString == "Some test text for testing." ||
              selectionString == "Some test text for testing");
}

TEST_P(ParameterizedWebFrameTest, SelectRangeDivContentEditable) {
  WebLocalFrame* frame;
  WebRect startWebRect;
  WebRect endWebRect;

  registerMockedHttpURLLoad("select_range_div_editable.html");

  // Select the middle of an editable element, then try to extend the selection
  // to the top of the document.  The selection range should be clipped to the
  // bounds of the editable element.
  FrameTestHelpers::WebViewHelper webViewHelper;
  initializeTextSelectionWebView(m_baseURL + "select_range_div_editable.html",
                                 &webViewHelper);
  frame = webViewHelper.webView()->mainFrameImpl();
  EXPECT_EQ("This text is initially selected.", selectionAsString(frame));
  webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);

  frame->selectRange(bottomRightMinusOne(endWebRect), WebPoint(0, 0));
  EXPECT_EQ("16-char header. This text is initially selected.",
            selectionAsString(frame));

  // As above, but extending the selection to the bottom of the document.
  initializeTextSelectionWebView(m_baseURL + "select_range_div_editable.html",
                                 &webViewHelper);
  frame = webViewHelper.webView()->mainFrameImpl();

  webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);
  frame->selectRange(topLeft(startWebRect), bottomRightMinusOne(endWebRect));
  EXPECT_EQ("This text is initially selected.", selectionAsString(frame));
  webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);

  webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);
  frame->selectRange(topLeft(startWebRect), WebPoint(640, 480));
  EXPECT_EQ("This text is initially selected. 16-char footer.",
            selectionAsString(frame));
}

// positionForPoint returns the wrong values for contenteditable spans. See
// http://crbug.com/238334.
TEST_P(ParameterizedWebFrameTest, DISABLED_SelectRangeSpanContentEditable) {
  WebLocalFrame* frame;
  WebRect startWebRect;
  WebRect endWebRect;

  registerMockedHttpURLLoad("select_range_span_editable.html");

  // Select the middle of an editable element, then try to extend the selection
  // to the top of the document.
  // The selection range should be clipped to the bounds of the editable
  // element.
  FrameTestHelpers::WebViewHelper webViewHelper;
  initializeTextSelectionWebView(m_baseURL + "select_range_span_editable.html",
                                 &webViewHelper);
  frame = webViewHelper.webView()->mainFrameImpl();
  EXPECT_EQ("This text is initially selected.", selectionAsString(frame));
  webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);

  frame->selectRange(bottomRightMinusOne(endWebRect), WebPoint(0, 0));
  EXPECT_EQ("16-char header. This text is initially selected.",
            selectionAsString(frame));

  // As above, but extending the selection to the bottom of the document.
  initializeTextSelectionWebView(m_baseURL + "select_range_span_editable.html",
                                 &webViewHelper);
  frame = webViewHelper.webView()->mainFrameImpl();

  webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);
  frame->selectRange(topLeft(startWebRect), bottomRightMinusOne(endWebRect));
  EXPECT_EQ("This text is initially selected.", selectionAsString(frame));
  webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);

  EXPECT_EQ("This text is initially selected.", selectionAsString(frame));
  webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);
  frame->selectRange(topLeft(startWebRect), WebPoint(640, 480));
  EXPECT_EQ("This text is initially selected. 16-char footer.",
            selectionAsString(frame));
}

TEST_P(ParameterizedWebFrameTest, SelectRangeCanMoveSelectionStart) {
  registerMockedHttpURLLoad("text_selection.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  initializeTextSelectionWebView(m_baseURL + "text_selection.html",
                                 &webViewHelper);
  WebLocalFrame* frame = webViewHelper.webView()->mainFrameImpl();

  // Select second span. We can move the start to include the first span.
  frame->executeScript(WebScriptSource("selectElement('header_2');"));
  EXPECT_EQ("Header 2.", selectionAsString(frame));
  frame->selectRange(bottomRightMinusOne(elementBounds(frame, "header_2")),
                     topLeft(elementBounds(frame, "header_1")));
  EXPECT_EQ("Header 1. Header 2.", selectionAsString(frame));

  // We can move the start and end together.
  frame->executeScript(WebScriptSource("selectElement('header_1');"));
  EXPECT_EQ("Header 1.", selectionAsString(frame));
  frame->selectRange(bottomRightMinusOne(elementBounds(frame, "header_1")),
                     bottomRightMinusOne(elementBounds(frame, "header_1")));
  EXPECT_EQ("", selectionAsString(frame));
  // Selection is a caret, not empty.
  EXPECT_FALSE(frame->selectionRange().isNull());

  // We can move the start across the end.
  frame->executeScript(WebScriptSource("selectElement('header_1');"));
  EXPECT_EQ("Header 1.", selectionAsString(frame));
  frame->selectRange(bottomRightMinusOne(elementBounds(frame, "header_1")),
                     bottomRightMinusOne(elementBounds(frame, "header_2")));
  EXPECT_EQ(" Header 2.", selectionAsString(frame));

  // Can't extend the selection part-way into an editable element.
  frame->executeScript(WebScriptSource("selectElement('footer_2');"));
  EXPECT_EQ("Footer 2.", selectionAsString(frame));
  frame->selectRange(bottomRightMinusOne(elementBounds(frame, "footer_2")),
                     topLeft(elementBounds(frame, "editable_2")));
  EXPECT_EQ(" [ Footer 1. Footer 2.", selectionAsString(frame));

  // Can extend the selection completely across editable elements.
  frame->executeScript(WebScriptSource("selectElement('footer_2');"));
  EXPECT_EQ("Footer 2.", selectionAsString(frame));
  frame->selectRange(bottomRightMinusOne(elementBounds(frame, "footer_2")),
                     topLeft(elementBounds(frame, "header_2")));
  EXPECT_EQ("Header 2. ] [ Editable 1. Editable 2. ] [ Footer 1. Footer 2.",
            selectionAsString(frame));

  // If the selection is editable text, we can't extend it into non-editable
  // text.
  frame->executeScript(WebScriptSource("selectElement('editable_2');"));
  EXPECT_EQ("Editable 2.", selectionAsString(frame));
  frame->selectRange(bottomRightMinusOne(elementBounds(frame, "editable_2")),
                     topLeft(elementBounds(frame, "header_2")));
  // positionForPoint returns the wrong values for contenteditable spans. See
  // http://crbug.com/238334.
  // EXPECT_EQ("[ Editable 1. Editable 2.", selectionAsString(frame));
}

TEST_P(ParameterizedWebFrameTest, SelectRangeCanMoveSelectionEnd) {
  registerMockedHttpURLLoad("text_selection.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  initializeTextSelectionWebView(m_baseURL + "text_selection.html",
                                 &webViewHelper);
  WebLocalFrame* frame = webViewHelper.webView()->mainFrameImpl();

  // Select first span. We can move the end to include the second span.
  frame->executeScript(WebScriptSource("selectElement('header_1');"));
  EXPECT_EQ("Header 1.", selectionAsString(frame));
  frame->selectRange(topLeft(elementBounds(frame, "header_1")),
                     bottomRightMinusOne(elementBounds(frame, "header_2")));
  EXPECT_EQ("Header 1. Header 2.", selectionAsString(frame));

  // We can move the start and end together.
  frame->executeScript(WebScriptSource("selectElement('header_2');"));
  EXPECT_EQ("Header 2.", selectionAsString(frame));
  frame->selectRange(topLeft(elementBounds(frame, "header_2")),
                     topLeft(elementBounds(frame, "header_2")));
  EXPECT_EQ("", selectionAsString(frame));
  // Selection is a caret, not empty.
  EXPECT_FALSE(frame->selectionRange().isNull());

  // We can move the end across the start.
  frame->executeScript(WebScriptSource("selectElement('header_2');"));
  EXPECT_EQ("Header 2.", selectionAsString(frame));
  frame->selectRange(topLeft(elementBounds(frame, "header_2")),
                     topLeft(elementBounds(frame, "header_1")));
  EXPECT_EQ("Header 1. ", selectionAsString(frame));

  // Can't extend the selection part-way into an editable element.
  frame->executeScript(WebScriptSource("selectElement('header_1');"));
  EXPECT_EQ("Header 1.", selectionAsString(frame));
  frame->selectRange(topLeft(elementBounds(frame, "header_1")),
                     bottomRightMinusOne(elementBounds(frame, "editable_1")));
  EXPECT_EQ("Header 1. Header 2. ] ", selectionAsString(frame));

  // Can extend the selection completely across editable elements.
  frame->executeScript(WebScriptSource("selectElement('header_1');"));
  EXPECT_EQ("Header 1.", selectionAsString(frame));
  frame->selectRange(topLeft(elementBounds(frame, "header_1")),
                     bottomRightMinusOne(elementBounds(frame, "footer_1")));
  EXPECT_EQ("Header 1. Header 2. ] [ Editable 1. Editable 2. ] [ Footer 1.",
            selectionAsString(frame));

  // If the selection is editable text, we can't extend it into non-editable
  // text.
  frame->executeScript(WebScriptSource("selectElement('editable_1');"));
  EXPECT_EQ("Editable 1.", selectionAsString(frame));
  frame->selectRange(topLeft(elementBounds(frame, "editable_1")),
                     bottomRightMinusOne(elementBounds(frame, "footer_1")));
  // positionForPoint returns the wrong values for contenteditable spans. See
  // http://crbug.com/238334.
  // EXPECT_EQ("Editable 1. Editable 2. ]", selectionAsString(frame));
}

TEST_P(ParameterizedWebFrameTest, MoveRangeSelectionExtent) {
  WebLocalFrameImpl* frame;
  WebRect startWebRect;
  WebRect endWebRect;

  registerMockedHttpURLLoad("move_range_selection_extent.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  initializeTextSelectionWebView(m_baseURL + "move_range_selection_extent.html",
                                 &webViewHelper);
  frame = webViewHelper.webView()->mainFrameImpl();
  EXPECT_EQ("This text is initially selected.", selectionAsString(frame));
  webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);

  frame->moveRangeSelectionExtent(WebPoint(640, 480));
  EXPECT_EQ("This text is initially selected. 16-char footer.",
            selectionAsString(frame));

  frame->moveRangeSelectionExtent(WebPoint(0, 0));
  EXPECT_EQ("16-char header. ", selectionAsString(frame));

  // Reset with swapped base and extent.
  frame->selectRange(topLeft(endWebRect), bottomRightMinusOne(startWebRect));
  EXPECT_EQ("This text is initially selected.", selectionAsString(frame));

  frame->moveRangeSelectionExtent(WebPoint(640, 480));
  EXPECT_EQ(" 16-char footer.", selectionAsString(frame));

  frame->moveRangeSelectionExtent(WebPoint(0, 0));
  EXPECT_EQ("16-char header. This text is initially selected.",
            selectionAsString(frame));

  frame->executeCommand(WebString::fromUTF8("Unselect"));
  EXPECT_EQ("", selectionAsString(frame));
}

TEST_P(ParameterizedWebFrameTest, MoveRangeSelectionExtentCannotCollapse) {
  WebLocalFrameImpl* frame;
  WebRect startWebRect;
  WebRect endWebRect;

  registerMockedHttpURLLoad("move_range_selection_extent.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  initializeTextSelectionWebView(m_baseURL + "move_range_selection_extent.html",
                                 &webViewHelper);
  frame = webViewHelper.webView()->mainFrameImpl();
  EXPECT_EQ("This text is initially selected.", selectionAsString(frame));
  webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);

  frame->moveRangeSelectionExtent(bottomRightMinusOne(startWebRect));
  EXPECT_EQ("This text is initially selected.", selectionAsString(frame));

  // Reset with swapped base and extent.
  frame->selectRange(topLeft(endWebRect), bottomRightMinusOne(startWebRect));
  EXPECT_EQ("This text is initially selected.", selectionAsString(frame));

  frame->moveRangeSelectionExtent(bottomRightMinusOne(endWebRect));
  EXPECT_EQ("This text is initially selected.", selectionAsString(frame));
}

TEST_P(ParameterizedWebFrameTest, MoveRangeSelectionExtentScollsInputField) {
  WebLocalFrameImpl* frame;
  WebRect startWebRect;
  WebRect endWebRect;

  registerMockedHttpURLLoad("move_range_selection_extent_input_field.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  initializeTextSelectionWebView(
      m_baseURL + "move_range_selection_extent_input_field.html",
      &webViewHelper);
  frame = webViewHelper.webView()->mainFrameImpl();
  EXPECT_EQ("Length", selectionAsString(frame));
  webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);

  EXPECT_EQ(0, frame->frame()->selection().rootEditableElement()->scrollLeft());
  frame->moveRangeSelectionExtent(WebPoint(endWebRect.x + 500, endWebRect.y));
  EXPECT_GE(frame->frame()->selection().rootEditableElement()->scrollLeft(), 1);
  EXPECT_EQ("Lengthy text goes here.", selectionAsString(frame));
}

static int computeOffset(LayoutObject* layoutObject, int x, int y) {
  return createVisiblePosition(
             layoutObject->positionForPoint(LayoutPoint(x, y)))
      .deepEquivalent()
      .computeOffsetInContainerNode();
}

// positionForPoint returns the wrong values for contenteditable spans. See
// http://crbug.com/238334.
TEST_P(ParameterizedWebFrameTest, DISABLED_PositionForPointTest) {
  registerMockedHttpURLLoad("select_range_span_editable.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  initializeTextSelectionWebView(m_baseURL + "select_range_span_editable.html",
                                 &webViewHelper);
  WebLocalFrameImpl* mainFrame = webViewHelper.webView()->mainFrameImpl();
  LayoutObject* layoutObject =
      mainFrame->frame()->selection().rootEditableElement()->layoutObject();
  EXPECT_EQ(0, computeOffset(layoutObject, -1, -1));
  EXPECT_EQ(64, computeOffset(layoutObject, 1000, 1000));

  registerMockedHttpURLLoad("select_range_div_editable.html");
  initializeTextSelectionWebView(m_baseURL + "select_range_div_editable.html",
                                 &webViewHelper);
  mainFrame = webViewHelper.webView()->mainFrameImpl();
  layoutObject =
      mainFrame->frame()->selection().rootEditableElement()->layoutObject();
  EXPECT_EQ(0, computeOffset(layoutObject, -1, -1));
  EXPECT_EQ(64, computeOffset(layoutObject, 1000, 1000));
}

#if !OS(MACOSX) && !OS(LINUX)
TEST_P(ParameterizedWebFrameTest,
       SelectRangeStaysHorizontallyAlignedWhenMoved) {
  registerMockedHttpURLLoad("move_caret.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  initializeTextSelectionWebView(m_baseURL + "move_caret.html", &webViewHelper);
  WebLocalFrameImpl* frame = webViewHelper.webView()->mainFrameImpl();

  WebRect initialStartRect;
  WebRect initialEndRect;
  WebRect startRect;
  WebRect endRect;

  frame->executeScript(WebScriptSource("selectRange();"));
  webViewHelper.webView()->selectionBounds(initialStartRect, initialEndRect);
  WebPoint movedStart(topLeft(initialStartRect));

  movedStart.y += 40;
  frame->selectRange(movedStart, bottomRightMinusOne(initialEndRect));
  webViewHelper.webView()->selectionBounds(startRect, endRect);
  EXPECT_EQ(startRect, initialStartRect);
  EXPECT_EQ(endRect, initialEndRect);

  movedStart.y -= 80;
  frame->selectRange(movedStart, bottomRightMinusOne(initialEndRect));
  webViewHelper.webView()->selectionBounds(startRect, endRect);
  EXPECT_EQ(startRect, initialStartRect);
  EXPECT_EQ(endRect, initialEndRect);

  WebPoint movedEnd(bottomRightMinusOne(initialEndRect));

  movedEnd.y += 40;
  frame->selectRange(topLeft(initialStartRect), movedEnd);
  webViewHelper.webView()->selectionBounds(startRect, endRect);
  EXPECT_EQ(startRect, initialStartRect);
  EXPECT_EQ(endRect, initialEndRect);

  movedEnd.y -= 80;
  frame->selectRange(topLeft(initialStartRect), movedEnd);
  webViewHelper.webView()->selectionBounds(startRect, endRect);
  EXPECT_EQ(startRect, initialStartRect);
  EXPECT_EQ(endRect, initialEndRect);
}

TEST_P(ParameterizedWebFrameTest, MoveCaretStaysHorizontallyAlignedWhenMoved) {
  WebLocalFrameImpl* frame;
  registerMockedHttpURLLoad("move_caret.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  initializeTextSelectionWebView(m_baseURL + "move_caret.html", &webViewHelper);
  frame = (WebLocalFrameImpl*)webViewHelper.webView()->mainFrame();

  WebRect initialStartRect;
  WebRect initialEndRect;
  WebRect startRect;
  WebRect endRect;

  frame->executeScript(WebScriptSource("selectCaret();"));
  webViewHelper.webView()->selectionBounds(initialStartRect, initialEndRect);
  WebPoint moveTo(topLeft(initialStartRect));

  moveTo.y += 40;
  frame->moveCaretSelection(moveTo);
  webViewHelper.webView()->selectionBounds(startRect, endRect);
  EXPECT_EQ(startRect, initialStartRect);
  EXPECT_EQ(endRect, initialEndRect);

  moveTo.y -= 80;
  frame->moveCaretSelection(moveTo);
  webViewHelper.webView()->selectionBounds(startRect, endRect);
  EXPECT_EQ(startRect, initialStartRect);
  EXPECT_EQ(endRect, initialEndRect);
}
#endif

class CompositedSelectionBoundsTestLayerTreeView : public WebLayerTreeView {
 public:
  CompositedSelectionBoundsTestLayerTreeView() : m_selectionCleared(false) {}
  ~CompositedSelectionBoundsTestLayerTreeView() override {}

  void registerSelection(const WebSelection& selection) override {
    m_selection = makeUnique<WebSelection>(selection);
  }

  void clearSelection() override {
    m_selectionCleared = true;
    m_selection.reset();
  }

  bool getAndResetSelectionCleared() {
    bool selectionCleared = m_selectionCleared;
    m_selectionCleared = false;
    return selectionCleared;
  }

  const WebSelection* selection() const { return m_selection.get(); }
  const WebSelectionBound* start() const {
    return m_selection ? &m_selection->start() : nullptr;
  }
  const WebSelectionBound* end() const {
    return m_selection ? &m_selection->end() : nullptr;
  }

 private:
  bool m_selectionCleared;
  std::unique_ptr<WebSelection> m_selection;
};

class CompositedSelectionBoundsTestWebViewClient
    : public FrameTestHelpers::TestWebViewClient {
 public:
  ~CompositedSelectionBoundsTestWebViewClient() override {}
  WebLayerTreeView* layerTreeView() override { return &m_testLayerTreeView; }

  CompositedSelectionBoundsTestLayerTreeView& selectionLayerTreeView() {
    return m_testLayerTreeView;
  }

 private:
  CompositedSelectionBoundsTestLayerTreeView m_testLayerTreeView;
};

class CompositedSelectionBoundsTest : public WebFrameTest {
 protected:
  CompositedSelectionBoundsTest()
      : m_fakeSelectionLayerTreeView(
            m_fakeSelectionWebViewClient.selectionLayerTreeView()) {
    RuntimeEnabledFeatures::setCompositedSelectionUpdateEnabled(true);
    registerMockedHttpURLLoad("Ahem.ttf");

    m_webViewHelper.initialize(true, nullptr, &m_fakeSelectionWebViewClient,
                               nullptr);
    m_webViewHelper.webView()->settings()->setDefaultFontSize(12);
    m_webViewHelper.webView()->setDefaultPageScaleLimits(1, 1);
    m_webViewHelper.resize(WebSize(640, 480));
  }

  void runTest(const char* testFile) {
    registerMockedHttpURLLoad(testFile);
    m_webViewHelper.webView()->setFocus(true);
    FrameTestHelpers::loadFrame(m_webViewHelper.webView()->mainFrame(),
                                m_baseURL + testFile);
    m_webViewHelper.webView()->updateAllLifecyclePhases();

    const WebSelection* selection = m_fakeSelectionLayerTreeView.selection();
    const WebSelectionBound* selectStart = m_fakeSelectionLayerTreeView.start();
    const WebSelectionBound* selectEnd = m_fakeSelectionLayerTreeView.end();

    v8::HandleScope handleScope(v8::Isolate::GetCurrent());
    v8::Local<v8::Value> result =
        m_webViewHelper.webView()->mainFrameImpl()->executeScriptAndReturnValue(
            WebScriptSource("expectedResult"));
    if (result.IsEmpty() || (*result)->IsUndefined()) {
      EXPECT_FALSE(selection);
      EXPECT_FALSE(selectStart);
      EXPECT_FALSE(selectEnd);
      return;
    }

    ASSERT_TRUE(selection);
    ASSERT_TRUE(selectStart);
    ASSERT_TRUE(selectEnd);

    EXPECT_FALSE(selection->isNone());

    ASSERT_TRUE((*result)->IsArray());
    v8::Array& expectedResult = *v8::Array::Cast(*result);
    ASSERT_GE(expectedResult.Length(), 10u);

    blink::Node* layerOwnerNodeForStart = V8Node::toImplWithTypeCheck(
        v8::Isolate::GetCurrent(), expectedResult.Get(0));
    ASSERT_TRUE(layerOwnerNodeForStart);
    EXPECT_EQ(layerOwnerNodeForStart->layoutObject()
                  ->enclosingLayer()
                  ->enclosingLayerForPaintInvalidation()
                  ->graphicsLayerBacking()
                  ->platformLayer()
                  ->id(),
              selectStart->layerId);
    v8::Local<v8::Context> context =
        v8::Isolate::GetCurrent()->GetCurrentContext();
    EXPECT_EQ(expectedResult.Get(context, 1)
                  .ToLocalChecked()
                  .As<v8::Int32>()
                  ->Value(),
              selectStart->edgeTopInLayer.x);
    EXPECT_EQ(expectedResult.Get(context, 2)
                  .ToLocalChecked()
                  .As<v8::Int32>()
                  ->Value(),
              selectStart->edgeTopInLayer.y);
    EXPECT_EQ(expectedResult.Get(context, 3)
                  .ToLocalChecked()
                  .As<v8::Int32>()
                  ->Value(),
              selectStart->edgeBottomInLayer.x);

    blink::Node* layerOwnerNodeForEnd = V8Node::toImplWithTypeCheck(
        v8::Isolate::GetCurrent(),
        expectedResult.Get(context, 5).ToLocalChecked());

    ASSERT_TRUE(layerOwnerNodeForEnd);
    EXPECT_EQ(layerOwnerNodeForEnd->layoutObject()
                  ->enclosingLayer()
                  ->enclosingLayerForPaintInvalidation()
                  ->graphicsLayerBacking()
                  ->platformLayer()
                  ->id(),
              selectEnd->layerId);
    EXPECT_EQ(expectedResult.Get(context, 6)
                  .ToLocalChecked()
                  .As<v8::Int32>()
                  ->Value(),
              selectEnd->edgeTopInLayer.x);
    EXPECT_EQ(expectedResult.Get(context, 7)
                  .ToLocalChecked()
                  .As<v8::Int32>()
                  ->Value(),
              selectEnd->edgeTopInLayer.y);
    EXPECT_EQ(expectedResult.Get(context, 8)
                  .ToLocalChecked()
                  .As<v8::Int32>()
                  ->Value(),
              selectEnd->edgeBottomInLayer.x);

    // Platform differences can introduce small stylistic deviations in
    // y-axis positioning, the details of which aren't relevant to
    // selection behavior. However, such deviations from the expected value
    // should be consistent for the corresponding y coordinates.
    int yBottomEpsilon = 0;
    if (expectedResult.Length() == 13)
      yBottomEpsilon = expectedResult.Get(context, 12)
                           .ToLocalChecked()
                           .As<v8::Int32>()
                           ->Value();
    int yBottomDeviation = expectedResult.Get(context, 4)
                               .ToLocalChecked()
                               .As<v8::Int32>()
                               ->Value() -
                           selectStart->edgeBottomInLayer.y;
    EXPECT_GE(yBottomEpsilon, std::abs(yBottomDeviation));
    EXPECT_EQ(yBottomDeviation, expectedResult.Get(context, 9)
                                        .ToLocalChecked()
                                        .As<v8::Int32>()
                                        ->Value() -
                                    selectEnd->edgeBottomInLayer.y);

    if (expectedResult.Length() >= 12) {
      EXPECT_EQ(expectedResult.Get(context, 10)
                    .ToLocalChecked()
                    .As<v8::Boolean>()
                    ->Value(),
                m_fakeSelectionLayerTreeView.selection()->isEditable());
      EXPECT_EQ(
          expectedResult.Get(context, 11)
              .ToLocalChecked()
              .As<v8::Boolean>()
              ->Value(),
          m_fakeSelectionLayerTreeView.selection()->isEmptyTextFormControl());
    }
  }

  void runTestWithMultipleFiles(const char* testFile, ...) {
    va_list auxFiles;
    va_start(auxFiles, testFile);
    while (const char* auxFile = va_arg(auxFiles, const char*))
      registerMockedHttpURLLoad(auxFile);
    va_end(auxFiles);

    runTest(testFile);
  }

  CompositedSelectionBoundsTestWebViewClient m_fakeSelectionWebViewClient;
  CompositedSelectionBoundsTestLayerTreeView& m_fakeSelectionLayerTreeView;
  FrameTestHelpers::WebViewHelper m_webViewHelper;
};

TEST_F(CompositedSelectionBoundsTest, None) {
  runTest("composited_selection_bounds_none.html");
}
TEST_F(CompositedSelectionBoundsTest, NoneReadonlyCaret) {
  runTest("composited_selection_bounds_none_readonly_caret.html");
}
TEST_F(CompositedSelectionBoundsTest, Basic) {
  runTest("composited_selection_bounds_basic.html");
}
TEST_F(CompositedSelectionBoundsTest, Transformed) {
  runTest("composited_selection_bounds_transformed.html");
}
TEST_F(CompositedSelectionBoundsTest, VerticalRightToLeft) {
  runTest("composited_selection_bounds_vertical_rl.html");
}
TEST_F(CompositedSelectionBoundsTest, VerticalLeftToRight) {
  runTest("composited_selection_bounds_vertical_lr.html");
}
TEST_F(CompositedSelectionBoundsTest, SplitLayer) {
  runTest("composited_selection_bounds_split_layer.html");
}
TEST_F(CompositedSelectionBoundsTest, EmptyLayer) {
  runTest("composited_selection_bounds_empty_layer.html");
}
TEST_F(CompositedSelectionBoundsTest, Iframe) {
  runTestWithMultipleFiles("composited_selection_bounds_iframe.html",
                           "composited_selection_bounds_basic.html", nullptr);
}
TEST_F(CompositedSelectionBoundsTest, DetachedFrame) {
  runTest("composited_selection_bounds_detached_frame.html");
}
TEST_F(CompositedSelectionBoundsTest, Editable) {
  runTest("composited_selection_bounds_editable.html");
}
TEST_F(CompositedSelectionBoundsTest, EditableDiv) {
  runTest("composited_selection_bounds_editable_div.html");
}
TEST_F(CompositedSelectionBoundsTest, EmptyEditableInput) {
  runTest("composited_selection_bounds_empty_editable_input.html");
}
TEST_F(CompositedSelectionBoundsTest, EmptyEditableArea) {
  runTest("composited_selection_bounds_empty_editable_area.html");
}

// Fails on Mac ASan 64 bot. https://crbug.com/588769.
#if OS(MACOSX) && defined(ADDRESS_SANITIZER)
TEST_P(ParameterizedWebFrameTest, DISABLED_CompositedSelectionBoundsCleared)
#else
TEST_P(ParameterizedWebFrameTest, CompositedSelectionBoundsCleared)
#endif
{
  RuntimeEnabledFeatures::setCompositedSelectionUpdateEnabled(true);

  registerMockedHttpURLLoad("select_range_basic.html");
  registerMockedHttpURLLoad("select_range_scroll.html");

  int viewWidth = 500;
  int viewHeight = 500;

  CompositedSelectionBoundsTestWebViewClient fakeSelectionWebViewClient;
  CompositedSelectionBoundsTestLayerTreeView& fakeSelectionLayerTreeView =
      fakeSelectionWebViewClient.selectionLayerTreeView();

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(true, nullptr, &fakeSelectionWebViewClient, nullptr);
  webViewHelper.webView()->settings()->setDefaultFontSize(12);
  webViewHelper.webView()->setDefaultPageScaleLimits(1, 1);
  webViewHelper.resize(WebSize(viewWidth, viewHeight));
  webViewHelper.webView()->setFocus(true);
  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              m_baseURL + "select_range_basic.html");

  // The frame starts with no selection.
  WebLocalFrame* frame = webViewHelper.webView()->mainFrameImpl();
  ASSERT_TRUE(frame->hasSelection());
  EXPECT_TRUE(fakeSelectionLayerTreeView.getAndResetSelectionCleared());

  // The selection cleared notification should be triggered upon layout.
  frame->executeCommand(WebString::fromUTF8("Unselect"));
  ASSERT_FALSE(frame->hasSelection());
  EXPECT_FALSE(fakeSelectionLayerTreeView.getAndResetSelectionCleared());
  webViewHelper.webView()->updateAllLifecyclePhases();
  EXPECT_TRUE(fakeSelectionLayerTreeView.getAndResetSelectionCleared());

  frame->executeCommand(WebString::fromUTF8("SelectAll"));
  webViewHelper.webView()->updateAllLifecyclePhases();
  ASSERT_TRUE(frame->hasSelection());
  EXPECT_FALSE(fakeSelectionLayerTreeView.getAndResetSelectionCleared());

  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              m_baseURL + "select_range_scroll.html");
  ASSERT_TRUE(frame->hasSelection());
  EXPECT_FALSE(fakeSelectionLayerTreeView.getAndResetSelectionCleared());

  // Transitions between non-empty selections should not trigger a clearing.
  WebRect startWebRect;
  WebRect endWebRect;
  webViewHelper.webView()->selectionBounds(startWebRect, endWebRect);
  WebPoint movedEnd(bottomRightMinusOne(endWebRect));
  endWebRect.x -= 20;
  frame->selectRange(topLeft(startWebRect), movedEnd);
  webViewHelper.webView()->updateAllLifecyclePhases();
  ASSERT_TRUE(frame->hasSelection());
  EXPECT_FALSE(fakeSelectionLayerTreeView.getAndResetSelectionCleared());

  frame = webViewHelper.webView()->mainFrameImpl();
  frame->executeCommand(WebString::fromUTF8("Unselect"));
  webViewHelper.webView()->updateAllLifecyclePhases();
  ASSERT_FALSE(frame->hasSelection());
  EXPECT_TRUE(fakeSelectionLayerTreeView.getAndResetSelectionCleared());
}

class DisambiguationPopupTestWebViewClient
    : public FrameTestHelpers::TestWebViewClient {
 public:
  bool didTapMultipleTargets(const WebSize&,
                             const WebRect&,
                             const WebVector<WebRect>& targetRects) override {
    EXPECT_GE(targetRects.size(), 2u);
    m_triggered = true;
    return true;
  }

  bool triggered() const { return m_triggered; }
  void resetTriggered() { m_triggered = false; }
  bool m_triggered;
};

static WebGestureEvent fatTap(int x, int y) {
  WebGestureEvent event;
  event.type = WebInputEvent::GestureTap;
  event.sourceDevice = WebGestureDeviceTouchscreen;
  event.x = x;
  event.y = y;
  event.data.tap.width = 50;
  event.data.tap.height = 50;
  return event;
}

TEST_P(ParameterizedWebFrameTest, DisambiguationPopup) {
  const std::string htmlFile = "disambiguation_popup.html";
  registerMockedHttpURLLoad(htmlFile);

  DisambiguationPopupTestWebViewClient client;

  // Make sure we initialize to minimum scale, even if the window size
  // only becomes available after the load begins.
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + htmlFile, true, 0, &client);
  webViewHelper.resize(WebSize(1000, 1000));

  client.resetTriggered();
  webViewHelper.webView()->handleInputEvent(fatTap(0, 0));
  EXPECT_FALSE(client.triggered());

  client.resetTriggered();
  webViewHelper.webView()->handleInputEvent(fatTap(200, 115));
  EXPECT_FALSE(client.triggered());

  for (int i = 0; i <= 46; i++) {
    client.resetTriggered();
    webViewHelper.webView()->handleInputEvent(fatTap(120, 230 + i * 5));

    int j = i % 10;
    if (j >= 7 && j <= 9)
      EXPECT_TRUE(client.triggered());
    else
      EXPECT_FALSE(client.triggered());
  }

  for (int i = 0; i <= 46; i++) {
    client.resetTriggered();
    webViewHelper.webView()->handleInputEvent(fatTap(10 + i * 5, 590));

    int j = i % 10;
    if (j >= 7 && j <= 9)
      EXPECT_TRUE(client.triggered());
    else
      EXPECT_FALSE(client.triggered());
  }

  // The same taps shouldn't trigger didTapMultipleTargets() after disabling the
  // notification for multi-target-tap.
  webViewHelper.webView()->settings()->setMultiTargetTapNotificationEnabled(
      false);

  for (int i = 0; i <= 46; i++) {
    client.resetTriggered();
    webViewHelper.webView()->handleInputEvent(fatTap(10 + i * 5, 590));
    EXPECT_FALSE(client.triggered());
  }
}

TEST_P(ParameterizedWebFrameTest, DisambiguationPopupNoContainer) {
  registerMockedHttpURLLoad("disambiguation_popup_no_container.html");

  DisambiguationPopupTestWebViewClient client;

  // Make sure we initialize to minimum scale, even if the window size
  // only becomes available after the load begins.
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(
      m_baseURL + "disambiguation_popup_no_container.html", true, 0, &client);
  webViewHelper.resize(WebSize(1000, 1000));

  client.resetTriggered();
  webViewHelper.webView()->handleInputEvent(fatTap(50, 50));
  EXPECT_FALSE(client.triggered());
}

TEST_P(ParameterizedWebFrameTest, DisambiguationPopupMobileSite) {
  const std::string htmlFile = "disambiguation_popup_mobile_site.html";
  registerMockedHttpURLLoad(htmlFile);

  DisambiguationPopupTestWebViewClient client;

  // Make sure we initialize to minimum scale, even if the window size
  // only becomes available after the load begins.
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + htmlFile, true, nullptr, &client,
                                  nullptr, enableViewportSettings);
  webViewHelper.resize(WebSize(1000, 1000));

  client.resetTriggered();
  webViewHelper.webView()->handleInputEvent(fatTap(0, 0));
  EXPECT_FALSE(client.triggered());

  client.resetTriggered();
  webViewHelper.webView()->handleInputEvent(fatTap(200, 115));
  EXPECT_FALSE(client.triggered());

  for (int i = 0; i <= 46; i++) {
    client.resetTriggered();
    webViewHelper.webView()->handleInputEvent(fatTap(120, 230 + i * 5));
    EXPECT_FALSE(client.triggered());
  }

  for (int i = 0; i <= 46; i++) {
    client.resetTriggered();
    webViewHelper.webView()->handleInputEvent(fatTap(10 + i * 5, 590));
    EXPECT_FALSE(client.triggered());
  }
}

TEST_P(ParameterizedWebFrameTest, DisambiguationPopupViewportSite) {
  const std::string htmlFile = "disambiguation_popup_viewport_site.html";
  registerMockedHttpURLLoad(htmlFile);

  DisambiguationPopupTestWebViewClient client;

  // Make sure we initialize to minimum scale, even if the window size
  // only becomes available after the load begins.
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + htmlFile, true, nullptr, &client,
                                  nullptr, enableViewportSettings);
  webViewHelper.resize(WebSize(1000, 1000));

  client.resetTriggered();
  webViewHelper.webView()->handleInputEvent(fatTap(0, 0));
  EXPECT_FALSE(client.triggered());

  client.resetTriggered();
  webViewHelper.webView()->handleInputEvent(fatTap(200, 115));
  EXPECT_FALSE(client.triggered());

  for (int i = 0; i <= 46; i++) {
    client.resetTriggered();
    webViewHelper.webView()->handleInputEvent(fatTap(120, 230 + i * 5));
    EXPECT_FALSE(client.triggered());
  }

  for (int i = 0; i <= 46; i++) {
    client.resetTriggered();
    webViewHelper.webView()->handleInputEvent(fatTap(10 + i * 5, 590));
    EXPECT_FALSE(client.triggered());
  }
}

TEST_F(WebFrameTest, DisambiguationPopupVisualViewport) {
  const std::string htmlFile = "disambiguation_popup_200_by_800.html";
  registerMockedHttpURLLoad(htmlFile);

  DisambiguationPopupTestWebViewClient client;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + htmlFile, true, nullptr, &client,
                                  nullptr, configureAndroid);

  WebViewImpl* webViewImpl = webViewHelper.webView();
  ASSERT_TRUE(webViewImpl);
  LocalFrame* frame = webViewImpl->mainFrameImpl()->frame();
  ASSERT_TRUE(frame);

  webViewHelper.resize(WebSize(100, 200));

  // Scroll main frame to the bottom of the document
  webViewImpl->mainFrame()->setScrollOffset(WebSize(0, 400));
  EXPECT_SIZE_EQ(ScrollOffset(0, 400), frame->view()->scrollOffset());

  webViewImpl->setPageScaleFactor(2.0);

  // Scroll visual viewport to the top of the main frame.
  VisualViewport& visualViewport = frame->page()->frameHost().visualViewport();
  visualViewport.setLocation(FloatPoint(0, 0));
  EXPECT_SIZE_EQ(ScrollOffset(0, 0), visualViewport.scrollOffset());

  // Tap at the top: there is nothing there.
  client.resetTriggered();
  webViewHelper.webView()->handleInputEvent(fatTap(10, 60));
  EXPECT_FALSE(client.triggered());

  // Scroll visual viewport to the bottom of the main frame.
  visualViewport.setLocation(FloatPoint(0, 200));
  EXPECT_SIZE_EQ(ScrollOffset(0, 200), visualViewport.scrollOffset());

  // Now the tap with the same coordinates should hit two elements.
  client.resetTriggered();
  webViewHelper.webView()->handleInputEvent(fatTap(10, 60));
  EXPECT_TRUE(client.triggered());

  // The same tap shouldn't trigger didTapMultipleTargets() after disabling the
  // notification for multi-target-tap.
  webViewHelper.webView()->settings()->setMultiTargetTapNotificationEnabled(
      false);
  client.resetTriggered();
  webViewHelper.webView()->handleInputEvent(fatTap(10, 60));
  EXPECT_FALSE(client.triggered());
}

TEST_P(ParameterizedWebFrameTest, DisambiguationPopupBlacklist) {
  const unsigned viewportWidth = 500;
  const unsigned viewportHeight = 1000;
  const unsigned divHeight = 100;
  const std::string htmlFile = "disambiguation_popup_blacklist.html";
  registerMockedHttpURLLoad(htmlFile);

  DisambiguationPopupTestWebViewClient client;

  // Make sure we initialize to minimum scale, even if the window size
  // only becomes available after the load begins.
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + htmlFile, true, 0, &client);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));

  // Click somewhere where the popup shouldn't appear.
  client.resetTriggered();
  webViewHelper.webView()->handleInputEvent(fatTap(viewportWidth / 2, 0));
  EXPECT_FALSE(client.triggered());

  // Click directly in between two container divs with click handlers, with
  // children that don't handle clicks.
  client.resetTriggered();
  webViewHelper.webView()->handleInputEvent(
      fatTap(viewportWidth / 2, divHeight));
  EXPECT_TRUE(client.triggered());

  // The third div container should be blacklisted if you click on the link it
  // contains.
  client.resetTriggered();
  webViewHelper.webView()->handleInputEvent(
      fatTap(viewportWidth / 2, divHeight * 3.25));
  EXPECT_FALSE(client.triggered());
}

TEST_P(ParameterizedWebFrameTest, DisambiguationPopupPageScale) {
  registerMockedHttpURLLoad("disambiguation_popup_page_scale.html");

  DisambiguationPopupTestWebViewClient client;

  // Make sure we initialize to minimum scale, even if the window size
  // only becomes available after the load begins.
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(
      m_baseURL + "disambiguation_popup_page_scale.html", true, 0, &client);
  webViewHelper.resize(WebSize(1000, 1000));

  client.resetTriggered();
  webViewHelper.webView()->handleInputEvent(fatTap(80, 80));
  EXPECT_TRUE(client.triggered());

  client.resetTriggered();
  webViewHelper.webView()->handleInputEvent(fatTap(230, 190));
  EXPECT_TRUE(client.triggered());

  webViewHelper.webView()->setPageScaleFactor(3.0f);
  webViewHelper.webView()->updateAllLifecyclePhases();

  client.resetTriggered();
  webViewHelper.webView()->handleInputEvent(fatTap(240, 240));
  EXPECT_TRUE(client.triggered());

  client.resetTriggered();
  webViewHelper.webView()->handleInputEvent(fatTap(690, 570));
  EXPECT_FALSE(client.triggered());
}

class TestSubstituteDataWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  TestSubstituteDataWebFrameClient() : m_commitCalled(false) {}

  virtual void didFailProvisionalLoad(WebLocalFrame* frame,
                                      const WebURLError& error,
                                      WebHistoryCommitType) {
    frame->loadHTMLString("This should appear",
                          toKURL("data:text/html,chromewebdata"),
                          error.unreachableURL, true);
  }

  virtual void didCommitProvisionalLoad(WebLocalFrame* frame,
                                        const WebHistoryItem&,
                                        WebHistoryCommitType) {
    if (frame->dataSource()->response().url() !=
        WebURL(URLTestHelpers::toKURL("about:blank")))
      m_commitCalled = true;
  }

  bool commitCalled() const { return m_commitCalled; }

 private:
  bool m_commitCalled;
};

TEST_P(ParameterizedWebFrameTest, ReplaceNavigationAfterHistoryNavigation) {
  TestSubstituteDataWebFrameClient webFrameClient;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad("about:blank", true, &webFrameClient);
  WebFrame* frame = webViewHelper.webView()->mainFrame();

  // Load a url as a history navigation that will return an error.
  // TestSubstituteDataWebFrameClient will start a SubstituteData load in
  // response to the load failure, which should get fully committed.  Due to
  // https://bugs.webkit.org/show_bug.cgi?id=91685,
  // FrameLoader::didReceiveData() wasn't getting called in this case, which
  // resulted in the SubstituteData document not getting displayed.
  WebURLError error;
  error.reason = 1337;
  error.domain = "WebFrameTest";
  std::string errorURL = "http://0.0.0.0";
  WebURLResponse response;
  response.setURL(URLTestHelpers::toKURL(errorURL));
  response.setMIMEType("text/html");
  response.setHTTPStatusCode(500);
  WebHistoryItem errorHistoryItem;
  errorHistoryItem.initialize();
  errorHistoryItem.setURLString(
      WebString::fromUTF8(errorURL.c_str(), errorURL.length()));
  Platform::current()->getURLLoaderMockFactory()->registerErrorURL(
      URLTestHelpers::toKURL(errorURL), response, error);
  FrameTestHelpers::loadHistoryItem(frame, errorHistoryItem,
                                    WebHistoryDifferentDocumentLoad,
                                    WebCachePolicy::UseProtocolCachePolicy);
  WebString text = WebFrameContentDumper::dumpWebViewAsText(
      webViewHelper.webView(), std::numeric_limits<size_t>::max());
  EXPECT_EQ("This should appear", text.utf8());
  EXPECT_TRUE(webFrameClient.commitCalled());
}

class TestWillInsertBodyWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  TestWillInsertBodyWebFrameClient() : m_numBodies(0), m_didLoad(false) {}

  void didCommitProvisionalLoad(WebLocalFrame*,
                                const WebHistoryItem&,
                                WebHistoryCommitType) override {
    m_numBodies = 0;
    m_didLoad = true;
  }

  void didCreateDocumentElement(WebLocalFrame*) override {
    EXPECT_EQ(0, m_numBodies);
  }

  void willInsertBody(WebLocalFrame*) override { m_numBodies++; }

  int m_numBodies;
  bool m_didLoad;
};

TEST_P(ParameterizedWebFrameTest, HTMLDocument) {
  registerMockedHttpURLLoad("clipped-body.html");

  TestWillInsertBodyWebFrameClient webFrameClient;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "clipped-body.html", false,
                                  &webFrameClient);

  EXPECT_TRUE(webFrameClient.m_didLoad);
  EXPECT_EQ(1, webFrameClient.m_numBodies);
}

TEST_P(ParameterizedWebFrameTest, EmptyDocument) {
  registerMockedHttpURLLoad("frameserializer/svg/green_rectangle.svg");

  TestWillInsertBodyWebFrameClient webFrameClient;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(false, &webFrameClient);

  EXPECT_FALSE(webFrameClient.m_didLoad);
  // The empty document that a new frame starts with triggers this.
  EXPECT_EQ(1, webFrameClient.m_numBodies);
}

TEST_P(ParameterizedWebFrameTest,
       MoveCaretSelectionTowardsWindowPointWithNoSelection) {
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad("about:blank", true);
  WebFrame* frame = webViewHelper.webView()->mainFrame();

  // This test passes if this doesn't crash.
  frame->toWebLocalFrame()->moveCaretSelection(WebPoint(0, 0));
}

class SpellCheckClient : public WebSpellCheckClient {
 public:
  explicit SpellCheckClient(uint32_t hash = 0)
      : m_numberOfTimesChecked(0), m_hash(hash) {}
  virtual ~SpellCheckClient() {}
  void requestCheckingOfText(const WebString&,
                             const WebVector<uint32_t>&,
                             const WebVector<unsigned>&,
                             WebTextCheckingCompletion* completion) override {
    ++m_numberOfTimesChecked;
    Vector<WebTextCheckingResult> results;
    const int misspellingStartOffset = 1;
    const int misspellingLength = 8;
    results.append(WebTextCheckingResult(
        WebTextDecorationTypeSpelling, misspellingStartOffset,
        misspellingLength, WebString(), m_hash));
    completion->didFinishCheckingText(results);
  }
  int numberOfTimesChecked() const { return m_numberOfTimesChecked; }

 private:
  int m_numberOfTimesChecked;
  uint32_t m_hash;
};

TEST_P(ParameterizedWebFrameTest, ReplaceMisspelledRange) {
  registerMockedHttpURLLoad("spell.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "spell.html");
  SpellCheckClient spellcheck;
  webViewHelper.webView()->setSpellCheckClient(&spellcheck);

  WebLocalFrameImpl* frame = webViewHelper.webView()->mainFrameImpl();
  Document* document = frame->frame()->document();
  Element* element = document->getElementById("data");

  webViewHelper.webView()->settings()->setEditingBehavior(
      WebSettings::EditingBehaviorWin);

  element->focus();
  NonThrowableExceptionState exceptionState;
  document->execCommand("InsertText", false, "_wellcome_.", exceptionState);
  EXPECT_FALSE(exceptionState.hadException());

  const int allTextBeginOffset = 0;
  const int allTextLength = 11;
  frame->selectRange(WebRange(allTextBeginOffset, allTextLength));
  EphemeralRange selectionRange =
      frame->frame()->selection().selection().toNormalizedEphemeralRange();

  EXPECT_EQ(1, spellcheck.numberOfTimesChecked());
  EXPECT_EQ(1U, document->markers()
                    .markersInRange(selectionRange, DocumentMarker::Spelling)
                    .size());

  frame->replaceMisspelledRange("welcome");
  EXPECT_EQ("_welcome_.",
            WebFrameContentDumper::dumpWebViewAsText(
                webViewHelper.webView(), std::numeric_limits<size_t>::max())
                .utf8());
}

TEST_P(ParameterizedWebFrameTest, RemoveSpellingMarkers) {
  registerMockedHttpURLLoad("spell.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "spell.html");
  SpellCheckClient spellcheck;
  webViewHelper.webView()->setSpellCheckClient(&spellcheck);

  WebLocalFrameImpl* frame = webViewHelper.webView()->mainFrameImpl();
  Document* document = frame->frame()->document();
  Element* element = document->getElementById("data");

  webViewHelper.webView()->settings()->setEditingBehavior(
      WebSettings::EditingBehaviorWin);

  element->focus();
  NonThrowableExceptionState exceptionState;
  document->execCommand("InsertText", false, "_wellcome_.", exceptionState);
  EXPECT_FALSE(exceptionState.hadException());

  frame->removeSpellingMarkers();

  const int allTextBeginOffset = 0;
  const int allTextLength = 11;
  frame->selectRange(WebRange(allTextBeginOffset, allTextLength));
  EphemeralRange selectionRange =
      frame->frame()->selection().selection().toNormalizedEphemeralRange();

  EXPECT_EQ(0U, document->markers()
                    .markersInRange(selectionRange, DocumentMarker::Spelling)
                    .size());
}

TEST_P(ParameterizedWebFrameTest, RemoveSpellingMarkersUnderWords) {
  registerMockedHttpURLLoad("spell.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "spell.html");
  SpellCheckClient spellcheck;
  webViewHelper.webView()->setSpellCheckClient(&spellcheck);

  LocalFrame* frame = webViewHelper.webView()->mainFrameImpl()->frame();
  Document* document = frame->document();
  Element* element = document->getElementById("data");

  webViewHelper.webView()->settings()->setEditingBehavior(
      WebSettings::EditingBehaviorWin);

  element->focus();
  NonThrowableExceptionState exceptionState;
  document->execCommand("InsertText", false, " wellcome ", exceptionState);
  EXPECT_FALSE(exceptionState.hadException());

  WebVector<uint32_t> documentMarkers1;
  webViewHelper.webView()->spellingMarkers(&documentMarkers1);
  EXPECT_EQ(1U, documentMarkers1.size());

  Vector<String> words;
  words.append("wellcome");
  frame->removeSpellingMarkersUnderWords(words);

  WebVector<uint32_t> documentMarkers2;
  webViewHelper.webView()->spellingMarkers(&documentMarkers2);
  EXPECT_EQ(0U, documentMarkers2.size());
}

TEST_P(ParameterizedWebFrameTest, MarkerHashIdentifiers) {
  registerMockedHttpURLLoad("spell.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "spell.html");

  static const uint32_t kHash = 42;
  SpellCheckClient spellcheck(kHash);
  webViewHelper.webView()->setSpellCheckClient(&spellcheck);

  WebLocalFrameImpl* frame = webViewHelper.webView()->mainFrameImpl();
  Document* document = frame->frame()->document();
  Element* element = document->getElementById("data");

  webViewHelper.webView()->settings()->setEditingBehavior(
      WebSettings::EditingBehaviorWin);

  element->focus();
  NonThrowableExceptionState exceptionState;
  document->execCommand("InsertText", false, "wellcome.", exceptionState);
  EXPECT_FALSE(exceptionState.hadException());

  WebVector<uint32_t> documentMarkers;
  webViewHelper.webView()->spellingMarkers(&documentMarkers);
  EXPECT_EQ(1U, documentMarkers.size());
  EXPECT_EQ(kHash, documentMarkers[0]);
}

class StubbornSpellCheckClient : public WebSpellCheckClient {
 public:
  StubbornSpellCheckClient() : m_completion(0) {}
  virtual ~StubbornSpellCheckClient() {}

  virtual void requestCheckingOfText(
      const WebString&,
      const WebVector<uint32_t>&,
      const WebVector<unsigned>&,
      WebTextCheckingCompletion* completion) override {
    m_completion = completion;
  }

  void cancelAllPendingRequests() override {
    if (!m_completion)
      return;
    m_completion->didCancelCheckingText();
    m_completion = nullptr;
  }

  void kickNoResults() { kick(-1, -1, WebTextDecorationTypeSpelling); }

  void kick() { kick(1, 8, WebTextDecorationTypeSpelling); }

  void kickGrammar() { kick(1, 8, WebTextDecorationTypeGrammar); }

  void kickInvisibleSpellcheck() {
    kick(1, 8, WebTextDecorationTypeInvisibleSpellcheck);
  }

 private:
  void kick(int misspellingStartOffset,
            int misspellingLength,
            WebTextDecorationType type) {
    if (!m_completion)
      return;
    Vector<WebTextCheckingResult> results;
    if (misspellingStartOffset >= 0 && misspellingLength > 0)
      results.append(WebTextCheckingResult(type, misspellingStartOffset,
                                           misspellingLength));
    m_completion->didFinishCheckingText(results);
    m_completion = 0;
  }

  WebTextCheckingCompletion* m_completion;
};

TEST_P(ParameterizedWebFrameTest, SlowSpellcheckMarkerPosition) {
  registerMockedHttpURLLoad("spell.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "spell.html");

  StubbornSpellCheckClient spellcheck;
  webViewHelper.webView()->setSpellCheckClient(&spellcheck);

  WebLocalFrameImpl* frame = webViewHelper.webView()->mainFrameImpl();
  Document* document = frame->frame()->document();
  Element* element = document->getElementById("data");

  webViewHelper.webView()->settings()->setEditingBehavior(
      WebSettings::EditingBehaviorWin);

  element->focus();
  NonThrowableExceptionState exceptionState;
  document->execCommand("InsertText", false, "wellcome ", exceptionState);
  EXPECT_FALSE(exceptionState.hadException());
  document->execCommand("InsertText", false, "he", exceptionState);
  EXPECT_FALSE(exceptionState.hadException());

  spellcheck.kick();

  WebVector<uint32_t> documentMarkers;
  webViewHelper.webView()->spellingMarkers(&documentMarkers);
  EXPECT_EQ(0U, documentMarkers.size());
}

// This test verifies that cancelling spelling request does not cause a
// write-after-free when there's no spellcheck client set.
TEST_P(ParameterizedWebFrameTest, CancelSpellingRequestCrash) {
  registerMockedHttpURLLoad("spell.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "spell.html");
  webViewHelper.webView()->setSpellCheckClient(0);

  WebLocalFrameImpl* frame = webViewHelper.webView()->mainFrameImpl();
  Document* document = frame->frame()->document();
  Element* element = document->getElementById("data");

  webViewHelper.webView()->settings()->setEditingBehavior(
      WebSettings::EditingBehaviorWin);

  element->focus();
  frame->frame()->editor().replaceSelectionWithText(
      "A", false, false, InputEvent::InputType::InsertReplacementText);
  frame->frame()->spellChecker().cancelCheck();
}

TEST_P(ParameterizedWebFrameTest, SpellcheckResultErasesMarkers) {
  registerMockedHttpURLLoad("spell.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "spell.html");

  StubbornSpellCheckClient spellcheck;
  webViewHelper.webView()->setSpellCheckClient(&spellcheck);

  WebLocalFrameImpl* frame = webViewHelper.webView()->mainFrameImpl();
  Document* document = frame->frame()->document();
  Element* element = document->getElementById("data");

  webViewHelper.webView()->settings()->setEditingBehavior(
      WebSettings::EditingBehaviorWin);

  element->focus();
  NonThrowableExceptionState exceptionState;
  document->execCommand("InsertText", false, "welcome ", exceptionState);

  document->updateStyleAndLayout();

  EXPECT_FALSE(exceptionState.hadException());
  auto range = EphemeralRange::rangeOfContents(*element);
  document->markers().addMarker(range.startPosition(), range.endPosition(),
                                DocumentMarker::Spelling);
  document->markers().addMarker(range.startPosition(), range.endPosition(),
                                DocumentMarker::Grammar);
  document->markers().addMarker(range.startPosition(), range.endPosition(),
                                DocumentMarker::InvisibleSpellcheck);
  EXPECT_EQ(3U, document->markers().markers().size());

  spellcheck.kickNoResults();
  EXPECT_EQ(0U, document->markers().markers().size());
}

TEST_P(ParameterizedWebFrameTest, SpellcheckResultsSavedInDocument) {
  registerMockedHttpURLLoad("spell.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "spell.html");

  StubbornSpellCheckClient spellcheck;
  webViewHelper.webView()->setSpellCheckClient(&spellcheck);

  WebLocalFrameImpl* frame = webViewHelper.webView()->mainFrameImpl();
  Document* document = frame->frame()->document();
  Element* element = document->getElementById("data");

  webViewHelper.webView()->settings()->setEditingBehavior(
      WebSettings::EditingBehaviorWin);

  element->focus();
  NonThrowableExceptionState exceptionState;
  document->execCommand("InsertText", false, "wellcome ", exceptionState);
  EXPECT_FALSE(exceptionState.hadException());

  spellcheck.kick();
  ASSERT_EQ(1U, document->markers().markers().size());
  ASSERT_NE(static_cast<DocumentMarker*>(0), document->markers().markers()[0]);
  EXPECT_EQ(DocumentMarker::Spelling, document->markers().markers()[0]->type());

  document->execCommand("InsertText", false, "wellcome ", exceptionState);
  EXPECT_FALSE(exceptionState.hadException());

  spellcheck.kickGrammar();
  ASSERT_EQ(1U, document->markers().markers().size());
  ASSERT_NE(static_cast<DocumentMarker*>(0), document->markers().markers()[0]);
  EXPECT_EQ(DocumentMarker::Grammar, document->markers().markers()[0]->type());

  document->execCommand("InsertText", false, "wellcome ", exceptionState);
  EXPECT_FALSE(exceptionState.hadException());

  spellcheck.kickInvisibleSpellcheck();
  ASSERT_EQ(1U, document->markers().markers().size());
  ASSERT_NE(static_cast<DocumentMarker*>(0), document->markers().markers()[0]);
  EXPECT_EQ(DocumentMarker::InvisibleSpellcheck,
            document->markers().markers()[0]->type());
}

class TestAccessInitialDocumentWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  TestAccessInitialDocumentWebFrameClient()
      : m_didAccessInitialDocument(false) {}

  virtual void didAccessInitialDocument() { m_didAccessInitialDocument = true; }

  bool m_didAccessInitialDocument;
};

TEST_P(ParameterizedWebFrameTest, DidAccessInitialDocumentBody) {
  // FIXME: Why is this local webViewClient needed instead of the default
  // WebViewHelper one? With out it there's some mysterious crash in the
  // WebViewHelper destructor.
  FrameTestHelpers::TestWebViewClient webViewClient;
  TestAccessInitialDocumentWebFrameClient webFrameClient;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(true, &webFrameClient, &webViewClient);
  runPendingTasks();
  EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

  // Create another window that will try to access it.
  FrameTestHelpers::WebViewHelper newWebViewHelper;
  WebView* newView = newWebViewHelper.initializeWithOpener(
      webViewHelper.webView()->mainFrame(), true);
  runPendingTasks();
  EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

  // Access the initial document by modifying the body.
  newView->mainFrame()->executeScript(
      WebScriptSource("window.opener.document.body.innerHTML += 'Modified';"));
  runPendingTasks();
  EXPECT_TRUE(webFrameClient.m_didAccessInitialDocument);

  // Access the initial document again, to ensure we don't notify twice.
  newView->mainFrame()->executeScript(
      WebScriptSource("window.opener.document.body.innerHTML += 'Modified';"));
  runPendingTasks();
  EXPECT_TRUE(webFrameClient.m_didAccessInitialDocument);
}

TEST_P(ParameterizedWebFrameTest, DidAccessInitialDocumentNavigator) {
  // FIXME: Why is this local webViewClient needed instead of the default
  // WebViewHelper one? With out it there's some mysterious crash in the
  // WebViewHelper destructor.
  FrameTestHelpers::TestWebViewClient webViewClient;
  TestAccessInitialDocumentWebFrameClient webFrameClient;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(true, &webFrameClient, &webViewClient);
  runPendingTasks();
  EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

  // Create another window that will try to access it.
  FrameTestHelpers::WebViewHelper newWebViewHelper;
  WebView* newView = newWebViewHelper.initializeWithOpener(
      webViewHelper.webView()->mainFrame(), true);
  runPendingTasks();
  EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

  // Access the initial document to get to the navigator object.
  newView->mainFrame()->executeScript(
      WebScriptSource("console.log(window.opener.navigator);"));
  runPendingTasks();
  EXPECT_TRUE(webFrameClient.m_didAccessInitialDocument);
}

TEST_P(ParameterizedWebFrameTest, DidAccessInitialDocumentViaJavascriptUrl) {
  TestAccessInitialDocumentWebFrameClient webFrameClient;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(true, &webFrameClient);
  runPendingTasks();
  EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

  // Access the initial document from a javascript: URL.
  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              "javascript:document.body.appendChild(document."
                              "createTextNode('Modified'))");
  EXPECT_TRUE(webFrameClient.m_didAccessInitialDocument);
}

// Fails on the WebKit XP (deps) bot. http://crbug.com/312192
#if OS(WIN)
TEST_P(ParameterizedWebFrameTest,
       DISABLED_DidAccessInitialDocumentBodyBeforeModalDialog)
#else
TEST_P(ParameterizedWebFrameTest, DidAccessInitialDocumentBodyBeforeModalDialog)
#endif
{
  // FIXME: Why is this local webViewClient needed instead of the default
  // WebViewHelper one? With out it there's some mysterious crash in the
  // WebViewHelper destructor.
  FrameTestHelpers::TestWebViewClient webViewClient;
  TestAccessInitialDocumentWebFrameClient webFrameClient;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(true, &webFrameClient, &webViewClient);
  runPendingTasks();
  EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

  // Create another window that will try to access it.
  FrameTestHelpers::WebViewHelper newWebViewHelper;
  WebView* newView = newWebViewHelper.initializeWithOpener(
      webViewHelper.webView()->mainFrame(), true);
  runPendingTasks();
  EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

  // Access the initial document by modifying the body.
  newView->mainFrame()->executeScript(
      WebScriptSource("window.opener.document.body.innerHTML += 'Modified';"));
  EXPECT_TRUE(webFrameClient.m_didAccessInitialDocument);

  // Run a modal dialog, which used to run a nested message loop and require
  // a special case for notifying about the access.
  newView->mainFrame()->executeScript(
      WebScriptSource("window.opener.confirm('Modal');"));
  EXPECT_TRUE(webFrameClient.m_didAccessInitialDocument);

  // Ensure that we don't notify again later.
  runPendingTasks();
  EXPECT_TRUE(webFrameClient.m_didAccessInitialDocument);
}

// Fails on the WebKit XP (deps) bot. http://crbug.com/312192
#if OS(WIN)
TEST_P(ParameterizedWebFrameTest,
       DISABLED_DidWriteToInitialDocumentBeforeModalDialog)
#else
TEST_P(ParameterizedWebFrameTest, DidWriteToInitialDocumentBeforeModalDialog)
#endif
{
  // FIXME: Why is this local webViewClient needed instead of the default
  // WebViewHelper one? With out it there's some mysterious crash in the
  // WebViewHelper destructor.
  FrameTestHelpers::TestWebViewClient webViewClient;
  TestAccessInitialDocumentWebFrameClient webFrameClient;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(true, &webFrameClient, &webViewClient);
  runPendingTasks();
  EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

  // Create another window that will try to access it.
  FrameTestHelpers::WebViewHelper newWebViewHelper;
  WebView* newView = newWebViewHelper.initializeWithOpener(
      webViewHelper.webView()->mainFrame(), true);
  runPendingTasks();
  EXPECT_FALSE(webFrameClient.m_didAccessInitialDocument);

  // Access the initial document with document.write, which moves us past the
  // initial empty document state of the state machine.
  newView->mainFrame()->executeScript(
      WebScriptSource("window.opener.document.write('Modified'); "
                      "window.opener.document.close();"));
  EXPECT_TRUE(webFrameClient.m_didAccessInitialDocument);

  // Run a modal dialog, which used to run a nested message loop and require
  // a special case for notifying about the access.
  newView->mainFrame()->executeScript(
      WebScriptSource("window.opener.confirm('Modal');"));
  EXPECT_TRUE(webFrameClient.m_didAccessInitialDocument);

  // Ensure that we don't notify again later.
  runPendingTasks();
  EXPECT_TRUE(webFrameClient.m_didAccessInitialDocument);
}

class TestScrolledFrameClient : public FrameTestHelpers::TestWebFrameClient {
 public:
  TestScrolledFrameClient() { reset(); }
  void reset() { m_didScrollFrame = false; }
  bool wasFrameScrolled() const { return m_didScrollFrame; }

  // WebFrameClient:
  void didChangeScrollOffset(WebLocalFrame* frame) override {
    if (frame->parent())
      return;
    EXPECT_FALSE(m_didScrollFrame);
    FrameView* view = toWebLocalFrameImpl(frame)->frameView();
    // FrameView can be scrolled in FrameView::setFixedVisibleContentRect which
    // is called from LocalFrame::createView (before the frame is associated
    // with the the view).
    if (view)
      m_didScrollFrame = true;
  }

 private:
  bool m_didScrollFrame;
};

TEST_F(WebFrameTest, CompositorScrollIsUserScrollLongPage) {
  registerMockedHttpURLLoad("long_scroll.html");
  TestScrolledFrameClient client;

  // Make sure we initialize to minimum scale, even if the window size
  // only becomes available after the load begins.
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "long_scroll.html", true,
                                  &client);
  webViewHelper.resize(WebSize(1000, 1000));

  WebLocalFrameImpl* frameImpl = webViewHelper.webView()->mainFrameImpl();
  DocumentLoader::InitialScrollState& initialScrollState =
      frameImpl->frame()->loader().documentLoader()->initialScrollState();
  GraphicsLayer* frameViewLayer = frameImpl->frameView()->layerForScrolling();

  EXPECT_FALSE(client.wasFrameScrolled());
  EXPECT_FALSE(initialScrollState.wasScrolledByUser);

  // Do a compositor scroll, verify that this is counted as a user scroll.
  frameViewLayer->platformLayer()->setScrollPositionDouble(
      WebDoublePoint(0, 1));
  frameViewLayer->didScroll();
  webViewHelper.webView()->applyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                               WebFloatSize(), 1.7f, 0);
  EXPECT_TRUE(client.wasFrameScrolled());
  EXPECT_TRUE(initialScrollState.wasScrolledByUser);

  client.reset();
  initialScrollState.wasScrolledByUser = false;

  // The page scale 1.0f and scroll.
  frameViewLayer->platformLayer()->setScrollPositionDouble(
      WebDoublePoint(0, 2));
  frameViewLayer->didScroll();
  webViewHelper.webView()->applyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                               WebFloatSize(), 1.0f, 0);
  EXPECT_TRUE(client.wasFrameScrolled());
  EXPECT_TRUE(initialScrollState.wasScrolledByUser);
  client.reset();
  initialScrollState.wasScrolledByUser = false;

  // No scroll event if there is no scroll delta.
  frameViewLayer->didScroll();
  webViewHelper.webView()->applyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                               WebFloatSize(), 1.0f, 0);
  EXPECT_FALSE(client.wasFrameScrolled());
  EXPECT_FALSE(initialScrollState.wasScrolledByUser);
  client.reset();

  // Non zero page scale and scroll.
  frameViewLayer->platformLayer()->setScrollPositionDouble(
      WebDoublePoint(9, 15));
  frameViewLayer->didScroll();
  webViewHelper.webView()->applyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                               WebFloatSize(), 0.6f, 0);
  EXPECT_TRUE(client.wasFrameScrolled());
  EXPECT_TRUE(initialScrollState.wasScrolledByUser);
  client.reset();
  initialScrollState.wasScrolledByUser = false;

  // Programmatic scroll.
  frameImpl->executeScript(WebScriptSource("window.scrollTo(0, 20);"));
  EXPECT_TRUE(client.wasFrameScrolled());
  EXPECT_FALSE(initialScrollState.wasScrolledByUser);
  client.reset();

  // Programmatic scroll to same offset. No scroll event should be generated.
  frameImpl->executeScript(WebScriptSource("window.scrollTo(0, 20);"));
  EXPECT_FALSE(client.wasFrameScrolled());
  EXPECT_FALSE(initialScrollState.wasScrolledByUser);
  client.reset();
}

TEST_P(ParameterizedWebFrameTest, FirstPartyForCookiesForRedirect) {
  String filePath = testing::blinkRootDir();
  filePath.append("/Source/web/tests/data/first_party.html");

  WebURL testURL(toKURL("http://internal.test/first_party_redirect.html"));
  char redirect[] = "http://internal.test/first_party.html";
  WebURL redirectURL(toKURL(redirect));
  WebURLResponse redirectResponse;
  redirectResponse.setMIMEType("text/html");
  redirectResponse.setHTTPStatusCode(302);
  redirectResponse.setHTTPHeaderField("Location", redirect);
  Platform::current()->getURLLoaderMockFactory()->registerURL(
      testURL, redirectResponse, filePath);

  WebURLResponse finalResponse;
  finalResponse.setMIMEType("text/html");
  Platform::current()->getURLLoaderMockFactory()->registerURL(
      redirectURL, finalResponse, filePath);

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "first_party_redirect.html",
                                  true);
  EXPECT_TRUE(
      webViewHelper.webView()->mainFrame()->document().firstPartyForCookies() ==
      redirectURL);
}

class TestNavigationPolicyWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  void didNavigateWithinPage(WebLocalFrame*,
                             const WebHistoryItem&,
                             WebHistoryCommitType,
                             bool) override {
    EXPECT_TRUE(false);
  }
};

TEST_P(ParameterizedWebFrameTest, SimulateFragmentAnchorMiddleClick) {
  registerMockedHttpURLLoad("fragment_middle_click.html");
  TestNavigationPolicyWebFrameClient client;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "fragment_middle_click.html",
                                  true, &client);

  Document* document =
      toLocalFrame(webViewHelper.webView()->page()->mainFrame())->document();
  KURL destination = document->url();
  destination.setFragmentIdentifier("test");

  Event* event = MouseEvent::create(
      EventTypeNames::click, false, false, document->domWindow(), 0, 0, 0, 0, 0,
      0, 0, PlatformEvent::NoModifiers, 1, 0, nullptr, 0,
      PlatformMouseEvent::RealOrIndistinguishable, String(), nullptr);
  FrameLoadRequest frameRequest(document, ResourceRequest(destination));
  frameRequest.setTriggeringEvent(event);
  toLocalFrame(webViewHelper.webView()->page()->mainFrame())
      ->loader()
      .load(frameRequest);
}

class TestNewWindowWebViewClient : public FrameTestHelpers::TestWebViewClient {
 public:
  virtual WebView* createView(WebLocalFrame*,
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
  TestNewWindowWebFrameClient() : m_decidePolicyCallCount(0) {}

  WebNavigationPolicy decidePolicyForNavigation(
      const NavigationPolicyInfo& info) override {
    m_decidePolicyCallCount++;
    return info.defaultPolicy;
  }

  int decidePolicyCallCount() const { return m_decidePolicyCallCount; }

 private:
  int m_decidePolicyCallCount;
};

TEST_P(ParameterizedWebFrameTest, ModifiedClickNewWindow) {
  registerMockedHttpURLLoad("ctrl_click.html");
  registerMockedHttpURLLoad("hello_world.html");
  TestNewWindowWebViewClient webViewClient;
  TestNewWindowWebFrameClient webFrameClient;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "ctrl_click.html", true,
                                  &webFrameClient, &webViewClient);

  Document* document =
      toLocalFrame(webViewHelper.webView()->page()->mainFrame())->document();
  KURL destination = toKURL(m_baseURL + "hello_world.html");

  // ctrl+click event
  Event* event = MouseEvent::create(
      EventTypeNames::click, false, false, document->domWindow(), 0, 0, 0, 0, 0,
      0, 0, PlatformEvent::CtrlKey, 0, 0, nullptr, 0,
      PlatformMouseEvent::RealOrIndistinguishable, String(), nullptr);
  FrameLoadRequest frameRequest(document, ResourceRequest(destination));
  frameRequest.setTriggeringEvent(event);
  UserGestureIndicator gesture(DocumentUserGestureToken::create(document));
  toLocalFrame(webViewHelper.webView()->page()->mainFrame())
      ->loader()
      .load(frameRequest);
  FrameTestHelpers::pumpPendingRequestsForFrameToLoad(
      webViewHelper.webView()->mainFrame());

  // decidePolicyForNavigation should be called both for the original request
  // and the ctrl+click.
  EXPECT_EQ(2, webFrameClient.decidePolicyCallCount());
}

TEST_P(ParameterizedWebFrameTest, BackToReload) {
  registerMockedHttpURLLoad("fragment_middle_click.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "fragment_middle_click.html",
                                  true);
  WebFrame* frame = webViewHelper.webView()->mainFrame();
  const FrameLoader& mainFrameLoader =
      webViewHelper.webView()->mainFrameImpl()->frame()->loader();
  Persistent<HistoryItem> firstItem = mainFrameLoader.currentItem();
  EXPECT_TRUE(firstItem);

  registerMockedHttpURLLoad("white-1x1.png");
  FrameTestHelpers::loadFrame(frame, m_baseURL + "white-1x1.png");
  EXPECT_NE(firstItem.get(), mainFrameLoader.currentItem());

  FrameTestHelpers::loadHistoryItem(frame, WebHistoryItem(firstItem.get()),
                                    WebHistoryDifferentDocumentLoad,
                                    WebCachePolicy::UseProtocolCachePolicy);
  EXPECT_EQ(firstItem.get(), mainFrameLoader.currentItem());

  FrameTestHelpers::reloadFrame(frame);
  EXPECT_EQ(WebCachePolicy::ValidatingCacheData,
            frame->dataSource()->request().getCachePolicy());
}

TEST_P(ParameterizedWebFrameTest, BackDuringChildFrameReload) {
  registerMockedHttpURLLoad("page_with_blank_iframe.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "page_with_blank_iframe.html",
                                  true);
  WebLocalFrame* mainFrame = webViewHelper.webView()->mainFrameImpl();
  const FrameLoader& mainFrameLoader =
      webViewHelper.webView()->mainFrameImpl()->frame()->loader();
  WebFrame* childFrame = mainFrame->firstChild();
  ASSERT_TRUE(childFrame);

  // Start a history navigation, then have a different frame commit a
  // navigation.  In this case, reload an about:blank frame, which will commit
  // synchronously.  After the history navigation completes, both the
  // appropriate document url and the current history item should reflect the
  // history navigation.
  registerMockedHttpURLLoad("white-1x1.png");
  WebHistoryItem item;
  item.initialize();
  WebURL historyURL(toKURL(m_baseURL + "white-1x1.png"));
  item.setURLString(historyURL.string());
  WebURLRequest request = mainFrame->requestFromHistoryItem(
      item, WebCachePolicy::UseProtocolCachePolicy);
  mainFrame->load(request, WebFrameLoadType::BackForward, item);

  FrameTestHelpers::reloadFrame(childFrame);
  EXPECT_EQ(item.urlString(), mainFrame->document().url().string());
  EXPECT_EQ(item.urlString(),
            WebString(mainFrameLoader.currentItem()->urlString()));
}

TEST_P(ParameterizedWebFrameTest, ReloadPost) {
  registerMockedHttpURLLoad("reload_post.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "reload_post.html", true);
  WebFrame* frame = webViewHelper.webView()->mainFrame();

  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              "javascript:document.forms[0].submit()");
  // Pump requests one more time after the javascript URL has executed to
  // trigger the actual POST load request.
  FrameTestHelpers::pumpPendingRequestsForFrameToLoad(
      webViewHelper.webView()->mainFrame());
  EXPECT_EQ(WebString::fromUTF8("POST"),
            frame->dataSource()->request().httpMethod());

  FrameTestHelpers::reloadFrame(frame);
  EXPECT_EQ(WebCachePolicy::ValidatingCacheData,
            frame->dataSource()->request().getCachePolicy());
  EXPECT_EQ(WebNavigationTypeFormResubmitted,
            frame->dataSource()->navigationType());
}

TEST_P(ParameterizedWebFrameTest, LoadHistoryItemReload) {
  registerMockedHttpURLLoad("fragment_middle_click.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "fragment_middle_click.html",
                                  true);
  WebFrame* frame = webViewHelper.webView()->mainFrame();
  const FrameLoader& mainFrameLoader =
      webViewHelper.webView()->mainFrameImpl()->frame()->loader();
  Persistent<HistoryItem> firstItem = mainFrameLoader.currentItem();
  EXPECT_TRUE(firstItem);

  registerMockedHttpURLLoad("white-1x1.png");
  FrameTestHelpers::loadFrame(frame, m_baseURL + "white-1x1.png");
  EXPECT_NE(firstItem.get(), mainFrameLoader.currentItem());

  // Cache policy overrides should take.
  FrameTestHelpers::loadHistoryItem(frame, WebHistoryItem(firstItem),
                                    WebHistoryDifferentDocumentLoad,
                                    WebCachePolicy::ValidatingCacheData);
  EXPECT_EQ(firstItem.get(), mainFrameLoader.currentItem());
  EXPECT_EQ(WebCachePolicy::ValidatingCacheData,
            frame->dataSource()->request().getCachePolicy());
}

class TestCachePolicyWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  explicit TestCachePolicyWebFrameClient(
      TestCachePolicyWebFrameClient* parentClient)
      : m_parentClient(parentClient),
        m_policy(WebCachePolicy::UseProtocolCachePolicy),
        m_childClient(0),
        m_willSendRequestCallCount(0),
        m_childFrameCreationCount(0) {}

  void setChildWebFrameClient(TestCachePolicyWebFrameClient* client) {
    m_childClient = client;
  }
  WebCachePolicy getCachePolicy() const { return m_policy; }
  int willSendRequestCallCount() const { return m_willSendRequestCallCount; }
  int childFrameCreationCount() const { return m_childFrameCreationCount; }

  virtual WebLocalFrame* createChildFrame(
      WebLocalFrame* parent,
      WebTreeScopeType scope,
      const WebString&,
      const WebString&,
      WebSandboxFlags,
      const WebFrameOwnerProperties& frameOwnerProperties) {
    DCHECK(m_childClient);
    m_childFrameCreationCount++;
    WebLocalFrame* frame = WebLocalFrame::create(scope, m_childClient);
    parent->appendChild(frame);
    return frame;
  }

  virtual void didStartLoading(bool toDifferentDocument) {
    if (m_parentClient) {
      m_parentClient->didStartLoading(toDifferentDocument);
      return;
    }
    TestWebFrameClient::didStartLoading(toDifferentDocument);
  }

  virtual void didStopLoading() {
    if (m_parentClient) {
      m_parentClient->didStopLoading();
      return;
    }
    TestWebFrameClient::didStopLoading();
  }

  void willSendRequest(WebLocalFrame*, WebURLRequest& request) override {
    m_policy = request.getCachePolicy();
    m_willSendRequestCallCount++;
  }

 private:
  TestCachePolicyWebFrameClient* m_parentClient;

  WebCachePolicy m_policy;
  TestCachePolicyWebFrameClient* m_childClient;
  int m_willSendRequestCallCount;
  int m_childFrameCreationCount;
};

TEST_P(ParameterizedWebFrameTest, ReloadIframe) {
  registerMockedHttpURLLoad("iframe_reload.html");
  registerMockedHttpURLLoad("visible_iframe.html");
  TestCachePolicyWebFrameClient mainClient(0);
  TestCachePolicyWebFrameClient childClient(&mainClient);
  mainClient.setChildWebFrameClient(&childClient);

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "iframe_reload.html", true,
                                  &mainClient);

  WebLocalFrameImpl* mainFrame = webViewHelper.webView()->mainFrameImpl();
  WebLocalFrameImpl* childFrame = toWebLocalFrameImpl(mainFrame->firstChild());
  ASSERT_EQ(childFrame->client(), &childClient);
  EXPECT_EQ(mainClient.childFrameCreationCount(), 1);
  EXPECT_EQ(childClient.willSendRequestCallCount(), 1);
  EXPECT_EQ(childClient.getCachePolicy(),
            WebCachePolicy::UseProtocolCachePolicy);

  FrameTestHelpers::reloadFrame(mainFrame);

  // A new WebFrame should have been created, but the child WebFrameClient
  // should be reused.
  ASSERT_NE(childFrame, toWebLocalFrameImpl(mainFrame->firstChild()));
  ASSERT_EQ(toWebLocalFrameImpl(mainFrame->firstChild())->client(),
            &childClient);

  EXPECT_EQ(mainClient.childFrameCreationCount(), 2);
  EXPECT_EQ(childClient.willSendRequestCallCount(), 2);
  EXPECT_EQ(childClient.getCachePolicy(), WebCachePolicy::ValidatingCacheData);
}

class TestSameDocumentWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  TestSameDocumentWebFrameClient()
      : m_frameLoadTypeReloadMainResourceSeen(false) {}

  virtual void willSendRequest(WebLocalFrame* frame, WebURLRequest&) {
    if (toWebLocalFrameImpl(frame)->frame()->loader().loadType() ==
        FrameLoadTypeReloadMainResource)
      m_frameLoadTypeReloadMainResourceSeen = true;
  }

  bool frameLoadTypeReloadMainResourceSeen() const {
    return m_frameLoadTypeReloadMainResourceSeen;
  }

 private:
  bool m_frameLoadTypeReloadMainResourceSeen;
};

TEST_P(ParameterizedWebFrameTest, NavigateToSame) {
  registerMockedHttpURLLoad("navigate_to_same.html");
  TestSameDocumentWebFrameClient client;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "navigate_to_same.html", true,
                                  &client);
  EXPECT_FALSE(client.frameLoadTypeReloadMainResourceSeen());

  FrameLoadRequest frameRequest(
      0,
      ResourceRequest(toLocalFrame(webViewHelper.webView()->page()->mainFrame())
                          ->document()
                          ->url()));
  toLocalFrame(webViewHelper.webView()->page()->mainFrame())
      ->loader()
      .load(frameRequest);
  FrameTestHelpers::pumpPendingRequestsForFrameToLoad(
      webViewHelper.webView()->mainFrame());

  EXPECT_TRUE(client.frameLoadTypeReloadMainResourceSeen());
}

class TestSameDocumentWithImageWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  TestSameDocumentWithImageWebFrameClient() : m_numOfImageRequests(0) {}

  virtual void willSendRequest(WebLocalFrame*, WebURLRequest& request) {
    if (request.getRequestContext() == WebURLRequest::RequestContextImage) {
      m_numOfImageRequests++;
      EXPECT_EQ(WebCachePolicy::UseProtocolCachePolicy,
                request.getCachePolicy());
    }
  }

  int numOfImageRequests() const { return m_numOfImageRequests; }

 private:
  int m_numOfImageRequests;
};

TEST_P(ParameterizedWebFrameTest,
       NavigateToSameNoConditionalRequestForSubresource) {
  registerMockedHttpURLLoad("foo_with_image.html");
  registerMockedHttpURLLoad("white-1x1.png");
  TestSameDocumentWithImageWebFrameClient client;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "foo_with_image.html", true,
                                  &client, nullptr, nullptr,
                                  &configureLoadsImagesAutomatically);

  WebCache::clear();
  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              m_baseURL + "foo_with_image.html");

  // 2 images are requested, and each triggers 2 willSendRequest() calls,
  // once for preloading and once for the real request.
  EXPECT_EQ(client.numOfImageRequests(), 4);
}

TEST_P(ParameterizedWebFrameTest, WebNodeImageContents) {
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad("about:blank", true);
  WebFrame* frame = webViewHelper.webView()->mainFrame();

  static const char bluePNG[] =
      "<img "
      "src=\"data:image/"
      "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAoAAAAKCAYAAACNMs+"
      "9AAAAGElEQVQYV2NkYPj/n4EIwDiqEF8oUT94AFIQE/cCn90IAAAAAElFTkSuQmCC\">";

  // Load up the image and test that we can extract the contents.
  KURL testURL = toKURL("about:blank");
  FrameTestHelpers::loadHTMLString(frame, bluePNG, testURL);

  WebNode node = frame->document().body().firstChild();
  EXPECT_TRUE(node.isElementNode());
  WebElement element = node.to<WebElement>();
  WebImage image = element.imageContents();
  ASSERT_FALSE(image.isNull());
  EXPECT_EQ(image.size().width, 10);
  EXPECT_EQ(image.size().height, 10);
  // FIXME: The rest of this test is disabled since the ImageDecodeCache state
  // may be inconsistent when this test runs, crbug.com/266088
  // SkBitmap bitmap = image.getSkBitmap();
  // SkAutoLockPixels locker(bitmap);
  // EXPECT_EQ(bitmap.getColor(0, 0), SK_ColorBLUE);
}

class TestStartStopCallbackWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  TestStartStopCallbackWebFrameClient()
      : m_startLoadingCount(0),
        m_stopLoadingCount(0),
        m_differentDocumentStartCount(0) {}

  void didStartLoading(bool toDifferentDocument) override {
    TestWebFrameClient::didStartLoading(toDifferentDocument);
    m_startLoadingCount++;
    if (toDifferentDocument)
      m_differentDocumentStartCount++;
  }

  void didStopLoading() override {
    TestWebFrameClient::didStopLoading();
    m_stopLoadingCount++;
  }

  int startLoadingCount() const { return m_startLoadingCount; }
  int stopLoadingCount() const { return m_stopLoadingCount; }
  int differentDocumentStartCount() const {
    return m_differentDocumentStartCount;
  }

 private:
  int m_startLoadingCount;
  int m_stopLoadingCount;
  int m_differentDocumentStartCount;
};

TEST_P(ParameterizedWebFrameTest, PushStateStartsAndStops) {
  registerMockedHttpURLLoad("push_state.html");
  TestStartStopCallbackWebFrameClient client;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "push_state.html", true, &client);

  EXPECT_EQ(client.startLoadingCount(), 2);
  EXPECT_EQ(client.stopLoadingCount(), 2);
  EXPECT_EQ(client.differentDocumentStartCount(), 1);
}

class TestDidNavigateCommitTypeWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  TestDidNavigateCommitTypeWebFrameClient()
      : m_lastCommitType(WebHistoryInertCommit) {}

  void didNavigateWithinPage(WebLocalFrame*,
                             const WebHistoryItem&,
                             WebHistoryCommitType type,
                             bool) override {
    m_lastCommitType = type;
  }

  WebHistoryCommitType lastCommitType() const { return m_lastCommitType; }

 private:
  WebHistoryCommitType m_lastCommitType;
};

TEST_P(ParameterizedWebFrameTest, SameDocumentHistoryNavigationCommitType) {
  registerMockedHttpURLLoad("push_state.html");
  TestDidNavigateCommitTypeWebFrameClient client;
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl = webViewHelper.initializeAndLoad(
      m_baseURL + "push_state.html", true, &client);
  Persistent<HistoryItem> item =
      toLocalFrame(webViewImpl->page()->mainFrame())->loader().currentItem();
  runPendingTasks();

  toLocalFrame(webViewImpl->page()->mainFrame())
      ->loader()
      .load(
          FrameLoadRequest(
              nullptr, FrameLoader::resourceRequestFromHistoryItem(
                           item.get(), WebCachePolicy::UseProtocolCachePolicy)),
          FrameLoadTypeBackForward, item.get(), HistorySameDocumentLoad);
  EXPECT_EQ(WebBackForwardCommit, client.lastCommitType());
}

class TestHistoryWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
 public:
  TestHistoryWebFrameClient() {
    m_replacesCurrentHistoryItem = false;
    m_frame = nullptr;
  }

  void didStartProvisionalLoad(WebLocalFrame* frame) {
    WebDataSource* ds = frame->provisionalDataSource();
    m_replacesCurrentHistoryItem = ds->replacesCurrentHistoryItem();
    m_frame = frame;
  }

  bool replacesCurrentHistoryItem() { return m_replacesCurrentHistoryItem; }
  WebFrame* frame() { return m_frame; }

 private:
  bool m_replacesCurrentHistoryItem;
  WebFrame* m_frame;
};

// Tests that the first navigation in an initially blank subframe will result in
// a history entry being replaced and not a new one being added.
TEST_P(ParameterizedWebFrameTest, FirstBlankSubframeNavigation) {
  registerMockedHttpURLLoad("history.html");
  registerMockedHttpURLLoad("find.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  TestHistoryWebFrameClient client;
  webViewHelper.initializeAndLoad("about:blank", true, &client);

  WebFrame* frame = webViewHelper.webView()->mainFrame();

  frame->executeScript(WebScriptSource(WebString::fromUTF8(
      "document.body.appendChild(document.createElement('iframe'))")));

  WebFrame* iframe = frame->firstChild();
  ASSERT_EQ(&client, toWebLocalFrameImpl(iframe)->client());
  EXPECT_EQ(iframe, client.frame());

  std::string url1 = m_baseURL + "history.html";
  FrameTestHelpers::loadFrame(iframe, url1);
  EXPECT_EQ(iframe, client.frame());
  EXPECT_EQ(url1, iframe->document().url().string().utf8());
  EXPECT_TRUE(client.replacesCurrentHistoryItem());

  std::string url2 = m_baseURL + "find.html";
  FrameTestHelpers::loadFrame(iframe, url2);
  EXPECT_EQ(iframe, client.frame());
  EXPECT_EQ(url2, iframe->document().url().string().utf8());
  EXPECT_FALSE(client.replacesCurrentHistoryItem());
}

// Tests that a navigation in a frame with a non-blank initial URL will create
// a new history item, unlike the case above.
TEST_P(ParameterizedWebFrameTest, FirstNonBlankSubframeNavigation) {
  registerMockedHttpURLLoad("history.html");
  registerMockedHttpURLLoad("find.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  TestHistoryWebFrameClient client;
  webViewHelper.initializeAndLoad("about:blank", true, &client);

  WebFrame* frame = webViewHelper.webView()->mainFrame();

  std::string url1 = m_baseURL + "history.html";
  FrameTestHelpers::loadFrame(
      frame,
      "javascript:var f = document.createElement('iframe'); "
      "f.src = '" +
          url1 +
          "';"
          "document.body.appendChild(f)");

  WebFrame* iframe = frame->firstChild();
  EXPECT_EQ(iframe, client.frame());
  EXPECT_EQ(url1, iframe->document().url().string().utf8());

  std::string url2 = m_baseURL + "find.html";
  FrameTestHelpers::loadFrame(iframe, url2);
  EXPECT_EQ(iframe, client.frame());
  EXPECT_EQ(url2, iframe->document().url().string().utf8());
  EXPECT_FALSE(client.replacesCurrentHistoryItem());
}

// Test verifies that layout will change a layer's scrollable attibutes
TEST_F(WebFrameTest, overflowHiddenRewrite) {
  registerMockedHttpURLLoad("non-scrollable.html");
  std::unique_ptr<FakeCompositingWebViewClient> fakeCompositingWebViewClient =
      makeUnique<FakeCompositingWebViewClient>();
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(true, nullptr, fakeCompositingWebViewClient.get(),
                           nullptr, &configureCompositingWebView);

  webViewHelper.resize(WebSize(100, 100));
  FrameTestHelpers::loadFrame(webViewHelper.webView()->mainFrame(),
                              m_baseURL + "non-scrollable.html");

  PaintLayerCompositor* compositor = webViewHelper.webView()->compositor();
  ASSERT_TRUE(compositor->scrollLayer());

  // Verify that the WebLayer is not scrollable initially.
  GraphicsLayer* scrollLayer = compositor->scrollLayer();
  WebLayer* webScrollLayer = scrollLayer->platformLayer();
  ASSERT_FALSE(webScrollLayer->userScrollableHorizontal());
  ASSERT_FALSE(webScrollLayer->userScrollableVertical());

  // Call javascript to make the layer scrollable, and verify it.
  WebLocalFrameImpl* frame =
      (WebLocalFrameImpl*)webViewHelper.webView()->mainFrame();
  frame->executeScript(WebScriptSource("allowScroll();"));
  webViewHelper.webView()->updateAllLifecyclePhases();
  ASSERT_TRUE(webScrollLayer->userScrollableHorizontal());
  ASSERT_TRUE(webScrollLayer->userScrollableVertical());
}

// Test that currentHistoryItem reflects the current page, not the provisional
// load.
TEST_P(ParameterizedWebFrameTest, CurrentHistoryItem) {
  registerMockedHttpURLLoad("fixed_layout.html");
  std::string url = m_baseURL + "fixed_layout.html";

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize();
  WebFrame* frame = webViewHelper.webView()->mainFrame();
  const FrameLoader& mainFrameLoader =
      webViewHelper.webView()->mainFrameImpl()->frame()->loader();
  WebURLRequest request(toKURL(url));
  frame->loadRequest(request);

  // Before commit, there is no history item.
  EXPECT_FALSE(mainFrameLoader.currentItem());

  FrameTestHelpers::pumpPendingRequestsForFrameToLoad(frame);

  // After commit, there is.
  HistoryItem* item = mainFrameLoader.currentItem();
  ASSERT_TRUE(item);
  EXPECT_EQ(WTF::String(url.data()), item->urlString());
}

class FailCreateChildFrame : public FrameTestHelpers::TestWebFrameClient {
 public:
  FailCreateChildFrame() : m_callCount(0) {}

  WebLocalFrame* createChildFrame(
      WebLocalFrame* parent,
      WebTreeScopeType scope,
      const WebString& frameName,
      const WebString& frameUniqueName,
      WebSandboxFlags sandboxFlags,
      const WebFrameOwnerProperties& frameOwnerProperties) override {
    ++m_callCount;
    return nullptr;
  }

  int callCount() const { return m_callCount; }

 private:
  int m_callCount;
};

// Test that we don't crash if WebFrameClient::createChildFrame() fails.
TEST_P(ParameterizedWebFrameTest, CreateChildFrameFailure) {
  registerMockedHttpURLLoad("create_child_frame_fail.html");
  FailCreateChildFrame client;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "create_child_frame_fail.html",
                                  true, &client);

  EXPECT_EQ(1, client.callCount());
}

TEST_P(ParameterizedWebFrameTest, fixedPositionInFixedViewport) {
  registerMockedHttpURLLoad("fixed-position-in-fixed-viewport.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(
      m_baseURL + "fixed-position-in-fixed-viewport.html", true, nullptr,
      nullptr, nullptr, enableViewportSettings);

  WebViewImpl* webView = webViewHelper.webView();
  webViewHelper.resize(WebSize(100, 100));

  Document* document = webView->mainFrameImpl()->frame()->document();
  Element* bottomFixed = document->getElementById("bottom-fixed");
  Element* topBottomFixed = document->getElementById("top-bottom-fixed");
  Element* rightFixed = document->getElementById("right-fixed");
  Element* leftRightFixed = document->getElementById("left-right-fixed");

  // The layout viewport will hit the min-scale limit of 0.25, so it'll be
  // 400x800.
  webViewHelper.resize(WebSize(100, 200));
  EXPECT_EQ(800, bottomFixed->offsetTop() + bottomFixed->offsetHeight());
  EXPECT_EQ(800, topBottomFixed->offsetHeight());

  // Now the layout viewport hits the content width limit of 500px so it'll be
  // 500x500.
  webViewHelper.resize(WebSize(200, 200));
  EXPECT_EQ(500, rightFixed->offsetLeft() + rightFixed->offsetWidth());
  EXPECT_EQ(500, leftRightFixed->offsetWidth());
}

TEST_P(ParameterizedWebFrameTest, FrameViewMoveWithSetFrameRect) {
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad("about:blank");
  webViewHelper.resize(WebSize(200, 200));
  webViewHelper.webView()->updateAllLifecyclePhases();

  FrameView* frameView = webViewHelper.webView()->mainFrameImpl()->frameView();
  EXPECT_RECT_EQ(IntRect(0, 0, 200, 200), frameView->frameRect());
  frameView->setFrameRect(IntRect(100, 100, 200, 200));
  EXPECT_RECT_EQ(IntRect(100, 100, 200, 200), frameView->frameRect());
}

TEST_F(WebFrameTest, FrameViewScrollAccountsForBrowserControls) {
  FakeCompositingWebViewClient client;
  registerMockedHttpURLLoad("long_scroll.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "long_scroll.html", true, nullptr,
                                  &client, nullptr, configureAndroid);

  WebViewImpl* webView = webViewHelper.webView();
  FrameView* frameView = webViewHelper.webView()->mainFrameImpl()->frameView();

  float browserControlsHeight = 40;
  webView->resizeWithBrowserControls(WebSize(100, 100), browserControlsHeight,
                                     false);
  webView->setPageScaleFactor(2.0f);
  webView->updateAllLifecyclePhases();

  webView->mainFrame()->setScrollOffset(WebSize(0, 2000));
  EXPECT_SIZE_EQ(ScrollOffset(0, 1900), frameView->scrollOffset());

  // Simulate the browser controls showing by 20px, thus shrinking the viewport
  // and allowing it to scroll an additional 20px.
  webView->applyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(),
                               1.0f, 20.0f / browserControlsHeight);
  EXPECT_SIZE_EQ(ScrollOffset(0, 1920), frameView->maximumScrollOffset());

  // Show more, make sure the scroll actually gets clamped.
  webView->applyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(),
                               1.0f, 20.0f / browserControlsHeight);
  webView->mainFrame()->setScrollOffset(WebSize(0, 2000));
  EXPECT_SIZE_EQ(ScrollOffset(0, 1940), frameView->scrollOffset());

  // Hide until there's 10px showing.
  webView->applyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(),
                               1.0f, -30.0f / browserControlsHeight);
  EXPECT_SIZE_EQ(ScrollOffset(0, 1910), frameView->maximumScrollOffset());

  // Simulate a LayoutPart::resize. The frame is resized to accomodate
  // the browser controls and Blink's view of the browser controls matches that
  // of the CC
  webView->applyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(),
                               1.0f, 30.0f / browserControlsHeight);
  webView->resizeWithBrowserControls(WebSize(100, 60), 40.0f, true);
  webView->updateAllLifecyclePhases();
  EXPECT_SIZE_EQ(ScrollOffset(0, 1940), frameView->maximumScrollOffset());

  // Now simulate hiding.
  webView->applyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(),
                               1.0f, -10.0f / browserControlsHeight);
  EXPECT_SIZE_EQ(ScrollOffset(0, 1930), frameView->maximumScrollOffset());

  // Reset to original state: 100px widget height, browser controls fully
  // hidden.
  webView->applyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(),
                               1.0f, -30.0f / browserControlsHeight);
  webView->resizeWithBrowserControls(WebSize(100, 100), browserControlsHeight,
                                     false);
  webView->updateAllLifecyclePhases();
  EXPECT_SIZE_EQ(ScrollOffset(0, 1900), frameView->maximumScrollOffset());

  // Show the browser controls by just 1px, since we're zoomed in to 2X, that
  // should allow an extra 0.5px of scrolling in the visual viewport. Make
  // sure we're not losing any pixels when applying the adjustment on the
  // main frame.
  webView->applyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(),
                               1.0f, 1.0f / browserControlsHeight);
  EXPECT_SIZE_EQ(ScrollOffset(0, 1901), frameView->maximumScrollOffset());

  webView->applyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(),
                               1.0f, 2.0f / browserControlsHeight);
  EXPECT_SIZE_EQ(ScrollOffset(0, 1903), frameView->maximumScrollOffset());
}

TEST_F(WebFrameTest, MaximumScrollPositionCanBeNegative) {
  registerMockedHttpURLLoad("rtl-overview-mode.html");

  FixedLayoutTestWebViewClient client;
  client.m_screenInfo.deviceScaleFactor = 1;
  int viewportWidth = 640;
  int viewportHeight = 480;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "rtl-overview-mode.html", true,
                                  nullptr, &client, nullptr,
                                  enableViewportSettings);
  webViewHelper.webView()->setInitialPageScaleOverride(-1);
  webViewHelper.webView()->settings()->setWideViewportQuirkEnabled(true);
  webViewHelper.webView()->settings()->setLoadWithOverviewMode(true);
  webViewHelper.webView()->settings()->setUseWideViewport(true);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));
  webViewHelper.webView()->updateAllLifecyclePhases();

  FrameView* frameView = webViewHelper.webView()->mainFrameImpl()->frameView();
  EXPECT_LT(frameView->maximumScrollOffset().width(), 0);
}

TEST_P(ParameterizedWebFrameTest, FullscreenLayerSize) {
  FakeCompositingWebViewClient client;
  registerMockedHttpURLLoad("fullscreen_div.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  int viewportWidth = 640;
  int viewportHeight = 480;
  client.m_screenInfo.rect.width = viewportWidth;
  client.m_screenInfo.rect.height = viewportHeight;
  WebViewImpl* webViewImpl = webViewHelper.initializeAndLoad(
      m_baseURL + "fullscreen_div.html", true, nullptr, &client, nullptr,
      configureAndroid);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));
  webViewImpl->updateAllLifecyclePhases();

  Document* document = webViewImpl->mainFrameImpl()->frame()->document();
  UserGestureIndicator gesture(DocumentUserGestureToken::create(document));
  Element* divFullscreen = document->getElementById("div1");
  Fullscreen::requestFullscreen(*divFullscreen, Fullscreen::PrefixedRequest);
  webViewImpl->didEnterFullscreen();
  webViewImpl->updateAllLifecyclePhases();
  EXPECT_EQ(Fullscreen::currentFullScreenElementFrom(*document), divFullscreen);

  // Verify that the element is sized to the viewport.
  LayoutFullScreen* fullscreenLayoutObject =
      Fullscreen::from(*document).fullScreenLayoutObject();
  EXPECT_EQ(viewportWidth, fullscreenLayoutObject->logicalWidth().toInt());
  EXPECT_EQ(viewportHeight, fullscreenLayoutObject->logicalHeight().toInt());

  // Verify it's updated after a device rotation.
  client.m_screenInfo.rect.width = viewportHeight;
  client.m_screenInfo.rect.height = viewportWidth;
  webViewHelper.resize(WebSize(viewportHeight, viewportWidth));
  webViewImpl->updateAllLifecyclePhases();
  EXPECT_EQ(viewportHeight, fullscreenLayoutObject->logicalWidth().toInt());
  EXPECT_EQ(viewportWidth, fullscreenLayoutObject->logicalHeight().toInt());
}

TEST_F(WebFrameTest, FullscreenLayerNonScrollable) {
  FakeCompositingWebViewClient client;
  registerMockedHttpURLLoad("fullscreen_div.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  int viewportWidth = 640;
  int viewportHeight = 480;
  WebViewImpl* webViewImpl = webViewHelper.initializeAndLoad(
      m_baseURL + "fullscreen_div.html", true, nullptr, &client, nullptr,
      configureAndroid);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));
  webViewImpl->updateAllLifecyclePhases();

  Document* document = webViewImpl->mainFrameImpl()->frame()->document();
  UserGestureIndicator gesture(DocumentUserGestureToken::create(document));
  Element* divFullscreen = document->getElementById("div1");
  Fullscreen::requestFullscreen(*divFullscreen, Fullscreen::PrefixedRequest);
  webViewImpl->didEnterFullscreen();
  webViewImpl->updateAllLifecyclePhases();

  // Verify that the viewports are nonscrollable.
  EXPECT_EQ(Fullscreen::currentFullScreenElementFrom(*document), divFullscreen);
  FrameView* frameView = webViewHelper.webView()->mainFrameImpl()->frameView();
  WebLayer* layoutViewportScrollLayer =
      webViewImpl->compositor()->scrollLayer()->platformLayer();
  WebLayer* visualViewportScrollLayer = frameView->page()
                                            ->frameHost()
                                            .visualViewport()
                                            .scrollLayer()
                                            ->platformLayer();
  ASSERT_FALSE(layoutViewportScrollLayer->userScrollableHorizontal());
  ASSERT_FALSE(layoutViewportScrollLayer->userScrollableVertical());
  ASSERT_FALSE(visualViewportScrollLayer->userScrollableHorizontal());
  ASSERT_FALSE(visualViewportScrollLayer->userScrollableVertical());

  // Verify that the viewports are scrollable upon exiting fullscreen.
  webViewImpl->didExitFullscreen();
  webViewImpl->updateAllLifecyclePhases();
  EXPECT_EQ(Fullscreen::currentFullScreenElementFrom(*document), nullptr);
  ASSERT_TRUE(layoutViewportScrollLayer->userScrollableHorizontal());
  ASSERT_TRUE(layoutViewportScrollLayer->userScrollableVertical());
  ASSERT_TRUE(visualViewportScrollLayer->userScrollableHorizontal());
  ASSERT_TRUE(visualViewportScrollLayer->userScrollableVertical());
}

TEST_P(ParameterizedWebFrameTest, FullscreenMainFrame) {
  FakeCompositingWebViewClient client;
  registerMockedHttpURLLoad("fullscreen_div.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  int viewportWidth = 640;
  int viewportHeight = 480;
  WebViewImpl* webViewImpl = webViewHelper.initializeAndLoad(
      m_baseURL + "fullscreen_div.html", true, nullptr, &client, nullptr,
      configureAndroid);
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));
  webViewImpl->updateAllLifecyclePhases();

  Document* document = webViewImpl->mainFrameImpl()->frame()->document();
  UserGestureIndicator gesture(DocumentUserGestureToken::create(document));
  Fullscreen::requestFullscreen(*document->documentElement(),
                                Fullscreen::PrefixedRequest);
  webViewImpl->didEnterFullscreen();
  webViewImpl->updateAllLifecyclePhases();

  // Verify that the main frame is still scrollable.
  EXPECT_EQ(Fullscreen::currentFullScreenElementFrom(*document),
            document->documentElement());
  WebLayer* webScrollLayer =
      webViewImpl->compositor()->scrollLayer()->platformLayer();
  ASSERT_TRUE(webScrollLayer->scrollable());
  ASSERT_TRUE(webScrollLayer->userScrollableHorizontal());
  ASSERT_TRUE(webScrollLayer->userScrollableVertical());

  // Verify the main frame still behaves correctly after a resize.
  webViewHelper.resize(WebSize(viewportHeight, viewportWidth));
  ASSERT_TRUE(webScrollLayer->scrollable());
  ASSERT_TRUE(webScrollLayer->userScrollableHorizontal());
  ASSERT_TRUE(webScrollLayer->userScrollableVertical());
}

TEST_P(ParameterizedWebFrameTest, FullscreenSubframe) {
  FakeCompositingWebViewClient client;
  registerMockedHttpURLLoad("fullscreen_iframe.html");
  registerMockedHttpURLLoad("fullscreen_div.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl = webViewHelper.initializeAndLoad(
      m_baseURL + "fullscreen_iframe.html", true, nullptr, &client, nullptr,
      configureAndroid);
  int viewportWidth = 640;
  int viewportHeight = 480;
  client.m_screenInfo.rect.width = viewportWidth;
  client.m_screenInfo.rect.height = viewportHeight;
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));
  webViewImpl->updateAllLifecyclePhases();

  Document* document =
      toWebLocalFrameImpl(webViewHelper.webView()->mainFrame()->firstChild())
          ->frame()
          ->document();
  UserGestureIndicator gesture(DocumentUserGestureToken::create(document));
  Element* divFullscreen = document->getElementById("div1");
  Fullscreen::requestFullscreen(*divFullscreen, Fullscreen::PrefixedRequest);
  webViewImpl->didEnterFullscreen();
  webViewImpl->updateAllLifecyclePhases();

  // Verify that the element is sized to the viewport.
  LayoutFullScreen* fullscreenLayoutObject =
      Fullscreen::from(*document).fullScreenLayoutObject();
  EXPECT_EQ(viewportWidth, fullscreenLayoutObject->logicalWidth().toInt());
  EXPECT_EQ(viewportHeight, fullscreenLayoutObject->logicalHeight().toInt());

  // Verify it's updated after a device rotation.
  client.m_screenInfo.rect.width = viewportHeight;
  client.m_screenInfo.rect.height = viewportWidth;
  webViewHelper.resize(WebSize(viewportHeight, viewportWidth));
  webViewImpl->updateAllLifecyclePhases();
  EXPECT_EQ(viewportHeight, fullscreenLayoutObject->logicalWidth().toInt());
  EXPECT_EQ(viewportWidth, fullscreenLayoutObject->logicalHeight().toInt());
}

TEST_P(ParameterizedWebFrameTest, FullscreenWithTinyViewport) {
  FakeCompositingWebViewClient client;
  registerMockedHttpURLLoad("viewport-tiny.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl = webViewHelper.initializeAndLoad(
      m_baseURL + "viewport-tiny.html", true, nullptr, &client, nullptr,
      configureAndroid);
  int viewportWidth = 384;
  int viewportHeight = 640;
  client.m_screenInfo.rect.width = viewportWidth;
  client.m_screenInfo.rect.height = viewportHeight;
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));
  webViewImpl->updateAllLifecyclePhases();

  LayoutViewItem layoutViewItem =
      webViewHelper.webView()->mainFrameImpl()->frameView()->layoutViewItem();
  EXPECT_EQ(320, layoutViewItem.logicalWidth().floor());
  EXPECT_EQ(533, layoutViewItem.logicalHeight().floor());
  EXPECT_FLOAT_EQ(1.2, webViewImpl->pageScaleFactor());
  EXPECT_FLOAT_EQ(1.2, webViewImpl->minimumPageScaleFactor());
  EXPECT_FLOAT_EQ(5.0, webViewImpl->maximumPageScaleFactor());

  Document* document = webViewImpl->mainFrameImpl()->frame()->document();
  UserGestureIndicator gesture(DocumentUserGestureToken::create(document));
  Fullscreen::requestFullscreen(*document->documentElement(),
                                Fullscreen::PrefixedRequest);
  webViewImpl->didEnterFullscreen();
  webViewImpl->updateAllLifecyclePhases();
  EXPECT_EQ(384, layoutViewItem.logicalWidth().floor());
  EXPECT_EQ(640, layoutViewItem.logicalHeight().floor());
  EXPECT_FLOAT_EQ(1.0, webViewImpl->pageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, webViewImpl->minimumPageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, webViewImpl->maximumPageScaleFactor());

  webViewImpl->didExitFullscreen();
  webViewImpl->updateAllLifecyclePhases();
  EXPECT_EQ(320, layoutViewItem.logicalWidth().floor());
  EXPECT_EQ(533, layoutViewItem.logicalHeight().floor());
  EXPECT_FLOAT_EQ(1.2, webViewImpl->pageScaleFactor());
  EXPECT_FLOAT_EQ(1.2, webViewImpl->minimumPageScaleFactor());
  EXPECT_FLOAT_EQ(5.0, webViewImpl->maximumPageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, FullscreenResizeWithTinyViewport) {
  FakeCompositingWebViewClient client;
  registerMockedHttpURLLoad("viewport-tiny.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl = webViewHelper.initializeAndLoad(
      m_baseURL + "viewport-tiny.html", true, nullptr, &client, nullptr,
      configureAndroid);
  int viewportWidth = 384;
  int viewportHeight = 640;
  client.m_screenInfo.rect.width = viewportWidth;
  client.m_screenInfo.rect.height = viewportHeight;
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));
  webViewImpl->updateAllLifecyclePhases();

  LayoutViewItem layoutViewItem =
      webViewHelper.webView()->mainFrameImpl()->frameView()->layoutViewItem();
  Document* document = webViewImpl->mainFrameImpl()->frame()->document();
  UserGestureIndicator gesture(DocumentUserGestureToken::create(document));
  Fullscreen::requestFullscreen(*document->documentElement(),
                                Fullscreen::PrefixedRequest);
  webViewImpl->didEnterFullscreen();
  webViewImpl->updateAllLifecyclePhases();
  EXPECT_EQ(384, layoutViewItem.logicalWidth().floor());
  EXPECT_EQ(640, layoutViewItem.logicalHeight().floor());
  EXPECT_FLOAT_EQ(1.0, webViewImpl->pageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, webViewImpl->minimumPageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, webViewImpl->maximumPageScaleFactor());

  viewportWidth = 640;
  viewportHeight = 384;
  client.m_screenInfo.rect.width = viewportWidth;
  client.m_screenInfo.rect.height = viewportHeight;
  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));
  webViewImpl->updateAllLifecyclePhases();
  EXPECT_EQ(640, layoutViewItem.logicalWidth().floor());
  EXPECT_EQ(384, layoutViewItem.logicalHeight().floor());
  EXPECT_FLOAT_EQ(1.0, webViewImpl->pageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, webViewImpl->minimumPageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, webViewImpl->maximumPageScaleFactor());

  webViewImpl->didExitFullscreen();
  webViewImpl->updateAllLifecyclePhases();
  EXPECT_EQ(320, layoutViewItem.logicalWidth().floor());
  EXPECT_EQ(192, layoutViewItem.logicalHeight().floor());
  EXPECT_FLOAT_EQ(2, webViewImpl->pageScaleFactor());
  EXPECT_FLOAT_EQ(2, webViewImpl->minimumPageScaleFactor());
  EXPECT_FLOAT_EQ(5.0, webViewImpl->maximumPageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, FullscreenRestoreScaleFactorUponExiting) {
  // The purpose of this test is to more precisely simulate the sequence of
  // resize and switching fullscreen state operations on WebView, with the
  // interference from Android status bars like a real device does.
  // This verifies we handle the transition and restore states correctly.
  WebSize screenSizeMinusStatusBarsMinusUrlBar(598, 303);
  WebSize screenSizeMinusStatusBars(598, 359);
  WebSize screenSize(640, 384);

  FakeCompositingWebViewClient client;
  registerMockedHttpURLLoad("fullscreen_restore_scale_factor.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl = webViewHelper.initializeAndLoad(
      m_baseURL + "fullscreen_restore_scale_factor.html", true, nullptr,
      &client, nullptr, &configureAndroid);
  client.m_screenInfo.rect.width = screenSizeMinusStatusBarsMinusUrlBar.width;
  client.m_screenInfo.rect.height = screenSizeMinusStatusBarsMinusUrlBar.height;
  webViewHelper.resize(screenSizeMinusStatusBarsMinusUrlBar);
  LayoutViewItem layoutViewItem =
      webViewHelper.webView()->mainFrameImpl()->frameView()->layoutViewItem();
  EXPECT_EQ(screenSizeMinusStatusBarsMinusUrlBar.width,
            layoutViewItem.logicalWidth().floor());
  EXPECT_EQ(screenSizeMinusStatusBarsMinusUrlBar.height,
            layoutViewItem.logicalHeight().floor());
  EXPECT_FLOAT_EQ(1.0, webViewImpl->pageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, webViewImpl->minimumPageScaleFactor());
  EXPECT_FLOAT_EQ(5.0, webViewImpl->maximumPageScaleFactor());

  {
    Document* document = webViewImpl->mainFrameImpl()->frame()->document();
    UserGestureIndicator gesture(DocumentUserGestureToken::create(document));
    Fullscreen::requestFullscreen(*document->body(),
                                  Fullscreen::PrefixedRequest);
  }

  webViewImpl->didEnterFullscreen();
  webViewImpl->updateAllLifecyclePhases();
  client.m_screenInfo.rect.width = screenSizeMinusStatusBars.width;
  client.m_screenInfo.rect.height = screenSizeMinusStatusBars.height;
  webViewHelper.resize(screenSizeMinusStatusBars);
  client.m_screenInfo.rect.width = screenSize.width;
  client.m_screenInfo.rect.height = screenSize.height;
  webViewHelper.resize(screenSize);
  EXPECT_EQ(screenSize.width, layoutViewItem.logicalWidth().floor());
  EXPECT_EQ(screenSize.height, layoutViewItem.logicalHeight().floor());
  EXPECT_FLOAT_EQ(1.0, webViewImpl->pageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, webViewImpl->minimumPageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, webViewImpl->maximumPageScaleFactor());

  webViewImpl->didExitFullscreen();
  webViewImpl->updateAllLifecyclePhases();
  client.m_screenInfo.rect.width = screenSizeMinusStatusBars.width;
  client.m_screenInfo.rect.height = screenSizeMinusStatusBars.height;
  webViewHelper.resize(screenSizeMinusStatusBars);
  client.m_screenInfo.rect.width = screenSizeMinusStatusBarsMinusUrlBar.width;
  client.m_screenInfo.rect.height = screenSizeMinusStatusBarsMinusUrlBar.height;
  webViewHelper.resize(screenSizeMinusStatusBarsMinusUrlBar);
  EXPECT_EQ(screenSizeMinusStatusBarsMinusUrlBar.width,
            layoutViewItem.logicalWidth().floor());
  EXPECT_EQ(screenSizeMinusStatusBarsMinusUrlBar.height,
            layoutViewItem.logicalHeight().floor());
  EXPECT_FLOAT_EQ(1.0, webViewImpl->pageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, webViewImpl->minimumPageScaleFactor());
  EXPECT_FLOAT_EQ(5.0, webViewImpl->maximumPageScaleFactor());
}

// Tests that leaving fullscreen by navigating to a new page resets the
// fullscreen page scale constraints.
TEST_P(ParameterizedWebFrameTest, ClearFullscreenConstraintsOnNavigation) {
  registerMockedHttpURLLoad("viewport-tiny.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  int viewportWidth = 100;
  int viewportHeight = 200;

  WebViewImpl* webViewImpl = webViewHelper.initializeAndLoad(
      m_baseURL + "viewport-tiny.html", true, nullptr, nullptr, nullptr,
      configureAndroid);

  webViewHelper.resize(WebSize(viewportWidth, viewportHeight));
  webViewImpl->updateAllLifecyclePhases();

  // viewport-tiny.html specifies a 320px layout width.
  LayoutViewItem layoutViewItem =
      webViewImpl->mainFrameImpl()->frameView()->layoutViewItem();
  EXPECT_EQ(320, layoutViewItem.logicalWidth().floor());
  EXPECT_EQ(640, layoutViewItem.logicalHeight().floor());
  EXPECT_FLOAT_EQ(0.3125, webViewImpl->pageScaleFactor());
  EXPECT_FLOAT_EQ(0.3125, webViewImpl->minimumPageScaleFactor());
  EXPECT_FLOAT_EQ(5.0, webViewImpl->maximumPageScaleFactor());

  Document* document = webViewImpl->mainFrameImpl()->frame()->document();
  UserGestureIndicator gesture(
      DocumentUserGestureToken::create(document, UserGestureToken::NewGesture));
  Fullscreen::requestFullscreen(*document->documentElement(),
                                Fullscreen::PrefixedRequest);
  webViewImpl->didEnterFullscreen();
  webViewImpl->updateAllLifecyclePhases();

  // Entering fullscreen causes layout size and page scale limits to be
  // overridden.
  EXPECT_EQ(100, layoutViewItem.logicalWidth().floor());
  EXPECT_EQ(200, layoutViewItem.logicalHeight().floor());
  EXPECT_FLOAT_EQ(1.0, webViewImpl->pageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, webViewImpl->minimumPageScaleFactor());
  EXPECT_FLOAT_EQ(1.0, webViewImpl->maximumPageScaleFactor());

  const char source[] = "<meta name=\"viewport\" content=\"width=200\">";

  // Load a new page before exiting fullscreen.
  KURL testURL = toKURL("about:blank");
  WebFrame* frame = webViewHelper.webView()->mainFrame();
  FrameTestHelpers::loadHTMLString(frame, source, testURL);
  webViewImpl->didExitFullscreen();
  webViewImpl->updateAllLifecyclePhases();

  // Make sure the new page's layout size and scale factor limits aren't
  // overridden.
  layoutViewItem = webViewImpl->mainFrameImpl()->frameView()->layoutViewItem();
  EXPECT_EQ(200, layoutViewItem.logicalWidth().floor());
  EXPECT_EQ(400, layoutViewItem.logicalHeight().floor());
  EXPECT_FLOAT_EQ(0.5, webViewImpl->minimumPageScaleFactor());
  EXPECT_FLOAT_EQ(5.0, webViewImpl->maximumPageScaleFactor());
}

TEST_P(ParameterizedWebFrameTest, LayoutBlockPercentHeightDescendants) {
  registerMockedHttpURLLoad("percent-height-descendants.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL +
                                  "percent-height-descendants.html");

  WebViewImpl* webView = webViewHelper.webView();
  webViewHelper.resize(WebSize(800, 800));
  webView->updateAllLifecyclePhases();

  Document* document = webView->mainFrameImpl()->frame()->document();
  LayoutBlock* container =
      toLayoutBlock(document->getElementById("container")->layoutObject());
  LayoutBox* percentHeightInAnonymous = toLayoutBox(
      document->getElementById("percent-height-in-anonymous")->layoutObject());
  LayoutBox* percentHeightDirectChild = toLayoutBox(
      document->getElementById("percent-height-direct-child")->layoutObject());

  EXPECT_TRUE(container->hasPercentHeightDescendant(percentHeightInAnonymous));
  EXPECT_TRUE(container->hasPercentHeightDescendant(percentHeightDirectChild));

  ASSERT_TRUE(container->percentHeightDescendants());
  ASSERT_TRUE(container->hasPercentHeightDescendants());
  EXPECT_EQ(2U, container->percentHeightDescendants()->size());
  EXPECT_TRUE(container->percentHeightDescendants()->contains(
      percentHeightInAnonymous));
  EXPECT_TRUE(container->percentHeightDescendants()->contains(
      percentHeightDirectChild));

  LayoutBlock* anonymousBlock = percentHeightInAnonymous->containingBlock();
  EXPECT_TRUE(anonymousBlock->isAnonymous());
  EXPECT_FALSE(anonymousBlock->hasPercentHeightDescendants());
}

TEST_P(ParameterizedWebFrameTest, HasVisibleContentOnVisibleFrames) {
  registerMockedHttpURLLoad("visible_frames.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl =
      webViewHelper.initializeAndLoad(m_baseURL + "visible_frames.html");
  for (WebFrame* frame = webViewImpl->mainFrameImpl()->traverseNext(); frame;
       frame = frame->traverseNext()) {
    EXPECT_TRUE(frame->hasVisibleContent());
  }
}

TEST_P(ParameterizedWebFrameTest, HasVisibleContentOnHiddenFrames) {
  registerMockedHttpURLLoad("hidden_frames.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl =
      webViewHelper.initializeAndLoad(m_baseURL + "hidden_frames.html");
  for (WebFrame* frame = webViewImpl->mainFrameImpl()->traverseNext(); frame;
       frame = frame->traverseNext()) {
    EXPECT_FALSE(frame->hasVisibleContent());
  }
}

class ManifestChangeWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  ManifestChangeWebFrameClient() : m_manifestChangeCount(0) {}
  void didChangeManifest() override { ++m_manifestChangeCount; }

  int manifestChangeCount() { return m_manifestChangeCount; }

 private:
  int m_manifestChangeCount;
};

TEST_P(ParameterizedWebFrameTest, NotifyManifestChange) {
  registerMockedHttpURLLoad("link-manifest-change.html");

  ManifestChangeWebFrameClient webFrameClient;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "link-manifest-change.html", true,
                                  &webFrameClient);

  EXPECT_EQ(14, webFrameClient.manifestChangeCount());
}

static Resource* fetchManifest(Document* document, const KURL& url) {
  FetchRequest fetchRequest =
      FetchRequest(ResourceRequest(url), FetchInitiatorInfo());
  fetchRequest.mutableResourceRequest().setRequestContext(
      WebURLRequest::RequestContextManifest);

  return RawResource::fetchSynchronously(fetchRequest, document->fetcher());
}

TEST_P(ParameterizedWebFrameTest, ManifestFetch) {
  registerMockedHttpURLLoad("foo.html");
  registerMockedHttpURLLoad("link-manifest-fetch.json");

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "foo.html");
  Document* document =
      webViewHelper.webView()->mainFrameImpl()->frame()->document();

  Resource* resource =
      fetchManifest(document, toKURL(m_baseURL + "link-manifest-fetch.json"));

  EXPECT_TRUE(resource->isLoaded());
}

TEST_P(ParameterizedWebFrameTest, ManifestCSPFetchAllow) {
  URLTestHelpers::registerMockedURLLoad(
      toKURL(m_notBaseURL + "link-manifest-fetch.json"),
      "link-manifest-fetch.json");
  registerMockedHttpURLLoadWithCSP("foo.html", "manifest-src *");

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "foo.html");
  Document* document =
      webViewHelper.webView()->mainFrameImpl()->frame()->document();

  Resource* resource = fetchManifest(
      document, toKURL(m_notBaseURL + "link-manifest-fetch.json"));

  EXPECT_TRUE(resource->isLoaded());
}

TEST_P(ParameterizedWebFrameTest, ManifestCSPFetchSelf) {
  URLTestHelpers::registerMockedURLLoad(
      toKURL(m_notBaseURL + "link-manifest-fetch.json"),
      "link-manifest-fetch.json");
  registerMockedHttpURLLoadWithCSP("foo.html", "manifest-src 'self'");

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "foo.html");
  Document* document =
      webViewHelper.webView()->mainFrameImpl()->frame()->document();

  Resource* resource = fetchManifest(
      document, toKURL(m_notBaseURL + "link-manifest-fetch.json"));

  // Fetching resource wasn't allowed.
  ASSERT_TRUE(resource);
  EXPECT_TRUE(resource->errorOccurred());
  EXPECT_TRUE(resource->resourceError().isAccessCheck());
}

TEST_P(ParameterizedWebFrameTest, ManifestCSPFetchSelfReportOnly) {
  URLTestHelpers::registerMockedURLLoad(
      toKURL(m_notBaseURL + "link-manifest-fetch.json"),
      "link-manifest-fetch.json");
  registerMockedHttpURLLoadWithCSP("foo.html", "manifest-src 'self'",
                                   /* report only */ true);

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "foo.html");
  Document* document =
      webViewHelper.webView()->mainFrameImpl()->frame()->document();

  Resource* resource = fetchManifest(
      document, toKURL(m_notBaseURL + "link-manifest-fetch.json"));

  EXPECT_TRUE(resource->isLoaded());
}

TEST_P(ParameterizedWebFrameTest, ReloadBypassingCache) {
  // Check that a reload ignoring cache on a frame will result in the cache
  // policy of the request being set to ReloadBypassingCache.
  registerMockedHttpURLLoad("foo.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "foo.html", true);
  WebFrame* frame = webViewHelper.webView()->mainFrame();
  FrameTestHelpers::reloadFrameIgnoringCache(frame);
  EXPECT_EQ(WebCachePolicy::BypassingCache,
            frame->dataSource()->request().getCachePolicy());
}

static void nodeImageTestValidation(const IntSize& referenceBitmapSize,
                                    DragImage* dragImage) {
  // Prepare the reference bitmap.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(referenceBitmapSize.width(),
                        referenceBitmapSize.height());
  SkCanvas canvas(bitmap);
  canvas.drawColor(SK_ColorGREEN);

  EXPECT_EQ(referenceBitmapSize.width(), dragImage->size().width());
  EXPECT_EQ(referenceBitmapSize.height(), dragImage->size().height());
  const SkBitmap& dragBitmap = dragImage->bitmap();
  SkAutoLockPixels lockPixel(dragBitmap);
  EXPECT_EQ(
      0, memcmp(bitmap.getPixels(), dragBitmap.getPixels(), bitmap.getSize()));
}

TEST_P(ParameterizedWebFrameTest, NodeImageTestCSSTransformDescendant) {
  FrameTestHelpers::WebViewHelper webViewHelper;
  std::unique_ptr<DragImage> dragImage = nodeImageTestSetup(
      &webViewHelper, std::string("case-css-3dtransform-descendant"));
  EXPECT_TRUE(dragImage);

  nodeImageTestValidation(IntSize(40, 40), dragImage.get());
}

TEST_P(ParameterizedWebFrameTest, NodeImageTestCSSTransform) {
  FrameTestHelpers::WebViewHelper webViewHelper;
  std::unique_ptr<DragImage> dragImage =
      nodeImageTestSetup(&webViewHelper, std::string("case-css-transform"));
  EXPECT_TRUE(dragImage);

  nodeImageTestValidation(IntSize(40, 40), dragImage.get());
}

TEST_P(ParameterizedWebFrameTest, NodeImageTestCSS3DTransform) {
  FrameTestHelpers::WebViewHelper webViewHelper;
  std::unique_ptr<DragImage> dragImage =
      nodeImageTestSetup(&webViewHelper, std::string("case-css-3dtransform"));
  EXPECT_TRUE(dragImage);

  nodeImageTestValidation(IntSize(40, 40), dragImage.get());
}

TEST_P(ParameterizedWebFrameTest, NodeImageTestInlineBlock) {
  FrameTestHelpers::WebViewHelper webViewHelper;
  std::unique_ptr<DragImage> dragImage =
      nodeImageTestSetup(&webViewHelper, std::string("case-inlineblock"));
  EXPECT_TRUE(dragImage);

  nodeImageTestValidation(IntSize(40, 40), dragImage.get());
}

TEST_P(ParameterizedWebFrameTest, NodeImageTestFloatLeft) {
  FrameTestHelpers::WebViewHelper webViewHelper;
  std::unique_ptr<DragImage> dragImage = nodeImageTestSetup(
      &webViewHelper, std::string("case-float-left-overflow-hidden"));
  EXPECT_TRUE(dragImage);

  nodeImageTestValidation(IntSize(40, 40), dragImage.get());
}

// Crashes on Android: http://crbug.com/403804
#if OS(ANDROID)
TEST_P(ParameterizedWebFrameTest, DISABLED_PrintingBasic)
#else
TEST_P(ParameterizedWebFrameTest, PrintingBasic)
#endif
{
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad("data:text/html,Hello, world.");

  WebFrame* frame = webViewHelper.webView()->mainFrame();

  WebPrintParams printParams;
  printParams.printContentArea.width = 500;
  printParams.printContentArea.height = 500;

  int pageCount = frame->printBegin(printParams);
  EXPECT_EQ(1, pageCount);
  frame->printEnd();
}

class ThemeColorTestWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  ThemeColorTestWebFrameClient() : m_didNotify(false) {}

  void reset() { m_didNotify = false; }

  bool didNotify() const { return m_didNotify; }

 private:
  virtual void didChangeThemeColor() { m_didNotify = true; }

  bool m_didNotify;
};

TEST_P(ParameterizedWebFrameTest, ThemeColor) {
  registerMockedHttpURLLoad("theme_color_test.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  ThemeColorTestWebFrameClient client;
  webViewHelper.initializeAndLoad(m_baseURL + "theme_color_test.html", true,
                                  &client);
  EXPECT_TRUE(client.didNotify());
  WebLocalFrameImpl* frame = webViewHelper.webView()->mainFrameImpl();
  EXPECT_EQ(0xff0000ff, frame->document().themeColor());
  // Change color by rgb.
  client.reset();
  frame->executeScript(
      WebScriptSource("document.getElementById('tc1').setAttribute('content', "
                      "'rgb(0, 0, 0)');"));
  EXPECT_TRUE(client.didNotify());
  EXPECT_EQ(0xff000000, frame->document().themeColor());
  // Change color by hsl.
  client.reset();
  frame->executeScript(
      WebScriptSource("document.getElementById('tc1').setAttribute('content', "
                      "'hsl(240,100%, 50%)');"));
  EXPECT_TRUE(client.didNotify());
  EXPECT_EQ(0xff0000ff, frame->document().themeColor());
  // Change of second theme-color meta tag will not change frame's theme
  // color.
  client.reset();
  frame->executeScript(WebScriptSource(
      "document.getElementById('tc2').setAttribute('content', '#00FF00');"));
  EXPECT_TRUE(client.didNotify());
  EXPECT_EQ(0xff0000ff, frame->document().themeColor());
}

// Make sure that an embedder-triggered detach with a remote frame parent
// doesn't leave behind dangling pointers.
TEST_P(ParameterizedWebFrameTest, EmbedderTriggeredDetachWithRemoteMainFrame) {
  // FIXME: Refactor some of this logic into WebViewHelper to make it easier to
  // write tests with a top-level remote frame.
  FrameTestHelpers::TestWebViewClient viewClient;
  FrameTestHelpers::TestWebRemoteFrameClient remoteClient;
  WebView* view = WebView::create(&viewClient, WebPageVisibilityStateVisible);
  view->setMainFrame(remoteClient.frame());
  WebLocalFrame* childFrame =
      FrameTestHelpers::createLocalChild(view->mainFrame()->toWebRemoteFrame());

  // Purposely keep the LocalFrame alive so it's the last thing to be destroyed.
  Persistent<Frame> childCoreFrame = childFrame->toImplBase()->frame();
  view->close();
  childCoreFrame.clear();
}

class WebFrameSwapTest : public WebFrameTest {
 protected:
  WebFrameSwapTest() {
    registerMockedHttpURLLoad("frame-a-b-c.html");
    registerMockedHttpURLLoad("subframe-a.html");
    registerMockedHttpURLLoad("subframe-b.html");
    registerMockedHttpURLLoad("subframe-c.html");
    registerMockedHttpURLLoad("subframe-hello.html");

    m_webViewHelper.initializeAndLoad(m_baseURL + "frame-a-b-c.html", true);
  }

  void reset() { m_webViewHelper.reset(); }
  WebFrame* mainFrame() const { return m_webViewHelper.webView()->mainFrame(); }
  WebView* webView() const { return m_webViewHelper.webView(); }

 private:
  FrameTestHelpers::WebViewHelper m_webViewHelper;
};

TEST_F(WebFrameSwapTest, SwapMainFrame) {
  WebRemoteFrame* remoteFrame =
      WebRemoteFrame::create(WebTreeScopeType::Document, nullptr);
  mainFrame()->swap(remoteFrame);

  FrameTestHelpers::TestWebFrameClient client;
  WebLocalFrame* localFrame = WebLocalFrame::createProvisional(
      &client, remoteFrame, WebSandboxFlags::None);
  FrameTestHelpers::TestWebWidgetClient webWidgetClient;
  WebFrameWidget::create(&webWidgetClient, localFrame);
  remoteFrame->swap(localFrame);

  // Finally, make sure an embedder triggered load in the local frame swapped
  // back in works.
  FrameTestHelpers::loadFrame(localFrame, m_baseURL + "subframe-hello.html");

  std::string content =
      WebFrameContentDumper::dumpWebViewAsText(webView(), 1024).utf8();
  EXPECT_EQ("hello", content);

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  reset();
  remoteFrame->close();
}

TEST_F(WebFrameSwapTest, ValidateSizeOnRemoteToLocalMainFrameSwap) {
  WebSize size(111, 222);

  WebRemoteFrame* remoteFrame =
      WebRemoteFrame::create(WebTreeScopeType::Document, nullptr);
  mainFrame()->swap(remoteFrame);

  remoteFrame->view()->resize(size);

  FrameTestHelpers::TestWebFrameClient client;
  WebLocalFrame* localFrame = WebLocalFrame::createProvisional(
      &client, remoteFrame, WebSandboxFlags::None);
  remoteFrame->swap(localFrame);

  // Verify that the size that was set with a remote main frame is correct
  // after swapping to a local frame.
  FrameHost* host =
      toWebViewImpl(localFrame->view())->page()->mainFrame()->host();
  EXPECT_EQ(size.width, host->visualViewport().size().width());
  EXPECT_EQ(size.height, host->visualViewport().size().height());

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  reset();
  remoteFrame->close();
}

namespace {

class SwapMainFrameWhenTitleChangesWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  SwapMainFrameWhenTitleChangesWebFrameClient() : m_remoteFrame(nullptr) {}

  ~SwapMainFrameWhenTitleChangesWebFrameClient() override {
    if (m_remoteFrame)
      m_remoteFrame->close();
  }

  void didReceiveTitle(WebLocalFrame* frame,
                       const WebString&,
                       WebTextDirection) override {
    if (!frame->parent()) {
      m_remoteFrame =
          WebRemoteFrame::create(WebTreeScopeType::Document, nullptr);
      frame->swap(m_remoteFrame);
    }
  }

 private:
  WebRemoteFrame* m_remoteFrame;
};

}  // anonymous namespace

TEST_F(WebFrameTest, SwapMainFrameWhileLoading) {
  SwapMainFrameWhenTitleChangesWebFrameClient frameClient;

  FrameTestHelpers::WebViewHelper webViewHelper;
  registerMockedHttpURLLoad("frame-a-b-c.html");
  registerMockedHttpURLLoad("subframe-a.html");
  registerMockedHttpURLLoad("subframe-b.html");
  registerMockedHttpURLLoad("subframe-c.html");
  registerMockedHttpURLLoad("subframe-hello.html");

  webViewHelper.initializeAndLoad(m_baseURL + "frame-a-b-c.html", true,
                                  &frameClient);
}

void WebFrameTest::swapAndVerifyFirstChildConsistency(const char* const message,
                                                      WebFrame* parent,
                                                      WebFrame* newChild) {
  SCOPED_TRACE(message);
  parent->firstChild()->swap(newChild);

  EXPECT_EQ(newChild, parent->firstChild());
  EXPECT_EQ(newChild->parent(), parent);
  EXPECT_EQ(newChild,
            parent->m_lastChild->m_previousSibling->m_previousSibling);
  EXPECT_EQ(newChild->nextSibling(), parent->m_lastChild->m_previousSibling);
}

TEST_F(WebFrameSwapTest, SwapFirstChild) {
  FrameTestHelpers::TestWebRemoteFrameClient remoteFrameClient;
  WebRemoteFrame* remoteFrame =
      WebRemoteFrame::create(WebTreeScopeType::Document, &remoteFrameClient);
  swapAndVerifyFirstChildConsistency("local->remote", mainFrame(), remoteFrame);

  FrameTestHelpers::TestWebFrameClient client;
  WebLocalFrame* localFrame = WebLocalFrame::createProvisional(
      &client, remoteFrame, WebSandboxFlags::None);
  swapAndVerifyFirstChildConsistency("remote->local", mainFrame(), localFrame);

  // FIXME: This almost certainly fires more load events on the iframe element
  // than it should.
  // Finally, make sure an embedder triggered load in the local frame swapped
  // back in works.
  FrameTestHelpers::loadFrame(localFrame, m_baseURL + "subframe-hello.html");
  std::string content =
      WebFrameContentDumper::dumpWebViewAsText(webView(), 1024).utf8();
  EXPECT_EQ("  \n\nhello\n\nb \n\na\n\nc", content);

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  reset();
  remoteFrame->close();
}

void WebFrameTest::swapAndVerifyMiddleChildConsistency(
    const char* const message,
    WebFrame* parent,
    WebFrame* newChild) {
  SCOPED_TRACE(message);
  parent->firstChild()->nextSibling()->swap(newChild);

  EXPECT_EQ(newChild, parent->firstChild()->nextSibling());
  EXPECT_EQ(newChild, parent->m_lastChild->m_previousSibling);
  EXPECT_EQ(newChild->parent(), parent);
  EXPECT_EQ(newChild, parent->firstChild()->nextSibling());
  EXPECT_EQ(newChild->m_previousSibling, parent->firstChild());
  EXPECT_EQ(newChild, parent->m_lastChild->m_previousSibling);
  EXPECT_EQ(newChild->nextSibling(), parent->m_lastChild);
}

TEST_F(WebFrameSwapTest, SwapMiddleChild) {
  FrameTestHelpers::TestWebRemoteFrameClient remoteFrameClient;
  WebRemoteFrame* remoteFrame =
      WebRemoteFrame::create(WebTreeScopeType::Document, &remoteFrameClient);
  swapAndVerifyMiddleChildConsistency("local->remote", mainFrame(),
                                      remoteFrame);

  FrameTestHelpers::TestWebFrameClient client;
  WebLocalFrame* localFrame = WebLocalFrame::createProvisional(
      &client, remoteFrame, WebSandboxFlags::None);
  swapAndVerifyMiddleChildConsistency("remote->local", mainFrame(), localFrame);

  // FIXME: This almost certainly fires more load events on the iframe element
  // than it should.
  // Finally, make sure an embedder triggered load in the local frame swapped
  // back in works.
  FrameTestHelpers::loadFrame(localFrame, m_baseURL + "subframe-hello.html");
  std::string content =
      WebFrameContentDumper::dumpWebViewAsText(webView(), 1024).utf8();
  EXPECT_EQ("  \n\na\n\nhello\n\nc", content);

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  reset();
  remoteFrame->close();
}

void WebFrameTest::swapAndVerifyLastChildConsistency(const char* const message,
                                                     WebFrame* parent,
                                                     WebFrame* newChild) {
  SCOPED_TRACE(message);
  lastChild(parent)->swap(newChild);

  EXPECT_EQ(newChild, lastChild(parent));
  EXPECT_EQ(newChild->parent(), parent);
  EXPECT_EQ(newChild, parent->firstChild()->nextSibling()->nextSibling());
  EXPECT_EQ(newChild->m_previousSibling, parent->firstChild()->nextSibling());
}

TEST_F(WebFrameSwapTest, SwapLastChild) {
  FrameTestHelpers::TestWebRemoteFrameClient remoteFrameClient;
  WebRemoteFrame* remoteFrame =
      WebRemoteFrame::create(WebTreeScopeType::Document, &remoteFrameClient);
  swapAndVerifyLastChildConsistency("local->remote", mainFrame(), remoteFrame);

  FrameTestHelpers::TestWebFrameClient client;
  WebLocalFrame* localFrame = WebLocalFrame::createProvisional(
      &client, remoteFrame, WebSandboxFlags::None);
  swapAndVerifyLastChildConsistency("remote->local", mainFrame(), localFrame);

  // FIXME: This almost certainly fires more load events on the iframe element
  // than it should.
  // Finally, make sure an embedder triggered load in the local frame swapped
  // back in works.
  FrameTestHelpers::loadFrame(localFrame, m_baseURL + "subframe-hello.html");
  std::string content =
      WebFrameContentDumper::dumpWebViewAsText(webView(), 1024).utf8();
  EXPECT_EQ("  \n\na\n\nb \n\na\n\nhello", content);

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  reset();
  remoteFrame->close();
}

void WebFrameTest::swapAndVerifySubframeConsistency(const char* const message,
                                                    WebFrame* oldFrame,
                                                    WebFrame* newFrame) {
  SCOPED_TRACE(message);

  EXPECT_TRUE(oldFrame->firstChild());
  oldFrame->swap(newFrame);

  EXPECT_FALSE(newFrame->firstChild());
  EXPECT_FALSE(newFrame->m_lastChild);
}

TEST_F(WebFrameSwapTest, SwapParentShouldDetachChildren) {
  FrameTestHelpers::TestWebRemoteFrameClient remoteFrameClient1;
  WebRemoteFrame* remoteFrame =
      WebRemoteFrame::create(WebTreeScopeType::Document, &remoteFrameClient1);
  WebFrame* targetFrame = mainFrame()->firstChild()->nextSibling();
  EXPECT_TRUE(targetFrame);
  swapAndVerifySubframeConsistency("local->remote", targetFrame, remoteFrame);

  targetFrame = mainFrame()->firstChild()->nextSibling();
  EXPECT_TRUE(targetFrame);

  // Create child frames in the target frame before testing the swap.
  FrameTestHelpers::TestWebRemoteFrameClient remoteFrameClient2;
  WebRemoteFrame* childRemoteFrame =
      FrameTestHelpers::createRemoteChild(remoteFrame, &remoteFrameClient2);

  FrameTestHelpers::TestWebFrameClient client;
  WebLocalFrame* localFrame = WebLocalFrame::createProvisional(
      &client, remoteFrame, WebSandboxFlags::None);
  swapAndVerifySubframeConsistency("remote->local", targetFrame, localFrame);

  // FIXME: This almost certainly fires more load events on the iframe element
  // than it should.
  // Finally, make sure an embedder triggered load in the local frame swapped
  // back in works.
  FrameTestHelpers::loadFrame(localFrame, m_baseURL + "subframe-hello.html");
  std::string content =
      WebFrameContentDumper::dumpWebViewAsText(webView(), 1024).utf8();
  EXPECT_EQ("  \n\na\n\nhello\n\nc", content);

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  reset();
  remoteFrame->close();
  childRemoteFrame->close();
}

TEST_F(WebFrameSwapTest, SwapPreservesGlobalContext) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Value> windowTop =
      mainFrame()->executeScriptAndReturnValue(WebScriptSource("window"));
  ASSERT_TRUE(windowTop->IsObject());
  v8::Local<v8::Value> originalWindow =
      mainFrame()->executeScriptAndReturnValue(
          WebScriptSource("document.querySelector('#frame2').contentWindow;"));
  ASSERT_TRUE(originalWindow->IsObject());

  // Make sure window reference stays the same when swapping to a remote frame.
  FrameTestHelpers::TestWebRemoteFrameClient remoteClient;
  WebRemoteFrame* remoteFrame = remoteClient.frame();
  WebFrame* targetFrame = mainFrame()->firstChild()->nextSibling();
  targetFrame->swap(remoteFrame);
  remoteFrame->setReplicatedOrigin(SecurityOrigin::createUnique());
  v8::Local<v8::Value> remoteWindow = mainFrame()->executeScriptAndReturnValue(
      WebScriptSource("document.querySelector('#frame2').contentWindow;"));
  EXPECT_TRUE(originalWindow->StrictEquals(remoteWindow));
  // Check that its view is consistent with the world.
  v8::Local<v8::Value> remoteWindowTop =
      mainFrame()->executeScriptAndReturnValue(WebScriptSource(
          "document.querySelector('#frame2').contentWindow.top;"));
  EXPECT_TRUE(windowTop->StrictEquals(remoteWindowTop));

  // Now check that remote -> local works too, since it goes through a different
  // code path.
  FrameTestHelpers::TestWebFrameClient client;
  WebLocalFrame* localFrame = WebLocalFrame::createProvisional(
      &client, remoteFrame, WebSandboxFlags::None);
  remoteFrame->swap(localFrame);
  v8::Local<v8::Value> localWindow = mainFrame()->executeScriptAndReturnValue(
      WebScriptSource("document.querySelector('#frame2').contentWindow;"));
  EXPECT_TRUE(originalWindow->StrictEquals(localWindow));
  v8::Local<v8::Value> localWindowTop =
      mainFrame()->executeScriptAndReturnValue(WebScriptSource(
          "document.querySelector('#frame2').contentWindow.top;"));
  EXPECT_TRUE(windowTop->StrictEquals(localWindowTop));

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  reset();
}

TEST_F(WebFrameSwapTest, SwapInitializesGlobal) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  v8::Local<v8::Value> windowTop =
      mainFrame()->executeScriptAndReturnValue(WebScriptSource("window"));
  ASSERT_TRUE(windowTop->IsObject());

  v8::Local<v8::Value> lastChild = mainFrame()->executeScriptAndReturnValue(
      WebScriptSource("saved = window[2]"));
  ASSERT_TRUE(lastChild->IsObject());

  FrameTestHelpers::TestWebRemoteFrameClient remoteClient;
  WebRemoteFrame* remoteFrame = remoteClient.frame();
  WebFrameTest::lastChild(mainFrame())->swap(remoteFrame);
  remoteFrame->setReplicatedOrigin(SecurityOrigin::createUnique());
  v8::Local<v8::Value> remoteWindowTop =
      mainFrame()->executeScriptAndReturnValue(WebScriptSource("saved.top"));
  EXPECT_TRUE(remoteWindowTop->IsObject());
  EXPECT_TRUE(windowTop->StrictEquals(remoteWindowTop));

  FrameTestHelpers::TestWebFrameClient client;
  WebLocalFrame* localFrame = WebLocalFrame::createProvisional(
      &client, remoteFrame, WebSandboxFlags::None);
  remoteFrame->swap(localFrame);
  v8::Local<v8::Value> localWindowTop =
      mainFrame()->executeScriptAndReturnValue(WebScriptSource("saved.top"));
  EXPECT_TRUE(localWindowTop->IsObject());
  EXPECT_TRUE(windowTop->StrictEquals(localWindowTop));

  reset();
}

TEST_F(WebFrameSwapTest, RemoteFramesAreIndexable) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  FrameTestHelpers::TestWebRemoteFrameClient remoteClient;
  WebRemoteFrame* remoteFrame = remoteClient.frame();
  lastChild(mainFrame())->swap(remoteFrame);
  remoteFrame->setReplicatedOrigin(SecurityOrigin::createUnique());
  v8::Local<v8::Value> remoteWindow =
      mainFrame()->executeScriptAndReturnValue(WebScriptSource("window[2]"));
  EXPECT_TRUE(remoteWindow->IsObject());
  v8::Local<v8::Value> windowLength = mainFrame()->executeScriptAndReturnValue(
      WebScriptSource("window.length"));
  ASSERT_TRUE(windowLength->IsInt32());
  EXPECT_EQ(3, windowLength.As<v8::Int32>()->Value());

  reset();
}

TEST_F(WebFrameSwapTest, RemoteFrameLengthAccess) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  FrameTestHelpers::TestWebRemoteFrameClient remoteClient;
  WebRemoteFrame* remoteFrame = remoteClient.frame();
  lastChild(mainFrame())->swap(remoteFrame);
  remoteFrame->setReplicatedOrigin(SecurityOrigin::createUnique());
  v8::Local<v8::Value> remoteWindowLength =
      mainFrame()->executeScriptAndReturnValue(
          WebScriptSource("window[2].length"));
  ASSERT_TRUE(remoteWindowLength->IsInt32());
  EXPECT_EQ(0, remoteWindowLength.As<v8::Int32>()->Value());

  reset();
}

TEST_F(WebFrameSwapTest, RemoteWindowNamedAccess) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  // FIXME: Once OOPIF unit test infrastructure is in place, test that named
  // window access on a remote window works. For now, just test that accessing
  // a named property doesn't crash.
  FrameTestHelpers::TestWebRemoteFrameClient remoteClient;
  WebRemoteFrame* remoteFrame = remoteClient.frame();
  lastChild(mainFrame())->swap(remoteFrame);
  remoteFrame->setReplicatedOrigin(SecurityOrigin::createUnique());
  v8::Local<v8::Value> remoteWindowProperty =
      mainFrame()->executeScriptAndReturnValue(
          WebScriptSource("window[2].foo"));
  EXPECT_TRUE(remoteWindowProperty.IsEmpty());

  reset();
}

// TODO(alexmos, dcheng): This test and some other OOPIF tests use
// very little of the test fixture support in WebFrameSwapTest.  We should
// clean these tests up.
TEST_F(WebFrameSwapTest, FramesOfRemoteParentAreIndexable) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  FrameTestHelpers::TestWebRemoteFrameClient remoteClient;
  WebRemoteFrame* remoteParentFrame = remoteClient.frame();
  mainFrame()->swap(remoteParentFrame);
  remoteParentFrame->setReplicatedOrigin(SecurityOrigin::createUnique());

  WebLocalFrame* childFrame =
      FrameTestHelpers::createLocalChild(remoteParentFrame);
  FrameTestHelpers::loadFrame(childFrame, m_baseURL + "subframe-hello.html");

  v8::Local<v8::Value> window =
      childFrame->executeScriptAndReturnValue(WebScriptSource("window"));
  v8::Local<v8::Value> childOfRemoteParent =
      childFrame->executeScriptAndReturnValue(
          WebScriptSource("parent.frames[0]"));
  EXPECT_TRUE(childOfRemoteParent->IsObject());
  EXPECT_TRUE(window->StrictEquals(childOfRemoteParent));

  v8::Local<v8::Value> windowLength = childFrame->executeScriptAndReturnValue(
      WebScriptSource("parent.frames.length"));
  ASSERT_TRUE(windowLength->IsInt32());
  EXPECT_EQ(1, windowLength.As<v8::Int32>()->Value());

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // clients.
  reset();
}

// Check that frames with a remote parent don't crash while accessing
// window.frameElement.
TEST_F(WebFrameSwapTest, FrameElementInFramesWithRemoteParent) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  FrameTestHelpers::TestWebRemoteFrameClient remoteClient;
  WebRemoteFrame* remoteParentFrame = remoteClient.frame();
  mainFrame()->swap(remoteParentFrame);
  remoteParentFrame->setReplicatedOrigin(SecurityOrigin::createUnique());

  WebLocalFrame* childFrame =
      FrameTestHelpers::createLocalChild(remoteParentFrame);
  FrameTestHelpers::loadFrame(childFrame, m_baseURL + "subframe-hello.html");

  v8::Local<v8::Value> frameElement = childFrame->executeScriptAndReturnValue(
      WebScriptSource("window.frameElement"));
  // frameElement should be null if cross-origin.
  ASSERT_FALSE(frameElement.IsEmpty());
  EXPECT_TRUE(frameElement->IsNull());

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // clients.
  reset();
}

class RemoteToLocalSwapWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  explicit RemoteToLocalSwapWebFrameClient(WebRemoteFrame* remoteFrame)
      : m_historyCommitType(WebHistoryInertCommit),
        m_remoteFrame(remoteFrame) {}

  void didCommitProvisionalLoad(
      WebLocalFrame* frame,
      const WebHistoryItem&,
      WebHistoryCommitType historyCommitType) override {
    m_historyCommitType = historyCommitType;
    m_remoteFrame->swap(frame);
  }

  WebHistoryCommitType historyCommitType() const { return m_historyCommitType; }

  WebHistoryCommitType m_historyCommitType;
  WebRemoteFrame* m_remoteFrame;
};

// The commit type should be Initial if we are swapping a RemoteFrame to a
// LocalFrame as it is first being created.  This happens when another frame
// exists in the same process, such that we create the RemoteFrame before the
// first navigation occurs.
TEST_F(WebFrameSwapTest, HistoryCommitTypeAfterNewRemoteToLocalSwap) {
  FrameTestHelpers::TestWebRemoteFrameClient remoteFrameClient;
  WebRemoteFrame* remoteFrame =
      WebRemoteFrame::create(WebTreeScopeType::Document, &remoteFrameClient);
  WebFrame* targetFrame = mainFrame()->firstChild();
  ASSERT_TRUE(targetFrame);
  targetFrame->swap(remoteFrame);
  ASSERT_TRUE(mainFrame()->firstChild());
  ASSERT_EQ(mainFrame()->firstChild(), remoteFrame);

  RemoteToLocalSwapWebFrameClient client(remoteFrame);
  WebLocalFrame* localFrame = WebLocalFrame::createProvisional(
      &client, remoteFrame, WebSandboxFlags::None);
  FrameTestHelpers::loadFrame(localFrame, m_baseURL + "subframe-hello.html");
  EXPECT_EQ(WebInitialCommitInChildFrame, client.historyCommitType());

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  reset();
  remoteFrame->close();
}

// The commit type should be Standard if we are swapping a RemoteFrame to a
// LocalFrame after commits have already happened in the frame.  The browser
// process will inform us via setCommittedFirstRealLoad.
TEST_F(WebFrameSwapTest, HistoryCommitTypeAfterExistingRemoteToLocalSwap) {
  FrameTestHelpers::TestWebRemoteFrameClient remoteFrameClient;
  WebRemoteFrame* remoteFrame =
      WebRemoteFrame::create(WebTreeScopeType::Document, &remoteFrameClient);
  WebFrame* targetFrame = mainFrame()->firstChild();
  ASSERT_TRUE(targetFrame);
  targetFrame->swap(remoteFrame);
  ASSERT_TRUE(mainFrame()->firstChild());
  ASSERT_EQ(mainFrame()->firstChild(), remoteFrame);

  RemoteToLocalSwapWebFrameClient client(remoteFrame);
  WebLocalFrame* localFrame = WebLocalFrame::createProvisional(
      &client, remoteFrame, WebSandboxFlags::None);
  localFrame->setCommittedFirstRealLoad();
  FrameTestHelpers::loadFrame(localFrame, m_baseURL + "subframe-hello.html");
  EXPECT_EQ(WebStandardCommit, client.historyCommitType());

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  reset();
  remoteFrame->close();
}

// The uniqueName should be preserved when swapping to a RemoteFrame and back,
// whether the frame has a name or not.
TEST_F(WebFrameSwapTest, UniqueNameAfterRemoteToLocalSwap) {
  // Start with a named frame.
  WebFrame* targetFrame = mainFrame()->firstChild();
  ASSERT_TRUE(targetFrame);
  WebString uniqueName = targetFrame->uniqueName();
  EXPECT_EQ("frame1", uniqueName.utf8());

  // Swap to a RemoteFrame.
  FrameTestHelpers::TestWebRemoteFrameClient remoteFrameClient;
  WebRemoteFrameImpl* remoteFrame = WebRemoteFrameImpl::create(
      WebTreeScopeType::Document, &remoteFrameClient);
  targetFrame->swap(remoteFrame);
  ASSERT_TRUE(mainFrame()->firstChild());
  ASSERT_EQ(mainFrame()->firstChild(), remoteFrame);
  EXPECT_EQ(uniqueName.utf8(),
            WebString(remoteFrame->frame()->tree().uniqueName()).utf8());

  // Swap back to a LocalFrame.
  RemoteToLocalSwapWebFrameClient client(remoteFrame);
  WebLocalFrame* localFrame = WebLocalFrame::createProvisional(
      &client, remoteFrame, WebSandboxFlags::None);
  FrameTestHelpers::loadFrame(localFrame, m_baseURL + "subframe-hello.html");
  EXPECT_EQ(uniqueName.utf8(), localFrame->uniqueName().utf8());
  EXPECT_EQ(uniqueName.utf8(), WebString(toWebLocalFrameImpl(localFrame)
                                             ->frame()
                                             ->loader()
                                             .currentItem()
                                             ->target())
                                   .utf8());

  // Repeat with no name on the frame.
  // (note that uniqueName is immutable after first real commit).
  localFrame->setName("");
  WebString uniqueName2 = localFrame->uniqueName();
  EXPECT_EQ("frame1", uniqueName2.utf8());

  FrameTestHelpers::TestWebRemoteFrameClient remoteFrameClient2;
  WebRemoteFrameImpl* remoteFrame2 = WebRemoteFrameImpl::create(
      WebTreeScopeType::Document, &remoteFrameClient2);
  localFrame->swap(remoteFrame2);
  ASSERT_TRUE(mainFrame()->firstChild());
  ASSERT_EQ(mainFrame()->firstChild(), remoteFrame2);
  EXPECT_EQ(uniqueName2.utf8(),
            WebString(remoteFrame2->frame()->tree().uniqueName()).utf8());

  RemoteToLocalSwapWebFrameClient client2(remoteFrame2);
  WebLocalFrame* localFrame2 = WebLocalFrame::createProvisional(
      &client2, remoteFrame2, WebSandboxFlags::None);
  FrameTestHelpers::loadFrame(localFrame2, m_baseURL + "subframe-hello.html");
  EXPECT_EQ(uniqueName2.utf8(), localFrame2->uniqueName().utf8());
  EXPECT_EQ(uniqueName2.utf8(), WebString(toWebLocalFrameImpl(localFrame2)
                                              ->frame()
                                              ->loader()
                                              .currentItem()
                                              ->target())
                                    .utf8());

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  reset();
  remoteFrame->close();
  remoteFrame2->close();
}

class RemoteNavigationClient
    : public FrameTestHelpers::TestWebRemoteFrameClient {
 public:
  void navigate(const WebURLRequest& request,
                bool shouldReplaceCurrentEntry) override {
    m_lastRequest = request;
  }

  const WebURLRequest& lastRequest() const { return m_lastRequest; }

 private:
  WebURLRequest m_lastRequest;
};

TEST_F(WebFrameSwapTest, NavigateRemoteFrameViaLocation) {
  RemoteNavigationClient client;
  WebRemoteFrame* remoteFrame = client.frame();
  WebFrame* targetFrame = mainFrame()->firstChild();
  ASSERT_TRUE(targetFrame);
  targetFrame->swap(remoteFrame);
  ASSERT_TRUE(mainFrame()->firstChild());
  ASSERT_EQ(mainFrame()->firstChild(), remoteFrame);

  remoteFrame->setReplicatedOrigin(
      WebSecurityOrigin::createFromString("http://127.0.0.1"));
  mainFrame()->executeScript(
      WebScriptSource("document.getElementsByTagName('iframe')[0]."
                      "contentWindow.location = 'data:text/html,hi'"));
  ASSERT_FALSE(client.lastRequest().isNull());
  EXPECT_EQ(WebURL(toKURL("data:text/html,hi")), client.lastRequest().url());

  // Manually reset to break WebViewHelper's dependency on the stack allocated
  // TestWebFrameClient.
  reset();
}

TEST_F(WebFrameSwapTest, WindowOpenOnRemoteFrame) {
  RemoteNavigationClient remoteClient;
  WebRemoteFrame* remoteFrame = remoteClient.frame();
  mainFrame()->firstChild()->swap(remoteFrame);
  remoteFrame->setReplicatedOrigin(
      WebSecurityOrigin::createFromString("http://127.0.0.1"));

  ASSERT_TRUE(mainFrame()->isWebLocalFrame());
  ASSERT_TRUE(mainFrame()->firstChild()->isWebRemoteFrame());
  LocalDOMWindow* mainWindow =
      toWebLocalFrameImpl(mainFrame())->frame()->localDOMWindow();

  KURL destination = toKURL("data:text/html:destination");
  mainWindow->open(destination.getString(), "frame1", "", mainWindow,
                   mainWindow);
  ASSERT_FALSE(remoteClient.lastRequest().isNull());
  EXPECT_EQ(remoteClient.lastRequest().url(), WebURL(destination));

  // Pointing a named frame to an empty URL should just return a reference to
  // the frame's window without navigating it.
  DOMWindow* result =
      mainWindow->open("", "frame1", "", mainWindow, mainWindow);
  EXPECT_EQ(remoteClient.lastRequest().url(), WebURL(destination));
  EXPECT_EQ(result, remoteFrame->toImplBase()->frame()->domWindow());

  reset();
}

class RemoteWindowCloseClient : public FrameTestHelpers::TestWebViewClient {
 public:
  RemoteWindowCloseClient() : m_closed(false) {}

  void closeWidgetSoon() override { m_closed = true; }

  bool closed() const { return m_closed; }

 private:
  bool m_closed;
};

TEST_F(WebFrameTest, WindowOpenRemoteClose) {
  FrameTestHelpers::WebViewHelper mainWebView;
  mainWebView.initialize(true);

  // Create a remote window that will be closed later in the test.
  RemoteWindowCloseClient viewClient;
  FrameTestHelpers::TestWebRemoteFrameClient frameClient;
  WebRemoteFrameImpl* webRemoteFrame = frameClient.frame();

  WebView* view = WebView::create(&viewClient, WebPageVisibilityStateVisible);
  view->setMainFrame(webRemoteFrame);
  view->mainFrame()->setOpener(mainWebView.webView()->mainFrame());
  webRemoteFrame->setReplicatedOrigin(
      WebSecurityOrigin::createFromString("http://127.0.0.1"));

  LocalFrame* localFrame =
      toLocalFrame(mainWebView.webView()->mainFrame()->toImplBase()->frame());
  RemoteFrame* remoteFrame = webRemoteFrame->frame();

  // Attempt to close the window, which should fail as it isn't opened
  // by a script.
  remoteFrame->domWindow()->close(localFrame->document());
  EXPECT_FALSE(viewClient.closed());

  // Marking it as opened by a script should now allow it to be closed.
  remoteFrame->page()->setOpenedByDOM();
  remoteFrame->domWindow()->close(localFrame->document());
  EXPECT_TRUE(viewClient.closed());

  view->close();
}

TEST_F(WebFrameTest, NavigateRemoteToLocalWithOpener) {
  FrameTestHelpers::WebViewHelper mainWebView;
  mainWebView.initialize(true);
  WebFrame* mainFrame = mainWebView.webView()->mainFrame();

  // Create a popup with a remote frame and set its opener to the main frame.
  FrameTestHelpers::TestWebViewClient popupViewClient;
  WebView* popupView =
      WebView::create(&popupViewClient, WebPageVisibilityStateVisible);
  FrameTestHelpers::TestWebRemoteFrameClient popupRemoteClient;
  WebRemoteFrame* popupRemoteFrame = popupRemoteClient.frame();
  popupView->setMainFrame(popupRemoteFrame);
  popupRemoteFrame->setOpener(mainFrame);
  popupRemoteFrame->setReplicatedOrigin(
      WebSecurityOrigin::createFromString("http://foo.com"));
  EXPECT_FALSE(mainFrame->getSecurityOrigin().canAccess(
      popupView->mainFrame()->getSecurityOrigin()));

  // Do a remote-to-local swap in the popup.
  FrameTestHelpers::TestWebFrameClient popupLocalClient;
  WebLocalFrame* popupLocalFrame = WebLocalFrame::createProvisional(
      &popupLocalClient, popupRemoteFrame, WebSandboxFlags::None);
  popupRemoteFrame->swap(popupLocalFrame);

  // The initial document created during the remote-to-local swap should have
  // inherited its opener's SecurityOrigin.
  EXPECT_TRUE(mainFrame->getSecurityOrigin().canAccess(
      popupView->mainFrame()->getSecurityOrigin()));

  popupView->close();
}

TEST_F(WebFrameTest, SwapWithOpenerCycle) {
  // First, create a remote main frame with itself as the opener.
  FrameTestHelpers::TestWebViewClient viewClient;
  WebView* view = WebView::create(&viewClient, WebPageVisibilityStateVisible);
  FrameTestHelpers::TestWebRemoteFrameClient remoteClient;
  WebRemoteFrame* remoteFrame = remoteClient.frame();
  view->setMainFrame(remoteFrame);
  remoteFrame->setOpener(remoteFrame);

  // Now swap in a local frame. It shouldn't crash.
  FrameTestHelpers::TestWebFrameClient localClient;
  WebLocalFrame* localFrame = WebLocalFrame::createProvisional(
      &localClient, remoteFrame, WebSandboxFlags::None);
  remoteFrame->swap(localFrame);

  // And the opener cycle should still be preserved.
  EXPECT_EQ(localFrame, localFrame->opener());

  view->close();
}

class CommitTypeWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
 public:
  explicit CommitTypeWebFrameClient()
      : m_historyCommitType(WebHistoryInertCommit) {}

  void didCommitProvisionalLoad(
      WebLocalFrame*,
      const WebHistoryItem&,
      WebHistoryCommitType historyCommitType) override {
    m_historyCommitType = historyCommitType;
  }

  WebHistoryCommitType historyCommitType() const { return m_historyCommitType; }

 private:
  WebHistoryCommitType m_historyCommitType;
};

TEST_P(ParameterizedWebFrameTest, RemoteFrameInitialCommitType) {
  FrameTestHelpers::TestWebViewClient viewClient;
  FrameTestHelpers::TestWebRemoteFrameClient remoteClient;
  WebView* view = WebView::create(&viewClient, WebPageVisibilityStateVisible);
  view->setMainFrame(remoteClient.frame());
  remoteClient.frame()->setReplicatedOrigin(
      WebSecurityOrigin::createFromString(WebString::fromUTF8(m_baseURL)));

  // If an iframe has a remote main frame, ensure the inital commit is correctly
  // identified as WebInitialCommitInChildFrame.
  CommitTypeWebFrameClient childFrameClient;
  WebLocalFrame* childFrame = FrameTestHelpers::createLocalChild(
      view->mainFrame()->toWebRemoteFrame(), "frameName", &childFrameClient);
  registerMockedHttpURLLoad("foo.html");
  FrameTestHelpers::loadFrame(childFrame, m_baseURL + "foo.html");
  EXPECT_EQ(WebInitialCommitInChildFrame, childFrameClient.historyCommitType());
  view->close();
}

class GestureEventTestWebWidgetClient
    : public FrameTestHelpers::TestWebWidgetClient {
 public:
  GestureEventTestWebWidgetClient() : m_didHandleGestureEvent(false) {}
  void didHandleGestureEvent(const WebGestureEvent& event,
                             bool eventCancelled) override {
    m_didHandleGestureEvent = true;
  }
  bool didHandleGestureEvent() const { return m_didHandleGestureEvent; }

 private:
  bool m_didHandleGestureEvent;
};

TEST_P(ParameterizedWebFrameTest, FrameWidgetTest) {
  FrameTestHelpers::TestWebViewClient viewClient;
  WebView* view = WebView::create(&viewClient, WebPageVisibilityStateVisible);

  FrameTestHelpers::TestWebRemoteFrameClient remoteClient;
  view->setMainFrame(remoteClient.frame());

  GestureEventTestWebWidgetClient childWidgetClient;
  WebLocalFrame* childFrame = FrameTestHelpers::createLocalChild(
      view->mainFrame()->toWebRemoteFrame(), WebString(), nullptr,
      &childWidgetClient);

  view->resize(WebSize(1000, 1000));

  childFrame->frameWidget()->handleInputEvent(fatTap(20, 20));
  EXPECT_TRUE(childWidgetClient.didHandleGestureEvent());

  view->close();
}

class MockDocumentThreadableLoaderClient
    : public DocumentThreadableLoaderClient {
 public:
  MockDocumentThreadableLoaderClient() : m_failed(false) {}
  void didFail(const ResourceError&) override { m_failed = true; }

  void reset() { m_failed = false; }
  bool failed() { return m_failed; }

  bool m_failed;
};

// FIXME: This would be better as a unittest on DocumentThreadableLoader but it
// requires spin-up of a frame. It may be possible to remove that requirement
// and convert it to a unittest.
TEST_P(ParameterizedWebFrameTest, LoaderOriginAccess) {
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad("about:blank");

  SchemeRegistry::registerURLSchemeAsDisplayIsolated("chrome");

  // Cross-origin request.
  KURL resourceUrl(ParsedURLString, "chrome://test.pdf");
  ResourceRequest request(resourceUrl);
  request.setRequestContext(WebURLRequest::RequestContextObject);
  registerMockedChromeURLLoad("test.pdf");

  LocalFrame* frame(toLocalFrame(webViewHelper.webView()->page()->mainFrame()));

  MockDocumentThreadableLoaderClient client;
  ThreadableLoaderOptions options;

  // First try to load the request with regular access. Should fail.
  options.crossOriginRequestPolicy = UseAccessControl;
  ResourceLoaderOptions resourceLoaderOptions;
  DocumentThreadableLoader::loadResourceSynchronously(
      *frame->document(), request, client, options, resourceLoaderOptions);
  EXPECT_TRUE(client.failed());

  client.reset();
  // Try to load the request with cross origin access. Should succeed.
  options.crossOriginRequestPolicy = AllowCrossOriginRequests;
  DocumentThreadableLoader::loadResourceSynchronously(
      *frame->document(), request, client, options, resourceLoaderOptions);
  EXPECT_FALSE(client.failed());
}

TEST_P(ParameterizedWebFrameTest, DetachRemoteFrame) {
  FrameTestHelpers::TestWebViewClient viewClient;
  FrameTestHelpers::TestWebRemoteFrameClient remoteClient;
  WebView* view = WebView::create(&viewClient, WebPageVisibilityStateVisible);
  view->setMainFrame(remoteClient.frame());
  FrameTestHelpers::TestWebRemoteFrameClient childFrameClient;
  WebRemoteFrame* childFrame = FrameTestHelpers::createRemoteChild(
      view->mainFrame()->toWebRemoteFrame(), &childFrameClient);
  childFrame->detach();
  view->close();
  childFrame->close();
}

class TestConsoleMessageWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  virtual void didAddMessageToConsole(const WebConsoleMessage& message,
                                      const WebString& sourceName,
                                      unsigned sourceLine,
                                      const WebString& stackTrace) {
    messages.append(message);
  }

  Vector<WebConsoleMessage> messages;
};

TEST_P(ParameterizedWebFrameTest, CrossDomainAccessErrorsUseCallingWindow) {
  registerMockedHttpURLLoad("hidden_frames.html");
  registerMockedChromeURLLoad("hello_world.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  TestConsoleMessageWebFrameClient webFrameClient;
  FrameTestHelpers::TestWebViewClient webViewClient;
  webViewHelper.initializeAndLoad(m_baseURL + "hidden_frames.html", true,
                                  &webFrameClient, &webViewClient);

  // Create another window with a cross-origin page, and point its opener to
  // first window.
  FrameTestHelpers::WebViewHelper popupWebViewHelper;
  TestConsoleMessageWebFrameClient popupWebFrameClient;
  WebView* popupView = popupWebViewHelper.initializeAndLoad(
      m_chromeURL + "hello_world.html", true, &popupWebFrameClient);
  popupView->mainFrame()->setOpener(webViewHelper.webView()->mainFrame());

  // Attempt a blocked navigation of an opener's subframe, and ensure that
  // the error shows up on the popup (calling) window's console, rather than
  // the target window.
  popupView->mainFrame()->executeScript(WebScriptSource(
      "try { opener.frames[1].location.href='data:text/html,foo'; } catch (e) "
      "{}"));
  EXPECT_TRUE(webFrameClient.messages.isEmpty());
  ASSERT_EQ(1u, popupWebFrameClient.messages.size());
  EXPECT_TRUE(std::string::npos !=
              popupWebFrameClient.messages[0].text.utf8().find(
                  "Unsafe JavaScript attempt to initiate navigation"));

  // Try setting a cross-origin iframe element's source to a javascript: URL,
  // and check that this error is also printed on the calling window.
  popupView->mainFrame()->executeScript(
      WebScriptSource("opener.document.querySelectorAll('iframe')[1].src='"
                      "javascript:alert()'"));
  EXPECT_TRUE(webFrameClient.messages.isEmpty());
  ASSERT_EQ(2u, popupWebFrameClient.messages.size());
  EXPECT_TRUE(
      std::string::npos !=
      popupWebFrameClient.messages[1].text.utf8().find("Blocked a frame"));

  // Manually reset to break WebViewHelpers' dependencies on the stack
  // allocated WebFrameClients.
  webViewHelper.reset();
  popupWebViewHelper.reset();
}

TEST_P(ParameterizedWebFrameTest, ResizeInvalidatesDeviceMediaQueries) {
  registerMockedHttpURLLoad("device_media_queries.html");
  FixedLayoutTestWebViewClient client;
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "device_media_queries.html", true,
                                  nullptr, &client, nullptr, configureAndroid);
  LocalFrame* frame =
      toLocalFrame(webViewHelper.webView()->page()->mainFrame());
  Element* element = frame->document()->getElementById("test");
  ASSERT_TRUE(element);

  client.m_screenInfo.rect = WebRect(0, 0, 700, 500);
  client.m_screenInfo.availableRect = client.m_screenInfo.rect;
  webViewHelper.resize(WebSize(700, 500));
  EXPECT_EQ(300, element->offsetWidth());
  EXPECT_EQ(300, element->offsetHeight());

  client.m_screenInfo.rect = WebRect(0, 0, 710, 500);
  client.m_screenInfo.availableRect = client.m_screenInfo.rect;
  webViewHelper.resize(WebSize(710, 500));
  EXPECT_EQ(400, element->offsetWidth());
  EXPECT_EQ(300, element->offsetHeight());

  client.m_screenInfo.rect = WebRect(0, 0, 690, 500);
  client.m_screenInfo.availableRect = client.m_screenInfo.rect;
  webViewHelper.resize(WebSize(690, 500));
  EXPECT_EQ(200, element->offsetWidth());
  EXPECT_EQ(300, element->offsetHeight());

  client.m_screenInfo.rect = WebRect(0, 0, 700, 510);
  client.m_screenInfo.availableRect = client.m_screenInfo.rect;
  webViewHelper.resize(WebSize(700, 510));
  EXPECT_EQ(300, element->offsetWidth());
  EXPECT_EQ(400, element->offsetHeight());

  client.m_screenInfo.rect = WebRect(0, 0, 700, 490);
  client.m_screenInfo.availableRect = client.m_screenInfo.rect;
  webViewHelper.resize(WebSize(700, 490));
  EXPECT_EQ(300, element->offsetWidth());
  EXPECT_EQ(200, element->offsetHeight());

  client.m_screenInfo.rect = WebRect(0, 0, 690, 510);
  client.m_screenInfo.availableRect = client.m_screenInfo.rect;
  webViewHelper.resize(WebSize(690, 510));
  EXPECT_EQ(200, element->offsetWidth());
  EXPECT_EQ(400, element->offsetHeight());
}

class DeviceEmulationTest : public ParameterizedWebFrameTest {
 protected:
  DeviceEmulationTest() {
    registerMockedHttpURLLoad("device_emulation.html");
    m_client.m_screenInfo.deviceScaleFactor = 1;
    m_webViewHelper.initializeAndLoad(m_baseURL + "device_emulation.html", true,
                                      0, &m_client);
  }

  void testResize(const WebSize size, const String& expectedSize) {
    m_client.m_screenInfo.rect = WebRect(0, 0, size.width, size.height);
    m_client.m_screenInfo.availableRect = m_client.m_screenInfo.rect;
    m_webViewHelper.resize(size);
    EXPECT_EQ(expectedSize, dumpSize("test"));
  }

  String dumpSize(const String& id) {
    String code = "dumpSize('" + id + "')";
    v8::HandleScope scope(v8::Isolate::GetCurrent());
    ScriptExecutionCallbackHelper callbackHelper(
        m_webViewHelper.webView()->mainFrame()->mainWorldScriptContext());
    m_webViewHelper.webView()
        ->mainFrameImpl()
        ->requestExecuteScriptAndReturnValue(WebScriptSource(WebString(code)),
                                             false, &callbackHelper);
    runPendingTasks();
    EXPECT_TRUE(callbackHelper.didComplete());
    return callbackHelper.stringValue();
  }

  FixedLayoutTestWebViewClient m_client;
  FrameTestHelpers::WebViewHelper m_webViewHelper;
};

INSTANTIATE_TEST_CASE_P(All, DeviceEmulationTest, ::testing::Bool());

TEST_P(DeviceEmulationTest, DeviceSizeInvalidatedOnResize) {
  WebDeviceEmulationParams params;
  params.screenPosition = WebDeviceEmulationParams::Mobile;
  m_webViewHelper.webView()->enableDeviceEmulation(params);

  testResize(WebSize(700, 500), "300x300");
  testResize(WebSize(710, 500), "400x300");
  testResize(WebSize(690, 500), "200x300");
  testResize(WebSize(700, 510), "300x400");
  testResize(WebSize(700, 490), "300x200");
  testResize(WebSize(710, 510), "400x400");
  testResize(WebSize(690, 490), "200x200");
  testResize(WebSize(800, 600), "400x400");

  m_webViewHelper.webView()->disableDeviceEmulation();
}

TEST_P(DeviceEmulationTest, PointerAndHoverTypes) {
  WebDeviceEmulationParams params;
  params.screenPosition = WebDeviceEmulationParams::Mobile;
  m_webViewHelper.webView()->enableDeviceEmulation(params);
  EXPECT_EQ("20x20", dumpSize("pointer"));
  m_webViewHelper.webView()->disableDeviceEmulation();
}

TEST_P(ParameterizedWebFrameTest, CreateLocalChildWithPreviousSibling) {
  FrameTestHelpers::TestWebViewClient viewClient;
  FrameTestHelpers::TestWebRemoteFrameClient remoteClient;
  WebView* view = WebView::create(&viewClient, WebPageVisibilityStateVisible);
  view->setMainFrame(remoteClient.frame());
  WebRemoteFrame* parent = view->mainFrame()->toWebRemoteFrame();

  WebLocalFrame* secondFrame(
      FrameTestHelpers::createLocalChild(parent, "name2"));
  WebLocalFrame* fourthFrame(FrameTestHelpers::createLocalChild(
      parent, "name4", nullptr, nullptr, secondFrame));
  WebLocalFrame* thirdFrame(FrameTestHelpers::createLocalChild(
      parent, "name3", nullptr, nullptr, secondFrame));
  WebLocalFrame* firstFrame(
      FrameTestHelpers::createLocalChild(parent, "name1"));

  EXPECT_EQ(firstFrame, parent->firstChild());
  EXPECT_EQ(nullptr, previousSibling(firstFrame));
  EXPECT_EQ(secondFrame, firstFrame->nextSibling());

  EXPECT_EQ(firstFrame, previousSibling(secondFrame));
  EXPECT_EQ(thirdFrame, secondFrame->nextSibling());

  EXPECT_EQ(secondFrame, previousSibling(thirdFrame));
  EXPECT_EQ(fourthFrame, thirdFrame->nextSibling());

  EXPECT_EQ(thirdFrame, previousSibling(fourthFrame));
  EXPECT_EQ(nullptr, fourthFrame->nextSibling());
  EXPECT_EQ(fourthFrame, lastChild(parent));

  EXPECT_EQ(parent, firstFrame->parent());
  EXPECT_EQ(parent, secondFrame->parent());
  EXPECT_EQ(parent, thirdFrame->parent());
  EXPECT_EQ(parent, fourthFrame->parent());

  view->close();
}

TEST_P(ParameterizedWebFrameTest, SendBeaconFromChildWithRemoteMainFrame) {
  FrameTestHelpers::TestWebViewClient viewClient;
  FrameTestHelpers::TestWebRemoteFrameClient remoteClient;
  WebView* view = WebView::create(&viewClient, WebPageVisibilityStateVisible);
  view->settings()->setJavaScriptEnabled(true);
  view->setMainFrame(remoteClient.frame());
  WebRemoteFrame* root = view->mainFrame()->toWebRemoteFrame();
  root->setReplicatedOrigin(SecurityOrigin::createUnique());

  WebLocalFrame* localFrame = FrameTestHelpers::createLocalChild(root);

  // Finally, make sure an embedder triggered load in the local frame swapped
  // back in works.
  registerMockedHttpURLLoad("send_beacon.html");
  registerMockedHttpURLLoad("reload_post.html");  // url param to sendBeacon()
  FrameTestHelpers::loadFrame(localFrame, m_baseURL + "send_beacon.html");

  view->close();
}

TEST_P(ParameterizedWebFrameTest,
       FirstPartyForCookiesFromChildWithRemoteMainFrame) {
  FrameTestHelpers::TestWebViewClient viewClient;
  FrameTestHelpers::TestWebRemoteFrameClient remoteClient;
  WebView* view = WebView::create(&viewClient, WebPageVisibilityStateVisible);
  view->setMainFrame(remoteClient.frame());
  WebRemoteFrame* root = view->mainFrame()->toWebRemoteFrame();
  root->setReplicatedOrigin(SecurityOrigin::create(toKURL(m_notBaseURL)));

  WebLocalFrame* localFrame = FrameTestHelpers::createLocalChild(root);

  registerMockedHttpURLLoad("foo.html");
  FrameTestHelpers::loadFrame(localFrame, m_baseURL + "foo.html");
  EXPECT_EQ(WebURL(SecurityOrigin::urlWithUniqueSecurityOrigin()),
            localFrame->document().firstPartyForCookies());

  SchemeRegistry::registerURLSchemeAsFirstPartyWhenTopLevel("http");
  EXPECT_EQ(WebURL(toKURL(m_notBaseURL)),
            localFrame->document().firstPartyForCookies());
  SchemeRegistry::removeURLSchemeAsFirstPartyWhenTopLevel("http");

  view->close();
}

// See https://crbug.com/525285.
TEST_P(ParameterizedWebFrameTest,
       RemoteToLocalSwapOnMainFrameInitializesCoreFrame) {
  FrameTestHelpers::TestWebViewClient viewClient;
  FrameTestHelpers::TestWebRemoteFrameClient remoteClient;
  WebView* view = WebView::create(&viewClient, WebPageVisibilityStateVisible);
  view->setMainFrame(remoteClient.frame());
  WebRemoteFrame* remoteRoot = view->mainFrame()->toWebRemoteFrame();
  remoteRoot->setReplicatedOrigin(SecurityOrigin::createUnique());

  FrameTestHelpers::createLocalChild(remoteRoot);

  // Do a remote-to-local swap of the top frame.
  FrameTestHelpers::TestWebFrameClient localClient;
  WebLocalFrame* localRoot = WebLocalFrame::createProvisional(
      &localClient, remoteRoot, WebSandboxFlags::None);
  FrameTestHelpers::TestWebWidgetClient webWidgetClient;
  WebFrameWidget::create(&webWidgetClient, localRoot);
  remoteRoot->swap(localRoot);

  // Load a page with a child frame in the new root to make sure this doesn't
  // crash when the child frame invokes setCoreFrame.
  registerMockedHttpURLLoad("single_iframe.html");
  registerMockedHttpURLLoad("visible_iframe.html");
  FrameTestHelpers::loadFrame(localRoot, m_baseURL + "single_iframe.html");

  view->close();
}

// See https://crbug.com/628942.
TEST_P(ParameterizedWebFrameTest, SuspendedPageLoadWithRemoteMainFrame) {
  // Prepare a page with a remote main frame.
  FrameTestHelpers::TestWebViewClient viewClient;
  FrameTestHelpers::TestWebRemoteFrameClient remoteClient;
  WebView* view = WebView::create(&viewClient, WebPageVisibilityStateVisible);
  view->setMainFrame(remoteClient.frame());
  WebRemoteFrame* remoteRoot = view->mainFrame()->toWebRemoteFrame();
  remoteRoot->setReplicatedOrigin(SecurityOrigin::createUnique());

  // Check that ScopedPageSuspender properly triggers deferred loading for
  // the current Page.
  Page* page = remoteRoot->toImplBase()->frame()->page();
  EXPECT_FALSE(page->suspended());
  {
    ScopedPageSuspender suspender;
    EXPECT_TRUE(page->suspended());
  }
  EXPECT_FALSE(page->suspended());

  // Repeat this for a page with a local child frame, and ensure that the
  // child frame's loads are also suspended.
  WebLocalFrame* webLocalChild = FrameTestHelpers::createLocalChild(remoteRoot);
  registerMockedHttpURLLoad("foo.html");
  FrameTestHelpers::loadFrame(webLocalChild, m_baseURL + "foo.html");
  LocalFrame* localChild = toWebLocalFrameImpl(webLocalChild)->frame();
  EXPECT_FALSE(page->suspended());
  EXPECT_FALSE(localChild->document()->fetcher()->defersLoading());
  {
    ScopedPageSuspender suspender;
    EXPECT_TRUE(page->suspended());
    EXPECT_TRUE(localChild->document()->fetcher()->defersLoading());
  }
  EXPECT_FALSE(page->suspended());
  EXPECT_FALSE(localChild->document()->fetcher()->defersLoading());

  view->close();
}

class OverscrollWebViewClient : public FrameTestHelpers::TestWebViewClient {
 public:
  MOCK_METHOD4(didOverscroll,
               void(const WebFloatSize&,
                    const WebFloatSize&,
                    const WebFloatPoint&,
                    const WebFloatSize&));
};

class WebFrameOverscrollTest
    : public WebFrameTest,
      public ::testing::WithParamInterface<blink::WebGestureDevice> {
 protected:
  WebGestureEvent generateEvent(WebInputEvent::Type type,
                                float deltaX = 0.0,
                                float deltaY = 0.0) {
    WebGestureEvent event;
    event.type = type;
    // TODO(wjmaclean): Make sure that touchpad device is only ever used for
    // gesture scrolling event types.
    event.sourceDevice = GetParam();
    event.x = 100;
    event.y = 100;
    if (type == WebInputEvent::GestureScrollUpdate) {
      event.data.scrollUpdate.deltaX = deltaX;
      event.data.scrollUpdate.deltaY = deltaY;
    }
    return event;
  }

  void ScrollBegin(FrameTestHelpers::WebViewHelper* webViewHelper) {
    webViewHelper->webView()->handleInputEvent(
        generateEvent(WebInputEvent::GestureScrollBegin));
  }

  void ScrollUpdate(FrameTestHelpers::WebViewHelper* webViewHelper,
                    float deltaX,
                    float deltaY) {
    webViewHelper->webView()->handleInputEvent(
        generateEvent(WebInputEvent::GestureScrollUpdate, deltaX, deltaY));
  }

  void ScrollEnd(FrameTestHelpers::WebViewHelper* webViewHelper) {
    webViewHelper->webView()->handleInputEvent(
        generateEvent(WebInputEvent::GestureScrollEnd));
  }
};

INSTANTIATE_TEST_CASE_P(All,
                        WebFrameOverscrollTest,
                        ::testing::Values(WebGestureDeviceTouchpad,
                                          WebGestureDeviceTouchscreen));

TEST_P(WebFrameOverscrollTest,
       AccumulatedRootOverscrollAndUnsedDeltaValuesOnOverscroll) {
  OverscrollWebViewClient client;
  registerMockedHttpURLLoad("overscroll/overscroll.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "overscroll/overscroll.html",
                                  true, nullptr, &client, nullptr,
                                  configureAndroid);
  webViewHelper.resize(WebSize(200, 200));

  // Calculation of accumulatedRootOverscroll and unusedDelta on multiple
  // scrollUpdate.
  ScrollBegin(&webViewHelper);
  EXPECT_CALL(client, didOverscroll(WebFloatSize(8, 16), WebFloatSize(8, 16),
                                    WebFloatPoint(100, 100), WebFloatSize()));
  ScrollUpdate(&webViewHelper, -308, -316);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client, didOverscroll(WebFloatSize(0, 13), WebFloatSize(8, 29),
                                    WebFloatPoint(100, 100), WebFloatSize()));
  ScrollUpdate(&webViewHelper, 0, -13);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client, didOverscroll(WebFloatSize(20, 13), WebFloatSize(28, 42),
                                    WebFloatPoint(100, 100), WebFloatSize()));
  ScrollUpdate(&webViewHelper, -20, -13);
  Mock::VerifyAndClearExpectations(&client);

  // Overscroll is not reported.
  EXPECT_CALL(client, didOverscroll(_, _, _, _)).Times(0);
  ScrollUpdate(&webViewHelper, 0, 1);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client, didOverscroll(_, _, _, _)).Times(0);
  ScrollUpdate(&webViewHelper, 1, 0);
  Mock::VerifyAndClearExpectations(&client);

  // Overscroll is reported.
  EXPECT_CALL(client,
              didOverscroll(WebFloatSize(0, -701), WebFloatSize(0, -701),
                            WebFloatPoint(100, 100), WebFloatSize()));
  ScrollUpdate(&webViewHelper, 0, 1000);
  Mock::VerifyAndClearExpectations(&client);

  // Overscroll is not reported.
  EXPECT_CALL(client, didOverscroll(_, _, _, _)).Times(0);
  ScrollEnd(&webViewHelper);
  Mock::VerifyAndClearExpectations(&client);
}

TEST_P(WebFrameOverscrollTest,
       AccumulatedOverscrollAndUnusedDeltaValuesOnDifferentAxesOverscroll) {
  OverscrollWebViewClient client;
  registerMockedHttpURLLoad("overscroll/div-overscroll.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "overscroll/div-overscroll.html",
                                  true, nullptr, &client, nullptr,
                                  configureAndroid);
  webViewHelper.resize(WebSize(200, 200));

  ScrollBegin(&webViewHelper);

  // Scroll the Div to the end.
  EXPECT_CALL(client, didOverscroll(_, _, _, _)).Times(0);
  ScrollUpdate(&webViewHelper, 0, -316);
  Mock::VerifyAndClearExpectations(&client);

  ScrollEnd(&webViewHelper);
  ScrollBegin(&webViewHelper);

  // Now On Scrolling DIV, scroll is bubbled and root layer is over-scrolled.
  EXPECT_CALL(client, didOverscroll(WebFloatSize(0, 100), WebFloatSize(0, 100),
                                    WebFloatPoint(100, 100), WebFloatSize()));
  ScrollUpdate(&webViewHelper, 0, -100);
  ScrollUpdate(&webViewHelper, 0, -100);
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
  registerMockedHttpURLLoad("overscroll/div-overscroll.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "overscroll/div-overscroll.html",
                                  true, nullptr, &client, nullptr,
                                  configureAndroid);
  webViewHelper.resize(WebSize(200, 200));

  ScrollBegin(&webViewHelper);

  // Scroll the Div to the end.
  EXPECT_CALL(client, didOverscroll(_, _, _, _)).Times(0);
  ScrollUpdate(&webViewHelper, 0, -316);
  Mock::VerifyAndClearExpectations(&client);

  ScrollEnd(&webViewHelper);
  ScrollBegin(&webViewHelper);

  // Now On Scrolling DIV, scroll is bubbled and root layer is over-scrolled.
  EXPECT_CALL(client, didOverscroll(WebFloatSize(0, 50), WebFloatSize(0, 50),
                                    WebFloatPoint(100, 100), WebFloatSize()));
  ScrollUpdate(&webViewHelper, 0, -150);
  Mock::VerifyAndClearExpectations(&client);
}

TEST_P(WebFrameOverscrollTest, RootLayerOverscrolledOnInnerIFrameOverScroll) {
  OverscrollWebViewClient client;
  registerMockedHttpURLLoad("overscroll/iframe-overscroll.html");
  registerMockedHttpURLLoad("overscroll/scrollable-iframe.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(
      m_baseURL + "overscroll/iframe-overscroll.html", true, nullptr, &client,
      nullptr, configureAndroid);
  webViewHelper.resize(WebSize(200, 200));

  ScrollBegin(&webViewHelper);
  // Scroll the IFrame to the end.
  EXPECT_CALL(client, didOverscroll(_, _, _, _)).Times(0);

  // This scroll will fully scroll the iframe but will be consumed before being
  // counted as overscroll.
  ScrollUpdate(&webViewHelper, 0, -320);

  // This scroll will again target the iframe but wont bubble further up. Make
  // sure that the unused scroll isn't handled as overscroll.
  ScrollUpdate(&webViewHelper, 0, -50);
  Mock::VerifyAndClearExpectations(&client);

  ScrollEnd(&webViewHelper);
  ScrollBegin(&webViewHelper);

  // Now On Scrolling IFrame, scroll is bubbled and root layer is over-scrolled.
  EXPECT_CALL(client, didOverscroll(WebFloatSize(0, 50), WebFloatSize(0, 50),
                                    WebFloatPoint(100, 100), WebFloatSize()));
  ScrollUpdate(&webViewHelper, 0, -150);
  Mock::VerifyAndClearExpectations(&client);

  ScrollEnd(&webViewHelper);
}

TEST_P(WebFrameOverscrollTest, ScaledPageRootLayerOverscrolled) {
  OverscrollWebViewClient client;
  registerMockedHttpURLLoad("overscroll/overscroll.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl = webViewHelper.initializeAndLoad(
      m_baseURL + "overscroll/overscroll.html", true, nullptr, &client, nullptr,
      configureAndroid);
  webViewHelper.resize(WebSize(200, 200));
  webViewImpl->setPageScaleFactor(3.0);

  // Calculation of accumulatedRootOverscroll and unusedDelta on scaled page.
  // The point is (99, 99) because we clamp in the division by 3 to 33 so when
  // we go back to viewport coordinates it becomes (99, 99).
  ScrollBegin(&webViewHelper);
  EXPECT_CALL(client, didOverscroll(WebFloatSize(0, -30), WebFloatSize(0, -30),
                                    WebFloatPoint(99, 99), WebFloatSize()));
  ScrollUpdate(&webViewHelper, 0, 30);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client, didOverscroll(WebFloatSize(0, -30), WebFloatSize(0, -60),
                                    WebFloatPoint(99, 99), WebFloatSize()));
  ScrollUpdate(&webViewHelper, 0, 30);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client,
              didOverscroll(WebFloatSize(-30, -30), WebFloatSize(-30, -90),
                            WebFloatPoint(99, 99), WebFloatSize()));
  ScrollUpdate(&webViewHelper, 30, 30);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client,
              didOverscroll(WebFloatSize(-30, 0), WebFloatSize(-60, -90),
                            WebFloatPoint(99, 99), WebFloatSize()));
  ScrollUpdate(&webViewHelper, 30, 0);
  Mock::VerifyAndClearExpectations(&client);

  // Overscroll is not reported.
  EXPECT_CALL(client, didOverscroll(_, _, _, _)).Times(0);
  ScrollEnd(&webViewHelper);
  Mock::VerifyAndClearExpectations(&client);
}

TEST_P(WebFrameOverscrollTest, NoOverscrollForSmallvalues) {
  OverscrollWebViewClient client;
  registerMockedHttpURLLoad("overscroll/overscroll.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "overscroll/overscroll.html",
                                  true, nullptr, &client, nullptr,
                                  configureAndroid);
  webViewHelper.resize(WebSize(200, 200));

  ScrollBegin(&webViewHelper);
  EXPECT_CALL(client,
              didOverscroll(WebFloatSize(-10, -10), WebFloatSize(-10, -10),
                            WebFloatPoint(100, 100), WebFloatSize()));
  ScrollUpdate(&webViewHelper, 10, 10);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client,
              didOverscroll(WebFloatSize(0, -0.10), WebFloatSize(-10, -10.10),
                            WebFloatPoint(100, 100), WebFloatSize()));
  ScrollUpdate(&webViewHelper, 0, 0.10);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client, didOverscroll(WebFloatSize(-0.10, 0),
                                    WebFloatSize(-10.10, -10.10),
                                    WebFloatPoint(100, 100), WebFloatSize()));
  ScrollUpdate(&webViewHelper, 0.10, 0);
  Mock::VerifyAndClearExpectations(&client);

  // For residual values overscrollDelta should be reset and didOverscroll
  // shouldn't be called.
  EXPECT_CALL(client, didOverscroll(_, _, _, _)).Times(0);
  ScrollUpdate(&webViewHelper, 0, 0.09);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client, didOverscroll(_, _, _, _)).Times(0);
  ScrollUpdate(&webViewHelper, 0.09, 0.09);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client, didOverscroll(_, _, _, _)).Times(0);
  ScrollUpdate(&webViewHelper, 0.09, 0);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client, didOverscroll(_, _, _, _)).Times(0);
  ScrollUpdate(&webViewHelper, 0, -0.09);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client, didOverscroll(_, _, _, _)).Times(0);
  ScrollUpdate(&webViewHelper, -0.09, -0.09);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client, didOverscroll(_, _, _, _)).Times(0);
  ScrollUpdate(&webViewHelper, -0.09, 0);
  Mock::VerifyAndClearExpectations(&client);

  EXPECT_CALL(client, didOverscroll(_, _, _, _)).Times(0);
  ScrollEnd(&webViewHelper);
  Mock::VerifyAndClearExpectations(&client);
}

TEST_F(WebFrameTest, OrientationFrameDetach) {
  RuntimeEnabledFeatures::setOrientationEventEnabled(true);
  registerMockedHttpURLLoad("orientation-frame-detach.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl = webViewHelper.initializeAndLoad(
      m_baseURL + "orientation-frame-detach.html", true);
  webViewImpl->mainFrameImpl()->sendOrientationChangeEvent();
}

TEST_F(WebFrameTest, MaxFramesDetach) {
  registerMockedHttpURLLoad("max-frames-detach.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl = webViewHelper.initializeAndLoad(
      m_baseURL + "max-frames-detach.html", true);
  webViewImpl->mainFrameImpl()->collectGarbage();
}

TEST_F(WebFrameTest, ImageDocumentLoadFinishTime) {
  // Loading an image resource directly generates an ImageDocument with
  // the document loader feeding image data into the resource of a generated
  // img tag. We expect the load finish time to be the same for the document
  // and the image resource.

  registerMockedHttpURLLoadWithMimeType("white-1x1.png", "image/png");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "white-1x1.png");
  WebViewImpl* webView = webViewHelper.webView();
  Document* document = webView->mainFrameImpl()->frame()->document();

  EXPECT_TRUE(document);
  EXPECT_TRUE(document->isImageDocument());

  ImageDocument* imgDocument = toImageDocument(document);
  ImageResource* resource = imgDocument->cachedImage();

  EXPECT_TRUE(resource);
  EXPECT_NE(0, resource->loadFinishTime());

  DocumentLoader* loader = document->loader();

  EXPECT_TRUE(loader);
  EXPECT_EQ(loader->timing().responseEnd(), resource->loadFinishTime());
}

class CallbackOrderingWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  CallbackOrderingWebFrameClient() : m_callbackCount(0) {}

  void didStartLoading(bool toDifferentDocument) override {
    EXPECT_EQ(0, m_callbackCount++);
    FrameTestHelpers::TestWebFrameClient::didStartLoading(toDifferentDocument);
  }
  void didStartProvisionalLoad(WebLocalFrame*) override {
    EXPECT_EQ(1, m_callbackCount++);
  }
  void didCommitProvisionalLoad(WebLocalFrame*,
                                const WebHistoryItem&,
                                WebHistoryCommitType) override {
    EXPECT_EQ(2, m_callbackCount++);
  }
  void didFinishDocumentLoad(WebLocalFrame*) override {
    EXPECT_EQ(3, m_callbackCount++);
  }
  void didHandleOnloadEvents(WebLocalFrame*) override {
    EXPECT_EQ(4, m_callbackCount++);
  }
  void didFinishLoad(WebLocalFrame*) override {
    EXPECT_EQ(5, m_callbackCount++);
  }
  void didStopLoading() override {
    EXPECT_EQ(6, m_callbackCount++);
    FrameTestHelpers::TestWebFrameClient::didStopLoading();
  }

 private:
  int m_callbackCount;
};

TEST_F(WebFrameTest, CallbackOrdering) {
  registerMockedHttpURLLoad("foo.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  CallbackOrderingWebFrameClient client;
  webViewHelper.initializeAndLoad(m_baseURL + "foo.html", true, &client);
}

class TestWebRemoteFrameClientForVisibility
    : public FrameTestHelpers::TestWebRemoteFrameClient {
 public:
  TestWebRemoteFrameClientForVisibility() : m_visible(true) {}
  void visibilityChanged(bool visible) override { m_visible = visible; }

  bool isVisible() const { return m_visible; }

 private:
  bool m_visible;
};

class WebFrameVisibilityChangeTest : public WebFrameTest {
 public:
  WebFrameVisibilityChangeTest() {
    registerMockedHttpURLLoad("visible_iframe.html");
    registerMockedHttpURLLoad("single_iframe.html");
    m_frame = m_webViewHelper
                  .initializeAndLoad(m_baseURL + "single_iframe.html", true)
                  ->mainFrame();
    m_webRemoteFrame = remoteFrameClient()->frame();
  }

  ~WebFrameVisibilityChangeTest() {}

  void executeScriptOnMainFrame(const WebScriptSource& script) {
    mainFrame()->executeScript(script);
    mainFrame()->view()->updateAllLifecyclePhases();
    runPendingTasks();
  }

  void swapLocalFrameToRemoteFrame() {
    lastChild(mainFrame())->swap(remoteFrame());
    remoteFrame()->setReplicatedOrigin(SecurityOrigin::createUnique());
  }

  WebFrame* mainFrame() { return m_frame; }
  WebRemoteFrameImpl* remoteFrame() { return m_webRemoteFrame; }
  TestWebRemoteFrameClientForVisibility* remoteFrameClient() {
    return &m_remoteFrameClient;
  }

 private:
  TestWebRemoteFrameClientForVisibility m_remoteFrameClient;
  FrameTestHelpers::WebViewHelper m_webViewHelper;
  WebFrame* m_frame;
  Persistent<WebRemoteFrameImpl> m_webRemoteFrame;
};

TEST_F(WebFrameVisibilityChangeTest, RemoteFrameVisibilityChange) {
  swapLocalFrameToRemoteFrame();
  executeScriptOnMainFrame(WebScriptSource(
      "document.querySelector('iframe').style.display = 'none';"));
  EXPECT_FALSE(remoteFrameClient()->isVisible());

  executeScriptOnMainFrame(WebScriptSource(
      "document.querySelector('iframe').style.display = 'block';"));
  EXPECT_TRUE(remoteFrameClient()->isVisible());
}

TEST_F(WebFrameVisibilityChangeTest, RemoteFrameParentVisibilityChange) {
  swapLocalFrameToRemoteFrame();
  executeScriptOnMainFrame(
      WebScriptSource("document.querySelector('iframe').parentElement.style."
                      "display = 'none';"));
  EXPECT_FALSE(remoteFrameClient()->isVisible());
}

static void enableGlobalReuseForUnownedMainFrames(WebSettings* settings) {
  settings->setShouldReuseGlobalForUnownedMainFrame(true);
}

// A main frame with no opener should have a unique security origin. Thus, the
// global should never be reused on the initial navigation.
TEST(WebFrameGlobalReuseTest, MainFrameWithNoOpener) {
  FrameTestHelpers::WebViewHelper helper;
  helper.initialize(true);

  WebLocalFrame* mainFrame = helper.webView()->mainFrameImpl();
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  mainFrame->executeScript(WebScriptSource("hello = 'world';"));
  FrameTestHelpers::loadFrame(mainFrame, "data:text/html,new page");
  v8::Local<v8::Value> result =
      mainFrame->executeScriptAndReturnValue(WebScriptSource("hello"));
  EXPECT_TRUE(result.IsEmpty());
}

// Child frames should never reuse the global on a cross-origin navigation, even
// if the setting is enabled. It's not safe to since the parent could have
// injected script before the initial navigation.
TEST(WebFrameGlobalReuseTest, ChildFrame) {
  FrameTestHelpers::WebViewHelper helper;
  helper.initialize(true, nullptr, nullptr, nullptr,
                    enableGlobalReuseForUnownedMainFrames);

  WebLocalFrame* mainFrame = helper.webView()->mainFrameImpl();
  FrameTestHelpers::loadFrame(mainFrame, "data:text/html,<iframe></iframe>");

  WebLocalFrame* childFrame = mainFrame->firstChild()->toWebLocalFrame();
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  childFrame->executeScript(WebScriptSource("hello = 'world';"));
  FrameTestHelpers::loadFrame(childFrame, "data:text/html,new page");
  v8::Local<v8::Value> result =
      childFrame->executeScriptAndReturnValue(WebScriptSource("hello"));
  EXPECT_TRUE(result.IsEmpty());
}

// A main frame with an opener should never reuse the global on a cross-origin
// navigation, even if the setting is enabled. It's not safe to since the opener
// could have injected script.
TEST(WebFrameGlobalReuseTest, MainFrameWithOpener) {
  FrameTestHelpers::TestWebViewClient openerWebViewClient;
  FrameTestHelpers::WebViewHelper openerHelper;
  openerHelper.initialize(false, nullptr, &openerWebViewClient, nullptr);
  FrameTestHelpers::WebViewHelper helper;
  helper.initializeWithOpener(openerHelper.webView()->mainFrame(), true,
                              nullptr, nullptr, nullptr,
                              enableGlobalReuseForUnownedMainFrames);

  WebLocalFrame* mainFrame = helper.webView()->mainFrameImpl();
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  mainFrame->executeScript(WebScriptSource("hello = 'world';"));
  FrameTestHelpers::loadFrame(mainFrame, "data:text/html,new page");
  v8::Local<v8::Value> result =
      mainFrame->executeScriptAndReturnValue(WebScriptSource("hello"));
  EXPECT_TRUE(result.IsEmpty());
}

// A main frame that is unrelated to any other frame /can/ reuse the global if
// the setting is enabled. In this case, it's impossible for any other frames to
// have touched the global. Only the embedder could have injected script, and
// the embedder enabling this setting is a signal that the injected script needs
// to persist on the first navigation away from the initial empty document.
TEST(WebFrameGlobalReuseTest, ReuseForMainFrameIfEnabled) {
  FrameTestHelpers::WebViewHelper helper;
  helper.initialize(true, nullptr, nullptr, nullptr,
                    enableGlobalReuseForUnownedMainFrames);

  WebLocalFrame* mainFrame = helper.webView()->mainFrameImpl();
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  mainFrame->executeScript(WebScriptSource("hello = 'world';"));
  FrameTestHelpers::loadFrame(mainFrame, "data:text/html,new page");
  v8::Local<v8::Value> result =
      mainFrame->executeScriptAndReturnValue(WebScriptSource("hello"));
  ASSERT_TRUE(result->IsString());
  EXPECT_EQ("world",
            toCoreString(result->ToString(mainFrame->mainWorldScriptContext())
                             .ToLocalChecked()));
}

class SaveImageFromDataURLWebFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  // WebFrameClient methods
  void saveImageFromDataURL(const WebString& dataURL) override {
    m_dataURL = dataURL;
  }

  // Local methods
  const WebString& result() const { return m_dataURL; }
  void reset() { m_dataURL = WebString(); }

 private:
  WebString m_dataURL;
};

TEST_F(WebFrameTest, SaveImageAt) {
  std::string url = m_baseURL + "image-with-data-url.html";
  URLTestHelpers::registerMockedURLLoad(toKURL(url),
                                        "image-with-data-url.html");
  URLTestHelpers::registerMockedURLLoad(toKURL("http://test"), "white-1x1.png");

  FrameTestHelpers::WebViewHelper helper;
  SaveImageFromDataURLWebFrameClient client;
  WebViewImpl* webView = helper.initializeAndLoad(url, true, &client);
  webView->resize(WebSize(400, 400));
  webView->updateAllLifecyclePhases();

  WebLocalFrame* localFrame = webView->mainFrameImpl();

  client.reset();
  localFrame->saveImageAt(WebPoint(1, 1));
  EXPECT_EQ(
      WebString::fromUTF8("data:image/gif;base64"
                          ",R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs="),
      client.result());

  client.reset();
  localFrame->saveImageAt(WebPoint(1, 2));
  EXPECT_EQ(WebString(), client.result());

  webView->setPageScaleFactor(4);
  webView->setVisualViewportOffset(WebFloatPoint(1, 1));

  client.reset();
  localFrame->saveImageAt(WebPoint(3, 3));
  EXPECT_EQ(
      WebString::fromUTF8("data:image/gif;base64"
                          ",R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs="),
      client.result());

  // Explicitly reset to break dependency on locally scoped client.
  helper.reset();
}

TEST_F(WebFrameTest, SaveImageWithImageMap) {
  std::string url = m_baseURL + "image-map.html";
  URLTestHelpers::registerMockedURLLoad(toKURL(url), "image-map.html");

  FrameTestHelpers::WebViewHelper helper;
  SaveImageFromDataURLWebFrameClient client;
  WebViewImpl* webView = helper.initializeAndLoad(url, true, &client);
  webView->resize(WebSize(400, 400));

  WebLocalFrame* localFrame = webView->mainFrameImpl();

  client.reset();
  localFrame->saveImageAt(WebPoint(25, 25));
  EXPECT_EQ(
      WebString::fromUTF8("data:image/gif;base64"
                          ",R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs="),
      client.result());

  client.reset();
  localFrame->saveImageAt(WebPoint(75, 25));
  EXPECT_EQ(
      WebString::fromUTF8("data:image/gif;base64"
                          ",R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs="),
      client.result());

  client.reset();
  localFrame->saveImageAt(WebPoint(125, 25));
  EXPECT_EQ(WebString(), client.result());

  // Explicitly reset to break dependency on locally scoped client.
  helper.reset();
}

TEST_F(WebFrameTest, CopyImageAt) {
  std::string url = m_baseURL + "canvas-copy-image.html";
  URLTestHelpers::registerMockedURLLoad(toKURL(url), "canvas-copy-image.html");

  FrameTestHelpers::WebViewHelper helper;
  WebViewImpl* webView = helper.initializeAndLoad(url, true, 0);
  webView->resize(WebSize(400, 400));

  uint64_t sequence = Platform::current()->clipboard()->sequenceNumber(
      WebClipboard::BufferStandard);

  WebLocalFrame* localFrame = webView->mainFrameImpl();
  localFrame->copyImageAt(WebPoint(50, 50));

  EXPECT_NE(sequence, Platform::current()->clipboard()->sequenceNumber(
                          WebClipboard::BufferStandard));

  WebImage image =
      static_cast<WebMockClipboard*>(Platform::current()->clipboard())
          ->readRawImage(WebClipboard::Buffer());

  SkAutoLockPixels autoLock(image.getSkBitmap());
  EXPECT_EQ(SkColorSetARGB(255, 255, 0, 0), image.getSkBitmap().getColor(0, 0));
};

TEST_F(WebFrameTest, CopyImageAtWithPinchZoom) {
  std::string url = m_baseURL + "canvas-copy-image.html";
  URLTestHelpers::registerMockedURLLoad(toKURL(url), "canvas-copy-image.html");

  FrameTestHelpers::WebViewHelper helper;
  WebViewImpl* webView = helper.initializeAndLoad(url, true, 0);
  webView->resize(WebSize(400, 400));
  webView->updateAllLifecyclePhases();
  webView->setPageScaleFactor(2);
  webView->setVisualViewportOffset(WebFloatPoint(200, 200));

  uint64_t sequence = Platform::current()->clipboard()->sequenceNumber(
      WebClipboard::BufferStandard);

  WebLocalFrame* localFrame = webView->mainFrameImpl();
  localFrame->copyImageAt(WebPoint(0, 0));

  EXPECT_NE(sequence, Platform::current()->clipboard()->sequenceNumber(
                          WebClipboard::BufferStandard));

  WebImage image =
      static_cast<WebMockClipboard*>(Platform::current()->clipboard())
          ->readRawImage(WebClipboard::Buffer());

  SkAutoLockPixels autoLock(image.getSkBitmap());
  EXPECT_EQ(SkColorSetARGB(255, 255, 0, 0), image.getSkBitmap().getColor(0, 0));
};

TEST_F(WebFrameTest, CopyImageWithImageMap) {
  SaveImageFromDataURLWebFrameClient client;

  std::string url = m_baseURL + "image-map.html";
  URLTestHelpers::registerMockedURLLoad(toKURL(url), "image-map.html");

  FrameTestHelpers::WebViewHelper helper;
  WebViewImpl* webView = helper.initializeAndLoad(url, true, &client);
  webView->resize(WebSize(400, 400));

  client.reset();
  WebLocalFrame* localFrame = webView->mainFrameImpl();
  localFrame->saveImageAt(WebPoint(25, 25));
  EXPECT_EQ(
      WebString::fromUTF8("data:image/gif;base64"
                          ",R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs="),
      client.result());

  client.reset();
  localFrame->saveImageAt(WebPoint(75, 25));
  EXPECT_EQ(
      WebString::fromUTF8("data:image/gif;base64"
                          ",R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs="),
      client.result());

  client.reset();
  localFrame->saveImageAt(WebPoint(125, 25));
  EXPECT_EQ(WebString(), client.result());
  // Explicitly reset to break dependency on locally scoped client.
  helper.reset();
}

TEST_F(WebFrameTest, LoadJavascriptURLInNewFrame) {
  FrameTestHelpers::WebViewHelper helper;
  helper.initialize(true);

  std::string redirectURL = m_baseURL + "foo.html";
  URLTestHelpers::registerMockedURLLoad(toKURL(redirectURL), "foo.html");
  WebURLRequest request(toKURL("javascript:location='" + redirectURL + "'"));
  helper.webView()->mainFrameImpl()->loadRequest(request);

  // Normally, the result of the JS url replaces the existing contents on the
  // Document. However, if the JS triggers a navigation, the contents should
  // not be replaced.
  EXPECT_EQ("", toLocalFrame(helper.webView()->page()->mainFrame())
                    ->document()
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

  void willSendRequest(WebLocalFrame*, WebURLRequest& request) override {
    ExpectedRequest* expectedRequest = m_expectedRequests.get(request.url());
    DCHECK(expectedRequest);
    EXPECT_EQ(expectedRequest->priority, request.getPriority());
    expectedRequest->seen = true;
  }

  void addExpectedRequest(const KURL& url, WebURLRequest::Priority priority) {
    m_expectedRequests.add(url, makeUnique<ExpectedRequest>(url, priority));
  }

  void verifyAllRequests() {
    for (const auto& request : m_expectedRequests)
      EXPECT_TRUE(request.value->seen);
  }

 private:
  HashMap<KURL, std::unique_ptr<ExpectedRequest>> m_expectedRequests;
};

TEST_F(WebFrameTest, ChangeResourcePriority) {
  TestResourcePriorityWebFrameClient client;
  registerMockedHttpURLLoad("promote_img_in_viewport_priority.html");
  registerMockedHttpURLLoad("image_slow.pl");
  registerMockedHttpURLLoad("image_slow_out_of_viewport.pl");
  client.addExpectedRequest(
      toKURL("http://internal.test/promote_img_in_viewport_priority.html"),
      WebURLRequest::PriorityVeryHigh);
  client.addExpectedRequest(toKURL("http://internal.test/image_slow.pl"),
                            WebURLRequest::PriorityLow);
  client.addExpectedRequest(
      toKURL("http://internal.test/image_slow_out_of_viewport.pl"),
      WebURLRequest::PriorityLow);

  FrameTestHelpers::WebViewHelper helper;
  helper.initialize(true, &client);
  helper.resize(WebSize(640, 480));
  FrameTestHelpers::loadFrame(
      helper.webView()->mainFrame(),
      m_baseURL + "promote_img_in_viewport_priority.html");

  // Ensure the image in the viewport got promoted after the request was sent.
  Resource* image = toWebLocalFrameImpl(helper.webView()->mainFrame())
                        ->frame()
                        ->document()
                        ->fetcher()
                        ->allResources()
                        .get(toKURL("http://internal.test/image_slow.pl"));
  DCHECK(image);
  EXPECT_EQ(ResourceLoadPriorityHigh, image->resourceRequest().priority());

  client.verifyAllRequests();
}

TEST_F(WebFrameTest, ScriptPriority) {
  TestResourcePriorityWebFrameClient client;
  registerMockedHttpURLLoad("script_priority.html");
  registerMockedHttpURLLoad("priorities/defer.js");
  registerMockedHttpURLLoad("priorities/async.js");
  registerMockedHttpURLLoad("priorities/head.js");
  registerMockedHttpURLLoad("priorities/document-write.js");
  registerMockedHttpURLLoad("priorities/injected.js");
  registerMockedHttpURLLoad("priorities/injected-async.js");
  registerMockedHttpURLLoad("priorities/body.js");
  client.addExpectedRequest(toKURL("http://internal.test/script_priority.html"),
                            WebURLRequest::PriorityVeryHigh);
  client.addExpectedRequest(toKURL("http://internal.test/priorities/defer.js"),
                            WebURLRequest::PriorityLow);
  client.addExpectedRequest(toKURL("http://internal.test/priorities/async.js"),
                            WebURLRequest::PriorityLow);
  client.addExpectedRequest(toKURL("http://internal.test/priorities/head.js"),
                            WebURLRequest::PriorityHigh);
  client.addExpectedRequest(
      toKURL("http://internal.test/priorities/document-write.js"),
      WebURLRequest::PriorityHigh);
  client.addExpectedRequest(
      toKURL("http://internal.test/priorities/injected.js"),
      WebURLRequest::PriorityLow);
  client.addExpectedRequest(
      toKURL("http://internal.test/priorities/injected-async.js"),
      WebURLRequest::PriorityLow);
  client.addExpectedRequest(toKURL("http://internal.test/priorities/body.js"),
                            WebURLRequest::PriorityHigh);

  FrameTestHelpers::WebViewHelper helper;
  helper.initializeAndLoad(m_baseURL + "script_priority.html", true, &client);
  client.verifyAllRequests();
}

class MultipleDataChunkDelegate : public WebURLLoaderTestDelegate {
 public:
  void didReceiveData(WebURLLoaderClient* originalClient,
                      WebURLLoader* loader,
                      const char* data,
                      int dataLength,
                      int encodedDataLength) override {
    EXPECT_GT(dataLength, 16);
    originalClient->didReceiveData(loader, data, 16, 16, 16);
    // This didReceiveData call shouldn't crash due to a failed assertion.
    originalClient->didReceiveData(loader, data + 16, dataLength - 16,
                                   encodedDataLength - 16, dataLength - 16);
  }
};

TEST_F(WebFrameTest, ImageDocumentDecodeError) {
  std::string url = m_baseURL + "not_an_image.ico";
  URLTestHelpers::registerMockedURLLoad(toKURL(url), "not_an_image.ico",
                                        "image/x-icon");
  MultipleDataChunkDelegate delegate;
  Platform::current()->getURLLoaderMockFactory()->setLoaderDelegate(&delegate);
  FrameTestHelpers::WebViewHelper helper;
  helper.initializeAndLoad(url, true);
  Platform::current()->getURLLoaderMockFactory()->setLoaderDelegate(nullptr);

  Document* document =
      toLocalFrame(helper.webView()->page()->mainFrame())->document();
  EXPECT_TRUE(document->isImageDocument());
  EXPECT_EQ(Resource::DecodeError,
            toImageDocument(document)->cachedImage()->getStatus());
}

// Ensure that the root layer -- whose size is ordinarily derived from the
// content size -- maintains a minimum height matching the viewport in cases
// where the content is smaller.
TEST_F(WebFrameTest, RootLayerMinimumHeight) {
  constexpr int viewportWidth = 320;
  constexpr int viewportHeight = 640;
  constexpr int browserControlsHeight = 100;

  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(true, nullptr, nullptr, nullptr,
                           enableViewportSettings);
  WebViewImpl* webView = webViewHelper.webView();
  webView->resizeWithBrowserControls(
      WebSize(viewportWidth, viewportHeight - browserControlsHeight),
      browserControlsHeight, true);

  initializeWithHTML(*webView->mainFrameImpl()->frame(),
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
  webView->updateAllLifecyclePhases();

  Document* document = webView->mainFrameImpl()->frame()->document();
  FrameView* frameView = webView->mainFrameImpl()->frameView();
  PaintLayerCompositor* compositor = frameView->layoutViewItem().compositor();

  EXPECT_EQ(viewportHeight - browserControlsHeight,
            compositor->rootLayer()->boundingBoxForCompositing().height());

  document->view()->setTracksPaintInvalidations(true);

  webView->resizeWithBrowserControls(WebSize(viewportWidth, viewportHeight),
                                     browserControlsHeight, false);

  EXPECT_EQ(viewportHeight,
            compositor->rootLayer()->boundingBoxForCompositing().height());
  EXPECT_EQ(viewportHeight,
            compositor->rootLayer()->graphicsLayerBacking()->size().height());
  EXPECT_EQ(viewportHeight, compositor->rootGraphicsLayer()->size().height());

  const RasterInvalidationTracking* invalidationTracking =
      document->layoutView()
          ->layer()
          ->graphicsLayerBacking()
          ->getRasterInvalidationTracking();
  ASSERT_TRUE(invalidationTracking);
  const auto* rasterInvalidations =
      &invalidationTracking->trackedRasterInvalidations;

  // The newly revealed content at the bottom of the screen should have been
  // invalidated. There are additional invalidations for the position: fixed
  // element.
  EXPECT_GT(rasterInvalidations->size(), 0u);
  EXPECT_TRUE((*rasterInvalidations)[0].rect.contains(
      IntRect(0, viewportHeight - browserControlsHeight, viewportWidth,
              browserControlsHeight)));

  document->view()->setTracksPaintInvalidations(false);
}

// Load a page with display:none set and try to scroll it. It shouldn't crash
// due to lack of layoutObject. crbug.com/653327.
TEST_F(WebFrameTest, ScrollBeforeLayoutDoesntCrash) {
  registerMockedHttpURLLoad("display-none.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "display-none.html");
  WebViewImpl* webView = webViewHelper.webView();
  webViewHelper.resize(WebSize(640, 480));

  Document* document = webView->mainFrameImpl()->frame()->document();
  document->documentElement()->setLayoutObject(nullptr);

  WebGestureEvent beginEvent;
  beginEvent.type = WebInputEvent::GestureScrollEnd;
  beginEvent.sourceDevice = WebGestureDeviceTouchpad;
  WebGestureEvent updateEvent;
  updateEvent.type = WebInputEvent::GestureScrollEnd;
  updateEvent.sourceDevice = WebGestureDeviceTouchpad;
  WebGestureEvent endEvent;
  endEvent.type = WebInputEvent::GestureScrollEnd;
  endEvent.sourceDevice = WebGestureDeviceTouchpad;

  // Try GestureScrollEnd and GestureScrollUpdate first to make sure that not
  // seeing a Begin first doesn't break anything. (This currently happens).
  webViewHelper.webView()->handleInputEvent(endEvent);
  webViewHelper.webView()->handleInputEvent(updateEvent);

  // Try a full Begin/Update/End cycle.
  webViewHelper.webView()->handleInputEvent(beginEvent);
  webViewHelper.webView()->handleInputEvent(updateEvent);
  webViewHelper.webView()->handleInputEvent(endEvent);
}

TEST_F(WebFrameTest, HidingScrollbarsOnScrollableAreaDisablesScrollbars) {
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(true);
  webViewHelper.resize(WebSize(800, 600));
  WebViewImpl* webView = webViewHelper.webView();

  initializeWithHTML(
      *webView->mainFrameImpl()->frame(),
      "<!DOCTYPE html>"
      "<style>"
      "  #scroller { overflow: scroll; width: 1000px; height: 1000px }"
      "  #spacer { width: 2000px; height: 2000px }"
      "</style>"
      "<div id='scroller'>"
      "  <div id='spacer'></div>"
      "</div>");

  Document* document = webView->mainFrameImpl()->frame()->document();
  FrameView* frameView = webView->mainFrameImpl()->frameView();
  Element* scroller = document->getElementById("scroller");
  ScrollableArea* scrollerArea =
      toLayoutBox(scroller->layoutObject())->getScrollableArea();

  ASSERT_TRUE(scrollerArea->horizontalScrollbar());
  ASSERT_TRUE(scrollerArea->verticalScrollbar());
  ASSERT_TRUE(frameView->horizontalScrollbar());
  ASSERT_TRUE(frameView->verticalScrollbar());

  EXPECT_FALSE(frameView->scrollbarsHidden());
  EXPECT_TRUE(
      frameView->horizontalScrollbar()->shouldParticipateInHitTesting());
  EXPECT_TRUE(frameView->verticalScrollbar()->shouldParticipateInHitTesting());

  EXPECT_FALSE(scrollerArea->scrollbarsHidden());
  EXPECT_TRUE(
      scrollerArea->horizontalScrollbar()->shouldParticipateInHitTesting());
  EXPECT_TRUE(
      scrollerArea->verticalScrollbar()->shouldParticipateInHitTesting());

  frameView->setScrollbarsHidden(true);
  EXPECT_FALSE(
      frameView->horizontalScrollbar()->shouldParticipateInHitTesting());
  EXPECT_FALSE(frameView->verticalScrollbar()->shouldParticipateInHitTesting());
  frameView->setScrollbarsHidden(false);
  EXPECT_TRUE(
      frameView->horizontalScrollbar()->shouldParticipateInHitTesting());
  EXPECT_TRUE(frameView->verticalScrollbar()->shouldParticipateInHitTesting());

  scrollerArea->setScrollbarsHidden(true);
  EXPECT_FALSE(
      scrollerArea->horizontalScrollbar()->shouldParticipateInHitTesting());
  EXPECT_FALSE(
      scrollerArea->verticalScrollbar()->shouldParticipateInHitTesting());
  scrollerArea->setScrollbarsHidden(false);
  EXPECT_TRUE(
      scrollerArea->horizontalScrollbar()->shouldParticipateInHitTesting());
  EXPECT_TRUE(
      scrollerArea->verticalScrollbar()->shouldParticipateInHitTesting());
}

// Makes sure that mouse hover over an overlay scrollbar doesn't activate
// elements below unless the scrollbar is faded out.
TEST_F(WebFrameTest, MouseOverLinkAndOverlayScrollbar) {
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initialize(
      true, nullptr, nullptr, nullptr,
      [](WebSettings* settings) { settings->setDeviceSupportsMouse(true); });
  webViewHelper.resize(WebSize(20, 20));
  WebViewImpl* webView = webViewHelper.webView();

  initializeWithHTML(*webView->mainFrameImpl()->frame(),
                     "<!DOCTYPE html>"
                     "<a id='a' href='javascript:void(0);'>"
                     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                     "</a>"
                     "<div style='position: absolute; top: 1000px'>end</div>");

  webView->updateAllLifecyclePhases();

  Document* document = webView->mainFrameImpl()->frame()->document();
  Element* aTag = document->getElementById("a");

  // Ensure hittest has scrollbar and link
  HitTestResult hitTestResult =
      webView->coreHitTestResultAt(WebPoint(18, aTag->offsetTop()));

  EXPECT_TRUE(hitTestResult.URLElement());
  EXPECT_TRUE(hitTestResult.innerElement());
  EXPECT_TRUE(hitTestResult.scrollbar());
  EXPECT_FALSE(hitTestResult.scrollbar()->isCustomScrollbar());

  // Mouse over link. Mouse cursor should be hand.
  PlatformMouseEvent mouseMoveOverLinkEvent(
      IntPoint(aTag->offsetLeft(), aTag->offsetTop()),
      IntPoint(aTag->offsetLeft(), aTag->offsetTop()),
      WebPointerProperties::Button::NoButton, PlatformEvent::MouseMoved, 0,
      PlatformEvent::NoModifiers, WTF::monotonicallyIncreasingTime());
  document->frame()->eventHandler().handleMouseMoveEvent(
      mouseMoveOverLinkEvent);

  EXPECT_EQ(
      Cursor::Type::Hand,
      document->frame()->chromeClient().lastSetCursorForTesting().getType());

  // Mouse over enabled overlay scrollbar. Mouse cursor should be pointer and no
  // active hover element.
  PlatformMouseEvent mouseMoveEvent(
      IntPoint(18, aTag->offsetTop()), IntPoint(18, aTag->offsetTop()),
      WebPointerProperties::Button::NoButton, PlatformEvent::MouseMoved, 0,
      PlatformEvent::NoModifiers, WTF::monotonicallyIncreasingTime());
  document->frame()->eventHandler().handleMouseMoveEvent(mouseMoveEvent);

  EXPECT_EQ(
      Cursor::Type::Pointer,
      document->frame()->chromeClient().lastSetCursorForTesting().getType());

  PlatformMouseEvent mousePressEvent(
      IntPoint(18, aTag->offsetTop()), IntPoint(18, aTag->offsetTop()),
      WebPointerProperties::Button::Left, PlatformEvent::MousePressed, 0,
      PlatformEvent::Modifiers::LeftButtonDown,
      WTF::monotonicallyIncreasingTime());
  document->frame()->eventHandler().handleMousePressEvent(mousePressEvent);

  EXPECT_FALSE(document->activeHoverElement());
  EXPECT_FALSE(document->hoverNode());

  PlatformMouseEvent MouseReleaseEvent(
      IntPoint(18, aTag->offsetTop()), IntPoint(18, aTag->offsetTop()),
      WebPointerProperties::Button::Left, PlatformEvent::MouseReleased, 0,
      PlatformEvent::Modifiers::LeftButtonDown,
      WTF::monotonicallyIncreasingTime());
  document->frame()->eventHandler().handleMouseReleaseEvent(MouseReleaseEvent);

  // Mouse over disabled overlay scrollbar. Mouse cursor should be hand and has
  // active hover element.
  webView->mainFrameImpl()->frameView()->setScrollbarsHidden(true);

  // Ensure hittest only has link
  hitTestResult = webView->coreHitTestResultAt(WebPoint(18, aTag->offsetTop()));

  EXPECT_TRUE(hitTestResult.URLElement());
  EXPECT_TRUE(hitTestResult.innerElement());
  EXPECT_FALSE(hitTestResult.scrollbar());

  document->frame()->eventHandler().handleMouseMoveEvent(mouseMoveEvent);

  EXPECT_EQ(
      Cursor::Type::Hand,
      document->frame()->chromeClient().lastSetCursorForTesting().getType());

  document->frame()->eventHandler().handleMousePressEvent(mousePressEvent);

  EXPECT_TRUE(document->activeHoverElement());
  EXPECT_TRUE(document->hoverNode());

  document->frame()->eventHandler().handleMouseReleaseEvent(MouseReleaseEvent);
}

// Makes sure that mouse hover over an custom scrollbar doesn't change the
// activate elements.
TEST_F(WebFrameTest, MouseOverCustomScrollbar) {
  registerMockedHttpURLLoad("custom-scrollbar-hover.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "custom-scrollbar-hover.html");

  webViewHelper.resize(WebSize(200, 200));

  webView->updateAllLifecyclePhases();

  Document* document = toLocalFrame(webView->page()->mainFrame())->document();

  Element* scrollbarDiv = document->getElementById("scrollbar");
  EXPECT_TRUE(scrollbarDiv);

  // Ensure hittest only has DIV
  HitTestResult hitTestResult = webView->coreHitTestResultAt(WebPoint(1, 1));

  EXPECT_TRUE(hitTestResult.innerElement());
  EXPECT_FALSE(hitTestResult.scrollbar());

  // Mouse over DIV
  PlatformMouseEvent mouseMoveOverDiv(
      IntPoint(1, 1), IntPoint(1, 1), WebPointerProperties::Button::NoButton,
      PlatformEvent::MouseMoved, 0, PlatformEvent::NoModifiers,
      WTF::monotonicallyIncreasingTime());
  document->frame()->eventHandler().handleMouseMoveEvent(mouseMoveOverDiv);

  // DIV :hover
  EXPECT_EQ(document->hoverNode(), scrollbarDiv);

  // Ensure hittest has DIV and scrollbar
  hitTestResult = webView->coreHitTestResultAt(WebPoint(175, 1));

  EXPECT_TRUE(hitTestResult.innerElement());
  EXPECT_TRUE(hitTestResult.scrollbar());
  EXPECT_TRUE(hitTestResult.scrollbar()->isCustomScrollbar());

  // Mouse over scrollbar
  PlatformMouseEvent mouseMoveOverDivAndScrollbar(
      IntPoint(175, 1), IntPoint(175, 1),
      WebPointerProperties::Button::NoButton, PlatformEvent::MouseMoved, 0,
      PlatformEvent::NoModifiers, WTF::monotonicallyIncreasingTime());
  document->frame()->eventHandler().handleMouseMoveEvent(
      mouseMoveOverDivAndScrollbar);

  // Custom not change the DIV :hover
  EXPECT_EQ(document->hoverNode(), scrollbarDiv);
  EXPECT_EQ(hitTestResult.scrollbar()->hoveredPart(), ScrollbarPart::ThumbPart);
}

static void disableCompositing(WebSettings* settings) {
  settings->setAcceleratedCompositingEnabled(false);
  settings->setPreferCompositingToLCDTextEnabled(false);
}

// Make sure overlay scrollbars on non-composited scrollers fade out and set
// the hidden bit as needed.
TEST_F(WebFrameTest, TestNonCompositedOverlayScrollbarsFade) {
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl = webViewHelper.initialize(
      true, nullptr, nullptr, nullptr, &disableCompositing);

  constexpr double kMockOverlayFadeOutDelayMs = 5.0;

  ScrollbarTheme& theme = ScrollbarTheme::theme();
  // This test relies on mock overlay scrollbars.
  ASSERT_TRUE(theme.isMockTheme());
  ASSERT_TRUE(theme.usesOverlayScrollbars());
  ScrollbarThemeOverlayMock& mockOverlayTheme =
      (ScrollbarThemeOverlayMock&)theme;
  mockOverlayTheme.setOverlayScrollbarFadeOutDelay(kMockOverlayFadeOutDelayMs /
                                                   1000.0);

  webViewImpl->resizeWithBrowserControls(WebSize(640, 480), 0, false);

  WebURL baseURL = URLTestHelpers::toKURL("http://example.com/");
  FrameTestHelpers::loadHTMLString(webViewImpl->mainFrame(),
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
                                   baseURL);
  webViewImpl->updateAllLifecyclePhases();

  WebLocalFrameImpl* frame = webViewHelper.webView()->mainFrameImpl();
  Document* document =
      toLocalFrame(webViewImpl->page()->mainFrame())->document();
  Element* container = document->getElementById("container");
  ScrollableArea* scrollableArea =
      toLayoutBox(container->layoutObject())->getScrollableArea();

  EXPECT_FALSE(scrollableArea->scrollbarsHidden());
  testing::runDelayedTasks(kMockOverlayFadeOutDelayMs);
  EXPECT_TRUE(scrollableArea->scrollbarsHidden());

  scrollableArea->setScrollOffset(ScrollOffset(10, 10), ProgrammaticScroll,
                                  ScrollBehaviorInstant);

  EXPECT_FALSE(scrollableArea->scrollbarsHidden());
  testing::runDelayedTasks(kMockOverlayFadeOutDelayMs);
  EXPECT_TRUE(scrollableArea->scrollbarsHidden());

  frame->executeScript(WebScriptSource(
      "document.getElementById('space').style.height = '500px';"));
  frame->view()->updateAllLifecyclePhases();

  EXPECT_FALSE(scrollableArea->scrollbarsHidden());
  testing::runDelayedTasks(kMockOverlayFadeOutDelayMs);
  EXPECT_TRUE(scrollableArea->scrollbarsHidden());

  frame->executeScript(WebScriptSource(
      "document.getElementById('container').style.height = '300px';"));
  frame->view()->updateAllLifecyclePhases();

  EXPECT_FALSE(scrollableArea->scrollbarsHidden());
  testing::runDelayedTasks(kMockOverlayFadeOutDelayMs);
  EXPECT_TRUE(scrollableArea->scrollbarsHidden());

  mockOverlayTheme.setOverlayScrollbarFadeOutDelay(0.0);
}

TEST_F(WebFrameTest, UniqueNames) {
  registerMockedHttpURLLoad("frameset-repeated-name.html");
  registerMockedHttpURLLoad("frameset-dest.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  webViewHelper.initializeAndLoad(m_baseURL + "frameset-repeated-name.html");
  Frame* mainFrame = webViewHelper.webView()->mainFrameImpl()->frame();
  HashSet<AtomicString> names;
  for (Frame* frame = mainFrame->tree().firstChild(); frame;
       frame = frame->tree().traverseNext()) {
    EXPECT_TRUE(names.add(frame->tree().uniqueName()).isNewEntry);
  }
  EXPECT_EQ(10u, names.size());
}

TEST_F(WebFrameTest, NoLoadingCompletionCallbacksInDetach) {
  class LoadingObserverFrameClient
      : public FrameTestHelpers::TestWebFrameClient {
   public:
    void frameDetached(WebLocalFrame*, DetachType) override {
      m_didCallFrameDetached = true;
    }

    void didStopLoading() override {
      // TODO(dcheng): Investigate not calling this as well during frame detach.
      m_didCallDidStopLoading = true;
    }

    void didFailProvisionalLoad(WebLocalFrame*,
                                const WebURLError&,
                                WebHistoryCommitType) override {
      EXPECT_TRUE(false) << "The load should not have failed.";
    }

    void didFinishDocumentLoad(WebLocalFrame*) override {
      // TODO(dcheng): Investigate not calling this as well during frame detach.
      m_didCallDidFinishDocumentLoad = true;
    }

    void didHandleOnloadEvents(WebLocalFrame*) override {
      // TODO(dcheng): Investigate not calling this as well during frame detach.
      m_didCallDidHandleOnloadEvents = true;
    }

    void didFinishLoad(WebLocalFrame*) override {
      EXPECT_TRUE(false) << "didFinishLoad() should not have been called.";
    }

    void dispatchLoad() override {
      EXPECT_TRUE(false) << "dispatchLoad() should not have been called.";
    }

    bool didCallFrameDetached() const { return m_didCallFrameDetached; }
    bool didCallDidStopLoading() const { return m_didCallDidStopLoading; }
    bool didCallDidFinishDocumentLoad() const {
      return m_didCallDidFinishDocumentLoad;
    }
    bool didCallDidHandleOnloadEvents() const {
      return m_didCallDidHandleOnloadEvents;
    }

   private:
    bool m_didCallFrameDetached = false;
    bool m_didCallDidStopLoading = false;
    bool m_didCallDidFinishDocumentLoad = false;
    bool m_didCallDidHandleOnloadEvents = false;
  };

  class MainFrameClient : public FrameTestHelpers::TestWebFrameClient {
   public:
    WebLocalFrame* createChildFrame(WebLocalFrame* parent,
                                    WebTreeScopeType scope,
                                    const WebString& name,
                                    const WebString& uniqueName,
                                    WebSandboxFlags sandboxFlags,
                                    const WebFrameOwnerProperties&) override {
      WebLocalFrame* frame = WebLocalFrame::create(scope, &m_childClient);
      parent->appendChild(frame);
      return frame;
    }

    LoadingObserverFrameClient& childClient() { return m_childClient; }

   private:
    LoadingObserverFrameClient m_childClient;
  };

  registerMockedHttpURLLoad("single_iframe.html");
  URLTestHelpers::registerMockedURLLoad(
      toKURL(m_baseURL + "visible_iframe.html"),
      WebString::fromUTF8("frame_with_frame.html"));
  registerMockedHttpURLLoad("parent_detaching_frame.html");

  FrameTestHelpers::WebViewHelper webViewHelper;
  MainFrameClient mainFrameClient;
  webViewHelper.initializeAndLoad(m_baseURL + "single_iframe.html", true,
                                  &mainFrameClient);

  EXPECT_TRUE(mainFrameClient.childClient().didCallFrameDetached());
  EXPECT_TRUE(mainFrameClient.childClient().didCallDidStopLoading());
  EXPECT_TRUE(mainFrameClient.childClient().didCallDidFinishDocumentLoad());
  EXPECT_TRUE(mainFrameClient.childClient().didCallDidHandleOnloadEvents());

  webViewHelper.reset();
}

}  // namespace blink
