// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <unordered_set>
#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"

#if defined(OS_WIN)
#include "base/win/atl.h"
#include "base/win/scoped_com_initializer.h"
#include "ui/base/win/atl_module.h"
#endif

// TODO(dmazzoni): Disabled accessibility tests on Win64. crbug.com/179717
#if defined(OS_WIN) && defined(ARCH_CPU_X86_64)
#define MAYBE_TableSpan DISABLED_TableSpan
#else
#define MAYBE_TableSpan TableSpan
#endif

namespace content {

class CrossPlatformAccessibilityBrowserTest : public ContentBrowserTest {
 public:
  CrossPlatformAccessibilityBrowserTest() {}

  // Tell the renderer to send an accessibility tree, then wait for the
  // notification that it's been received.
  const ui::AXTree& GetAXTree(
      ui::AXMode accessibility_mode = ui::kAXModeComplete) {
    AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                           accessibility_mode,
                                           ax::mojom::Event::kLayoutComplete);
    waiter.WaitForNotification();
    return waiter.GetAXTree();
  }

  // Make sure each node in the tree has a unique id.
  void RecursiveAssertUniqueIds(const ui::AXNode* node,
                                std::unordered_set<int>* ids) {
    ASSERT_TRUE(ids->find(node->id()) == ids->end());
    ids->insert(node->id());
    for (int i = 0; i < node->child_count(); i++)
      RecursiveAssertUniqueIds(node->ChildAtIndex(i), ids);
  }

  // ContentBrowserTest
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 protected:
  void LoadInitialAccessibilityTreeFromUrl(
      const GURL& url,
      ui::AXMode accessibility_mode = ui::kAXModeComplete) {
    AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                           accessibility_mode,
                                           ax::mojom::Event::kLoadComplete);
    NavigateToURL(shell(), url);
    waiter.WaitForNotification();
  }

  void LoadInitialAccessibilityTreeFromHtmlFilePath(
      const std::string& html_file_path,
      ui::AXMode accessibility_mode = ui::kAXModeComplete) {
    if (!embedded_test_server()->Started())
      ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_test_server()->Started());
    LoadInitialAccessibilityTreeFromUrl(
        embedded_test_server()->GetURL(html_file_path), accessibility_mode);
  }

  BrowserAccessibilityManager* GetManager() {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    return web_contents->GetRootBrowserAccessibilityManager();
  }

  std::string GetAttr(const ui::AXNode* node,
                      const ax::mojom::StringAttribute attr);
  int GetIntAttr(const ui::AXNode* node, const ax::mojom::IntAttribute attr);
  bool GetBoolAttr(const ui::AXNode* node, const ax::mojom::BoolAttribute attr);

 private:
#if defined(OS_WIN)
  std::unique_ptr<base::win::ScopedCOMInitializer> com_initializer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(CrossPlatformAccessibilityBrowserTest);
};

void CrossPlatformAccessibilityBrowserTest::SetUpOnMainThread() {
#if defined(OS_WIN)
  ui::win::CreateATLModuleIfNeeded();
  com_initializer_.reset(new base::win::ScopedCOMInitializer());
#endif
}

void CrossPlatformAccessibilityBrowserTest::TearDownOnMainThread() {
#if defined(OS_WIN)
  com_initializer_.reset();
#endif
}

// Convenience method to get the value of a particular AXNode
// attribute as a UTF-8 string.
std::string CrossPlatformAccessibilityBrowserTest::GetAttr(
    const ui::AXNode* node,
    const ax::mojom::StringAttribute attr) {
  const ui::AXNodeData& data = node->data();
  for (size_t i = 0; i < data.string_attributes.size(); ++i) {
    if (data.string_attributes[i].first == attr)
      return data.string_attributes[i].second;
  }
  return std::string();
}

// Convenience method to get the value of a particular AXNode
// integer attribute.
int CrossPlatformAccessibilityBrowserTest::GetIntAttr(
    const ui::AXNode* node,
    const ax::mojom::IntAttribute attr) {
  const ui::AXNodeData& data = node->data();
  for (size_t i = 0; i < data.int_attributes.size(); ++i) {
    if (data.int_attributes[i].first == attr)
      return data.int_attributes[i].second;
  }
  return -1;
}

// Convenience method to get the value of a particular AXNode
// boolean attribute.
bool CrossPlatformAccessibilityBrowserTest::GetBoolAttr(
    const ui::AXNode* node,
    const ax::mojom::BoolAttribute attr) {
  const ui::AXNodeData& data = node->data();
  for (size_t i = 0; i < data.bool_attributes.size(); ++i) {
    if (data.bool_attributes[i].first == attr)
      return data.bool_attributes[i].second;
  }
  return false;
}

IN_PROC_BROWSER_TEST_F(CrossPlatformAccessibilityBrowserTest,
                       WebpageAccessibility) {
  // Create a data url and load it.
  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<html><head><title>Accessibility Test</title></head>"
      "<body><input type='button' value='push' /><input type='checkbox' />"
      "</body></html>";
  GURL url(url_str);
  NavigateToURL(shell(), url);
  const ui::AXTree& tree = GetAXTree();
  const ui::AXNode* root = tree.root();

  // Check properties of thet tree.
  EXPECT_STREQ(url_str, tree.data().url.c_str());
  EXPECT_STREQ("Accessibility Test", tree.data().title.c_str());
  EXPECT_STREQ("html", tree.data().doctype.c_str());
  EXPECT_STREQ("text/html", tree.data().mimetype.c_str());

  // Check properties of the root element of the tree.
  EXPECT_STREQ("Accessibility Test",
               GetAttr(root, ax::mojom::StringAttribute::kName).c_str());
  EXPECT_EQ(ax::mojom::Role::kRootWebArea, root->data().role);

  // Check properties of the BODY element.
  ASSERT_EQ(1, root->child_count());
  const ui::AXNode* body = root->ChildAtIndex(0);
  EXPECT_EQ(ax::mojom::Role::kGenericContainer, body->data().role);
  EXPECT_STREQ("body",
               GetAttr(body, ax::mojom::StringAttribute::kHtmlTag).c_str());
  EXPECT_STREQ("block",
               GetAttr(body, ax::mojom::StringAttribute::kDisplay).c_str());

  // Check properties of the two children of the BODY element.
  ASSERT_EQ(2, body->child_count());

  const ui::AXNode* button = body->ChildAtIndex(0);
  EXPECT_EQ(ax::mojom::Role::kButton, button->data().role);
  EXPECT_STREQ("input",
               GetAttr(button, ax::mojom::StringAttribute::kHtmlTag).c_str());
  EXPECT_STREQ("push",
               GetAttr(button, ax::mojom::StringAttribute::kName).c_str());
  EXPECT_STREQ("inline-block",
               GetAttr(button, ax::mojom::StringAttribute::kDisplay).c_str());
  ASSERT_EQ(2U, button->data().html_attributes.size());
  EXPECT_STREQ("type", button->data().html_attributes[0].first.c_str());
  EXPECT_STREQ("button", button->data().html_attributes[0].second.c_str());
  EXPECT_STREQ("value", button->data().html_attributes[1].first.c_str());
  EXPECT_STREQ("push", button->data().html_attributes[1].second.c_str());

  const ui::AXNode* checkbox = body->ChildAtIndex(1);
  EXPECT_EQ(ax::mojom::Role::kCheckBox, checkbox->data().role);
  EXPECT_STREQ("input",
               GetAttr(checkbox, ax::mojom::StringAttribute::kHtmlTag).c_str());
  EXPECT_STREQ("inline-block",
               GetAttr(checkbox, ax::mojom::StringAttribute::kDisplay).c_str());
  ASSERT_EQ(1U, checkbox->data().html_attributes.size());
  EXPECT_STREQ("type", checkbox->data().html_attributes[0].first.c_str());
  EXPECT_STREQ("checkbox", checkbox->data().html_attributes[0].second.c_str());
}

IN_PROC_BROWSER_TEST_F(CrossPlatformAccessibilityBrowserTest,
                       UnselectedEditableTextAccessibility) {
  // Create a data url and load it.
  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<body>"
      "<input value=\"Hello, world.\"/>"
      "</body></html>";
  GURL url(url_str);
  NavigateToURL(shell(), url);

  const ui::AXTree& tree = GetAXTree();
  const ui::AXNode* root = tree.root();
  ASSERT_EQ(1, root->child_count());
  const ui::AXNode* body = root->ChildAtIndex(0);
  ASSERT_EQ(1, body->child_count());
  const ui::AXNode* text = body->ChildAtIndex(0);
  EXPECT_EQ(ax::mojom::Role::kTextField, text->data().role);
  EXPECT_STREQ("input",
               GetAttr(text, ax::mojom::StringAttribute::kHtmlTag).c_str());
  EXPECT_EQ(0, GetIntAttr(text, ax::mojom::IntAttribute::kTextSelStart));
  EXPECT_EQ(0, GetIntAttr(text, ax::mojom::IntAttribute::kTextSelEnd));
  EXPECT_STREQ("Hello, world.",
               GetAttr(text, ax::mojom::StringAttribute::kValue).c_str());

  // TODO(dmazzoni): as soon as more accessibility code is cross-platform,
  // this code should test that the accessible info is dynamically updated
  // if the selection or value changes.
}

IN_PROC_BROWSER_TEST_F(CrossPlatformAccessibilityBrowserTest,
                       SelectedEditableTextAccessibility) {
  // Create a data url and load it.
  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<body onload=\"document.body.children[0].select();\">"
      "<input value=\"Hello, world.\"/>"
      "</body></html>";
  GURL url(url_str);
  NavigateToURL(shell(), url);

  const ui::AXTree& tree = GetAXTree();
  const ui::AXNode* root = tree.root();
  ASSERT_EQ(1, root->child_count());
  const ui::AXNode* body = root->ChildAtIndex(0);
  ASSERT_EQ(1, body->child_count());
  const ui::AXNode* text = body->ChildAtIndex(0);
  EXPECT_EQ(ax::mojom::Role::kTextField, text->data().role);
  EXPECT_STREQ("input",
               GetAttr(text, ax::mojom::StringAttribute::kHtmlTag).c_str());
  EXPECT_EQ(0, GetIntAttr(text, ax::mojom::IntAttribute::kTextSelStart));
  EXPECT_EQ(13, GetIntAttr(text, ax::mojom::IntAttribute::kTextSelEnd));
  EXPECT_STREQ("Hello, world.",
               GetAttr(text, ax::mojom::StringAttribute::kValue).c_str());
}

IN_PROC_BROWSER_TEST_F(CrossPlatformAccessibilityBrowserTest,
                       MultipleInheritanceAccessibility2) {
  // Here's a html snippet where Blink puts the same node as a child
  // of two different parents. Instead of checking the exact output, just
  // make sure that no id is reused in the resulting tree.
  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<script>\n"
      "  document.writeln('<q><section></section></q><q><li>');\n"
      "  setTimeout(function() {\n"
      "    document.close();\n"
      "  }, 1);\n"
      "</script>";
  GURL url(url_str);
  NavigateToURL(shell(), url);

  const ui::AXTree& tree = GetAXTree();
  const ui::AXNode* root = tree.root();
  std::unordered_set<int> ids;
  RecursiveAssertUniqueIds(root, &ids);
}

// TODO(dmazzoni): Needs to be rebaselined. http://crbug.com/347464
IN_PROC_BROWSER_TEST_F(CrossPlatformAccessibilityBrowserTest,
                       DISABLED_IframeAccessibility) {
  // Create a data url and load it.
  const char url_str[] =
      "data:text/html,"
      "<!doctype html><html><body>"
      "<button>Button 1</button>"
      "<iframe src='data:text/html,"
      "<!doctype html><html><body><button>Button 2</button></body></html>"
      "'></iframe>"
      "<button>Button 3</button>"
      "</body></html>";
  GURL url(url_str);
  NavigateToURL(shell(), url);

  const ui::AXTree& tree = GetAXTree();
  const ui::AXNode* root = tree.root();
  ASSERT_EQ(1, root->child_count());
  const ui::AXNode* body = root->ChildAtIndex(0);
  ASSERT_EQ(3, body->child_count());

  const ui::AXNode* button1 = body->ChildAtIndex(0);
  EXPECT_EQ(ax::mojom::Role::kButton, button1->data().role);
  EXPECT_STREQ("Button 1",
               GetAttr(button1, ax::mojom::StringAttribute::kName).c_str());

  const ui::AXNode* iframe = body->ChildAtIndex(1);
  EXPECT_STREQ("iframe",
               GetAttr(iframe, ax::mojom::StringAttribute::kHtmlTag).c_str());
  ASSERT_EQ(1, iframe->child_count());

  const ui::AXNode* sub_document = iframe->ChildAtIndex(0);
  EXPECT_EQ(ax::mojom::Role::kWebArea, sub_document->data().role);
  ASSERT_EQ(1, sub_document->child_count());

  const ui::AXNode* sub_body = sub_document->ChildAtIndex(0);
  ASSERT_EQ(1, sub_body->child_count());

  const ui::AXNode* button2 = sub_body->ChildAtIndex(0);
  EXPECT_EQ(ax::mojom::Role::kButton, button2->data().role);
  EXPECT_STREQ("Button 2",
               GetAttr(button2, ax::mojom::StringAttribute::kName).c_str());

  const ui::AXNode* button3 = body->ChildAtIndex(2);
  EXPECT_EQ(ax::mojom::Role::kButton, button3->data().role);
  EXPECT_STREQ("Button 3",
               GetAttr(button3, ax::mojom::StringAttribute::kName).c_str());
}

IN_PROC_BROWSER_TEST_F(CrossPlatformAccessibilityBrowserTest,
                       DuplicateChildrenAccessibility) {
  // Here's another html snippet where WebKit has a parent node containing
  // two duplicate child nodes. Instead of checking the exact output, just
  // make sure that no id is reused in the resulting tree.
  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<em><code ><h4 ></em>";
  GURL url(url_str);
  NavigateToURL(shell(), url);

  const ui::AXTree& tree = GetAXTree();
  const ui::AXNode* root = tree.root();
  std::unordered_set<int> ids;
  RecursiveAssertUniqueIds(root, &ids);
}

IN_PROC_BROWSER_TEST_F(CrossPlatformAccessibilityBrowserTest, MAYBE_TableSpan) {
  // +---+---+---+
  // |   1   | 2 |
  // +---+---+---+
  // | 3 |   4   |
  // +---+---+---+

  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<table border=1>"
      " <tr>"
      "  <td colspan=2>1</td><td>2</td>"
      " </tr>"
      " <tr>"
      "  <td>3</td><td colspan=2>4</td>"
      " </tr>"
      "</table>";
  GURL url(url_str);
  NavigateToURL(shell(), url);

  const ui::AXTree& tree = GetAXTree();
  const ui::AXNode* root = tree.root();
  const ui::AXNode* table = root->ChildAtIndex(0);
  EXPECT_EQ(ax::mojom::Role::kTable, table->data().role);
  ASSERT_GE(table->child_count(), 2);
  EXPECT_EQ(ax::mojom::Role::kRow, table->ChildAtIndex(0)->data().role);
  EXPECT_EQ(ax::mojom::Role::kRow, table->ChildAtIndex(1)->data().role);
  EXPECT_EQ(3, GetIntAttr(table, ax::mojom::IntAttribute::kTableColumnCount));
  EXPECT_EQ(2, GetIntAttr(table, ax::mojom::IntAttribute::kTableRowCount));

  const ui::AXNode* cell1 = table->ChildAtIndex(0)->ChildAtIndex(0);
  const ui::AXNode* cell2 = table->ChildAtIndex(0)->ChildAtIndex(1);
  const ui::AXNode* cell3 = table->ChildAtIndex(1)->ChildAtIndex(0);
  const ui::AXNode* cell4 = table->ChildAtIndex(1)->ChildAtIndex(1);

  EXPECT_EQ(0,
            GetIntAttr(cell1, ax::mojom::IntAttribute::kTableCellColumnIndex));
  EXPECT_EQ(0, GetIntAttr(cell1, ax::mojom::IntAttribute::kTableCellRowIndex));
  EXPECT_EQ(2,
            GetIntAttr(cell1, ax::mojom::IntAttribute::kTableCellColumnSpan));
  EXPECT_EQ(1, GetIntAttr(cell1, ax::mojom::IntAttribute::kTableCellRowSpan));
  EXPECT_EQ(2,
            GetIntAttr(cell2, ax::mojom::IntAttribute::kTableCellColumnIndex));
  EXPECT_EQ(1,
            GetIntAttr(cell2, ax::mojom::IntAttribute::kTableCellColumnSpan));
  EXPECT_EQ(0,
            GetIntAttr(cell3, ax::mojom::IntAttribute::kTableCellColumnIndex));
  EXPECT_EQ(1,
            GetIntAttr(cell3, ax::mojom::IntAttribute::kTableCellColumnSpan));
  EXPECT_EQ(1,
            GetIntAttr(cell4, ax::mojom::IntAttribute::kTableCellColumnIndex));
  EXPECT_EQ(2,
            GetIntAttr(cell4, ax::mojom::IntAttribute::kTableCellColumnSpan));
}

IN_PROC_BROWSER_TEST_F(CrossPlatformAccessibilityBrowserTest, WritableElement) {
  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<div role='textbox' tabindex=0>"
      " Some text"
      "</div>";
  GURL url(url_str);
  NavigateToURL(shell(), url);
  const ui::AXTree& tree = GetAXTree();
  const ui::AXNode* root = tree.root();
  ASSERT_EQ(1, root->child_count());
  const ui::AXNode* textbox = root->ChildAtIndex(0);
  EXPECT_TRUE(textbox->data().HasAction(ax::mojom::Action::kSetValue));
}

IN_PROC_BROWSER_TEST_F(CrossPlatformAccessibilityBrowserTest,
                       AriaSortDirection) {
  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<table><tr>"
      "<th scope='row' aria-sort='ascending'>row header 1</th>"
      "<th scope='row' aria-sort='descending'>row header 2</th>"
      "<th scope='col' aria-sort='custom'>col header 1</th>"
      "<th scope='col' aria-sort='none'>col header 2</th>"
      "<th scope='col'>col header 3</th>"
      "</tr></table>";
  GURL url(url_str);
  NavigateToURL(shell(), url);

  const ui::AXTree& tree = GetAXTree();
  const ui::AXNode* root = tree.root();
  const ui::AXNode* table = root->ChildAtIndex(0);
  EXPECT_EQ(ax::mojom::Role::kTable, table->data().role);
  EXPECT_EQ(1, table->child_count());
  const ui::AXNode* row = table->ChildAtIndex(0);
  EXPECT_EQ(5, row->child_count());

  const ui::AXNode* header1 = row->ChildAtIndex(0);
  const ui::AXNode* header2 = row->ChildAtIndex(1);
  const ui::AXNode* header3 = row->ChildAtIndex(2);
  const ui::AXNode* header4 = row->ChildAtIndex(3);
  const ui::AXNode* header5 = row->ChildAtIndex(4);

  EXPECT_EQ(static_cast<int>(ax::mojom::SortDirection::kAscending),
            GetIntAttr(header1, ax::mojom::IntAttribute::kSortDirection));

  EXPECT_EQ(static_cast<int>(ax::mojom::SortDirection::kDescending),
            GetIntAttr(header2, ax::mojom::IntAttribute::kSortDirection));

  EXPECT_EQ(static_cast<int>(ax::mojom::SortDirection::kOther),
            GetIntAttr(header3, ax::mojom::IntAttribute::kSortDirection));

  EXPECT_EQ(-1, GetIntAttr(header4, ax::mojom::IntAttribute::kSortDirection));
  EXPECT_EQ(-1, GetIntAttr(header5, ax::mojom::IntAttribute::kSortDirection));
}

IN_PROC_BROWSER_TEST_F(CrossPlatformAccessibilityBrowserTest,
                       LocalizedLandmarkType) {
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(
      "data:text/html,"
      "<!doctype html>"
      "<header aria-label='header'></header>"
      "<aside aria-label='aside'></aside>"
      "<footer aria-label='footer'></footer>"
      "<form aria-label='form'></form>"
      "<main aria-label='main'></main>"
      "<nav aria-label='nav'></nav>"
      "<section></section>"
      "<section aria-label='section'></section>"
      "<div role='banner' aria-label='banner'></div>"
      "<div role='complementary' aria-label='complementary'></div>"
      "<div role='contentinfo' aria-label='contentinfo'></div>"
      "<div role='form' aria-label='role_form'></div>"
      "<div role='main' aria-label='role_main'></div>"
      "<div role='navigation' aria-label='role_nav'></div>"
      "<div role='region'></div>"
      "<div role='region' aria-label='region'></div>"
      "<div role='search' aria-label='search'></div>");

  NavigateToURL(shell(), url);
  waiter.WaitForNotification();

  BrowserAccessibility* root = GetManager()->GetRoot();
  ASSERT_NE(nullptr, root);
  ASSERT_EQ(17u, root->PlatformChildCount());

  auto TestLocalizedLandmarkType =
      [root](int child_index, ax::mojom::Role expected_role,
             const std::string& expected_name,
             const base::string16& expected_localized_landmark_type = {}) {
        BrowserAccessibility* node = root->PlatformGetChild(child_index);
        ASSERT_NE(nullptr, node);

        EXPECT_EQ(expected_role, node->GetRole());
        EXPECT_EQ(expected_name,
                  node->GetStringAttribute(ax::mojom::StringAttribute::kName));
        EXPECT_EQ(expected_localized_landmark_type,
                  node->GetLocalizedStringForLandmarkType());
      };

  // For testing purposes, assume we get en-US localized strings.
  TestLocalizedLandmarkType(0, ax::mojom::Role::kBanner, "header",
                            base::ASCIIToUTF16("banner"));
  TestLocalizedLandmarkType(1, ax::mojom::Role::kComplementary, "aside",
                            base::ASCIIToUTF16("complementary"));
  TestLocalizedLandmarkType(2, ax::mojom::Role::kFooter, "footer",
                            base::ASCIIToUTF16("content info"));
  TestLocalizedLandmarkType(3, ax::mojom::Role::kForm, "form");
  TestLocalizedLandmarkType(4, ax::mojom::Role::kMain, "main");
  TestLocalizedLandmarkType(5, ax::mojom::Role::kNavigation, "nav");
  TestLocalizedLandmarkType(6, ax::mojom::Role::kRegion, "");
  TestLocalizedLandmarkType(7, ax::mojom::Role::kRegion, "section",
                            base::ASCIIToUTF16("region"));

  TestLocalizedLandmarkType(8, ax::mojom::Role::kBanner, "banner",
                            base::ASCIIToUTF16("banner"));
  TestLocalizedLandmarkType(9, ax::mojom::Role::kComplementary, "complementary",
                            base::ASCIIToUTF16("complementary"));
  TestLocalizedLandmarkType(10, ax::mojom::Role::kContentInfo, "contentinfo",
                            base::ASCIIToUTF16("content info"));
  TestLocalizedLandmarkType(11, ax::mojom::Role::kForm, "role_form");
  TestLocalizedLandmarkType(12, ax::mojom::Role::kMain, "role_main");
  TestLocalizedLandmarkType(13, ax::mojom::Role::kNavigation, "role_nav");
  TestLocalizedLandmarkType(14, ax::mojom::Role::kRegion, "");
  TestLocalizedLandmarkType(15, ax::mojom::Role::kRegion, "region",
                            base::ASCIIToUTF16("region"));
  TestLocalizedLandmarkType(16, ax::mojom::Role::kSearch, "search");
}

IN_PROC_BROWSER_TEST_F(CrossPlatformAccessibilityBrowserTest,
                       GetStyleNameAttributeAsLocalizedString) {
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(
      "data:text/html,"
      "<!doctype html>"
      "<p>text <mark>mark text</mark></p>");

  NavigateToURL(shell(), url);
  waiter.WaitForNotification();

  BrowserAccessibility* root = GetManager()->GetRoot();
  ASSERT_NE(nullptr, root);
  ASSERT_EQ(1u, root->PlatformChildCount());

  auto TestGetStyleNameAttributeAsLocalizedString =
      [](BrowserAccessibility* node, ax::mojom::Role expected_role,
         const base::string16& expected_localized_style_name_attribute = {}) {
        ASSERT_NE(nullptr, node);

        EXPECT_EQ(expected_role, node->GetRole());
        EXPECT_EQ(expected_localized_style_name_attribute,
                  node->GetStyleNameAttributeAsLocalizedString());
      };

  // For testing purposes, assume we get en-US localized strings.
  BrowserAccessibility* para_node = root->PlatformGetChild(0);
  ASSERT_EQ(2u, para_node->PlatformChildCount());
  TestGetStyleNameAttributeAsLocalizedString(para_node,
                                             ax::mojom::Role::kParagraph);

  BrowserAccessibility* text_node = para_node->PlatformGetChild(0);
  ASSERT_EQ(0u, text_node->PlatformChildCount());
  TestGetStyleNameAttributeAsLocalizedString(text_node,
                                             ax::mojom::Role::kStaticText);

  BrowserAccessibility* mark_node = para_node->PlatformGetChild(1);
  TestGetStyleNameAttributeAsLocalizedString(
      mark_node, ax::mojom::Role::kMark,
      base::ASCIIToUTF16("highlighted content"));

  // Android doesn't always have a child in this case.
  if (mark_node->PlatformChildCount() > 0) {
    BrowserAccessibility* mark_text_node = mark_node->PlatformGetChild(0);
    ASSERT_EQ(0u, mark_text_node->PlatformChildCount());
    TestGetStyleNameAttributeAsLocalizedString(
        mark_text_node, ax::mojom::Role::kStaticText,
        base::ASCIIToUTF16("highlighted content"));
  }
}

IN_PROC_BROWSER_TEST_F(CrossPlatformAccessibilityBrowserTest,
                       TooltipStringAttributeMutuallyExclusiveOfNameFromTitle) {
  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<input type='text' title='title'>"
      "<input type='text' title='title' aria-labelledby='inputlabel'>"
      "<div id='inputlabel'>aria-labelledby</div>";
  GURL url(url_str);
  NavigateToURL(shell(), url);

  const ui::AXTree& tree = GetAXTree();
  const ui::AXNode* root = tree.root();
  const ui::AXNode* input1 = root->ChildAtIndex(0);
  const ui::AXNode* input2 = root->ChildAtIndex(1);

  EXPECT_EQ(static_cast<int>(ax::mojom::NameFrom::kTitle),
            GetIntAttr(input1, ax::mojom::IntAttribute::kNameFrom));
  EXPECT_STREQ("title",
               GetAttr(input1, ax::mojom::StringAttribute::kName).c_str());
  EXPECT_STREQ("",
               GetAttr(input1, ax::mojom::StringAttribute::kTooltip).c_str());

  EXPECT_EQ(static_cast<int>(ax::mojom::NameFrom::kRelatedElement),
            GetIntAttr(input2, ax::mojom::IntAttribute::kNameFrom));
  EXPECT_STREQ("aria-labelledby",
               GetAttr(input2, ax::mojom::StringAttribute::kName).c_str());
  EXPECT_STREQ("title",
               GetAttr(input2, ax::mojom::StringAttribute::kTooltip).c_str());
}

IN_PROC_BROWSER_TEST_F(
    CrossPlatformAccessibilityBrowserTest,
    PlaceholderStringAttributeMutuallyExclusiveOfNameFromPlaceholder) {
  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<input type='text' placeholder='placeholder'>"
      "<input type='text' placeholder='placeholder' aria-label='label'>";
  GURL url(url_str);
  NavigateToURL(shell(), url);

  const ui::AXTree& tree = GetAXTree();
  const ui::AXNode* root = tree.root();
  const ui::AXNode* group = root->ChildAtIndex(0);
  const ui::AXNode* input1 = group->ChildAtIndex(0);
  const ui::AXNode* input2 = group->ChildAtIndex(1);

  using ax::mojom::StringAttribute;

  EXPECT_EQ(static_cast<int>(ax::mojom::NameFrom::kPlaceholder),
            GetIntAttr(input1, ax::mojom::IntAttribute::kNameFrom));
  EXPECT_STREQ("placeholder", GetAttr(input1, StringAttribute::kName).c_str());
  EXPECT_STREQ("", GetAttr(input1, StringAttribute::kPlaceholder).c_str());

  EXPECT_EQ(static_cast<int>(ax::mojom::NameFrom::kAttribute),
            GetIntAttr(input2, ax::mojom::IntAttribute::kNameFrom));
  EXPECT_STREQ("label", GetAttr(input2, StringAttribute::kName).c_str());
  EXPECT_STREQ("placeholder",
               GetAttr(input2, StringAttribute::kPlaceholder).c_str());
}

// On Android root scroll offset is handled by the Java layer. The final rect
// bounds is device specific.
#ifndef OS_ANDROID
IN_PROC_BROWSER_TEST_F(CrossPlatformAccessibilityBrowserTest,
                       GetBoundsRectUnclippedRootFrameFromIFrame) {
  // Start by loading a document with iframes.
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/html/iframe-padding.html");
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Second Button");

  // Get the delegate for the iframe leaf of the top-level accessibility tree
  // for the second iframe.
  BrowserAccessibilityManager* browser_accessibility_manager = GetManager();
  ASSERT_NE(nullptr, browser_accessibility_manager);
  BrowserAccessibility* root_browser_accessibility =
      browser_accessibility_manager->GetRoot();
  ASSERT_NE(nullptr, root_browser_accessibility);
  BrowserAccessibility* leaf_iframe_browser_accessibility =
      root_browser_accessibility->InternalDeepestLastChild();
  ASSERT_NE(nullptr, leaf_iframe_browser_accessibility);
  ASSERT_EQ(ax::mojom::Role::kIframe,
            leaf_iframe_browser_accessibility->GetRole());

  // The frame coordinates of the iframe node within the top-level tree is
  // relative to the top level frame. That is why the top-level default padding
  // is included.
  ASSERT_EQ(gfx::Rect(30, 230, 300, 100).ToString(),
            leaf_iframe_browser_accessibility
                ->GetBoundsRect(ui::AXCoordinateSystem::kRootFrame,
                                ui::AXClippingBehavior::kUnclipped)
                .ToString());

  // Now get the root delegate of the iframe's accessibility tree.
  AXTreeID iframe_tree_id = AXTreeID::FromString(
      leaf_iframe_browser_accessibility->GetStringAttribute(
          ax::mojom::StringAttribute::kChildTreeId));
  BrowserAccessibilityManager* iframe_browser_accessibility_manager =
      BrowserAccessibilityManager::FromID(iframe_tree_id);
  ASSERT_NE(nullptr, iframe_browser_accessibility_manager);
  BrowserAccessibility* root_iframe_browser_accessibility =
      iframe_browser_accessibility_manager->GetRoot();
  ASSERT_NE(nullptr, root_iframe_browser_accessibility);
  ASSERT_EQ(ax::mojom::Role::kRootWebArea,
            root_iframe_browser_accessibility->GetRole());

  // The root frame bounds of the iframe are still relative to the top-level
  // frame.
  ASSERT_EQ(gfx::Rect(30, 230, 300, 100).ToString(),
            root_iframe_browser_accessibility
                ->GetBoundsRect(ui::AXCoordinateSystem::kRootFrame,
                                ui::AXClippingBehavior::kUnclipped)
                .ToString());
}

IN_PROC_BROWSER_TEST_F(CrossPlatformAccessibilityBrowserTest,
                       GetBoundsRectUnclippedFrameFromIFrame) {
  // Start by loading a document with iframes.
  LoadInitialAccessibilityTreeFromHtmlFilePath(
      "/accessibility/html/iframe-padding.html");
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Second Button");

  // Get the delegate for the iframe leaf of the top-level accessibility tree
  // for the second iframe.
  BrowserAccessibilityManager* browser_accessibility_manager = GetManager();
  ASSERT_NE(nullptr, browser_accessibility_manager);
  BrowserAccessibility* root_browser_accessibility =
      browser_accessibility_manager->GetRoot();
  ASSERT_NE(nullptr, root_browser_accessibility);
  BrowserAccessibility* leaf_iframe_browser_accessibility =
      root_browser_accessibility->InternalDeepestLastChild();
  ASSERT_NE(nullptr, leaf_iframe_browser_accessibility);
  ASSERT_EQ(ax::mojom::Role::kIframe,
            leaf_iframe_browser_accessibility->GetRole());

  // The frame coordinates of the iframe node within the top-level tree is
  // relative to the top level frame.
  ASSERT_EQ(gfx::Rect(30, 230, 300, 100).ToString(),
            leaf_iframe_browser_accessibility
                ->GetBoundsRect(ui::AXCoordinateSystem::kFrame,
                                ui::AXClippingBehavior::kUnclipped)
                .ToString());

  // Now get the root delegate of the iframe's accessibility tree.
  AXTreeID iframe_tree_id = AXTreeID::FromString(
      leaf_iframe_browser_accessibility->GetStringAttribute(
          ax::mojom::StringAttribute::kChildTreeId));
  BrowserAccessibilityManager* iframe_browser_accessibility_manager =
      BrowserAccessibilityManager::FromID(iframe_tree_id);
  ASSERT_NE(nullptr, iframe_browser_accessibility_manager);
  BrowserAccessibility* root_iframe_browser_accessibility =
      iframe_browser_accessibility_manager->GetRoot();
  ASSERT_NE(nullptr, root_iframe_browser_accessibility);
  ASSERT_EQ(ax::mojom::Role::kRootWebArea,
            root_iframe_browser_accessibility->GetRole());

  // The frame bounds of the iframe are now relative to itself.
  ASSERT_EQ(gfx::Rect(0, 0, 300, 100).ToString(),
            root_iframe_browser_accessibility
                ->GetBoundsRect(ui::AXCoordinateSystem::kFrame,
                                ui::AXClippingBehavior::kUnclipped)
                .ToString());
}
#endif
}  // namespace content
