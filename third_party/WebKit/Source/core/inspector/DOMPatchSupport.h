/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DOMPatchSupport_h
#define DOMPatchSupport_h

#include "platform/heap/Handle.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ContainerNode;
class DOMEditor;
class Document;
class ExceptionState;
class Node;

class DOMPatchSupport final {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(DOMPatchSupport);

 public:
  static void PatchDocument(Document&, const String& markup);

  DOMPatchSupport(DOMEditor*, Document&);

  void PatchDocument(const String& markup);
  Node* PatchNode(Node*, const String& markup, ExceptionState&);

 private:
  class Digest : public GarbageCollectedFinalized<Digest> {
   public:
    explicit Digest(Node* node) : node_(node) {}
    DECLARE_TRACE();

    String sha1_;
    String attrs_sha1_;
    Member<Node> node_;
    HeapVector<Member<Digest>> children_;
  };

  typedef HeapVector<std::pair<Member<Digest>, size_t>> ResultMap;
  typedef HeapHashMap<String, Member<Digest>> UnusedNodesMap;

  bool InnerPatchNode(Digest* old_node, Digest* new_node, ExceptionState&);
  std::pair<ResultMap, ResultMap> Diff(
      const HeapVector<Member<Digest>>& old_children,
      const HeapVector<Member<Digest>>& new_children);
  bool InnerPatchChildren(ContainerNode*,
                          const HeapVector<Member<Digest>>& old_children,
                          const HeapVector<Member<Digest>>& new_children,
                          ExceptionState&);
  Digest* CreateDigest(Node*, UnusedNodesMap*);
  bool InsertBeforeAndMarkAsUsed(ContainerNode*,
                                 Digest*,
                                 Node* anchor,
                                 ExceptionState&);
  bool RemoveChildAndMoveToNew(Digest*, ExceptionState&);
  void MarkNodeAsUsed(Digest*);
  Document& GetDocument() const { return *document_; }

  Member<DOMEditor> dom_editor_;
  Member<Document> document_;

  UnusedNodesMap unused_nodes_map_;
};

}  // namespace blink

#endif  // !defined(DOMPatchSupport_h)
