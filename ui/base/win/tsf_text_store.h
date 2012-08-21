// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_TSF_TEXT_STORE_H_
#define UI_BASE_WIN_TSF_TEXT_STORE_H_

#include <msctf.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/win/scoped_comptr.h"

namespace ui {

// TsfTextStore is used to interact with the system input method via TSF.
class TsfTextStore : public ITextStoreACP,
                     public ITfContextOwnerCompositionSink,
                     public ITfTextEditSink {
 public:
  TsfTextStore();
  ~TsfTextStore();

  virtual ULONG STDMETHODCALLTYPE AddRef() OVERRIDE;
  virtual ULONG STDMETHODCALLTYPE Release() OVERRIDE;

  // Subclasses should extend this to return any interfaces they provide.
  virtual STDMETHODIMP QueryInterface(REFIID iid, void** ppv) OVERRIDE;

  // ITextStoreACP overrides.
  virtual STDMETHODIMP AdviseSink(REFIID iid, IUnknown* unknown,
                                  DWORD mask) OVERRIDE;
  virtual STDMETHODIMP FindNextAttrTransition(
      LONG acp_start,
      LONG acp_halt,
      ULONG num_filter_attributes,
      const TS_ATTRID* filter_attributes,
      DWORD flags,
      LONG* acp_next,
      BOOL* found,
      LONG* found_offset) OVERRIDE;
  virtual STDMETHODIMP GetACPFromPoint(TsViewCookie view_cookie,
                                       const POINT* point,
                                       DWORD flags,
                                       LONG* acp) OVERRIDE;
  virtual STDMETHODIMP GetActiveView(TsViewCookie* view_cookie) OVERRIDE;
  virtual STDMETHODIMP GetEmbedded(LONG acp_pos,
                                   REFGUID service,
                                   REFIID iid,
                                   IUnknown** unknown) OVERRIDE;
  virtual STDMETHODIMP GetEndACP(LONG* acp) OVERRIDE;
  virtual STDMETHODIMP GetFormattedText(LONG acp_start,
                                        LONG acp_end,
                                        IDataObject** data_object) OVERRIDE;
  virtual STDMETHODIMP GetScreenExt(TsViewCookie view_cookie,
                                    RECT* rect) OVERRIDE;
  virtual STDMETHODIMP GetSelection(ULONG selection_index,
                                    ULONG selection_buffer_size,
                                    TS_SELECTION_ACP* selection_buffer,
                                    ULONG* fetched_count) OVERRIDE;
  virtual STDMETHODIMP GetStatus(TS_STATUS* pdcs) OVERRIDE;
  virtual STDMETHODIMP GetText(LONG acp_start,
                               LONG acp_end,
                               wchar_t* text_buffer,
                               ULONG text_buffer_size,
                               ULONG* text_buffer_copied,
                               TS_RUNINFO* run_info_buffer,
                               ULONG run_info_buffer_size,
                               ULONG* run_info_buffer_copied,
                               LONG* next_acp) OVERRIDE;
  virtual STDMETHODIMP GetTextExt(TsViewCookie view_cookie,
                                  LONG acp_start,
                                  LONG acp_end,
                                  RECT* rect,
                                  BOOL* clipped) OVERRIDE;
  virtual STDMETHODIMP GetWnd(TsViewCookie view_cookie,
                              HWND* window_handle) OVERRIDE;
  virtual STDMETHODIMP InsertEmbedded(DWORD flags,
                                      LONG acp_start,
                                      LONG acp_end,
                                      IDataObject* data_object,
                                      TS_TEXTCHANGE* change) OVERRIDE;
  virtual STDMETHODIMP InsertEmbeddedAtSelection(
      DWORD flags,
      IDataObject* data_object,
      LONG* acp_start,
      LONG* acp_end,
      TS_TEXTCHANGE* change) OVERRIDE;
  virtual STDMETHODIMP InsertTextAtSelection(
      DWORD flags,
      const wchar_t* text_buffer,
      ULONG text_buffer_size,
      LONG* acp_start,
      LONG* acp_end,
      TS_TEXTCHANGE* text_change) OVERRIDE;
  virtual STDMETHODIMP QueryInsert(LONG acp_test_start,
                                   LONG acp_test_end,
                                   ULONG text_size,
                                   LONG* acp_result_start,
                                   LONG* acp_result_end) OVERRIDE;
  virtual STDMETHODIMP QueryInsertEmbedded(const GUID* service,
                                           const FORMATETC* format,
                                           BOOL* insertable) OVERRIDE;
  virtual STDMETHODIMP RequestAttrsAtPosition(
      LONG acp_pos,
      ULONG attribute_buffer_size,
      const TS_ATTRID* attribute_buffer,
      DWORD flags) OVERRIDE;
  virtual STDMETHODIMP RequestAttrsTransitioningAtPosition(
      LONG acp_pos,
      ULONG attribute_buffer_size,
      const TS_ATTRID* attribute_buffer,
      DWORD flags) OVERRIDE;
  virtual STDMETHODIMP RequestLock(DWORD lock_flags, HRESULT* result) OVERRIDE;
  virtual STDMETHODIMP RequestSupportedAttrs(
      DWORD flags,
      ULONG attribute_buffer_size,
      const TS_ATTRID* attribute_buffer) OVERRIDE;
  virtual STDMETHODIMP RetrieveRequestedAttrs(
      ULONG attribute_buffer_size,
      TS_ATTRVAL* attribute_buffer,
      ULONG* attribute_buffer_copied) OVERRIDE;
  virtual STDMETHODIMP SetSelection(
      ULONG selection_buffer_size,
      const TS_SELECTION_ACP* selection_buffer) OVERRIDE;
  virtual STDMETHODIMP SetText(DWORD flags,
                               LONG acp_start,
                               LONG acp_end,
                               const wchar_t* text_buffer,
                               ULONG text_buffer_size,
                               TS_TEXTCHANGE* text_change) OVERRIDE;
  virtual STDMETHODIMP UnadviseSink(IUnknown* unknown) OVERRIDE;

  // ITfContextOwnerCompositionSink overrides.
  virtual STDMETHODIMP OnStartComposition(ITfCompositionView* composition_view,
                                          BOOL* ok) OVERRIDE;
  virtual STDMETHODIMP OnUpdateComposition(ITfCompositionView* composition_view,
                                           ITfRange* range) OVERRIDE;
  virtual STDMETHODIMP OnEndComposition(
      ITfCompositionView* composition_view) OVERRIDE;

  // ITfTextEditSink overrides.
  virtual STDMETHODIMP OnEndEdit(ITfContext* context,
                                 TfEditCookie read_only_edit_cookie,
                                 ITfEditRecord* edit_record) OVERRIDE;

 private:
  // The refrence count of this instance.
  volatile LONG ref_count_;

  // A pointer of ITextStoreACPSink, this instance is given in AdviseSink.
  base::win::ScopedComPtr<ITextStoreACPSink> text_store_acp_sink_;

  // The current mask of |text_store_acp_sink_|.
  DWORD text_store_acp_sink_mask_;

  DISALLOW_COPY_AND_ASSIGN(TsfTextStore);
};

}  // namespace ui

#endif  // UI_BASE_WIN_TSF_TEXT_STORE_H_
