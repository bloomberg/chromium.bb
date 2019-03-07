// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_textprovider_win.h"

#include <utility>

#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/ax_platform_node_textrangeprovider_win.h"

#define UIA_VALIDATE_TEXTPROVIDER_CALL() \
  if (!owner()->GetDelegate())           \
    return UIA_E_ELEMENTNOTAVAILABLE;

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
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeTextProviderWin::GetVisibleRanges(
    SAFEARRAY** visible_ranges) {
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeTextProviderWin::RangeFromChild(
    IRawElementProviderSimple* child,
    ITextRangeProvider** range) {
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeTextProviderWin::RangeFromPoint(
    UiaPoint uia_point,
    ITextRangeProvider** range) {
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeTextProviderWin::get_DocumentRange(
    ITextRangeProvider** range) {
  UIA_VALIDATE_TEXTPROVIDER_CALL();
  DVLOG(1) << __func__;

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
      deepest_last_child =
          AXPlatformNode::FromNativeViewAccessible(owner()->ChildAtIndex(
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
  UIA_VALIDATE_TEXTPROVIDER_CALL();

  *text_selection = SupportedTextSelection_Single;
  return S_OK;
}

//
// ITextEditProvider methods.
//

STDMETHODIMP AXPlatformNodeTextProviderWin::GetActiveComposition(
    ITextRangeProvider** range) {
  return E_NOTIMPL;
}

STDMETHODIMP AXPlatformNodeTextProviderWin::GetConversionTarget(
    ITextRangeProvider** range) {
  return E_NOTIMPL;
}

ui::AXPlatformNodeWin* AXPlatformNodeTextProviderWin::owner() const {
  return owner_;
}

}  // namespace ui
