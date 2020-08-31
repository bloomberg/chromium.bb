// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DOM_WINDOW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DOM_WINDOW_H_

#include "third_party/blink/renderer/bindings/core/v8/serialization/transferables.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class Document;
class InputDeviceCapabilitiesConstants;
class LocalDOMWindow;
class Location;
class MessageEvent;
class ScriptValue;
class SecurityOrigin;
class SerializedScriptValue;
class WindowPostMessageOptions;
class WindowProxyManager;

// DOMWindow is an abstract class of Window interface implementations.
// We have two derived implementation classes;  LocalDOMWindow and
// RemoteDOMWindow.
//
// TODO(tkent): Rename DOMWindow to Window. The class was named as 'DOMWindow'
// because WebKit already had KJS::Window.  We have no reasons to avoid
// blink::Window now.
class CORE_EXPORT DOMWindow : public EventTargetWithInlineData {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum class CrossDocumentAccessPolicy { kAllowed, kDisallowed };

  ~DOMWindow() override;

  Frame* GetFrame() const {
    // A Frame is typically reused for navigations. If |frame_| is not null,
    // two conditions must always be true:
    // - |frame_->domWindow()| must point back to this DOMWindow. If it does
    //   not, it is easy to introduce a bug where script execution uses the
    //   wrong DOMWindow (which may be cross-origin).
    // - |frame_| must be attached, i.e. |frame_->page()| must not be null.
    //   If |frame_->page()| is null, this indicates a bug where the frame was
    //   detached but |frame_| was not set to null. This bug can lead to
    //   issues where executing script incorrectly schedules work on a detached
    //   frame.
    SECURITY_DCHECK(!frame_ ||
                    (frame_->DomWindow() == this && frame_->GetPage()));
    return frame_;
  }

  // GarbageCollected overrides:
  void Trace(Visitor*) override;

  virtual bool IsLocalDOMWindow() const = 0;
  virtual bool IsRemoteDOMWindow() const = 0;

  // ScriptWrappable overrides:
  v8::Local<v8::Value> Wrap(v8::Isolate*,
                            v8::Local<v8::Object> creation_context) final;
  v8::Local<v8::Object> AssociateWithWrapper(
      v8::Isolate*,
      const WrapperTypeInfo*,
      v8::Local<v8::Object> wrapper) final;

  // EventTarget overrides:
  const AtomicString& InterfaceName() const override;
  const DOMWindow* ToDOMWindow() const override;
  bool IsWindowOrWorkerGlobalScope() const final;

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

  void focus(v8::Isolate*);
  virtual void blur() = 0;
  void close(v8::Isolate*);
  void Close(LocalDOMWindow* incumbent_window);

  void postMessage(v8::Isolate*,
                   const ScriptValue& message,
                   const String& target_origin,
                   HeapVector<ScriptValue>& transfer,
                   ExceptionState&);

  void postMessage(v8::Isolate*,
                   const ScriptValue& message,
                   const WindowPostMessageOptions* options,
                   ExceptionState&);

  // Indexed properties
  DOMWindow* AnonymousIndexedGetter(uint32_t index) const;

  String SanitizedCrossDomainAccessErrorMessage(
      const LocalDOMWindow* accessing_window,
      CrossDocumentAccessPolicy cross_document_access) const;
  String CrossDomainAccessErrorMessage(
      const LocalDOMWindow* accessing_window,
      CrossDocumentAccessPolicy cross_document_access) const;

  // FIXME: When this DOMWindow is no longer the active DOMWindow (i.e.,
  // when its document is no longer the document that is displayed in its
  // frame), we would like to zero out |frame_| to avoid being confused
  // by the document that is currently active in |frame_|.
  // See https://bugs.webkit.org/show_bug.cgi?id=62054
  bool IsCurrentlyDisplayedInFrame() const;

  InputDeviceCapabilitiesConstants* GetInputDeviceCapabilities();

  void PostMessageForTesting(scoped_refptr<SerializedScriptValue> message,
                             const MessagePortArray&,
                             const String& target_origin,
                             LocalDOMWindow* source,
                             ExceptionState&);

 protected:
  explicit DOMWindow(Frame&);

  virtual void SchedulePostMessage(MessageEvent*,
                                   scoped_refptr<const SecurityOrigin> target,
                                   Document* source) = 0;

  void DisconnectFromFrame() { frame_ = nullptr; }

 private:
  void DoPostMessage(scoped_refptr<SerializedScriptValue> message,
                     const MessagePortArray&,
                     const WindowPostMessageOptions* options,
                     LocalDOMWindow* source,
                     ExceptionState&);

  Member<Frame> frame_;
  // Unlike |frame_|, |window_proxy_manager_| is available even after the
  // window's frame gets detached from the DOM, until the end of the lifetime
  // of this object.
  const Member<WindowProxyManager> window_proxy_manager_;
  Member<InputDeviceCapabilitiesConstants> input_capabilities_;
  mutable Member<Location> location_;

  // Set to true when close() has been called. Needed for
  // |window.closed| determinism; having it return 'true'
  // only after the layout widget's deferred window close
  // operation has been performed, exposes (confusing)
  // implementation details to scripts.
  bool window_is_closing_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_DOM_WINDOW_H_
