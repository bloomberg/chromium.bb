#include "core/exported/WebViewBase.h"
#include "core/frame/FrameTestHelpers.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/WebLocalFrameBase.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebInputEvent.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebFrame.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebHistoryItem.h"
#include "public/web/WebScriptSource.h"
#include "public/web/WebSettings.h"
#include "public/web/WebView.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ProgrammaticScrollTest : public ::testing::Test {
 public:
  ProgrammaticScrollTest() : base_url_("http://www.test.com/") {}

  void TearDown() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

 protected:
  void RegisterMockedHttpURLLoad(const std::string& file_name) {
    URLTestHelpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), testing::WebTestDataPath(),
        WebString::FromUTF8(file_name));
  }

  std::string base_url_;
  FrameTestHelpers::TestWebFrameClient mock_web_frame_client_;
};

TEST_F(ProgrammaticScrollTest, RestoreScrollPositionAndViewStateWithScale) {
  RegisterMockedHttpURLLoad("long_scroll.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewBase* web_view =
      web_view_helper.InitializeAndLoad(base_url_ + "long_scroll.html");
  web_view->Resize(WebSize(1000, 1000));
  web_view->UpdateAllLifecyclePhases();

  FrameLoader& loader = web_view->MainFrameImpl()->GetFrame()->Loader();
  loader.GetDocumentLoader()->SetLoadType(kFrameLoadTypeBackForward);

  web_view->SetPageScaleFactor(3.0f);
  web_view->MainFrame()->SetScrollOffset(WebSize(0, 500));
  loader.GetDocumentLoader()->GetInitialScrollState().was_scrolled_by_user =
      false;
  loader.GetDocumentLoader()->GetHistoryItem()->SetPageScaleFactor(2);
  loader.GetDocumentLoader()->GetHistoryItem()->SetScrollOffset(
      ScrollOffset(0, 200));

  // Flip back the wasScrolledByUser flag which was set to true by
  // setPageScaleFactor because otherwise
  // FrameLoader::restoreScrollPositionAndViewState does nothing.
  loader.GetDocumentLoader()->GetInitialScrollState().was_scrolled_by_user =
      false;
  loader.RestoreScrollPositionAndViewState();

  // Expect that both scroll and scale were restored.
  EXPECT_EQ(2.0f, web_view->PageScaleFactor());
  EXPECT_EQ(200, web_view->MainFrameImpl()->GetScrollOffset().height);
}

TEST_F(ProgrammaticScrollTest, RestoreScrollPositionAndViewStateWithoutScale) {
  RegisterMockedHttpURLLoad("long_scroll.html");

  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewBase* web_view =
      web_view_helper.InitializeAndLoad(base_url_ + "long_scroll.html");
  web_view->Resize(WebSize(1000, 1000));
  web_view->UpdateAllLifecyclePhases();

  FrameLoader& loader = web_view->MainFrameImpl()->GetFrame()->Loader();
  loader.GetDocumentLoader()->SetLoadType(kFrameLoadTypeBackForward);

  web_view->SetPageScaleFactor(3.0f);
  web_view->MainFrame()->SetScrollOffset(WebSize(0, 500));
  loader.GetDocumentLoader()->GetInitialScrollState().was_scrolled_by_user =
      false;
  loader.GetDocumentLoader()->GetHistoryItem()->SetPageScaleFactor(0);
  loader.GetDocumentLoader()->GetHistoryItem()->SetScrollOffset(
      ScrollOffset(0, 400));

  // FrameLoader::restoreScrollPositionAndViewState flows differently if scale
  // is zero.
  loader.RestoreScrollPositionAndViewState();

  // Expect that only the scroll position was restored.
  EXPECT_EQ(3.0f, web_view->PageScaleFactor());
  EXPECT_EQ(400, web_view->MainFrameImpl()->GetScrollOffset().height);
}

}  // namespace blink
