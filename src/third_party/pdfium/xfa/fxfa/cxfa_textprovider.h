// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_CXFA_TEXTPROVIDER_H_
#define XFA_FXFA_CXFA_TEXTPROVIDER_H_

#include "core/fxcrt/widestring.h"
#include "fxjs/gc/heap.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "v8/include/cppgc/garbage-collected.h"
#include "v8/include/cppgc/member.h"
#include "v8/include/cppgc/visitor.h"
#include "xfa/fxfa/cxfa_textlayout.h"

class CXFA_Font;
class CXFA_Node;
class CXFA_Para;

class CXFA_TextProvider : public cppgc::GarbageCollected<CXFA_TextProvider> {
 public:
  enum class Type : uint8_t {
    kText,
    kCaption,
    kRollover,
    kDown,
  };

  CONSTRUCT_VIA_MAKE_GARBAGE_COLLECTED;
  ~CXFA_TextProvider();

  void Trace(cppgc::Visitor* visitor) const;

  CXFA_Node* GetTextNode(bool* bRichText);
  CXFA_Para* GetParaIfExists();
  CXFA_Font* GetFontIfExists();
  bool IsCheckButtonAndAutoWidth() const;
  absl::optional<WideString> GetEmbeddedObj(const WideString& wsAttr) const;

 private:
  CXFA_TextProvider(CXFA_Node* pNode, Type eType);

  cppgc::Member<CXFA_Node> m_pNode;
  const Type m_eType;
};

#endif  // XFA_FXFA_CXFA_TEXTPROVIDER_H_
