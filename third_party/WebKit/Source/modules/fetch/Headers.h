// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Headers_h
#define Headers_h

#include "bindings/core/v8/Iterable.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "modules/fetch/FetchHeaderList.h"
#include "wtf/Forward.h"

namespace blink {

class Dictionary;
class ExceptionState;

// http://fetch.spec.whatwg.org/#headers-class
class MODULES_EXPORT Headers final : public GarbageCollected<Headers>,
                                     public ScriptWrappable,
                                     public PairIterable<String, String> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum Guard {
    kImmutableGuard,
    kRequestGuard,
    kRequestNoCORSGuard,
    kResponseGuard,
    kNoneGuard
  };

  static Headers* Create();
  static Headers* Create(ExceptionState&);
  static Headers* Create(const Headers*, ExceptionState&);
  static Headers* Create(const Vector<Vector<String>>&, ExceptionState&);
  static Headers* Create(const Dictionary&, ExceptionState&);

  // Shares the FetchHeaderList. Called when creating a Request or Response.
  static Headers* Create(FetchHeaderList*);

  Headers* Clone() const;

  // Headers.idl implementation.
  void append(const String& name, const String& value, ExceptionState&);
  void remove(const String& key, ExceptionState&);
  String get(const String& key, ExceptionState&);
  Vector<String> getAll(const String& key, ExceptionState&);
  bool has(const String& key, ExceptionState&);
  void set(const String& key, const String& value, ExceptionState&);

  void SetGuard(Guard guard) { guard_ = guard; }
  Guard GetGuard() const { return guard_; }

  // These methods should only be called when size() would return 0.
  void FillWith(const Headers*, ExceptionState&);
  void FillWith(const Vector<Vector<String>>&, ExceptionState&);
  void FillWith(const Dictionary&, ExceptionState&);

  FetchHeaderList* HeaderList() const { return header_list_; }
  DECLARE_TRACE();

 private:
  Headers();
  // Shares the FetchHeaderList. Called when creating a Request or Response.
  explicit Headers(FetchHeaderList*);

  Member<FetchHeaderList> header_list_;
  Guard guard_;

  IterationSource* StartIteration(ScriptState*, ExceptionState&) override;
};

}  // namespace blink

#endif  // Headers_h
