// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_accessibility_tree.h"

#include <algorithm>
#include <utility>

#include "base/i18n/break_iterator.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "components/pdf/renderer/pdf_ax_action_target.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/renderer/pepper_plugin_instance.h"
#include "content/public/renderer/render_accessibility.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "pdf/pdf_features.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/null_ax_action_target.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace pdf {

namespace {

// Don't try to apply font size thresholds to automatically identify headings
// if the median font size is not at least this many points.
const float kMinimumFontSize = 5.0f;

// Don't try to apply paragraph break thresholds to automatically identify
// paragraph breaks if the median line break is not at least this many points.
const float kMinimumLineSpacing = 5.0f;

// Ratio between the font size of one text run and the median on the page
// for that text run to be considered to be a heading instead of normal text.
const float kHeadingFontSizeRatio = 1.2f;

// Ratio between the line spacing between two lines and the median on the
// page for that line spacing to be considered a paragraph break.
const float kParagraphLineSpacingRatio = 1.2f;

gfx::RectF PpFloatRectToGfxRectF(const PP_FloatRect& r) {
  return gfx::RectF(r.point.x, r.point.y, r.size.width, r.size.height);
}

gfx::RectF PPRectToGfxRectF(const PP_Rect& r) {
  return gfx::RectF(r.point.x, r.point.y, r.size.width, r.size.height);
}

gfx::Vector2dF PpPointToVector2dF(const PP_Point& p) {
  return gfx::Vector2dF(p.x, p.y);
}

// This class is used as part of our heuristic to determine which text runs live
// on the same "line".  As we process runs, we keep a weighted average of the
// top and bottom coordinates of the line, and if a new run falls within that
// range (within a threshold) it is considered part of the line.
class LineHelper {
 public:
  explicit LineHelper(
      const std::vector<ppapi::PdfAccessibilityTextRunInfo>& text_runs)
      : text_runs_(text_runs) {
    StartNewLine(0);
  }

  void StartNewLine(size_t current_index) {
    DCHECK(current_index == 0 || current_index < text_runs_.size());
    start_index_ = current_index;
    accumulated_weight_top_ = 0.0f;
    accumulated_weight_bottom_ = 0.0f;
    accumulated_width_ = 0.0f;
  }

  void ProcessNextRun(size_t run_index) {
    DCHECK_LT(run_index, text_runs_.size());
    RemoveOldRunsUpTo(run_index);
    AddRun(text_runs_[run_index].bounds);
  }

  bool IsRunOnSameLine(size_t run_index) const {
    DCHECK_LT(run_index, text_runs_.size());

    // Calculate new top/bottom bounds for our line.
    if (accumulated_width_ == 0.0f)
      return false;

    float line_top = accumulated_weight_top_ / accumulated_width_;
    float line_bottom = accumulated_weight_bottom_ / accumulated_width_;

    // Look at the next run, and determine how much it overlaps the line.
    const auto& run_bounds = text_runs_[run_index].bounds;
    if (run_bounds.size.height == 0.0f)
      return false;

    float clamped_top = std::max(line_top, run_bounds.point.y);
    float clamped_bottom =
        std::min(line_bottom, run_bounds.point.y + run_bounds.size.height);
    if (clamped_bottom < clamped_top)
      return false;

    float coverage = (clamped_bottom - clamped_top) / (run_bounds.size.height);

    // See if it falls within the line (within our threshold).
    constexpr float kLineCoverageThreshold = 0.25f;
    return coverage > kLineCoverageThreshold;
  }

 private:
  void AddRun(const PP_FloatRect& run_bounds) {
    float run_width = fabsf(run_bounds.size.width);
    accumulated_width_ += run_width;
    accumulated_weight_top_ += run_bounds.point.y * run_width;
    accumulated_weight_bottom_ +=
        (run_bounds.point.y + run_bounds.size.height) * run_width;
  }

  void RemoveRun(const PP_FloatRect& run_bounds) {
    float run_width = fabsf(run_bounds.size.width);
    accumulated_width_ -= run_width;
    accumulated_weight_top_ -= run_bounds.point.y * run_width;
    accumulated_weight_bottom_ -=
        (run_bounds.point.y + run_bounds.size.height) * run_width;
  }

  void RemoveOldRunsUpTo(size_t stop_index) {
    // Remove older runs from the weighted average if we've exceeded the
    // threshold distance from them. We remove them to prevent e.g. drop-caps
    // from unduly influencing future lines.
    constexpr float kBoxRemoveWidthThreshold = 3.0f;
    while (start_index_ < stop_index &&
           accumulated_width_ > text_runs_[start_index_].bounds.size.width *
                                    kBoxRemoveWidthThreshold) {
      const auto& old_bounds = text_runs_[start_index_].bounds;
      RemoveRun(old_bounds);
      start_index_++;
    }
  }

  const std::vector<ppapi::PdfAccessibilityTextRunInfo>& text_runs_;
  size_t start_index_;
  float accumulated_weight_top_;
  float accumulated_weight_bottom_;
  float accumulated_width_;

  DISALLOW_COPY_AND_ASSIGN(LineHelper);
};

void ComputeParagraphAndHeadingThresholds(
    const std::vector<ppapi::PdfAccessibilityTextRunInfo>& text_runs,
    float* out_heading_font_size_threshold,
    float* out_paragraph_spacing_threshold) {
  // Scan over the font sizes and line spacing within this page and
  // set heuristic thresholds so that text larger than the median font
  // size can be marked as a heading, and spacing larger than the median
  // line spacing can be a paragraph break.
  std::vector<float> font_sizes;
  std::vector<float> line_spacings;
  for (size_t i = 0; i < text_runs.size(); ++i) {
    font_sizes.push_back(text_runs[i].style.font_size);
    if (i > 0) {
      const auto& cur = text_runs[i].bounds;
      const auto& prev = text_runs[i - 1].bounds;
      if (cur.point.y > prev.point.y + prev.size.height / 2)
        line_spacings.push_back(cur.point.y - prev.point.y);
    }
  }
  if (font_sizes.size() > 2) {
    std::sort(font_sizes.begin(), font_sizes.end());
    float median_font_size = font_sizes[font_sizes.size() / 2];
    if (median_font_size > kMinimumFontSize) {
      *out_heading_font_size_threshold =
          median_font_size * kHeadingFontSizeRatio;
    }
  }
  if (line_spacings.size() > 4) {
    std::sort(line_spacings.begin(), line_spacings.end());
    float median_line_spacing = line_spacings[line_spacings.size() / 2];
    if (median_line_spacing > kMinimumLineSpacing) {
      *out_paragraph_spacing_threshold =
          median_line_spacing * kParagraphLineSpacingRatio;
    }
  }
}

void FinishStaticNode(ui::AXNodeData** static_text_node,
                      std::string* static_text) {
  // If we're in the middle of building a static text node, finish it before
  // moving on to the next object.
  if (*static_text_node) {
    (*static_text_node)
        ->AddStringAttribute(ax::mojom::StringAttribute::kName, (*static_text));
    static_text->clear();
  }
  *static_text_node = nullptr;
}

void ConnectPreviousAndNextOnLine(ui::AXNodeData* previous_on_line_node,
                                  ui::AXNodeData* next_on_line_node) {
  previous_on_line_node->AddIntAttribute(ax::mojom::IntAttribute::kNextOnLineId,
                                         next_on_line_node->id);
  next_on_line_node->AddIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId,
                                     previous_on_line_node->id);
}

bool BreakParagraph(
    const std::vector<ppapi::PdfAccessibilityTextRunInfo>& text_runs,
    uint32_t text_run_index,
    float paragraph_spacing_threshold) {
  // Check to see if its also a new paragraph, i.e., if the distance between
  // lines is greater than the threshold.  If there's no threshold, that
  // means there weren't enough lines to compute an accurate median, so
  // we compare against the line size instead.
  float line_spacing = fabsf(text_runs[text_run_index + 1].bounds.point.y -
                             text_runs[text_run_index].bounds.point.y);
  return ((paragraph_spacing_threshold > 0 &&
           line_spacing > paragraph_spacing_threshold) ||
          (paragraph_spacing_threshold == 0 &&
           line_spacing > kParagraphLineSpacingRatio *
                              text_runs[text_run_index].bounds.size.height));
}

ui::AXNode* GetStaticTextNodeFromNode(ui::AXNode* node) {
  // Returns the appropriate static text node given |node|'s type.
  // Returns nullptr if there is no appropriate static text node.
  ui::AXNode* static_node = node;
  // Get the static text from the link node.
  if (node &&
      (node->data().role == ax::mojom::Role::kLink ||
       node->data().role == ax::mojom::Role::kPdfActionableHighlight) &&
      node->children().size() == 1) {
    static_node = node->children()[0];
  }
  // If it's static text node, then it holds text.
  if (static_node && static_node->data().role == ax::mojom::Role::kStaticText)
    return static_node;
  return nullptr;
}

std::string GetTextRunCharsAsUTF8(
    const ppapi::PdfAccessibilityTextRunInfo& text_run,
    const std::vector<PP_PrivateAccessibilityCharInfo>& chars,
    int char_index) {
  std::string chars_utf8;
  for (uint32_t i = 0; i < text_run.len; ++i) {
    base::WriteUnicodeCharacter(chars[char_index + i].unicode_character,
                                &chars_utf8);
  }
  return chars_utf8;
}

std::vector<int32_t> GetTextRunCharOffsets(
    const ppapi::PdfAccessibilityTextRunInfo& text_run,
    const std::vector<PP_PrivateAccessibilityCharInfo>& chars,
    int char_index) {
  std::vector<int32_t> char_offsets(text_run.len);
  double offset = 0.0;
  for (uint32_t i = 0; i < text_run.len; ++i) {
    offset += chars[char_index + i].char_width;
    char_offsets[i] = floor(offset);
  }
  return char_offsets;
}

template <typename T>
bool CompareTextRuns(const T& a, const T& b) {
  return a.text_run_index < b.text_run_index;
}

template <typename T>
bool IsObjectInTextRun(const std::vector<T>& objects,
                       uint32_t object_index,
                       size_t text_run_index) {
  return (object_index < objects.size() &&
          objects[object_index].text_run_index <= text_run_index);
}

size_t NormalizeTextRunIndex(uint32_t object_end_text_run_index,
                             size_t current_text_run_index) {
  return std::max<size_t>(
      object_end_text_run_index,
      current_text_run_index ? current_text_run_index - 1 : 0);
}

bool IsTextRenderModeFill(const PP_TextRenderingMode& mode) {
  switch (mode) {
    case PP_TEXTRENDERINGMODE_FILL:
    case PP_TEXTRENDERINGMODE_FILLSTROKE:
    case PP_TEXTRENDERINGMODE_FILLCLIP:
    case PP_TEXTRENDERINGMODE_FILLSTROKECLIP:
      return true;
    default:
      return false;
  }
}

bool IsTextRenderModeStroke(const PP_TextRenderingMode& mode) {
  switch (mode) {
    case PP_TEXTRENDERINGMODE_STROKE:
    case PP_TEXTRENDERINGMODE_FILLSTROKE:
    case PP_TEXTRENDERINGMODE_STROKECLIP:
    case PP_TEXTRENDERINGMODE_FILLSTROKECLIP:
      return true;
    default:
      return false;
  }
}

}  // namespace

PdfAccessibilityTree::PdfAccessibilityTree(content::RendererPpapiHost* host,
                                           PP_Instance instance)
    : host_(host), instance_(instance) {}

PdfAccessibilityTree::~PdfAccessibilityTree() {
  content::RenderAccessibility* render_accessibility = GetRenderAccessibility();
  if (render_accessibility)
    render_accessibility->SetPluginTreeSource(nullptr);
}

// static
bool PdfAccessibilityTree::IsDataFromPluginValid(
    const std::vector<ppapi::PdfAccessibilityTextRunInfo>& text_runs,
    const std::vector<PP_PrivateAccessibilityCharInfo>& chars,
    const ppapi::PdfAccessibilityPageObjects& page_objects) {
  base::CheckedNumeric<uint32_t> char_length = 0;
  for (const ppapi::PdfAccessibilityTextRunInfo& text_run : text_runs)
    char_length += text_run.len;

  if (!char_length.IsValid() || char_length.ValueOrDie() != chars.size())
    return false;

  const std::vector<ppapi::PdfAccessibilityLinkInfo>& links =
      page_objects.links;
  if (!std::is_sorted(links.begin(), links.end(),
                      CompareTextRuns<ppapi::PdfAccessibilityLinkInfo>)) {
    return false;
  }
  // Text run index of a |link| is out of bounds if it exceeds the size of
  // |text_runs|. The index denotes the position of the link relative to the
  // text runs. The index value equal to the size of |text_runs| indicates that
  // the link should be after the last text run.
  for (const ppapi::PdfAccessibilityLinkInfo& link : links) {
    base::CheckedNumeric<uint32_t> index = link.text_run_index;
    index += link.text_run_count;
    if (!index.IsValid() || index.ValueOrDie() > text_runs.size())
      return false;
  }

  const std::vector<ppapi::PdfAccessibilityImageInfo>& images =
      page_objects.images;
  if (!std::is_sorted(images.begin(), images.end(),
                      CompareTextRuns<ppapi::PdfAccessibilityImageInfo>)) {
    return false;
  }
  // Text run index of an |image| works on the same logic as the text run index
  // of a |link| as mentioned above.
  for (const ppapi::PdfAccessibilityImageInfo& image : images) {
    if (image.text_run_index > text_runs.size())
      return false;
  }

  const std::vector<ppapi::PdfAccessibilityHighlightInfo>& highlights =
      page_objects.highlights;
  if (!std::is_sorted(highlights.begin(), highlights.end(),
                      CompareTextRuns<ppapi::PdfAccessibilityHighlightInfo>)) {
    return false;
  }

  // Since highlights also span across text runs similar to links, the
  // validation method is the same.
  for (const auto& highlight : highlights) {
    base::CheckedNumeric<uint32_t> index = highlight.text_run_index;
    index += highlight.text_run_count;
    if (!index.IsValid() || index.ValueOrDie() > text_runs.size())
      return false;
  }

  return true;
}

void PdfAccessibilityTree::SetAccessibilityViewportInfo(
    const PP_PrivateAccessibilityViewportInfo& viewport_info) {
  zoom_ = viewport_info.zoom;
  scale_ = viewport_info.scale;
  CHECK_GT(zoom_, 0);
  CHECK_GT(scale_, 0);
  scroll_ = PpPointToVector2dF(viewport_info.scroll);
  offset_ = PpPointToVector2dF(viewport_info.offset);

  selection_start_page_index_ = viewport_info.selection_start_page_index;
  selection_start_char_index_ = viewport_info.selection_start_char_index;
  selection_end_page_index_ = viewport_info.selection_end_page_index;
  selection_end_char_index_ = viewport_info.selection_end_char_index;

  content::RenderAccessibility* render_accessibility = GetRenderAccessibility();
  if (render_accessibility && tree_.size() > 1) {
    ui::AXNode* root = tree_.root();
    ui::AXNodeData root_data = root->data();
    root_data.relative_bounds.transform = MakeTransformFromViewInfo();
    root->SetData(root_data);
    UpdateAXTreeDataFromSelection();
    render_accessibility->OnPluginRootNodeUpdated();
  }
}

void PdfAccessibilityTree::SetAccessibilityDocInfo(
    const PP_PrivateAccessibilityDocInfo& doc_info) {
  if (!GetRenderAccessibility())
    return;

  doc_info_ = doc_info;
  doc_node_ =
      CreateNode(ax::mojom::Role::kDocument, ax::mojom::Restriction::kReadOnly);
  doc_node_->AddStringAttribute(
      ax::mojom::StringAttribute::kName,
      l10n_util::GetPluralStringFUTF8(IDS_PDF_DOCUMENT_PAGE_COUNT,
                                      doc_info.page_count));

  // Because all of the coordinates are expressed relative to the
  // doc's coordinates, the origin of the doc must be (0, 0). Its
  // width and height will be updated as we add each page so that the
  // doc's bounding box surrounds all pages.
  doc_node_->relative_bounds.bounds = gfx::RectF(0, 0, 1, 1);
}

void PdfAccessibilityTree::SetAccessibilityPageInfo(
    const PP_PrivateAccessibilityPageInfo& page_info,
    const std::vector<ppapi::PdfAccessibilityTextRunInfo>& text_runs,
    const std::vector<PP_PrivateAccessibilityCharInfo>& chars,
    const ppapi::PdfAccessibilityPageObjects& page_objects) {
  content::RenderAccessibility* render_accessibility = GetRenderAccessibility();
  if (!render_accessibility)
    return;

  // If unsanitized data is found, don't trust the PPAPI process sending it and
  // stop creation of the accessibility tree.
  if (!invalid_plugin_message_received_) {
    invalid_plugin_message_received_ =
        !IsDataFromPluginValid(text_runs, chars, page_objects);
  }
  if (invalid_plugin_message_received_)
    return;

  uint32_t page_index = page_info.page_index;
  CHECK_GE(page_index, 0U);
  CHECK_LT(page_index, doc_info_.page_count);

  ui::AXNodeData* page_node =
      CreateNode(ax::mojom::Role::kRegion, ax::mojom::Restriction::kReadOnly);
  page_node->AddStringAttribute(
      ax::mojom::StringAttribute::kName,
      l10n_util::GetPluralStringFUTF8(IDS_PDF_PAGE_INDEX, page_index + 1));
  page_node->AddBoolAttribute(ax::mojom::BoolAttribute::kIsPageBreakingObject,
                              true);

  gfx::RectF page_bounds = PPRectToGfxRectF(page_info.bounds);
  page_node->relative_bounds.bounds = page_bounds;
  doc_node_->relative_bounds.bounds.Union(page_node->relative_bounds.bounds);
  doc_node_->child_ids.push_back(page_node->id);

  AddPageContent(page_node, page_bounds, page_index, text_runs, chars,
                 page_objects);

  if (page_index == doc_info_.page_count - 1)
    Finish();
}

void PdfAccessibilityTree::AddPageContent(
    ui::AXNodeData* page_node,
    const gfx::RectF& page_bounds,
    uint32_t page_index,
    const std::vector<ppapi::PdfAccessibilityTextRunInfo>& text_runs,
    const std::vector<PP_PrivateAccessibilityCharInfo>& chars,
    const ppapi::PdfAccessibilityPageObjects& page_objects) {
  DCHECK(page_node);
  float heading_font_size_threshold = 0;
  float paragraph_spacing_threshold = 0;
  ComputeParagraphAndHeadingThresholds(text_runs, &heading_font_size_threshold,
                                       &paragraph_spacing_threshold);

  std::vector<uint32_t> text_run_start_indices;
  if (!text_runs.empty()) {
    text_run_start_indices.reserve(text_runs.size());
    text_run_start_indices.push_back(0);
    for (size_t i = 0; i < text_runs.size() - 1; ++i) {
      text_run_start_indices.push_back(text_run_start_indices[i] +
                                       text_runs[i].len);
    }
  }
  ui::AXNodeData* para_node = nullptr;
  ui::AXNodeData* static_text_node = nullptr;
  ui::AXNodeData* previous_on_line_node = nullptr;
  std::string static_text;
  uint32_t current_link_index = 0;
  uint32_t current_image_index = 0;
  uint32_t current_highlight_index = 0;
  uint32_t current_text_field_index = 0;
  LineHelper line_helper(text_runs);
  const std::vector<ppapi::PdfAccessibilityLinkInfo>& links =
      page_objects.links;
  const std::vector<ppapi::PdfAccessibilityImageInfo>& images =
      page_objects.images;
  const std::vector<ppapi::PdfAccessibilityHighlightInfo>& highlights =
      page_objects.highlights;
  const std::vector<ppapi::PdfAccessibilityTextFieldInfo>& text_fields =
      page_objects.text_fields;

  for (size_t text_run_index = 0; text_run_index < text_runs.size();
       ++text_run_index) {
    // If we don't have a paragraph, create one.
    if (!para_node) {
      para_node = CreateParagraphNode(text_runs[text_run_index].style.font_size,
                                      heading_font_size_threshold);
      page_node->child_ids.push_back(para_node->id);
    }

    // If the |text_run_index| is less than or equal to the link's
    // text_run_index, then push the link node in the paragraph.
    if (IsObjectInTextRun(links, current_link_index, text_run_index)) {
      FinishStaticNode(&static_text_node, &static_text);
      const ppapi::PdfAccessibilityLinkInfo& link = links[current_link_index++];
      ui::AXNodeData* link_node = CreateLinkNode(link, page_index);
      para_node->child_ids.push_back(link_node->id);

      // If |link.text_run_count| == 0, then the link is not part of the page
      // text. Push it ahead of the current text run.
      if (link.text_run_count == 0) {
        --text_run_index;
        continue;
      }

      // Annotations can overlap in PDF. There can be two overlapping scenarios:
      // Partial overlap and Complete overlap.
      // Partial overlap
      //
      // Link A starts      Link B starts     Link A ends            Link B ends
      //      |a1                |b1               |a2                    |b2
      // -----------------------------------------------------------------------
      //                                    Text
      //
      // Complete overlap
      // Link A starts      Link B starts     Link B ends            Link A ends
      //      |a1                |b1               |b2                    |a2
      // -----------------------------------------------------------------------
      //                                    Text
      //
      // For overlapping annotations, both annotations would store the full
      // text data and nothing will get truncated. For partial overlap, link `A`
      // would contain text between a1 and a2 while link `B` would contain text
      // between b1 and b2. For complete overlap as well, link `A` would contain
      // text between a1 and a2 and link `B` would contain text between b1 and
      // b2. The links would appear in the tree in the order of which they are
      // present. In the tree for both overlapping scenarios, link `A` would
      // appear first in the tree and link `B` after it.

      // If |link.text_run_count| > 0, then the link is part of the page text.
      // Make the text runs contained by the link children of the link node.
      size_t end_text_run_index = link.text_run_index + link.text_run_count;
      uint32_t link_end_text_run_index =
          std::min(end_text_run_index, text_runs.size()) - 1;
      AddTextToAXNode(link.text_run_index, link_end_text_run_index, text_runs,
                      chars, page_bounds, page_index, text_run_start_indices,
                      link_node, &previous_on_line_node);

      para_node->relative_bounds.bounds.Union(
          link_node->relative_bounds.bounds);

      text_run_index =
          NormalizeTextRunIndex(link_end_text_run_index, text_run_index);
    } else if (IsObjectInTextRun(images, current_image_index, text_run_index)) {
      FinishStaticNode(&static_text_node, &static_text);
      // If the |text_run_index| is less than or equal to the image's text run
      // index, then push the image ahead of the current text run.
      ui::AXNodeData* image_node = CreateImageNode(images[current_image_index]);
      para_node->child_ids.push_back(image_node->id);
      ++current_image_index;
      --text_run_index;
      continue;
    } else if (IsObjectInTextRun(highlights, current_highlight_index,
                                 text_run_index) &&
               base::FeatureList::IsEnabled(
                   chrome_pdf::features::kAccessiblePDFHighlight)) {
      FinishStaticNode(&static_text_node, &static_text);

      const ppapi::PdfAccessibilityHighlightInfo& highlight =
          highlights[current_highlight_index++];

      ui::AXNodeData* highlight_node = CreateHighlightNode(highlight);
      para_node->child_ids.push_back(highlight_node->id);

      // Make the text runs contained by the highlight children of
      // the highlight node.
      size_t end_text_run_index =
          highlight.text_run_index + highlight.text_run_count;
      uint32_t highlight_end_text_run_index =
          std::min(end_text_run_index, text_runs.size()) - 1;
      AddTextToAXNode(highlight.text_run_index, highlight_end_text_run_index,
                      text_runs, chars, page_bounds, page_index,
                      text_run_start_indices, highlight_node,
                      &previous_on_line_node);

      para_node->relative_bounds.bounds.Union(
          highlight_node->relative_bounds.bounds);

      text_run_index =
          NormalizeTextRunIndex(highlight_end_text_run_index, text_run_index);
    } else if (IsObjectInTextRun(text_fields, current_text_field_index,
                                 text_run_index) &&
               base::FeatureList::IsEnabled(
                   chrome_pdf::features::kAccessiblePDFForm)) {
      FinishStaticNode(&static_text_node, &static_text);
      // If the |text_run_index| is less than or equal to the text_field's text
      // run index, then push the text_field ahead of the current text run.
      ui::AXNodeData* text_field_node =
          CreateTextFieldNode(text_fields[current_text_field_index]);
      para_node->child_ids.push_back(text_field_node->id);
      ++current_text_field_index;
      --text_run_index;
      continue;
    } else {
      PP_PdfPageCharacterIndex page_char_index = {
          page_index, text_run_start_indices[text_run_index]};

      // This node is for the text inside the paragraph, it includes
      // the text of all of the text runs.
      if (!static_text_node) {
        static_text_node = CreateStaticTextNode(page_char_index);
        para_node->child_ids.push_back(static_text_node->id);
      }

      const ppapi::PdfAccessibilityTextRunInfo& text_run =
          text_runs[text_run_index];
      // Add this text run to the current static text node.
      ui::AXNodeData* inline_text_box_node = CreateInlineTextBoxNode(
          text_run, chars, page_char_index, page_bounds);
      static_text_node->child_ids.push_back(inline_text_box_node->id);

      static_text += inline_text_box_node->GetStringAttribute(
          ax::mojom::StringAttribute::kName);

      para_node->relative_bounds.bounds.Union(
          inline_text_box_node->relative_bounds.bounds);
      static_text_node->relative_bounds.bounds.Union(
          inline_text_box_node->relative_bounds.bounds);

      if (previous_on_line_node) {
        ConnectPreviousAndNextOnLine(previous_on_line_node,
                                     inline_text_box_node);
      } else {
        line_helper.StartNewLine(text_run_index);
      }
      line_helper.ProcessNextRun(text_run_index);

      if (text_run_index < text_runs.size() - 1) {
        if (line_helper.IsRunOnSameLine(text_run_index + 1)) {
          // The next run is on the same line.
          previous_on_line_node = inline_text_box_node;
        } else {
          // The next run is on a new line.
          previous_on_line_node = nullptr;
        }
      }
    }

    if (text_run_index == text_runs.size() - 1) {
      FinishStaticNode(&static_text_node, &static_text);
      break;
    }

    if (!previous_on_line_node) {
      if (BreakParagraph(text_runs, text_run_index,
                         paragraph_spacing_threshold)) {
        FinishStaticNode(&static_text_node, &static_text);
        para_node = nullptr;
      }
    }
  }

  base::span<const ppapi::PdfAccessibilityLinkInfo> remaining_links =
      base::make_span(links).subspan(current_link_index);
  base::span<const ppapi::PdfAccessibilityImageInfo> remaining_images =
      base::make_span(images).subspan(current_image_index);
  base::span<const ppapi::PdfAccessibilityTextFieldInfo> remaining_text_fields =
      base::make_span(text_fields).subspan(current_text_field_index);
  AddRemainingAnnotations(page_node, page_bounds, page_index, remaining_links,
                          remaining_images, remaining_text_fields, para_node);
}

void PdfAccessibilityTree::AddRemainingAnnotations(
    ui::AXNodeData* page_node,
    const gfx::RectF& page_bounds,
    uint32_t page_index,
    base::span<const ppapi::PdfAccessibilityLinkInfo> links,
    base::span<const ppapi::PdfAccessibilityImageInfo> images,
    base::span<const ppapi::PdfAccessibilityTextFieldInfo> text_fields,
    ui::AXNodeData* para_node) {
  // If we have additional links, images or text fields to insert in the tree,
  // and we don't have a paragraph node, create a new one.
  if (!para_node &&
      (!links.empty() || !images.empty() || !text_fields.empty())) {
    para_node = CreateNode(ax::mojom::Role::kParagraph,
                           ax::mojom::Restriction::kReadOnly);
    page_node->child_ids.push_back(para_node->id);
  }
  // Push all the links not anchored to any text run to the last paragraph.
  for (const ppapi::PdfAccessibilityLinkInfo& link : links) {
    ui::AXNodeData* link_node = CreateLinkNode(link, page_index);
    para_node->child_ids.push_back(link_node->id);
  }
  // Push all the images not anchored to any text run to the last paragraph.
  for (const ppapi::PdfAccessibilityImageInfo& image : images) {
    ui::AXNodeData* image_node = CreateImageNode(image);
    para_node->child_ids.push_back(image_node->id);
  }

  if (base::FeatureList::IsEnabled(chrome_pdf::features::kAccessiblePDFForm)) {
    // Push all the text fields not anchored to any text run to the last
    // paragraph.
    for (const ppapi::PdfAccessibilityTextFieldInfo& text_field : text_fields) {
      ui::AXNodeData* text_field_node = CreateTextFieldNode(text_field);
      para_node->child_ids.push_back(text_field_node->id);
    }
  }
}

void PdfAccessibilityTree::Finish() {
  doc_node_->relative_bounds.transform = MakeTransformFromViewInfo();

  ui::AXTreeUpdate update;
  update.root_id = doc_node_->id;
  for (const auto& node : nodes_)
    update.nodes.push_back(*node);

  if (!tree_.Unserialize(update))
    LOG(FATAL) << tree_.error();

  UpdateAXTreeDataFromSelection();

  content::RenderAccessibility* render_accessibility = GetRenderAccessibility();
  if (render_accessibility)
    render_accessibility->SetPluginTreeSource(this);
}

void PdfAccessibilityTree::UpdateAXTreeDataFromSelection() {
  tree_data_.sel_is_backward = false;
  if (selection_start_page_index_ > selection_end_page_index_) {
    tree_data_.sel_is_backward = true;
  } else if (selection_start_page_index_ == selection_end_page_index_ &&
             selection_start_char_index_ > selection_end_char_index_) {
    tree_data_.sel_is_backward = true;
  }

  FindNodeOffset(selection_start_page_index_, selection_start_char_index_,
                 &tree_data_.sel_anchor_object_id,
                 &tree_data_.sel_anchor_offset);
  FindNodeOffset(selection_end_page_index_, selection_end_char_index_,
                 &tree_data_.sel_focus_object_id, &tree_data_.sel_focus_offset);
}

void PdfAccessibilityTree::FindNodeOffset(uint32_t page_index,
                                          uint32_t page_char_index,
                                          int32_t* out_node_id,
                                          int32_t* out_node_char_index) const {
  *out_node_id = -1;
  *out_node_char_index = 0;
  ui::AXNode* root = tree_.root();
  if (page_index >= root->children().size())
    return;
  ui::AXNode* page = root->children()[page_index];

  // Iterate over all paragraphs within this given page, and static text nodes
  // within each paragraph.
  for (ui::AXNode* para : page->children()) {
    for (ui::AXNode* child_node : para->children()) {
      ui::AXNode* static_text = GetStaticTextNodeFromNode(child_node);
      if (!static_text)
        continue;
      // Look up the page-relative character index for static nodes from a map
      // we built while the document was initially built.
      auto iter = node_id_to_page_char_index_.find(static_text->id());
      uint32_t char_index = iter->second.char_index;
      uint32_t len = static_text->data()
                         .GetStringAttribute(ax::mojom::StringAttribute::kName)
                         .size();

      // If the character index we're looking for falls within the range
      // of this node, return the node ID and index within this node's text.
      if (page_char_index <= char_index + len) {
        *out_node_id = static_text->id();
        *out_node_char_index = page_char_index - char_index;
        return;
      }
    }
  }
}

bool PdfAccessibilityTree::FindCharacterOffset(
    const ui::AXNode& node,
    uint32_t char_offset_in_node,
    PP_PdfPageCharacterIndex* page_char_index) const {
  auto iter = node_id_to_page_char_index_.find(GetId(&node));
  if (iter == node_id_to_page_char_index_.end())
    return false;
  page_char_index->char_index = iter->second.char_index + char_offset_in_node;
  page_char_index->page_index = iter->second.page_index;
  return true;
}

ui::AXNodeData* PdfAccessibilityTree::CreateNode(
    ax::mojom::Role role,
    ax::mojom::Restriction restriction) {
  content::RenderAccessibility* render_accessibility = GetRenderAccessibility();
  DCHECK(render_accessibility);

  auto node = std::make_unique<ui::AXNodeData>();
  node->id = render_accessibility->GenerateAXID();
  node->role = role;
  node->SetRestriction(restriction);

  // All nodes other than the first one have coordinates relative to
  // the first node.
  if (!nodes_.empty())
    node->relative_bounds.offset_container_id = nodes_[0]->id;

  ui::AXNodeData* node_ptr = node.get();
  nodes_.push_back(std::move(node));

  return node_ptr;
}

ui::AXNodeData* PdfAccessibilityTree::CreateParagraphNode(
    float font_size,
    float heading_font_size_threshold) {
  ui::AXNodeData* para_node = CreateNode(ax::mojom::Role::kParagraph,
                                         ax::mojom::Restriction::kReadOnly);
  para_node->AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                              true);

  // If font size exceeds the |heading_font_size_threshold|, then classify
  // it as a Heading.
  if (heading_font_size_threshold > 0 &&
      font_size > heading_font_size_threshold) {
    para_node->role = ax::mojom::Role::kHeading;
    para_node->AddIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel, 2);
    para_node->AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "h2");
  }

  return para_node;
}

ui::AXNodeData* PdfAccessibilityTree::CreateStaticTextNode(
    const PP_PdfPageCharacterIndex& page_char_index) {
  ui::AXNodeData* static_text_node = CreateNode(
      ax::mojom::Role::kStaticText, ax::mojom::Restriction::kReadOnly);
  node_id_to_page_char_index_.emplace(static_text_node->id, page_char_index);
  return static_text_node;
}

ui::AXNodeData* PdfAccessibilityTree::CreateInlineTextBoxNode(
    const ppapi::PdfAccessibilityTextRunInfo& text_run,
    const std::vector<PP_PrivateAccessibilityCharInfo>& chars,
    const PP_PdfPageCharacterIndex& page_char_index,
    const gfx::RectF& page_bounds) {
  ui::AXNodeData* inline_text_box_node = CreateNode(
      ax::mojom::Role::kInlineTextBox, ax::mojom::Restriction::kReadOnly);

  std::string chars_utf8 =
      GetTextRunCharsAsUTF8(text_run, chars, page_char_index.char_index);
  inline_text_box_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                           chars_utf8);
  inline_text_box_node->AddIntAttribute(ax::mojom::IntAttribute::kTextDirection,
                                        text_run.direction);
  inline_text_box_node->AddStringAttribute(
      ax::mojom::StringAttribute::kFontFamily, text_run.style.font_name);
  inline_text_box_node->AddFloatAttribute(ax::mojom::FloatAttribute::kFontSize,
                                          text_run.style.font_size);
  inline_text_box_node->AddFloatAttribute(
      ax::mojom::FloatAttribute::kFontWeight, text_run.style.font_weight);
  if (text_run.style.is_italic)
    inline_text_box_node->AddTextStyle(ax::mojom::TextStyle::kItalic);
  if (text_run.style.is_bold)
    inline_text_box_node->AddTextStyle(ax::mojom::TextStyle::kBold);
  if (IsTextRenderModeFill(text_run.style.render_mode)) {
    inline_text_box_node->AddIntAttribute(ax::mojom::IntAttribute::kColor,
                                          text_run.style.fill_color);
  } else if (IsTextRenderModeStroke(text_run.style.render_mode)) {
    inline_text_box_node->AddIntAttribute(ax::mojom::IntAttribute::kColor,
                                          text_run.style.stroke_color);
  }

  inline_text_box_node->relative_bounds.bounds =
      PpFloatRectToGfxRectF(text_run.bounds) + page_bounds.OffsetFromOrigin();
  std::vector<int32_t> char_offsets =
      GetTextRunCharOffsets(text_run, chars, page_char_index.char_index);
  inline_text_box_node->AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, char_offsets);
  AddWordStartsAndEnds(inline_text_box_node);
  node_id_to_page_char_index_.emplace(inline_text_box_node->id,
                                      page_char_index);
  return inline_text_box_node;
}

ui::AXNodeData* PdfAccessibilityTree::CreateLinkNode(
    const ppapi::PdfAccessibilityLinkInfo& link,
    uint32_t page_index) {
  ui::AXNodeData* link_node =
      CreateNode(ax::mojom::Role::kLink, ax::mojom::Restriction::kReadOnly);

  link_node->AddStringAttribute(ax::mojom::StringAttribute::kUrl, link.url);
  link_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                std::string());
  link_node->relative_bounds.bounds = PpFloatRectToGfxRectF(link.bounds);
  node_id_to_annotation_info_.emplace(
      link_node->id, AnnotationInfo(page_index, link.index_in_page));

  return link_node;
}

ui::AXNodeData* PdfAccessibilityTree::CreateImageNode(
    const ppapi::PdfAccessibilityImageInfo& image) {
  ui::AXNodeData* image_node =
      CreateNode(ax::mojom::Role::kImage, ax::mojom::Restriction::kReadOnly);

  if (image.alt_text.empty()) {
    image_node->AddStringAttribute(
        ax::mojom::StringAttribute::kName,
        l10n_util::GetStringUTF8(IDS_AX_UNLABELED_IMAGE_ROLE_DESCRIPTION));
  } else {
    image_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                   image.alt_text);
  }
  image_node->relative_bounds.bounds = PpFloatRectToGfxRectF(image.bounds);
  return image_node;
}

ui::AXNodeData* PdfAccessibilityTree::CreateHighlightNode(
    const ppapi::PdfAccessibilityHighlightInfo& highlight) {
  ui::AXNodeData* highlight_node =
      CreateNode(ax::mojom::Role::kPdfActionableHighlight,
                 ax::mojom::Restriction::kReadOnly);

  highlight_node->AddStringAttribute(
      ax::mojom::StringAttribute::kRoleDescription,
      l10n_util::GetStringUTF8(IDS_AX_ROLE_DESCRIPTION_PDF_HIGHLIGHT));
  highlight_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                     std::string());
  highlight_node->relative_bounds.bounds =
      PpFloatRectToGfxRectF(highlight.bounds);
  highlight_node->AddIntAttribute(ax::mojom::IntAttribute::kBackgroundColor,
                                  highlight.color);

  return highlight_node;
}

ui::AXNodeData* PdfAccessibilityTree::CreateTextFieldNode(
    const ppapi::PdfAccessibilityTextFieldInfo& text_field) {
  ax::mojom::Restriction restriction = text_field.is_read_only
                                           ? ax::mojom::Restriction::kReadOnly
                                           : ax::mojom::Restriction::kNone;
  ui::AXNodeData* text_field_node =
      CreateNode(ax::mojom::Role::kTextField, restriction);

  text_field_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                      text_field.name);
  text_field_node->AddStringAttribute(ax::mojom::StringAttribute::kValue,
                                      text_field.value);
  text_field_node->AddState(ax::mojom::State::kFocusable);
  if (text_field.is_required)
    text_field_node->AddState(ax::mojom::State::kRequired);
  if (text_field.is_password)
    text_field_node->AddState(ax::mojom::State::kProtected);
  text_field_node->relative_bounds.bounds =
      PpFloatRectToGfxRectF(text_field.bounds);
  return text_field_node;
}

void PdfAccessibilityTree::AddTextToAXNode(
    uint32_t start_text_run_index,
    uint32_t end_text_run_index,
    const std::vector<ppapi::PdfAccessibilityTextRunInfo>& text_runs,
    const std::vector<PP_PrivateAccessibilityCharInfo>& chars,
    const gfx::RectF& page_bounds,
    uint32_t page_index,
    const std::vector<uint32_t>& text_run_start_indices,
    ui::AXNodeData* ax_node,
    ui::AXNodeData** previous_on_line_node) {
  PP_PdfPageCharacterIndex page_char_index = {
      page_index, text_run_start_indices[start_text_run_index]};
  ui::AXNodeData* ax_static_text_node = CreateStaticTextNode(page_char_index);
  ax_node->child_ids.push_back(ax_static_text_node->id);
  // Accumulate the text of the node.
  std::string ax_name;
  LineHelper line_helper(text_runs);

  for (size_t text_run_index = start_text_run_index;
       text_run_index <= end_text_run_index; ++text_run_index) {
    const ppapi::PdfAccessibilityTextRunInfo& text_run =
        text_runs[text_run_index];
    page_char_index.char_index = text_run_start_indices[text_run_index];
    // Add this text run to the current static text node.
    ui::AXNodeData* inline_text_box_node =
        CreateInlineTextBoxNode(text_run, chars, page_char_index, page_bounds);
    ax_static_text_node->child_ids.push_back(inline_text_box_node->id);

    ax_static_text_node->relative_bounds.bounds.Union(
        inline_text_box_node->relative_bounds.bounds);
    ax_name += inline_text_box_node->GetStringAttribute(
        ax::mojom::StringAttribute::kName);

    if (*previous_on_line_node) {
      ConnectPreviousAndNextOnLine(*previous_on_line_node,
                                   inline_text_box_node);
    } else {
      line_helper.StartNewLine(text_run_index);
    }
    line_helper.ProcessNextRun(text_run_index);

    if (text_run_index < text_runs.size() - 1) {
      if (line_helper.IsRunOnSameLine(text_run_index + 1)) {
        // The next run is on the same line.
        *previous_on_line_node = inline_text_box_node;
      } else {
        // The next run is on a new line.
        *previous_on_line_node = nullptr;
      }
    }
  }

  ax_node->AddStringAttribute(ax::mojom::StringAttribute::kName, ax_name);
  ax_static_text_node->AddStringAttribute(ax::mojom::StringAttribute::kName,
                                          ax_name);
}

content::RenderAccessibility* PdfAccessibilityTree::GetRenderAccessibility() {
  content::RenderFrame* render_frame =
      host_->GetRenderFrameForInstance(instance_);
  if (!render_frame)
    return nullptr;
  content::RenderAccessibility* render_accessibility =
      render_frame->GetRenderAccessibility();
  if (!render_accessibility)
    return nullptr;

  // If RenderAccessibility is unable to generate valid positive IDs,
  // we shouldn't use it. This can happen if Blink accessibility is disabled
  // after we started generating the accessible PDF.
  if (render_accessibility->GenerateAXID() <= 0)
    return nullptr;

  return render_accessibility;
}

std::unique_ptr<gfx::Transform>
PdfAccessibilityTree::MakeTransformFromViewInfo() const {
  double applicable_scale_factor =
      content::RenderThread::Get()->IsUseZoomForDSF() ? scale_ : 1;
  auto transform = std::make_unique<gfx::Transform>();
  // |scroll_| represents the x offset from which PDF content starts. It is the
  // width of the PDF toolbar in pixels. Size of PDF toolbar does not change
  // with zoom.
  transform->Scale(applicable_scale_factor, applicable_scale_factor);
  transform->Translate(-scroll_);
  transform->Scale(zoom_, zoom_);
  transform->Translate(offset_);
  return transform;
}

void PdfAccessibilityTree::AddWordStartsAndEnds(
    ui::AXNodeData* inline_text_box) {
  base::string16 text =
      inline_text_box->GetString16Attribute(ax::mojom::StringAttribute::kName);
  base::i18n::BreakIterator iter(text, base::i18n::BreakIterator::BREAK_WORD);
  if (!iter.Init())
    return;

  std::vector<int32_t> word_starts;
  std::vector<int32_t> word_ends;
  while (iter.Advance()) {
    if (iter.IsWord()) {
      word_starts.push_back(iter.prev());
      word_ends.push_back(iter.pos());
    }
  }
  inline_text_box->AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                       word_starts);
  inline_text_box->AddIntListAttribute(ax::mojom::IntListAttribute::kWordEnds,
                                       word_ends);
}

PdfAccessibilityTree::AnnotationInfo::AnnotationInfo(uint32_t page_index,
                                                     uint32_t annotation_index)
    : page_index(page_index), annotation_index(annotation_index) {}

PdfAccessibilityTree::AnnotationInfo::AnnotationInfo(
    const AnnotationInfo& other) = default;

PdfAccessibilityTree::AnnotationInfo::~AnnotationInfo() = default;

//
// AXTreeSource implementation.
//

bool PdfAccessibilityTree::GetTreeData(ui::AXTreeData* tree_data) const {
  tree_data->sel_is_backward = tree_data_.sel_is_backward;
  tree_data->sel_anchor_object_id = tree_data_.sel_anchor_object_id;
  tree_data->sel_anchor_offset = tree_data_.sel_anchor_offset;
  tree_data->sel_focus_object_id = tree_data_.sel_focus_object_id;
  tree_data->sel_focus_offset = tree_data_.sel_focus_offset;
  return true;
}

ui::AXNode* PdfAccessibilityTree::GetRoot() const {
  return tree_.root();
}

ui::AXNode* PdfAccessibilityTree::GetFromId(int32_t id) const {
  return tree_.GetFromId(id);
}

int32_t PdfAccessibilityTree::GetId(const ui::AXNode* node) const {
  return node->id();
}

void PdfAccessibilityTree::GetChildren(
    const ui::AXNode* node,
    std::vector<const ui::AXNode*>* out_children) const {
  *out_children = std::vector<const ui::AXNode*>(node->children().cbegin(),
                                                 node->children().cend());
}

ui::AXNode* PdfAccessibilityTree::GetParent(const ui::AXNode* node) const {
  return node->parent();
}

bool PdfAccessibilityTree::IsIgnored(const ui::AXNode* node) const {
  return node->IsIgnored();
}

bool PdfAccessibilityTree::IsValid(const ui::AXNode* node) const {
  return node != nullptr;
}

bool PdfAccessibilityTree::IsEqual(const ui::AXNode* node1,
                                   const ui::AXNode* node2) const {
  return node1 == node2;
}

const ui::AXNode* PdfAccessibilityTree::GetNull() const {
  return nullptr;
}

void PdfAccessibilityTree::SerializeNode(
    const ui::AXNode* node, ui::AXNodeData* out_data) const {
  *out_data = node->data();
}

std::unique_ptr<ui::AXActionTarget> PdfAccessibilityTree::CreateActionTarget(
    const ui::AXNode& target_node) {
  return std::make_unique<PdfAXActionTarget>(target_node, this);
}

void PdfAccessibilityTree::HandleAction(
    const PP_PdfAccessibilityActionData& action_data) {
  content::PepperPluginInstance* plugin_instance =
      host_->GetPluginInstance(instance_);
  if (plugin_instance) {
    plugin_instance->HandleAccessibilityAction(action_data);
  }
}

base::Optional<PdfAccessibilityTree::AnnotationInfo>
PdfAccessibilityTree::GetPdfAnnotationInfoFromAXNode(int32_t ax_node_id) const {
  auto iter = node_id_to_annotation_info_.find(ax_node_id);
  if (iter == node_id_to_annotation_info_.end())
    return base::nullopt;

  return AnnotationInfo(iter->second.page_index, iter->second.annotation_index);
}

}  // namespace pdf
