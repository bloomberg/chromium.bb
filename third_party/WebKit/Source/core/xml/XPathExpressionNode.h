/*
 * Copyright (C) 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006, 2009 Apple Inc.
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

#ifndef XPathExpressionNode_h
#define XPathExpressionNode_h

#include "core/CoreExport.h"
#include "core/dom/Node.h"
#include "core/xml/XPathValue.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/StringHash.h"

namespace blink {

namespace XPath {

struct CORE_EXPORT EvaluationContext {
  STACK_ALLOCATED();

 public:
  explicit EvaluationContext(Node&);

  Member<Node> node;
  unsigned long size;
  unsigned long position;
  HashMap<String, String> variable_bindings;

  bool had_type_conversion_error;
};

class CORE_EXPORT ParseNode : public GarbageCollectedFinalized<ParseNode> {
 public:
  virtual ~ParseNode() {}
  virtual void Trace(blink::Visitor* visitor) {}
};

class CORE_EXPORT Expression : public ParseNode {
  WTF_MAKE_NONCOPYABLE(Expression);

 public:
  Expression();
  ~Expression() override;
  virtual void Trace(blink::Visitor*);

  virtual Value Evaluate(EvaluationContext&) const = 0;

  void AddSubExpression(Expression* expr) {
    is_context_node_sensitive_ |= expr->is_context_node_sensitive_;
    is_context_position_sensitive_ |= expr->is_context_position_sensitive_;
    is_context_size_sensitive_ |= expr->is_context_size_sensitive_;
    sub_expressions_.push_back(expr);
  }

  bool IsContextNodeSensitive() const { return is_context_node_sensitive_; }
  bool IsContextPositionSensitive() const {
    return is_context_position_sensitive_;
  }
  bool IsContextSizeSensitive() const { return is_context_size_sensitive_; }
  void SetIsContextNodeSensitive(bool value) {
    is_context_node_sensitive_ = value;
  }
  void SetIsContextPositionSensitive(bool value) {
    is_context_position_sensitive_ = value;
  }
  void SetIsContextSizeSensitive(bool value) {
    is_context_size_sensitive_ = value;
  }

  virtual Value::Type ResultType() const = 0;

 protected:
  unsigned SubExprCount() const { return sub_expressions_.size(); }
  Expression* SubExpr(unsigned i) { return sub_expressions_[i].Get(); }
  const Expression* SubExpr(unsigned i) const {
    return sub_expressions_[i].Get();
  }

 private:
  HeapVector<Member<Expression>> sub_expressions_;

  // Evaluation details that can be used for optimization.
  bool is_context_node_sensitive_;
  bool is_context_position_sensitive_;
  bool is_context_size_sensitive_;
};

}  // namespace XPath

}  // namespace blink

#endif  // XPathExpressionNode_h
