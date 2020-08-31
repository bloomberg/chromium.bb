// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_mac.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#import "content/app_shim_remote_cocoa/render_widget_host_view_cocoa.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/platform/web_text_input_type.h"

@interface TextInputFlagChangeWaiter : NSObject
@end

@implementation TextInputFlagChangeWaiter {
  RenderWidgetHostViewCocoa* _rwhv_cocoa;
  std::unique_ptr<base::RunLoop> _run_loop;
}

- (instancetype)initWithRenderWidgetHostViewCocoa:
    (RenderWidgetHostViewCocoa*)rwhv_cocoa {
  if ((self = [super init])) {
    _rwhv_cocoa = rwhv_cocoa;
    [_rwhv_cocoa addObserver:self
                  forKeyPath:@"textInputFlags"
                     options:NSKeyValueObservingOptionNew
                     context:nullptr];
    [self reset];
  }
  return self;
}

- (void)dealloc {
  [_rwhv_cocoa removeObserver:self forKeyPath:@"textInputFlags"];
  [super dealloc];
}

- (void)reset {
  _run_loop = std::make_unique<base::RunLoop>();
}

- (void)wait {
  _run_loop->Run();

  [self reset];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id>*)change
                       context:(void*)context {
  _run_loop->Quit();
}
@end

namespace content {

namespace {

class TextCallbackWaiter {
 public:
  TextCallbackWaiter() {}

  void Wait() { run_loop_.Run(); }

  const base::string16& text() const { return text_; }

  void GetText(const base::string16& text) {
    text_ = text;
    run_loop_.Quit();
  }

 private:
  base::string16 text_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TextCallbackWaiter);
};

}  // namespace

class RenderWidgetHostViewMacTest : public ContentBrowserTest {};

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacTest, GetPageTextForSpeech) {
  GURL url(
      "data:text/html,<span>Hello</span>"
      "<span style='display:none'>Goodbye</span>"
      "<span>World</span>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderWidgetHostView* rwhv =
      shell()->web_contents()->GetMainFrame()->GetView();
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rwhv);

  TextCallbackWaiter waiter;
  rwhv_mac->GetPageTextForSpeech(
      base::BindOnce(&TextCallbackWaiter::GetText, base::Unretained(&waiter)));
  waiter.Wait();

  EXPECT_EQ(base::ASCIIToUTF16("Hello\nWorld"), waiter.text());
}

// Test that -firstRectForCharacterRange:actualRange: works when the range
// isn't in the active selection, which requres a sync IPC to the renderer.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacTest,
                       GetFirstRectForCharacterRangeUncached) {
  GURL url("data:text/html,Hello");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  auto* root = web_contents_impl->GetFrameTree()->root();
  web_contents_impl->GetFrameTree()->SetFocusedFrame(
      root, root->current_frame_host()->GetSiteInstance());

  RenderWidgetHostView* rwhv =
      shell()->web_contents()->GetMainFrame()->GetView();
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rwhv);

  NSRect rect = [rwhv_mac->GetInProcessNSView()
      firstRectForCharacterRange:NSMakeRange(2, 1)
                     actualRange:nullptr];
  EXPECT_GT(NSMinX(rect), 0);
  EXPECT_GT(NSWidth(rect), 0);
  EXPECT_GT(NSHeight(rect), 0);
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacTest, UpdateInputFlags) {
  class InputMethodObserver {};

  GURL url("data:text/html,<!doctype html><textarea id=ta></textarea>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderWidgetHostView* rwhv =
      shell()->web_contents()->GetMainFrame()->GetView();
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rwhv);
  RenderWidgetHostViewCocoa* rwhv_cocoa = rwhv_mac->GetInProcessNSView();
  base::scoped_nsobject<TextInputFlagChangeWaiter> flag_change_waiter(
      [[TextInputFlagChangeWaiter alloc]
          initWithRenderWidgetHostViewCocoa:rwhv_cocoa]);

  EXPECT_TRUE(ExecJs(shell(), "ta.focus();"));
  [flag_change_waiter wait];
  EXPECT_FALSE(rwhv_cocoa.textInputFlags &
               blink::kWebTextInputFlagAutocorrectOff);

  EXPECT_TRUE(ExecJs(
      shell(),
      "ta.setAttribute('autocorrect', 'off'); console.log(ta.outerHTML);"));
  [flag_change_waiter wait];
  EXPECT_TRUE(rwhv_cocoa.textInputFlags &
              blink::kWebTextInputFlagAutocorrectOff);
}

}  // namespace content
