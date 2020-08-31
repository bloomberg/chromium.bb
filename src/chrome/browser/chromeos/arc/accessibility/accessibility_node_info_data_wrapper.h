// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ACCESSIBILITY_NODE_INFO_DATA_WRAPPER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ACCESSIBILITY_NODE_INFO_DATA_WRAPPER_H_

#include "chrome/browser/chromeos/arc/accessibility/accessibility_info_data_wrapper.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_node_data.h"

namespace arc {

class AXTreeSourceArc;

// Wrapper class for an AccessibilityWindowInfoData.
class AccessibilityNodeInfoDataWrapper : public AccessibilityInfoDataWrapper {
 public:
  AccessibilityNodeInfoDataWrapper(AXTreeSourceArc* tree_source,
                                   mojom::AccessibilityNodeInfoData* node,
                                   bool is_clickable_leaf,
                                   bool is_important);

  ~AccessibilityNodeInfoDataWrapper() override;

  // AccessibilityInfoDataWrapper overrides.
  bool IsNode() const override;
  mojom::AccessibilityNodeInfoData* GetNode() const override;
  mojom::AccessibilityWindowInfoData* GetWindow() const override;
  int32_t GetId() const override;
  const gfx::Rect GetBounds() const override;
  bool IsVisibleToUser() const override;
  bool IsVirtualNode() const override;
  bool CanBeAccessibilityFocused() const override;
  void PopulateAXRole(ui::AXNodeData* out_data) const override;
  void PopulateAXState(ui::AXNodeData* out_data) const override;
  void Serialize(ui::AXNodeData* out_data) const override;
  void GetChildren(
      std::vector<AccessibilityInfoDataWrapper*>* children) const override;

  mojom::AccessibilityNodeInfoData* node() { return node_ptr_; }

  void set_role(ax::mojom::Role role) { role_ = role; }
  void set_cached_name(const std::string& name) { cached_name_ = name; }
  void set_container_live_status(mojom::AccessibilityLiveRegionType status) {
    container_live_status_ = status;
  }

 private:
  bool GetProperty(mojom::AccessibilityBooleanProperty prop) const;
  bool GetProperty(mojom::AccessibilityIntProperty prop,
                   int32_t* out_value) const;
  bool HasProperty(mojom::AccessibilityStringProperty prop) const;
  bool GetProperty(mojom::AccessibilityStringProperty prop,
                   std::string* out_value) const;
  bool GetProperty(mojom::AccessibilityIntListProperty prop,
                   std::vector<int32_t>* out_value) const;
  bool GetProperty(mojom::AccessibilityStringListProperty prop,
                   std::vector<std::string>* out_value) const;

  bool HasStandardAction(mojom::AccessibilityActionType action) const;

  bool HasCoveringSpan(mojom::AccessibilityStringProperty prop,
                       mojom::SpanType span_type) const;

  void ComputeNameFromContents(const AccessibilityNodeInfoDataWrapper* data,
                               std::vector<std::string>* names) const;

  bool IsInterestingLeaf() const;

  mojom::AccessibilityNodeInfoData* node_ptr_ = nullptr;

  bool is_clickable_leaf_;
  bool is_important_;

  base::Optional<ax::mojom::Role> role_;
  base::Optional<std::string> cached_name_;
  mojom::AccessibilityLiveRegionType container_live_status_ =
      mojom::AccessibilityLiveRegionType::NONE;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityNodeInfoDataWrapper);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ACCESSIBILITY_NODE_INFO_DATA_WRAPPER_H_
