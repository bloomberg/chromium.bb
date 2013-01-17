// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win8/test/ui_automation_client.h"

#include <atlbase.h>
#include <atlcom.h>
#include <oleauto.h>
#include <uiautomation.h>

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_variant.h"

namespace win8 {
namespace internal {

// The guts of the UI automation client which runs on a dedicated thread in the
// multi-threaded COM apartment. An instance may be constructed on any thread,
// but Initialize() must be invoked on a thread in the MTA.
class UIAutomationClient::Context {
 public:
  // Returns a new instance ready for initialization and use on another thread.
  static base::WeakPtr<Context> Create();

  // Deletes the instance.
  void DeleteOnAutomationThread();

  // Initializes the context, invoking |init_callback| via |client_runner| when
  // done. On success, |result_callback| will eventually be called after the
  // window has been processed. On failure, this instance self-destructs after
  // posting |init_callback|.
  void Initialize(
      scoped_refptr<base::SingleThreadTaskRunner> client_runner,
      string16 class_name,
      string16 item_name,
      UIAutomationClient::InitializedCallback init_callback,
      UIAutomationClient::ResultCallback result_callback);

  // Methods invoked by event handlers via weak pointers.
  void HandleAutomationEvent(
      base::win::ScopedComPtr<IUIAutomationElement> sender,
      EVENTID eventId);

 private:
  class EventHandler;

  // The only and only method that may be called from outside of the automation
  // thread.
  Context();
  ~Context();

  HRESULT InstallWindowObserver();
  HRESULT RemoveWindowObserver();

  void HandleWindowOpen(
      const base::win::ScopedComPtr<IUIAutomationElement>& window);
  void ProcessWindow(
      const base::win::ScopedComPtr<IUIAutomationElement>& window);
  HRESULT InvokeDesiredItem(
      const base::win::ScopedComPtr<IUIAutomationElement>& element);
  HRESULT GetInvokableItems(
      const base::win::ScopedComPtr<IUIAutomationElement>& element,
      std::vector<string16>* choices);
  void CloseWindow(const base::win::ScopedComPtr<IUIAutomationElement>& window);

  base::ThreadChecker thread_checker_;

  // The loop on which the client itself lives.
  scoped_refptr<base::SingleThreadTaskRunner> client_runner_;

  // The class name of the window for which the client waits.
  string16 class_name_;

  // The name of the item to invoke.
  string16 item_name_;

  // The consumer's result callback.
  ResultCallback result_callback_;

  // The automation client.
  base::win::ScopedComPtr<IUIAutomation> automation_;

  // A handler of Window open events.
  base::win::ScopedComPtr<IUIAutomationEventHandler> event_handler_;

  // Weak pointers to the context are given to event handlers.
  base::WeakPtrFactory<UIAutomationClient::Context> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

class UIAutomationClient::Context::EventHandler
    : public CComObjectRootEx<CComMultiThreadModel>,
      public IUIAutomationEventHandler {
 public:
  BEGIN_COM_MAP(UIAutomationClient::Context::EventHandler)
    COM_INTERFACE_ENTRY(IUIAutomationEventHandler)
  END_COM_MAP()

  EventHandler();
  virtual ~EventHandler();

  // Initializes the object with its parent UI automation client context's
  // message loop and pointer. Events are dispatched back to the context on
  // the given loop.
  void Initialize(
      const scoped_refptr<base::SingleThreadTaskRunner>& context_runner,
      const base::WeakPtr<UIAutomationClient::Context>& context);

  // IUIAutomationEventHandler methods.
  STDMETHOD(HandleAutomationEvent)(IUIAutomationElement* sender,
                                   EVENTID eventId);

 private:
  // The task runner for the UI automation client context.
  scoped_refptr<base::SingleThreadTaskRunner> context_runner_;

  // The parent UI automation client context.
  base::WeakPtr<UIAutomationClient::Context> context_;

  DISALLOW_COPY_AND_ASSIGN(EventHandler);
};

UIAutomationClient::Context::EventHandler::EventHandler() {}

UIAutomationClient::Context::EventHandler::~EventHandler() {}

void UIAutomationClient::Context::EventHandler::Initialize(
    const scoped_refptr<base::SingleThreadTaskRunner>& context_runner,
    const base::WeakPtr<UIAutomationClient::Context>& context) {
  context_runner_ = context_runner;
  context_ = context;
}

HRESULT UIAutomationClient::Context::EventHandler::HandleAutomationEvent(
    IUIAutomationElement* sender,
    EVENTID eventId) {
  // Event handlers are invoked on an arbitrary thread in the MTA. Send the
  // event back to the main UI automation thread for processing.
  context_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UIAutomationClient::Context::HandleAutomationEvent, context_,
                 base::win::ScopedComPtr<IUIAutomationElement>(sender),
                 eventId));

  return S_OK;
}

base::WeakPtr<UIAutomationClient::Context>
    UIAutomationClient::Context::Create() {
  Context* context = new Context();
  base::WeakPtr<Context> context_ptr(context->weak_ptr_factory_.GetWeakPtr());
  // Unbind from this thread so that the instance will bind to the automation
  // thread when Initialize is called.
  context->weak_ptr_factory_.DetachFromThread();
  return context_ptr;
}

void UIAutomationClient::Context::DeleteOnAutomationThread() {
  DCHECK(thread_checker_.CalledOnValidThread());
  delete this;
}

UIAutomationClient::Context::Context()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {}

UIAutomationClient::Context::~Context() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (event_handler_.get()) {
    event_handler_ = NULL;
    HRESULT result = automation_->RemoveAllEventHandlers();
    LOG_IF(ERROR, FAILED(result)) << std::hex << result;
  }
}

void UIAutomationClient::Context::Initialize(
    scoped_refptr<base::SingleThreadTaskRunner> client_runner,
    string16 class_name,
    string16 item_name,
    UIAutomationClient::InitializedCallback init_callback,
    UIAutomationClient::ResultCallback result_callback) {
  // This and all other methods must be called on the automation thread.
  DCHECK(!client_runner->BelongsToCurrentThread());
  // Bind the checker to this thread.
  thread_checker_.DetachFromThread();
  DCHECK(thread_checker_.CalledOnValidThread());

  client_runner_ = client_runner;
  class_name_ = class_name;
  item_name_ = item_name;
  result_callback_ = result_callback;

  HRESULT result = automation_.CreateInstance(CLSID_CUIAutomation, NULL,
                                              CLSCTX_INPROC_SERVER);
  if (FAILED(result) || !automation_.get())
    LOG(ERROR) << std::hex << result;
  else
    result = InstallWindowObserver();

  // Tell the client that initialization is complete.
  client_runner_->PostTask(FROM_HERE, base::Bind(init_callback, result));

  // Self-destruct if the overall operation failed.
  if (FAILED(result))
    delete this;
}

// Installs the window observer.
HRESULT UIAutomationClient::Context::InstallWindowObserver() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(automation_.get());
  DCHECK(!event_handler_.get());

  HRESULT result = S_OK;
  base::win::ScopedComPtr<IUIAutomationElement> root_element;
  base::win::ScopedComPtr<IUIAutomationCacheRequest> cache_request;

  // Observe the opening of all windows.
  result = automation_->GetRootElement(root_element.Receive());
  if (FAILED(result)) {
    LOG(ERROR) << std::hex << result;
    return result;
  }

  // Cache Window class, HWND, and window pattern for opened windows.
  result = automation_->CreateCacheRequest(cache_request.Receive());
  if (FAILED(result)) {
    LOG(ERROR) << std::hex << result;
    return result;
  }
  cache_request->AddProperty(UIA_ClassNamePropertyId);
  cache_request->AddProperty(UIA_NativeWindowHandlePropertyId);

  // Create the observer.
  CComObject<EventHandler>* event_handler_obj = NULL;
  result = CComObject<EventHandler>::CreateInstance(&event_handler_obj);
  if (FAILED(result)) {
    LOG(ERROR) << std::hex << result;
    return result;
  }
  event_handler_obj->Initialize(base::ThreadTaskRunnerHandle::Get(),
                                weak_ptr_factory_.GetWeakPtr());
  base::win::ScopedComPtr<IUIAutomationEventHandler> event_handler(
      event_handler_obj);

  result = automation_->AddAutomationEventHandler(
      UIA_Window_WindowOpenedEventId,
      root_element,
      TreeScope_Descendants,
      cache_request,
      event_handler);

  if (FAILED(result)) {
    LOG(ERROR) << std::hex << result;
    return result;
  }

  event_handler_ = event_handler;
  return S_OK;
}

// Removes this instance's window observer.
HRESULT UIAutomationClient::Context::RemoveWindowObserver() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(automation_.get());
  DCHECK(event_handler_.get());

  HRESULT result = S_OK;
  base::win::ScopedComPtr<IUIAutomationElement> root_element;

  // The opening of all windows are observed.
  result = automation_->GetRootElement(root_element.Receive());
  if (FAILED(result)) {
    LOG(ERROR) << std::hex << result;
    return result;
  }

  result = automation_->RemoveAutomationEventHandler(
      UIA_Window_WindowOpenedEventId,
      root_element,
      event_handler_);
  if (FAILED(result)) {
    LOG(ERROR) << std::hex << result;
    return result;
  }

  event_handler_ = NULL;
  return S_OK;
}

// Handles an automation event. If the event results in the processing for which
// this context was created, the context self-destructs after posting the
// results to the client.
void UIAutomationClient::Context::HandleAutomationEvent(
    base::win::ScopedComPtr<IUIAutomationElement> sender,
    EVENTID eventId) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (eventId == UIA_Window_WindowOpenedEventId)
    HandleWindowOpen(sender);
}

// Handles a WindowOpen event. If |window| is the one for which this instance is
// waiting, it is processed and this instance self-destructs after posting the
// results to the client.
void UIAutomationClient::Context::HandleWindowOpen(
    const base::win::ScopedComPtr<IUIAutomationElement>& window) {
  DCHECK(thread_checker_.CalledOnValidThread());
  HRESULT hr = S_OK;
  base::win::ScopedVariant var;

  hr = window->GetCachedPropertyValueEx(UIA_ClassNamePropertyId, TRUE,
                                        var.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << std::hex << hr;
    return;
  }

  if (V_VT(&var) != VT_BSTR) {
    LOG(ERROR) << __FUNCTION__ " class name is not a BSTR: " << V_VT(&var);
    return;
  }

  string16 class_name(V_BSTR(&var));

  // Window class names are atoms, which are case-insensitive.
  if (class_name.size() == class_name_.size() &&
      std::equal(class_name.begin(), class_name.end(), class_name_.begin(),
                 base::CaseInsensitiveCompare<wchar_t>())) {
    RemoveWindowObserver();
    ProcessWindow(window);
  }
}

// Processes |window| by invoking the desired child item. If the item cannot be
// found or invoked, an attempt is made to get a list of all invokable children.
// The results are posted back to the client on |client_runner_|, and this
// instance self-destructs.
void UIAutomationClient::Context::ProcessWindow(
    const base::win::ScopedComPtr<IUIAutomationElement>& window) {
  DCHECK(thread_checker_.CalledOnValidThread());

  HRESULT result = S_OK;
  std::vector<string16> choices;
  result = InvokeDesiredItem(window);
  if (FAILED(result)) {
    GetInvokableItems(window, &choices);
    CloseWindow(window);
  }

  client_runner_->PostTask(FROM_HERE,
                           base::Bind(result_callback_, result, choices));

  // Self-destruct since there's nothing more to be done here.
  delete this;
}

// Invokes the desired child of |element|.
HRESULT UIAutomationClient::Context::InvokeDesiredItem(
    const base::win::ScopedComPtr<IUIAutomationElement>& element) {
  DCHECK(thread_checker_.CalledOnValidThread());

  HRESULT result = S_OK;
  base::win::ScopedVariant var;
  base::win::ScopedComPtr<IUIAutomationCondition> invokable_condition;
  base::win::ScopedComPtr<IUIAutomationCondition> item_name_condition;
  base::win::ScopedComPtr<IUIAutomationCondition> control_view_condition;
  base::win::ScopedComPtr<IUIAutomationCondition> condition;
  base::win::ScopedComPtr<IUIAutomationCacheRequest> cache_request;
  base::win::ScopedComPtr<IUIAutomationElement> target;

  // Search for an invokable element named item_name.
  var.Set(true);
  result = automation_->CreatePropertyCondition(
      UIA_IsInvokePatternAvailablePropertyId,
      var,
      invokable_condition.Receive());
  var.Reset();
  if (FAILED(result)) {
    LOG(ERROR) << std::hex << result;
    return false;
  }

  var.Set(item_name_.c_str());
  result = automation_->CreatePropertyCondition(UIA_NamePropertyId,
                                                var,
                                                item_name_condition.Receive());
  var.Reset();
  if (FAILED(result)) {
    LOG(ERROR) << std::hex << result;
    return result;
  }

  result = automation_->get_ControlViewCondition(
      control_view_condition.Receive());
  if (FAILED(result)) {
    LOG(ERROR) << std::hex << result;
    return result;
  }

  std::vector<IUIAutomationCondition*> conditions;
  conditions.push_back(invokable_condition.get());
  conditions.push_back(item_name_condition.get());
  conditions.push_back(control_view_condition.get());
  result = automation_->CreateAndConditionFromNativeArray(
      &conditions[0], conditions.size(), condition.Receive());
  if (FAILED(result)) {
    LOG(ERROR) << std::hex << result;
    return result;
  }

  // Cache invokable pattern for the item.
  result = automation_->CreateCacheRequest(cache_request.Receive());
  if (FAILED(result)) {
    LOG(ERROR) << std::hex << result;
    return result;
  }
  cache_request->AddPattern(UIA_InvokePatternId);

  result = element->FindFirstBuildCache(
      static_cast<TreeScope>(TreeScope_Children | TreeScope_Descendants),
      condition,
      cache_request,
      target.Receive());
  if (FAILED(result)) {
    LOG(ERROR) << std::hex << result;
    return result;
  }

  // If the item was found, invoke it.
  if (!target.get()) {
    LOG(ERROR) << "Failed to find desired item to invoke.";
    return E_FAIL;
  }

  base::win::ScopedComPtr<IUIAutomationInvokePattern> invoker;
  result = target->GetCachedPatternAs(UIA_InvokePatternId, invoker.iid(),
                                      invoker.ReceiveVoid());
  if (FAILED(result)) {
    LOG(ERROR) << std::hex << result;
    return result;
  }

  result = invoker->Invoke();
  if (FAILED(result)) {
    LOG(ERROR) << std::hex << result;
    return result;
  }

  return S_OK;
}

// Populates |choices| with the names of all invokable children of |element|.
HRESULT UIAutomationClient::Context::GetInvokableItems(
    const base::win::ScopedComPtr<IUIAutomationElement>& element,
    std::vector<string16>* choices) {
  DCHECK(choices);
  DCHECK(thread_checker_.CalledOnValidThread());

  HRESULT result = S_OK;
  base::win::ScopedVariant var;
  base::win::ScopedComPtr<IUIAutomationCondition> invokable_condition;
  base::win::ScopedComPtr<IUIAutomationCondition> control_view_condition;
  base::win::ScopedComPtr<IUIAutomationCondition> condition;
  base::win::ScopedComPtr<IUIAutomationCacheRequest> cache_request;
  base::win::ScopedComPtr<IUIAutomationElementArray> element_array;
  base::win::ScopedComPtr<IUIAutomationElement> child_element;

  // Search for all invokable elements.
  var.Set(true);
  result = automation_->CreatePropertyCondition(
      UIA_IsInvokePatternAvailablePropertyId,
      var,
      invokable_condition.Receive());
  var.Reset();
  if (FAILED(result)) {
    LOG(ERROR) << std::hex << result;
    return result;
  }

  result = automation_->get_ControlViewCondition(
      control_view_condition.Receive());
  if (FAILED(result)) {
    LOG(ERROR) << std::hex << result;
    return result;
  }

  result = automation_->CreateAndCondition(
      invokable_condition, control_view_condition, condition.Receive());
  if (FAILED(result)) {
    LOG(ERROR) << std::hex << result;
    return result;
  }

  // Cache item names.
  result = automation_->CreateCacheRequest(cache_request.Receive());
  if (FAILED(result)) {
    LOG(ERROR) << std::hex << result;
    return result;
  }
  cache_request->AddProperty(UIA_NamePropertyId);

  result = element->FindAllBuildCache(
      static_cast<TreeScope>(TreeScope_Children | TreeScope_Descendants),
      condition,
      cache_request,
      element_array.Receive());
  if (FAILED(result)) {
    LOG(ERROR) << std::hex << result;
    return result;
  }

  if (!element_array.get()) {
    LOG(ERROR) << "The window may have vanished.";
    return S_OK;
  }

  int num_elements = 0;
  result = element_array->get_Length(&num_elements);
  if (FAILED(result)) {
    LOG(ERROR) << std::hex << result;
    return result;
  }

  choices->clear();
  choices->reserve(num_elements);
  for (int i = 0; i < num_elements; ++i) {
    child_element.Release();
    result = element_array->GetElement(i, child_element.Receive());
    if (FAILED(result)) {
      LOG(ERROR) << std::hex << result;
      continue;
    }
    result = child_element->GetCachedPropertyValueEx(UIA_NamePropertyId, TRUE,
                                                     var.Receive());
    if (FAILED(result)) {
      LOG(ERROR) << std::hex << result;
      continue;
    }
    if (V_VT(&var) != VT_BSTR) {
      LOG(ERROR) << __FUNCTION__ " name is not a BSTR: " << V_VT(&var);
      continue;
    }
    choices->push_back(string16(V_BSTR(&var)));
    var.Reset();
  }

  return result;
}

// Closes the element |window| by sending it an escape key.
void UIAutomationClient::Context::CloseWindow(
    const base::win::ScopedComPtr<IUIAutomationElement>& window) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // It's tempting to get the Window pattern from |window| and invoke its Close
  // method. Unfortunately, this doesn't work. Sending an escape key does the
  // trick, though.
  HRESULT result = S_OK;
  base::win::ScopedVariant var;

  result = window->GetCachedPropertyValueEx(
      UIA_NativeWindowHandlePropertyId,
      TRUE,
      var.Receive());
  if (FAILED(result)) {
    LOG(ERROR) << std::hex << result;
    return;
  }

  if (V_VT(&var) != VT_I4) {
    LOG(ERROR) << __FUNCTION__ " window handle is not an int: " << V_VT(&var);
    return;
  }

  HWND handle = reinterpret_cast<HWND>(V_I4(&var));

  uint32 scan_code = MapVirtualKey(VK_ESCAPE, MAPVK_VK_TO_VSC);
  PostMessage(handle, WM_KEYDOWN, VK_ESCAPE,
              MAKELPARAM(1, scan_code));
  PostMessage(handle, WM_KEYUP, VK_ESCAPE,
              MAKELPARAM(1, scan_code | KF_REPEAT | KF_UP));
}

UIAutomationClient::UIAutomationClient()
    : automation_thread_("UIAutomation") {}

UIAutomationClient::~UIAutomationClient() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // context_ is still valid when the caller destroys the instance before the
  // callback(s) have fired. In this case, delete the context on the automation
  // thread before joining with it.
  automation_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&UIAutomationClient::Context::DeleteOnAutomationThread,
                 context_));
}

void UIAutomationClient::Begin(const wchar_t* class_name,
                               const string16& item_name,
                               const InitializedCallback& init_callback,
                               const ResultCallback& result_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(context_, static_cast<Context*>(NULL));

  // Start the automation thread and initialize our automation client on it.
  context_ = Context::Create();
  automation_thread_.init_com_with_mta(true);
  automation_thread_.Start();
  automation_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&UIAutomationClient::Context::Initialize,
                 context_,
                 base::ThreadTaskRunnerHandle::Get(),
                 string16(class_name),
                 item_name,
                 init_callback,
                 result_callback));
}

}  // namespace internal
}  // namespace win8
