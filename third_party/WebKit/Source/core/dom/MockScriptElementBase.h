// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockScriptElementBase_h
#define MockScriptElementBase_h

#include "bindings/core/v8/HTMLScriptElementOrSVGScriptElement.h"
#include "core/dom/Document.h"
#include "core/dom/ScriptElementBase.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blink {

class MockScriptElementBase
    : public GarbageCollectedFinalized<MockScriptElementBase>,
      public ScriptElementBase {
  USING_GARBAGE_COLLECTED_MIXIN(MockScriptElementBase);

 public:
  static MockScriptElementBase* Create() {
    return new ::testing::StrictMock<MockScriptElementBase>();
  }
  virtual ~MockScriptElementBase() {}

  MOCK_METHOD0(DispatchLoadEvent, void());
  MOCK_METHOD0(DispatchErrorEvent, void());
  MOCK_CONST_METHOD0(AsyncAttributeValue, bool());
  MOCK_CONST_METHOD0(CharsetAttributeValue, String());
  MOCK_CONST_METHOD0(CrossOriginAttributeValue, String());
  MOCK_CONST_METHOD0(DeferAttributeValue, bool());
  MOCK_CONST_METHOD0(EventAttributeValue, String());
  MOCK_CONST_METHOD0(ForAttributeValue, String());
  MOCK_CONST_METHOD0(IntegrityAttributeValue, String());
  MOCK_CONST_METHOD0(LanguageAttributeValue, String());
  MOCK_CONST_METHOD0(NomoduleAttributeValue, bool());
  MOCK_CONST_METHOD0(SourceAttributeValue, String());
  MOCK_CONST_METHOD0(TypeAttributeValue, String());

  MOCK_METHOD0(TextFromChildren, String());
  MOCK_CONST_METHOD0(HasSourceAttribute, bool());
  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_CONST_METHOD0(HasChildren, bool());
  MOCK_CONST_METHOD0(GetNonceForElement, const AtomicString&());
  MOCK_CONST_METHOD0(InitiatorName, AtomicString());
  MOCK_METHOD3(AllowInlineScriptForCSP,
               bool(const AtomicString&,
                    const WTF::OrdinalNumber&,
                    const String&));
  MOCK_CONST_METHOD0(GetDocument, Document&());
  MOCK_METHOD1(SetScriptElementForBinding,
               void(HTMLScriptElementOrSVGScriptElement&));
  MOCK_CONST_METHOD0(Loader, ScriptLoader*());

  DEFINE_INLINE_VIRTUAL_TRACE() { ScriptElementBase::Trace(visitor); }
};

}  // namespace blink

#endif  // MockScriptElementBase_h
