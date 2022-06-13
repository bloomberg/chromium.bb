// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_autofill_util.h"

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_label_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_option_element.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "third_party/blink/public/web/web_select_element.h"

using blink::WebAutofillState;
using blink::WebDocument;
using blink::WebElement;
using blink::WebElementCollection;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebInputElement;
using blink::WebLabelElement;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebOptionElement;
using blink::WebSelectElement;
using blink::WebString;
using blink::WebVector;

namespace autofill {

using mojom::ButtonTitleType;

namespace form_util {

struct ShadowFieldData {
  ShadowFieldData() = default;
  ShadowFieldData(ShadowFieldData&& other) = default;
  ShadowFieldData& operator=(ShadowFieldData&& other) = default;
  ShadowFieldData(const ShadowFieldData& other) = delete;
  ShadowFieldData& operator=(const ShadowFieldData& other) = delete;
  ~ShadowFieldData() = default;

  // If the form control is inside shadow DOM, then these lists will contain
  // id and name attributes of the parent shadow host elements. There may be
  // more than one if the form control is in nested shadow DOM.
  std::vector<std::u16string> shadow_host_id_attributes;
  std::vector<std::u16string> shadow_host_name_attributes;
};

namespace {

// Maximal length of a button's title.
const int kMaxLengthForSingleButtonTitle = 30;
// Maximal length of all button titles.
const int kMaxLengthForAllButtonTitles = 200;

// Number of shadow roots to traverse upwards when looking for relevant forms
// and labels of an input element inside a shadow root.
const int kMaxShadowLevelsUp = 2;

// Text features to detect form submission buttons. Features are selected based
// on analysis of real forms and their buttons.
// TODO(crbug.com/910546): Consider to add more features (e.g. non-English
// features).
const char* const kButtonFeatures[] = {"button", "btn", "submit",
                                       "boton" /* "button" in Spanish */};

// A bit field mask for FillForm functions to not fill some fields.
enum FieldFilterMask {
  FILTER_NONE = 0,
  FILTER_DISABLED_ELEMENTS = 1 << 0,
  FILTER_READONLY_ELEMENTS = 1 << 1,
  // Filters non-focusable elements with the exception of select elements, which
  // are sometimes made non-focusable because they are present for accessibility
  // while a prettier, non-<select> dropdown is shown. We still want to autofill
  // the non-focusable <select>.
  FILTER_NON_FOCUSABLE_ELEMENTS = 1 << 2,
  FILTER_ALL_NON_EDITABLE_ELEMENTS = FILTER_DISABLED_ELEMENTS |
                                     FILTER_READONLY_ELEMENTS |
                                     FILTER_NON_FOCUSABLE_ELEMENTS,
};

void TruncateString(std::u16string* str, size_t max_length) {
  if (str->length() > max_length)
    str->resize(max_length);
}

bool IsOptionElement(const WebElement& element) {
  static base::NoDestructor<WebString> kOption("option");
  return element.HasHTMLTagName(*kOption);
}

bool IsScriptElement(const WebElement& element) {
  static base::NoDestructor<WebString> kScript("script");
  return element.HasHTMLTagName(*kScript);
}

bool IsNoScriptElement(const WebElement& element) {
  static base::NoDestructor<WebString> kNoScript("noscript");
  return element.HasHTMLTagName(*kNoScript);
}

bool HasTagName(const WebNode& node, const blink::WebString& tag) {
  return node.IsElementNode() && node.ToConst<WebElement>().HasHTMLTagName(tag);
}

bool IsElementInControlElementSet(
    const WebElement& element,
    const std::vector<WebFormControlElement>& control_elements) {
  if (!element.IsFormControlElement())
    return false;
  const WebFormControlElement form_control_element =
      element.ToConst<WebFormControlElement>();
  return base::Contains(control_elements, form_control_element);
}

bool IsElementInsideFormOrFieldSet(const WebElement& element,
                                   bool consider_fieldset_tags) {
  for (WebNode parent_node = element.ParentNode(); !parent_node.IsNull();
       parent_node = parent_node.ParentNode()) {
    if (!parent_node.IsElementNode())
      continue;

    WebElement cur_element = parent_node.To<WebElement>();
    if (cur_element.HasHTMLTagName("form") ||
        (consider_fieldset_tags && cur_element.HasHTMLTagName("fieldset"))) {
      return true;
    }
  }
  return false;
}

// Returns true if |node| is an element and it is a container type that
// InferLabelForElement() can traverse.
bool IsTraversableContainerElement(const WebNode& node) {
  if (!node.IsElementNode())
    return false;

  const WebElement element = node.ToConst<WebElement>();
  return element.HasHTMLTagName("dd") || element.HasHTMLTagName("div") ||
         element.HasHTMLTagName("fieldset") || element.HasHTMLTagName("li") ||
         element.HasHTMLTagName("td") || element.HasHTMLTagName("table");
}

// Returns the colspan for a <td> / <th>. Defaults to 1.
size_t CalculateTableCellColumnSpan(const WebElement& element) {
  DCHECK(element.HasHTMLTagName("td") || element.HasHTMLTagName("th"));

  size_t span = 1;
  if (element.HasAttribute("colspan")) {
    std::u16string colspan = element.GetAttribute("colspan").Utf16();
    // Do not check return value to accept imperfect conversions.
    base::StringToSizeT(colspan, &span);
    // Handle overflow.
    if (span == std::numeric_limits<size_t>::max())
      span = 1;
    span = std::max(span, static_cast<size_t>(1));
  }

  return span;
}

// Appends |suffix| to |prefix| so that any intermediary whitespace is collapsed
// to a single space.  If |force_whitespace| is true, then the resulting string
// is guaranteed to have a space between |prefix| and |suffix|.  Otherwise, the
// result includes a space only if |prefix| has trailing whitespace or |suffix|
// has leading whitespace.
// A few examples:
//  * CombineAndCollapseWhitespace("foo", "bar", false)       -> "foobar"
//  * CombineAndCollapseWhitespace("foo", "bar", true)        -> "foo bar"
//  * CombineAndCollapseWhitespace("foo ", "bar", false)      -> "foo bar"
//  * CombineAndCollapseWhitespace("foo", " bar", false)      -> "foo bar"
//  * CombineAndCollapseWhitespace("foo", " bar", true)       -> "foo bar"
//  * CombineAndCollapseWhitespace("foo   ", "   bar", false) -> "foo bar"
//  * CombineAndCollapseWhitespace(" foo", "bar ", false)     -> " foobar "
//  * CombineAndCollapseWhitespace(" foo", "bar ", true)      -> " foo bar "
const std::u16string CombineAndCollapseWhitespace(const std::u16string& prefix,
                                                  const std::u16string& suffix,
                                                  bool force_whitespace) {
  std::u16string prefix_trimmed;
  base::TrimPositions prefix_trailing_whitespace =
      base::TrimWhitespace(prefix, base::TRIM_TRAILING, &prefix_trimmed);

  // Recursively compute the children's text.
  std::u16string suffix_trimmed;
  base::TrimPositions suffix_leading_whitespace =
      base::TrimWhitespace(suffix, base::TRIM_LEADING, &suffix_trimmed);

  if (prefix_trailing_whitespace || suffix_leading_whitespace ||
      force_whitespace) {
    return prefix_trimmed + u" " + suffix_trimmed;
  }
  return prefix_trimmed + suffix_trimmed;
}

// This is a helper function for the FindChildText() function (see below).
// Search depth is limited with the |depth| parameter.
// |divs_to_skip| is a list of <div> tags to ignore if encountered.
std::u16string FindChildTextInner(const WebNode& node,
                                  int depth,
                                  const std::set<WebNode>& divs_to_skip) {
  if (depth <= 0 || node.IsNull())
    return std::u16string();

  // Skip over comments.
  if (node.IsCommentNode())
    return FindChildTextInner(node.NextSibling(), depth - 1, divs_to_skip);

  if (!node.IsElementNode() && !node.IsTextNode())
    return std::u16string();

  // Ignore elements known not to contain inferable labels.
  if (node.IsElementNode()) {
    const WebElement element = node.ToConst<WebElement>();
    if (IsOptionElement(element) || IsScriptElement(element) ||
        IsNoScriptElement(element) ||
        (element.IsFormControlElement() &&
         IsAutofillableElement(element.ToConst<WebFormControlElement>()))) {
      return std::u16string();
    }

    if (element.HasHTMLTagName("div") && base::Contains(divs_to_skip, node))
      return std::u16string();
  }

  // Extract the text exactly at this node.
  std::u16string node_text = node.NodeValue().Utf16();

  // Recursively compute the children's text.
  // Preserve inter-element whitespace separation.
  std::u16string child_text =
      FindChildTextInner(node.FirstChild(), depth - 1, divs_to_skip);
  bool add_space = node.IsTextNode() && node_text.empty();
  node_text = CombineAndCollapseWhitespace(node_text, child_text, add_space);

  // Recursively compute the siblings' text.
  // Again, preserve inter-element whitespace separation.
  std::u16string sibling_text =
      FindChildTextInner(node.NextSibling(), depth - 1, divs_to_skip);
  add_space = node.IsTextNode() && node_text.empty();
  node_text = CombineAndCollapseWhitespace(node_text, sibling_text, add_space);

  return node_text;
}

// Same as FindChildText() below, but with a list of div nodes to skip.
std::u16string FindChildTextWithIgnoreList(
    const WebNode& node,
    const std::set<WebNode>& divs_to_skip) {
  if (node.IsTextNode())
    return node.NodeValue().Utf16();

  WebNode child = node.FirstChild();

  const int kChildSearchDepth = 10;
  std::u16string node_text =
      FindChildTextInner(child, kChildSearchDepth, divs_to_skip);
  base::TrimWhitespace(node_text, base::TRIM_ALL, &node_text);
  return node_text;
}

bool IsLabelValid(base::StringPiece16 inferred_label) {
  // List of characters a label can't be entirely made of (this list can grow).
  auto IsStopWord = [](char16_t c) {
    switch (c) {
      case u' ':
      case u'*':
      case u':':
      case u'-':
      case u'–':  // U+2013
      case u'(':
      case u')':
        return true;
      default:
        return false;
    }
  };
  return !base::ranges::all_of(inferred_label, IsStopWord);
}

// Shared function for InferLabelFromPrevious() and InferLabelFromNext().
bool InferLabelFromSibling(const WebFormControlElement& element,
                           bool forward,
                           std::u16string* label,
                           FormFieldData::LabelSource* label_source) {
  std::u16string inferred_label;
  FormFieldData::LabelSource inferred_label_source =
      FormFieldData::LabelSource::kUnknown;
  WebNode sibling = element;
  while (true) {
    sibling = forward ? sibling.NextSibling() : sibling.PreviousSibling();
    if (sibling.IsNull())
      break;

    // Skip over comments.
    if (sibling.IsCommentNode())
      continue;

    // Otherwise, only consider normal HTML elements and their contents.
    if (!sibling.IsElementNode() && !sibling.IsTextNode())
      break;

    // A label might be split across multiple "lightweight" nodes.
    // Coalesce any text contained in multiple consecutive
    //  (a) plain text nodes or
    //  (b) inline HTML elements that are essentially equivalent to text nodes.
    static base::NoDestructor<WebString> kBold("b");
    static base::NoDestructor<WebString> kStrong("strong");
    static base::NoDestructor<WebString> kSpan("span");
    static base::NoDestructor<WebString> kFont("font");
    if (sibling.IsTextNode() || HasTagName(sibling, *kBold) ||
        HasTagName(sibling, *kStrong) || HasTagName(sibling, *kSpan) ||
        HasTagName(sibling, *kFont)) {
      std::u16string value = FindChildText(sibling);
      // A text node's value will be empty if it is for a line break.
      bool add_space = sibling.IsTextNode() && value.empty();
      inferred_label_source = FormFieldData::LabelSource::kCombined;
      inferred_label =
          CombineAndCollapseWhitespace(value, inferred_label, add_space);
      continue;
    }

    // If we have identified a partial label and have reached a non-lightweight
    // element, consider the label to be complete.
    std::u16string trimmed_label;
    base::TrimWhitespace(inferred_label, base::TRIM_ALL, &trimmed_label);
    if (!trimmed_label.empty()) {
      inferred_label_source = FormFieldData::LabelSource::kCombined;
      break;
    }

    // <img> and <br> tags often appear between the input element and its
    // label text, so skip over them.
    static base::NoDestructor<WebString> kImage("img");
    static base::NoDestructor<WebString> kBreak("br");
    if (HasTagName(sibling, *kImage) || HasTagName(sibling, *kBreak))
      continue;

    // We only expect <p> and <label> tags to contain the full label text.
    static base::NoDestructor<WebString> kPage("p");
    static base::NoDestructor<WebString> kLabel("label");
    bool has_label_tag = HasTagName(sibling, *kLabel);
    if (HasTagName(sibling, *kPage) || has_label_tag) {
      inferred_label = FindChildText(sibling);
      inferred_label_source = has_label_tag
                                  ? FormFieldData::LabelSource::kLabelTag
                                  : FormFieldData::LabelSource::kPTag;
    }

    break;
  }

  base::TrimWhitespace(inferred_label, base::TRIM_ALL, &inferred_label);
  if (IsLabelValid(inferred_label)) {
    *label = std::move(inferred_label);
    *label_source = inferred_label_source;
    return true;
  }
  return false;
}

// Helper function to add a button's |title| to the |list|.
void AddButtonTitleToList(std::u16string title,
                          ButtonTitleType button_type,
                          ButtonTitleList* list) {
  title = base::CollapseWhitespace(std::move(title), false);
  if (title.empty())
    return;
  TruncateString(&title, kMaxLengthForSingleButtonTitle);
  list->push_back(std::make_pair(std::move(title), button_type));
}

// Returns true iff |attribute| contains one of |kButtonFeatures|.
bool AttributeHasButtonFeature(const WebString& attribute) {
  if (attribute.IsNull())
    return false;
  std::string value = attribute.Utf8();
  std::transform(value.begin(), value.end(), value.begin(), ::tolower);
  for (const char* const button_feature : kButtonFeatures) {
    if (value.find(button_feature, 0) != std::string::npos)
      return true;
  }
  return false;
}

// Returns true if |element|'s id, name or css class contain |kButtonFeatures|.
bool ElementAttributesHasButtonFeature(const WebElement& element) {
  return AttributeHasButtonFeature(element.GetAttribute("id")) ||
         AttributeHasButtonFeature(element.GetAttribute("name")) ||
         AttributeHasButtonFeature(element.GetAttribute("class"));
}

// Finds elements from |elements| that contains |kButtonFeatures|, adds them to
// |list| and updates the |total_length| of the |list|'s items.
// If |extract_value_attribute|, the "value" attribute is extracted as a button
// title. Otherwise, |WebElement::TextContent| (aka innerText in Javascript) is
// extracted as a title.
void FindElementsWithButtonFeatures(const WebElementCollection& elements,
                                    bool only_formless_elements,
                                    ButtonTitleType button_type,
                                    bool extract_value_attribute,
                                    ButtonTitleList* list) {
  static base::NoDestructor<WebString> kValue("value");
  for (WebElement item = elements.FirstItem(); !item.IsNull();
       item = elements.NextItem()) {
    if (!ElementAttributesHasButtonFeature(item))
      continue;
    if (only_formless_elements &&
        IsElementInsideFormOrFieldSet(item,
                                      /*consider_fieldset_tags=*/false)) {
      continue;
    }
    std::u16string title =
        extract_value_attribute
            ? (item.HasAttribute(*kValue) ? item.GetAttribute(*kValue).Utf16()
                                          : std::u16string())
            : item.TextContent().Utf16();
    if (extract_value_attribute && title.empty())
      title = item.TextContent().Utf16();
    AddButtonTitleToList(std::move(title), button_type, list);
  }
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// a previous sibling of |element|,
// e.g. Some Text <input ...>
// or   Some <span>Text</span> <input ...>
// or   <p>Some Text</p><input ...>
// or   <label>Some Text</label> <input ...>
// or   Some Text <img><input ...>
// or   <b>Some Text</b><br/> <input ...>.
bool InferLabelFromPrevious(const WebFormControlElement& element,
                            std::u16string* label,
                            FormFieldData::LabelSource* label_source) {
  return InferLabelFromSibling(element, /*forward=*/false, label, label_source);
}

// Same as InferLabelFromPrevious(), but in the other direction.
// Useful for cases like: <span><input type="checkbox">Label For Checkbox</span>
bool InferLabelFromNext(const WebFormControlElement& element,
                        std::u16string* label,
                        FormFieldData::LabelSource* label_source) {
  return InferLabelFromSibling(element, /*forward=*/true, label, label_source);
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// the placeholder text. e.g. <input placeholder="foo">
std::u16string InferLabelFromPlaceholder(const WebFormControlElement& element) {
  static base::NoDestructor<WebString> kPlaceholder("placeholder");
  if (element.HasAttribute(*kPlaceholder))
    return element.GetAttribute(*kPlaceholder).Utf16();

  return std::u16string();
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// the aria-label. e.g. <input aria-label="foo">
std::u16string InferLabelFromAriaLabel(const WebFormControlElement& element) {
  static const base::NoDestructor<WebString> kAriaLabel("aria-label");
  if (element.HasAttribute(*kAriaLabel))
    return element.GetAttribute(*kAriaLabel).Utf16();

  return std::u16string();
}

// Helper for |InferLabelForElement()| that infers a label, from
// the value attribute when it is present and user has not typed in (if
// element's value attribute is same as the element's value).
std::u16string InferLabelFromValueAttr(const WebFormControlElement& element) {
  static base::NoDestructor<WebString> kValue("value");
  if (element.HasAttribute(*kValue) &&
      element.GetAttribute(*kValue) == element.Value()) {
    return element.GetAttribute(*kValue).Utf16();
  }

  return std::u16string();
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// enclosing list item,
// e.g. <li>Some Text<input ...><input ...><input ...></li>
std::u16string InferLabelFromListItem(const WebFormControlElement& element) {
  WebNode parent = element.ParentNode();
  static base::NoDestructor<WebString> kListItem("li");
  while (!parent.IsNull() && parent.IsElementNode() &&
         !parent.To<WebElement>().HasHTMLTagName(*kListItem)) {
    parent = parent.ParentNode();
  }

  if (!parent.IsNull() && HasTagName(parent, *kListItem))
    return FindChildText(parent);

  return std::u16string();
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// enclosing label,
// e.g. <label>Some Text<input ...><input ...><input ...></label>
std::u16string InferLabelFromEnclosingLabel(
    const WebFormControlElement& element) {
  WebNode parent = element.ParentNode();
  static base::NoDestructor<WebString> kLabel("label");
  while (!parent.IsNull() && parent.IsElementNode() &&
         !parent.To<WebElement>().HasHTMLTagName(*kLabel)) {
    parent = parent.ParentNode();
  }

  if (!parent.IsNull() && HasTagName(parent, *kLabel))
    return FindChildText(parent);

  return std::u16string();
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// surrounding table structure,
// e.g. <tr><td>Some Text</td><td><input ...></td></tr>
// or   <tr><th>Some Text</th><td><input ...></td></tr>
// or   <tr><td><b>Some Text</b></td><td><b><input ...></b></td></tr>
// or   <tr><th><b>Some Text</b></th><td><b><input ...></b></td></tr>
std::u16string InferLabelFromTableColumn(const WebFormControlElement& element) {
  static base::NoDestructor<WebString> kTableCell("td");
  WebNode parent = element.ParentNode();
  while (!parent.IsNull() && parent.IsElementNode() &&
         !parent.To<WebElement>().HasHTMLTagName(*kTableCell)) {
    parent = parent.ParentNode();
  }

  if (parent.IsNull())
    return std::u16string();

  // Check all previous siblings, skipping non-element nodes, until we find a
  // non-empty text block.
  std::u16string inferred_label;
  WebNode previous = parent.PreviousSibling();
  static base::NoDestructor<WebString> kTableHeader("th");
  while (inferred_label.empty() && !previous.IsNull()) {
    if (HasTagName(previous, *kTableCell) ||
        HasTagName(previous, *kTableHeader))
      inferred_label = FindChildText(previous);

    previous = previous.PreviousSibling();
  }

  return inferred_label;
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// surrounding table structure,
//
// If there are multiple cells and the row with the input matches up with the
// previous row, then look for a specific cell within the previous row.
// e.g. <tr><td>Input 1 label</td><td>Input 2 label</td></tr>
//      <tr><td><input name="input 1"></td><td><input name="input2"></td></tr>
//
// Otherwise, just look in the entire previous row.
// e.g. <tr><td>Some Text</td></tr><tr><td><input ...></td></tr>
std::u16string InferLabelFromTableRow(const WebFormControlElement& element) {
  static base::NoDestructor<WebString> kTableCell("td");
  std::u16string inferred_label;

  // First find the <td> that contains |element|.
  WebNode cell = element.ParentNode();
  while (!cell.IsNull()) {
    if (cell.IsElementNode() &&
        cell.To<WebElement>().HasHTMLTagName(*kTableCell)) {
      break;
    }
    cell = cell.ParentNode();
  }

  // Not in a cell - bail out.
  if (cell.IsNull())
    return inferred_label;

  // Count the cell holding |element|.
  size_t cell_count = CalculateTableCellColumnSpan(cell.To<WebElement>());
  size_t cell_position = 0;
  size_t cell_position_end = cell_count - 1;

  // Count cells to the left to figure out |element|'s cell's position.
  for (WebNode cell_it = cell.PreviousSibling(); !cell_it.IsNull();
       cell_it = cell_it.PreviousSibling()) {
    if (cell_it.IsElementNode() &&
        cell_it.To<WebElement>().HasHTMLTagName(*kTableCell)) {
      cell_position += CalculateTableCellColumnSpan(cell_it.To<WebElement>());
    }
  }

  // Count cells to the right.
  for (WebNode cell_it = cell.NextSibling(); !cell_it.IsNull();
       cell_it = cell_it.NextSibling()) {
    if (cell_it.IsElementNode() &&
        cell_it.To<WebElement>().HasHTMLTagName(*kTableCell)) {
      cell_count += CalculateTableCellColumnSpan(cell_it.To<WebElement>());
    }
  }

  // Combine left + right.
  cell_count += cell_position;
  cell_position_end += cell_position;

  // Find the current row.
  static base::NoDestructor<WebString> kTableRow("tr");
  WebNode parent = element.ParentNode();
  while (!parent.IsNull() && parent.IsElementNode() &&
         !parent.To<WebElement>().HasHTMLTagName(*kTableRow)) {
    parent = parent.ParentNode();
  }

  if (parent.IsNull())
    return inferred_label;

  // Now find the previous row.
  WebNode row_it = parent.PreviousSibling();
  while (!row_it.IsNull()) {
    if (row_it.IsElementNode() &&
        row_it.To<WebElement>().HasHTMLTagName(*kTableRow)) {
      break;
    }
    row_it = row_it.PreviousSibling();
  }

  // If there exists a previous row, check its cells and size. If they align
  // with the current row, infer the label from the cell above.
  if (!row_it.IsNull()) {
    WebNode matching_cell;
    size_t prev_row_count = 0;
    WebNode prev_row_it = row_it.FirstChild();
    static base::NoDestructor<WebString> kTableHeader("th");
    while (!prev_row_it.IsNull()) {
      if (prev_row_it.IsElementNode()) {
        WebElement prev_row_element = prev_row_it.To<WebElement>();
        if (prev_row_element.HasHTMLTagName(*kTableCell) ||
            prev_row_element.HasHTMLTagName(*kTableHeader)) {
          size_t span = CalculateTableCellColumnSpan(prev_row_element);
          size_t prev_row_count_end = prev_row_count + span - 1;
          if (prev_row_count == cell_position &&
              prev_row_count_end == cell_position_end) {
            matching_cell = prev_row_it;
          }
          prev_row_count += span;
        }
      }
      prev_row_it = prev_row_it.NextSibling();
    }
    if ((cell_count == prev_row_count) && !matching_cell.IsNull()) {
      inferred_label = FindChildText(matching_cell);
      if (!inferred_label.empty())
        return inferred_label;
    }
  }

  // If there is no previous row, or if the previous row and current row do not
  // align, check all previous siblings, skipping non-element nodes, until we
  // find a non-empty text block.
  WebNode previous = parent.PreviousSibling();
  while (inferred_label.empty() && !previous.IsNull()) {
    if (HasTagName(previous, *kTableRow))
      inferred_label = FindChildText(previous);

    previous = previous.PreviousSibling();
  }

  return inferred_label;
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// a surrounding div table,
// e.g. <div>Some Text<span><input ...></span></div>
// e.g. <div>Some Text</div><div><input ...></div>
//
// Because this is already traversing the <div> structure, if it finds a <label>
// sibling along the way, infer from that <label>.
std::u16string InferLabelFromDivTable(const WebFormControlElement& element) {
  WebNode node = element.ParentNode();
  bool looking_for_parent = true;
  std::set<WebNode> divs_to_skip;

  // Search the sibling and parent <div>s until we find a candidate label.
  std::u16string inferred_label;
  static base::NoDestructor<WebString> kDiv("div");
  static base::NoDestructor<WebString> kLabel("label");
  while (inferred_label.empty() && !node.IsNull()) {
    if (HasTagName(node, *kDiv)) {
      if (looking_for_parent)
        inferred_label = FindChildTextWithIgnoreList(node, divs_to_skip);
      else
        inferred_label = FindChildText(node);

      // Avoid sibling DIVs that contain autofillable fields.
      if (!looking_for_parent && !inferred_label.empty()) {
        static base::NoDestructor<WebString> kSelector(
            "input, select, textarea");
        WebElement result_element = node.QuerySelector(*kSelector);
        if (!result_element.IsNull()) {
          inferred_label.clear();
          divs_to_skip.insert(node);
        }
      }

      looking_for_parent = false;
    } else if (!looking_for_parent && HasTagName(node, *kLabel)) {
      WebLabelElement label_element = node.To<WebLabelElement>();
      if (label_element.CorrespondingControl().IsNull())
        inferred_label = FindChildText(node);
    } else if (looking_for_parent && IsTraversableContainerElement(node)) {
      // If the element is in a non-div container, its label most likely is too.
      break;
    }

    if (node.PreviousSibling().IsNull()) {
      // If there are no more siblings, continue walking up the tree.
      looking_for_parent = true;
    }

    node = looking_for_parent ? node.ParentNode() : node.PreviousSibling();
  }

  return inferred_label;
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// a surrounding definition list,
// e.g. <dl><dt>Some Text</dt><dd><input ...></dd></dl>
// e.g. <dl><dt><b>Some Text</b></dt><dd><b><input ...></b></dd></dl>
std::u16string InferLabelFromDefinitionList(
    const WebFormControlElement& element) {
  static base::NoDestructor<WebString> kDefinitionData("dd");
  WebNode parent = element.ParentNode();
  while (!parent.IsNull() && parent.IsElementNode() &&
         !parent.To<WebElement>().HasHTMLTagName(*kDefinitionData))
    parent = parent.ParentNode();

  if (parent.IsNull() || !HasTagName(parent, *kDefinitionData))
    return std::u16string();

  // Skip by any intervening text nodes.
  WebNode previous = parent.PreviousSibling();
  while (!previous.IsNull() && previous.IsTextNode())
    previous = previous.PreviousSibling();

  static base::NoDestructor<WebString> kDefinitionTag("dt");
  if (previous.IsNull() || !HasTagName(previous, *kDefinitionTag))
    return std::u16string();

  return FindChildText(previous);
}

// Returns the element type for all ancestor nodes in CAPS, starting with the
// parent node.
std::vector<std::string> AncestorTagNames(
    const WebFormControlElement& element) {
  std::vector<std::string> tag_names;
  for (WebNode parent_node = element.ParentNode(); !parent_node.IsNull();
       parent_node = parent_node.ParentNode()) {
    if (!parent_node.IsElementNode())
      continue;

    tag_names.push_back(parent_node.To<WebElement>().TagName().Utf8());
  }
  return tag_names;
}

// Infers corresponding label for |element| from surrounding context in the DOM,
// e.g. the contents of the preceding <p> tag or text element.
bool InferLabelForElement(const WebFormControlElement& element,
                          std::u16string* label,
                          FormFieldData::LabelSource* label_source) {
  if (IsCheckableElement(ToWebInputElement(&element))) {
    if (InferLabelFromNext(element, label, label_source))
      return true;
  }

  if (InferLabelFromPrevious(element, label, label_source))
    return true;

  // If we didn't find a label, check for placeholder text.
  std::u16string inferred_label = InferLabelFromPlaceholder(element);
  if (IsLabelValid(inferred_label)) {
    *label_source = FormFieldData::LabelSource::kPlaceHolder;
    *label = std::move(inferred_label);
    return true;
  }

  // If we didn't find a placeholder, check for aria-label text.
  inferred_label = InferLabelFromAriaLabel(element);
  if (IsLabelValid(inferred_label)) {
    *label_source = FormFieldData::LabelSource::kAriaLabel;
    *label = std::move(inferred_label);
    return true;
  }

  // For all other searches that involve traversing up the tree, the search
  // order is based on which tag is the closest ancestor to |element|.
  std::vector<std::string> tag_names = AncestorTagNames(element);
  std::set<std::string> seen_tag_names;
  FormFieldData::LabelSource ancestor_label_source =
      FormFieldData::LabelSource::kUnknown;
  for (const std::string& tag_name : tag_names) {
    if (base::Contains(seen_tag_names, tag_name))
      continue;

    seen_tag_names.insert(tag_name);
    if (tag_name == "LABEL") {
      ancestor_label_source = FormFieldData::LabelSource::kLabelTag;
      inferred_label = InferLabelFromEnclosingLabel(element);
    } else if (tag_name == "DIV") {
      ancestor_label_source = FormFieldData::LabelSource::kDivTable;
      inferred_label = InferLabelFromDivTable(element);
    } else if (tag_name == "TD") {
      ancestor_label_source = FormFieldData::LabelSource::kTdTag;
      inferred_label = InferLabelFromTableColumn(element);
      if (!IsLabelValid(inferred_label))
        inferred_label = InferLabelFromTableRow(element);
    } else if (tag_name == "DD") {
      ancestor_label_source = FormFieldData::LabelSource::kDdTag;
      inferred_label = InferLabelFromDefinitionList(element);
    } else if (tag_name == "LI") {
      ancestor_label_source = FormFieldData::LabelSource::kLiTag;
      inferred_label = InferLabelFromListItem(element);
    } else if (tag_name == "FIELDSET") {
      break;
    }

    if (IsLabelValid(inferred_label)) {
      *label_source = ancestor_label_source;
      *label = std::move(inferred_label);
      return true;
    }
  }

  // If we didn't find a label, check the value attr used as the placeholder.
  inferred_label = InferLabelFromValueAttr(element);
  if (IsLabelValid(inferred_label)) {
    *label_source = FormFieldData::LabelSource::kValue;
    *label = std::move(inferred_label);
    return true;
  }
  return false;
}

// Removes the duplicate titles and limits totals length. The order of the list
// is preserved as first elements are more reliable features than following
// ones.
void RemoveDuplicatesAndLimitTotalLength(ButtonTitleList* result) {
  std::set<ButtonTitleInfo> already_added;
  ButtonTitleList unique_titles;
  int total_length = 0;
  for (auto title : *result) {
    if (already_added.find(title) != already_added.end())
      continue;
    already_added.insert(title);

    total_length += title.first.length();
    if (total_length > kMaxLengthForAllButtonTitles) {
      int new_length =
          title.first.length() - (total_length - kMaxLengthForAllButtonTitles);
      TruncateString(&title.first, new_length);
    }
    unique_titles.push_back(std::move(title));

    if (total_length >= kMaxLengthForAllButtonTitles)
      break;
  }
  *result = std::move(unique_titles);
}

// Infer button titles enclosed by |root_element|. |root_element| may be a
// <form> or a <body> if there are <input>s that are not enclosed in a <form>
// element.
ButtonTitleList InferButtonTitlesForForm(const WebElement& root_element) {
  static base::NoDestructor<WebString> kA("a");
  static base::NoDestructor<WebString> kButton("button");
  static base::NoDestructor<WebString> kDiv("div");
  static base::NoDestructor<WebString> kForm("form");
  static base::NoDestructor<WebString> kInput("input");
  static base::NoDestructor<WebString> kSpan("span");
  static base::NoDestructor<WebString> kSubmit("submit");

  // If the |root_element| is not a <form>, ignore the elements inclosed in a
  // <form>.
  bool only_formless_elements = !root_element.HasHTMLTagName(*kForm);

  ButtonTitleList result;
  WebElementCollection input_elements =
      root_element.GetElementsByHTMLTagName(*kInput);
  int total_length = 0;
  for (WebElement item = input_elements.FirstItem();
       !item.IsNull() && total_length < kMaxLengthForAllButtonTitles;
       item = input_elements.NextItem()) {
    DCHECK(item.IsFormControlElement());
    WebFormControlElement control_element =
        item.ToConst<WebFormControlElement>();
    if (only_formless_elements && !control_element.Form().IsNull())
      continue;
    bool is_submit_input =
        control_element.FormControlTypeForAutofill() == *kSubmit;
    bool is_button_input =
        control_element.FormControlTypeForAutofill() == *kButton;
    if (!is_submit_input && !is_button_input)
      continue;
    std::u16string title = control_element.Value().Utf16();
    AddButtonTitleToList(std::move(title),
                         is_submit_input
                             ? ButtonTitleType::INPUT_ELEMENT_SUBMIT_TYPE
                             : ButtonTitleType::INPUT_ELEMENT_BUTTON_TYPE,
                         &result);
  }
  WebElementCollection buttons =
      root_element.GetElementsByHTMLTagName(*kButton);
  for (WebElement item = buttons.FirstItem(); !item.IsNull();
       item = buttons.NextItem()) {
    WebString type_attribute = item.GetAttribute("type");
    if (!type_attribute.IsNull() && type_attribute != *kButton &&
        type_attribute != *kSubmit) {
      // Neither type='submit' nor type='button'. Skip this button.
      continue;
    }
    if (only_formless_elements &&
        IsElementInsideFormOrFieldSet(item,
                                      /*consider_fieldset_tags=*/false)) {
      continue;
    }
    bool is_submit_type = type_attribute.IsNull() || type_attribute == *kSubmit;
    std::u16string title = item.TextContent().Utf16();
    AddButtonTitleToList(std::move(title),
                         is_submit_type
                             ? ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE
                             : ButtonTitleType::BUTTON_ELEMENT_BUTTON_TYPE,
                         &result);
  }
  FindElementsWithButtonFeatures(
      root_element.GetElementsByHTMLTagName(*kA), only_formless_elements,
      ButtonTitleType::HYPERLINK, /*extract_value_attribute=*/true, &result);
  FindElementsWithButtonFeatures(root_element.GetElementsByHTMLTagName(*kDiv),
                                 only_formless_elements, ButtonTitleType::DIV,
                                 /*extract_value_attribute=*/false, &result);
  FindElementsWithButtonFeatures(root_element.GetElementsByHTMLTagName(*kSpan),
                                 only_formless_elements, ButtonTitleType::SPAN,
                                 /*extract_value_attribute=*/false, &result);
  RemoveDuplicatesAndLimitTotalLength(&result);
  return result;
}

// Fills |option_strings| with the values of the <option> elements present in
// |select_element|.
void GetOptionStringsFromElement(const WebSelectElement& select_element,
                                 std::vector<SelectOption>* options) {
  DCHECK(!select_element.IsNull());

  options->clear();
  WebVector<WebElement> list_items = select_element.GetListItems();

  // Constrain the maximum list length to prevent a malicious site from DOS'ing
  // the browser, without entirely breaking autocomplete for some extreme
  // legitimate sites: http://crbug.com/49332 and http://crbug.com/363094
  if (list_items.size() > kMaxListSize)
    return;

  options->reserve(list_items.size());
  for (const auto& list_item : list_items) {
    if (IsOptionElement(list_item)) {
      const WebOptionElement option = list_item.ToConst<WebOptionElement>();
      options->push_back({.value = option.Value().Utf16(),
                          .content = option.GetText().Utf16()});
    }
  }
}

// Use insertion sort to sort the almost sorted |elements|.
void SortByFieldRendererIds(std::vector<WebFormControlElement>& elements) {
  for (auto it = elements.begin(); it != elements.end(); it++) {
    // insertion_point will point to the first element that is greater than
    // to_be_inserted.
    const auto& to_be_inserted = *it;
    const auto insertion_point = base::ranges::upper_bound(
        elements.begin(), it, to_be_inserted.UniqueRendererFormControlId(),
        base::ranges::less{},
        &WebFormControlElement::UniqueRendererFormControlId);

    // Shift all elements from [insertion_point, it) right and move |it| to the
    // front.
    std::rotate(insertion_point, it, it + 1);
  }
}

std::vector<WebFormControlElement>::iterator SearchInSortedVector(
    const FormFieldData& field,
    std::vector<WebFormControlElement>& sorted_elements) {
  auto get_field_renderer_id = [](const WebFormControlElement& e) {
    return GetFieldRendererId(e);
  };
  // Find the first element whose unique renderer ID is greater or equal to
  // |fields|.
  auto it = base::ranges::lower_bound(
      sorted_elements.begin(), sorted_elements.end(), field.unique_renderer_id,
      base::ranges::less{}, get_field_renderer_id);
  if (it == sorted_elements.end() ||
      GetFieldRendererId(*it) != field.unique_renderer_id) {
    return sorted_elements.end();
  }
  return it;
}

// The callback type used by |ForEachMatchingFormField()|.
typedef void (*Callback)(const FormFieldData&,
                         bool, /* is_initiating_element */
                         blink::WebFormControlElement*);

// Note that the order of elements in |control_elements| may be changed by this
// function. Also the returned WebFormControlElements won't appear in DOM
// traversal order.
std::vector<WebFormControlElement> ForEachMatchingFormFieldCommon(
    std::vector<WebFormControlElement>& control_elements,
    const WebElement& initiating_element,
    const FormData& data,
    FieldFilterMask filters,
    mojom::RendererFormDataAction action,
    const Callback& callback) {
  const bool num_elements_matches_num_fields =
      control_elements.size() == data.fields.size();
  UMA_HISTOGRAM_BOOLEAN("Autofill.NumElementsMatchesNumFields",
                        num_elements_matches_num_fields);

  // The intended behaviour is:
  // * Autofill the currently focused element.
  // * Send the blur event.
  // * For each other element, focus -> autofill -> blur.
  // * Send the focus event for the initially focused element.
  WebFormControlElement* initially_focused_element = nullptr;

  // This container stores the pairs of autofillable WebFormControlElement* and
  // the corresponding indexes of |data.fields| that are used to fill this
  // element.
  std::vector<std::pair<WebFormControlElement*, size_t>>
      autofillable_elements_index_pairs;

  std::vector<WebFormControlElement> matching_fields;
  matching_fields.reserve(control_elements.size());

  // Prepare for binary search.
  SortByFieldRendererIds(control_elements);

  // If this is a preview filling, prevent already autofilled fields from being
  // highlighted.
  if (action == mojom::RendererFormDataAction::kPreview &&
      base::FeatureList::IsEnabled(
          features::kAutofillHighlightOnlyChangedValuesInPreviewMode)) {
    for (auto& element : control_elements) {
      element.SetPreventHighlightingOfAutofilledFields(true);
    }
  }

  for (size_t i = 0; i < data.fields.size(); ++i) {
    if (matching_fields.size() == control_elements.size())
      break;  // All possible matches are already found.

    auto it = SearchInSortedVector(data.fields[i], control_elements);
    if (it == control_elements.end())
      continue;

    WebFormControlElement& element = *it;

    element.SetAutofillSection(WebString::FromUTF8(data.fields[i].section));

    // Only autofill empty fields (or those with the field's default value
    // attribute) and the field that initiated the filling, i.e. the field the
    // user is currently editing and interacting with.
    const WebInputElement* input_element = ToWebInputElement(&element);
    static base::NoDestructor<WebString> kValue("value");
    static base::NoDestructor<WebString> kPlaceholder("placeholder");

    if (((filters & FILTER_DISABLED_ELEMENTS) && !element.IsEnabled()) ||
        ((filters & FILTER_READONLY_ELEMENTS) && element.IsReadOnly()) ||
        // See description for FILTER_NON_FOCUSABLE_ELEMENTS.
        ((filters & FILTER_NON_FOCUSABLE_ELEMENTS) && !element.IsFocusable() &&
         !IsSelectElement(element))) {
      continue;
    }

    // Autofill the initiating element.
    bool is_initiating_element = (element == initiating_element);
    if (is_initiating_element) {
      if (action == mojom::RendererFormDataAction::kFill && element.Focused())
        initially_focused_element = &element;

      matching_fields.push_back(element);
      // In preview mode, only fill the field if it changes the fields value.
      // With this, the WebAutofillState is not changed from kAutofilled to
      // kPreviewed. This prevents the highlighting to change.
      if (action == mojom::RendererFormDataAction::kFill ||
          data.fields[i].value != element.Value().Utf16() ||
          !base::FeatureList::IsEnabled(
              features::kAutofillHighlightOnlyChangedValuesInPreviewMode)) {
        callback(data.fields[i], is_initiating_element, &element);
      }
      continue;
    }

    if (element.GetAutofillState() == WebAutofillState::kAutofilled)
      continue;

    // A text field, with a non-empty value that is entered by the user,
    // and is NOT the value of the input field's "value" or "placeholder"
    // attribute, is skipped. Some sites fill the fields with formatting
    // string. To tell the difference between the values entered by the user
    // and the site, we'll sanitize the value. If the sanitized value is
    // empty, it means that the site has filled the field, in this case, the
    // field is not skipped. Nevertheless the below condition does not hold
    // for sites set the |kValue| attribute to the user-input value.
    if ((IsAutofillableInputElement(input_element) ||
         IsTextAreaElement(element)) &&
        element.UserHasEditedTheField() &&
        !SanitizedFieldIsEmpty(element.Value().Utf16()) &&
        (!element.HasAttribute(*kValue) ||
         element.GetAttribute(*kValue) != element.Value()) &&
        (!element.HasAttribute(*kPlaceholder) ||
         base::i18n::ToLower(element.GetAttribute(*kPlaceholder).Utf16()) !=
             base::i18n::ToLower(element.Value().Utf16()))) {
      continue;
    }

    // Check if we should autofill/preview/clear a select element or leave it.
    if (IsSelectElement(element) && element.UserHasEditedTheField() &&
        !SanitizedFieldIsEmpty(element.Value().Utf16())) {
      continue;
    }

    // Storing the indexes of non-initiating elements to be autofilled after
    // triggering the blur event for the initiating element.
    autofillable_elements_index_pairs.emplace_back(&element, i);
  }

  // If there is no other field to be autofilled, sending the blur event and
  // then the focus event for the initiating element does not make sense.
  if (autofillable_elements_index_pairs.empty())
    return matching_fields;

  // A blur event is emitted for the focused element if it is the initiating
  // element before all other elements are autofilled.
  if (initially_focused_element)
    initially_focused_element->DispatchBlurEvent();

  // Autofill the non-initiating elements.
  for (const auto& pair : autofillable_elements_index_pairs) {
    WebFormControlElement* filled_element = pair.first;
    size_t index_in_data_fields = pair.second;
    matching_fields.push_back(*filled_element);
    callback(data.fields[index_in_data_fields], false, filled_element);
  }

  // A focus event is emitted for the initiating element after autofilling is
  // completed. It is not intended to work for the preview filling.
  if (initially_focused_element)
    initially_focused_element->DispatchFocusEvent();

  return matching_fields;
}

// For each autofillable field in |data| that matches a field in the |form|,
// the |callback| is invoked with the corresponding |form| field data.
std::vector<WebFormControlElement> ForEachMatchingFormField(
    const WebFormElement& form_element,
    const WebElement& initiating_element,
    const FormData& data,
    FieldFilterMask filters,
    mojom::RendererFormDataAction action,
    const Callback& callback) {
  std::vector<WebFormControlElement> control_elements =
      ExtractAutofillableElementsInForm(form_element);
  return ForEachMatchingFormFieldCommon(control_elements, initiating_element,
                                        data, filters, action, callback);
}

// For each autofillable field in |data| that matches a field in the set of
// unowned autofillable form fields, the |callback| is invoked with the
// corresponding |data| field.
std::vector<WebFormControlElement> ForEachMatchingUnownedFormField(
    const WebElement& initiating_element,
    const FormData& data,
    FieldFilterMask filters,
    mojom::RendererFormDataAction action,
    const Callback& callback) {
  if (initiating_element.IsNull())
    return {};

  std::vector<WebFormControlElement> control_elements =
      GetUnownedAutofillableFormFieldElements(initiating_element.GetDocument(),
                                              nullptr);
  if (!IsElementInControlElementSet(initiating_element, control_elements))
    return {};

  return ForEachMatchingFormFieldCommon(control_elements, initiating_element,
                                        data, filters, action, callback);
}

// Sets the |field|'s value to the value in |data|, and specifies the section
// for filled fields.  Also sets the "autofilled" attribute,
// causing the background to be yellow.
void FillFormField(const FormFieldData& data,
                   bool is_initiating_node,
                   blink::WebFormControlElement* field) {
  // Nothing to fill.
  if (data.value.empty())
    return;

  if (!data.is_autofilled)
    return;

  WebInputElement* input_element = ToWebInputElement(field);

  if (IsCheckableElement(input_element)) {
    input_element->SetChecked(IsChecked(data.check_status), true);
  } else {
    std::u16string value = data.value;
    if (IsTextInput(input_element) || IsMonthInput(input_element)) {
      // If the maxlength attribute contains a negative value, maxLength()
      // returns the default maxlength value.
      TruncateString(&value, input_element->MaxLength());
    }
    field->SetAutofillValue(blink::WebString::FromUTF16(value));
  }
  // Setting the form might trigger JavaScript, which is capable of
  // destroying the frame.
  if (!field->GetDocument().GetFrame())
    return;

  field->SetAutofillState(WebAutofillState::kAutofilled);

  if (is_initiating_node &&
      ((IsTextInput(input_element) || IsMonthInput(input_element)) ||
       IsTextAreaElement(*field))) {
    int length = field->Value().length();
    field->SetSelectionRange(length, length);
    // Clear the current IME composition (the underline), if there is one.
    field->GetDocument().GetFrame()->UnmarkText();
  }
}

// Sets the |field|'s "suggested" (non JS visible) value to the value in |data|.
// Also sets the "autofilled" attribute, causing the background to be yellow.
void PreviewFormField(const FormFieldData& data,
                      bool is_initiating_node,
                      blink::WebFormControlElement* field) {
  // Nothing to preview.
  if (data.value.empty())
    return;

  if (!data.is_autofilled)
    return;

  // Preview input, textarea and select fields. For input fields, excludes
  // checkboxes and radio buttons, as there is no provision for
  // setSuggestedCheckedValue in WebInputElement.
  WebInputElement* input_element = ToWebInputElement(field);
  if (IsTextInput(input_element) || IsMonthInput(input_element)) {
    // If the maxlength attribute contains a negative value, maxLength()
    // returns the default maxlength value.
    input_element->SetSuggestedValue(blink::WebString::FromUTF16(
        data.value.substr(0, input_element->MaxLength())));
    input_element->SetAutofillState(WebAutofillState::kPreviewed);
  } else if (IsTextAreaElement(*field) || IsSelectElement(*field)) {
    field->SetSuggestedValue(blink::WebString::FromUTF16(data.value));
    field->SetAutofillState(WebAutofillState::kPreviewed);
  }

  if (is_initiating_node &&
      (IsTextInput(input_element) || IsTextAreaElement(*field))) {
    // Select the part of the text that the user didn't type.
    PreviewSuggestion(field->SuggestedValue().Utf16(), field->Value().Utf16(),
                      field);
  }
}

// A less-than comparator for FormFieldDatas pointer by their FieldRendererId.
// It also supports direct comparison of a FieldRendererId with a FormFieldData
// pointer.
struct CompareByRendererId {
  using is_transparent = void;
  bool operator()(const std::pair<FormFieldData*, ShadowFieldData>& f,
                  const std::pair<FormFieldData*, ShadowFieldData>& g) const {
    DCHECK(f.first && g.first);
    return f.first->unique_renderer_id < g.first->unique_renderer_id;
  }
  bool operator()(const FieldRendererId f,
                  const std::pair<FormFieldData*, ShadowFieldData>& g) const {
    DCHECK(g.first);
    return f < g.first->unique_renderer_id;
  }
  bool operator()(const std::pair<FormFieldData*, ShadowFieldData>& f,
                  FieldRendererId g) const {
    DCHECK(f.first);
    return f.first->unique_renderer_id < g;
  }
};

// Searches |field_set| for a unique field with name |field_name|. If there is
// none or more than one field with that name, the fields' shadow hosts' name
// and id attributes are tested, and the first match is returned. Returns
// nullptr if no match was found.
FormFieldData* SearchForFormControlByName(
    const std::u16string& field_name,
    const base::flat_set<std::pair<FormFieldData*, ShadowFieldData>,
                         CompareByRendererId>& field_set) {
  if (field_name.empty())
    return nullptr;

  auto get_field_name = [](const auto& p) { return p.first->name; };
  auto it = base::ranges::find(field_set, field_name, get_field_name);
  auto end = field_set.end();
  if (it == end ||
      base::ranges::find(it + 1, end, field_name, get_field_name) != end) {
    auto ShadowHostHasTargetName = [&](const auto& p) {
      return base::Contains(p.second.shadow_host_name_attributes, field_name) ||
             base::Contains(p.second.shadow_host_id_attributes, field_name);
    };
    it = base::ranges::find_if(field_set, ShadowHostHasTargetName);
  }
  return it != end ? it->first : nullptr;
}

// Updates the FormFieldData::label of each field in `field_set` according to
// the <label> descendant of |form_or_fieldset|, if there is any. The extracted
// label is label.firstChild().nodeValue() of the label element.
void MatchLabelsAndFields(
    const WebElement& form_or_fieldset,
    const base::flat_set<std::pair<FormFieldData*, ShadowFieldData>,
                         CompareByRendererId>& field_set) {
  static base::NoDestructor<WebString> kLabel("label");
  static base::NoDestructor<WebString> kFor("for");
  static base::NoDestructor<WebString> kHidden("hidden");

  WebElementCollection labels =
      form_or_fieldset.GetElementsByHTMLTagName(*kLabel);
  DCHECK(!labels.IsNull());

  for (WebElement item = labels.FirstItem(); !item.IsNull();
       item = labels.NextItem()) {
    WebLabelElement label = item.To<WebLabelElement>();
    WebElement control = label.CorrespondingControl();
    FormFieldData* field_data = nullptr;

    if (control.IsNull()) {
      // Sometimes site authors will incorrectly specify the corresponding
      // field element's name rather than its id, so we compensate here.
      field_data = SearchForFormControlByName(label.GetAttribute(*kFor).Utf16(),
                                              field_set);
    } else if (control.IsFormControlElement()) {
      WebFormControlElement form_control = control.To<WebFormControlElement>();
      if (form_control.FormControlTypeForAutofill() == *kHidden)
        continue;
      // Typical case: look up |field_data| in |field_set|.
      auto iter = field_set.find(GetFieldRendererId(form_control));
      if (iter == field_set.end())
        continue;
      field_data = iter->first;
    }

    if (!field_data)
      continue;

    std::u16string label_text = FindChildText(label);

    // Concatenate labels because some sites might have multiple label
    // candidates.
    if (!field_data->label.empty() && !label_text.empty())
      field_data->label += u" ";
    field_data->label += label_text;
  }
}

// Populates the |form|'s
//  * FormData::fields
//  * FormData::child_frames
// using the DOM elements in |control_elements| and |iframe_elements|.
//
// Optionally, |optional_field| is set to the FormFieldData that corresponds to
// |form_control_element|.
//
// The labels of the FormData::fields are extracted from the |form_element| or,
// if |form_element| is nullptr, from the <fieldset> elements |fieldsets|.
//
// |field_data_manager| and |extract_mask| are only passed to
// WebFormControlElementToFormField().
bool FormOrFieldsetsToFormData(
    const WebFrame* frame,
    const blink::WebFormElement* form_element,
    const blink::WebFormControlElement* form_control_element,
    const std::vector<blink::WebElement>& fieldsets,
    const WebVector<WebFormControlElement>& control_elements,
    const std::vector<blink::WebElement>& iframe_elements,
    const FieldDataManager* field_data_manager,
    ExtractMask extract_mask,
    FormData* form,
    FormFieldData* optional_field) {
  DCHECK(form);
  DCHECK(form->fields.empty());
  DCHECK(form->child_frames.empty());
  DCHECK(!optional_field || form_control_element);
  DCHECK(!form_element || fieldsets.empty());

  // Extracts fields from |control_elements| into `form->fields` and sets
  // `form->child_frames[i].predecessor` to the field index of the last field
  // that precedes the |i|th child frame.
  //
  // If `control_elements[i]` is autofillable, `fields_extracted[i]` is set to
  // true and the corresponding FormFieldData is appended to `form->fields`.
  //
  // After each iteration, `iframe_elements[next_iframe]` is the first iframe
  // that comes after `control_elements[i]`.
  //
  // After the loop,
  // - `form->fields` is completely populated;
  // - `form->child_frames` has the correct size and
  //   `form->child_frames[i].predecessor` is set to the correct value, but
  //   `form->child_frames[i].token` is not initialized yet.
  form->fields.reserve(control_elements.size());
  if (base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    form->child_frames.resize(iframe_elements.size());
  }
  std::vector<bool> fields_extracted(control_elements.size(), false);

  std::vector<ShadowFieldData> shadow_fields;

  for (size_t i = 0, next_iframe = 0; i < control_elements.size(); ++i) {
    const WebFormControlElement& control_element = control_elements[i];

    if (!IsAutofillableElement(control_element))
      continue;

    form->fields.emplace_back();
    shadow_fields.emplace_back();
    WebFormControlElementToFormField(
        form->unique_renderer_id, control_element, field_data_manager,
        extract_mask, &form->fields.back(), &shadow_fields.back());
    fields_extracted[i] = true;

    if (base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
      // Providing |common_ancestor| speeds up IsDomPredecessor(). If the
      // |control_element| is part of a form, it is guaranteed that the
      // |form_element| is a common ancestor, otherwise we fall back to the
      // Document as a non-ideal but always correct alternative.
      const blink::WebNode& common_ancestor =
          form_element ? static_cast<const blink::WebNode&>(*form_element)
                       : static_cast<const blink::WebNode&>(
                             control_elements[i].GetDocument());
      // Finds the last frame that precedes |control_element|.
      while (next_iframe < iframe_elements.size() &&
             !IsDomPredecessor(control_element, iframe_elements[next_iframe],
                               common_ancestor)) {
        ++next_iframe;
      }
      // The |next_frame|th frame precedes `control_element` and thus the last
      // added FormFieldData. The |k|th frames for |k| > |next_frame| may also
      // precede that FormFieldData. If they do not,
      // `form->child_frames[i].predecessor` will be updated in a later
      // iteration.
      for (size_t k = next_iframe; k < iframe_elements.size(); ++k)
        form->child_frames[k].predecessor = form->fields.size() - 1;
    }

    if (form->fields.size() > kMaxParseableFields) {
      form->child_frames.clear();
      form->fields.clear();
      return false;
    }
    DCHECK_LE(form->fields.size(), control_elements.size());
  }

  // Extracts field labels from the <label for="..."> tags.
  {
    std::vector<std::pair<FormFieldData*, ShadowFieldData>> items;
    DCHECK_EQ(form->fields.size(), shadow_fields.size());
    for (size_t i = 0; i < form->fields.size(); i++) {
      items.emplace_back(&form->fields[i], std::move(shadow_fields[i]));
    }
    base::flat_set<std::pair<FormFieldData*, ShadowFieldData>,
                   CompareByRendererId>
        field_set(std::move(items));

    if (form_element) {
      MatchLabelsAndFields(*form_element, field_set);
    } else {
      for (const WebElement& fieldset : fieldsets)
        MatchLabelsAndFields(fieldset, field_set);
    }
  }

  // Infers field labels from other tags or <labels> without for="...".
  bool found_field = false;
  DCHECK_EQ(control_elements.size(), fields_extracted.size());
  DCHECK_EQ(form->fields.size(),
            base::as_unsigned(base::ranges::count(fields_extracted, true)));
  for (size_t element_index = 0, field_index = 0;
       element_index < control_elements.size(); ++element_index) {
    if (!fields_extracted[element_index])
      continue;

    const WebFormControlElement& control_element =
        control_elements[element_index];
    FormFieldData& field = form->fields[field_index++];
    if (field.label.empty())
      InferLabelForElement(control_element, &field.label, &field.label_source);
    TruncateString(&field.label, kMaxDataLength);

    if (optional_field && *form_control_element == control_element) {
      *optional_field = field;
      found_field = true;
    }
  }

  // The form_control_element was not found in control_elements. This can
  // happen if elements are dynamically removed from the form while it is
  // being processed. See http://crbug.com/849870
  if (optional_field && !found_field) {
    form->fields.clear();
    form->child_frames.clear();
    return false;
  }

  // Extracts the frame tokens of |iframe_elements|.
  if (base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    DCHECK_EQ(form->child_frames.size(), iframe_elements.size());
    for (size_t i = 0; i < iframe_elements.size(); ++i) {
      WebFrame* iframe = WebFrame::FromFrameOwnerElement(iframe_elements[i]);
      if (iframe && iframe->IsWebLocalFrame()) {
        form->child_frames[i].token = LocalFrameToken(
            iframe->ToWebLocalFrame()->GetLocalFrameToken().value());
      } else if (iframe && iframe->IsWebRemoteFrame()) {
        form->child_frames[i].token = RemoteFrameToken(
            iframe->ToWebRemoteFrame()->GetRemoteFrameToken().value());
      }
    }
    base::EraseIf(form->child_frames, [](const auto& child_frame) {
      return absl::visit([](const auto& token) { return token.is_empty(); },
                         child_frame.token);
    });
  }

  if (form->child_frames.size() > kMaxParseableChildFrames)
    form->child_frames.clear();

  const bool success = (!form->fields.empty() || !form->child_frames.empty()) &&
                       form->fields.size() < kMaxParseableFields;
  if (!success) {
    form->fields.clear();
    form->child_frames.clear();
  }
  return success;
}

// Check if a script modified username is suitable for Password Manager to
// remember.
bool ScriptModifiedUsernameAcceptable(
    const std::u16string& value,
    const std::u16string& typed_value,
    const FieldDataManager* field_data_manager) {
  // The minimal size of a field value that will be substring-matched.
  constexpr size_t kMinMatchSize = 3u;
  const auto lowercase = base::i18n::ToLower(value);
  const auto typed_lowercase = base::i18n::ToLower(typed_value);
  // If the page-generated value is just a completion of the typed value, that's
  // likely acceptable.
  if (base::StartsWith(lowercase, typed_lowercase,
                       base::CompareCase::SENSITIVE)) {
    return true;
  }
  if (typed_lowercase.size() >= kMinMatchSize &&
      lowercase.find(typed_lowercase) != std::u16string::npos) {
    return true;
  }

  // If the page-generated value comes from user typed or autofilled values in
  // other fields, that's also likely OK.
  return field_data_manager->FindMachedValue(value);
}

// Trim the vector before sending it to the browser process to ensure we
// don't send too much data through the IPC.
void TrimStringVectorForIPC(std::vector<std::u16string>* strings) {
  // Limit the size of the vector.
  if (strings->size() > kMaxListSize)
    strings->resize(kMaxListSize);

  // Limit the size of the strings in the vector.
  for (auto& string : *strings) {
    if (string.length() > kMaxDataLength)
      string.resize(kMaxDataLength);
  }
}

// Build a map from entries in |form_control_renderer_ids| to their indices,
// for more efficient lookup.
base::flat_map<FieldRendererId, size_t> BuildRendererIdToIndex(
    const std::vector<FieldRendererId>& form_control_renderer_ids) {
  std::vector<std::pair<FieldRendererId, size_t>> items;
  items.reserve(form_control_renderer_ids.size());
  for (size_t i = 0; i < form_control_renderer_ids.size(); i++)
    items.emplace_back(form_control_renderer_ids[i], i);
  return base::flat_map<FieldRendererId, size_t>(std::move(items));
}

std::string GetAutocompleteAttribute(const WebElement& element) {
  static base::NoDestructor<WebString> kAutocomplete("autocomplete");
  std::string autocomplete_attribute =
      element.GetAttribute(*kAutocomplete).Utf8();
  if (autocomplete_attribute.size() > kMaxDataLength) {
    // Discard overly long attribute values to avoid DOS-ing the browser
    // process.  However, send over a default string to indicate that the
    // attribute was present.
    return "x-max-data-length-exceeded";
  }
  return autocomplete_attribute;
}

void FindFormElementUpShadowRoots(const WebElement& element,
                                  WebFormElement* found_form_element) {
  // If we are in shadowdom, then look to see if the host(s) are inside a form
  // element we can use.
  int levels_up = kMaxShadowLevelsUp;
  for (WebElement host = element.OwnerShadowHost(); !host.IsNull() && levels_up;
       host = host.OwnerShadowHost(), --levels_up) {
    for (WebNode parent = host; !parent.IsNull();
         parent = parent.ParentNode()) {
      if (parent.IsElementNode()) {
        WebElement parentElement = parent.To<WebElement>();
        if (parentElement.HasHTMLTagName("form")) {
          *found_form_element = parentElement.To<WebFormElement>();
          return;
        }
      }
    }
  }
}

}  // namespace

bool IsVisibleIframe(const WebElement& element) {
  DCHECK(element.HasHTMLTagName("iframe"));
  // It is common for not-humanly-visible elements to have very small yet
  // positive bounds. The threshold of 10 pixels is chosen rather arbitrarily.
  constexpr int kMinPixelSize = 10;
  gfx::Rect bounds = element.BoundsInViewport();
  return element.IsFocusable() && bounds.width() > kMinPixelSize &&
         bounds.height() > kMinPixelSize;
}

WebFormElement GetClosestAncestorFormElement(WebNode n) {
  while (!n.IsNull()) {
    if (n.IsElementNode() && n.To<WebElement>().HasHTMLTagName("form"))
      return n.To<WebFormElement>();
    n = n.ParentNode();
  }
  return WebFormElement();
}

bool IsDomPredecessor(const blink::WebNode& x,
                      const blink::WebNode& y,
                      const blink::WebNode& common_ancestor) {
  DCHECK(x.GetDocument() == y.GetDocument());
  DCHECK(x.GetDocument() == common_ancestor.GetDocument());
  // Paths are backwards: the last element is the top-most node.
  auto BuildPath = [&common_ancestor](blink::WebNode node) {
    DCHECK(common_ancestor != node);
    std::vector<WebNode> path;
    path.reserve(16);
    while (!node.IsNull() && node != common_ancestor) {
      path.push_back(node);
      node = path.back().ParentNode();
    }
    DCHECK(!path.empty());
    return path;
  };
  std::vector<blink::WebNode> x_path = BuildPath(x);
  std::vector<blink::WebNode> y_path = BuildPath(y);
  auto x_it = x_path.rbegin();
  auto y_it = y_path.rbegin();
  // Find the first different nodes in the paths. If such nodes exist, they are
  // siblings and their sibling order determines |x| and |y|'s relationship.
  while (x_it != x_path.rend() && y_it != y_path.rend()) {
    if (*x_it != *y_it) {
      DCHECK(x_it->ParentNode() == y_it->ParentNode());
      for (blink::WebNode n = *y_it; !n.IsNull(); n = n.NextSibling()) {
        if (n == *x_it)
          return false;
      }
      return true;
    }
    ++x_it;
    ++y_it;
  }
  // If the paths don't differ in a node, the shorter path indicates a
  // predecessor since DOM traversal is in-order.
  return x_it == x_path.rend() && y_it != y_path.rend();
}

void GetDataListSuggestions(const WebInputElement& element,
                            std::vector<std::u16string>* values,
                            std::vector<std::u16string>* labels) {
  for (const auto& option : element.FilteredDataListOptions()) {
    values->push_back(option.Value().Utf16());
    if (option.Value() != option.Label())
      labels->push_back(option.Label().Utf16());
    else
      labels->push_back(std::u16string());
  }
  TrimStringVectorForIPC(values);
  TrimStringVectorForIPC(labels);
}

bool ExtractFormData(const WebFormElement& form_element,
                     const FieldDataManager& field_data_manager,
                     FormData* data) {
  return WebFormElementToFormData(
      form_element, WebFormControlElement(), &field_data_manager,
      static_cast<form_util::ExtractMask>(form_util::EXTRACT_VALUE |
                                          form_util::EXTRACT_OPTION_TEXT |
                                          form_util::EXTRACT_OPTIONS),
      data, nullptr);
}

bool IsSomeControlElementVisible(
    blink::WebLocalFrame* frame,
    const std::set<FieldRendererId>& control_elements) {
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
      "Autofill.IsSomeControlElementVisibleDuration");

  WebDocument doc = frame->GetDocument();
  if (doc.IsNull())
    return false;

  if (base::FeatureList::IsEnabled(
          features::kAutofillUseUnassociatedListedElements)) {
    // Returns true iff at least one element from |fields| is visible and there
    // exists an element in |control_elements| with the same field renderer id.
    // The average case time complexity is O(N log M), where N is the number of
    // elements in |fields| and M is the number of elements in
    // |control_elements|.
    auto ContainsVisibleField =
        [&](const WebVector<WebFormControlElement>& fields) {
          return base::ranges::any_of(
              fields, [&](const WebFormControlElement& field) {
                return IsWebElementVisible(field) &&
                       base::Contains(control_elements,
                                      GetFieldRendererId(field));
              });
        };

    return base::ranges::any_of(doc.Forms(), ContainsVisibleField,
                                &WebFormElement::GetFormControlElements) ||
           ContainsVisibleField(doc.UnassociatedFormControls());
  } else {
    // This is basically a set intersection of |control_elements| and the form
    // controls on the website.
    // Iterating over all form controls on the website and checking their
    // existence in control_elements makes this O(|DOM| + N log M), where N is
    // the number of form controls on the website and M the number of elements
    // in |control_elements|.
    WebElementCollection elements = doc.All();

    for (WebElement element = elements.FirstItem(); !element.IsNull();
         element = elements.NextItem()) {
      if (!element.IsFormControlElement() || !IsWebElementVisible(element))
        continue;
      WebFormControlElement control = element.To<WebFormControlElement>();
      FieldRendererId field_renderer_id(control.UniqueRendererFormControlId());
      if (control_elements.find(field_renderer_id) != control_elements.end())
        return true;
    }
    return false;
  }
}

GURL GetCanonicalActionForForm(const WebFormElement& form) {
  WebString action = form.Action();
  if (action.IsNull())
    action = WebString("");  // missing 'action' attribute implies current URL.
  GURL full_action(form.GetDocument().CompleteURL(action));
  return StripAuthAndParams(full_action);
}

GURL GetDocumentUrlWithoutAuth(const WebDocument& document) {
  GURL::Replacements rep;
  rep.ClearUsername();
  rep.ClearPassword();
  GURL full_origin(document.Url());
  return full_origin.ReplaceComponents(rep);
}

bool IsMonthInput(const WebInputElement* element) {
  static base::NoDestructor<WebString> kMonth("month");
  return element && !element->IsNull() &&
         element->FormControlTypeForAutofill() == *kMonth;
}

// All text fields, including password fields, should be extracted.
bool IsTextInput(const WebInputElement* element) {
  return element && !element->IsNull() && element->IsTextField();
}

bool IsSelectElement(const WebFormControlElement& element) {
  // Static for improved performance.
  static base::NoDestructor<WebString> kSelectOne("select-one");
  return !element.IsNull() &&
         element.FormControlTypeForAutofill() == *kSelectOne;
}

bool IsTextAreaElement(const WebFormControlElement& element) {
  // Static for improved performance.
  static base::NoDestructor<WebString> kTextArea("textarea");
  return !element.IsNull() &&
         element.FormControlTypeForAutofill() == *kTextArea;
}

bool IsCheckableElement(const WebInputElement* element) {
  if (!element || element->IsNull())
    return false;

  return element->IsCheckbox() || element->IsRadioButton();
}

bool IsAutofillableInputElement(const WebInputElement* element) {
  return IsTextInput(element) || IsMonthInput(element) ||
         IsCheckableElement(element);
}

bool IsAutofillableElement(const WebFormControlElement& element) {
  const WebInputElement* input_element = ToWebInputElement(&element);
  return IsAutofillableInputElement(input_element) ||
         IsSelectElement(element) || IsTextAreaElement(element);
}

bool IsWebElementVisible(const blink::WebElement& element) {
  return element.IsFocusable();
}

std::u16string GetFormIdentifier(const WebFormElement& form) {
  std::u16string identifier = form.GetName().Utf16();
  if (identifier.empty())
    identifier = form.GetIdAttribute().Utf16();
  return identifier;
}

FormRendererId GetFormRendererId(const blink::WebFormElement& form) {
  return form.IsNull() ? FormRendererId()
                       : FormRendererId(form.UniqueRendererFormId());
}

FieldRendererId GetFieldRendererId(const blink::WebFormControlElement& field) {
  DCHECK(!field.IsNull());
  return FieldRendererId(field.UniqueRendererFormControlId());
}

base::i18n::TextDirection GetTextDirectionForElement(
    const blink::WebFormControlElement& element) {
  // Use 'text-align: left|right' if set or 'direction' otherwise.
  // See https://crbug.com/482339
  base::i18n::TextDirection direction = element.DirectionForFormData() == "rtl"
                                            ? base::i18n::RIGHT_TO_LEFT
                                            : base::i18n::LEFT_TO_RIGHT;
  if (element.AlignmentForFormData() == "left")
    direction = base::i18n::LEFT_TO_RIGHT;
  else if (element.AlignmentForFormData() == "right")
    direction = base::i18n::RIGHT_TO_LEFT;
  return direction;
}

std::vector<blink::WebFormControlElement> ExtractAutofillableElementsFromSet(
    const WebVector<WebFormControlElement>& control_elements) {
  std::vector<blink::WebFormControlElement> autofillable_elements;
  for (const auto& element : control_elements) {
    if (!IsAutofillableElement(element))
      continue;

    autofillable_elements.push_back(element);
  }
  return autofillable_elements;
}

std::vector<WebFormControlElement> ExtractAutofillableElementsInForm(
    const WebFormElement& form_element) {
  return ExtractAutofillableElementsFromSet(
      form_element.GetFormControlElements());
}

void WebFormControlElementToFormField(
    FormRendererId form_renderer_id,
    const WebFormControlElement& element,
    const FieldDataManager* field_data_manager,
    ExtractMask extract_mask,
    FormFieldData* field,
    ShadowFieldData* shadow_data) {
  DCHECK(field);
  DCHECK(!element.IsNull());
  DCHECK(element.GetDocument().GetFrame());
  static base::NoDestructor<WebString> kName("name");
  static base::NoDestructor<WebString> kRole("role");
  static base::NoDestructor<WebString> kPlaceholder("placeholder");
  static base::NoDestructor<WebString> kClass("class");

  // Save both id and name attributes, if present. If there is only one of them,
  // it will be saved to |name|. See HTMLFormControlElement::nameForAutofill.
  field->name = element.NameForAutofill().Utf16();
  field->id_attribute = element.GetIdAttribute().Utf16();
  field->name_attribute = element.GetAttribute(*kName).Utf16();
  field->unique_renderer_id = GetFieldRendererId(element);
  field->host_form_id = form_renderer_id;
  field->form_control_ax_id = element.GetAxId();
  field->form_control_type = element.FormControlTypeForAutofill().Utf8();
  field->autocomplete_attribute = GetAutocompleteAttribute(element);
  if (base::LowerCaseEqualsASCII(element.GetAttribute(*kRole).Utf16(),
                                 "presentation")) {
    field->role = FormFieldData::RoleAttribute::kPresentation;
  }

  field->placeholder = element.GetAttribute(*kPlaceholder).Utf16();
  if (element.HasAttribute(*kClass))
    field->css_classes = element.GetAttribute(*kClass).Utf16();

  const FieldRendererId renderer_id(element.UniqueRendererFormControlId());
  if (field_data_manager && field_data_manager->HasFieldData(renderer_id)) {
    field->properties_mask =
        field_data_manager->GetFieldPropertiesMask(renderer_id);
  }

  field->aria_label = GetAriaLabel(element.GetDocument(), element);
  field->aria_description = GetAriaDescription(element.GetDocument(), element);

  // Traverse up through shadow hosts to see if we can gather missing fields.
  WebFormElement form_element_up_shadow_hosts;
  FindFormElementUpShadowRoots(element, &form_element_up_shadow_hosts);
  int levels_up = kMaxShadowLevelsUp;
  for (WebElement host = element.OwnerShadowHost();
       !host.IsNull() && levels_up &&
       (!form_element_up_shadow_hosts.IsNull() &&
        form_element_up_shadow_hosts.OwnerShadowHost() != host);
       host = host.OwnerShadowHost(), --levels_up) {
    std::u16string shadow_host_id = host.GetIdAttribute().Utf16();
    if (shadow_data && !shadow_host_id.empty())
      shadow_data->shadow_host_id_attributes.push_back(shadow_host_id);
    std::u16string shadow_host_name = host.GetAttribute(*kName).Utf16();
    if (shadow_data && !shadow_host_name.empty())
      shadow_data->shadow_host_name_attributes.push_back(shadow_host_name);

    if (field->id_attribute.empty())
      field->id_attribute = host.GetIdAttribute().Utf16();
    if (field->name_attribute.empty())
      field->name_attribute = host.GetAttribute(*kName).Utf16();
    if (field->name.empty()) {
      field->name = field->name_attribute.empty() ? field->id_attribute
                                                  : field->name_attribute;
    }
    if (field->autocomplete_attribute.empty())
      field->autocomplete_attribute = GetAutocompleteAttribute(host);
    if (field->css_classes.empty() && host.HasAttribute(*kClass))
      field->css_classes = host.GetAttribute(*kClass).Utf16();
    if (field->aria_label.empty())
      field->aria_label = GetAriaLabel(host.GetDocument(), host);
    if (field->aria_description.empty())
      field->aria_description = GetAriaDescription(host.GetDocument(), host);
  }

  if (!IsAutofillableElement(element))
    return;

  const WebInputElement* input_element = ToWebInputElement(&element);
  if (IsAutofillableInputElement(input_element) || IsTextAreaElement(element) ||
      IsSelectElement(element)) {
    // The browser doesn't need to differentiate between preview and autofill.
    field->is_autofilled = element.IsAutofilled();
    field->is_focusable = IsWebElementVisible(element);
    field->should_autocomplete = element.AutoComplete();

    field->text_direction = GetTextDirectionForElement(element);
    field->is_enabled = element.IsEnabled();
    field->is_readonly = element.IsReadOnly();
  }

  if (IsAutofillableInputElement(input_element)) {
    if (IsTextInput(input_element))
      field->max_length = input_element->MaxLength();

    SetCheckStatus(field, IsCheckableElement(input_element),
                   input_element->IsChecked());
  } else if (IsTextAreaElement(element)) {
    // Nothing more to do in this case.
  } else if (extract_mask & EXTRACT_OPTIONS) {
    // Set option strings on the field if available.
    DCHECK(IsSelectElement(element));
    const WebSelectElement select_element = element.ToConst<WebSelectElement>();
    GetOptionStringsFromElement(select_element, &field->options);
  }
  if (extract_mask & EXTRACT_BOUNDS) {
    if (auto* local_frame = element.GetDocument().GetFrame()) {
      if (auto* render_frame =
              content::RenderFrame::FromWebFrame(local_frame)) {
        field->bounds = render_frame->ElementBoundsInWindow(element);
      }
    }
  }
  if (extract_mask & EXTRACT_DATALIST) {
    if (auto* input = blink::ToWebInputElement(&element)) {
      GetDataListSuggestions(*input, &field->datalist_values,
                             &field->datalist_labels);
    }
  }

  if (!(extract_mask & EXTRACT_VALUE))
    return;

  std::u16string value = element.Value().Utf16();

  if (IsSelectElement(element) && (extract_mask & EXTRACT_OPTION_TEXT)) {
    const WebSelectElement select_element = element.ToConst<WebSelectElement>();
    // Convert the |select_element| value to text if requested.
    WebVector<WebElement> list_items = select_element.GetListItems();
    for (const auto& list_item : list_items) {
      if (IsOptionElement(list_item)) {
        const WebOptionElement option_element =
            list_item.ToConst<WebOptionElement>();
        if (option_element.Value().Utf16() == value) {
          value = option_element.GetText().Utf16();
          break;
        }
      }
    }
  }

  // Constrain the maximum data length to prevent a malicious site from DOS'ing
  // the browser: http://crbug.com/49332
  TruncateString(&value, kMaxDataLength);

  field->value = value;

  // If the field was autofilled or the user typed into it, check the value
  // stored in |field_data_manager| against the value property of the DOM
  // element. If they differ, then the scripts on the website modified the
  // value afterwards. Store the original value as the |typed_value|, unless
  // this is one of recognised situations when the site-modified value is more
  // useful for filling.
  if (field_data_manager &&
      field->properties_mask & (FieldPropertiesFlags::kUserTyped |
                                FieldPropertiesFlags::kAutofilled)) {
    const std::u16string user_input =
        field_data_manager->GetUserInput(GetFieldRendererId(element));

    // The typed value is preserved for all passwords. It is also preserved for
    // potential usernames, as long as the |value| is not deemed acceptable.
    if (field->form_control_type == "password" ||
        !ScriptModifiedUsernameAcceptable(value, user_input,
                                          field_data_manager)) {
      field->user_input = user_input;
    }
  }
}

bool WebFormElementToFormData(
    const blink::WebFormElement& form_element,
    const blink::WebFormControlElement& form_control_element,
    const FieldDataManager* field_data_manager,
    ExtractMask extract_mask,
    FormData* form,
    FormFieldData* field) {
  WebLocalFrame* frame = form_element.GetDocument().GetFrame();
  if (!frame)
    return false;

  form->name = GetFormIdentifier(form_element);
  form->unique_renderer_id = GetFormRendererId(form_element);
  form->action = GetCanonicalActionForForm(form_element);
  form->is_action_empty =
      form_element.Action().IsNull() || form_element.Action().IsEmpty();
  form->main_frame_origin = url::Origin();
  // If the completed URL is not valid, just use the action we get from
  // WebKit.
  if (!form->action.is_valid())
    form->action = GURL(blink::WebStringToGURL(form_element.Action()));

  std::vector<WebElement> owned_iframes;
  if (base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    WebElementCollection iframes =
        form_element.GetElementsByHTMLTagName("iframe");
    for (WebElement iframe = iframes.FirstItem(); !iframe.IsNull();
         iframe = iframes.NextItem()) {
      if (GetClosestAncestorFormElement(iframe) == form_element &&
          IsVisibleIframe(iframe)) {
        owned_iframes.push_back(iframe);
      }
    }
  }

  std::vector<blink::WebElement> dummy_fieldset;
  return FormOrFieldsetsToFormData(
      frame, &form_element, &form_control_element, dummy_fieldset,
      form_element.GetFormControlElements(), owned_iframes, field_data_manager,
      extract_mask, form, field);
}

std::vector<WebFormControlElement>
GetUnownedFormFieldElementsWithListedElements(
    const WebDocument& document,
    std::vector<WebElement>* fieldsets) {
  std::vector<WebFormControlElement> unowned_form_field_elements =
      document.UnassociatedFormControls().ReleaseVector();
  if (fieldsets) {
    for (const auto& element : unowned_form_field_elements) {
      if (element.HasHTMLTagName("fieldset") &&
          !IsElementInsideFormOrFieldSet(element,
                                         /*consider_fieldset_tags=*/true)) {
        fieldsets->push_back(element);
      }
    }
  }
  return unowned_form_field_elements;
}

std::vector<WebFormControlElement> GetUnownedFormFieldElements(
    const WebDocument& document,
    std::vector<WebElement>* fieldsets) {
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS("Autofill.GetUnownedFormFieldsDuration");
  if (base::FeatureList::IsEnabled(
          features::kAutofillUseUnassociatedListedElements)) {
    return GetUnownedFormFieldElementsWithListedElements(document, fieldsets);
  }
  std::vector<WebFormControlElement> unowned_fieldset_children;
  const WebElementCollection& elements = document.All();
  for (WebElement element = elements.FirstItem(); !element.IsNull();
       element = elements.NextItem()) {
    if (element.IsFormControlElement()) {
      WebFormControlElement control = element.To<WebFormControlElement>();
      if (control.Form().IsNull())
        unowned_fieldset_children.push_back(control);
    }

    if (fieldsets && element.HasHTMLTagName("fieldset") &&
        !IsElementInsideFormOrFieldSet(element,
                                       /*consider_fieldset_tags=*/true)) {
      fieldsets->push_back(element);
    }
  }
  return unowned_fieldset_children;
}

std::vector<WebFormControlElement> GetUnownedAutofillableFormFieldElements(
    const WebDocument& document,
    std::vector<WebElement>* fieldsets) {
  return ExtractAutofillableElementsFromSet(
      GetUnownedFormFieldElements(document, fieldsets));
}

std::vector<WebElement> GetUnownedIframeElements(const WebDocument& document) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes))
    return {};

  std::vector<WebElement> unowned_iframes;
  WebElementCollection iframes = document.GetElementsByHTMLTagName("iframe");
  for (WebElement iframe = iframes.FirstItem(); !iframe.IsNull();
       iframe = iframes.NextItem()) {
    if (IsVisibleIframe(iframe) &&
        GetClosestAncestorFormElement(iframe).IsNull()) {
      unowned_iframes.push_back(iframe);
    }
  }
  return unowned_iframes;
}

bool UnownedFormElementsAndFieldSetsToFormData(
    const std::vector<blink::WebElement>& fieldsets,
    const std::vector<blink::WebFormControlElement>& control_elements,
    const std::vector<blink::WebElement>& iframe_elements,
    const blink::WebFormControlElement* element,
    const blink::WebDocument& document,
    const FieldDataManager* field_data_manager,
    ExtractMask extract_mask,
    FormData* form,
    FormFieldData* field) {
  blink::WebLocalFrame* frame = document.GetFrame();
  if (!frame)
    return false;

  form->unique_renderer_id = FormRendererId();
  form->main_frame_origin = url::Origin();

  form->is_form_tag = false;

  return FormOrFieldsetsToFormData(
      frame, nullptr, element, fieldsets, control_elements, iframe_elements,
      field_data_manager, extract_mask, form, field);
}

bool FindFormAndFieldForFormControlElement(
    const WebFormControlElement& element,
    const FieldDataManager* field_data_manager,
    ExtractMask extract_mask,
    FormData* form,
    FormFieldData* field) {
  DCHECK(!element.IsNull());

  if (!IsAutofillableElement(element))
    return false;

  extract_mask =
      static_cast<ExtractMask>(EXTRACT_VALUE | EXTRACT_OPTIONS | extract_mask);
  WebFormElement form_element = element.Form();

  if (form_element.IsNull())
    FindFormElementUpShadowRoots(element, &form_element);

  if (form_element.IsNull()) {
    // No associated form, try the synthetic form for unowned form elements.
    WebDocument document = element.GetDocument();
    std::vector<WebElement> fieldsets;
    std::vector<WebFormControlElement> control_elements =
        GetUnownedAutofillableFormFieldElements(document, &fieldsets);
    std::vector<WebElement> iframe_elements =
        GetUnownedIframeElements(document);
    return UnownedFormElementsAndFieldSetsToFormData(
        fieldsets, control_elements, iframe_elements, &element, document,
        field_data_manager, extract_mask, form, field);
  }

  return WebFormElementToFormData(form_element, element, field_data_manager,
                                  extract_mask, form, field);
}

bool FindFormAndFieldForFormControlElement(
    const WebFormControlElement& element,
    const FieldDataManager* field_data_manager,
    FormData* form,
    FormFieldData* field) {
  return FindFormAndFieldForFormControlElement(
      element, field_data_manager, form_util::EXTRACT_NONE, form, field);
}

std::vector<WebFormControlElement> FillOrPreviewForm(
    const FormData& form,
    const WebFormControlElement& element,
    mojom::RendererFormDataAction action) {
  WebFormElement form_element = element.Form();
  if (form_element.IsNull())
    FindFormElementUpShadowRoots(element, &form_element);

  Callback callback = action == mojom::RendererFormDataAction::kPreview
                          ? &PreviewFormField
                          : &FillFormField;
  if (form_element.IsNull()) {
    return ForEachMatchingUnownedFormField(
        element, form, FILTER_ALL_NON_EDITABLE_ELEMENTS, action, callback);
  }
  return ForEachMatchingFormField(form_element, element, form,
                                  FILTER_ALL_NON_EDITABLE_ELEMENTS, action,
                                  callback);
}

void ClearPreviewedElements(
    std::vector<blink::WebFormControlElement>& previewed_elements,
    const WebFormControlElement& initiating_element,
    blink::WebAutofillState old_autofill_state) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillHighlightOnlyChangedValuesInPreviewMode)) {
    // If this is a synthetic form, get the unowned form elements. Otherwise,
    // get all element associated with the form of the initiated field.
    std::vector<WebFormControlElement> form_elements =
        initiating_element.Form().IsNull()
            ? GetUnownedFormFieldElements(initiating_element.GetDocument(),
                                          nullptr)
            : ExtractAutofillableElementsInForm(initiating_element.Form());

    // Allow the highlighting of already autofilled fields again.
    for (auto& element : form_elements) {
      element.SetPreventHighlightingOfAutofilledFields(false);
    }
  }

  for (WebFormControlElement& control_element : previewed_elements) {
    if (control_element.IsNull())
      continue;

    // Only clear previewed fields.
    if (control_element.GetAutofillState() != WebAutofillState::kPreviewed)
      continue;

    if (control_element.SuggestedValue().IsEmpty())
      continue;

    // Clear the suggested value. For the initiating node, also restore the
    // original value.
    WebInputElement* input_element = ToWebInputElement(&control_element);
    if (IsTextInput(input_element) || IsMonthInput(input_element) ||
        IsTextAreaElement(control_element)) {
      control_element.SetSuggestedValue(WebString());
      bool is_initiating_node = (initiating_element == control_element);
      if (is_initiating_node) {
        // Clearing the suggested value in the focused node (above) can cause
        // selection to be lost. We force selection range to restore the text
        // cursor.
        int length = control_element.Value().length();
        control_element.SetSelectionRange(length, length);
        control_element.SetAutofillState(old_autofill_state);
      } else {
        control_element.SetAutofillState(WebAutofillState::kNotFilled);
      }
    } else {
      control_element.SetSuggestedValue(WebString());
      control_element.SetAutofillState(WebAutofillState::kNotFilled);
    }
  }
}

bool IsOwnedByFrame(const WebNode& node, content::RenderFrame* frame) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes))
    return true;
  if (node.IsNull() || !frame)
    return false;
  const blink::WebDocument& doc = node.GetDocument();
  blink::WebLocalFrame* node_frame = !doc.IsNull() ? doc.GetFrame() : nullptr;
  blink::WebLocalFrame* expected_frame = frame->GetWebFrame();
  return expected_frame && node_frame &&
         expected_frame->GetLocalFrameToken() ==
             node_frame->GetLocalFrameToken();
}

bool IsWebpageEmpty(const blink::WebLocalFrame* frame) {
  blink::WebDocument document = frame->GetDocument();

  return IsWebElementEmpty(document.Head()) &&
         IsWebElementEmpty(document.Body());
}

bool IsWebElementEmpty(const blink::WebElement& root) {
  static base::NoDestructor<WebString> kScript("script");
  static base::NoDestructor<WebString> kMeta("meta");
  static base::NoDestructor<WebString> kTitle("title");

  if (root.IsNull())
    return true;

  for (WebNode child = root.FirstChild(); !child.IsNull();
       child = child.NextSibling()) {
    if (child.IsTextNode() && !base::ContainsOnlyChars(child.NodeValue().Utf8(),
                                                       base::kWhitespaceASCII))
      return false;

    if (!child.IsElementNode())
      continue;

    WebElement element = child.To<WebElement>();
    if (!element.HasHTMLTagName(*kScript) && !element.HasHTMLTagName(*kMeta) &&
        !element.HasHTMLTagName(*kTitle))
      return false;
  }
  return true;
}

void PreviewSuggestion(const std::u16string& suggestion,
                       const std::u16string& user_input,
                       blink::WebFormControlElement* input_element) {
  size_t selection_start = user_input.length();
  if (IsFeatureSubstringMatchEnabled()) {
    size_t offset = GetTextSelectionStart(suggestion, user_input, false);
    // Zero selection start is for password manager, which can show usernames
    // that do not begin with the user input value.
    selection_start = (offset == std::u16string::npos) ? 0 : offset;
  }

  input_element->SetSelectionRange(selection_start, suggestion.length());
}

std::u16string FindChildText(const WebNode& node) {
  return FindChildTextWithIgnoreList(node, std::set<WebNode>());
}

ButtonTitleList GetButtonTitles(const WebFormElement& web_form,
                                const WebDocument& document,
                                ButtonTitlesCache* button_titles_cache) {
  if (!button_titles_cache) {
    // Button titles scraping is disabled for this form.
    return ButtonTitleList();
  }

  // True if the cache has no entry for |web_form|.
  bool cache_miss = true;
  // Iterator pointing to the entry for |web_form| if the entry for |web_form|
  // is found.
  ButtonTitlesCache::iterator form_position;
  std::tie(form_position, cache_miss) = button_titles_cache->emplace(
      GetFormRendererId(web_form), ButtonTitleList());
  if (!cache_miss)
    return form_position->second;

  ButtonTitleList button_titles;
  DCHECK(!web_form.IsNull() || !document.IsNull());
  if (web_form.IsNull()) {
    const WebElement& body = document.Body();
    if (!body.IsNull()) {
      SCOPED_UMA_HISTOGRAM_TIMER(
          "PasswordManager.ButtonTitlePerformance.NoFormTag");
      button_titles = InferButtonTitlesForForm(body);
    }
  } else {
    SCOPED_UMA_HISTOGRAM_TIMER(
        "PasswordManager.ButtonTitlePerformance.HasFormTag");
    button_titles = InferButtonTitlesForForm(web_form);
  }
  form_position->second = std::move(button_titles);
  return form_position->second;
}

std::u16string FindChildTextWithIgnoreListForTesting(
    const WebNode& node,
    const std::set<WebNode>& divs_to_skip) {
  return FindChildTextWithIgnoreList(node, divs_to_skip);
}

bool InferLabelForElementForTesting(const WebFormControlElement& element,
                                    std::u16string* label,
                                    FormFieldData::LabelSource* label_source) {
  return InferLabelForElement(element, label, label_source);
}

WebFormElement FindFormByUniqueRendererId(const WebDocument& doc,
                                          FormRendererId form_renderer_id) {
  for (const auto& form : doc.Forms()) {
    if (GetFormRendererId(form) == form_renderer_id)
      return form;
  }
  return WebFormElement();
}

WebFormControlElement FindFormControlElementByUniqueRendererId(
    const WebDocument& doc,
    FieldRendererId queried_form_control,
    absl::optional<FormRendererId> form_to_be_searched /*= absl::nullopt*/) {
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
      "Autofill.FindFormControlElementByUniqueRendererIdDuration");

  if (base::FeatureList::IsEnabled(
          features::kAutofillUseUnassociatedListedElements)) {
    auto FindField = [&](const WebVector<WebFormControlElement>& fields) {
      auto it =
          base::ranges::find(fields, queried_form_control, GetFieldRendererId);
      return it != fields.end() ? *it : WebFormControlElement();
    };

    auto IsCandidate = [&form_to_be_searched](
                           const FormRendererId& expected_form_renderer_id) {
      return !form_to_be_searched.has_value() ||
             form_to_be_searched.value() == expected_form_renderer_id;
    };

    if (IsCandidate(FormRendererId())) {
      // Search the unowned form.
      WebFormControlElement e = FindField(doc.UnassociatedFormControls());
      if (form_to_be_searched == FormRendererId() || !e.IsNull())
        return e;
    }
    for (const WebFormElement& form : doc.Forms()) {
      // If the |form_to_be_searched| is specified, skip this form if it is not
      // the right one.
      if (!IsCandidate(GetFormRendererId(form))) {
        continue;
      }
      WebFormControlElement e = FindField(form.GetFormControlElements());
      if (form_to_be_searched == GetFormRendererId(form) || !e.IsNull())
        return e;
    }
    return WebFormControlElement();
  } else {
    WebElementCollection elements = doc.All();

    for (WebElement element = elements.FirstItem(); !element.IsNull();
         element = elements.NextItem()) {
      if (!element.IsFormControlElement())
        continue;
      WebFormControlElement control = element.To<WebFormControlElement>();
      if (queried_form_control == GetFieldRendererId(control))
        return control;
    }
    return WebFormControlElement();
  }
}

std::vector<WebFormControlElement> FindFormControlElementsByUniqueRendererId(
    const WebDocument& doc,
    const std::vector<FieldRendererId>& queried_form_controls) {
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
      "Autofill.FindFormControlElementsByUniqueRendererIdDuration");

  std::vector<WebFormControlElement> result(queried_form_controls.size());
  auto renderer_id_to_index_map = BuildRendererIdToIndex(queried_form_controls);

  if (base::FeatureList::IsEnabled(
          features::kAutofillUseUnassociatedListedElements)) {
    auto AddToResultIfQueried = [&](const WebFormControlElement& field) {
      auto it = renderer_id_to_index_map.find(GetFieldRendererId(field));
      if (it != renderer_id_to_index_map.end())
        result[it->second] = field;
    };

    for (const auto& form : doc.Forms()) {
      for (const auto& field : form.GetFormControlElements())
        AddToResultIfQueried(field);
    }
    for (const auto& field : doc.UnassociatedFormControls()) {
      AddToResultIfQueried(field);
    }
    return result;
  } else {
    WebElementCollection elements = doc.All();

    for (WebElement element = elements.FirstItem(); !element.IsNull();
         element = elements.NextItem()) {
      if (!element.IsFormControlElement())
        continue;
      WebFormControlElement control = element.To<WebFormControlElement>();
      auto it = renderer_id_to_index_map.find(GetFieldRendererId(control));
      if (it == renderer_id_to_index_map.end())
        continue;
      result[it->second] = control;
    }
    return result;
  }
}

std::vector<WebFormControlElement> FindFormControlElementsByUniqueRendererId(
    const WebDocument& doc,
    FormRendererId form_renderer_id,
    const std::vector<FieldRendererId>& queried_form_controls) {
  std::vector<WebFormControlElement> result(queried_form_controls.size());
  WebFormElement form = FindFormByUniqueRendererId(doc, form_renderer_id);
  if (form.IsNull())
    return result;

  auto renderer_id_to_index_map = BuildRendererIdToIndex(queried_form_controls);

  for (const auto& field : form.GetFormControlElements()) {
    auto it = renderer_id_to_index_map.find(GetFieldRendererId(field));
    if (it == renderer_id_to_index_map.end())
      continue;
    result[it->second] = field;
  }
  return result;
}

namespace {

// Returns the coalesced child of the elements who's ids are found in
// |id_list|.
//
// For example, given this document...
//
//      <div id="billing">Billing</div>
//      <div>
//        <div id="name">Name</div>
//        <input id="field1" type="text" aria-labelledby="billing name"/>
//     </div>
//     <div>
//       <div id="address">Address</div>
//       <input id="field2" type="text" aria-labelledby="billing address"/>
//     </div>
//
// The coalesced text by the id_list found in the aria-labelledby attribute
// of the field1 input element would be "Billing Name" and for field2 it would
// be "Billing Address".
std::u16string CoalesceTextByIdList(const WebDocument& document,
                                    const WebString& id_list) {
  const std::u16string kSpace = u" ";

  std::u16string text;
  std::u16string id_list_utf16 = id_list.Utf16();
  for (const auto& id : base::SplitStringPiece(
           id_list_utf16, base::kWhitespaceUTF16, base::KEEP_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    auto node = document.GetElementById(WebString(id.data(), id.length()));
    if (!node.IsNull()) {
      std::u16string child_text = FindChildText(node);
      if (!child_text.empty()) {
        if (!text.empty())
          text.append(kSpace);
        text.append(child_text);
      }
    }
  }
  base::TrimWhitespace(text, base::TRIM_ALL, &text);
  return text;
}

}  // namespace

std::u16string GetAriaLabel(const blink::WebDocument& document,
                            const WebElement& element) {
  static const base::NoDestructor<WebString> kAriaLabelledBy("aria-labelledby");
  if (element.HasAttribute(*kAriaLabelledBy)) {
    std::u16string text =
        CoalesceTextByIdList(document, element.GetAttribute(*kAriaLabelledBy));
    if (!text.empty())
      return text;
  }

  static const base::NoDestructor<WebString> kAriaLabel("aria-label");
  if (element.HasAttribute(*kAriaLabel))
    return element.GetAttribute(*kAriaLabel).Utf16();

  return std::u16string();
}

std::u16string GetAriaDescription(const blink::WebDocument& document,
                                  const WebElement& element) {
  static const base::NoDestructor<WebString> kAriaDescribedBy(
      "aria-describedby");
  return CoalesceTextByIdList(document,
                              element.GetAttribute(*kAriaDescribedBy));
}

}  // namespace form_util
}  // namespace autofill
