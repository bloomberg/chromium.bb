// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_TEST_AX_NODE_WRAPPER_H_
#define UI_ACCESSIBILITY_PLATFORM_TEST_AX_NODE_WRAPPER_H_

#include <set>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate_base.h"

#if defined(OS_WIN)
namespace gfx {
const AcceleratedWidget kMockAcceleratedWidget = reinterpret_cast<HWND>(-1);
}
#endif

namespace ui {

// For testing, a TestAXNodeWrapper wraps an AXNode, implements
// AXPlatformNodeDelegate, and owns an AXPlatformNode.
class TestAXNodeWrapper : public AXPlatformNodeDelegateBase {
 public:
  // Create TestAXNodeWrapper instances on-demand from an AXTree and AXNode.
  static TestAXNodeWrapper* GetOrCreate(AXTree* tree, AXNode* node);

  // Set a global coordinate offset for testing.
  static void SetGlobalCoordinateOffset(const gfx::Vector2d& offset);

  // Get the last node which ShowContextMenu was called from for testing.
  static const AXNode* GetNodeFromLastShowContextMenu();

  // Get the last node which AccessibilityPerformAction default action was
  // called from for testing.
  static const AXNode* GetNodeFromLastDefaultAction();

  ~TestAXNodeWrapper() override;

  AXPlatformNode* ax_platform_node() const { return platform_node_; }

  // Test helpers.
  void BuildAllWrappers(AXTree* tree, AXNode* node);
  void ResetNativeEventTarget();

  // AXPlatformNodeDelegate.
  const AXNodeData& GetData() const override;
  const AXTreeData& GetTreeData() const override;
  AXNodePosition::AXPositionInstance CreateTextPositionAt(
      int offset,
      ax::mojom::TextAffinity affinity =
          ax::mojom::TextAffinity::kDownstream) const override;
  gfx::NativeViewAccessible GetParent() override;
  int GetChildCount() override;
  gfx::NativeViewAccessible ChildAtIndex(int index) override;
  gfx::Rect GetBoundsRect(const AXCoordinateSystem coordinate_system,
                          const AXClippingBehavior clipping_behavior,
                          AXOffscreenResult* offscreen_result) const override;
  gfx::Rect GetInnerTextRangeBoundsRect(
      const int start_offset,
      const int end_offset,
      const AXCoordinateSystem coordinate_system,
      const AXClippingBehavior clipping_behavior,
      AXOffscreenResult* offscreen_result) const override;
  gfx::Rect GetHypertextRangeBoundsRect(
      const int start_offset,
      const int end_offset,
      const AXCoordinateSystem coordinate_system,
      const AXClippingBehavior clipping_behavior,
      AXOffscreenResult* offscreen_result) const override;
  gfx::NativeViewAccessible HitTestSync(int x, int y) override;
  gfx::NativeViewAccessible GetFocus() override;
  AXPlatformNode* GetFromNodeID(int32_t id) override;
  int GetIndexInParent() const override;
  bool IsTable() const override;
  int GetTableRowCount() const override;
  int GetTableColCount() const override;
  base::Optional<int32_t> GetTableAriaColCount() const override;
  base::Optional<int32_t> GetTableAriaRowCount() const override;
  int GetTableCellCount() const override;
  const std::vector<int32_t> GetColHeaderNodeIds() const override;
  const std::vector<int32_t> GetColHeaderNodeIds(
      int32_t col_index) const override;
  const std::vector<int32_t> GetRowHeaderNodeIds() const override;
  const std::vector<int32_t> GetRowHeaderNodeIds(
      int32_t row_index) const override;
  bool IsTableRow() const override;
  int GetTableRowRowIndex() const override;
  bool IsTableCellOrHeader() const override;
  int GetTableCellIndex() const override;
  int GetTableCellColIndex() const override;
  int GetTableCellRowIndex() const override;
  int GetTableCellColSpan() const override;
  int GetTableCellRowSpan() const override;
  int GetTableCellAriaColIndex() const override;
  int GetTableCellAriaRowIndex() const override;
  int32_t GetCellId(int32_t row_index, int32_t col_index) const override;
  int32_t CellIndexToId(int32_t cell_index) const override;
  bool IsCellOrHeaderOfARIATable() const override;
  bool IsCellOrHeaderOfARIAGrid() const override;
  gfx::AcceleratedWidget GetTargetForNativeAccessibilityEvent() override;
  bool AccessibilityPerformAction(const AXActionData& data) override;
  base::string16 GetLocalizedRoleDescriptionForUnlabeledImage() const override;
  base::string16 GetLocalizedStringForLandmarkType() const override;
  base::string16 GetLocalizedStringForImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus status) const override;
  base::string16 GetStyleNameAttributeAsLocalizedString() const override;
  bool ShouldIgnoreHoveredStateForTesting() override;
  const ui::AXUniqueId& GetUniqueId() const override;
  std::set<AXPlatformNode*> GetReverseRelations(
      ax::mojom::IntAttribute attr) override;
  std::set<AXPlatformNode*> GetReverseRelations(
      ax::mojom::IntListAttribute attr) override;
  bool IsOrderedSetItem() const override;
  bool IsOrderedSet() const override;
  int32_t GetPosInSet() const override;
  int32_t GetSetSize() const override;
  const std::vector<gfx::NativeViewAccessible> GetDescendants() const override;

 private:
  TestAXNodeWrapper(AXTree* tree, AXNode* node);
  void ReplaceIntAttribute(int32_t node_id,
                           ax::mojom::IntAttribute attribute,
                           int32_t value);
  void ReplaceFloatAttribute(ax::mojom::FloatAttribute attribute, float value);
  void ReplaceBoolAttribute(ax::mojom::BoolAttribute attribute, bool value);
  void ReplaceStringAttribute(ax::mojom::StringAttribute attribute,
                              std::string value);
  void ReplaceTreeDataTextSelection(int32_t anchor_node_id,
                                    int32_t anchor_offset,
                                    int32_t focus_node_id,
                                    int32_t focus_offset);

  TestAXNodeWrapper* HitTestSyncInternal(int x, int y);
  void Descendants(const AXNode* node,
                   std::vector<gfx::NativeViewAccessible>* descendants) const;

  AXTree* tree_;
  AXNode* node_;
  ui::AXUniqueId unique_id_;
  AXPlatformNode* platform_node_;
  gfx::AcceleratedWidget native_event_target_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_TEST_AX_NODE_WRAPPER_H_
