// ole_document_impl.h : IOleDocument implementation
//
// This file is a modified version of the OleDocument.h file, which is
// part of the ActiveDoc MSDN sample. The modifications are largely
// conversions to Google coding guidelines. Below if the original header
// from the file.

// This is a part of the Active Template Library.
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
// This source code is only intended as a supplement to the
// Active Template Library Reference and related
// electronic documentation provided with the library.
// See these sources for detailed information regarding the
// Active Template Library product.

#ifndef CHROME_FRAME_OLE_DOCUMENT_IMPL_H_
#define CHROME_FRAME_OLE_DOCUMENT_IMPL_H_

// TODO(sanjeevr): Revisit this impl file and cleanup dependencies
#include <atlbase.h>
#include <docobj.h>

#include "base/logging.h"

//////////////////////////////////////////////////////////////////////////////
// IOleDocumentImpl
template <class T>
class ATL_NO_VTABLE IOleDocumentImpl : public IOleDocument {
 public:
  STDMETHOD(CreateView)(IOleInPlaceSite* in_place_site,
                        IStream* stream,
                        DWORD reserved ,
                        IOleDocumentView** new_view) {
    DVLOG(1) << __FUNCTION__;
    if (new_view == NULL)
      return E_POINTER;
    T* t = static_cast<T*>(this);
    // If we've already created a view then we can't create another as we
    // currently only support the ability to create one view
    if (t->m_spInPlaceSite)
      return E_FAIL;
    IOleDocumentView* view;
    t->GetUnknown()->QueryInterface(IID_IOleDocumentView,
                                    reinterpret_cast<void**>(&view));
    // If we support IOleDocument we should support IOleDocumentView
    ATLENSURE(view != NULL);
    // If they've given us a site then use it
    if (in_place_site != NULL)
      view->SetInPlaceSite(in_place_site);
    // If they have given us an IStream pointer then use it to
    // initialize the view
    if (stream != NULL)
      view->ApplyViewState(stream);
    // Return the view
    *new_view = view;
    return S_OK;
  }

  STDMETHOD(GetDocMiscStatus)(DWORD* status) {
    DVLOG(1) << __FUNCTION__;
    if (NULL == status)
      return E_POINTER;
    *status = DOCMISC_NOFILESUPPORT;
    return S_OK;
  }

  STDMETHOD(EnumViews)(IEnumOleDocumentViews** enum_views,
                       IOleDocumentView** view) {
    DVLOG(1) << __FUNCTION__;
    if (view == NULL)
      return E_POINTER;
    T* t = static_cast<T*>(this);
    // We only support one view
    return t->_InternalQueryInterface(IID_IOleDocumentView,
                                      reinterpret_cast<void**>(view));
  }
};

//////////////////////////////////////////////////////////////////////////////
// IOleDocumentViewImpl

template <class T>
class ATL_NO_VTABLE IOleDocumentViewImpl : public IOleDocumentView {
 public:
  STDMETHOD(SetInPlaceSite)(IOleInPlaceSite* in_place_site) {
    DVLOG(1) << __FUNCTION__;
    T* t = static_cast<T*>(this);
    if (t->m_spInPlaceSite) {
      // If we already have a site get rid of it
      UIActivate(FALSE);
      HRESULT hr = t->InPlaceDeactivate();
      if (FAILED(hr))
        return hr;
      DCHECK(!t->m_bInPlaceActive);
    }
    if (in_place_site != NULL) {
      t->m_spInPlaceSite.Release();
      in_place_site->QueryInterface(
          IID_IOleInPlaceSiteWindowless,
          reinterpret_cast<void **>(&t->m_spInPlaceSite));
      if (!t->m_spInPlaceSite) {
        // TODO(sanjeevr): This is a super-hack because m_spInPlaceSite
        // is an IOleInPlaceSiteWindowless pointer and we are setting
        // an IOleInPlaceSite pointer into it. The problem is that ATL
        // (CComControlBase) uses this in a schizophrenic manner based
        // on the m_bWndLess flag. Ouch, ouch, ouch! Find a way to clean
        // this up.
        // Disclaimer: I did not invent this hack, it exists in the MSDN
        // sample from where this code has been derived and it also exists
        // in ATL itself (look at atlctl.h line 938).
        t->m_spInPlaceSite =
            reinterpret_cast<IOleInPlaceSiteWindowless*>(in_place_site);
      }
    }
    return S_OK;
  }

  STDMETHOD(GetInPlaceSite)(IOleInPlaceSite** in_place_site) {
    DVLOG(1) << __FUNCTION__;
    if (in_place_site == NULL)
      return E_POINTER;
    T* t = static_cast<T*>(this);
    return t->m_spInPlaceSite->QueryInterface(
        IID_IOleInPlaceSite,
        reinterpret_cast<LPVOID *>(in_place_site));
  }

  STDMETHOD(GetDocument)(IUnknown** document) {
    DVLOG(1) << __FUNCTION__;
    if (document == NULL)
      return E_POINTER;
    T* t = static_cast<T*>(this);
    *document = t->GetUnknown();
    (*document)->AddRef();
    return S_OK;
  }

  STDMETHOD(SetRect)(LPRECT view_rect) {
    static bool is_resizing = false;
    if (is_resizing)
      return S_OK;
    is_resizing = true;
    DVLOG(1) << __FUNCTION__ << " " << view_rect->left << ","
             << view_rect->top << "," << view_rect->right << ","
             << view_rect->bottom;
    T* t = static_cast<T*>(this);
    t->SetObjectRects(view_rect, view_rect);
    is_resizing = false;
    return S_OK;
  }

  STDMETHOD(GetRect)(LPRECT view_rect) {
    DVLOG(1) << __FUNCTION__;
    if (view_rect == NULL)
      return E_POINTER;
    T* t = static_cast<T*>(this);
    *view_rect = t->m_rcPos;
    return S_OK;
  }

  STDMETHOD(SetRectComplex)(LPRECT view_rect,
                            LPRECT hscroll_rect,
                            LPRECT vscroll_rect,
                            LPRECT size_box_rect) {
    DVLOG(1) << __FUNCTION__ << " not implemented";
    return E_NOTIMPL;
  }

  STDMETHOD(Show)(BOOL show) {
    DVLOG(1) << __FUNCTION__;
    T* t = static_cast<T*>(this);
    HRESULT hr = S_OK;
    if (show) {
      if (!t->m_bUIActive)
        hr = t->ActiveXDocActivate(OLEIVERB_INPLACEACTIVATE);
    } else {
      hr = t->UIActivate(FALSE);
      ::ShowWindow(t->m_hWnd, SW_HIDE);
    }
    return hr;
  }

  STDMETHOD(UIActivate)(BOOL ui_activate) {
    DVLOG(1) << __FUNCTION__;
    T* t = static_cast<T*>(this);
    HRESULT hr = S_OK;
    if (ui_activate) {
      // We must know the client site first
      if (t->m_spInPlaceSite == NULL)
        return E_UNEXPECTED;
      if (!t->m_bUIActive)
        hr = t->ActiveXDocActivate(OLEIVERB_UIACTIVATE);
    } else {
      // Menu integration is still not complete, so do not destroy
      // IE's menus.  If we call InPlaceMenuDestroy here, menu items such
      // as Print etc will be disabled and we will not get calls to QueryStatus
      // for those commands.
      // t->InPlaceMenuDestroy();
      // t->DestroyToolbar();
      hr = t->UIDeactivate();
    }
    return hr;
  }

  STDMETHOD(Open)() {
    DVLOG(1) << __FUNCTION__ << " not implemented";
    return E_NOTIMPL;
  }

  STDMETHOD(CloseView)(DWORD reserved) {
    DVLOG(1) << __FUNCTION__;
    T* t = static_cast<T*>(this);
    t->Show(FALSE);
    t->SetInPlaceSite(NULL);
    return S_OK;
  }

  STDMETHOD(SaveViewState)(LPSTREAM stream) {
    DVLOG(1) << __FUNCTION__ << " not implemented";
    return E_NOTIMPL;
  }

  STDMETHOD(ApplyViewState)(LPSTREAM stream) {
    DVLOG(1) << __FUNCTION__ << " not implemented";
    return E_NOTIMPL;
  }

  STDMETHOD(Clone)(IOleInPlaceSite* new_in_place_site,
                   IOleDocumentView** new_view) {
    DVLOG(1) << __FUNCTION__ << " not implemented";
    return E_NOTIMPL;
  }

  HRESULT ActiveXDocActivate(LONG verb) {
    return E_NOTIMPL;
  }
};

#endif  // CHROME_FRAME_OLE_DOCUMENT_IMPL_H_
