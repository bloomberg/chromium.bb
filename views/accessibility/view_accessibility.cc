// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/accessibility/view_accessibility.h"

#include "ui/base/view_prop.h"
#include "views/controls/button/native_button.h"
#include "views/widget/widget.h"
#include "views/widget/widget_win.h"

const char kViewsNativeHostPropForAccessibility[] =
    "Views_NativeViewHostHWNDForAccessibility";

// static
scoped_refptr<ViewAccessibility> ViewAccessibility::Create(views::View* view) {
  CComObject<ViewAccessibility>* instance = NULL;
  HRESULT hr = CComObject<ViewAccessibility>::CreateInstance(&instance);
  DCHECK(SUCCEEDED(hr));
  instance->set_view(view);
  return scoped_refptr<ViewAccessibility>(instance);
}

// static
IAccessible* ViewAccessibility::GetAccessibleForView(views::View* view) {
  IAccessible* accessible = NULL;

  // First, check to see if the view is a native view.
  if (view->GetClassName() == views::NativeViewHost::kViewClassName) {
    views::NativeViewHost* native_host =
        static_cast<views::NativeViewHost*>(view);
    if (GetNativeIAccessibleInterface(native_host, &accessible) == S_OK)
      return accessible;
  }

  // Next, see if the view is a widget container.
  if (view->child_widget()) {
    views::WidgetWin* native_widget =
      reinterpret_cast<views::WidgetWin*>(view->child_widget());
    if (GetNativeIAccessibleInterface(
        native_widget->GetNativeView(), &accessible) == S_OK) {
      return accessible;
    }
  }

  // Finally, use our ViewAccessibility implementation.
  return view->GetViewAccessibility();
}

ViewAccessibility::ViewAccessibility() : view_(NULL) {
}

ViewAccessibility::~ViewAccessibility() {
}

// TODO(ctguil): Handle case where child View is not contained by parent.
STDMETHODIMP ViewAccessibility::accHitTest(
    LONG x_left, LONG y_top, VARIANT* child) {
  if (!child)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  gfx::Point point(x_left, y_top);
  views::View::ConvertPointToView(NULL, view_, &point);

  if (!view_->HitTest(point)) {
    // If containing parent is not hit, return with failure.
    child->vt = VT_EMPTY;
    return S_FALSE;
  }

  views::View* view = view_->GetViewForPoint(point);
  if (view == view_) {
    // No child hit, return parent id.
    child->vt = VT_I4;
    child->lVal = CHILDID_SELF;
  } else {
    child->vt = VT_DISPATCH;
    child->pdispVal = GetAccessibleForView(view);
    child->pdispVal->AddRef();
  }
  return S_OK;
}

HRESULT ViewAccessibility::accDoDefaultAction(VARIANT var_id) {
  if (!IsValidId(var_id))
    return E_INVALIDARG;

  if (view_->GetClassName() == views::NativeButton::kViewClassName) {
    views::NativeButton* native_button =
        static_cast<views::NativeButton*>(view_);
    native_button->ButtonPressed();
    return S_OK;
  }

  // The object does not support the method. This value is returned for
  // controls that do not perform actions, such as edit fields.
  return DISP_E_MEMBERNOTFOUND;
}

STDMETHODIMP ViewAccessibility::accLocation(
    LONG* x_left, LONG* y_top, LONG* width, LONG* height, VARIANT var_id) {
  if (!IsValidId(var_id) || !x_left || !y_top || !width || !height)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  if (!view_->bounds().IsEmpty()) {
    *width  = view_->width();
    *height = view_->height();
    gfx::Point topleft(view_->bounds().origin());
    views::View::ConvertPointToScreen(view_->parent() ? view_->parent() : view_,
                                      &topleft);
    *x_left = topleft.x();
    *y_top  = topleft.y();
  } else {
    return E_FAIL;
  }
  return S_OK;
}

STDMETHODIMP ViewAccessibility::accNavigate(LONG nav_dir, VARIANT start,
                                            VARIANT* end) {
  if (start.vt != VT_I4 || !end)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  switch (nav_dir) {
    case NAVDIR_FIRSTCHILD:
    case NAVDIR_LASTCHILD: {
      if (start.lVal != CHILDID_SELF) {
        // Start of navigation must be on the View itself.
        return E_INVALIDARG;
      } else if (!view_->has_children()) {
        // No children found.
        return S_FALSE;
      }

      // Set child_id based on first or last child.
      int child_id = 0;
      if (nav_dir == NAVDIR_LASTCHILD)
        child_id = view_->child_count() - 1;

      views::View* child = view_->GetChildViewAt(child_id);
      end->vt = VT_DISPATCH;
      end->pdispVal = GetAccessibleForView(child);
      end->pdispVal->AddRef();
      return S_OK;
    }
    case NAVDIR_LEFT:
    case NAVDIR_UP:
    case NAVDIR_PREVIOUS:
    case NAVDIR_RIGHT:
    case NAVDIR_DOWN:
    case NAVDIR_NEXT: {
      // Retrieve parent to access view index and perform bounds checking.
      views::View* parent = view_->parent();
      if (!parent) {
        return E_FAIL;
      }

      if (start.lVal == CHILDID_SELF) {
        int view_index = parent->GetIndexOf(view_);
        // Check navigation bounds, adjusting for View child indexing (MSAA
        // child indexing starts with 1, whereas View indexing starts with 0).
        if (!IsValidNav(nav_dir, view_index, -1,
                        parent->child_count() - 1)) {
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
        end->pdispVal = GetAccessibleForView(child);
        end->vt = VT_DISPATCH;
        end->pdispVal->AddRef();
        return S_OK;
      } else {
        // Check navigation bounds, adjusting for MSAA child indexing (MSAA
        // child indexing starts with 1, whereas View indexing starts with 0).
        if (!IsValidNav(nav_dir, start.lVal, 0, parent->child_count() + 1)) {
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
    int child_id_as_index = child_id - 1;
    if (child_id_as_index < view_->child_count()) {
      // Note: child_id is a one based index when indexing children.
      child_view = view_->GetChildViewAt(child_id_as_index);
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

  *disp_child = GetAccessibleForView(child_view);
  (*disp_child)->AddRef();
  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accChildCount(LONG* child_count) {
  if (!child_count || !view_)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  *child_count = view_->child_count();
  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accDefaultAction(
    VARIANT var_id, BSTR* def_action) {
  if (!IsValidId(var_id) || !def_action)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  string16 temp_action = view_->GetAccessibleDefaultAction();

  if (!temp_action.empty()) {
    *def_action = SysAllocString(temp_action.c_str());
  } else {
    return S_FALSE;
  }

  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accDescription(VARIANT var_id, BSTR* desc) {
  if (!IsValidId(var_id) || !desc)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

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
  if (!focus_child)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  views::View* focus = NULL;
  views::FocusManager* focus_manager = view_->GetFocusManager();
  if (focus_manager)
    focus = focus_manager->GetFocusedView();
  if (focus == view_) {
    // This view has focus.
    focus_child->vt = VT_I4;
    focus_child->lVal = CHILDID_SELF;
  } else if (focus && view_->Contains(focus)) {
    // Return the child object that has the keyboard focus.
    focus_child->pdispVal = GetAccessibleForView(focus);
    focus_child->pdispVal->AddRef();
    return S_OK;
  } else {
    // Neither this object nor any of its children has the keyboard focus.
    focus_child->vt = VT_EMPTY;
  }
  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accKeyboardShortcut(
    VARIANT var_id, BSTR* acc_key) {
  if (!IsValidId(var_id) || !acc_key)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  string16 temp_key = view_->GetAccessibleKeyboardShortcut();

  if (!temp_key.empty()) {
    *acc_key = SysAllocString(temp_key.c_str());
  } else {
    return S_FALSE;
  }

  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accName(VARIANT var_id, BSTR* name) {
  if (!IsValidId(var_id) || !name)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  string16 temp_name;

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
  if (!disp_parent)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  views::View* parent_view = view_->parent();

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

  *disp_parent = GetAccessibleForView(parent_view);
  (*disp_parent)->AddRef();
  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accRole(VARIANT var_id, VARIANT* role) {
  if (!IsValidId(var_id) || !role)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  role->vt = VT_I4;
  role->lVal = MSAARole(view_->GetAccessibleRole());
  return S_OK;
}

STDMETHODIMP ViewAccessibility::get_accState(VARIANT var_id, VARIANT* state) {
  if (!IsValidId(var_id) || !state)
    return E_INVALIDARG;

  if (!view_)
    return E_FAIL;

  state->vt = VT_I4;

  // Retrieve all currently applicable states of the parent.
  SetState(state, view_);

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

  // Retrieve the current view's value.
  string16 temp_value = view_->GetAccessibleValue();

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

  if (!view->IsEnabled())
    msaa_state->lVal |= STATE_SYSTEM_UNAVAILABLE;
  if (!view->IsVisible())
    msaa_state->lVal |= STATE_SYSTEM_INVISIBLE;
  if (view->IsHotTracked())
    msaa_state->lVal |= STATE_SYSTEM_HOTTRACKED;
  if (view->HasFocus())
    msaa_state->lVal |= STATE_SYSTEM_FOCUSED;

  // Add on any view-specific states.
  msaa_state->lVal |= MSAAState(view->GetAccessibleState());
}

// IAccessible functions not supported.

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
    case AccessibilityTypes::EVENT_NAME_CHANGED:
      return EVENT_OBJECT_NAMECHANGE;
    case AccessibilityTypes::EVENT_TEXT_CHANGED:
      return EVENT_OBJECT_VALUECHANGE;
    case AccessibilityTypes::EVENT_SELECTION_CHANGED:
      return EVENT_OBJECT_TEXTSELECTIONCHANGED;
    case AccessibilityTypes::EVENT_VALUE_CHANGED:
      return EVENT_OBJECT_VALUECHANGE;
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

// static
HRESULT ViewAccessibility::GetNativeIAccessibleInterface(
    views::NativeViewHost* native_host, IAccessible** accessible) {
  if (!native_host || !accessible)
    return E_INVALIDARG;

  HWND native_view_window = static_cast<HWND>(
      ui::ViewProp::GetValue(native_host->native_view(),
                             kViewsNativeHostPropForAccessibility));
  if (!IsWindow(native_view_window)) {
    native_view_window = native_host->native_view();
  }

  return GetNativeIAccessibleInterface(native_view_window, accessible);
}

// static
HRESULT ViewAccessibility::GetNativeIAccessibleInterface(
    HWND native_view_window , IAccessible** accessible) {
  if (IsWindow(native_view_window)) {
    LRESULT ret = SendMessage(native_view_window, WM_GETOBJECT, 0,
                              OBJID_CLIENT);
    return ObjectFromLresult(ret, IID_IDispatch, 0,
                             reinterpret_cast<void**>(accessible));
  }

  return E_FAIL;
}
