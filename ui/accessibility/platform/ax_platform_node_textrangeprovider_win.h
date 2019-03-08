// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_TEXTRANGEPROVIDER_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_TEXTRANGEPROVIDER_WIN_H_

#include <tuple>
#include <vector>

#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_position.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"

namespace ui {
class __declspec(uuid("3071e40d-a10d-45ff-a59f-6e8e1138e2c1"))
    AXPlatformNodeTextRangeProviderWin
    : public CComObjectRootEx<CComMultiThreadModel>,
      public ITextRangeProvider {
 public:
  BEGIN_COM_MAP(AXPlatformNodeTextRangeProviderWin)
  COM_INTERFACE_ENTRY(ITextRangeProvider)
  COM_INTERFACE_ENTRY(AXPlatformNodeTextRangeProviderWin)
  END_COM_MAP()

  AXPlatformNodeTextRangeProviderWin();
  ~AXPlatformNodeTextRangeProviderWin();

  // Creates an instance of the class.
  // Returns a successful HRESULT on success
  static HRESULT CreateTextRangeProvider(
      ui::AXPlatformNodeWin* owner,
      AXNodePosition::AXPositionInstance start,
      AXNodePosition::AXPositionInstance end,
      ITextRangeProvider** provider);

  //
  // ITextRangeProvider methods.
  //

  STDMETHODIMP Clone(ITextRangeProvider** clone) override;
  STDMETHODIMP Compare(ITextRangeProvider* other, BOOL* result) override;
  STDMETHODIMP
  CompareEndpoints(TextPatternRangeEndpoint endpoint,
                   ITextRangeProvider* other,
                   TextPatternRangeEndpoint targetEndpoint,
                   int* result) override;
  STDMETHODIMP ExpandToEnclosingUnit(TextUnit unit) override;
  STDMETHODIMP
  FindAttribute(TEXTATTRIBUTEID attributeId,
                VARIANT val,
                BOOL backward,
                ITextRangeProvider** result) override;
  STDMETHODIMP
  FindText(BSTR string,
           BOOL backwards,
           BOOL ignore_case,
           ITextRangeProvider** result) override;
  STDMETHODIMP GetAttributeValue(TEXTATTRIBUTEID attributeId,
                                 VARIANT* value) override;
  STDMETHODIMP
  GetBoundingRectangles(SAFEARRAY** rectangles) override;
  STDMETHODIMP
  GetEnclosingElement(IRawElementProviderSimple** element) override;
  STDMETHODIMP GetText(int max_count, BSTR* text) override;
  STDMETHODIMP Move(TextUnit unit, int count, int* units_moved) override;
  STDMETHODIMP
  MoveEndpointByUnit(TextPatternRangeEndpoint endpoint,
                     TextUnit unit,
                     int count,
                     int* units_moved) override;
  STDMETHODIMP
  MoveEndpointByRange(TextPatternRangeEndpoint endpoint,
                      ITextRangeProvider* other,
                      TextPatternRangeEndpoint targetEndpoint) override;
  STDMETHODIMP Select() override;
  STDMETHODIMP AddToSelection() override;
  STDMETHODIMP RemoveFromSelection() override;
  STDMETHODIMP ScrollIntoView(BOOL align_to_top) override;
  STDMETHODIMP GetChildren(SAFEARRAY** children) override;

 private:
  friend class AXPlatformNodeTextRangeProviderTest;
  base::string16 GetString();
  ui::AXPlatformNodeWin* owner() const;

  using AXPositionInstance = AXNodePosition::AXPositionInstance;
  using AXNodeRange = AXRange<AXNodePosition::AXPositionInstance::element_type>;
  using AXPositionInstancePair =
      std::tuple<AXPositionInstance, AXPositionInstance>;

  AXNodePosition::AXPositionInstance start_;
  AXNodePosition::AXPositionInstance end_;
  CComPtr<ui::AXPlatformNodeWin> owner_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_TEXTRANGEPROVIDER_WIN_H_
