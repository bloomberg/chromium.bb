// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/check.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/data_url.h"
#include "net/base/escape.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

namespace content {

namespace {

class AccessibilityActionBrowserTest : public ContentBrowserTest {
 public:
  AccessibilityActionBrowserTest() {}
  ~AccessibilityActionBrowserTest() override {}

 protected:
  BrowserAccessibility* FindNode(ax::mojom::Role role,
                                 const std::string& name_or_value) {
    BrowserAccessibility* root = GetManager()->GetRoot();
    CHECK(root);
    return FindNodeInSubtree(*root, role, name_or_value);
  }

  BrowserAccessibilityManager* GetManager() {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    return web_contents->GetRootBrowserAccessibilityManager();
  }

  void GetBitmapFromImageDataURL(BrowserAccessibility* target,
                                 SkBitmap* bitmap) {
    std::string image_data_url =
        target->GetStringAttribute(ax::mojom::StringAttribute::kImageDataUrl);
    std::string mimetype;
    std::string charset;
    std::string png_data;
    ASSERT_TRUE(net::DataURL::Parse(GURL(image_data_url), &mimetype, &charset,
                                    &png_data));
    ASSERT_EQ("image/png", mimetype);
    ASSERT_TRUE(gfx::PNGCodec::Decode(
        reinterpret_cast<const unsigned char*>(png_data.data()),
        png_data.size(), bitmap));
  }

  void LoadInitialAccessibilityTreeFromHtml(const std::string& html) {
    AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                           ui::kAXModeComplete,
                                           ax::mojom::Event::kLoadComplete);
    GURL html_data_url("data:text/html," +
                       net::EscapeQueryParamValue(html, false));
    EXPECT_TRUE(NavigateToURL(shell(), html_data_url));
    waiter.WaitForNotification();
  }

  void ScrollNodeIntoView(BrowserAccessibility* node,
                          ax::mojom::ScrollAlignment horizontal_alignment,
                          ax::mojom::ScrollAlignment vertical_alignment,
                          bool wait_for_event = true) {
    gfx::Rect bounds = node->GetUnclippedScreenBoundsRect();

    AccessibilityNotificationWaiter waiter(
        shell()->web_contents(), ui::kAXModeComplete,
        horizontal_alignment == ax::mojom::ScrollAlignment::kNone
            ? ui::AXEventGenerator::Event::SCROLL_VERTICAL_POSITION_CHANGED
            : ui::AXEventGenerator::Event::SCROLL_HORIZONTAL_POSITION_CHANGED);
    ui::AXActionData action_data;
    action_data.target_node_id = node->GetData().id;
    action_data.action = ax::mojom::Action::kScrollToMakeVisible;
    action_data.target_rect = gfx::Rect(0, 0, bounds.width(), bounds.height());
    action_data.horizontal_scroll_alignment = horizontal_alignment;
    action_data.vertical_scroll_alignment = vertical_alignment;
    node->AccessibilityPerformAction(action_data);

    if (wait_for_event)
      waiter.WaitForNotification();
  }

  void ScrollToTop(bool will_scroll_horizontally = false) {
    AccessibilityNotificationWaiter waiter(
        shell()->web_contents(), ui::kAXModeComplete,
        will_scroll_horizontally
            ? ui::AXEventGenerator::Event::SCROLL_HORIZONTAL_POSITION_CHANGED
            : ui::AXEventGenerator::Event::SCROLL_VERTICAL_POSITION_CHANGED);
    BrowserAccessibility* document = GetManager()->GetRoot();
    ui::AXActionData action_data;
    action_data.target_node_id = document->GetData().id;
    action_data.action = ax::mojom::Action::kSetScrollOffset;
    action_data.target_point = gfx::Point(0, 0);
    document->AccessibilityPerformAction(action_data);
    waiter.WaitForNotification();
  }

 private:
  BrowserAccessibility* FindNodeInSubtree(BrowserAccessibility& node,
                                          ax::mojom::Role role,
                                          const std::string& name_or_value) {
    const std::string& name =
        node.GetStringAttribute(ax::mojom::StringAttribute::kName);
    // Note that in the case of a text field,
    // "BrowserAccessibility::GetValueForControl" has the added functionality
    // of computing the value of an ARIA text box from its inner text.
    //
    // <div contenteditable="true" role="textbox">Hello world.</div>
    // Will expose no HTML value attribute, but some screen readers, such as
    // Jaws, VoiceOver and Talkback, require one to be computed.
    const std::string value = base::UTF16ToUTF8(node.GetValueForControl());
    if (node.GetRole() == role &&
        (name == name_or_value || value == name_or_value)) {
      return &node;
    }

    for (unsigned int i = 0; i < node.PlatformChildCount(); ++i) {
      BrowserAccessibility* result =
          FindNodeInSubtree(*node.PlatformGetChild(i), role, name_or_value);
      if (result)
        return result;
    }
    return nullptr;
  }
};

}  // namespace

// Canvas tests rely on the harness producing pixel output in order to read back
// pixels from a canvas element. So we have to override the setup function.
class AccessibilityCanvasActionBrowserTest
    : public AccessibilityActionBrowserTest {
 public:
  void SetUp() override {
    EnablePixelOutput();
    ContentBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(AccessibilityActionBrowserTest, DoDefaultAction) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <div id="button" role="button" tabIndex=0>Click</div>
      <p role="group"></p>
      <script>
        document.getElementById('button').addEventListener('click', () => {
          document.querySelector('p').setAttribute('aria-label', 'success');
        });
      </script>
      )HTML");

  BrowserAccessibility* target = FindNode(ax::mojom::Role::kButton, "Click");
  ASSERT_NE(nullptr, target);

  // Call DoDefaultAction.
  AccessibilityNotificationWaiter waiter2(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kClicked);
  GetManager()->DoDefaultAction(*target);
  waiter2.WaitForNotification();

  // Ensure that the button was clicked - it should change the paragraph
  // text to "success".
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "success");

  // When calling DoDefault on a focusable element, the element should get
  // focused, just like what happens when you click it with the mouse.
  BrowserAccessibility* focus = GetManager()->GetFocus();
  ASSERT_NE(nullptr, focus);
  EXPECT_EQ(target->GetId(), focus->GetId());
}

IN_PROC_BROWSER_TEST_F(AccessibilityActionBrowserTest, FocusAction) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <button>One</button>
      <button>Two</button>
      <button>Three</button>
      )HTML");

  BrowserAccessibility* target = FindNode(ax::mojom::Role::kButton, "One");
  ASSERT_NE(nullptr, target);

  AccessibilityNotificationWaiter waiter2(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kFocus);
  GetManager()->SetFocus(*target);
  waiter2.WaitForNotification();

  BrowserAccessibility* focus = GetManager()->GetFocus();
  ASSERT_NE(nullptr, focus);
  EXPECT_EQ(target->GetId(), focus->GetId());
}

IN_PROC_BROWSER_TEST_F(AccessibilityActionBrowserTest,
                       IncrementDecrementActions) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <input type=range min=2 value=8 max=10 step=2>
      )HTML");

  BrowserAccessibility* target = FindNode(ax::mojom::Role::kSlider, "");
  ASSERT_NE(nullptr, target);
  EXPECT_EQ(8.0, target->GetFloatAttribute(
                     ax::mojom::FloatAttribute::kValueForRange));
  // Numerical ranges (see ui::IsRangeValueSupported) shouldn't have
  // ax::mojom::StringAttribute::kValue set unless they use aria-valuetext.
  EXPECT_FALSE(target->HasStringAttribute(ax::mojom::StringAttribute::kValue));

  // Increment, should result in value changing from 8 to 10.
  {
    AccessibilityNotificationWaiter waiter2(shell()->web_contents(),
                                            ui::kAXModeComplete,
                                            ax::mojom::Event::kValueChanged);
    GetManager()->Increment(*target);
    waiter2.WaitForNotification();
  }
  EXPECT_EQ(10.0, target->GetFloatAttribute(
                      ax::mojom::FloatAttribute::kValueForRange));

  // Increment, should result in value staying the same (max).
  {
    AccessibilityNotificationWaiter waiter2(shell()->web_contents(),
                                            ui::kAXModeComplete,
                                            ax::mojom::Event::kValueChanged);
    GetManager()->Increment(*target);
    waiter2.WaitForNotification();
  }
  EXPECT_EQ(10.0, target->GetFloatAttribute(
                      ax::mojom::FloatAttribute::kValueForRange));

  // Decrement, should result in value changing from 10 to 8.
  {
    AccessibilityNotificationWaiter waiter2(shell()->web_contents(),
                                            ui::kAXModeComplete,
                                            ax::mojom::Event::kValueChanged);
    GetManager()->Decrement(*target);
    waiter2.WaitForNotification();
  }
  EXPECT_EQ(8.0, target->GetFloatAttribute(
                     ax::mojom::FloatAttribute::kValueForRange));
}

IN_PROC_BROWSER_TEST_F(AccessibilityActionBrowserTest, Scroll) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <div role="group" style="width:100; height:50; overflow:scroll"
          aria-label="shakespeare">
        To be or not to be, that is the question.
      </div>
      )HTML");

  BrowserAccessibility* target =
      FindNode(ax::mojom::Role::kGroup, "shakespeare");
  EXPECT_NE(target, nullptr);

  int y_before = target->GetIntAttribute(ax::mojom::IntAttribute::kScrollY);

  AccessibilityNotificationWaiter waiter2(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::SCROLL_VERTICAL_POSITION_CHANGED);

  ui::AXActionData data;
  data.action = ax::mojom::Action::kScrollDown;
  data.target_node_id = target->GetId();

  target->manager()->delegate()->AccessibilityPerformAction(data);
  waiter2.WaitForNotification();

  int y_after = target->GetIntAttribute(ax::mojom::IntAttribute::kScrollY);

  EXPECT_GT(y_after, y_before);
}

IN_PROC_BROWSER_TEST_F(AccessibilityCanvasActionBrowserTest, CanvasGetImage) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <body>
        <canvas aria-label="canvas" id="c" width="4" height="2">
        </canvas>
        <script>
          var c = document.getElementById('c').getContext('2d');
          c.beginPath();
          c.moveTo(0, 0.5);
          c.lineTo(4, 0.5);
          c.strokeStyle = '#ff0000';
          c.stroke();
          c.beginPath();
          c.moveTo(0, 1.5);
          c.lineTo(4, 1.5);
          c.strokeStyle = '#0000ff';
          c.stroke();
        </script>
      </body>
      )HTML");

  BrowserAccessibility* target = FindNode(ax::mojom::Role::kCanvas, "canvas");
  ASSERT_NE(nullptr, target);

  AccessibilityNotificationWaiter waiter2(shell()->web_contents(),
                                          ui::kAXModeComplete,
                                          ax::mojom::Event::kImageFrameUpdated);
  GetManager()->GetImageData(*target, gfx::Size());
  waiter2.WaitForNotification();

  SkBitmap bitmap;
  GetBitmapFromImageDataURL(target, &bitmap);
  ASSERT_EQ(4, bitmap.width());
  ASSERT_EQ(2, bitmap.height());
  EXPECT_EQ(SK_ColorRED, bitmap.getColor(0, 0));
  EXPECT_EQ(SK_ColorRED, bitmap.getColor(1, 0));
  EXPECT_EQ(SK_ColorRED, bitmap.getColor(2, 0));
  EXPECT_EQ(SK_ColorRED, bitmap.getColor(3, 0));
  EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(0, 1));
  EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(1, 1));
  EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(2, 1));
  EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(3, 1));
}

IN_PROC_BROWSER_TEST_F(AccessibilityCanvasActionBrowserTest,
                       CanvasGetImageScale) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <body>
      <canvas aria-label="canvas" id="c" width="40" height="20">
      </canvas>
      <script>
        var c = document.getElementById('c').getContext('2d');
        c.fillStyle = '#00ff00';
        c.fillRect(0, 0, 40, 10);
        c.fillStyle = '#ff00ff';
        c.fillRect(0, 10, 40, 10);
      </script>
    </body>
    )HTML");

  BrowserAccessibility* target = FindNode(ax::mojom::Role::kCanvas, "canvas");
  ASSERT_NE(nullptr, target);

  AccessibilityNotificationWaiter waiter2(shell()->web_contents(),
                                          ui::kAXModeComplete,
                                          ax::mojom::Event::kImageFrameUpdated);
  GetManager()->GetImageData(*target, gfx::Size(4, 4));
  waiter2.WaitForNotification();

  SkBitmap bitmap;
  GetBitmapFromImageDataURL(target, &bitmap);
  ASSERT_EQ(4, bitmap.width());
  ASSERT_EQ(2, bitmap.height());
  EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(0, 0));
  EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(1, 0));
  EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(2, 0));
  EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(3, 0));
  EXPECT_EQ(SK_ColorMAGENTA, bitmap.getColor(0, 1));
  EXPECT_EQ(SK_ColorMAGENTA, bitmap.getColor(1, 1));
  EXPECT_EQ(SK_ColorMAGENTA, bitmap.getColor(2, 1));
  EXPECT_EQ(SK_ColorMAGENTA, bitmap.getColor(3, 1));
}

IN_PROC_BROWSER_TEST_F(AccessibilityActionBrowserTest, ImgElementGetImage) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(
      "data:text/html,"
      "<body>"
      "<img src='data:image/gif;base64,R0lGODdhAgADAKEDAAAA//"
      "8AAAD/AP///ywAAAAAAgADAAACBEwkAAUAOw=='>"
      "</body>");

  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();

  BrowserAccessibility* target = FindNode(ax::mojom::Role::kImage, "");
  ASSERT_NE(nullptr, target);

  AccessibilityNotificationWaiter waiter2(shell()->web_contents(),
                                          ui::kAXModeComplete,
                                          ax::mojom::Event::kImageFrameUpdated);
  GetManager()->GetImageData(*target, gfx::Size());
  waiter2.WaitForNotification();

  SkBitmap bitmap;
  GetBitmapFromImageDataURL(target, &bitmap);
  ASSERT_EQ(2, bitmap.width());
  ASSERT_EQ(3, bitmap.height());
  EXPECT_EQ(SK_ColorRED, bitmap.getColor(0, 0));
  EXPECT_EQ(SK_ColorRED, bitmap.getColor(1, 0));
  EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(0, 1));
  EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(1, 1));
  EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(0, 2));
  EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(1, 2));
}

IN_PROC_BROWSER_TEST_F(AccessibilityActionBrowserTest,
                       DoDefaultActionFocusesContentEditable) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <div><button>Before</button></div>
      <div contenteditable>Editable text</div>
      <div><button>After</button></div>
      )HTML");

  BrowserAccessibility* target =
      FindNode(ax::mojom::Role::kGenericContainer, "Editable text");
  ASSERT_NE(nullptr, target);

  AccessibilityNotificationWaiter waiter2(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kFocus);
  GetManager()->DoDefaultAction(*target);
  waiter2.WaitForNotification();

  BrowserAccessibility* focus = GetManager()->GetFocus();
  EXPECT_EQ(focus->GetId(), target->GetId());
}

IN_PROC_BROWSER_TEST_F(AccessibilityActionBrowserTest, InputSetValue) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <input aria-label="Answer" value="Before">
      )HTML");

  BrowserAccessibility* target =
      FindNode(ax::mojom::Role::kTextField, "Answer");
  ASSERT_NE(nullptr, target);
  EXPECT_EQ(u"Before", target->GetValueForControl());

  AccessibilityNotificationWaiter waiter2(shell()->web_contents(),
                                          ui::kAXModeComplete,
                                          ax::mojom::Event::kValueChanged);
  GetManager()->SetValue(*target, "After");
  waiter2.WaitForNotification();

  EXPECT_EQ(u"After", target->GetValueForControl());
}

IN_PROC_BROWSER_TEST_F(AccessibilityActionBrowserTest, TextareaSetValue) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <textarea aria-label="Answer">Before</textarea>
      )HTML");

  BrowserAccessibility* target =
      FindNode(ax::mojom::Role::kTextField, "Answer");
  ASSERT_NE(nullptr, target);
  EXPECT_EQ(u"Before", target->GetValueForControl());

  AccessibilityNotificationWaiter waiter2(shell()->web_contents(),
                                          ui::kAXModeComplete,
                                          ax::mojom::Event::kValueChanged);
  GetManager()->SetValue(*target, "Line1\nLine2");
  waiter2.WaitForNotification();

  EXPECT_EQ(u"Line1\nLine2", target->GetValueForControl());

  // TODO(dmazzoni): On Android we use an ifdef to disable inline text boxes,
  // which contain all of the line break information.
  //
  // We should do it with accessibility flags instead. http://crbug.com/672205
#if !defined(OS_ANDROID)
  // Check that it really does contain two lines.
  BrowserAccessibility::AXPosition start_position =
      target->CreateTextPositionAt(0);
  BrowserAccessibility::AXPosition end_of_line_1 =
      start_position->CreateNextLineEndPosition(
          ui::AXBoundaryBehavior::kCrossBoundary);
  EXPECT_EQ(5, end_of_line_1->text_offset());
#endif
}

IN_PROC_BROWSER_TEST_F(AccessibilityActionBrowserTest,
                       ContenteditableSetValue) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <div contenteditable aria-label="Answer">Before</div>
      )HTML");

  BrowserAccessibility* target =
      FindNode(ax::mojom::Role::kGenericContainer, "Answer");
  ASSERT_NE(nullptr, target);
  EXPECT_EQ(u"Before", target->GetValueForControl());

  AccessibilityNotificationWaiter waiter2(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::VALUE_IN_TEXT_FIELD_CHANGED);
  GetManager()->SetValue(*target, "Line1\nLine2");
  waiter2.WaitForNotification();

  EXPECT_EQ(u"Line1\nLine2", target->GetValueForControl());

  // TODO(dmazzoni): On Android we use an ifdef to disable inline text boxes,
  // which contain all of the line break information.
  //
  // We should do it with accessibility flags instead. http://crbug.com/672205
#if !defined(OS_ANDROID)
  // Check that it really does contain two lines.
  BrowserAccessibility::AXPosition start_position =
      target->CreateTextPositionAt(0);
  BrowserAccessibility::AXPosition end_of_line_1 =
      start_position->CreateNextLineEndPosition(
          ui::AXBoundaryBehavior::kCrossBoundary);
  EXPECT_EQ(5, end_of_line_1->text_offset());
#endif
}

IN_PROC_BROWSER_TEST_F(AccessibilityActionBrowserTest, ShowContextMenu) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <a href="about:blank">1</a>
      <a href="about:blank">2</a>
      )HTML");

  BrowserAccessibility* target_node = FindNode(ax::mojom::Role::kLink, "2");
  EXPECT_NE(target_node, nullptr);

  // Create a ContextMenuInterceptor to intercept the ShowContextMenu event
  // before RenderFrameHost receives.
  auto context_menu_interceptor = std::make_unique<ContextMenuInterceptor>(
      shell()->web_contents()->GetMainFrame(),
      ContextMenuInterceptor::ShowBehavior::kPreventShow);

  // Raise the ShowContextMenu event from the second link.
  ui::AXActionData context_menu_action;
  context_menu_action.action = ax::mojom::Action::kShowContextMenu;
  target_node->AccessibilityPerformAction(context_menu_action);
  context_menu_interceptor->Wait();

  blink::UntrustworthyContextMenuParams context_menu_params =
      context_menu_interceptor->get_params();
  EXPECT_EQ(u"2", context_menu_params.link_text);
  EXPECT_EQ(ui::MenuSourceType::MENU_SOURCE_KEYBOARD,
            context_menu_params.source_type);
}

IN_PROC_BROWSER_TEST_F(AccessibilityActionBrowserTest,
                       ShowContextMenuOnMultilineElement) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <a style="line-height: 16px" href='www.google.com'>
      This is a <br><br><br><br>multiline link.</a>
      )HTML");

  BrowserAccessibility* target_node =
      FindNode(ax::mojom::Role::kLink, "This is a multiline link.");
  EXPECT_NE(target_node, nullptr);

  // Create a ContextMenuInterceptor to intercept the ShowContextMenu event
  // before RenderFrameHost receives.
  auto context_menu_interceptor = std::make_unique<ContextMenuInterceptor>(
      shell()->web_contents()->GetMainFrame(),
      ContextMenuInterceptor::ShowBehavior::kPreventShow);

  // Raise the ShowContextMenu event from the link.
  ui::AXActionData context_menu_action;
  context_menu_action.action = ax::mojom::Action::kShowContextMenu;
  target_node->AccessibilityPerformAction(context_menu_action);
  context_menu_interceptor->Wait();

  blink::UntrustworthyContextMenuParams context_menu_params =
      context_menu_interceptor->get_params();
  std::string link_text = base::UTF16ToUTF8(context_menu_params.link_text);
  base::ReplaceChars(link_text, "\n", "\\n", &link_text);
  EXPECT_EQ("This is a\\n\\n\\n\\nmultiline link.", link_text);
  EXPECT_EQ(ui::MenuSourceType::MENU_SOURCE_KEYBOARD,
            context_menu_params.source_type);
  // Expect the context menu to open on the same line as the first line of link
  // text. Check that the y coordinate of the context menu is near the line
  // height.
  EXPECT_NEAR(16, context_menu_params.y, 15);
}

IN_PROC_BROWSER_TEST_F(AccessibilityActionBrowserTest,
                       ShowContextMenuOnOffscreenElement) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <a href='www.google.com'
      style='position: absolute; top: -1000px; left: -1000px'>
      Offscreen</a></div>
      )HTML");

  BrowserAccessibility* target_node =
      FindNode(ax::mojom::Role::kLink, "Offscreen");
  EXPECT_NE(target_node, nullptr);

  // Create a ContextMenuInterceptor to intercept the ShowContextMenu event
  // before RenderFrameHost receives.
  auto context_menu_interceptor = std::make_unique<ContextMenuInterceptor>(
      shell()->web_contents()->GetMainFrame(),
      ContextMenuInterceptor::ShowBehavior::kPreventShow);

  // Raise the ShowContextMenu event from the link.
  ui::AXActionData context_menu_action;
  context_menu_action.action = ax::mojom::Action::kShowContextMenu;
  target_node->AccessibilityPerformAction(context_menu_action);
  context_menu_interceptor->Wait();

  blink::UntrustworthyContextMenuParams context_menu_params =
      context_menu_interceptor->get_params();
  EXPECT_EQ(u"Offscreen", context_menu_params.link_text);
  EXPECT_EQ(ui::MenuSourceType::MENU_SOURCE_KEYBOARD,
            context_menu_params.source_type);
  // Expect the context menu point to be 0, 0.
  EXPECT_EQ(0, context_menu_params.x);
  EXPECT_EQ(0, context_menu_params.y);
}

IN_PROC_BROWSER_TEST_F(AccessibilityActionBrowserTest,
                       ShowContextMenuOnObscuredElement) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <a href='www.google.com'>Obscured</a>
      <div style="position: absolute; height: 100px; width: 100px; top: 0px;
                  left: 0px; background-color:red; line-height: 16px"></div>
      )HTML");

  BrowserAccessibility* target_node =
      FindNode(ax::mojom::Role::kLink, "Obscured");
  EXPECT_NE(target_node, nullptr);

  // Create a ContextMenuInterceptor to intercept the ShowContextMenu event
  // before RenderFrameHost receives.
  auto context_menu_interceptor = std::make_unique<ContextMenuInterceptor>(
      shell()->web_contents()->GetMainFrame(),
      ContextMenuInterceptor::ShowBehavior::kPreventShow);

  // Raise the ShowContextMenu event from the link.
  ui::AXActionData context_menu_action;
  context_menu_action.action = ax::mojom::Action::kShowContextMenu;
  target_node->AccessibilityPerformAction(context_menu_action);
  context_menu_interceptor->Wait();

  blink::UntrustworthyContextMenuParams context_menu_params =
      context_menu_interceptor->get_params();
  EXPECT_EQ(u"Obscured", context_menu_params.link_text);
  EXPECT_EQ(ui::MenuSourceType::MENU_SOURCE_KEYBOARD,
            context_menu_params.source_type);
  // Expect the context menu to open on the same line as the link text. Check
  // that the y coordinate of the context menu is near the line height.
  EXPECT_NEAR(16, context_menu_params.y, 15);
}

IN_PROC_BROWSER_TEST_F(AccessibilityActionBrowserTest,
                       AriaGridSelectedChangedEvent) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(
      "data:text/html,"
      "<body>"
      "<script>"
      "function tdclick(ele, event) {"
      "var selected = ele.getAttribute('aria-selected');"
      "ele.setAttribute('aria-selected', selected != 'true');"
      "event.stopPropagation();"
      "}"
      "</script>"
      "<table role='grid' multi aria-multiselectable='true'><tbody>"
      "<tr>"
      "<td role='gridcell' aria-selected='true' tabindex='0' "
      "onclick='tdclick(this, event)'>A</td>"
      "<td role='gridcell' aria-selected='false' tabindex='-1' "
      "onclick='tdclick(this, event)'>B</td>"
      "</tr>"
      "</tbody></table>"
      "</body>");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();

  BrowserAccessibility* cell1 = FindNode(ax::mojom::Role::kCell, "A");
  ASSERT_NE(nullptr, cell1);

  BrowserAccessibility* cell2 = FindNode(ax::mojom::Role::kCell, "B");
  ASSERT_NE(nullptr, cell2);

  // Initial state
  EXPECT_TRUE(cell1->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_FALSE(cell2->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  {
    AccessibilityNotificationWaiter selection_waiter(
        shell()->web_contents(), ui::kAXModeComplete,
        ui::AXEventGenerator::Event::SELECTED_CHANGED);
    GetManager()->DoDefaultAction(*cell2);
    selection_waiter.WaitForNotification();
    EXPECT_EQ(cell2->GetId(), selection_waiter.event_target_id());

    EXPECT_TRUE(cell1->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
    EXPECT_TRUE(cell2->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  }

  {
    AccessibilityNotificationWaiter selection_waiter(
        shell()->web_contents(), ui::kAXModeComplete,
        ui::AXEventGenerator::Event::SELECTED_CHANGED);
    GetManager()->DoDefaultAction(*cell1);
    selection_waiter.WaitForNotification();
    EXPECT_EQ(cell1->GetId(), selection_waiter.event_target_id());

    EXPECT_FALSE(cell1->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
    EXPECT_TRUE(cell2->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  }

  {
    AccessibilityNotificationWaiter selection_waiter(
        shell()->web_contents(), ui::kAXModeComplete,
        ui::AXEventGenerator::Event::SELECTED_CHANGED);
    GetManager()->DoDefaultAction(*cell2);
    selection_waiter.WaitForNotification();
    EXPECT_EQ(cell2->GetId(), selection_waiter.event_target_id());

    EXPECT_FALSE(cell1->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
    EXPECT_FALSE(cell2->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityActionBrowserTest,
                       AriaControlsChangedEvent) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(
      "data:text/html,"
      "<body>"
      "<script>"
      "function setcontrols(ele, event) {"
      "  ele.setAttribute('aria-controls', 'radio1 radio2');"
      "}"
      "</script>"
      "<div id='radiogroup' role='radiogroup' aria-label='group'"
      "     onclick='setcontrols(this, event)'>"
      "<div id='radio1' role='radio'>radio1</div>"
      "<div id='radio2' role='radio'>radio2</div>"
      "</div>"
      "</body>");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();

  BrowserAccessibility* target =
      FindNode(ax::mojom::Role::kRadioGroup, "group");
  ASSERT_NE(nullptr, target);
  BrowserAccessibility* radio1 =
      FindNode(ax::mojom::Role::kRadioButton, "radio1");
  ASSERT_NE(nullptr, radio1);
  BrowserAccessibility* radio2 =
      FindNode(ax::mojom::Role::kRadioButton, "radio2");
  ASSERT_NE(nullptr, radio2);

  AccessibilityNotificationWaiter waiter2(
      shell()->web_contents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::CONTROLS_CHANGED);
  GetManager()->DoDefaultAction(*target);
  waiter2.WaitForNotification();

  auto&& control_list =
      target->GetIntListAttribute(ax::mojom::IntListAttribute::kControlsIds);
  EXPECT_EQ(2u, control_list.size());

  auto find_radio1 =
      std::find(control_list.cbegin(), control_list.cend(), radio1->GetId());
  auto find_radio2 =
      std::find(control_list.cbegin(), control_list.cend(), radio2->GetId());
  EXPECT_NE(find_radio1, control_list.cend());
  EXPECT_NE(find_radio2, control_list.cend());
}

IN_PROC_BROWSER_TEST_F(AccessibilityActionBrowserTest, FocusLostOnDeletedNode) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  GURL url(
      "data:text/html,"
      "<button id='1'>1</button>"
      "<iframe id='iframe' srcdoc=\""
      "<button id='2'>2</button>"
      "\"></iframe>");

  EXPECT_TRUE(NavigateToURL(shell(), url));
  EnableAccessibilityForWebContents(shell()->web_contents());

  auto FocusNodeAndReload = [this, &url](const std::string& node_name,
                                         const std::string& focus_node_script) {
    WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                  node_name);
    BrowserAccessibility* node = FindNode(ax::mojom::Role::kButton, node_name);
    ASSERT_NE(nullptr, node);

    EXPECT_TRUE(ExecJs(shell(), focus_node_script));
    WaitForAccessibilityFocusChange();

    EXPECT_EQ(node->GetId(),
              GetFocusedAccessibilityNodeInfo(shell()->web_contents()).id);

    // Reloading the frames will achieve two things:
    //   1. Force the deletion of the node being tested.
    //   2. Lose focus on the node by focusing a new frame.
    AccessibilityNotificationWaiter load_waiter(
        shell()->web_contents(), ui::kAXModeComplete,
        ax::mojom::Event::kLoadComplete);
    EXPECT_TRUE(NavigateToURL(shell(), url));
    load_waiter.WaitForNotification();
  };

  FocusNodeAndReload("1", "document.getElementById('1').focus();");
  FocusNodeAndReload("2",
                     "var iframe = document.getElementById('iframe');"
                     "var inner_doc = iframe.contentWindow.document;"
                     "inner_doc.getElementById('2').focus();");
}

// Action::kScrollToMakeVisible does not seem reliable on Android and we are
// currently only using it for desktop screen readers.
#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(AccessibilityActionBrowserTest, ScrollIntoView) {
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <!DOCTYPE html>
      <html>
      <body>
        <div style='height: 5000px; width: 5000px;'></div>
        <div role='group' aria-label='target' style='position: relative;
             left: 2000px; width: 100px;'>One</div>
        <div style='height: 5000px;'></div>
      </body>
      </html>"
      )HTML");

  BrowserAccessibility* root = GetManager()->GetRoot();
  gfx::Rect doc_bounds = root->GetClippedScreenBoundsRect();

  int one_third_doc_height = base::ClampRound(doc_bounds.height() / 3.0f);
  int one_third_doc_width = base::ClampRound(doc_bounds.width() / 3.0f);

  gfx::Rect doc_top_third = doc_bounds;
  doc_top_third.set_height(one_third_doc_height);
  gfx::Rect doc_left_third = doc_bounds;
  doc_left_third.set_width(one_third_doc_width);

  gfx::Rect doc_bottom_third = doc_top_third;
  doc_bottom_third.set_y(doc_bounds.bottom() - one_third_doc_height);
  gfx::Rect doc_right_third = doc_left_third;
  doc_right_third.set_x(doc_bounds.right() - one_third_doc_width);

  BrowserAccessibility* target_node =
      FindNode(ax::mojom::Role::kGroup, "target");
  EXPECT_NE(target_node, nullptr);

  ScrollNodeIntoView(target_node,
                     ax::mojom::ScrollAlignment::kScrollAlignmentClosestEdge,
                     ax::mojom::ScrollAlignment::kScrollAlignmentClosestEdge);
  gfx::Rect bounds = target_node->GetUnclippedScreenBoundsRect();
  {
    ::testing::Message message;
    message << "Expected" << bounds.ToString() << " to be within "
            << doc_bottom_third.ToString() << " and "
            << doc_right_third.ToString();
    SCOPED_TRACE(message);
    EXPECT_TRUE(doc_bottom_third.Contains(bounds));
    EXPECT_TRUE(doc_right_third.Contains(bounds));
  }

  // Scrolling again should have no effect, since the node is already onscreen.
  ScrollNodeIntoView(target_node,
                     ax::mojom::ScrollAlignment::kScrollAlignmentCenter,
                     ax::mojom::ScrollAlignment::kScrollAlignmentCenter,
                     false /* wait_for_event */);
  gfx::Rect new_bounds = target_node->GetUnclippedScreenBoundsRect();
  EXPECT_EQ(bounds, new_bounds);

  ScrollToTop();
  bounds = target_node->GetUnclippedScreenBoundsRect();
  EXPECT_FALSE(doc_bounds.Contains(bounds));

  ScrollNodeIntoView(target_node,
                     ax::mojom::ScrollAlignment::kScrollAlignmentLeft,
                     ax::mojom::ScrollAlignment::kScrollAlignmentTop);
  bounds = target_node->GetUnclippedScreenBoundsRect();
  {
    ::testing::Message message;
    message << "Expected" << bounds.ToString() << " to be within "
            << doc_top_third.ToString() << " and " << doc_left_third.ToString();
    EXPECT_TRUE(doc_bounds.Contains(bounds));
    EXPECT_TRUE(doc_top_third.Contains(bounds));
    EXPECT_TRUE(doc_left_third.Contains(bounds));
  }

  ScrollToTop();
  bounds = target_node->GetUnclippedScreenBoundsRect();
  EXPECT_FALSE(doc_bounds.Contains(bounds));

  ScrollNodeIntoView(target_node,
                     ax::mojom::ScrollAlignment::kScrollAlignmentRight,
                     ax::mojom::ScrollAlignment::kScrollAlignmentBottom);
  bounds = target_node->GetUnclippedScreenBoundsRect();
  {
    ::testing::Message message;
    message << "Expected" << bounds.ToString() << " to be within "
            << doc_bottom_third.ToString() << " and "
            << doc_right_third.ToString();
    EXPECT_TRUE(doc_bounds.Contains(bounds));
    EXPECT_TRUE(doc_bottom_third.Contains(bounds));
    EXPECT_TRUE(doc_right_third.Contains(bounds));
  }

  ScrollToTop();
  bounds = target_node->GetUnclippedScreenBoundsRect();
  EXPECT_FALSE(doc_bounds.Contains(bounds));

  // Now we test scrolling in only dimension at a time. When doing this, the
  // scroll position in the other dimension should not be touched.
  ScrollNodeIntoView(target_node, ax::mojom::ScrollAlignment::kNone,
                     ax::mojom::ScrollAlignment::kScrollAlignmentBottom);
  bounds = target_node->GetUnclippedScreenBoundsRect();
  EXPECT_GE(bounds.y(), doc_bottom_third.y());
  EXPECT_LE(bounds.y(), doc_bottom_third.bottom());
  EXPECT_FALSE(doc_bounds.Contains(bounds));

  ScrollToTop();
  bounds = target_node->GetUnclippedScreenBoundsRect();
  EXPECT_FALSE(doc_bounds.Contains(bounds));

  ScrollNodeIntoView(target_node,
                     ax::mojom::ScrollAlignment::kScrollAlignmentRight,
                     ax::mojom::ScrollAlignment::kNone);
  bounds = target_node->GetUnclippedScreenBoundsRect();
  EXPECT_GE(bounds.x(), doc_right_third.x());
  EXPECT_LE(bounds.x(), doc_right_third.right());
  EXPECT_FALSE(doc_bounds.Contains(bounds));
  ScrollToTop(true /* horizontally scrolls */);
  bounds = target_node->GetUnclippedScreenBoundsRect();
  EXPECT_FALSE(doc_bounds.Contains(bounds));

  // When scrolling to the center, the target node should more or less be
  // centered.
  ScrollNodeIntoView(target_node,
                     ax::mojom::ScrollAlignment::kScrollAlignmentCenter,
                     ax::mojom::ScrollAlignment::kScrollAlignmentCenter);
  bounds = target_node->GetUnclippedScreenBoundsRect();
  {
    ::testing::Message message;
    message << "Expected" << bounds.ToString() << " to not be within "
            << doc_top_third.ToString() << ", " << doc_bottom_third.ToString()
            << ", " << doc_left_third.ToString() << ", and "
            << doc_right_third.ToString();
    EXPECT_TRUE(doc_bounds.Contains(bounds));
    EXPECT_FALSE(doc_top_third.Contains(bounds));
    EXPECT_FALSE(doc_bottom_third.Contains(bounds));
    EXPECT_FALSE(doc_right_third.Contains(bounds));
    EXPECT_FALSE(doc_left_third.Contains(bounds));
  }
}
#endif  // !defined(OS_ANDROID)

IN_PROC_BROWSER_TEST_F(AccessibilityActionBrowserTest, ClickSVG) {
  // Create an svg link element that has the shape of a small, red square.
  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <svg aria-label="svg" width="10" height="10" viewBox="0 0 10 10"
        onclick="(function() {
          let para = document.createElement('p');
          para.innerHTML = 'SVG link was clicked!';
          document.body.appendChild(para);})()">
        <a xlink:href="#">
          <path fill-opacity="1" fill="#ff0000"
            d="M 0 0 L 10 0 L 10 10 L 0 10 Z"></path>
        </a>
      </svg>
      )HTML");

  AccessibilityNotificationWaiter click_waiter(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kClicked);
  BrowserAccessibility* target_node =
      FindNode(ax::mojom::Role::kSvgRoot, "svg");
  ASSERT_NE(target_node, nullptr);
  EXPECT_EQ(1U, target_node->PlatformChildCount());
  GetManager()->DoDefaultAction(*target_node);
  click_waiter.WaitForNotification();
#if !defined(OS_ANDROID)
  // This waiter times out on some Android try bots.
  // TODO(akihiroota): Refactor test to be applicable to all platforms.
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "SVG link was clicked!");
#endif  // !defined(OS_ANDROID)
}

// This test ony makes sense on platforms where the popup menu is implemented
// internally as an HTML page in a popup, not where it's a native popup.
#if defined(OS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH) || defined(USE_ATK)
IN_PROC_BROWSER_TEST_F(AccessibilityActionBrowserTest,
                       OpenSelectPopupWithNoAXMenuList) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kDisableAXMenuList);

  LoadInitialAccessibilityTreeFromHtml(R"HTML(
      <head><title>No AXMenuList</title></head>
      <body>
        <select>
          <option selected>One</option>
          <option>Two</option>
          <option>Three</option>
        </select>
      </body>
      )HTML");

  BrowserAccessibility* target = FindNode(ax::mojom::Role::kPopUpButton, "One");
  ASSERT_NE(nullptr, target);

  EXPECT_EQ(0U, target->PlatformChildCount());
  EXPECT_EQ(nullptr, FindNode(ax::mojom::Role::kListBox, ""));

  // Call DoDefaultAction.
  AccessibilityNotificationWaiter waiter2(
      shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kClicked);
  GetManager()->DoDefaultAction(*target);
  waiter2.WaitForNotification();

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Three");

  ASSERT_EQ(1U, target->PlatformChildCount());
  BrowserAccessibility* popup_web_area = target->PlatformGetChild(0);
  EXPECT_EQ(ax::mojom::Role::kRootWebArea, popup_web_area->GetRole());

  BrowserAccessibility* listbox = FindNode(ax::mojom::Role::kListBox, "");
  ASSERT_TRUE(listbox);
  EXPECT_EQ(3U, listbox->PlatformChildCount());
}
#endif  // defined(OS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH) || defined(USE_ATK)

}  // namespace content
