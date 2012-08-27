// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_TSF_TEXT_STORE_H_
#define UI_BASE_WIN_TSF_TEXT_STORE_H_

#include <msctf.h>
#include <deque>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "base/win/scoped_comptr.h"
#include "ui/base/ime/composition_underline.h"
#include "ui/base/range/range.h"
#include "ui/base/ui_export.h"

namespace ui {
class TextInputClient;

// TsfTextStore is used to interact with the input method via TSF manager.
// TsfTextStore have a string buffer which is manipulated by TSF manager through
// ITextStoreACP interface methods such as SetText().
// When the input method updates the composition, TsfTextStore calls
// TextInputClient::SetCompositionText(). And when the input method finishes the
// composition, TsfTextStore calls TextInputClient::InsertText() and clears the
// buffer.
//
// How TsfTextStore works:
//  - The user enters "a".
//    - The input method set composition as "a".
//    - TSF manager calls TsfTextStore::RequestLock().
//    - TsfTextStore callbacks ITextStoreACPSink::OnLockGranted().
//    - In OnLockGranted(), TSF manager calls
//      - TsfTextStore::OnStartComposition()
//      - TsfTextStore::SetText()
//        The string buffer is set as "a".
//      - TsfTextStore::OnUpdateComposition()
//      - TsfTextStore::OnEndEdit()
//        TsfTextStore can get the composition information such as underlines.
//   - TsfTextStore calls TextInputClient::SetCompositionText().
//     "a" is shown with an underline as composition string.
// - The user enters <space>.
//    - The input method set composition as "A".
//    - TSF manager calls TsfTextStore::RequestLock().
//    - TsfTextStore callbacks ITextStoreACPSink::OnLockGranted().
//    - In OnLockGranted(), TSF manager calls
//      - TsfTextStore::SetText()
//        The string buffer is set as "A".
//      - TsfTextStore::OnUpdateComposition()
//      - TsfTextStore::OnEndEdit()
//   - TsfTextStore calls TextInputClient::SetCompositionText().
//     "A" is shown with an underline as composition string.
// - The user enters <enter>.
//    - The input method commits "A".
//    - TSF manager calls TsfTextStore::RequestLock().
//    - TsfTextStore callbacks ITextStoreACPSink::OnLockGranted().
//    - In OnLockGranted(), TSF manager calls
//      - TsfTextStore::OnEndComposition()
//      - TsfTextStore::OnEndEdit()
//        TsfTextStore knows "A" is committed.
//   - TsfTextStore calls TextInputClient::InsertText().
//     "A" is shown as committed string.
//   - TsfTextStore clears the string buffer.
//   - TsfTextStore calls OnSelectionChange(), OnLayoutChange() and
//     OnTextChange() of ITextStoreACPSink to let TSF manager know that the
//     string buffer has been changed.
//
// About the locking scheme:
// When TSF manager manipulates the string buffer it calls RequestLock() to get
// the lock of the document. If TsfTextStore can grant the lock request, it
// callbacks ITextStoreACPSink::OnLockGranted().
// RequestLock() is called from only one thread, but called recursively in
// OnLockGranted() or OnSelectionChange() or OnLayoutChange() or OnTextChange().
// If the document is locked and the lock request is asynchronous, TsfTextStore
// queues the request. The queued requests will be handled after the current
// lock is removed.
// More information about document locks can be found here:
//   http://msdn.microsoft.com/en-us/library/ms538064
//
// More information about TSF can be found here:
//   http://msdn.microsoft.com/en-us/library/ms629032
class UI_EXPORT TsfTextStore : public ITextStoreACP,
                               public ITfContextOwnerCompositionSink,
                               public ITfTextEditSink {
 public:
  TsfTextStore();
  virtual ~TsfTextStore();

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

  // Sets currently focused TextInputClient.
  void SetFocusedTextInputClient(HWND focused_window,
                                 TextInputClient* text_input_client);
  // Removes currently focused TextInputClient.
  void RemoveFocusedTextInputClient(TextInputClient* text_input_client);

  // Sends OnLayoutChange() via |text_store_acp_sink_|.
  void SendOnLayoutChange();

 private:
  friend class TsfTextStoreTest;
  friend class TsfTextStoreTestCallback;

  // Checks if the document has a read-only lock.
  bool HasReadLock() const;

  // Checks if the document has a read and write lock.
  bool HasReadWriteLock() const;

  // Gets the display attribute structure.
  bool GetDisplayAttribute(TfGuidAtom guid_atom,
                           TF_DISPLAYATTRIBUTE* attribute);

  // Gets the committed string size and underline information of the context.
  bool GetCompositionStatus(ITfContext* context,
                            const TfEditCookie read_only_edit_cookie,
                            size_t* committed_size,
                            CompositionUnderlines* undelines);

  // The refrence count of this instance.
  volatile LONG ref_count_;

  // A pointer of ITextStoreACPSink, this instance is given in AdviseSink.
  base::win::ScopedComPtr<ITextStoreACPSink> text_store_acp_sink_;

  // The current mask of |text_store_acp_sink_|.
  DWORD text_store_acp_sink_mask_;

  // HWND of the current view window which is set in SetFocusedTextInputClient.
  HWND window_handle_;

  // Current TextInputClient which is set in SetFocusedTextInputClient.
  TextInputClient* text_input_client_;

  //  |string_buffer_| contains committed string and composition string.
  //  Example: "aoi" is committed, and "umi" is under composition.
  //    |string_buffer_|: "aoiumi"
  //    |committed_size_|: 3
  string16 string_buffer_;
  size_t committed_size_;

  //  |selection_start_| and |selection_end_| indicates the selection range.
  //  Example: "iue" is selected
  //    |string_buffer_|: "aiueo"
  //    |selection_.start()|: 1
  //    |selection_.end()|: 4
  Range selection_;

  //  |start_offset| and |end_offset| of |composition_undelines_| indicates
  //  the offsets in |string_buffer_|.
  //  Example: "aoi" is committed. There are two underlines in "umi" and "no".
  //    |string_buffer_|: "aoiumino"
  //    |committed_size_|: 3
  //    composition_undelines_.underlines[0].start_offset: 3
  //    composition_undelines_.underlines[0].end_offset: 6
  //    composition_undelines_.underlines[1].start_offset: 6
  //    composition_undelines_.underlines[1].end_offset: 8
  CompositionUnderlines composition_undelines_;

  // |edit_flag_| indicates that the status is edited during
  // ITextStoreACPSink::OnLockGranted().
  bool edit_flag_;

  // The type of current lock.
  //   0: No lock.
  //   TS_LF_READ: read-only lock.
  //   TS_LF_READWRITE: read/write lock.
  DWORD current_lock_type_;

  // Queue of the lock request used in RequestLock().
  std::deque<DWORD> lock_queue_;

  // Category manager and Display attribute manager are used to obtain the
  // attributes of the composition string.
  base::win::ScopedComPtr<ITfCategoryMgr> category_manager_;
  base::win::ScopedComPtr<ITfDisplayAttributeMgr> display_attribute_manager_;

  DISALLOW_COPY_AND_ASSIGN(TsfTextStore);
};

}  // namespace ui

#endif  // UI_BASE_WIN_TSF_TEXT_STORE_H_