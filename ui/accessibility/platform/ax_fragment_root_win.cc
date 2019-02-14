// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_fragment_root_win.h"

#include "base/no_destructor.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/base/win/atl_module.h"

namespace ui {

class AXFragmentRootPlatformNodeWin : public AXPlatformNodeWin,
                                      public IRawElementProviderFragmentRoot {
 public:
  BEGIN_COM_MAP(AXFragmentRootPlatformNodeWin)
  COM_INTERFACE_ENTRY(IRawElementProviderFragmentRoot)
  COM_INTERFACE_ENTRY_CHAIN(AXPlatformNodeWin)
  END_COM_MAP()

  static AXFragmentRootPlatformNodeWin* Create(
      AXPlatformNodeDelegate* delegate) {
    // Make sure ATL is initialized in this module.
    win::CreateATLModuleIfNeeded();

    CComObject<AXFragmentRootPlatformNodeWin>* instance = nullptr;
    HRESULT hr =
        CComObject<AXFragmentRootPlatformNodeWin>::CreateInstance(&instance);
    DCHECK(SUCCEEDED(hr));
    instance->Init(delegate);
    instance->AddRef();
    return instance;
  }

  //
  // IRawElementProviderSimple methods.
  //

  STDMETHOD(get_HostRawElementProvider)
  (IRawElementProviderSimple** host_element_provider) override {
    UIA_VALIDATE_CALL_1_ARG(host_element_provider);

    HWND hwnd = GetDelegate()->GetTargetForNativeAccessibilityEvent();
    return UiaHostProviderFromHwnd(hwnd, host_element_provider);
  }

  //
  // IRawElementProviderFragment methods.
  //

  STDMETHOD(get_FragmentRoot)
  (IRawElementProviderFragmentRoot** fragment_root) override {
    UIA_VALIDATE_CALL_1_ARG(fragment_root);

    QueryInterface(IID_PPV_ARGS(fragment_root));
    return S_OK;
  }

  STDMETHOD(GetRuntimeId)(SAFEARRAY** runtime_id) {
    UIA_VALIDATE_CALL_1_ARG(runtime_id);

    // UIA obtains a runtime ID for a fragment root from the associated HWND.
    *runtime_id = nullptr;
    return S_OK;
  }

  //
  // IRawElementProviderFragmentRoot methods.
  //

  // x and y are in pixels, in screen coordinates.
  STDMETHOD(ElementProviderFromPoint)
  (double x,
   double y,
   IRawElementProviderFragment** element_provider) override {
    UIA_VALIDATE_CALL_1_ARG(element_provider);

    *element_provider = nullptr;

    gfx::NativeViewAccessible hit_element = nullptr;

    // Descend the tree until we get a non-hit or can't go any further.
    AXPlatformNode* node_to_test = this;
    do {
      gfx::NativeViewAccessible test_result =
          node_to_test->GetDelegate()->HitTestSync(x, y);
      if (test_result != nullptr && test_result != hit_element) {
        hit_element = test_result;
        node_to_test = AXPlatformNode::FromNativeViewAccessible(test_result);
      } else {
        node_to_test = nullptr;
      }
    } while (node_to_test);

    if (hit_element)
      hit_element->QueryInterface(element_provider);

    return S_OK;
  }

  STDMETHOD(GetFocus)(IRawElementProviderFragment** focus) override {
    UIA_VALIDATE_CALL_1_ARG(focus);

    *focus = nullptr;

    gfx::NativeViewAccessible focused_element = GetDelegate()->GetFocus();
    if (focused_element != nullptr) {
      focused_element->QueryInterface(IID_PPV_ARGS(focus));
    }

    return S_OK;
  }
};

class AXFragmentRootMapWin {
 public:
  static AXFragmentRootMapWin& GetInstance() {
    static base::NoDestructor<AXFragmentRootMapWin> instance;
    return *instance;
  }

  void AddFragmentRoot(gfx::AcceleratedWidget widget,
                       AXFragmentRootWin* fragment_root) {
    map_[widget] = fragment_root;
  }

  void RemoveFragmentRoot(gfx::AcceleratedWidget widget) { map_.erase(widget); }

  ui::AXFragmentRootWin* GetFragmentRoot(gfx::AcceleratedWidget widget) {
    return map_[widget];
  }

 private:
  std::unordered_map<gfx::AcceleratedWidget, AXFragmentRootWin*> map_;
};

AXFragmentRootWin::AXFragmentRootWin(gfx::AcceleratedWidget widget,
                                     gfx::NativeViewAccessible child) {
  widget_ = widget;
  SetChild(child);
  platform_node_ = ui::AXFragmentRootPlatformNodeWin::Create(this);
  AXFragmentRootMapWin::GetInstance().AddFragmentRoot(widget, this);
}

AXFragmentRootWin::~AXFragmentRootWin() {
  AXFragmentRootMapWin::GetInstance().RemoveFragmentRoot(widget_);
  platform_node_->Destroy();
  platform_node_ = nullptr;
  child_ = nullptr;
}

AXFragmentRootWin* AXFragmentRootWin::GetForAcceleratedWidget(
    gfx::AcceleratedWidget widget) {
  return AXFragmentRootMapWin::GetInstance().GetFragmentRoot(widget);
}

gfx::NativeViewAccessible AXFragmentRootWin::GetNativeViewAccessible() {
  return platform_node_.Get();
}

void AXFragmentRootWin::SetChild(gfx::NativeViewAccessible child) {
  if (child != nullptr) {
    child_ = static_cast<ui::AXPlatformNodeWin*>(
        ui::AXPlatformNode::FromNativeViewAccessible(child));
    DCHECK(child_);
  } else {
    child_ = nullptr;
  }
}

int AXFragmentRootWin::GetChildCount() {
  return (child_ != nullptr) ? 1 : 0;
}

gfx::NativeViewAccessible AXFragmentRootWin::ChildAtIndex(int index) {
  if (index == 0 && child_ != nullptr) {
    return child_->GetNativeViewAccessible();
  }

  return nullptr;
}

gfx::NativeViewAccessible AXFragmentRootWin::HitTestSync(int x, int y) {
  if (child_ != nullptr)
    return child_->GetDelegate()->HitTestSync(x, y);

  return nullptr;
}

gfx::NativeViewAccessible AXFragmentRootWin::GetFocus() {
  if (child_ != nullptr)
    return child_->GetDelegate()->GetFocus();

  return nullptr;
}

const ui::AXUniqueId& AXFragmentRootWin::GetUniqueId() const {
  return unique_id_;
}

gfx::AcceleratedWidget
AXFragmentRootWin::GetTargetForNativeAccessibilityEvent() {
  return widget_;
}
}  // namespace ui
