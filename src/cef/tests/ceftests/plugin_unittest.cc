// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_callback.h"
#include "include/cef_pack_resources.h"
#include "include/cef_request_context_handler.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_stream_resource_handler.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/client_app_browser.h"
#include "tests/shared/browser/resource_util.h"

namespace {

// Browser-side app delegate.
class PluginBrowserTest : public client::ClientAppBrowser::Delegate {
 public:
  PluginBrowserTest() {}

  void OnBeforeCommandLineProcessing(
      CefRefPtr<client::ClientAppBrowser> app,
      CefRefPtr<CefCommandLine> command_line) override {
    // Allow all plugin loading by default.
    command_line->AppendSwitchWithValue("plugin-policy", "allow");
  }

 private:
  IMPLEMENT_REFCOUNTING(PluginBrowserTest);
};

const char kPdfTestOrigin[] = "http://tests/";
const char kPdfHtmlUrl[] = "http://tests/pdf.html";
const char kPdfDirectUrl[] = "http://tests/pdf.pdf";

// Delay waiting for iframe tests to load the PDF file.
#if defined(OS_LINUX)
const int64 kPdfLoadDelayMs = 7000;
#else
const int64 kPdfLoadDelayMs = 5000;
#endif

// Browser-side test handler.
class PluginTestHandler : public RoutingTestHandler,
                          public CefContextMenuHandler {
 public:
  enum Mode {
    // No specified context or handler (implicitly uses the global context).
    GLOBAL_DEFAULT,

    // Global context with no handler.
    GLOBAL_NO_HANDLER,

    // Global context with handler that allows plugin load.
    GLOBAL_ALLOW,

    // Global context with handler that blocks plugin load. Then, load the
    // plugin via the context menu.
    GLOBAL_BLOCK_LOAD,

    // Global context with handler that blocks plugin load. Then, hide the
    // plugin via the context menu.
    GLOBAL_BLOCK_HIDE,

    // Global context with handler that disables plugin load. Then, hide the
    // plugin via the context menu.
    GLOBAL_DISABLE_HIDE,

    // Global context with handler that removes the plugin from the
    // `navigator.plugins` list and consequently disables plugin load.
    GLOBAL_NO_LIST,

    // Custom context with no handler.
    CUSTOM_NO_HANDLER,

    // Custom context with handler that allows plugin load.
    CUSTOM_ALLOW,

    // Custom context with handler that blocks plugin load. Then, load the
    // plugin via the context menu.
    CUSTOM_BLOCK_LOAD,

    // Custom context with handler that blocks plugin load. Then, hide the
    // plugin via the context menu.
    CUSTOM_BLOCK_HIDE,

    // Custom context with handler that disables plugin load. Then, hide the
    // plugin via the context menu.
    CUSTOM_DISABLE_HIDE,

    // Custom context with handler that removes the plugin from the
    // `navigator.plugins` list and consequently disables plugin load.
    CUSTOM_NO_LIST,
  };

  class RequestContextHandler : public CefRequestContextHandler {
   public:
    explicit RequestContextHandler(PluginTestHandler* handler)
        : handler_(handler) {}

    bool OnBeforePluginLoad(const CefString& mime_type,
                            const CefString& plugin_url,
                            bool is_main_frame,
                            const CefString& top_origin_url,
                            CefRefPtr<CefWebPluginInfo> plugin_info,
                            PluginPolicy* plugin_policy) override {
      const std::string& mime_type_str = mime_type;
      EXPECT_STREQ("application/pdf", mime_type_str.c_str());
      EXPECT_STREQ("chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/",
                   plugin_info->GetPath().ToString().c_str());

      if (top_origin_url.empty()) {
        if (!handler_->got_on_before_plugin_empty_origin_) {
          // Checking for PDF support in the plugin frame (navigator.plugins
          // listing, pdf load, etc).
          handler_->got_on_before_plugin_empty_origin_.yes();
        } else if (handler_->HasNoList()) {
          // When listing is disabled there should be an additional check in the
          // main frame for the navigator.plugins listing.
          if (!handler_->got_on_before_plugin_empty_origin2_) {
            EXPECT_TRUE(is_main_frame);
            handler_->got_on_before_plugin_empty_origin2_.yes();
          }
        }

        if (handler_->HasNoList()) {
          // Remove the PDF plugin from the `navigator.plugins` list.
          *plugin_policy = PLUGIN_POLICY_DISABLE;
          return true;
        }

        // Ignore requests for building the plugin list.
        return false;
      }

      // Should only get requests for the test origin.
      EXPECT_STREQ(kPdfTestOrigin, top_origin_url.ToString().c_str());

      if (!handler_->got_on_before_plugin_load_pdf_)
        handler_->got_on_before_plugin_load_pdf_.yes();
      else
        NOTREACHED();

      if (handler_->HasAllow()) {
        *plugin_policy = PLUGIN_POLICY_ALLOW;
        return true;
      } else if (handler_->HasBlock()) {
        *plugin_policy = PLUGIN_POLICY_BLOCK;
        return true;
      } else if (handler_->HasDisable()) {
        *plugin_policy = PLUGIN_POLICY_DISABLE;
        return true;
      }

      return false;
    }

    void Detach() { handler_ = nullptr; }

   private:
    PluginTestHandler* handler_;

    IMPLEMENT_REFCOUNTING(RequestContextHandler);
  };

  PluginTestHandler(Mode mode, const std::string& url)
      : mode_(mode), url_(url) {}

  // Loading the PDF directly in the main frame instead of a sub-frame.
  bool HasDirectPluginLoad() const { return url_ == kPdfDirectUrl; }

  // Has a specified RequestContext but not necessarily a custom handler.
  bool HasRequestContext() const { return mode_ != GLOBAL_DEFAULT; }

  // Has a specified RequestContext and custom handler.
  bool HasRequestContextHandler() const {
    return mode_ != GLOBAL_DEFAULT && mode_ != GLOBAL_NO_HANDLER &&
           mode_ != CUSTOM_NO_HANDLER;
  }

  // Using the global request context, either directly or with a custom handler.
  bool HasGlobalRequestContext() const {
    return mode_ >= GLOBAL_DEFAULT && mode_ <= GLOBAL_NO_LIST;
  }

  // Should allow the plugin load via the custom handler.
  bool HasAllow() const {
    return mode_ == GLOBAL_ALLOW || mode_ == CUSTOM_ALLOW;
  }

  // Should block the plugin load via the custom handler.
  bool HasBlock() const {
    return mode_ == GLOBAL_BLOCK_LOAD || mode_ == GLOBAL_BLOCK_HIDE ||
           mode_ == CUSTOM_BLOCK_LOAD || mode_ == CUSTOM_BLOCK_HIDE;
  }

  // Should disable the plugin load via the custom handler.
  bool HasDisable() const {
    return mode_ == GLOBAL_DISABLE_HIDE || mode_ == CUSTOM_DISABLE_HIDE;
  }

  // Should exclude the plugin from the `navigator.plugins` list.
  bool HasNoList() const {
    return mode_ == GLOBAL_NO_LIST || mode_ == CUSTOM_NO_LIST;
  }

  // Should load the plugin via the context menu.
  bool HasContextLoad() const {
    return mode_ == GLOBAL_BLOCK_LOAD || mode_ == CUSTOM_BLOCK_LOAD;
  }

  // Should hide the plugin via the context menu.
  bool HasContextHide() const {
    return mode_ == GLOBAL_BLOCK_HIDE || mode_ == GLOBAL_DISABLE_HIDE ||
           mode_ == CUSTOM_BLOCK_HIDE || mode_ == CUSTOM_DISABLE_HIDE;
  }

  std::string GetPluginNode() const {
    return "document.getElementsByTagName('embed')[0]";
  }

  void WaitForNavigatorPlugins(CefRefPtr<CefFrame> frame) {
    // We can no longer test for navigator.plugins because it returns empty
    // as of https://www.chromestatus.com/feature/5741884322349056.
    if (HasNoList()) {
      // The plugin will not load. End the test.
      EndTest();
    } else if (HasBlock() || HasDisable()) {
      // Wait for the plugin placeholder for the first PDF file to load. The
      // test will continue from OnQuery.
      WaitForPlaceholderLoad(frame);
    } else {
      // Wait for the first PDF file to load.
      WaitForPluginLoad(frame);
    }
  }

  void WaitForPlaceholderLoad(CefRefPtr<CefFrame> frame) {
    // Keep track of the frame that contains the placeholder.
    placeholder_frame_ = frame;

    // Waits for the placeholder to load. This is indicated by the presence
    // of the plugin node.
    const std::string& code =
        "(function waitForNode(i) {"
        "   setTimeout(function() {"
        "     var plugin = " +
        GetPluginNode() +
        ";"
        "     if (plugin) {"
        "        window.testQuery({request:'placeholder_loaded'});"
        "        return;"
        "     }"
        "     if (--i) {"
        "       waitForNode(i);"
        "     } else {"
        "       console.log('Timeout waiting for plugin load');"
        "     }"
        "   }, 250)"
        "})(4);";
    frame->ExecuteJavaScript(code, frame->GetURL(), 0);
  }

  void WaitForPlaceholderHide() {
    // Waits for the placeholder to be hidden (style set to display:none).
    // See PluginPlaceholderBase::HidePlugin.
    const std::string& code =
        "var plugin = " + GetPluginNode() +
        ";"
        "var observer = new MutationObserver(function(mutations) {"
        "  mutations.forEach(function(mutationRecord) {"
        "    window.testQuery({request:'placeholder_hidden'});"
        "    observer.disconnect();"
        "  });"
        "});"
        "observer.observe(plugin, "
        "  { attributes : true, attributeFilter : ['style'] });";
    placeholder_frame_->ExecuteJavaScript(code, placeholder_frame_->GetURL(),
                                          0);
    placeholder_frame_ = nullptr;
  }

  void WaitForPluginLoad(CefRefPtr<CefFrame> frame) {
    if (got_context_menu_dismissed_) {
      // After context menu display. Destroy the test.
      CefPostDelayedTask(TID_UI,
                         base::BindOnce(&PluginTestHandler::DestroyTest, this),
                         kPdfLoadDelayMs);
    } else {
      // Trigger the context menu.
      CefPostDelayedTask(TID_UI,
                         base::BindOnce(&PluginTestHandler::TriggerContextMenu,
                                        this, frame->GetBrowser()),
                         kPdfLoadDelayMs);
    }
  }

  void EndTest() {
    CefPostTask(TID_UI, base::BindOnce(&PluginTestHandler::DestroyTest, this));
  }

  CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override {
    return this;
  }

  void RunTest() override {
    CefRefPtr<CefRequestContext> request_context;

    if (HasRequestContext()) {
      if (HasRequestContextHandler())
        context_handler_ = new RequestContextHandler(this);

      if (HasGlobalRequestContext()) {
        // Use the global request context.
        request_context = CefRequestContext::CreateContext(
            CefRequestContext::GetGlobalContext(), context_handler_.get());
      } else {
        // Create the request context that will use an in-memory cache.
        CefRequestContextSettings settings;
        request_context =
            CefRequestContext::CreateContext(settings, context_handler_.get());
      }
    }

    // Create the browser.
    CreateBrowser(url_, request_context);

    // Time out the test after a reasonable period of time.
    SetTestTimeout(5000 + kPdfLoadDelayMs);
  }

  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    const std::string& url = request->GetURL();
    if (url == kPdfHtmlUrl) {
      CefRefPtr<CefStreamReader> stream =
          client::GetBinaryResourceReader("pdf.html");
      return new CefStreamResourceHandler("text/html", stream);
    } else if (url == kPdfDirectUrl) {
      CefRefPtr<CefStreamReader> stream =
          client::GetBinaryResourceReader("pdf.pdf");
      return new CefStreamResourceHandler("application/pdf", stream);
    }

    return nullptr;
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    bool is_pdf1 = false;
    const std::string& url = frame->GetURL();
    if (url == kPdfHtmlUrl) {
      if (!got_on_load_end_html_)
        got_on_load_end_html_.yes();
      else
        NOTREACHED();
    } else if (url == kPdfDirectUrl) {
      if (!got_on_load_end_pdf1_) {
        got_on_load_end_pdf1_.yes();
        is_pdf1 = true;
      } else if (!got_on_load_end_pdf2_) {
        got_on_load_end_pdf2_.yes();
      } else {
        NOTREACHED();
      }
    } else {
      NOTREACHED();
    }

    if (HasNoList()) {
      // If the plugin is not listed then the PDF documents will never load.
      EXPECT_STREQ(kPdfHtmlUrl, url.c_str());
      WaitForNavigatorPlugins(frame);
    } else if (is_pdf1) {
      // The first PDF document has loaded.
      WaitForNavigatorPlugins(frame);
    }
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64 query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    if (request == "placeholder_loaded") {
      EXPECT_FALSE(got_placeholder_loaded_);
      EXPECT_FALSE(got_placeholder_hidden_);
      got_placeholder_loaded_.yes();

      // The plugin placeholder has loaded. Trigger the context menu.
      TriggerContextMenu(frame->GetBrowser());
    } else if (request == "placeholder_hidden") {
      EXPECT_TRUE(got_placeholder_loaded_);
      EXPECT_FALSE(got_placeholder_hidden_);
      got_placeholder_hidden_.yes();

      if (HasContextLoad()) {
        // Wait for the PDF plugin to load.
        WaitForPluginLoad(frame);
      } else {
        // The plugin placeholder has been hidden. End the test.
        EndTest();
      }
    } else {
      NOTREACHED();
    }

    return true;
  }

  void TriggerContextMenu(CefRefPtr<CefBrowser> browser) {
    CefMouseEvent mouse_event;

    if (HasDirectPluginLoad()) {
      // Somewhere in the main PDF viewing area (avoid left preview bar).
      mouse_event.x = 400;
      mouse_event.y = 200;
    } else {
      // Somewhere in the first plugin.
      mouse_event.x = 100;
      mouse_event.y = 100;
    }

    // Send right-click mouse down and mouse up to tigger context menu.
    browser->GetHost()->SendMouseClickEvent(mouse_event, MBT_RIGHT, false, 1);
    browser->GetHost()->SendMouseClickEvent(mouse_event, MBT_RIGHT, true, 1);
  }

  bool RunContextMenu(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefContextMenuParams> params,
                      CefRefPtr<CefMenuModel> model,
                      CefRefPtr<CefRunContextMenuCallback> callback) override {
    EXPECT_FALSE(got_run_context_menu_);
    got_run_context_menu_.yes();

    if (HasContextLoad() || HasContextHide()) {
      // Should have 4 elements -- plugin name, separator, run, hide.
      EXPECT_EQ(4, model->GetCount());

      // Wait for the placeholder to be hidden.
      WaitForPlaceholderHide();

      int command_id;
      if (HasContextLoad()) {
        // Execute the run command.
        command_id = model->GetCommandIdAt(2);
      } else {
        // Execute the hide command.
        command_id = model->GetCommandIdAt(3);
      }

      EXPECT_GE(command_id, MENU_ID_CUSTOM_FIRST);
      EXPECT_LE(command_id, MENU_ID_CUSTOM_LAST);

      callback->Continue(command_id, static_cast<EventFlags>(0));
    } else {
      // Do nothing with the context menu.
      callback->Cancel();
    }
    return true;
  }

  void OnContextMenuDismissed(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame) override {
    EXPECT_FALSE(got_context_menu_dismissed_);
    got_context_menu_dismissed_.yes();

    if (HasContextHide()) {
      // Nothing to do here. The test will continue from OnQuery.
      // TODO: Once block-then-load is working this should also be the case for
      // HasContextLoad().
      return;
    }

    EndTest();
  }

  void DestroyTest() override {
    if (context_handler_.get()) {
      context_handler_->Detach();
      context_handler_ = nullptr;
    }

    if (HasContextLoad()) {
      EXPECT_TRUE(got_placeholder_loaded_);
      // TODO: Once block-then-load is working this value should be true.
      EXPECT_FALSE(got_placeholder_hidden_);
    } else if (HasContextHide()) {
      EXPECT_TRUE(got_placeholder_loaded_);
      EXPECT_TRUE(got_placeholder_hidden_);
    } else {
      EXPECT_FALSE(got_placeholder_loaded_);
      EXPECT_FALSE(got_placeholder_hidden_);
    }
    EXPECT_FALSE(placeholder_frame_);

    if (HasRequestContextHandler())
      EXPECT_TRUE(got_on_before_plugin_empty_origin_);
    else
      EXPECT_FALSE(got_on_before_plugin_empty_origin_);

    if (HasNoList()) {
      EXPECT_TRUE(got_on_before_plugin_empty_origin2_);
      EXPECT_FALSE(got_run_context_menu_);
      EXPECT_FALSE(got_context_menu_dismissed_);
    } else {
      EXPECT_FALSE(got_on_before_plugin_empty_origin2_);
      EXPECT_TRUE(got_run_context_menu_);
      EXPECT_TRUE(got_context_menu_dismissed_);
    }

    if (url_ == kPdfHtmlUrl) {
      // The HTML file will load the PDF twice in iframes.
      EXPECT_TRUE(got_on_load_end_html_);

      if (!HasNoList()) {
        EXPECT_TRUE(got_on_load_end_pdf1_);
        EXPECT_TRUE(got_on_load_end_pdf2_);

        if (HasRequestContextHandler()) {
          EXPECT_TRUE(got_on_before_plugin_load_pdf_);
        }
      }
    } else if (url_ == kPdfDirectUrl) {
      // Load the PDF file directly.
      EXPECT_FALSE(got_on_load_end_html_);
      EXPECT_TRUE(got_on_load_end_pdf1_);
      EXPECT_FALSE(got_on_load_end_pdf2_);

      if (HasRequestContextHandler()) {
        EXPECT_TRUE(got_on_before_plugin_load_pdf_);
      }
    } else {
      NOTREACHED();
    }

    if (!HasRequestContextHandler() || HasNoList()) {
      EXPECT_FALSE(got_on_before_plugin_load_pdf_);
    }

    TestHandler::DestroyTest();
  }

  const Mode mode_;
  const std::string url_;

  CefRefPtr<CefFrame> placeholder_frame_;

  TrackCallback got_on_before_plugin_empty_origin_;
  TrackCallback got_on_before_plugin_empty_origin2_;
  TrackCallback got_on_before_plugin_load_pdf_;
  TrackCallback got_on_load_end_html_;
  TrackCallback got_on_load_end_pdf1_;
  TrackCallback got_on_load_end_pdf2_;
  TrackCallback got_placeholder_loaded_;
  TrackCallback got_placeholder_hidden_;
  TrackCallback got_run_context_menu_;
  TrackCallback got_context_menu_dismissed_;

  CefRefPtr<RequestContextHandler> context_handler_;

  IMPLEMENT_REFCOUNTING(PluginTestHandler);
};

}  // namespace

#define RUN_TEST(name, type, url)                            \
  TEST(PluginTest, name) {                                   \
    CefRefPtr<PluginTestHandler> handler =                   \
        new PluginTestHandler(PluginTestHandler::type, url); \
    handler->ExecuteTest();                                  \
    ReleaseAndWaitForDestructor(handler);                    \
  }

RUN_TEST(GlobalDefaultPdfDirect, GLOBAL_DEFAULT, kPdfDirectUrl)
RUN_TEST(GlobalDefaultPdfHtml, GLOBAL_DEFAULT, kPdfHtmlUrl)

RUN_TEST(GlobalNoHandlerPdfDirect, GLOBAL_NO_HANDLER, kPdfDirectUrl)
RUN_TEST(GlobalNoHandlerPdfHtml, GLOBAL_NO_HANDLER, kPdfHtmlUrl)
RUN_TEST(GlobalAllowPdfDirect, GLOBAL_ALLOW, kPdfDirectUrl)
RUN_TEST(GlobalAllowPdfHtml, GLOBAL_ALLOW, kPdfHtmlUrl)
RUN_TEST(GlobalBlockThenLoadPdfDirect, GLOBAL_BLOCK_LOAD, kPdfDirectUrl)
RUN_TEST(GlobalBlockThenLoadPdfHtml, GLOBAL_BLOCK_LOAD, kPdfHtmlUrl)
RUN_TEST(GlobalBlockThenHidePdfDirect, GLOBAL_BLOCK_HIDE, kPdfDirectUrl)
RUN_TEST(GlobalBlockThenHidePdfHtml, GLOBAL_BLOCK_HIDE, kPdfHtmlUrl)
RUN_TEST(GlobalDisableThenHidePdfDirect, GLOBAL_DISABLE_HIDE, kPdfDirectUrl)
RUN_TEST(GlobalDisableThenHidePdfHtml, GLOBAL_DISABLE_HIDE, kPdfHtmlUrl)
RUN_TEST(GlobalNoListHtml, GLOBAL_NO_LIST, kPdfHtmlUrl)

RUN_TEST(CustomNoHandlerPdfDirect, CUSTOM_NO_HANDLER, kPdfDirectUrl)
RUN_TEST(CustomNoHandlerPdfHtml, CUSTOM_NO_HANDLER, kPdfHtmlUrl)
RUN_TEST(CustomAllowPdfDirect, CUSTOM_ALLOW, kPdfDirectUrl)
RUN_TEST(CustomAllowPdfHtml, CUSTOM_ALLOW, kPdfHtmlUrl)
RUN_TEST(CustomBlockThenLoadPdfDirect, CUSTOM_BLOCK_LOAD, kPdfDirectUrl)
RUN_TEST(CustomBlockThenLoadPdfHtml, CUSTOM_BLOCK_LOAD, kPdfHtmlUrl)
RUN_TEST(CustomBlockThenHidePdfDirect, CUSTOM_BLOCK_HIDE, kPdfDirectUrl)
RUN_TEST(CustomBlockThenHidePdfHtml, CUSTOM_BLOCK_HIDE, kPdfHtmlUrl)
RUN_TEST(CustomDisableThenHidePdfDirect, CUSTOM_DISABLE_HIDE, kPdfDirectUrl)
RUN_TEST(CustomDisableThenHidePdfHtml, CUSTOM_DISABLE_HIDE, kPdfHtmlUrl)
RUN_TEST(CustomNoListHtml, CUSTOM_NO_LIST, kPdfHtmlUrl)

// Entry point for creating plugin browser test objects.
// Called from client_app_delegates.cc.
void CreatePluginBrowserTests(
    client::ClientAppBrowser::DelegateSet& delegates) {
  delegates.insert(new PluginBrowserTest);
}
