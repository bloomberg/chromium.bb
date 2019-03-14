// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_textrangeprovider_win.h"

#include <utility>
#include <vector>

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
    AXNodePosition::AXPositionInstance start,
    AXNodePosition::AXPositionInstance end,
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
  *clone = nullptr;

  return CreateTextRangeProvider(owner_, start_->Clone(), end_->Clone(), clone);
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::Compare(
    __in ITextRangeProvider* other,
    __out BOOL* result) {
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::CompareEndpoints(
    TextPatternRangeEndpoint endpoint,
    __in ITextRangeProvider* other,
    TextPatternRangeEndpoint targetEndpoint,
    __out int* result) {
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::ExpandToEnclosingUnit(
    TextUnit unit) {
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::FindAttribute(
    TEXTATTRIBUTEID text_attribute_id,
    VARIANT val,
    BOOL backward,
    ITextRangeProvider** result) {
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::FindText(
    BSTR string,
    BOOL backwards,
    BOOL ignore_case,
    ITextRangeProvider** result) {
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::GetAttributeValue(
    TEXTATTRIBUTEID attributeId,
    VARIANT* value) {
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::GetBoundingRectangles(
    SAFEARRAY** rectangles) {
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
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::GetText(int max_count,
                                                         BSTR* text) {
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
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::MoveEndpointByUnit(
    TextPatternRangeEndpoint endpoint,
    TextUnit unit,
    int count,
    int* units_moved) {
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::MoveEndpointByRange(
    TextPatternRangeEndpoint endpoint,
    ITextRangeProvider* other,
    TextPatternRangeEndpoint targetEndpoint) {
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::Select() {
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::AddToSelection() {
  return UIA_E_INVALIDOPERATION;  // not supporting disjoint text selections
}

STDMETHODIMP
AXPlatformNodeTextRangeProviderWin::RemoveFromSelection() {
  return UIA_E_INVALIDOPERATION;  // not supporting disjoint text selections
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::ScrollIntoView(
    BOOL align_to_top) {
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::GetChildren(
    SAFEARRAY** children) {
  return E_NOTIMPL;
}

base::string16 AXPlatformNodeTextRangeProviderWin::GetString() {
  AXNodeRange range(start_->Clone(), end_->Clone());

  return range.GetText();
}

ui::AXPlatformNodeWin* AXPlatformNodeTextRangeProviderWin::owner() const {
  return owner_;
}

}  // namespace ui
