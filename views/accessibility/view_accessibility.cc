// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/accessibility/view_accessibility.h"

#include "views/accessibility/view_accessibility_wrapper.h"
#include "views/widget/widget.h"
#include "views/widget/widget_win.h"

const wchar_t kViewsUninitializeAccessibilityInstance[] =
  L"Views_Uninitialize_AccessibilityInstance";

const wchar_t kViewsNativeHostPropForAccessibility[] =
  L"Views_NativeViewHostHWNDForAccessibility";


HRESULT ViewAccessibility::Initialize(views::View* view) {
  if (!view) {
    return E_INVALIDARG;
  }

  view_ = view;
  return S_OK;
}

// TODO(ctguil): Handle case where child View is not contained by parent.
STDMETHODIMP ViewAccessibility::accHitTest(
    LONG x_left, LONG y_top, VARIANT* child) {
  if (!child) {
    return E_INVALIDARG;
  }

  if (!view_)
    return E_FAIL;

  gfx::Point pt(x_left, y_top);
  views::View::ConvertPointToView(NULL, view_, &pt);

  if (!view_->HitTest(pt)) {
    // If containing parent is not hit, return with failure.
    child->vt = VT_EMPTY;
    return S_FALSE;
  }

  int child_count = view_->GetChildViewCount();
  bool child_hit = false;
  views::View* child_view = NULL;
  for (int child_id = 0; child_id < child_count; ++child_id) {
    // Search for hit within any of the children.
    child_view = view_->GetChildViewAt(child_id);
    views::View::ConvertPointToView(view_, child_view, &pt);
    if (child_view->HitTest(pt)) {
      // Store child_id (adjusted with +1 to convert to MSAA indexing).
      child->lVal = child_id + 1;
      child_hit = true;
      break;
    }
    // Convert point back to parent view to test next child.
    views::View::ConvertPointToView(child_view, view_, &pt);
  }

  child->vt = VT_I4;

  if (!child_hit) {
    // No child hit, return parent id.
    child->lVal = CHILDID_SELF;
  } else {
    if (child_view == NULL) {
      return E_FAIL;
    } else if (child_view->GetChildViewCount() != 0) {
      // Retrieve IDispatch for child, if it is not a leaf.
      child->vt = VT_DISPATCH;
      if ((GetViewAccessibilityWrapper(child_view))->
          GetInstance(IID_IAccessible,
                      reinterpret_cast<void**>(&child->pdispVal)) == S_OK) {
        // Increment the reference count for the retrieved interface.
        child->pdispVal->AddRef();
        return S_OK;
      } else {
        return E_NOINTERFACE;
      }
    }
  }

  return S_OK;
}

STDMETHODIMP ViewAccessibility::accLocation(
    LONG* x_left, LONG* y_top, LONG* width, LONG* height, VARIANT var_id) {
  if (!IsValidId(var_id) || !x_left || !y_top || !width || !height) {
    return E_INVALIDARG;
  }

  if (!view_)
    return E_FAIL;

  gfx::Rect view_bounds;
  // Retrieving the parent View to be used for converting from view-to-screen
  // coordinates.
  views::View* parent = view_->GetParent();

  if (parent == NULL) {
    // If no parent, remain within the same View.
    parent = view_;
  }

  // Retrieve active View's bounds.
  view_bounds = view_->bounds();

  if (!view_bounds.IsEmpty()) {
    *width  = view_bounds.width();
    *height = view_bounds.height();

    gfx::Point topleft(view_bounds.origin());
    views::View::ConvertPointToScreen(parent, &topleft);
    *x_left = topleft.x();
    *y_top  = topleft.y();
  } else {
    return E_FAIL;
  }
  return S_OK;
}

STDMETHODIMP ViewAccessibility::accNavigate(LONG nav_dir, VARIANT start,
                                            VARIANT* end) {
  if (start.vt != VT_I4 || !end) {
    return E_INVALIDARG;
  }

  if (!view_)
    return E_FAIL;

  switch (nav_dir) {
    case NAVDIR_FIRSTCHILD:
    case NAVDIR_LASTCHILD: {
      if (start.lVal != CHILDID_SELF) {
        // Start of navigation must be on the View itself.
        return E_INVALIDARG;
      } else if (view_->GetChildViewCount() == 0) {
        // No children found.
        return S_FALSE;
      }

      // Set child_id based on first or last child.
      int child_id = 0;
      if (nav_dir == NAVDIR_LASTCHILD) {
        child_id = view_->GetChildViewCount() - 1;
      }

      views::View* child = view_->GetChildViewAt(child_id);

      if (child->GetChildViewCount() != 0) {
        end->vt = VT_DISPATCH;
        if ((GetViewAccessibilityWrapper(child))->
            GetInstance(IID_IAccessible,
                        reinterpret_cast<void**>(&end->pdispVal)) == S_OK) {
          // Increment the reference count for the retrieved interface.
          end->pdispVal->AddRef();
          return S_OK;
        } else {
          return E_NOINTERFACE;
        }
      } else {
        end->vt = VT_I4;
        // Set return child lVal, adjusted for MSAA indexing. (MSAA
        // child indexing starts with 1, whereas View indexing starts with 0).
        end->lVal = child_id + 1;
      }
      break;
    }
    case NAVDIR_LEFT:
    case NAVDIR_UP:
    case NAVDIR_PREVIOUS:
    case NAVDIR_RIGHT:
    case NAVDIR_DOWN:
    case NAVDIR_NEXT: {
      // Retrieve parent to access view index and perform bounds checking.
      views::View* parent = view_->GetParent();
      if (!parent) {
        return E_FAIL;
      }

      if (start.lVal == CHILDID_SELF) {
        int view_index = parent->GetChildIndex(view_);
        // Check navigation bounds, adjusting for View child indexing (MSAA
        // child indexing starts with 1, whereas View indexing starts with 0).
        if (!IsValidNav(nav_dir, view_index, -1,
                        parent->GetChildViewCount() - 1)) {
          // Navigation attempted to go out-of-bounds.
          end->vt = VT_EMPTY;
          return S_FALSE;
        } else {
          if (IsNavDirNext(nav_dir)) {
            view_index += 1;
          } else {
            view_index -=1;
          }
        }

        views::View* child = parent->GetChildViewAt(view_index);
        if (child->GetChildViewCount() != 0) {
          end->vt = VT_DISPATCH;
          // Retrieve IDispatch for non-leaf child.
          if ((GetViewAccessibilityWrapper(child))->
              GetInstance(IID_IAccessible,
                          reinterpret_cast<void**>(&end->pdispVal)) == S_OK) {
            // Increment the reference count for the retrieved interface.
            end->pdispVal->AddRef();
            return S_OK;
          } else {
            return E_NOINTERFACE;
          }
        } else {
          end->vt = VT_I4;
          // Modifying view_index to give lVal correct MSAA-based value. (MSAA
          // child indexing starts with 1, whereas View indexing starts with 0).
          end->lVal = view_index + 1;
        }
      } else {
        // Check navigation bounds, adjusting for MSAA child indexing (MSAA
        // child indexing starts with 1, whereas View indexing starts with 0).
        if (!IsValidNav(nav_dir, start.lVal, 0,
                        parent->GetChildViewCount() + 1)) {
            // Navigation attempted to go out-of-bounds.
            end->vt = VT_EMPTY;
            return S_FALSE;
          } else {
            if (IsNavDirNext(nav_dir)) {
              start.lVal += 1;
            } else {
              start.lVal -= 1;
            }
        }

        HRESULT result = this->get_accChild(start, &end->pdispVal);
        if (result == S_FALSE) {
          // Child is a leaf.
          end->vt = VT_I4;
          end->lVal = start.lVal;
        } else if (result == E_INVALIDARG) {
          return E_INVALIDARG;
        } else {
          // Child is not a leaf.
          end->vt = VT_DISPATCH;
        }
      }
      break;
    }
    default:
      return E_INVALIDARG;
  }
  // Navigation performed correctly. Global return for this function, if no
  // error triggered an escape earlier.
  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accChild(VARIANT var_child,
                                             IDispatch** disp_child) {
  if (var_child.vt != VT_I4 || !disp_child)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  LONG child_id = V_I4(&var_child);

  if (child_id == CHILDID_SELF) {
    // Remain with the same dispatch.
    return S_OK;
  }

  views::View* child_view = NULL;
  if (child_id > 0) {
    if (child_id <= view_->GetChildViewCount()) {
      // Note: child_id is a one based index when indexing children.
      child_view = view_->GetChildViewAt(child_id - 1);
    } else {
      // Attempt to retrieve a child view with the specified id.
      child_view = view_->GetViewByID(child_id);
    }
  } else {
    // Negative values are used for events fired using the view's WidgetWin
    views::WidgetWin* widget =
        static_cast<views::WidgetWin*>(view_->GetWidget());
    child_view = widget->GetAccessibilityViewEventAt(child_id);
  }

  if (!child_view) {
    // No child found.
    *disp_child = NULL;
    return E_FAIL;
  }

  // First, check to see if the child is a native view.
  if (child_view->GetClassName() == views::NativeViewHost::kViewClassName) {
    views::NativeViewHost* native_host =
        static_cast<views::NativeViewHost*>(child_view);
    if (GetNativeIAccessibleInterface(native_host, disp_child) == S_OK)
      return S_OK;
  }

  // Next, see if the child view is a widget container.
  if (child_view->child_widget()) {
    views::WidgetWin* native_widget =
      reinterpret_cast<views::WidgetWin*>(child_view->child_widget());
    if (GetNativeIAccessibleInterface(
        native_widget->GetNativeView(), disp_child) == S_OK) {
      return S_OK;
    }
  }

  // Finally, try our ViewAccessibility implementation.
  // Retrieve the IUnknown interface for the requested child view, and
  // assign the IDispatch returned.
  HRESULT hr = GetViewAccessibilityWrapper(child_view)->
      GetInstance(IID_IAccessible, reinterpret_cast<void**>(disp_child));
  if (hr == S_OK) {
    // Increment the reference count for the retrieved interface.
    (*disp_child)->AddRef();
    return S_OK;
  } else {
    return E_NOINTERFACE;
  }
}

STDMETHODIMP ViewAccessibility::get_accChildCount(LONG* child_count) {
  if (!child_count || !view_) {
    return E_INVALIDARG;
  }

  if (!view_)
    return E_FAIL;

  *child_count = view_->GetChildViewCount();
  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accDefaultAction(
    VARIANT var_id, BSTR* def_action) {
  if (!IsValidId(var_id) || !def_action) {
    return E_INVALIDARG;
  }

  if (!view_)
    return E_FAIL;

  std::wstring temp_action;

  view_->GetAccessibleDefaultAction(&temp_action);
  if (!temp_action.empty()) {
    *def_action = SysAllocString(temp_action.c_str());
  } else {
    return S_FALSE;
  }

  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accDescription(VARIANT var_id, BSTR* desc) {
  if (!IsValidId(var_id) || !desc) {
    return E_INVALIDARG;
  }

  std::wstring temp_desc;

  view_->GetTooltipText(gfx::Point(), &temp_desc);
  if (!temp_desc.empty()) {
    *desc = SysAllocString(temp_desc.c_str());
  } else {
    return S_FALSE;
  }

  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accFocus(VARIANT* focus_child) {
  if (!focus_child) {
    return E_INVALIDARG;
  }

  if (!view_)
    return E_FAIL;

  if (view_->GetChildViewCount() == 0 && view_->HasFocus()) {
    // Parent view has focus.
    focus_child->vt = VT_I4;
    focus_child->lVal = CHILDID_SELF;
  } else {
    bool has_focus = false;
    int child_count = view_->GetChildViewCount();
    // Search for child view with focus.
    for (int child_id = 0; child_id < child_count; ++child_id) {
      if (view_->GetChildViewAt(child_id)->HasFocus()) {
        focus_child->vt = VT_I4;
        focus_child->lVal = child_id + 1;

        // If child view is no leaf, retrieve IDispatch.
        if (view_->GetChildViewAt(child_id)->GetChildViewCount() != 0) {
          focus_child->vt = VT_DISPATCH;
          this->get_accChild(*focus_child, &focus_child->pdispVal);
        }
        has_focus = true;
        break;
      }
    }
    // No current focus on any of the children.
    if (!has_focus) {
      focus_child->vt = VT_EMPTY;
      return S_FALSE;
    }
  }

  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accKeyboardShortcut(
    VARIANT var_id, BSTR* acc_key) {
  if (!IsValidId(var_id) || !acc_key)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  std::wstring temp_key;

  view_->GetAccessibleKeyboardShortcut(&temp_key);
  if (!temp_key.empty()) {
    *acc_key = SysAllocString(temp_key.c_str());
  } else {
    return S_FALSE;
  }

  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accName(VARIANT var_id, BSTR* name) {
  if (!IsValidId(var_id) || !name) {
    return E_INVALIDARG;
  }

  if (!view_)
    return E_FAIL;

  std::wstring temp_name;

  // Retrieve the current view's name.
  view_->GetAccessibleName(&temp_name);
  if (!temp_name.empty()) {
    // Return name retrieved.
    *name = SysAllocString(temp_name.c_str());
  } else {
    // If view has no name, return S_FALSE.
    return S_FALSE;
  }

  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accParent(IDispatch** disp_parent) {
  if (!disp_parent) {
    return E_INVALIDARG;
  }

  if (!view_)
    return E_FAIL;

  views::View* parent_view = view_->GetParent();

  if (!parent_view) {
    // This function can get called during teardown of WidetWin so we
    // should bail out if we fail to get the HWND.
    if (!view_->GetWidget() || !view_->GetWidget()->GetNativeView()) {
      *disp_parent = NULL;
      return S_FALSE;
    }

    // For a View that has no parent (e.g. root), point the accessible parent to
    // the default implementation, to interface with Windows' hierarchy and to
    // support calls from e.g. WindowFromAccessibleObject.
    HRESULT hr =
        ::AccessibleObjectFromWindow(view_->GetWidget()->GetNativeView(),
                                     OBJID_WINDOW, IID_IAccessible,
                                     reinterpret_cast<void**>(disp_parent));

    if (!SUCCEEDED(hr)) {
      *disp_parent = NULL;
      return S_FALSE;
    }

    return S_OK;
  }

  // Retrieve the IUnknown interface for the parent view, and assign the
  // IDispatch returned.
  if ((GetViewAccessibilityWrapper(parent_view))->
      GetInstance(IID_IAccessible,
                  reinterpret_cast<void**>(disp_parent)) == S_OK) {
    // Increment the reference count for the retrieved interface.
    (*disp_parent)->AddRef();
    return S_OK;
  } else {
    return E_NOINTERFACE;
  }
}

STDMETHODIMP ViewAccessibility::get_accRole(VARIANT var_id, VARIANT* role) {
  if (!IsValidId(var_id) || !role)
    return E_INVALIDARG;

  AccessibilityTypes::Role acc_role;

  // Retrieve parent role.
  if (!view_->GetAccessibleRole(&acc_role))
    return E_FAIL;

  role->vt = VT_I4;
  role->lVal = MSAARole(acc_role);
  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accState(VARIANT var_id, VARIANT* state) {
  if (!IsValidId(var_id) || !state) {
    return E_INVALIDARG;
  }

  state->vt = VT_I4;

  // Retrieve all currently applicable states of the parent.
  this->SetState(state, view_);

  // Make sure that state is not empty, and has the proper type.
  if (state->vt == VT_EMPTY)
    return E_FAIL;

  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accValue(VARIANT var_id, BSTR* value) {
  if (!IsValidId(var_id) || !value)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  std::wstring temp_value;

  // Retrieve the current view's value.
  view_->GetAccessibleValue(&temp_value);
  if (!temp_value.empty()) {
    // Return value retrieved.
    *value = SysAllocString(temp_value.c_str());
  } else {
    // If view has no value, fall back into the default implementation.
    *value = NULL;
    return E_NOTIMPL;
  }

  return S_OK;
}

// Helper functions.

bool ViewAccessibility::IsNavDirNext(int nav_dir) const {
  if (nav_dir == NAVDIR_RIGHT || nav_dir == NAVDIR_DOWN ||
      nav_dir == NAVDIR_NEXT) {
      return true;
  }
  return false;
}

bool ViewAccessibility::IsValidNav(int nav_dir, int start_id, int lower_bound,
                                   int upper_bound) const {
  if (IsNavDirNext(nav_dir)) {
    if ((start_id + 1) > upper_bound) {
      return false;
    }
  } else {
    if ((start_id - 1) <= lower_bound) {
      return false;
    }
  }
  return true;
}

bool ViewAccessibility::IsValidId(const VARIANT& child) const {
  // View accessibility returns an IAccessible for each view so we only support
  // the CHILDID_SELF id.
  return (VT_I4 == child.vt) && (CHILDID_SELF == child.lVal);
}

void ViewAccessibility::SetState(VARIANT* msaa_state, views::View* view) {
  // Ensure the output param is initialized to zero.
  msaa_state->lVal = 0;

  // Default state; all views can have accessibility focus.
  msaa_state->lVal |= STATE_SYSTEM_FOCUSABLE;

  if (!view)
    return;

  if (!view->IsEnabled()) {
    msaa_state->lVal |= STATE_SYSTEM_UNAVAILABLE;
  }
  if (!view->IsVisible()) {
    msaa_state->lVal |= STATE_SYSTEM_INVISIBLE;
  }
  if (view->IsHotTracked()) {
    msaa_state->lVal |= STATE_SYSTEM_HOTTRACKED;
  }
  if (view->IsPushed()) {
    msaa_state->lVal |= STATE_SYSTEM_PRESSED;
  }
  // Check both for actual View focus, as well as accessibility focus.
  views::View* parent = view->GetParent();

  if (view->HasFocus())
    msaa_state->lVal |= STATE_SYSTEM_FOCUSED;

  // Add on any view-specific states.
  AccessibilityTypes::State state;
  if (view->GetAccessibleState(&state))
    msaa_state->lVal |= MSAAState(state);
}

// IAccessible functions not supported.

HRESULT ViewAccessibility::accDoDefaultAction(VARIANT var_id) {
  return E_NOTIMPL;
}

STDMETHODIMP ViewAccessibility::get_accSelection(VARIANT* selected) {
  if (selected)
    selected->vt = VT_EMPTY;
  return E_NOTIMPL;
}

STDMETHODIMP ViewAccessibility::accSelect(LONG flagsSelect, VARIANT var_id) {
  return E_NOTIMPL;
}

STDMETHODIMP ViewAccessibility::get_accHelp(VARIANT var_id, BSTR* help) {
  if (help)
    *help = NULL;
  return E_NOTIMPL;
}

STDMETHODIMP ViewAccessibility::get_accHelpTopic(
    BSTR* help_file, VARIANT var_id, LONG* topic_id) {
  if (help_file) {
    *help_file = NULL;
  }
  if (topic_id) {
    *topic_id = static_cast<LONG>(-1);
  }
  return E_NOTIMPL;
}

STDMETHODIMP ViewAccessibility::put_accName(VARIANT var_id, BSTR put_name) {
  // Deprecated.
  return E_NOTIMPL;
}

STDMETHODIMP ViewAccessibility::put_accValue(VARIANT var_id, BSTR put_val) {
  // TODO(ctguil): This use looks incorrect. var_id should be of type VT_I4.
  if (V_VT(&var_id) == VT_BSTR) {
    if (!lstrcmpi(var_id.bstrVal, kViewsUninitializeAccessibilityInstance)) {
      view_ = NULL;
      return S_OK;
    }
  }
  // Deprecated.
  return E_NOTIMPL;
}

int32 ViewAccessibility::MSAAEvent(AccessibilityTypes::Event event) {
  switch (event) {
    case AccessibilityTypes::EVENT_ALERT:
      return EVENT_SYSTEM_ALERT;
    case AccessibilityTypes::EVENT_FOCUS:
      return EVENT_OBJECT_FOCUS;
    case AccessibilityTypes::EVENT_MENUSTART:
      return EVENT_SYSTEM_MENUSTART;
    case AccessibilityTypes::EVENT_MENUEND:
      return EVENT_SYSTEM_MENUEND;
    case AccessibilityTypes::EVENT_MENUPOPUPSTART:
      return EVENT_SYSTEM_MENUPOPUPSTART;
    case AccessibilityTypes::EVENT_MENUPOPUPEND:
      return EVENT_SYSTEM_MENUPOPUPEND;
    default:
      // Not supported or invalid event.
      NOTREACHED();
      return -1;
  }
}

int32 ViewAccessibility::MSAARole(AccessibilityTypes::Role role) {
  switch (role) {
    case AccessibilityTypes::ROLE_ALERT:
return ROLE_SYSTEM_ALERT;
    case AccessibilityTypes::ROLE_APPLICATION:
      return ROLE_SYSTEM_APPLICATION;
    case AccessibilityTypes::ROLE_BUTTONDROPDOWN:
      return ROLE_SYSTEM_BUTTONDROPDOWN;
    case AccessibilityTypes::ROLE_BUTTONMENU:
      return ROLE_SYSTEM_BUTTONMENU;
    case AccessibilityTypes::ROLE_CHECKBUTTON:
      return ROLE_SYSTEM_CHECKBUTTON;
    case AccessibilityTypes::ROLE_COMBOBOX:
      return ROLE_SYSTEM_COMBOBOX;
    case AccessibilityTypes::ROLE_DIALOG:
      return ROLE_SYSTEM_DIALOG;
    case AccessibilityTypes::ROLE_GRAPHIC:
      return ROLE_SYSTEM_GRAPHIC;
    case AccessibilityTypes::ROLE_GROUPING:
      return ROLE_SYSTEM_GROUPING;
    case AccessibilityTypes::ROLE_LINK:
      return ROLE_SYSTEM_LINK;
    case AccessibilityTypes::ROLE_MENUBAR:
      return ROLE_SYSTEM_MENUBAR;
    case AccessibilityTypes::ROLE_MENUITEM:
      return ROLE_SYSTEM_MENUITEM;
    case AccessibilityTypes::ROLE_MENUPOPUP:
      return ROLE_SYSTEM_MENUPOPUP;
    case AccessibilityTypes::ROLE_OUTLINE:
      return ROLE_SYSTEM_OUTLINE;
    case AccessibilityTypes::ROLE_OUTLINEITEM:
      return ROLE_SYSTEM_OUTLINEITEM;
    case AccessibilityTypes::ROLE_PAGETAB:
      return ROLE_SYSTEM_PAGETAB;
    case AccessibilityTypes::ROLE_PAGETABLIST:
      return ROLE_SYSTEM_PAGETABLIST;
    case AccessibilityTypes::ROLE_PANE:
      return ROLE_SYSTEM_PANE;
    case AccessibilityTypes::ROLE_PROGRESSBAR:
      return ROLE_SYSTEM_PROGRESSBAR;
    case AccessibilityTypes::ROLE_PUSHBUTTON:
      return ROLE_SYSTEM_PUSHBUTTON;
    case AccessibilityTypes::ROLE_RADIOBUTTON:
      return ROLE_SYSTEM_RADIOBUTTON;
    case AccessibilityTypes::ROLE_SCROLLBAR:
      return ROLE_SYSTEM_SCROLLBAR;
    case AccessibilityTypes::ROLE_SEPARATOR:
      return ROLE_SYSTEM_SEPARATOR;
    case AccessibilityTypes::ROLE_STATICTEXT:
      return ROLE_SYSTEM_STATICTEXT;
    case AccessibilityTypes::ROLE_TEXT:
      return ROLE_SYSTEM_TEXT;
    case AccessibilityTypes::ROLE_TITLEBAR:
      return ROLE_SYSTEM_TITLEBAR;
    case AccessibilityTypes::ROLE_TOOLBAR:
      return ROLE_SYSTEM_TOOLBAR;
    case AccessibilityTypes::ROLE_WINDOW:
      return ROLE_SYSTEM_WINDOW;
    case AccessibilityTypes::ROLE_CLIENT:
    default:
      // This is the default role for MSAA.
      return ROLE_SYSTEM_CLIENT;
  }
}

int32 ViewAccessibility::MSAAState(AccessibilityTypes::State state) {
  int32 msaa_state = 0;
  if (state & AccessibilityTypes::STATE_CHECKED)
    msaa_state |= STATE_SYSTEM_CHECKED;
  if (state & AccessibilityTypes::STATE_COLLAPSED)
    msaa_state |= STATE_SYSTEM_COLLAPSED;
  if (state & AccessibilityTypes::STATE_DEFAULT)
    msaa_state |= STATE_SYSTEM_DEFAULT;
  if (state & AccessibilityTypes::STATE_EXPANDED)
    msaa_state |= STATE_SYSTEM_EXPANDED;
  if (state & AccessibilityTypes::STATE_HASPOPUP)
    msaa_state |= STATE_SYSTEM_HASPOPUP;
  if (state & AccessibilityTypes::STATE_HOTTRACKED)
    msaa_state |= STATE_SYSTEM_HOTTRACKED;
  if (state & AccessibilityTypes::STATE_INVISIBLE)
    msaa_state |= STATE_SYSTEM_INVISIBLE;
  if (state & AccessibilityTypes::STATE_LINKED)
    msaa_state |= STATE_SYSTEM_LINKED;
  if (state & AccessibilityTypes::STATE_OFFSCREEN)
    msaa_state |= STATE_SYSTEM_OFFSCREEN;
  if (state & AccessibilityTypes::STATE_PRESSED)
    msaa_state |= STATE_SYSTEM_PRESSED;
  if (state & AccessibilityTypes::STATE_PROTECTED)
    msaa_state |= STATE_SYSTEM_PROTECTED;
  if (state & AccessibilityTypes::STATE_READONLY)
    msaa_state |= STATE_SYSTEM_READONLY;
  if (state & AccessibilityTypes::STATE_SELECTED)
    msaa_state |= STATE_SYSTEM_SELECTED;
  if (state & AccessibilityTypes::STATE_FOCUSED)
    msaa_state |= STATE_SYSTEM_FOCUSED;
  if (state & AccessibilityTypes::STATE_UNAVAILABLE)
    msaa_state |= STATE_SYSTEM_UNAVAILABLE;
  return msaa_state;
}

HRESULT ViewAccessibility::GetNativeIAccessibleInterface(
    views::NativeViewHost* native_host, IDispatch** disp_child) {
  if (!native_host || !disp_child) {
    return E_INVALIDARG;
  }

  HWND native_view_window =
      static_cast<HWND>(GetProp(native_host->native_view(),
                                kViewsNativeHostPropForAccessibility));
  if (!IsWindow(native_view_window)) {
    native_view_window = native_host->native_view();
  }

  return GetNativeIAccessibleInterface(native_view_window, disp_child);
}

HRESULT ViewAccessibility::GetNativeIAccessibleInterface(
    HWND native_view_window , IDispatch** disp_child) {
  if (IsWindow(native_view_window)) {
    LRESULT ret = SendMessage(native_view_window, WM_GETOBJECT, 0,
                              OBJID_CLIENT);
    return ObjectFromLresult(ret, IID_IDispatch, 0,
                             reinterpret_cast<void**>(disp_child));
  }

  return E_FAIL;
}
