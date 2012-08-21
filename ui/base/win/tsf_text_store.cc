// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/tsf_text_store.h"

#include <OleCtl.h>
#include "base/logging.h"

namespace ui {

TsfTextStore::TsfTextStore()
    : ref_count_(0),
      text_store_acp_sink_mask_(0) {
}

TsfTextStore::~TsfTextStore() {
}

ULONG STDMETHODCALLTYPE TsfTextStore::AddRef() {
  return InterlockedIncrement(&ref_count_);
}

ULONG STDMETHODCALLTYPE TsfTextStore::Release() {
  const LONG count = InterlockedDecrement(&ref_count_);
  if (!count) {
    delete this;
    return 0;
  }
  return static_cast<ULONG>(count);
}

STDMETHODIMP TsfTextStore::QueryInterface(REFIID iid, void** result) {
  if (iid == IID_IUnknown || iid == IID_ITextStoreACP) {
    *result = static_cast<ITextStoreACP*>(this);
  } else if (iid == IID_ITfContextOwnerCompositionSink) {
    *result = static_cast<ITfContextOwnerCompositionSink*>(this);
  } else if (iid == IID_ITfTextEditSink) {
    *result = static_cast<ITfTextEditSink*>(this);
  } else {
    *result = NULL;
    return E_NOINTERFACE;
  }
  AddRef();
  return S_OK;
}

STDMETHODIMP TsfTextStore::AdviseSink(REFIID iid, IUnknown* unknown,
                                      DWORD mask) {
  if (!IsEqualGUID(iid, IID_ITextStoreACPSink))
    return TS_E_NOOBJECT;
  if (text_store_acp_sink_) {
    if (text_store_acp_sink_.IsSameObject(unknown)) {
      text_store_acp_sink_mask_ = mask;
      return S_OK;
    } else {
      return CONNECT_E_ADVISELIMIT;
    }
  }
  if (FAILED(text_store_acp_sink_.QueryFrom(unknown)))
    return E_NOINTERFACE;
  text_store_acp_sink_mask_ = mask;

  return S_OK;
}

STDMETHODIMP TsfTextStore::FindNextAttrTransition(
    LONG acp_start,
    LONG acp_halt,
    ULONG num_filter_attributes,
    const TS_ATTRID* filter_attributes,
    DWORD flags,
    LONG* acp_next,
    BOOL* found,
    LONG* found_offset) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::GetACPFromPoint(
    TsViewCookie view_cookie,
    const POINT* point,
    DWORD flags,
    LONG* acp) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::GetActiveView(TsViewCookie* view_cookie) {
  NOTIMPLEMENTED();
  if (!view_cookie)
    return E_INVALIDARG;
  return S_OK;
}

STDMETHODIMP TsfTextStore::GetEmbedded(LONG acp_pos,
                                       REFGUID service,
                                       REFIID iid,
                                       IUnknown** unknown) {
  NOTIMPLEMENTED();
  if (!unknown)
    return E_INVALIDARG;
  *unknown = NULL;
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::GetEndACP(LONG* acp) {
  NOTIMPLEMENTED();
  if (!acp)
    return E_INVALIDARG;
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::GetFormattedText(LONG acp_start, LONG acp_end,
                                            IDataObject** data_object) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::GetScreenExt(TsViewCookie view_cookie, RECT* rect) {
  NOTIMPLEMENTED();
  if (!rect)
    return E_INVALIDARG;
  SetRect(rect, 0, 0, 0, 0);
  return S_OK;
}

STDMETHODIMP TsfTextStore::GetSelection(ULONG selection_index,
                                        ULONG selection_buffer_size,
                                        TS_SELECTION_ACP* selection_buffer,
                                        ULONG* fetched_count) {
  NOTIMPLEMENTED();
  if (!selection_buffer)
    return E_INVALIDARG;
  if (!fetched_count)
    return E_INVALIDARG;
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::GetStatus(TS_STATUS* status) {
  NOTIMPLEMENTED();
  if (!status)
    return E_INVALIDARG;
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::GetText(LONG acp_start,
                                   LONG acp_end,
                                   wchar_t* text_buffer,
                                   ULONG text_buffer_size,
                                   ULONG* text_buffer_copied,
                                   TS_RUNINFO* run_info_buffer,
                                   ULONG run_info_buffer_size,
                                   ULONG* run_info_buffer_copied,
                                   LONG* next_acp) {
  NOTIMPLEMENTED();
  if (!text_buffer_copied || !run_info_buffer_copied)
    return E_INVALIDARG;
  if (!text_buffer && text_buffer_size != 0)
    return E_INVALIDARG;
  if (!run_info_buffer && run_info_buffer_size != 0)
    return E_INVALIDARG;
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::GetTextExt(TsViewCookie view_cookie,
                                      LONG acp_start,
                                      LONG acp_end,
                                      RECT* rect,
                                      BOOL* clipped) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::GetWnd(TsViewCookie view_cookie,
                                  HWND* window_handle) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::InsertEmbedded(DWORD flags,
                                          LONG acp_start,
                                          LONG acp_end,
                                          IDataObject* data_object,
                                          TS_TEXTCHANGE* change) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::InsertEmbeddedAtSelection(DWORD flags,
                                                     IDataObject* data_object,
                                                     LONG* acp_start,
                                                     LONG* acp_end,
                                                     TS_TEXTCHANGE* change) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::InsertTextAtSelection(DWORD flags,
                                                 const wchar_t* text_buffer,
                                                 ULONG text_buffer_size,
                                                 LONG* acp_start,
                                                 LONG* acp_end,
                                                 TS_TEXTCHANGE* text_change) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::QueryInsert(
    LONG acp_test_start,
    LONG acp_test_end,
    ULONG text_size,
    LONG* acp_result_start,
    LONG* acp_result_end) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::QueryInsertEmbedded(const GUID* service,
                                               const FORMATETC* format,
                                               BOOL* insertable) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::RequestAttrsAtPosition(
    LONG acp_pos,
    ULONG attribute_buffer_size,
    const TS_ATTRID* attribute_buffer,
    DWORD flags) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::RequestAttrsTransitioningAtPosition(
    LONG acp_pos,
    ULONG attribute_buffer_size,
    const TS_ATTRID* attribute_buffer,
    DWORD flags) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::RequestLock(DWORD lock_flags, HRESULT* result) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::RequestSupportedAttrs(
    DWORD flags,
    ULONG attribute_buffer_size,
    const TS_ATTRID* attribute_buffer) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::RetrieveRequestedAttrs(
    ULONG attribute_buffer_size,
    TS_ATTRVAL* attribute_buffer,
    ULONG* attribute_buffer_copied) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::SetSelection(
    ULONG selection_buffer_size,
    const TS_SELECTION_ACP* selection_buffer) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::SetText(DWORD flags,
                                   LONG acp_start,
                                   LONG acp_end,
                                   const wchar_t* text_buffer,
                                   ULONG text_buffer_size,
                                   TS_TEXTCHANGE* text_change) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::UnadviseSink(IUnknown* unknown) {
  if (!text_store_acp_sink_.IsSameObject(unknown))
    return CONNECT_E_NOCONNECTION;
  text_store_acp_sink_.Release();
  text_store_acp_sink_mask_ = 0;
  return S_OK;
}

STDMETHODIMP TsfTextStore::OnStartComposition(
    ITfCompositionView* composition_view,
    BOOL* ok) {
  if (!ok)
    *ok = TRUE;
  return S_OK;
}

STDMETHODIMP TsfTextStore::OnUpdateComposition(
    ITfCompositionView* composition_view,
    ITfRange* range) {
  return S_OK;
}

STDMETHODIMP TsfTextStore::OnEndComposition(
    ITfCompositionView* composition_view) {
  return S_OK;
}

STDMETHODIMP TsfTextStore::OnEndEdit(ITfContext* context,
                                     TfEditCookie read_only_edit_cookie,
                                     ITfEditRecord* edit_record) {
  return S_OK;
}

}  // namespace ui
