// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_ACCESSIBILITY_VIEW_ACCESSIBILITY_H_
#define VIEWS_ACCESSIBILITY_VIEW_ACCESSIBILITY_H_
#pragma once

#include <atlbase.h>
#include <atlcom.h>

#include <oleacc.h>

#include "base/scoped_ptr.h"
#include "views/controls/native/native_view_host.h"
#include "views/view.h"

////////////////////////////////////////////////////////////////////////////////
//
// ViewAccessibility
//
// Class implementing the MSAA IAccessible COM interface for a generic View,
// providing accessibility to be used by screen readers and other assistive
// technology (AT).
//
////////////////////////////////////////////////////////////////////////////////
class ATL_NO_VTABLE ViewAccessibility
  : public CComObjectRootEx<CComMultiThreadModel>,
    public IDispatchImpl<IAccessible, &IID_IAccessible, &LIBID_Accessibility> {
 public:
  BEGIN_COM_MAP(ViewAccessibility)
    COM_INTERFACE_ENTRY2(IDispatch, IAccessible)
    COM_INTERFACE_ENTRY(IAccessible)
  END_COM_MAP()

  // Create method for view accessibility.
  static scoped_refptr<ViewAccessibility> Create(views::View* view);

  // Returns the IAccessible interface for a view.
  static IAccessible* GetAccessibleForView(views::View* view);

  virtual ~ViewAccessibility();

  void set_view(views::View* view) { view_ = view; }

  // Supported IAccessible methods.

  // Retrieves the child element or child object at a given point on the screen.
  STDMETHODIMP accHitTest(LONG x_left, LONG y_top, VARIANT* child);

  // Performs the object's default action.
  STDMETHODIMP accDoDefaultAction(VARIANT var_id);

  // Retrieves the specified object's current screen location.
  STDMETHODIMP accLocation(LONG* x_left,
                           LONG* y_top,
                           LONG* width,
                           LONG* height,
                           VARIANT var_id);

  // Traverses to another UI element and retrieves the object.
  STDMETHODIMP accNavigate(LONG nav_dir, VARIANT start, VARIANT* end);

  // Retrieves an IDispatch interface pointer for the specified child.
  STDMETHODIMP get_accChild(VARIANT var_child, IDispatch** disp_child);

  // Retrieves the number of accessible children.
  STDMETHODIMP get_accChildCount(LONG* child_count);

  // Retrieves a string that describes the object's default action.
  STDMETHODIMP get_accDefaultAction(VARIANT var_id, BSTR* default_action);

  // Retrieves the tooltip description.
  STDMETHODIMP get_accDescription(VARIANT var_id, BSTR* desc);

  // Retrieves the object that has the keyboard focus.
  STDMETHODIMP get_accFocus(VARIANT* focus_child);

  // Retrieves the specified object's shortcut.
  STDMETHODIMP get_accKeyboardShortcut(VARIANT var_id, BSTR* access_key);

  // Retrieves the name of the specified object.
  STDMETHODIMP get_accName(VARIANT var_id, BSTR* name);

  // Retrieves the IDispatch interface of the object's parent.
  STDMETHODIMP get_accParent(IDispatch** disp_parent);

  // Retrieves information describing the role of the specified object.
  STDMETHODIMP get_accRole(VARIANT var_id, VARIANT* role);

  // Retrieves the current state of the specified object.
  STDMETHODIMP get_accState(VARIANT var_id, VARIANT* state);

  // Retrieves the current value associated with the specified object.
  STDMETHODIMP get_accValue(VARIANT var_id, BSTR* value);

  // Non-supported IAccessible methods.

  // Selections not applicable to views.
  STDMETHODIMP get_accSelection(VARIANT* selected);
  STDMETHODIMP accSelect(LONG flags_sel, VARIANT var_id);

  // Help functions not supported.
  STDMETHODIMP get_accHelp(VARIANT var_id, BSTR* help);
  STDMETHODIMP get_accHelpTopic(BSTR* help_file,
                                VARIANT var_id,
                                LONG* topic_id);

  // Deprecated functions, not implemented here.
  STDMETHODIMP put_accName(VARIANT var_id, BSTR put_name);
  STDMETHODIMP put_accValue(VARIANT var_id, BSTR put_val);

  // Returns a conversion from the event (as defined in accessibility_types.h)
  // to an MSAA event.
  static int32 MSAAEvent(AccessibilityTypes::Event event);

  // Returns a conversion from the Role (as defined in accessibility_types.h)
  // to an MSAA role.
  static int32 MSAARole(AccessibilityTypes::Role role);

  // Returns a conversion from the State (as defined in accessibility_types.h)
  // to MSAA states set.
  static int32 MSAAState(AccessibilityTypes::State state);

 private:
  ViewAccessibility();

  // Determines navigation direction for accNavigate, based on left, up and
  // previous being mapped all to previous and right, down, next being mapped
  // to next. Returns true if navigation direction is next, false otherwise.
  bool IsNavDirNext(int nav_dir) const;

  // Determines if the navigation target is within the allowed bounds. Returns
  // true if it is, false otherwise.
  bool IsValidNav(int nav_dir,
                  int start_id,
                  int lower_bound,
                  int upper_bound) const;

  // Determines if the child id variant is valid.
  bool IsValidId(const VARIANT& child) const;

  // Helper function which sets applicable states of view.
  void SetState(VARIANT* msaa_state, views::View* view);

  // Returns the IAccessible interface for a native view if applicable.
  // Returns S_OK on success.
  static HRESULT GetNativeIAccessibleInterface(
      views::NativeViewHost* native_host, IAccessible** accessible);

  static HRESULT GetNativeIAccessibleInterface(
      HWND native_view_window, IAccessible** accessible);

  // Give CComObject access to the class constructor.
  template <class Base> friend class CComObject;

  // Member View needed for view-specific calls.
  views::View* view_;

  DISALLOW_COPY_AND_ASSIGN(ViewAccessibility);
};

extern const char kViewsNativeHostPropForAccessibility[];

#endif  // VIEWS_ACCESSIBILITY_VIEW_ACCESSIBILITY_H_
