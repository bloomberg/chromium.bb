// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/win/tsf_event_router.h"

#include <msctf.h>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/win/scoped_comptr.h"
#include "base/win/metro.h"
#include "ui/base/win/atl_module.h"

namespace ui {


// TsfEventRouter::TsfEventRouterDelegate  ------------------------------------

// The implementation class of ITfUIElementSink, whose member functions will be
// called back by TSF when the UI element status is changed, for example when
// the candidate window is opened or closed. This class also implements
// ITfTextEditSink, whose member function is called back by TSF when the text
// editting session is finished.
class ATL_NO_VTABLE TsfEventRouter::TsfEventRouterDelegate
    : public ATL::CComObjectRootEx<CComSingleThreadModel>,
      public ITfUIElementSink,
      public ITfTextEditSink {
 public:
  BEGIN_COM_MAP(TsfEventRouterDelegate)
    COM_INTERFACE_ENTRY(ITfUIElementSink)
    COM_INTERFACE_ENTRY(ITfTextEditSink)
  END_COM_MAP()

  TsfEventRouterDelegate();
  ~TsfEventRouterDelegate();

  // ITfTextEditSink:
  STDMETHOD_(HRESULT, OnEndEdit)(ITfContext* context,
                                 TfEditCookie read_only_cookie,
                                 ITfEditRecord* edit_record) OVERRIDE;

  // ITfUiElementSink:
  STDMETHOD_(HRESULT, BeginUIElement)(DWORD element_id, BOOL* is_show) OVERRIDE;
  STDMETHOD_(HRESULT, UpdateUIElement)(DWORD element_id) OVERRIDE;
  STDMETHOD_(HRESULT, EndUIElement)(DWORD element_id) OVERRIDE;

  // Sets |thread_manager| to be monitored. |thread_manager| can be NULL.
  void SetManager(ITfThreadMgr* thread_manager);

  // Returns true if the IME is composing text.
  bool IsImeComposing();

  // Sets |router| to be forwarded TSF-related events.
  void SetRouter(TsfEventRouter* router);

 private:
  // Returns true if the given |context| is composing.
  static bool IsImeComposingInternal(ITfContext* context);

  // Returns true if the given |element_id| represents the candidate window.
  bool IsCandidateWindowInternal(DWORD element_id);

  // A context associated with this class.
  base::win::ScopedComPtr<ITfContext> context_;

  // The ITfSource associated with |context_|.
  base::win::ScopedComPtr<ITfSource> context_source_;

  // The cookie for |context_source_|.
  DWORD context_source_cookie_;

  // A UIElementMgr associated with this class.
  base::win::ScopedComPtr<ITfUIElementMgr> ui_element_manager_;

  // The ITfSouce associated with |ui_element_manager_|.
  base::win::ScopedComPtr<ITfSource> ui_source_;

  // The set of currently opened candidate window ids.
  std::set<DWORD> open_candidate_window_ids_;

  // The cookie for |ui_source_|.
  DWORD ui_source_cookie_;

  TsfEventRouter* router_;

  DISALLOW_COPY_AND_ASSIGN(TsfEventRouterDelegate);
};

TsfEventRouter::TsfEventRouterDelegate::TsfEventRouterDelegate()
    : context_source_cookie_(TF_INVALID_COOKIE),
      ui_source_cookie_(TF_INVALID_COOKIE),
      router_(NULL) {
}

TsfEventRouter::TsfEventRouterDelegate::~TsfEventRouterDelegate() {}

void TsfEventRouter::TsfEventRouterDelegate::SetRouter(TsfEventRouter* router) {
  router_ = router;
}

STDMETHODIMP TsfEventRouter::TsfEventRouterDelegate::OnEndEdit(
    ITfContext* context,
    TfEditCookie read_only_cookie,
    ITfEditRecord* edit_record) {
  if (!edit_record || !context)
    return E_INVALIDARG;
  if (!router_)
    return S_OK;

  // |edit_record| can be used to obtain updated ranges in terms of text
  // contents and/or text attributes. Here we are interested only in text update
  // so we use TF_GTP_INCL_TEXT and check if there is any range which contains
  // updated text.
  base::win::ScopedComPtr<IEnumTfRanges> ranges;
  if (FAILED(edit_record->GetTextAndPropertyUpdates(TF_GTP_INCL_TEXT, NULL, 0,
                                                    ranges.Receive())))
     return S_OK;  // Don't care about failures.

  ULONG fetched_count = 0;
  base::win::ScopedComPtr<ITfRange> range;
  if (FAILED(ranges->Next(1, range.Receive(), &fetched_count)))
    return S_OK;  // Don't care about failures.

  // |fetched_count| != 0 means there is at least one range that contains
  // updated text.
  if (fetched_count != 0)
    router_->OnTextUpdated();
  return S_OK;
}

STDMETHODIMP TsfEventRouter::TsfEventRouterDelegate::BeginUIElement(
    DWORD element_id,
    BOOL* is_show) {
  if (is_show)
    *is_show = TRUE;  // Without this the UI element will not be shown.

  if (!IsCandidateWindowInternal(element_id))
    return S_OK;

  std::pair<std::set<DWORD>::iterator, bool> insert_result =
      open_candidate_window_ids_.insert(element_id);
  // Don't call if |router_| is null or |element_id| is already handled.
  if (router_ && insert_result.second)
    router_->OnCandidateWindowCountChanged(open_candidate_window_ids_.size());

  return S_OK;
}

STDMETHODIMP TsfEventRouter::TsfEventRouterDelegate::UpdateUIElement(
    DWORD element_id) {
  return S_OK;
}

STDMETHODIMP TsfEventRouter::TsfEventRouterDelegate::EndUIElement(
    DWORD element_id) {
  if ((open_candidate_window_ids_.erase(element_id) != 0) && router_)
    router_->OnCandidateWindowCountChanged(open_candidate_window_ids_.size());
  return S_OK;
}

void TsfEventRouter::TsfEventRouterDelegate::SetManager(
    ITfThreadMgr* thread_manager) {
  context_.Release();

  if (context_source_) {
    context_source_->UnadviseSink(context_source_cookie_);
    context_source_.Release();
  }
  context_source_cookie_ = TF_INVALID_COOKIE;

  ui_element_manager_.Release();
  if (ui_source_) {
    ui_source_->UnadviseSink(ui_source_cookie_);
    ui_source_.Release();
  }
  ui_source_cookie_ = TF_INVALID_COOKIE;

  if (!thread_manager)
    return;

  base::win::ScopedComPtr<ITfDocumentMgr> document_manager;
  if (FAILED(thread_manager->GetFocus(document_manager.Receive())) ||
      FAILED(document_manager->GetBase(context_.Receive())) ||
      FAILED(context_source_.QueryFrom(context_)))
    return;
  context_source_->AdviseSink(IID_ITfTextEditSink,
                              static_cast<ITfTextEditSink*>(this),
                              &context_source_cookie_);

  if (FAILED(ui_element_manager_.QueryFrom(thread_manager)) ||
      FAILED(ui_source_.QueryFrom(ui_element_manager_)))
    return;
  ui_source_->AdviseSink(IID_ITfUIElementSink,
                         static_cast<ITfUIElementSink*>(this),
                         &ui_source_cookie_);
}

bool TsfEventRouter::TsfEventRouterDelegate::IsImeComposing() {
  return context_ && IsImeComposingInternal(context_);
}

// static
bool TsfEventRouter::TsfEventRouterDelegate::IsImeComposingInternal(
    ITfContext* context) {
  DCHECK(context);
  base::win::ScopedComPtr<ITfContextComposition> context_composition;
  if (FAILED(context_composition.QueryFrom(context)))
    return false;
  base::win::ScopedComPtr<IEnumITfCompositionView> enum_composition_view;
  if (FAILED(context_composition->EnumCompositions(
      enum_composition_view.Receive())))
    return false;
  base::win::ScopedComPtr<ITfCompositionView> composition_view;
  return enum_composition_view->Next(1, composition_view.Receive(),
                                     NULL) == S_OK;
}

bool TsfEventRouter::TsfEventRouterDelegate::IsCandidateWindowInternal(
    DWORD element_id) {
  DCHECK(ui_element_manager_.get());
  base::win::ScopedComPtr<ITfUIElement> ui_element;
  if (FAILED(ui_element_manager_->GetUIElement(element_id,
                                               ui_element.Receive())))
    return false;
  base::win::ScopedComPtr<ITfCandidateListUIElement> candidate_list_ui_element;
  return SUCCEEDED(candidate_list_ui_element.QueryFrom(ui_element));
}


// TsfEventRouter  ------------------------------------------------------------

TsfEventRouter::TsfEventRouter(TsfEventRouterObserver* observer)
    : observer_(observer),
      delegate_(NULL) {
  DCHECK(base::win::IsTsfAwareRequired())
      << "Do not use TsfEventRouter without TSF environment.";
  DCHECK(observer_);
  CComObject<TsfEventRouterDelegate>* delegate;
  ui::win::CreateATLModuleIfNeeded();
  if (SUCCEEDED(CComObject<TsfEventRouterDelegate>::CreateInstance(
      &delegate))) {
    delegate->AddRef();
    delegate_.Attach(delegate);
    delegate_->SetRouter(this);
  }
}

TsfEventRouter::~TsfEventRouter() {
  if (delegate_) {
    delegate_->SetManager(NULL);
    delegate_->SetRouter(NULL);
  }
}

void TsfEventRouter::SetManager(ITfThreadMgr* thread_manager) {
  delegate_->SetManager(thread_manager);
}

bool TsfEventRouter::IsImeComposing() {
  return delegate_->IsImeComposing();
}

void TsfEventRouter::OnTextUpdated() {
  observer_->OnTextUpdated();
}

void TsfEventRouter::OnCandidateWindowCountChanged(size_t window_count) {
  observer_->OnCandidateWindowCountChanged(window_count);
}

}  // namespace ui
