// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/tsf_text_store.h"

#include <OleCtl.h>

#include "base/win/scoped_variant.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/gfx/rect.h"

namespace ui {
namespace {

// We support only one view.
const TsViewCookie kViewCookie = 1;

}  // namespace

TsfTextStore::TsfTextStore()
    : ref_count_(0),
      text_store_acp_sink_mask_(0),
      window_handle_(NULL),
      text_input_client_(NULL),
      committed_size_(0),
      edit_flag_(false),
      current_lock_type_(0) {
  if (FAILED(category_manager_.CreateInstance(CLSID_TF_CategoryMgr))) {
    LOG(FATAL) << "Failed to initialize CategoryMgr.";
    return;
  }
  if (FAILED(display_attribute_manager_.CreateInstance(
          CLSID_TF_DisplayAttributeMgr))) {
    LOG(FATAL) << "Failed to initialize DisplayAttributeMgr.";
    return;
  }
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
    return E_INVALIDARG;
  if (text_store_acp_sink_) {
    if (text_store_acp_sink_.IsSameObject(unknown)) {
      text_store_acp_sink_mask_ = mask;
      return S_OK;
    } else {
      return CONNECT_E_ADVISELIMIT;
    }
  }
  if (FAILED(text_store_acp_sink_.QueryFrom(unknown)))
    return E_UNEXPECTED;
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
  if (!acp_next || !found || !found_offset)
    return E_INVALIDARG;
  // We don't support any attributes.
  // So we always return "not found".
  *acp_next = 0;
  *found = FALSE;
  *found_offset = 0;
  return S_OK;
}

STDMETHODIMP TsfTextStore::GetACPFromPoint(
    TsViewCookie view_cookie,
    const POINT* point,
    DWORD flags,
    LONG* acp) {
  NOTIMPLEMENTED();
  if (view_cookie != kViewCookie)
    return E_INVALIDARG;
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::GetActiveView(TsViewCookie* view_cookie) {
  if (!view_cookie)
    return E_INVALIDARG;
  // We support only one view.
  *view_cookie = kViewCookie;
  return S_OK;
}

STDMETHODIMP TsfTextStore::GetEmbedded(LONG acp_pos,
                                       REFGUID service,
                                       REFIID iid,
                                       IUnknown** unknown) {
  // We don't support any embedded objects.
  NOTIMPLEMENTED();
  if (!unknown)
    return E_INVALIDARG;
  *unknown = NULL;
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::GetEndACP(LONG* acp) {
  if (!acp)
    return E_INVALIDARG;
  if (!HasReadLock())
    return TS_E_NOLOCK;
  *acp = string_buffer_.size();
  return S_OK;
}

STDMETHODIMP TsfTextStore::GetFormattedText(LONG acp_start, LONG acp_end,
                                            IDataObject** data_object) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::GetScreenExt(TsViewCookie view_cookie, RECT* rect) {
  NOTIMPLEMENTED();
  if (view_cookie != kViewCookie)
    return E_INVALIDARG;
  if (!rect)
    return E_INVALIDARG;
  SetRect(rect, 0, 0, 0, 0);
  return S_OK;
}

STDMETHODIMP TsfTextStore::GetSelection(ULONG selection_index,
                                        ULONG selection_buffer_size,
                                        TS_SELECTION_ACP* selection_buffer,
                                        ULONG* fetched_count) {
  if (!selection_buffer)
    return E_INVALIDARG;
  if (!fetched_count)
    return E_INVALIDARG;
  if (!HasReadLock())
    return TS_E_NOLOCK;
  *fetched_count = 0;
  if ((selection_buffer_size > 0) &&
      ((selection_index == 0) || (selection_index == TS_DEFAULT_SELECTION))) {
    selection_buffer[0].acpStart = selection_.start();
    selection_buffer[0].acpEnd = selection_.end();
    selection_buffer[0].style.ase = TS_AE_END;
    selection_buffer[0].style.fInterimChar = FALSE;
    *fetched_count = 1;
  }
  return S_OK;
}

STDMETHODIMP TsfTextStore::GetStatus(TS_STATUS* status) {
  if (!status)
    return E_INVALIDARG;

  status->dwDynamicFlags = 0;
  // We don't support hidden text.
  status->dwStaticFlags = TS_SS_NOHIDDENTEXT;

  return S_OK;
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
  if (!text_buffer_copied || !run_info_buffer_copied)
    return E_INVALIDARG;
  if (!text_buffer && text_buffer_size != 0)
    return E_INVALIDARG;
  if (!run_info_buffer && run_info_buffer_size != 0)
    return E_INVALIDARG;
  if (!next_acp)
    return E_INVALIDARG;
  if (!HasReadLock())
    return TF_E_NOLOCK;
  const LONG string_buffer_size = string_buffer_.size();
  if (acp_end == -1)
    acp_end = string_buffer_size;
  if (!((0 <= acp_start) &&
        (acp_start <= acp_end) &&
        (acp_end <= string_buffer_size))) {
    return TF_E_INVALIDPOS;
  }
  acp_end = std::min(acp_end, acp_start + static_cast<LONG>(text_buffer_size));
  *text_buffer_copied = acp_end - acp_start;

  const string16& result =
      string_buffer_.substr(acp_start, *text_buffer_copied);
  for (size_t i = 0; i < result.size(); ++i) {
    text_buffer[i] = result[i];
  }

  if (run_info_buffer_size) {
    run_info_buffer[0].uCount = *text_buffer_copied;
    run_info_buffer[0].type = TS_RT_PLAIN;
    *run_info_buffer_copied = 1;
  }

  *next_acp = acp_end;
  return S_OK;
}

STDMETHODIMP TsfTextStore::GetTextExt(TsViewCookie view_cookie,
                                      LONG acp_start,
                                      LONG acp_end,
                                      RECT* rect,
                                      BOOL* clipped) {
  if (!rect || !clipped)
    return E_INVALIDARG;
  if (!text_input_client_)
    return E_UNEXPECTED;
  if (view_cookie != kViewCookie)
    return E_INVALIDARG;
  if (!HasReadLock())
    return TS_E_NOLOCK;
  if (!((static_cast<LONG>(committed_size_) <= acp_start) &&
       (acp_start <= acp_end) &&
       (acp_end <= static_cast<LONG>(string_buffer_.size())))) {
    return TS_E_INVALIDPOS;
  }

  gfx::Rect result;
  gfx::Rect tmp_rect;
  const uint32 start_pos = acp_start - committed_size_;
  const uint32 end_pos = acp_end - committed_size_;

  if (start_pos == end_pos) {
    // According to MSDN document, if |acp_start| and |acp_end| are equal it is
    // OK to just return E_INVALIDARG.
    // http://msdn.microsoft.com/en-us/library/ms538435
    // But when using Pinin IME of Windows 8, this method is called with the
    // equal values of |acp_start| and |acp_end|. So we handle this condition.
    if (start_pos == 0) {
      if (text_input_client_->GetCompositionCharacterBounds(0, &tmp_rect)) {
        result = tmp_rect;
        result.set_width(0);
      } else if (string_buffer_.size() == committed_size_) {
        result = text_input_client_->GetCaretBounds();
      } else {
        return TS_E_NOLAYOUT;
      }
    } else if (text_input_client_->GetCompositionCharacterBounds(start_pos - 1,
                                                                 &tmp_rect)) {
      result.set_x(tmp_rect.right());
      result.set_y(tmp_rect.y());
      result.set_width(0);
      result.set_height(tmp_rect.height());
    } else {
      return TS_E_NOLAYOUT;
    }
  } else {
    if (!text_input_client_->GetCompositionCharacterBounds(start_pos, &result))
      return TS_E_NOLAYOUT;

    for (uint32 i = start_pos + 1; i < end_pos; ++i) {
      if (!text_input_client_->GetCompositionCharacterBounds(i, &tmp_rect))
        return TS_E_NOLAYOUT;
      result = result.Union(tmp_rect);
    }
  }

  *rect =  result.ToRECT();
  *clipped = FALSE;
  return S_OK;
}

STDMETHODIMP TsfTextStore::GetWnd(TsViewCookie view_cookie,
                                  HWND* window_handle) {
  if (!window_handle)
    return E_INVALIDARG;
  if (view_cookie != kViewCookie)
    return E_INVALIDARG;
  *window_handle = window_handle_;
  return S_OK;
}

STDMETHODIMP TsfTextStore::InsertEmbedded(DWORD flags,
                                          LONG acp_start,
                                          LONG acp_end,
                                          IDataObject* data_object,
                                          TS_TEXTCHANGE* change) {
  // We don't support any embedded objects.
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::InsertEmbeddedAtSelection(DWORD flags,
                                                     IDataObject* data_object,
                                                     LONG* acp_start,
                                                     LONG* acp_end,
                                                     TS_TEXTCHANGE* change) {
  // We don't support any embedded objects.
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP TsfTextStore::InsertTextAtSelection(DWORD flags,
                                                 const wchar_t* text_buffer,
                                                 ULONG text_buffer_size,
                                                 LONG* acp_start,
                                                 LONG* acp_end,
                                                 TS_TEXTCHANGE* text_change) {
  const LONG start_pos = selection_.start();
  const LONG end_pos = selection_.end();
  const LONG new_end_pos = start_pos + text_buffer_size;

  if (flags & TS_IAS_QUERYONLY) {
    if (!HasReadLock())
      return TS_E_NOLOCK;
    if (acp_start)
      *acp_start = start_pos;
    if (acp_end) {
      *acp_end = new_end_pos;
    }
    return S_OK;
  }

  if (!HasReadWriteLock())
    return TS_E_NOLOCK;
  if (!text_buffer)
    return E_INVALIDARG;

  DCHECK_LE(start_pos, end_pos);
  string_buffer_ = string_buffer_.substr(0, start_pos) +
                   string16(text_buffer, text_buffer + text_buffer_size) +
                   string_buffer_.substr(end_pos);
  if (acp_start)
    *acp_start = start_pos;
  if (acp_end)
    *acp_end = new_end_pos;
  if (text_change) {
    text_change->acpStart = start_pos;
    text_change->acpOldEnd = end_pos;
    text_change->acpNewEnd = new_end_pos;
  }
  selection_.set_start(start_pos);
  selection_.set_end(new_end_pos);
  return S_OK;
}

STDMETHODIMP TsfTextStore::QueryInsert(
    LONG acp_test_start,
    LONG acp_test_end,
    ULONG text_size,
    LONG* acp_result_start,
    LONG* acp_result_end) {
  if (!acp_result_start || !acp_result_end)
    return E_INVALIDARG;
  if (!((static_cast<LONG>(committed_size_) <= acp_test_start) &&
        (acp_test_start <= acp_test_end) &&
        (acp_test_end <= static_cast<LONG>(string_buffer_.size())))) {
    return E_INVALIDARG;
  }
  *acp_result_start = acp_test_start;
  *acp_result_end = acp_test_start + text_size;
  return S_OK;
}

STDMETHODIMP TsfTextStore::QueryInsertEmbedded(const GUID* service,
                                               const FORMATETC* format,
                                               BOOL* insertable) {
  if (!format)
    return E_INVALIDARG;
  // We don't support any embedded objects.
  if (insertable)
    *insertable = FALSE;
  return S_OK;
}

STDMETHODIMP TsfTextStore::RequestAttrsAtPosition(
    LONG acp_pos,
    ULONG attribute_buffer_size,
    const TS_ATTRID* attribute_buffer,
    DWORD flags) {
  // We don't support any document attributes.
  // This method just returns S_OK, and the subsequently called
  // RetrieveRequestedAttrs() returns 0 as the number of supported attributes.
  return S_OK;
}

STDMETHODIMP TsfTextStore::RequestAttrsTransitioningAtPosition(
    LONG acp_pos,
    ULONG attribute_buffer_size,
    const TS_ATTRID* attribute_buffer,
    DWORD flags) {
  // We don't support any document attributes.
  // This method just returns S_OK, and the subsequently called
  // RetrieveRequestedAttrs() returns 0 as the number of supported attributes.
  return S_OK;
}

STDMETHODIMP TsfTextStore::RequestLock(DWORD lock_flags, HRESULT* result) {
  if (!text_store_acp_sink_.get())
    return E_FAIL;
  if (!result)
    return E_INVALIDARG;

  if (current_lock_type_ != 0) {
    if (lock_flags & TS_LF_SYNC) {
      // Can't lock synchronously.
      *result = TS_E_SYNCHRONOUS;
      return S_OK;
    }
    // Queue the lock request.
    lock_queue_.push_back(lock_flags & TS_LF_READWRITE);
    *result = TS_S_ASYNC;
    return S_OK;
  }

  // Lock
  current_lock_type_ = (lock_flags & TS_LF_READWRITE);

  edit_flag_ = false;
  const size_t last_committed_size = committed_size_;

  // Grant the lock.
  *result = text_store_acp_sink_->OnLockGranted(current_lock_type_);

  // Unlock
  current_lock_type_ = 0;

  // Handles the pending lock requests.
  while (!lock_queue_.empty()) {
    current_lock_type_ = lock_queue_.front();
    lock_queue_.pop_front();
    text_store_acp_sink_->OnLockGranted(current_lock_type_);
    current_lock_type_ = 0;
  }

  if (!edit_flag_) {
    return S_OK;
  }

  // If the text store is edited in OnLockGranted(), we may need to call
  // TextInputClient::InsertText() or TextInputClient::SetCompositionText().
  const size_t new_committed_size = committed_size_;
  const string16& new_committed_string =
      string_buffer_.substr(last_committed_size,
                            new_committed_size - last_committed_size);
  const string16& composition_string =
      string_buffer_.substr(new_committed_size);

  // If there is new committed string, calls TextInputClient::InsertText().
  if ((!new_committed_string.empty()) && text_input_client_) {
    text_input_client_->InsertText(new_committed_string);
  }

  // Calls TextInputClient::SetCompositionText().
  CompositionText composition_text;
  composition_text.text = composition_string;
  composition_text.underlines = composition_undelines_;
  // Adjusts the offset.
  for (size_t i = 0; i < composition_text.underlines.size(); ++i) {
    composition_text.underlines[i].start_offset -= new_committed_size;
    composition_text.underlines[i].end_offset -= new_committed_size;
  }
  if (selection_.start() < new_committed_size) {
    composition_text.selection.set_start(0);
  } else {
    composition_text.selection.set_start(
        selection_.start() - new_committed_size);
  }
  if (selection_.end() < new_committed_size) {
    composition_text.selection.set_end(0);
  } else {
    composition_text.selection.set_end(selection_.end() - new_committed_size);
  }
  if (text_input_client_)
    text_input_client_->SetCompositionText(composition_text);

  // If there is no composition string, clear the text store status.
  // And call OnSelectionChange(), OnLayoutChange(), and OnTextChange().
  if ((composition_string.empty()) && (new_committed_size != 0)) {
    string_buffer_.clear();
    committed_size_ = 0;
    selection_.set_start(0);
    selection_.set_end(0);
    if (text_store_acp_sink_mask_ & TS_AS_SEL_CHANGE)
      text_store_acp_sink_->OnSelectionChange();
    if (text_store_acp_sink_mask_ & TS_AS_LAYOUT_CHANGE)
      text_store_acp_sink_->OnLayoutChange(TS_LC_CHANGE, 0);
    if (text_store_acp_sink_mask_ & TS_AS_TEXT_CHANGE) {
      TS_TEXTCHANGE textChange;
      textChange.acpStart = 0;
      textChange.acpOldEnd = new_committed_size;
      textChange.acpNewEnd = 0;
      text_store_acp_sink_->OnTextChange(0, &textChange);
    }
  }

  return S_OK;
}

STDMETHODIMP TsfTextStore::RequestSupportedAttrs(
    DWORD flags,
    ULONG attribute_buffer_size,
    const TS_ATTRID* attribute_buffer) {
  // We don't support any document attributes.
  // This method just returns S_OK, and the subsequently called
  // RetrieveRequestedAttrs() returns 0 as the number of supported attributes.
  return S_OK;
}

STDMETHODIMP TsfTextStore::RetrieveRequestedAttrs(
    ULONG attribute_buffer_size,
    TS_ATTRVAL* attribute_buffer,
    ULONG* attribute_buffer_copied) {
  if (!attribute_buffer_copied)
    return E_INVALIDARG;
  // We don't support any document attributes.
  attribute_buffer_copied = 0;
  return S_OK;
}

STDMETHODIMP TsfTextStore::SetSelection(
    ULONG selection_buffer_size,
    const TS_SELECTION_ACP* selection_buffer) {
  if (!HasReadWriteLock())
    return TF_E_NOLOCK;
  if (selection_buffer_size > 0) {
    const LONG start_pos = selection_buffer[0].acpStart;
    const LONG end_pos = selection_buffer[0].acpEnd;
    if (!((static_cast<LONG>(committed_size_) <= start_pos) &&
          (start_pos <= end_pos) &&
          (end_pos <= static_cast<LONG>(string_buffer_.size())))) {
      return TF_E_INVALIDPOS;
    }
    selection_.set_start(start_pos);
    selection_.set_end(end_pos);
  }
  return S_OK;
}

STDMETHODIMP TsfTextStore::SetText(DWORD flags,
                                   LONG acp_start,
                                   LONG acp_end,
                                   const wchar_t* text_buffer,
                                   ULONG text_buffer_size,
                                   TS_TEXTCHANGE* text_change) {
  if (!HasReadWriteLock())
    return TS_E_NOLOCK;
  if (!((static_cast<LONG>(committed_size_) <= acp_start) &&
        (acp_start <= acp_end) &&
        (acp_end <= static_cast<LONG>(string_buffer_.size())))) {
    return TS_E_INVALIDPOS;
  }

  TS_SELECTION_ACP selection;
  selection.acpStart = acp_start;
  selection.acpEnd = acp_end;
  selection.style.ase = TS_AE_NONE;
  selection.style.fInterimChar = 0;

  HRESULT ret;
  ret = SetSelection(1, &selection);
  if (ret != S_OK)
    return ret;

  TS_TEXTCHANGE change;
  ret = InsertTextAtSelection(0, text_buffer, text_buffer_size,
                              &acp_start, &acp_end, &change);
  if (ret != S_OK)
    return ret;

  if (text_change)
    *text_change = change;

  return S_OK;
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
  if (ok)
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
  if (!context || !edit_record)
    return E_INVALIDARG;

  size_t committed_size;
  CompositionUnderlines undelines;
  if (!GetCompositionStatus(context, read_only_edit_cookie, &committed_size,
                            &undelines)) {
    return S_OK;
  }
  composition_undelines_ = undelines;
  committed_size_ = committed_size;
  edit_flag_ = true;
  return S_OK;
}

bool TsfTextStore::GetDisplayAttribute(TfGuidAtom guid_atom,
                                       TF_DISPLAYATTRIBUTE* attribute) {
  GUID guid;
  if (FAILED(category_manager_->GetGUID(guid_atom, &guid)))
    return false;

  base::win::ScopedComPtr<ITfDisplayAttributeInfo> display_attribute_info;
  if (FAILED(display_attribute_manager_->GetDisplayAttributeInfo(
          guid, display_attribute_info.Receive(), NULL))) {
    return false;
  }
  return SUCCEEDED(display_attribute_info->GetAttributeInfo(attribute));
}

bool TsfTextStore::GetCompositionStatus(
    ITfContext* context,
    const TfEditCookie read_only_edit_cookie,
    size_t* committed_size,
    CompositionUnderlines* undelines) {
  DCHECK(context);
  DCHECK(committed_size);
  DCHECK(undelines);
  const GUID* rgGuids[2] = {&GUID_PROP_COMPOSING, &GUID_PROP_ATTRIBUTE};
  base::win::ScopedComPtr<ITfReadOnlyProperty> track_property;
  if (FAILED(context->TrackProperties(rgGuids, 2, NULL, 0,
                                      track_property.Receive()))) {
    return false;
  }

  *committed_size = 0;
  undelines->clear();
  base::win::ScopedComPtr<ITfRange> start_to_end_range;
  base::win::ScopedComPtr<ITfRange> end_range;
  if (FAILED(context->GetStart(read_only_edit_cookie,
                               start_to_end_range.Receive()))) {
    return false;
  }
  if (FAILED(context->GetEnd(read_only_edit_cookie, end_range.Receive())))
    return false;
  if (FAILED(start_to_end_range->ShiftEndToRange(read_only_edit_cookie,
                                                 end_range, TF_ANCHOR_END))) {
    return false;
  }

  base::win::ScopedComPtr<IEnumTfRanges> ranges;
  if (FAILED(track_property->EnumRanges(read_only_edit_cookie, ranges.Receive(),
                                        start_to_end_range))) {
    return false;
  }

  while (true) {
    base::win::ScopedComPtr<ITfRange> range;
    if (ranges->Next(1, range.Receive(), NULL) != S_OK)
      break;
    base::win::ScopedVariant value;
    base::win::ScopedComPtr<IEnumTfPropertyValue> enum_prop_value;
    if (FAILED(track_property->GetValue(read_only_edit_cookie, range,
                                        value.Receive()))) {
      return false;
    }
    if (FAILED(enum_prop_value.QueryFrom(value.AsInput()->punkVal)))
      return false;

    TF_PROPERTYVAL property_value;
    bool is_composition = false;
    bool has_display_attribute = false;
    TF_DISPLAYATTRIBUTE display_attribute;
    while (enum_prop_value->Next(1, &property_value, NULL) == S_OK) {
      if (IsEqualGUID(property_value.guidId, GUID_PROP_COMPOSING)) {
        is_composition = (property_value.varValue.lVal == TRUE);
      } else if (IsEqualGUID(property_value.guidId, GUID_PROP_ATTRIBUTE)) {
        TfGuidAtom guid_atom =
            static_cast<TfGuidAtom>(property_value.varValue.lVal);
        if (GetDisplayAttribute(guid_atom, &display_attribute))
          has_display_attribute = true;
      }
      VariantClear(&property_value.varValue);
    }

    base::win::ScopedComPtr<ITfRangeACP> range_acp;
    range_acp.QueryFrom(range);
    LONG start_pos, length;
    range_acp->GetExtent(&start_pos, &length);
    if (!is_composition) {
      if (*committed_size < static_cast<size_t>(start_pos + length))
        *committed_size = start_pos + length;
    } else {
      CompositionUnderline underline;
      underline.start_offset = start_pos;
      underline.end_offset = start_pos + length;
      underline.color = SK_ColorBLACK;
      if (has_display_attribute)
        underline.thick = !!display_attribute.fBoldLine;
      undelines->push_back(underline);
    }
  }
  return true;
}

void TsfTextStore::SetFocusedTextInputClient(
    HWND focused_window,
    TextInputClient* text_input_client) {
  window_handle_ = focused_window;
  text_input_client_ = text_input_client;
}

void TsfTextStore::RemoveFocusedTextInputClient(
    TextInputClient* text_input_client) {
  if (text_input_client_ == text_input_client) {
    window_handle_ = NULL;
    text_input_client_ = NULL;
  }
}

void TsfTextStore::SendOnLayoutChange() {
  if (text_store_acp_sink_ && (text_store_acp_sink_mask_ & TS_AS_LAYOUT_CHANGE))
    text_store_acp_sink_->OnLayoutChange(TS_LC_CHANGE, 0);
}

bool TsfTextStore::HasReadLock() const {
  return (current_lock_type_ & TS_LF_READ) == TS_LF_READ;
}

bool TsfTextStore::HasReadWriteLock() const {
  return (current_lock_type_ & TS_LF_READWRITE) == TS_LF_READWRITE;
}

}  // namespace ui
