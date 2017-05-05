// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMWindow_h
#define DOMWindow_h

#include "bindings/core/v8/serialization/Transferables.h"
#include "core/CoreExport.h"
#include "core/events/EventTarget.h"
#include "core/frame/DOMWindowBase64.h"
#include "core/frame/Frame.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/Forward.h"

namespace blink {

class Document;
class InputDeviceCapabilitiesConstants;
class LocalDOMWindow;
class Location;
class MessageEvent;
class ScriptValue;
class SerializedScriptValue;
class WindowProxyManager;

class CORE_EXPORT DOMWindow : public EventTargetWithInlineData,
                              public DOMWindowBase64 {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~DOMWindow() override;

  Frame* GetFrame() const {
    // A Frame is typically reused for navigations. If |m_frame| is not null,
    // two conditions must always be true:
    // - |m_frame->domWindow()| must point back to this DOMWindow. If it does
    //   not, it is easy to introduce a bug where script execution uses the
    //   wrong DOMWindow (which may be cross-origin).
    // - |m_frame| must be attached, i.e. |m_frame->page()| must not be null.
    //   If |m_frame->page()| is null, this indicates a bug where the frame was
    //   detached but |m_frame| was not set to null. This bug can lead to
    //   issues where executing script incorrectly schedules work on a detached
    //   frame.
    SECURITY_DCHECK(!frame_ ||
                    (frame_->DomWindow() == this && frame_->GetPage()));
    return frame_;
  }

  // GarbageCollectedFinalized overrides:
  DECLARE_VIRTUAL_TRACE();

  DECLARE_VIRTUAL_TRACE_WRAPPERS();

  virtual bool IsLocalDOMWindow() const = 0;
  virtual bool IsRemoteDOMWindow() const = 0;

  // ScriptWrappable overrides:
  v8::Local<v8::Object> Wrap(v8::Isolate*,
                             v8::Local<v8::Object> creation_context) final;
  v8::Local<v8::Object> AssociateWithWrapper(
      v8::Isolate*,
      const WrapperTypeInfo*,
      v8::Local<v8::Object> wrapper) final;

  // EventTarget overrides:
  const AtomicString& InterfaceName() const override;
  const DOMWindow* ToDOMWindow() const override;

  // Cross-origin DOM Level 0
  Location* location() const;
  bool closed() const;
  unsigned length() const;

  // Self-referential attributes
  DOMWindow* self() const;
  DOMWindow* window() const { return self(); }
  DOMWindow* frames() const { return self(); }

  DOMWindow* opener() const;
  DOMWindow* parent() const;
  DOMWindow* top() const;

  void focus(ExecutionContext*);
  virtual void blur() = 0;
  void close(ExecutionContext*);

  // Indexed properties
  DOMWindow* AnonymousIndexedGetter(uint32_t index) const;
  bool AnonymousIndexedSetter(uint32_t index, const ScriptValue&);

  void postMessage(PassRefPtr<SerializedScriptValue> message,
                   const MessagePortArray&,
                   const String& target_origin,
                   LocalDOMWindow* source,
                   ExceptionState&);

  String SanitizedCrossDomainAccessErrorMessage(
      const LocalDOMWindow* calling_window) const;
  String CrossDomainAccessErrorMessage(
      const LocalDOMWindow* calling_window) const;
  bool IsInsecureScriptAccess(LocalDOMWindow& calling_window, const KURL&);

  // FIXME: When this DOMWindow is no longer the active DOMWindow (i.e.,
  // when its document is no longer the document that is displayed in its
  // frame), we would like to zero out m_frame to avoid being confused
  // by the document that is currently active in m_frame.
  // See https://bugs.webkit.org/show_bug.cgi?id=62054
  bool IsCurrentlyDisplayedInFrame() const;

  bool isSecureContext() const;

  InputDeviceCapabilitiesConstants* GetInputDeviceCapabilities();

 protected:
  explicit DOMWindow(Frame&);

  virtual void SchedulePostMessage(MessageEvent*,
                                   PassRefPtr<SecurityOrigin> target,
                                   Document* source) = 0;

  void DisconnectFromFrame() { frame_ = nullptr; }

 private:
  Member<Frame> frame_;
  // Unlike |frame_|, |window_proxy_manager_| is available even after the
  // window's frame gets detached from the DOM, until the end of the lifetime
  // of this object.
  const Member<WindowProxyManager> window_proxy_manager_;
  Member<InputDeviceCapabilitiesConstants> input_capabilities_;
  mutable TraceWrapperMember<Location> location_;

  // Set to true when close() has been called. Needed for
  // |window.closed| determinism; having it return 'true'
  // only after the layout widget's deferred window close
  // operation has been performed, exposes (confusing)
  // implementation details to scripts.
  bool window_is_closing_;
};

}  // namespace blink

#endif  // DOMWindow_h
