// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_H_
#define COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/renderer/plugin_ax_tree_source.h"
#include "content/public/renderer/render_frame_observer.h"
#include "pdf/pdf_accessibility_data_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_source.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace chrome_pdf {
class PdfAccessibilityActionHandler;
struct AccessibilityActionData;
struct AccessibilityCharInfo;
struct AccessibilityDocInfo;
struct AccessibilityPageInfo;
struct AccessibilityPageObjects;
struct AccessibilityTextRunInfo;
struct AccessibilityViewportInfo;
struct PageCharacterIndex;
}  // namespace chrome_pdf

namespace content {
class RenderAccessibility;
class RenderFrame;
}  // namespace content

namespace gfx {
class Transform;
}

namespace pdf {

class PdfAccessibilityTree : public content::PluginAXTreeSource,
                             public content::RenderFrameObserver,
                             public chrome_pdf::PdfAccessibilityDataHandler {
 public:
  PdfAccessibilityTree(
      content::RenderFrame* render_frame,
      chrome_pdf::PdfAccessibilityActionHandler* action_handler);
  ~PdfAccessibilityTree() override;

  static bool IsDataFromPluginValid(
      const std::vector<chrome_pdf::AccessibilityTextRunInfo>& text_runs,
      const std::vector<chrome_pdf::AccessibilityCharInfo>& chars,
      const chrome_pdf::AccessibilityPageObjects& page_objects);

  // Stores the page index and annotation index in the page.
  struct AnnotationInfo {
    AnnotationInfo(uint32_t page_index, uint32_t annotation_index);
    AnnotationInfo(const AnnotationInfo& other);
    ~AnnotationInfo();

    uint32_t page_index;
    uint32_t annotation_index;
  };

  // chrome_pdf::PdfAccessibilityDataHandler:
  void SetAccessibilityViewportInfo(
      const chrome_pdf::AccessibilityViewportInfo& viewport_info) override;
  void SetAccessibilityDocInfo(
      const chrome_pdf::AccessibilityDocInfo& doc_info) override;
  void SetAccessibilityPageInfo(
      const chrome_pdf::AccessibilityPageInfo& page_info,
      const std::vector<chrome_pdf::AccessibilityTextRunInfo>& text_runs,
      const std::vector<chrome_pdf::AccessibilityCharInfo>& chars,
      const chrome_pdf::AccessibilityPageObjects& page_objects) override;

  void HandleAction(const chrome_pdf::AccessibilityActionData& action_data);
  absl::optional<AnnotationInfo> GetPdfAnnotationInfoFromAXNode(
      int32_t ax_node_id) const;

  // Given the AXNode and the character offset within the AXNode, finds the
  // respective page index and character index within the page. Returns
  // false if the `node` is not a valid static text or inline text box
  // AXNode. Used to find the character offsets of selection.
  bool FindCharacterOffset(
      const ui::AXNode& node,
      uint32_t char_offset_in_node,
      chrome_pdf::PageCharacterIndex& page_char_index) const;

  // content::PluginAXTreeSource:
  bool GetTreeData(ui::AXTreeData* tree_data) const override;
  ui::AXNode* GetRoot() const override;
  ui::AXNode* GetFromId(int32_t id) const override;
  int32_t GetId(const ui::AXNode* node) const override;
  void GetChildren(const ui::AXNode* node,
                   std::vector<const ui::AXNode*>* out_children) const override;
  ui::AXNode* GetParent(const ui::AXNode* node) const override;
  bool IsIgnored(const ui::AXNode* node) const override;
  bool IsValid(const ui::AXNode* node) const override;
  bool IsEqual(const ui::AXNode* node1, const ui::AXNode* node2) const override;
  const ui::AXNode* GetNull() const override;
  void SerializeNode(const ui::AXNode* node, ui::AXNodeData* out_data)
      const override;
  std::unique_ptr<ui::AXActionTarget> CreateActionTarget(
      const ui::AXNode& target_node) override;

  // content::RenderFrameObserver:
  void AccessibilityModeChanged(const ui::AXMode& /*mode*/) override;
  void OnDestruct() override;

  bool ShowContextMenu();

 private:
  // Update the AXTreeData when the selected range changed.
  void UpdateAXTreeDataFromSelection();

  // Given a 0-based page index and 0-based character index within a page,
  // find the node ID of the associated static text AXNode, and the character
  // index within that text node. Used to find the start and end of the
  // selected text range.
  void FindNodeOffset(uint32_t page_index,
                      uint32_t page_char_index,
                      int32_t* out_node_id,
                      int32_t* out_node_char_index) const;

  // Called after the data for all pages in the PDF have been received.
  // Finishes assembling a complete accessibility tree and grafts it
  // onto the host tree.
  void Finish();

  void AddPageContent(
      ui::AXNodeData* page_node,
      const gfx::RectF& page_bounds,
      uint32_t page_index,
      const std::vector<chrome_pdf::AccessibilityTextRunInfo>& text_runs,
      const std::vector<chrome_pdf::AccessibilityCharInfo>& chars,
      const chrome_pdf::AccessibilityPageObjects& page_objects);

  // Clears the local cache of node data used to create the tree so that
  // replacement node data can be introduced.
  void ClearAccessibilityNodes();

  content::RenderAccessibility* GetRenderAccessibility();

  // WARNING: May cause `this` to be deleted.
  content::RenderAccessibility* GetRenderAccessibilityIfEnabled();

  std::unique_ptr<gfx::Transform> MakeTransformFromViewInfo() const;

  // Handles an accessibility change only if there is a valid
  // `RenderAccessibility` for the frame.
  void MaybeHandleAccessibilityChange();

  ui::AXTreeData tree_data_;
  ui::AXTree tree_;

  // Unowned. Must outlive `this`.
  chrome_pdf::PdfAccessibilityActionHandler* const action_handler_;

  // `zoom_` signifies the zoom level set in for the browser content.
  // `scale_` signifies the scale level set by user. Scale is applied
  // by the OS while zoom is applied by the application. Higher scale
  // values are usually set to increase the size of everything on screen.
  // Preferred by people with blurry/low vision. `zoom_` and `scale_`
  // both help us increase/descrease the size of content on screen.
  // From PDF plugin we receive all the data in logical pixels. Which is
  // without the zoom and scale factor applied. We apply the `zoom_` and
  // `scale_` to generate the final bounding boxes of elements in accessibility
  // tree.
  double zoom_ = 1.0;
  double scale_ = 1.0;
  gfx::Vector2dF scroll_;
  gfx::Vector2dF offset_;
  uint32_t selection_start_page_index_ = 0;
  uint32_t selection_start_char_index_ = 0;
  uint32_t selection_end_page_index_ = 0;
  uint32_t selection_end_char_index_ = 0;
  uint32_t page_count_ = 0;
  ui::AXNodeData* doc_node_;
  std::vector<std::unique_ptr<ui::AXNodeData>> nodes_;

  // Map from the id of each static text AXNode and inline text box
  // AXNode to the page index and index of the character within its
  // page. Used to find the node associated with the start or end of
  // a selection and vice-versa.
  std::map<int32_t, chrome_pdf::PageCharacterIndex> node_id_to_page_char_index_;

  // Map between AXNode id to annotation object. Used to find the annotation
  // object to which an action can be passed.
  std::map<int32_t, AnnotationInfo> node_id_to_annotation_info_;
  bool invalid_plugin_message_received_ = false;

  // Index of the next expected PDF accessibility page info, used to ignore
  // outdated calls of SetAccessibilityPageInfo().
  uint32_t next_page_index_ = 0;

  bool did_get_a_text_run_ = false;

  base::WeakPtrFactory<PdfAccessibilityTree> weak_ptr_factory_{this};
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TREE_H_
