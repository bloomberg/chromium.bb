// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8PrivateProperty_h
#define V8PrivateProperty_h

#include <memory>

#include "bindings/core/v8/ScriptPromiseProperties.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "core/CoreExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/PtrUtil.h"
#include "v8/include/v8.h"

namespace blink {

class ScriptWrappable;

// TODO(peria): Remove properties just to keep V8 objects alive.
// e.g. InternalBody.Buffer, InternalBody.Stream, IDBCursor.Request,
// FetchEvent.Request.
// Apply |X| for each pair of (InterfaceName, PrivateKeyName).
#define V8_PRIVATE_PROPERTY_FOR_EACH(X)               \
  X(CustomElement, AdoptedCallback)                   \
  X(CustomElement, AttributeChangedCallback)          \
  X(CustomElement, ConnectedCallback)                 \
  X(CustomElement, DisconnectedCallback)              \
  X(CustomElement, Document)                          \
  X(CustomElement, IsInterfacePrototypeObject)        \
  X(CustomElement, NamespaceURI)                      \
  X(CustomElement, RegistryMap)                       \
  X(CustomElement, TagName)                           \
  X(CustomElement, Type)                              \
  X(CustomElementLifecycle, AttachedCallback)         \
  X(CustomElementLifecycle, AttributeChangedCallback) \
  X(CustomElementLifecycle, CreatedCallback)          \
  X(CustomElementLifecycle, DetachedCallback)         \
  X(CustomEvent, Detail)                              \
  X(DOMException, Error)                              \
  X(ErrorEvent, Error)                                \
  X(FetchEvent, Request)                              \
  X(Global, Event)                                    \
  X(IDBCursor, Request)                               \
  X(IDBObserver, Callback)                            \
  X(InternalBody, Buffer)                             \
  X(InternalBody, Stream)                             \
  X(IntersectionObserver, Callback)                   \
  X(LazyEventListener, ToString)                      \
  X(MessageChannel, Port1)                            \
  X(MessageChannel, Port2)                            \
  X(MessageEvent, CachedData)                         \
  X(MutationObserver, Callback)                       \
  X(NamedConstructor, Initialized)                    \
  X(PerformanceObserver, Callback)                    \
  X(PopStateEvent, State)                             \
  X(SameObject, NotificationActions)                  \
  X(SameObject, NotificationData)                     \
  X(SameObject, NotificationVibrate)                  \
  X(SameObject, PerformanceLongTaskTimingAttribution) \
  X(V8NodeFilterCondition, Filter)                    \
  SCRIPT_PROMISE_PROPERTIES(X, Promise)               \
  SCRIPT_PROMISE_PROPERTIES(X, Resolver)

// The getter's name for a private property.
#define V8_PRIVATE_PROPERTY_GETTER_NAME(InterfaceName, PrivateKeyName) \
  get##InterfaceName##PrivateKeyName

// The member variable's name for a private property.
#define V8_PRIVATE_PROPERTY_MEMBER_NAME(InterfaceName, PrivateKeyName) \
  m_symbol##InterfaceName##PrivateKeyName

// The string used to create a private symbol.  Must be unique per V8 instance.
#define V8_PRIVATE_PROPERTY_SYMBOL_STRING(InterfaceName, PrivateKeyName) \
  #InterfaceName "#" #PrivateKeyName  // NOLINT(whitespace/indent)

// Provides access to V8's private properties.
//
// Usage 1) Fast path to use a pre-registered symbol.
//   auto private = V8PrivateProperty::getMessageEventCachedData(isolate);
//   v8::Local<v8::Object> object = ...;
//   v8::Local<v8::Value> value = private.getOrUndefined(object);
//   value = ...;
//   private.set(object, value);
//
// Usage 2) Slow path to create a global private symbol.
//   const char symbolName[] = "Interface#PrivateKeyName";
//   auto private = V8PrivateProperty::createSymbol(isolate, symbolName);
//   ...
class CORE_EXPORT V8PrivateProperty {
  USING_FAST_MALLOC(V8PrivateProperty);
  WTF_MAKE_NONCOPYABLE(V8PrivateProperty);

 public:
  // Provides fast access to V8's private properties.
  //
  // Retrieving/creating a global private symbol from a string is very
  // expensive compared to get or set a private property.  This class
  // provides a way to cache a private symbol and re-use it.
  class CORE_EXPORT Symbol {
    STACK_ALLOCATED();

   public:
    bool hasValue(v8::Local<v8::Object> object) const {
      return object->HasPrivate(context(), m_privateSymbol).ToChecked();
    }

    // Returns the value of the private property if set, or undefined.
    v8::Local<v8::Value> getOrUndefined(v8::Local<v8::Object> object) const {
      return object->GetPrivate(context(), m_privateSymbol).ToLocalChecked();
    }

    // TODO(peria): Remove this method, and use getOrUndefined() instead.
    // Returns the value of the private property if set, or an empty handle.
    v8::Local<v8::Value> getOrEmpty(v8::Local<v8::Object> object) const {
      if (hasValue(object))
        return getOrUndefined(object);
      return v8::Local<v8::Value>();
    }

    bool set(v8::Local<v8::Object> object, v8::Local<v8::Value> value) const {
      return object->SetPrivate(context(), m_privateSymbol, value).ToChecked();
    }

    bool deleteProperty(v8::Local<v8::Object> object) const {
      return object->DeletePrivate(context(), m_privateSymbol).ToChecked();
    }

    v8::Local<v8::Private> getPrivate() const { return m_privateSymbol; }

   private:
    friend class V8PrivateProperty;
    // The following classes are exceptionally allowed to call to
    // getFromMainWorld.
    friend class V8CustomEvent;
    friend class V8ServiceWorkerMessageEventInternal;

    Symbol(v8::Isolate* isolate, v8::Local<v8::Private> privateSymbol)
        : m_privateSymbol(privateSymbol), m_isolate(isolate) {}

    // To get/set private property, we should use the current context.
    v8::Local<v8::Context> context() const {
      return m_isolate->GetCurrentContext();
    }

    // Only friend classes are allowed to use this API.
    v8::Local<v8::Value> getFromMainWorld(ScriptWrappable*);

    v8::Local<v8::Private> m_privateSymbol;
    v8::Isolate* m_isolate;
  };

  static std::unique_ptr<V8PrivateProperty> create() {
    return WTF::wrapUnique(new V8PrivateProperty());
  }

#define V8_PRIVATE_PROPERTY_DEFINE_GETTER(InterfaceName, KeyName)              \
  static Symbol V8_PRIVATE_PROPERTY_GETTER_NAME(/* // NOLINT */                \
                                                InterfaceName, KeyName)(       \
      v8::Isolate * isolate) {                                                 \
    V8PrivateProperty* privateProp =                                           \
        V8PerIsolateData::from(isolate)->privateProperty();                    \
    v8::Eternal<v8::Private>& propertyHandle =                                 \
        privateProp->V8_PRIVATE_PROPERTY_MEMBER_NAME(InterfaceName, KeyName);  \
    if (UNLIKELY(propertyHandle.IsEmpty())) {                                  \
      propertyHandle.Set(                                                      \
          isolate, createV8Private(isolate, V8_PRIVATE_PROPERTY_SYMBOL_STRING( \
                                                InterfaceName, KeyName)));     \
    }                                                                          \
    return Symbol(isolate, propertyHandle.Get(isolate));                       \
  }

  V8_PRIVATE_PROPERTY_FOR_EACH(V8_PRIVATE_PROPERTY_DEFINE_GETTER)
#undef V8_PRIVATE_PROPERTY_DEFINE_GETTER

  // TODO(peria): Do not use this specialized hack. See a TODO comment
  // on m_symbolWindowDocumentCachedAccessor.
  static Symbol getWindowDocumentCachedAccessor(v8::Isolate* isolate) {
    V8PrivateProperty* privateProp =
        V8PerIsolateData::from(isolate)->privateProperty();
    if (UNLIKELY(privateProp->m_symbolWindowDocumentCachedAccessor.isEmpty())) {
      privateProp->m_symbolWindowDocumentCachedAccessor.set(
          isolate, createCachedV8Private(
                       isolate, V8_PRIVATE_PROPERTY_SYMBOL_STRING(
                                    "Window", "DocumentCachedAccessor")));
    }
    return Symbol(
        isolate,
        privateProp->m_symbolWindowDocumentCachedAccessor.newLocal(isolate));
  }

  static Symbol getSymbol(v8::Isolate* isolate, const char* symbol) {
    return Symbol(isolate, createCachedV8Private(isolate, symbol));
  }

 private:
  V8PrivateProperty() {}

  static v8::Local<v8::Private> createV8Private(v8::Isolate*,
                                                const char* symbol);
  // TODO(peria): Remove this method. We should not use v8::Private::ForApi().
  static v8::Local<v8::Private> createCachedV8Private(v8::Isolate*,
                                                      const char* symbol);

#define V8_PRIVATE_PROPERTY_DECLARE_MEMBER(InterfaceName, KeyName) \
  v8::Eternal<v8::Private> V8_PRIVATE_PROPERTY_MEMBER_NAME(        \
      InterfaceName, KeyName);  // NOLINT(readability/naming/underscores)
  V8_PRIVATE_PROPERTY_FOR_EACH(V8_PRIVATE_PROPERTY_DECLARE_MEMBER)
#undef V8_PRIVATE_PROPERTY_DECLARE_MEMBER

  // TODO(peria): Do not use this specialized hack for
  // Window#DocumentCachedAccessor. This is required to put v8::Private key in
  // a snapshot, and it cannot be a v8::Eternal<> due to V8 serializer's
  // requirement.
  ScopedPersistent<v8::Private> m_symbolWindowDocumentCachedAccessor;
};

}  // namespace blink

#endif  // V8PrivateProperty_h
