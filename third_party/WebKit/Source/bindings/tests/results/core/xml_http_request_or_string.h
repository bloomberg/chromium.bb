// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/WebKit/Source/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef XMLHttpRequestOrString_h
#define XMLHttpRequestOrString_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Optional.h"

namespace blink {

class XMLHttpRequest;

class CORE_EXPORT XMLHttpRequestOrString final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
 public:
  XMLHttpRequestOrString();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsString() const { return type_ == SpecificType::kString; }
  const String& GetAsString() const;
  void SetString(const String&);
  static XMLHttpRequestOrString FromString(const String&);

  bool IsXMLHttpRequest() const { return type_ == SpecificType::kXMLHttpRequest; }
  XMLHttpRequest* GetAsXMLHttpRequest() const;
  void SetXMLHttpRequest(XMLHttpRequest*);
  static XMLHttpRequestOrString FromXMLHttpRequest(XMLHttpRequest*);

  XMLHttpRequestOrString(const XMLHttpRequestOrString&);
  ~XMLHttpRequestOrString();
  XMLHttpRequestOrString& operator=(const XMLHttpRequestOrString&);
  void Trace(blink::Visitor*);

 private:
  enum class SpecificType {
    kNone,
    kString,
    kXMLHttpRequest,
  };
  SpecificType type_;

  String string_;
  Member<XMLHttpRequest> xml_http_request_;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const XMLHttpRequestOrString&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8XMLHttpRequestOrString final {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, XMLHttpRequestOrString&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const XMLHttpRequestOrString&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, XMLHttpRequestOrString& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, XMLHttpRequestOrString& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<XMLHttpRequestOrString> : public NativeValueTraitsBase<XMLHttpRequestOrString> {
  CORE_EXPORT static XMLHttpRequestOrString NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
  CORE_EXPORT static XMLHttpRequestOrString NullValue() { return XMLHttpRequestOrString(); }
};

template <>
struct V8TypeOf<XMLHttpRequestOrString> {
  typedef V8XMLHttpRequestOrString Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::XMLHttpRequestOrString);

#endif  // XMLHttpRequestOrString_h
