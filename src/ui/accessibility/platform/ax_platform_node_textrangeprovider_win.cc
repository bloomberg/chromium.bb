// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_textrangeprovider_win.h"

#include <utility>
#include <vector>

#include "base/i18n/string_search.h"
#include "base/win/scoped_variant.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

#define UIA_VALIDATE_TEXTRANGEPROVIDER_CALL()                        \
  if (!owner() || !owner()->GetDelegate() || !start_->GetAnchor() || \
      !end_->GetAnchor())                                            \
    return UIA_E_ELEMENTNOTAVAILABLE;

namespace ui {

AXPlatformNodeTextRangeProviderWin::AXPlatformNodeTextRangeProviderWin() {
  DVLOG(1) << __func__;
}

AXPlatformNodeTextRangeProviderWin::~AXPlatformNodeTextRangeProviderWin() {}

HRESULT AXPlatformNodeTextRangeProviderWin::CreateTextRangeProvider(
    ui::AXPlatformNodeWin* owner,
    AXPositionInstance start,
    AXPositionInstance end,
    ITextRangeProvider** provider) {
  CComObject<AXPlatformNodeTextRangeProviderWin>* text_range_provider = nullptr;
  HRESULT hr = CComObject<AXPlatformNodeTextRangeProviderWin>::CreateInstance(
      &text_range_provider);
  if (SUCCEEDED(hr)) {
    DCHECK(text_range_provider);
    text_range_provider->owner_ = owner;
    text_range_provider->start_ = std::move(start);
    text_range_provider->end_ = std::move(end);
    text_range_provider->AddRef();
    *provider = text_range_provider;
  }

  return hr;
}

//
// ITextRangeProvider methods.
//
STDMETHODIMP AXPlatformNodeTextRangeProviderWin::Clone(
    ITextRangeProvider** clone) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_CLONE);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL();
  *clone = nullptr;

  return CreateTextRangeProvider(owner_, start_->Clone(), end_->Clone(), clone);
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::Compare(
    ITextRangeProvider* other,
    BOOL* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_COMPARE);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL();

  CComPtr<AXPlatformNodeTextRangeProviderWin> other_provider;
  if (other->QueryInterface(&other_provider) != S_OK) {
    return UIA_E_INVALIDOPERATION;
  }

  *result = FALSE;
  if (*start_ == *(other_provider->start_) &&
      *end_ == *(other_provider->end_)) {
    *result = TRUE;
  }

  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::CompareEndpoints(
    TextPatternRangeEndpoint this_endpoint,
    ITextRangeProvider* other,
    TextPatternRangeEndpoint other_endpoint,
    int* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_COMPAREENDPOINTS);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL();

  CComPtr<AXPlatformNodeTextRangeProviderWin> other_provider;
  if (other->QueryInterface(&other_provider) != S_OK) {
    return UIA_E_INVALIDOPERATION;
  }

  const AXPositionInstance& this_provider_endpoint =
      (this_endpoint == TextPatternRangeEndpoint_Start) ? start_ : end_;

  const AXPositionInstance& other_provider_endpoint =
      (other_endpoint == TextPatternRangeEndpoint_Start)
          ? other_provider->start_
          : other_provider->end_;

  if (*this_provider_endpoint < *other_provider_endpoint) {
    *result = -1;
  } else if (*this_provider_endpoint > *other_provider_endpoint) {
    *result = 1;
  } else {
    *result = 0;
  }

  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::ExpandToEnclosingUnit(
    TextUnit unit) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_EXPANDTOENCLOSINGUNIT);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL();

  // Determine if start is on a boundary of the specified TextUnit, if it is
  // not, move backwards until it is. Move the end forwards from start until it
  // is on the next TextUnit boundary, if one exists.
  switch (unit) {
    case TextUnit_Character: {
      // For characters, start_ will always be on a TextUnit boundary.
      // Thus, end_ is the only position that needs to be moved.
      AXNodePosition::AXPositionInstance next =
          start_->CreateNextCharacterPosition(
              ui::AXBoundaryBehavior::CrossBoundary);
      if (!next->IsNullPosition())
        end_ = next->Clone();
      return S_OK;
    }
    case TextUnit_Format:
      return E_NOTIMPL;
    case TextUnit_Word:
      start_ = start_->CreatePreviousWordStartPosition(
          ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary);
      end_ = start_->CreateNextWordEndPosition(
          ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary);
      return S_OK;
    case TextUnit_Line:
      start_ = start_->CreatePreviousLineStartPosition(
          ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary);
      end_ = start_->CreateNextLineEndPosition(
          ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary);
      return S_OK;
    case TextUnit_Paragraph:
      return E_NOTIMPL;
    // Since web content is not paginated, TextUnit_Page is not supported.
    // Substituting it by the next larger unit: TextUnit_Document.
    case TextUnit_Page:
    case TextUnit_Document:
      start_ = start_->CreatePositionAtStartOfDocument()->AsLeafTextPosition();
      end_ = start_->CreatePositionAtEndOfDocument();
      return S_OK;
    default:
      return UIA_E_NOTSUPPORTED;
  }
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::FindAttribute(
    TEXTATTRIBUTEID text_attribute_id,
    VARIANT attribute_val,
    BOOL is_backward,
    ITextRangeProvider** result) {
  // Algorithm description:
  // Performs linear search. Expand forward or backward to fetch the first
  // instance of a sub text range that matches the attribute and its value.
  // |is_backward| determines the direction of our search.
  // |is_backward=true|, we search from the end of this text range to its
  // beginning.
  // |is_backward=false|, we search from the beginning of this text range to its
  // end.
  //
  // 1. Iterate through the vector of AXRanges in this text range in the
  //    direction denoted by |is_backward|.
  // 2. The |matched_range| is initially denoted as null since no range
  //    currently matches. We initialize |matched_range| to non-null value when
  //    we encounter the first AXRange instance that matches in attribute and
  //    value. We then set the |matched_range_start| to be the start (anchor) of
  //    the current AXRange, and |matched_range_end| to be the end (focus) of
  //    the current AXRange.
  // 3. If the current AXRange we are iterating on continues to match attribute
  //    and value, we extend |matched_range| in one of the two following ways:
  //    - If |is_backward=true|, we extend the |matched_range| by moving
  //      |matched_range_start| backward. We do so by setting
  //      |matched_range_start| to the start (anchor) of the current AXRange.
  //    - If |is_backward=false|, we extend the |matched_range| by moving
  //      |matched_range_end| forward. We do so by setting |matched_range_end|
  //      to the end (focus) of the current AXRange.
  // 4. We found a match when the current AXRange we are iterating on does not
  //    match the attribute and value and there is a previously matched range.
  //    The previously matched range is the final match we found.
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_FINDATTRIBUTE);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL();

  *result = nullptr;
  AXNodeRange range(start_->Clone(), end_->Clone());
  std::vector<AXNodeRange> anchors = range.GetAnchors();
  AXPositionInstance matched_range_start = nullptr, matched_range_end = nullptr;

  auto expand_match = [&matched_range_start, &matched_range_end, is_backward](
                          auto& current_start, auto& current_end) {
    // The current AXRange has the attribute and its value that we are looking
    // for, we expand the matched text range if a previously matched exists,
    // otherwise initialize a newly matched text range.
    if (matched_range_start != nullptr && matched_range_end != nullptr) {
      // Continue expanding the matched text range forward/backward based on
      // the search direction.
      if (is_backward)
        matched_range_start = current_start->Clone();
      else
        matched_range_end = current_end->Clone();
    } else {
      // Initialize the matched text range. The first AXRange instance that
      // matches the attribute and its value encountered.
      matched_range_start = current_start->Clone();
      matched_range_end = current_end->Clone();
    }
  };

  HRESULT hr_result =
      is_backward
          ? FindAttributeRange(text_attribute_id, attribute_val,
                               anchors.crbegin(), anchors.crend(), expand_match)
          : FindAttributeRange(text_attribute_id, attribute_val,
                               anchors.cbegin(), anchors.cend(), expand_match);
  if (FAILED(hr_result))
    return E_FAIL;

  if (matched_range_start != nullptr && matched_range_end != nullptr)
    return CreateTextRangeProvider(owner(), std::move(matched_range_start),
                                   std::move(matched_range_end), result);
  return S_OK;
}

template <typename AnchorIterator, typename ExpandMatchLambda>
HRESULT AXPlatformNodeTextRangeProviderWin::FindAttributeRange(
    const TEXTATTRIBUTEID text_attribute_id,
    VARIANT attribute_val,
    const AnchorIterator first,
    const AnchorIterator last,
    ExpandMatchLambda expand_match) {
  AXPlatformNodeWin* current_platform_node;
  bool is_match_found = false;

  for (auto it = first; it != last; ++it) {
    const auto& current_start = it->anchor();
    const auto& current_end = it->focus();

    DCHECK(current_start->GetAnchor() == current_end->GetAnchor());

    current_platform_node =
        static_cast<AXPlatformNodeWin*>(owner()->GetDelegate()->GetFromNodeID(
            current_start->GetAnchor()->id()));

    base::win::ScopedVariant current_attribute_value;
    if (FAILED(current_platform_node->GetTextAttributeValue(
            text_attribute_id, current_attribute_value.Receive())))
      return E_FAIL;

    if (VARCMP_EQ == VarCmp(&attribute_val, current_attribute_value.AsInput(),
                            LOCALE_USER_DEFAULT, 0)) {
      // When we encounter an AXRange instance that matches the attribute
      // and its value which we are looking for and no previously matched text
      // range exists, we expand or initialize the matched range.
      is_match_found = true;
      expand_match(current_start, current_end);
    } else if (is_match_found) {
      // When we encounter an AXRange instance that does not match the attribute
      // and its value which we are looking for and a previously matched text
      // range exists, the previously matched text range is the result we found.
      break;
    }
  }

  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::FindText(
    BSTR string,
    BOOL backwards,
    BOOL ignore_case,
    ITextRangeProvider** result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_FINDTEXT);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL();
  if (!result || !string)
    return E_INVALIDARG;

  *result = nullptr;
  base::string16 search_string(string);

  if (search_string.length() <= 0)
    return E_INVALIDARG;

  base::string16 text_range = GetString();
  size_t find_start;
  size_t find_length;
  if (base::i18n::StringSearch(search_string, text_range, &find_start,
                               &find_length, !ignore_case, !backwards)) {
    const AXPlatformNodeDelegate* delegate = owner()->GetDelegate();
    return CreateTextRangeProvider(
        owner_, delegate->CreateTextPositionAt(find_start),
        delegate->CreateTextPositionAt(find_start + find_length), result);
  }
  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::GetAttributeValue(
    TEXTATTRIBUTEID attribute_id,
    VARIANT* value) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_GETATTRIBUTEVALUE);

  AXNodeRange range(start_->Clone(), end_->Clone());
  std::vector<AXNodeRange> anchors = range.GetAnchors();

  if (anchors.empty())
    return UIA_E_ELEMENTNOTAVAILABLE;

  base::win::ScopedVariant attribute_value_variant;

  AXPlatformNodeDelegate* delegate = owner()->GetDelegate();

  for (auto&& current_range : anchors) {
    DCHECK(current_range.anchor()->GetAnchor() ==
           current_range.focus()->GetAnchor());

    AXPlatformNodeWin* platform_node = static_cast<AXPlatformNodeWin*>(
        delegate->GetFromNodeID(current_range.anchor()->GetAnchor()->id()));
    DCHECK(platform_node);
    if (!platform_node)
      continue;

    base::win::ScopedVariant current_variant;

    HRESULT hr = platform_node->GetTextAttributeValue(
        attribute_id, current_variant.Receive());
    if (FAILED(hr))
      return E_FAIL;

    if (attribute_value_variant.type() == VT_EMPTY) {
      attribute_value_variant.Reset(current_variant);
    } else if (0 != attribute_value_variant.Compare(current_variant)) {
      V_VT(value) = VT_UNKNOWN;
      return ::UiaGetReservedMixedAttributeValue(&V_UNKNOWN(value));
    }
  }

  *value = attribute_value_variant.Release();
  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::GetBoundingRectangles(
    SAFEARRAY** rectangles) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_GETBOUNDINGRECTANGLES);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL();

  *rectangles = nullptr;

  AXNodeRange range(start_->Clone(), end_->Clone());
  std::vector<AXNodeRange> anchors = range.GetAnchors();

  std::vector<gfx::Rect> rects;
  for (auto&& current_range : anchors) {
    std::vector<gfx::Rect> current_anchor_rects =
        current_range.GetScreenRects();
    // std::vector does not have a built-in way of appending another
    // std::vector. Using insert with iterators is the safest and most
    // performant way to accomplish this.
    rects.insert(rects.end(), current_anchor_rects.begin(),
                 current_anchor_rects.end());
  }

  // 4 array items per rect: left, top, width, height
  SAFEARRAY* safe_array = SafeArrayCreateVector(
      VT_R8 /* element type */, 0 /* lower bound */, rects.size() * 4);

  if (!safe_array)
    return E_OUTOFMEMORY;

  if (rects.size() > 0) {
    double* double_array = nullptr;
    HRESULT hr = SafeArrayAccessData(safe_array,
                                     reinterpret_cast<void**>(&double_array));

    if (SUCCEEDED(hr)) {
      for (size_t rect_index = 0; rect_index < rects.size(); rect_index++) {
        const gfx::Rect& rect = rects[rect_index];
        double_array[rect_index * 4] = rect.x();
        double_array[rect_index * 4 + 1] = rect.y();
        double_array[rect_index * 4 + 2] = rect.width();
        double_array[rect_index * 4 + 3] = rect.height();
      }
      hr = SafeArrayUnaccessData(safe_array);
    }

    if (FAILED(hr)) {
      DCHECK(safe_array);
      SafeArrayDestroy(safe_array);
      return E_FAIL;
    }
  }

  *rectangles = safe_array;

  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::GetEnclosingElement(
    IRawElementProviderSimple** element) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_GETENCLOSINGELEMENT);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL();

  AXPositionInstance common_ancestor = start_->LowestCommonAncestor(*end_);
  owner()
      ->GetDelegate()
      ->GetFromNodeID(common_ancestor->anchor_id())
      ->GetNativeViewAccessible()
      ->QueryInterface(IID_PPV_ARGS(element));

  DCHECK(*element);
  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::GetText(int max_count,
                                                         BSTR* text) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_GETTEXT);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL();

  // -1 is a valid value that signifies that the caller wants complete text.
  // Any other negative value is invalid.
  if (max_count < -1 || !text)
    return E_INVALIDARG;

  base::string16 full_text = GetString();
  if (!full_text.empty()) {
    size_t length = full_text.length();

    if (max_count != -1 && max_count < static_cast<int>(length))
      *text = SysAllocStringLen(full_text.c_str(), max_count);
    else
      *text = SysAllocStringLen(full_text.c_str(), length);
  } else {
    *text = SysAllocString(L"");
  }

  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::Move(TextUnit unit,
                                                      int count,
                                                      int* units_moved) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_MOVE);
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::MoveEndpointByUnit(
    TextPatternRangeEndpoint endpoint,
    TextUnit unit,
    int count,
    int* units_moved) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_MOVEENDPOINTBYUNIT);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL();

  // Per MSDN, MoveEndpointByUnit with zero count has no effect.
  if (count == 0) {
    *units_moved = 0;
    return S_OK;
  }

  bool is_start_endpoint = endpoint == TextPatternRangeEndpoint_Start;
  AXPositionInstance& position_to_move = is_start_endpoint ? start_ : end_;
  AXPositionInstance new_position;

  switch (unit) {
    case TextUnit_Character:
      new_position =
          MoveEndpointByCharacter(position_to_move, count, units_moved);
      break;
    case TextUnit_Format:
      return E_NOTIMPL;
    case TextUnit_Word:
      new_position = MoveEndpointByWord(position_to_move, is_start_endpoint,
                                        count, units_moved);
      break;
    case TextUnit_Line:
      new_position = MoveEndpointByLine(position_to_move, is_start_endpoint,
                                        count, units_moved);
      break;
    case TextUnit_Paragraph:
      return E_NOTIMPL;
    // Since web content is not paginated, TextUnit_Page is not supported.
    // Substituting it by the next larger unit: TextUnit_Document.
    case TextUnit_Page:
    case TextUnit_Document:
      new_position =
          MoveEndpointByDocument(position_to_move, count, units_moved);
      break;
    default:
      return UIA_E_NOTSUPPORTED;
  }

  position_to_move = std::move(new_position);

  // If the start was moved past the end, create a degenerate range with the end
  // equal to the start. Do the equivalent if the end moved past the start.
  if (*start_ > *end_) {
    if (is_start_endpoint)
      end_ = start_->Clone();
    else
      start_ = end_->Clone();
  }
  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::MoveEndpointByRange(
    TextPatternRangeEndpoint this_endpoint,
    ITextRangeProvider* other,
    TextPatternRangeEndpoint other_endpoint) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_MOVEENPOINTBYRANGE);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL();

  CComPtr<AXPlatformNodeTextRangeProviderWin> other_provider;
  if (other->QueryInterface(&other_provider) != S_OK) {
    return UIA_E_INVALIDOPERATION;
  }

  const AXPositionInstance& other_provider_endpoint =
      (other_endpoint == TextPatternRangeEndpoint_Start)
          ? other_provider->start_
          : other_provider->end_;

  if (this_endpoint == TextPatternRangeEndpoint_Start) {
    start_ = other_provider_endpoint->Clone();
    if (*start_ > *end_)
      end_ = start_->Clone();
  } else {
    end_ = other_provider_endpoint->Clone();
    if (*start_ > *end_)
      start_ = end_->Clone();
  }

  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::Select() {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_SELECT);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL();

  AXNodeRange range(start_->Clone(), end_->Clone());
  AXActionData action_data;
  action_data.anchor_node_id = range.anchor()->anchor_id();
  action_data.anchor_offset = range.anchor()->text_offset();
  action_data.focus_node_id = range.focus()->anchor_id();
  action_data.focus_offset = range.focus()->text_offset();
  action_data.action = ax::mojom::Action::kSetSelection;
  owner()->GetDelegate()->AccessibilityPerformAction(action_data);
  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::AddToSelection() {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_ADDTOSELECTION);
  return UIA_E_INVALIDOPERATION;  // not supporting disjoint text selections
}

STDMETHODIMP
AXPlatformNodeTextRangeProviderWin::RemoveFromSelection() {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_REMOVEFROMSELECTION);
  return UIA_E_INVALIDOPERATION;  // not supporting disjoint text selections
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::ScrollIntoView(
    BOOL align_to_top) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_SCROLLINTOVIEW);
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::GetChildren(
    SAFEARRAY** children) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_GETCHILDREN);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL();

  std::vector<gfx::NativeViewAccessible> descendants;

  AXPositionInstance common_ancestor =
      start_->LowestCommonAncestor(*end_.get());

  if (common_ancestor->GetAnchor()->child_count() > 0) {
    descendants = owner()
                      ->GetDelegate()
                      ->GetFromNodeID(common_ancestor->anchor_id())
                      ->GetDelegate()
                      ->GetDescendants();
  }

  SAFEARRAY* safe_array =
      SafeArrayCreateVector(VT_UNKNOWN, 0, descendants.size());

  if (!safe_array)
    return E_OUTOFMEMORY;

  if (safe_array->rgsabound->cElements != descendants.size()) {
    DCHECK(safe_array);
    SafeArrayDestroy(safe_array);
    return E_OUTOFMEMORY;
  }

  LONG i = 0;
  for (const gfx::NativeViewAccessible& descendant : descendants) {
    IRawElementProviderSimple* raw_provider;
    descendant->QueryInterface(IID_PPV_ARGS(&raw_provider));
    SafeArrayPutElement(safe_array, &i, raw_provider);
    ++i;
  }

  *children = safe_array;
  return S_OK;
}

base::string16 AXPlatformNodeTextRangeProviderWin::GetString() {
  AXNodeRange range(start_->Clone(), end_->Clone());

  return range.GetText();
}

ui::AXPlatformNodeWin* AXPlatformNodeTextRangeProviderWin::owner() const {
  return owner_;
}

AXPlatformNodeTextRangeProviderWin::AXPositionInstance
AXPlatformNodeTextRangeProviderWin::MoveEndpointByCharacter(
    const AXPositionInstance& endpoint,
    const int count,
    int* units_moved) {
  DCHECK_NE(count, 0);

  return MoveEndpointByUnitHelper(
      std::move(endpoint),
      (count > 0) ? &AXPositionInstanceType::CreateNextCharacterPosition
                  : &AXPositionInstanceType::CreatePreviousCharacterPosition,
      count, units_moved);
}

AXPlatformNodeTextRangeProviderWin::AXPositionInstance
AXPlatformNodeTextRangeProviderWin::MoveEndpointByWord(
    const AXPositionInstance& endpoint,
    bool is_start_endpoint,
    const int count,
    int* units_moved) {
  DCHECK_NE(count, 0);

  CreateNextPositionFunction create_next_position = nullptr;
  if (count > 0)
    create_next_position =
        is_start_endpoint ? &AXPositionInstanceType::CreateNextWordStartPosition
                          : &AXPositionInstanceType::CreateNextWordEndPosition;
  else
    create_next_position =
        is_start_endpoint
            ? &AXPositionInstanceType::CreatePreviousWordStartPosition
            : &AXPositionInstanceType::CreatePreviousWordEndPosition;

  return MoveEndpointByUnitHelper(std::move(endpoint), create_next_position,
                                  count, units_moved);
}

AXPlatformNodeTextRangeProviderWin::AXPositionInstance
AXPlatformNodeTextRangeProviderWin::MoveEndpointByLine(
    const AXPositionInstance& endpoint,
    bool is_start_endpoint,
    const int count,
    int* units_moved) {
  DCHECK_NE(count, 0);

  CreateNextPositionFunction create_next_position = nullptr;
  if (count > 0)
    create_next_position =
        is_start_endpoint ? &AXPositionInstanceType::CreateNextLineStartPosition
                          : &AXPositionInstanceType::CreateNextLineEndPosition;
  else
    create_next_position =
        is_start_endpoint
            ? &AXPositionInstanceType::CreatePreviousLineStartPosition
            : &AXPositionInstanceType::CreatePreviousLineEndPosition;

  return MoveEndpointByUnitHelper(std::move(endpoint), create_next_position,
                                  count, units_moved);
}

AXPlatformNodeTextRangeProviderWin::AXPositionInstance
AXPlatformNodeTextRangeProviderWin::MoveEndpointByDocument(
    const AXPositionInstance& endpoint,
    const int count,
    int* units_moved) {
  DCHECK_NE(count, 0);

  *units_moved = 0;
  AXPositionInstance current_endpoint = endpoint->Clone();
  const bool forwards = count > 0;

  if (forwards && !current_endpoint->AtEndOfDocument()) {
    current_endpoint = endpoint->CreatePositionAtEndOfDocument();
    *units_moved = 1;
  } else if (!forwards && !current_endpoint->AtStartOfDocument()) {
    current_endpoint = endpoint->CreatePositionAtStartOfDocument();
    *units_moved = -1;
  }

  return current_endpoint;
}

AXPlatformNodeTextRangeProviderWin::AXPositionInstance
AXPlatformNodeTextRangeProviderWin::MoveEndpointByUnitHelper(
    const AXPositionInstance& endpoint,
    CreateNextPositionFunction create_next_position,
    const int count,
    int* units_moved) {
  DCHECK_NE(count, 0);

  AXPositionInstance current_endpoint = endpoint->Clone();

  for (int iteration = 0; iteration < std::abs(count); ++iteration) {
    AXPositionInstance next_endpoint =
        (current_endpoint.get()->*create_next_position)(
            AXBoundaryBehavior::CrossBoundary);

    // We've reached either the start or the end of the document.
    if (next_endpoint->IsNullPosition()) {
      *units_moved = (count > 0) ? iteration : -iteration;
      return current_endpoint;
    }
    current_endpoint = std::move(next_endpoint);
  }

  *units_moved = count;
  return current_endpoint;
}

}  // namespace ui
