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

#include "core/xml/XPathResult.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/xml/XPathEvaluator.h"
#include "core/xml/XPathExpressionNode.h"

namespace blink {

using namespace XPath;

XPathResult::XPathResult(EvaluationContext& context, const Value& value)
    : value_(value), node_set_position_(0), dom_tree_version_(0) {
  switch (value_.GetType()) {
    case Value::kBooleanValue:
      result_type_ = kBooleanType;
      return;
    case Value::kNumberValue:
      result_type_ = kNumberType;
      return;
    case Value::kStringValue:
      result_type_ = kStringType;
      return;
    case Value::kNodeSetValue:
      result_type_ = kUnorderedNodeIteratorType;
      node_set_position_ = 0;
      node_set_ = NodeSet::Create(value_.ToNodeSet(&context));
      document_ = &context.node->GetDocument();
      dom_tree_version_ = document_->DomTreeVersion();
      return;
  }
  NOTREACHED();
}

void XPathResult::Trace(blink::Visitor* visitor) {
  visitor->Trace(value_);
  visitor->Trace(node_set_);
  visitor->Trace(document_);
  ScriptWrappable::Trace(visitor);
}

void XPathResult::ConvertTo(unsigned short type,
                            ExceptionState& exception_state) {
  switch (type) {
    case kAnyType:
      break;
    case kNumberType:
      result_type_ = type;
      value_ = value_.ToNumber();
      break;
    case kStringType:
      result_type_ = type;
      value_ = value_.ToString();
      break;
    case kBooleanType:
      result_type_ = type;
      value_ = value_.ToBoolean();
      break;
    case kUnorderedNodeIteratorType:
    case kUnorderedNodeSnapshotType:
    case kAnyUnorderedNodeType:
    // This is correct - singleNodeValue() will take care of ordering.
    case kFirstOrderedNodeType:
      if (!value_.IsNodeSet()) {
        exception_state.ThrowTypeError(
            "The result is not a node set, and therefore cannot be converted "
            "to the desired type.");
        return;
      }
      result_type_ = type;
      break;
    case kOrderedNodeIteratorType:
      if (!value_.IsNodeSet()) {
        exception_state.ThrowTypeError(
            "The result is not a node set, and therefore cannot be converted "
            "to the desired type.");
        return;
      }
      GetNodeSet().Sort();
      result_type_ = type;
      break;
    case kOrderedNodeSnapshotType:
      if (!value_.IsNodeSet()) {
        exception_state.ThrowTypeError(
            "The result is not a node set, and therefore cannot be converted "
            "to the desired type.");
        return;
      }
      value_.ToNodeSet(nullptr).Sort();
      result_type_ = type;
      break;
  }
}

unsigned short XPathResult::resultType() const {
  return result_type_;
}

double XPathResult::numberValue(ExceptionState& exception_state) const {
  if (resultType() != kNumberType) {
    exception_state.ThrowTypeError("The result type is not a number.");
    return 0.0;
  }
  return value_.ToNumber();
}

String XPathResult::stringValue(ExceptionState& exception_state) const {
  if (resultType() != kStringType) {
    exception_state.ThrowTypeError("The result type is not a string.");
    return String();
  }
  return value_.ToString();
}

bool XPathResult::booleanValue(ExceptionState& exception_state) const {
  if (resultType() != kBooleanType) {
    exception_state.ThrowTypeError("The result type is not a boolean.");
    return false;
  }
  return value_.ToBoolean();
}

Node* XPathResult::singleNodeValue(ExceptionState& exception_state) const {
  if (resultType() != kAnyUnorderedNodeType &&
      resultType() != kFirstOrderedNodeType) {
    exception_state.ThrowTypeError("The result type is not a single node.");
    return nullptr;
  }

  const NodeSet& nodes = value_.ToNodeSet(nullptr);
  if (resultType() == kFirstOrderedNodeType)
    return nodes.FirstNode();
  return nodes.AnyNode();
}

bool XPathResult::invalidIteratorState() const {
  if (resultType() != kUnorderedNodeIteratorType &&
      resultType() != kOrderedNodeIteratorType)
    return false;

  DCHECK(document_);
  return document_->DomTreeVersion() != dom_tree_version_;
}

unsigned XPathResult::snapshotLength(ExceptionState& exception_state) const {
  if (resultType() != kUnorderedNodeSnapshotType &&
      resultType() != kOrderedNodeSnapshotType) {
    exception_state.ThrowTypeError("The result type is not a snapshot.");
    return 0;
  }

  return value_.ToNodeSet(nullptr).size();
}

Node* XPathResult::iterateNext(ExceptionState& exception_state) {
  if (resultType() != kUnorderedNodeIteratorType &&
      resultType() != kOrderedNodeIteratorType) {
    exception_state.ThrowTypeError("The result type is not an iterator.");
    return nullptr;
  }

  if (invalidIteratorState()) {
    exception_state.ThrowDOMException(
        kInvalidStateError,
        "The document has mutated since the result was returned.");
    return nullptr;
  }

  if (node_set_position_ + 1 > GetNodeSet().size())
    return nullptr;

  Node* node = GetNodeSet()[node_set_position_];

  node_set_position_++;

  return node;
}

Node* XPathResult::snapshotItem(unsigned index,
                                ExceptionState& exception_state) {
  if (resultType() != kUnorderedNodeSnapshotType &&
      resultType() != kOrderedNodeSnapshotType) {
    exception_state.ThrowTypeError("The result type is not a snapshot.");
    return nullptr;
  }

  const NodeSet& nodes = value_.ToNodeSet(nullptr);
  if (index >= nodes.size())
    return nullptr;

  return nodes[index];
}

}  // namespace blink
