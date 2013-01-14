// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <msctf.h>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/threading/thread_local_storage.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_variant.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/win/tsf_bridge.h"
#include "ui/base/ime/win/tsf_text_store.h"

namespace ui {

namespace {

// We use thread local storage for TSFBridge lifespan management.
base::ThreadLocalStorage::StaticSlot tls_tsf_bridge = TLS_INITIALIZER;


// TsfBridgeDelegate -----------------------------------------------------------

// A TLS implementation of TSFBridge.
class TSFBridgeDelegate : public TSFBridge {
 public:
  TSFBridgeDelegate();
  virtual ~TSFBridgeDelegate();

  bool Initialize();

  // TsfBridge:
  virtual void Shutdown() OVERRIDE;
  virtual void OnTextInputTypeChanged(TextInputClient* client) OVERRIDE;
  virtual bool CancelComposition() OVERRIDE;
  virtual void SetFocusedClient(HWND focused_window,
                                TextInputClient* client) OVERRIDE;
  virtual void RemoveFocusedClient(TextInputClient* client) OVERRIDE;
  virtual base::win::ScopedComPtr<ITfThreadMgr> GetThreadManager() OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<TSFBridgeDelegate>;

  // Initializes |document_manager_for_editable_|.
  bool InitializeForEnabledDocumentManager();

  // Initializes |document_manager_for_password_|.
  bool InitializeForPasswordDocumentManager();

  // Initializes |document_manager_for_non_editable_|.
  bool InitializeForDisabledDocumentManager();

  // Creates |document_manager| with |text_store|. If text store is not
  // necessary, pass |text_store| and |source_cookie| as NULL.
  bool CreateDocumentManager(TSFTextStore* text_store,
                             ITfDocumentMgr** document_manager,
                             ITfContext** context,
                             DWORD* source_cookie);

  // Returns true if |document_manager| is the focused document manager.
  bool IsFocused(base::win::ScopedComPtr<ITfDocumentMgr> document_manager);

  // Returns true if already initialized.
  bool IsInitialized();

  // An ITfContext object to be used in normal text field. This document manager
  // contains a TSFTextStore instance and an activated context.
  base::win::ScopedComPtr<ITfDocumentMgr> document_manager_for_editable_;

  // Altough TSF support IME enable/disable switching without context changing,
  // some IMEs don't change their state. So we should switch a focus to a
  // |document_manager_for_non_editable_| which contains deactivated context to
  // disable IMEs.
  base::win::ScopedComPtr<ITfDocumentMgr> document_manager_for_non_editable_;

  // In the case of password field, we can't use
  // |document_manager_for_non_editable_| because password field should accept
  // key input but IMEs must not be enbaled. In addition to this, a user expects
  // on-screen keyboard layout should be changed to a sutable one on the
  // password field. To achieve these requirements, we use a different document
  // manager where 1) the context has special TSF compartments to specify IME
  // status, and 2) TSF TextStore has a special input scope to specify password
  // type keyboard layout.
  base::win::ScopedComPtr<ITfDocumentMgr> document_manager_for_password_;

  // An ITfThreadMgr object to be used in focus and document management.
  base::win::ScopedComPtr<ITfThreadMgr> thread_manager_;

  // A TextStore object to be used in communicating with IME for normal field.
  scoped_ptr<scoped_refptr<TSFTextStore> > text_store_;

  // A TextStore object to be used in communicating with IME for password field.
  scoped_ptr<scoped_refptr<TSFTextStore> > password_text_store_;

  DWORD source_cookie_;
  DWORD password_source_cookie_;
  TfClientId client_id_;

  // Current focused text input client. Do not free |client_|.
  TextInputClient* client_;

  DISALLOW_COPY_AND_ASSIGN(TSFBridgeDelegate);
};

TSFBridgeDelegate::TSFBridgeDelegate()
    : source_cookie_(TF_INVALID_COOKIE),
      password_source_cookie_(TF_INVALID_COOKIE),
      client_id_(TF_CLIENTID_NULL),
      client_(NULL) {
}

TSFBridgeDelegate::~TSFBridgeDelegate() {
}

bool TSFBridgeDelegate::Initialize() {
  DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());
  if (client_id_ != TF_CLIENTID_NULL) {
    DVLOG(1) << "Already initialized.";
    return false;
  }

  if (FAILED(thread_manager_.CreateInstance(CLSID_TF_ThreadMgr))) {
    DVLOG(1) << "Failed to create ThreadManager instance.";
    return false;
  }

  if (FAILED(thread_manager_->Activate(&client_id_))) {
    DVLOG(1) << "Failed to activate Thread Manager.";
    return false;
  }

  if (!InitializeForEnabledDocumentManager() ||
      !InitializeForPasswordDocumentManager() ||
      !InitializeForDisabledDocumentManager())
    return false;

  // Japanese IME expects the default value of this compartment is
  // TF_SENTENCEMODE_PHRASEPREDICT like IMM32 implementation. This value is
  // managed per thread, so that it is enough to set this value at once. This
  // value does not affect other language's IME behaviors.
  base::win::ScopedComPtr<ITfCompartmentMgr> thread_compartment_manager;
  if (FAILED(thread_compartment_manager.QueryFrom(thread_manager_))) {
    DVLOG(1) << "Failed to get ITfCompartmentMgr.";
    return false;
  }

  base::win::ScopedComPtr<ITfCompartment> sentence_compartment;
  if (FAILED(thread_compartment_manager->GetCompartment(
      GUID_COMPARTMENT_KEYBOARD_INPUTMODE_SENTENCE,
      sentence_compartment.Receive()))) {
    DVLOG(1) << "Failed to get sentence compartment.";
    return false;
  }

  base::win::ScopedVariant sentence_variant;
  sentence_variant.Set(TF_SENTENCEMODE_PHRASEPREDICT);
  if (FAILED(sentence_compartment->SetValue(client_id_, &sentence_variant))) {
    DVLOG(1) << "Failed to change the sentence mode.";
    return false;
  }

  return true;
}

void TSFBridgeDelegate::Shutdown() {
  DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());
  if (!IsInitialized())
    return;
  base::win::ScopedComPtr<ITfContext> context;
  if (FAILED(document_manager_for_editable_->GetBase(context.Receive())))
    return;
  base::win::ScopedComPtr<ITfSource> source;
  if (FAILED(source.QueryFrom(context)) ||
      source_cookie_ == TF_INVALID_COOKIE ||
      FAILED(source->UnadviseSink(source_cookie_)))
    return;

  base::win::ScopedComPtr<ITfContext> password_context;
  if (FAILED(document_manager_for_password_->GetBase(
      password_context.Receive())))
    return;
  base::win::ScopedComPtr<ITfSource> password_source;
  if (FAILED(password_source.QueryFrom(password_context)) ||
      password_source_cookie_ == TF_INVALID_COOKIE ||
      FAILED(password_source->UnadviseSink(password_source_cookie_)))
    return;

  DCHECK(text_store_.get());
  text_store_.reset();
  client_id_ = TF_CLIENTID_NULL;
}

void TSFBridgeDelegate::OnTextInputTypeChanged(TextInputClient* client) {
  DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());
  DCHECK(IsInitialized());

  if (client != client_) {
    // Called from not focusing client. Do nothing.
    return;
  }

  DCHECK(client_);
  const TextInputType type = client_->GetTextInputType();
  switch (type) {
    case TEXT_INPUT_TYPE_NONE:
      thread_manager_->SetFocus(document_manager_for_non_editable_);
      break;
    case TEXT_INPUT_TYPE_PASSWORD:
      thread_manager_->SetFocus(document_manager_for_password_);
      break;
    default:
      thread_manager_->SetFocus(document_manager_for_editable_);
      break;
  }
}

bool TSFBridgeDelegate::CancelComposition() {
  DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());
  DCHECK(IsInitialized());
  // If the current focused document manager is not
  // |document_manager_for_editable_|, do nothing here.
  if (!IsFocused(document_manager_for_editable_))
    return false;

  base::win::ScopedComPtr<ITfContext> context;
  // We should use ITfDocumentMgr::GetBase instead of ITfDocumentMgr::GetTop,
  // which may return a temporal context created by an IME for its modal UI
  // handling, to obtain a context against which on-going composition is
  // canceled. This is because ITfDocumentMgr::GetBase always returns the
  // context that is created by us and owns the on-going composition.
  // See http://crbug.com/169664 for details.
  if (FAILED(document_manager_for_editable_->GetBase(context.Receive()))) {
    DVLOG(1) << "Failed to get top context.";
    return false;
  }

  base::win::ScopedComPtr<ITfContextOwnerCompositionServices> owner;
  if (FAILED(owner.QueryFrom(context))) {
    DVLOG(1) << "Failed to get ITfContextOwnerCompositionService.";
    return false;
  }
  // Cancel all composition.
  owner->TerminateComposition(NULL);
  return true;
}

void TSFBridgeDelegate::SetFocusedClient(HWND focused_window,
                                         TextInputClient* client) {
  DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());
  DCHECK(client);
  DCHECK(IsInitialized());
  client_ = client;
  text_store_->get()->SetFocusedTextInputClient(focused_window, client);
  password_text_store_->get()->SetFocusedTextInputClient(focused_window,
                                                         client);

  // Synchronize text input type state.
  OnTextInputTypeChanged(client);
}

void TSFBridgeDelegate::RemoveFocusedClient(TextInputClient* client) {
  DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());
  DCHECK(IsInitialized());
  if (client_ == client) {
    client_ = NULL;
    text_store_->get()->SetFocusedTextInputClient(NULL, NULL);
    password_text_store_->get()->SetFocusedTextInputClient(NULL, NULL);
  }
}

base::win::ScopedComPtr<ITfThreadMgr> TSFBridgeDelegate::GetThreadManager() {
  DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());
  DCHECK(IsInitialized());
  return thread_manager_;
}

bool TSFBridgeDelegate::CreateDocumentManager(TSFTextStore* text_store,
                                              ITfDocumentMgr** document_manager,
                                              ITfContext** context,
                                              DWORD* source_cookie) {
  if (FAILED(thread_manager_->CreateDocumentMgr(document_manager))) {
    DVLOG(1) << "Failed to create Document Manager.";
    return false;
  }

  DWORD edit_cookie = TF_INVALID_EDIT_COOKIE;
  if (FAILED(document_manager_for_editable_->CreateContext(
      client_id_,
      0,
      static_cast<ITextStoreACP*>(text_store),
      context,
      &edit_cookie))) {
    DVLOG(1) << "Failed to create Context.";
    return false;
  }

  if (FAILED((*document_manager)->Push(*context))) {
    DVLOG(1) << "Failed to push context.";
    return false;
  }

  if (!text_store || !source_cookie)
    return true;

  base::win::ScopedComPtr<ITfSource> source;
  if (FAILED(source.QueryFrom(*context))) {
    DVLOG(1) << "Failed to get source.";
    return false;
  }

  if (FAILED(source->AdviseSink(IID_ITfTextEditSink,
                                static_cast<ITfTextEditSink*>(text_store),
                                source_cookie))) {
    DVLOG(1) << "AdviseSink failed.";
    return false;
  }

  if (*source_cookie == TF_INVALID_COOKIE) {
    DVLOG(1) << "The result of cookie is invalid.";
    return false;
  }
  return true;
}

bool TSFBridgeDelegate::InitializeForEnabledDocumentManager() {
  text_store_.reset(new scoped_refptr<TSFTextStore>(new TSFTextStore()));

  base::win::ScopedComPtr<ITfContext> context;
  return CreateDocumentManager(text_store_->get(),
                               document_manager_for_editable_.Receive(),
                               context.Receive(),
                               &source_cookie_);
}

bool TSFBridgeDelegate::InitializeForPasswordDocumentManager() {
  password_text_store_.reset(
      new scoped_refptr<TSFTextStore>(new TSFTextStore()));

  base::win::ScopedComPtr<ITfContext> context;
  if (!CreateDocumentManager(password_text_store_->get(),
                             document_manager_for_password_.Receive(),
                             context.Receive(),
                             &password_source_cookie_))
    return false;

  base::win::ScopedComPtr<ITfCompartmentMgr> compartment_mgr;
  if (FAILED(compartment_mgr.QueryFrom(context))) {
    DVLOG(1) << "Failed to get CompartmentMgr.";
    return false;
  }

  base::win::ScopedComPtr<ITfCompartment> disabled_compartment;
  if (FAILED(compartment_mgr->GetCompartment(
      GUID_COMPARTMENT_KEYBOARD_DISABLED,
      disabled_compartment.Receive()))) {
    DVLOG(1) << "Failed to get keyboard disabled compartment.";
    return false;
  }

  base::win::ScopedVariant variant;
  variant.Set(static_cast<int32>(1));
  if (FAILED(disabled_compartment->SetValue(client_id_, &variant))) {
    DVLOG(1) << "Failed to disable the DocumentMgr.";
    return false;
  }

  base::win::ScopedComPtr<ITfCompartment> empty_context;
  if (FAILED(compartment_mgr->GetCompartment(GUID_COMPARTMENT_EMPTYCONTEXT,
                                             empty_context.Receive()))) {
    DVLOG(1) << "Failed to get empty context compartment.";
    return false;
  }
  base::win::ScopedVariant empty_context_variant;
  empty_context_variant.Set(1);
  if (FAILED(empty_context->SetValue(client_id_, &empty_context_variant))) {
    DVLOG(1) << "Failed to set empty context.";
    return false;
  }

  return true;
}

bool TSFBridgeDelegate::InitializeForDisabledDocumentManager() {
  base::win::ScopedComPtr<ITfContext> context;
  if (!CreateDocumentManager(NULL,
                             document_manager_for_non_editable_.Receive(),
                             context.Receive(),
                             NULL))
    return false;

  base::win::ScopedComPtr<ITfCompartmentMgr> compartment_mgr;
  if (FAILED(compartment_mgr.QueryFrom(context))) {
    DVLOG(1) << "Failed to get CompartmentMgr.";
    return false;
  }

  base::win::ScopedComPtr<ITfCompartment> disabled_compartment;
  if (FAILED(compartment_mgr->GetCompartment(
      GUID_COMPARTMENT_KEYBOARD_DISABLED,
      disabled_compartment.Receive()))) {
    DVLOG(1) << "Failed to get keyboard disabled compartment.";
    return false;
  }

  base::win::ScopedVariant variant;
  variant.Set(1);
  if (FAILED(disabled_compartment->SetValue(client_id_, &variant))) {
    DVLOG(1) << "Failed to disable the DocumentMgr.";
    return false;
  }

  base::win::ScopedComPtr<ITfCompartment> empty_context;
  if (FAILED(compartment_mgr->GetCompartment(GUID_COMPARTMENT_EMPTYCONTEXT,
                                             empty_context.Receive()))) {
    DVLOG(1) << "Failed to get empty context compartment.";
    return false;
  }
  base::win::ScopedVariant empty_context_variant;
  empty_context_variant.Set(static_cast<int32>(1));
  if (FAILED(empty_context->SetValue(client_id_, &empty_context_variant))) {
    DVLOG(1) << "Failed to set empty context.";
    return false;
  }

  return true;
}

bool TSFBridgeDelegate::IsFocused(
    base::win::ScopedComPtr<ITfDocumentMgr> document_manager) {
  base::win::ScopedComPtr<ITfDocumentMgr> focused_document_manager;
  if (FAILED(thread_manager_->GetFocus(focused_document_manager.Receive())))
    return false;
  return focused_document_manager.IsSameObject(document_manager);
}

bool TSFBridgeDelegate::IsInitialized() {
  return client_id_ != TF_CLIENTID_NULL;
}

}  // namespace


// TsfBridge  -----------------------------------------------------------------

TSFBridge::TSFBridge() {
}

TSFBridge::~TSFBridge() {
}

// static
bool TSFBridge::Initialize() {
  if (MessageLoop::current()->type() != MessageLoop::TYPE_UI) {
    DVLOG(1) << "Do not use TSFBridge without UI thread.";
    return false;
  }
  tls_tsf_bridge.Initialize(TSFBridge::Finalize);
  TSFBridgeDelegate* delegate = new TSFBridgeDelegate();
  tls_tsf_bridge.Set(delegate);
  return delegate->Initialize();
}

// static
TSFBridge* TSFBridge::ReplaceForTesting(TSFBridge* bridge) {
  if (MessageLoop::current()->type() != MessageLoop::TYPE_UI) {
    DVLOG(1) << "Do not use TSFBridge without UI thread.";
    return NULL;
  }
  TSFBridge* old_bridge = TSFBridge::GetInstance();
  tls_tsf_bridge.Set(bridge);
  return old_bridge;
}

// static
TSFBridge* TSFBridge::GetInstance() {
  if (MessageLoop::current()->type() != MessageLoop::TYPE_UI) {
    DVLOG(1) << "Do not use TSFBridge without UI thread.";
    return NULL;
  }
  TSFBridgeDelegate* delegate =
      static_cast<TSFBridgeDelegate*>(tls_tsf_bridge.Get());
  DCHECK(delegate) << "Do no call GetInstance before TSFBridge::Initialize.";
  return delegate;
}

// static
void TSFBridge::Finalize(void* data) {
  TSFBridgeDelegate* delegate = static_cast<TSFBridgeDelegate*>(data);
  delete delegate;
}

}  // namespace ui
