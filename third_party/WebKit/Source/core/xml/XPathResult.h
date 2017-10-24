/*
 * Copyright (C) 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef XPathResult_h
#define XPathResult_h

#include "core/xml/XPathValue.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class Document;
class ExceptionState;
class Node;

namespace XPath {
struct EvaluationContext;
}

class XPathResult final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum XPathResultType {
    kAnyType = 0,
    kNumberType = 1,
    kStringType = 2,
    kBooleanType = 3,
    kUnorderedNodeIteratorType = 4,
    kOrderedNodeIteratorType = 5,
    kUnorderedNodeSnapshotType = 6,
    kOrderedNodeSnapshotType = 7,
    kAnyUnorderedNodeType = 8,
    kFirstOrderedNodeType = 9
  };

  static XPathResult* Create(XPath::EvaluationContext& context,
                             const XPath::Value& value) {
    return new XPathResult(context, value);
  }

  void ConvertTo(unsigned short type, ExceptionState&);

  unsigned short resultType() const;

  double numberValue(ExceptionState&) const;
  String stringValue(ExceptionState&) const;
  bool booleanValue(ExceptionState&) const;
  Node* singleNodeValue(ExceptionState&) const;

  bool invalidIteratorState() const;
  unsigned snapshotLength(ExceptionState&) const;
  Node* iterateNext(ExceptionState&);
  Node* snapshotItem(unsigned index, ExceptionState&);

  const XPath::Value& GetValue() const { return value_; }

  void Trace(blink::Visitor*);

 private:
  XPathResult(XPath::EvaluationContext&, const XPath::Value&);
  XPath::NodeSet& GetNodeSet() { return *node_set_; }

  XPath::Value value_;
  unsigned node_set_position_;
  Member<XPath::NodeSet>
      node_set_;  // FIXME: why duplicate the node set stored in m_value?
  unsigned short result_type_;
  Member<Document> document_;
  uint64_t dom_tree_version_;
};

}  // namespace blink

#endif  // XPathResult_h
