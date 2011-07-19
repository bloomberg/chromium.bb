// in_place_menu.h : menu merging implementation
//
// This file is a modified version of the menu.h file, which is
// part of the ActiveDoc MSDN sample. The modifications are largely
// conversions to Google coding guidelines. Below is the original header
// from the file.

// This is a part of the Active Template Library.
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// This source code is only intended as a supplement to the
// Active Template Library Reference and related
// electronic documentation provided with the library.
// See these sources for detailed information regarding the
// Active Template Library product.

#ifndef CHROME_FRAME_IN_PLACE_MENU_H_
#define CHROME_FRAME_IN_PLACE_MENU_H_

#include "base/logging.h"
#include "base/win/scoped_comptr.h"

template <class T>
class InPlaceMenu {
 public:
  InPlaceMenu() : shared_menu_(NULL), ole_menu_(NULL), our_menu_(NULL) {
  }

  ~InPlaceMenu() {
    InPlaceMenuDestroy();
  }

  HRESULT InPlaceMenuCreate(LPCWSTR menu_name) {
    // We might already have an in-place menu set, because we set menus
    // IOleDocumentView::UIActivate as well as in
    // IOleInPlaceActiveObject::OnDocWindowActivate. If we have already
    // done our work, just return silently
    if (ole_menu_ || shared_menu_)
      return S_OK;

    base::win::ScopedComPtr<IOleInPlaceFrame> in_place_frame;
    GetInPlaceFrame(in_place_frame.Receive());
    // We have no IOleInPlaceFrame, no menu merging possible
    if (!in_place_frame) {
      NOTREACHED();
      return E_FAIL;
    }
    // Create a blank menu and ask the container to add
    // its menus into the OLEMENUGROUPWIDTHS structure
    shared_menu_ = ::CreateMenu();
    OLEMENUGROUPWIDTHS mgw = {0};
    HRESULT hr = in_place_frame->InsertMenus(shared_menu_, &mgw);
    if (FAILED(hr)) {
      ::DestroyMenu(shared_menu_);
      shared_menu_ = NULL;
      return hr;
    }
    // Insert our menus
    our_menu_ = LoadMenu(_AtlBaseModule.GetResourceInstance(),menu_name);
    MergeMenus(shared_menu_, our_menu_, &mgw.width[0], 1);
    // Send the menu to the client
    ole_menu_ = (HMENU)OleCreateMenuDescriptor(shared_menu_, &mgw);
    T* t = static_cast<T*>(this);
    in_place_frame->SetMenu(shared_menu_, ole_menu_, t->m_hWnd);
    return S_OK;
  }

  HRESULT InPlaceMenuDestroy() {
    base::win::ScopedComPtr<IOleInPlaceFrame> in_place_frame;
    GetInPlaceFrame(in_place_frame.Receive());
    if (in_place_frame) {
      in_place_frame->RemoveMenus(shared_menu_);
      in_place_frame->SetMenu(NULL, NULL, NULL);
    }
    if (ole_menu_) {
      OleDestroyMenuDescriptor(ole_menu_);
      ole_menu_ = NULL;
    }
    if (shared_menu_) {
      UnmergeMenus(shared_menu_, our_menu_);
      DestroyMenu(shared_menu_);
      shared_menu_ = NULL;
    }
    if (our_menu_) {
      DestroyMenu(our_menu_);
      our_menu_ = NULL;
    }
    return S_OK;
  }

  void MergeMenus(HMENU shared_menu, HMENU source_menu, LONG* menu_widths,
                  unsigned int width_index) {
    // Copy the popups from the source menu
    // Insert at appropriate spot depending on width_index
    DCHECK(width_index == 0 || width_index == 1);
    int position = 0;
    if (width_index == 1)
      position = (int)menu_widths[0];
    int group_width = 0;
    int menu_items = GetMenuItemCount(source_menu);
    for (int index = 0; index < menu_items; index++) {
      // Get the HMENU of the popup
      HMENU popup_menu = ::GetSubMenu(source_menu, index);
      // Separators move us to next group
      UINT state = GetMenuState(source_menu, index, MF_BYPOSITION);
      if (!popup_menu && (state & MF_SEPARATOR)) {
         // Servers should not touch past 5
        DCHECK(width_index <= 5);
        menu_widths[width_index] = group_width;
        group_width = 0;
        if (width_index < 5)
          position += static_cast<int>(menu_widths[width_index+1]);
        width_index += 2;
      } else {
        // Get the menu item text
        TCHAR item_text[256] = {0};
        int text_length = GetMenuString(source_menu, index, item_text,
                                        ARRAYSIZE(item_text), MF_BYPOSITION);
        // Popups are handled differently than normal menu items
        if (popup_menu) {
          if (::GetMenuItemCount(popup_menu) != 0) {
            // Strip the HIBYTE because it contains a count of items
            state = LOBYTE(state) | MF_POPUP;   // Must be popup
            // Non-empty popup -- add it to the shared menu bar
            InsertMenu(shared_menu, position, state|MF_BYPOSITION,
                       reinterpret_cast<UINT_PTR>(popup_menu), item_text);
            ++position;
            ++group_width;
          }
        } else if (text_length > 0) {
          // only non-empty items are added
          DCHECK(item_text[0] != 0);
          // here the state does not contain a count in the HIBYTE
          InsertMenu(shared_menu, position, state|MF_BYPOSITION,
                     GetMenuItemID(source_menu, index), item_text);
          ++position;
          ++group_width;
        }
      }
    }
  }

  void UnmergeMenus(HMENU shared_menu, HMENU source_menu) {
    int our_item_count = GetMenuItemCount(source_menu);
    int shared_item_count = GetMenuItemCount(shared_menu);

    for (int index = shared_item_count - 1; index >= 0; index--) {
      // Check the popup menus
      HMENU popup_menu = ::GetSubMenu(shared_menu, index);
      if (popup_menu) {
        // If it is one of ours, remove it from the shared menu
        for (int sub_index = 0; sub_index < our_item_count; sub_index++) {
          if (::GetSubMenu(source_menu, sub_index) == popup_menu) {
            // Remove the menu from hMenuShared
            RemoveMenu(shared_menu, index, MF_BYPOSITION);
            break;
          }
        }
      }
    }
  }

 protected:
  HRESULT GetInPlaceFrame(IOleInPlaceFrame** in_place_frame) {
    if (!in_place_frame) {
      NOTREACHED();
      return E_POINTER;
    }
    T* t = static_cast<T*>(this);
    HRESULT hr = E_FAIL;
    if (S_OK != t->GetInPlaceFrame(in_place_frame)) {
      // We weren't given an IOleInPlaceFrame pointer, so
      // we'll have to get it ourselves.
      if (t->m_spInPlaceSite) {
        t->frame_info_.cb = sizeof(OLEINPLACEFRAMEINFO);
        base::win::ScopedComPtr<IOleInPlaceUIWindow> in_place_ui_window;
        RECT position_rect = {0};
        RECT clip_rect = {0};
        hr = t->m_spInPlaceSite->GetWindowContext(in_place_frame,
                                                  in_place_ui_window.Receive(),
                                                  &position_rect, &clip_rect,
                                                  &t->frame_info_);
      }
    }
    return hr;
  }

 protected:
  // The OLE menu descriptor created by the OleCreateMenuDescriptor
  HMENU ole_menu_;
  // The shared menu that we pass to IOleInPlaceFrame::SetMenu
  HMENU shared_menu_;
  // Our menu resource that we want to insert
  HMENU our_menu_;
};

#endif  // CHROME_FRAME_IN_PLACE_MENU_H_

