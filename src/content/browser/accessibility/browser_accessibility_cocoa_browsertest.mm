// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_cocoa.h"
#include "content/browser/accessibility/browser_accessibility_mac.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_manager_mac.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"
#include "net/base/data_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

// This file uses the deprecated NSObject accessibility APIs:
// https://crbug.com/921109
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace content {

namespace {

class BrowserAccessibilityCocoaBrowserTest : public ContentBrowserTest {
 public:
  BrowserAccessibilityCocoaBrowserTest() {}
  ~BrowserAccessibilityCocoaBrowserTest() override {}

 protected:
  BrowserAccessibility* FindNode(ax::mojom::Role role) {
    BrowserAccessibility* root = GetManager()->GetRoot();
    CHECK(root);
    return FindNodeInSubtree(*root, role);
  }

  BrowserAccessibilityManager* GetManager() {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    return web_contents->GetRootBrowserAccessibilityManager();
  }

 private:
  BrowserAccessibility* FindNodeInSubtree(BrowserAccessibility& node,
                                          ax::mojom::Role role) {
    if (node.GetRole() == role)
      return &node;
    for (unsigned int i = 0; i < node.PlatformChildCount(); ++i) {
      BrowserAccessibility* result =
          FindNodeInSubtree(*node.PlatformGetChild(i), role);
      if (result)
        return result;
    }
    return nullptr;
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(BrowserAccessibilityCocoaBrowserTest,
                       AXCellForColumnAndRow) {
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(R"HTML(data:text/html,
             <table>
               <thead style=display:block>
                 <tr>
                   <th>Name</th>
                   <th>LDAP</th>
                 </tr>
               </thead>
               <tbody style=display:block>
                 <tr>
                   <td>John Doe</td>
                   <td>johndoe@</td>
                 </tr>
                 <tr>
                   <td>Jenny Doe</td>
                   <td>jennydoe@</td>
                 </tr>
               </tbody>
             </table>)HTML");

  NavigateToURL(shell(), url);
  waiter.WaitForNotification();

  BrowserAccessibility* table = FindNode(ax::mojom::Role::kTable);
  ASSERT_NE(nullptr, table);
  base::scoped_nsobject<BrowserAccessibilityCocoa> cocoa_table(
      [ToBrowserAccessibilityCocoa(table) retain]);

  // Test AXCellForColumnAndRow for four coordinates
  for (unsigned col = 0; col < 2; col++) {
    for (unsigned row = 0; row < 2; row++) {
      id parameter = [[[NSMutableArray alloc] initWithCapacity:2] autorelease];
      [parameter addObject:[NSNumber numberWithInt:col]];
      [parameter addObject:[NSNumber numberWithInt:row]];
      base::scoped_nsobject<BrowserAccessibilityCocoa> cell(
          [[cocoa_table accessibilityAttributeValue:@"AXCellForColumnAndRow"
                                       forParameter:parameter] retain]);

      // It should be a cell.
      EXPECT_NSEQ(@"AXCell", [cell role]);

      // The column index and row index of the cell should match what we asked
      // for.
      EXPECT_EQ(col, [[cell accessibilityAttributeValue:@"AXColumnIndexRange"]
                         rangeValue]
                         .location);
      EXPECT_EQ(row, [[cell accessibilityAttributeValue:@"AXRowIndexRange"]
                         rangeValue]
                         .location);
    }
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAccessibilityCocoaBrowserTest,
                       TestUnlabeledImageRoleDescription) {
  ui::AXNodeData image;
  image.id = 1;
  image.role = ax::mojom::Role::kImage;
  image.AddStringAttribute(ax::mojom::StringAttribute::kRoleDescription, "bar");
  image.SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation);

  std::unique_ptr<BrowserAccessibilityManagerMac> manager(
      new BrowserAccessibilityManagerMac(MakeAXTreeUpdate(image), nullptr));

  base::scoped_nsobject<BrowserAccessibilityCocoa> ax_node(
      [ToBrowserAccessibilityCocoa(manager->GetRoot()) retain]);

  EXPECT_NSEQ(@"Unlabeled image", [ax_node roleDescription]);
}

IN_PROC_BROWSER_TEST_F(BrowserAccessibilityCocoaBrowserTest,
                       TestAnnotatedImageDescription) {
  std::vector<const char*> expected_descriptions;

  ui::AXTreeUpdate tree;
  tree.root_id = 1;
  tree.nodes.resize(10);
  tree.nodes[0].id = 1;
  tree.nodes[0].child_ids = {2, 3, 4, 5, 6, 7, 8, 9, 10};

  // If the status is EligibleForAnnotation and there's no existing label,
  // the description should be the discoverability string.
  tree.nodes[1].id = 2;
  tree.nodes[1].role = ax::mojom::Role::kImage;
  tree.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[1].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation);
  expected_descriptions.push_back(
      "To get missing image descriptions, open the context menu.");

  // If the status is EligibleForAnnotation, the discoverability string
  // should be appended to the existing name.
  tree.nodes[2].id = 3;
  tree.nodes[2].role = ax::mojom::Role::kImage;
  tree.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[2].SetName("ExistingLabel");
  tree.nodes[2].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation);
  expected_descriptions.push_back(
      "ExistingLabel. To get missing image descriptions, open the context "
      "menu.");

  // If the status is IneligibleForAnnotation, nothing should be appended.
  tree.nodes[3].id = 4;
  tree.nodes[3].role = ax::mojom::Role::kImage;
  tree.nodes[3].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[3].SetName("ExistingLabel");
  tree.nodes[3].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation);
  expected_descriptions.push_back("ExistingLabel");

  // If the status is AnnotationPending, pending text should be appended
  // to the name.
  tree.nodes[4].id = 5;
  tree.nodes[4].role = ax::mojom::Role::kImage;
  tree.nodes[4].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[4].SetName("ExistingLabel");
  tree.nodes[4].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationPending);
  expected_descriptions.push_back("ExistingLabel. Getting description…");

  // If the status is AnnotationSucceeded, and there's no annotation,
  // nothing should be appended. (Ideally this shouldn't happen.)
  tree.nodes[5].id = 6;
  tree.nodes[5].role = ax::mojom::Role::kImage;
  tree.nodes[5].SetName("ExistingLabel");
  tree.nodes[5].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded);
  expected_descriptions.push_back("ExistingLabel");

  // If the status is AnnotationSucceeded, the annotation should be appended
  // to the existing label.
  tree.nodes[6].id = 7;
  tree.nodes[6].role = ax::mojom::Role::kImage;
  tree.nodes[6].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[6].SetName("ExistingLabel");
  tree.nodes[6].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded);
  expected_descriptions.push_back("ExistingLabel. Annotation");

  // If the status is AnnotationEmpty, failure text should be added to the
  // name.
  tree.nodes[7].id = 8;
  tree.nodes[7].role = ax::mojom::Role::kImage;
  tree.nodes[7].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[7].SetName("ExistingLabel");
  tree.nodes[7].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationEmpty);
  expected_descriptions.push_back("ExistingLabel. No description available.");

  // If the status is AnnotationAdult, appropriate text should be appended
  // to the name.
  tree.nodes[8].id = 9;
  tree.nodes[8].role = ax::mojom::Role::kImage;
  tree.nodes[8].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[8].SetName("ExistingLabel");
  tree.nodes[8].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationAdult);
  expected_descriptions.push_back("ExistingLabel. Appears to contain adult "
                                  "content. No description available.");

  // If the status is AnnotationProcessFailed, failure text should be added
  // to the name.
  tree.nodes[9].id = 10;
  tree.nodes[9].role = ax::mojom::Role::kImage;
  tree.nodes[9].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[9].SetName("ExistingLabel");
  tree.nodes[9].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed);
  expected_descriptions.push_back("ExistingLabel. No description available.");

  // We should have one expected description per child of the root.
  ASSERT_EQ(expected_descriptions.size(), tree.nodes[0].child_ids.size());
  int child_count = static_cast<int>(expected_descriptions.size());

  std::unique_ptr<BrowserAccessibilityManagerMac> manager(
      new BrowserAccessibilityManagerMac(tree, nullptr));

  for (int child_index = 0; child_index < child_count; child_index++) {
    BrowserAccessibility* child =
        manager->GetRoot()->PlatformGetChild(child_index);
    base::scoped_nsobject<BrowserAccessibilityCocoa> child_obj(
        [ToBrowserAccessibilityCocoa(child) retain]);

    EXPECT_NSEQ(base::SysUTF8ToNSString(expected_descriptions[child_index]),
                [child_obj description]);
  }
}

}  // namespace content

#pragma clang diagnostic pop
