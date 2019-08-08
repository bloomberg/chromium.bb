// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_textprovider_win.h"

#include <utility>

#include "base/win/scoped_safearray.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/ax_platform_node_textrangeprovider_win.h"

#define UIA_VALIDATE_TEXTPROVIDER_CALL() \
  if (!owner()->GetDelegate())           \
    return UIA_E_ELEMENTNOTAVAILABLE;
#define UIA_VALIDATE_TEXTPROVIDER_CALL_1_ARG(arg) \
  if (!owner()->GetDelegate())                    \
    return UIA_E_ELEMENTNOTAVAILABLE;             \
  if (!arg)                                       \
    return E_INVALIDARG;

namespace ui {

AXPlatformNodeTextProviderWin::AXPlatformNodeTextProviderWin() {
  DVLOG(1) << __func__;
}

AXPlatformNodeTextProviderWin::~AXPlatformNodeTextProviderWin() {}

// static
HRESULT AXPlatformNodeTextProviderWin::Create(ui::AXPlatformNodeWin* owner,
                                              IUnknown** provider) {
  CComObject<AXPlatformNodeTextProviderWin>* text_provider = nullptr;
  HRESULT hr =
      CComObject<AXPlatformNodeTextProviderWin>::CreateInstance(&text_provider);
  if (SUCCEEDED(hr)) {
    DCHECK(text_provider);
    text_provider->owner_ = owner;
    hr = text_provider->QueryInterface(IID_PPV_ARGS(provider));
  }

  return hr;
}

//
// ITextProvider methods.
//

STDMETHODIMP AXPlatformNodeTextProviderWin::GetSelection(
    SAFEARRAY** selection) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXT_GETSELECTION);
  UIA_VALIDATE_TEXTPROVIDER_CALL();

  *selection = nullptr;

  AXPlatformNodeDelegate* delegate = owner()->GetDelegate();

  AXPlatformNode* anchor_object =
      delegate->GetFromNodeID(delegate->GetTreeData().sel_anchor_object_id);
  AXPlatformNode* focus_object =
      delegate->GetFromNodeID(delegate->GetTreeData().sel_focus_object_id);

  // If there's no selected object (or the selected object is not in the
  // subtree), return success and don't fill the SAFEARRAY
  //
  // Note that if a selection spans multiple elements, this will report
  // that no selection took place. This is expected for this API, rather
  // than returning the subset of the selection within this node, because
  // subsequently expanding the ITextRange wouldn't  expand to the full
  // selection.
  if (!anchor_object || !focus_object || (anchor_object != focus_object) ||
      (!anchor_object->IsDescendantOf(owner())))
    return S_OK;

  // sel_anchor_offset corresponds to the selection start index
  // and sel_focus_offset is where the selection ends.
  // If they are equal, that indicates a caret on editable text,
  // which should return a degenerate (empty) text range.
  auto start_offset = delegate->GetTreeData().sel_anchor_offset;
  auto end_offset = delegate->GetTreeData().sel_focus_offset;

  // Reverse start and end if the selection goes backwards
  if (start_offset > end_offset)
    std::swap(start_offset, end_offset);

  AXNodePosition::AXPositionInstance start =
      anchor_object->GetDelegate()->CreateTextPositionAt(start_offset);
  AXNodePosition::AXPositionInstance end =
      anchor_object->GetDelegate()->CreateTextPositionAt(end_offset);

  DCHECK(!start->IsNullPosition());
  DCHECK(!end->IsNullPosition());

  CComPtr<ITextRangeProvider> text_range_provider;
  HRESULT hr = AXPlatformNodeTextRangeProviderWin::CreateTextRangeProvider(
      owner_, std::move(start), std::move(end), &text_range_provider);

  DCHECK(SUCCEEDED(hr));
  if (FAILED(hr))
    return E_FAIL;

  // Since we don't support disjoint text ranges, the SAFEARRAY returned
  // will always have one element
  base::win::ScopedSafearray selections_to_return(
      SafeArrayCreateVector(VT_UNKNOWN /* element type */, 0 /* lower bound */,
                            1 /* number of elements */));

  if (!selections_to_return.Get())
    return E_OUTOFMEMORY;

  LONG index = 0;
  hr = SafeArrayPutElement(selections_to_return.Get(), &index,
                           text_range_provider);
  DCHECK(SUCCEEDED(hr));

  // Since DCHECK only happens in debug builds, return immediately to ensure
  // that we're not leaking the SAFEARRAY on release builds
  if (FAILED(hr))
    return E_FAIL;

  *selection = selections_to_return.Release();

  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextProviderWin::GetVisibleRanges(
    SAFEARRAY** visible_ranges) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXT_GETVISIBLERANGES);
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeTextProviderWin::RangeFromChild(
    IRawElementProviderSimple* child,
    ITextRangeProvider** range) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXT_RANGEFROMCHILD);
  UIA_VALIDATE_TEXTPROVIDER_CALL_1_ARG(child);

  *range = nullptr;

  Microsoft::WRL::ComPtr<ui::AXPlatformNodeWin> child_platform_node;
  if (!SUCCEEDED(child->QueryInterface(IID_PPV_ARGS(&child_platform_node))))
    return UIA_E_INVALIDOPERATION;

  if (!owner()->IsDescendant(child_platform_node.Get()))
    return E_INVALIDARG;

  *range = GetRangeFromChild(owner(), child_platform_node.Get());

  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextProviderWin::RangeFromPoint(
    UiaPoint uia_point,
    ITextRangeProvider** range) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXT_RANGEFROMPOINT);
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeTextProviderWin::get_DocumentRange(
    ITextRangeProvider** range) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXT_GET_DOCUMENTRANGE);
  UIA_VALIDATE_TEXTPROVIDER_CALL();

  *range = nullptr;

  // Start and end should be leaf text positions that span the beginning
  // and end of text content within a node for get_DocumentRange. The start
  // position should be the directly first child and the end position should
  // be the deepest last child node.
  AXNodePosition::AXPositionInstance start =
      owner()->GetDelegate()->CreateTextPositionAt(0)->AsLeafTextPosition();

  AXNodePosition::AXPositionInstance end;
  if (owner()->GetChildCount() == 0) {
    end = start->CreatePositionAtEndOfAnchor()->AsLeafTextPosition();
  } else {
    AXPlatformNode* deepest_last_child =
        AXPlatformNode::FromNativeViewAccessible(
            owner()->ChildAtIndex(owner()->GetChildCount() - 1));

    while (deepest_last_child &&
           deepest_last_child->GetDelegate()->GetChildCount() > 0) {
      deepest_last_child = AXPlatformNode::FromNativeViewAccessible(
          deepest_last_child->GetDelegate()->ChildAtIndex(
              deepest_last_child->GetDelegate()->GetChildCount() - 1));
    }
    end = deepest_last_child->GetDelegate()
              ->CreateTextPositionAt(0)
              ->CreatePositionAtEndOfAnchor()
              ->AsLeafTextPosition();
  }

  return AXPlatformNodeTextRangeProviderWin::CreateTextRangeProvider(
      owner_, std::move(start), std::move(end), range);
}

STDMETHODIMP AXPlatformNodeTextProviderWin::get_SupportedTextSelection(
    enum SupportedTextSelection* text_selection) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXT_GET_SUPPORTEDTEXTSELECTION);
  UIA_VALIDATE_TEXTPROVIDER_CALL();

  *text_selection = SupportedTextSelection_Single;
  return S_OK;
}

//
// ITextEditProvider methods.
//

STDMETHODIMP AXPlatformNodeTextProviderWin::GetActiveComposition(
    ITextRangeProvider** range) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTEDIT_GETACTIVECOMPOSITION);
  UIA_VALIDATE_TEXTPROVIDER_CALL();

  *range = nullptr;
  return GetTextRangeProviderFromActiveComposition(range);
}

STDMETHODIMP AXPlatformNodeTextProviderWin::GetConversionTarget(
    ITextRangeProvider** range) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTEDIT_GETCONVERSIONTARGET);
  UIA_VALIDATE_TEXTPROVIDER_CALL();

  *range = nullptr;
  return GetTextRangeProviderFromActiveComposition(range);
}

ITextRangeProvider* AXPlatformNodeTextProviderWin::GetRangeFromChild(
    ui::AXPlatformNodeWin* ancestor,
    ui::AXPlatformNodeWin* descendant) {
  ITextRangeProvider* range = nullptr;

  DCHECK(ancestor);
  DCHECK(descendant);
  DCHECK(descendant->GetDelegate());
  DCHECK(ancestor->IsDescendant(descendant));

  // Start and end should be leaf text positions.
  AXNodePosition::AXPositionInstance start =
      descendant->GetDelegate()->CreateTextPositionAt(0)->AsLeafTextPosition();

  AXNodePosition::AXPositionInstance end =
      descendant->GetDelegate()
          ->CreateTextPositionAt(start->MaxTextOffset())
          ->AsLeafTextPosition()
          ->CreatePositionAtEndOfAnchor();

  if (!SUCCEEDED(AXPlatformNodeTextRangeProviderWin::CreateTextRangeProvider(
          ancestor, std::move(start), std::move(end), &range))) {
    return nullptr;
  }

  return range;
}

ui::AXPlatformNodeWin* AXPlatformNodeTextProviderWin::owner() const {
  return owner_;
}

HRESULT
AXPlatformNodeTextProviderWin::GetTextRangeProviderFromActiveComposition(
    ITextRangeProvider** range) {
  *range = nullptr;
  // We fetch the start and end offset of an active composition only if
  // this object has focus and TSF is in composition mode.
  // The offsets here refer to the character positions in a plain text
  // view of the DOM tree. Ex: if the active composition in an element
  // has "abc" then the range will be (0,3) in both TSF and accessibility
  if ((AXPlatformNode::FromNativeViewAccessible(
           owner()->GetDelegate()->GetFocus()) ==
       static_cast<AXPlatformNode*>(owner())) &&
      owner()->HasActiveComposition()) {
    gfx::Range active_composition_offset =
        owner()->GetActiveCompositionOffsets();
    AXNodePosition::AXPositionInstance start =
        owner()->GetDelegate()->CreateTextPositionAt(
            /*offset*/ active_composition_offset.start());
    AXNodePosition::AXPositionInstance end =
        owner()->GetDelegate()->CreateTextPositionAt(
            /*offset*/ active_composition_offset.end());

    return AXPlatformNodeTextRangeProviderWin::CreateTextRangeProvider(
        owner_, std::move(start), std::move(end), range);
  }

  return S_OK;
}

}  // namespace ui
