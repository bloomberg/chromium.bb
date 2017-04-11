/*
 * Copyright (C) 2010 Adam Barth. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SuffixTree_h
#define SuffixTree_h

#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class UnicodeCodebook {
  STATIC_ONLY(UnicodeCodebook);

 public:
  static int CodeWord(UChar c) { return c; }
  enum { kCodeSize = 1 << 8 * sizeof(UChar) };
};

class ASCIICodebook {
  STATIC_ONLY(ASCIICodebook);

 public:
  static int CodeWord(UChar c) { return c & (kCodeSize - 1); }
  enum { kCodeSize = 1 << (8 * sizeof(char) - 1) };
};

template <typename Codebook>
class SuffixTree {
  USING_FAST_MALLOC(SuffixTree);
  WTF_MAKE_NONCOPYABLE(SuffixTree);

 public:
  SuffixTree(const String& text, unsigned depth) : depth_(depth), leaf_(true) {
    Build(text);
  }

  bool MightContain(const String& query) {
    Node* current = &root_;
    int limit = std::min(depth_, query.length());
    for (int i = 0; i < limit; ++i) {
      current = current->at(Codebook::CodeWord(query[i]));
      if (!current)
        return false;
    }
    return true;
  }

 private:
  class Node {
    USING_FAST_MALLOC(Node);
    WTF_MAKE_NONCOPYABLE(Node);

   public:
    Node(bool is_leaf = false) {
      children_.Resize(Codebook::kCodeSize);
      children_.Fill(0);
      is_leaf_ = is_leaf;
    }

    ~Node() {
      for (unsigned i = 0; i < children_.size(); ++i) {
        Node* child = children_.at(i);
        if (child && !child->is_leaf_)
          delete child;
      }
    }

    Node*& at(int code_word) { return children_.at(code_word); }

   private:
    typedef Vector<Node*, Codebook::kCodeSize> ChildrenVector;

    ChildrenVector children_;
    bool is_leaf_;
  };

  void Build(const String& text) {
    for (unsigned base = 0; base < text.length(); ++base) {
      Node* current = &root_;
      unsigned limit = std::min(base + depth_, text.length());
      for (unsigned offset = 0; base + offset < limit; ++offset) {
        DCHECK_NE(current, &leaf_);
        Node*& child = current->at(Codebook::CodeWord(text[base + offset]));
        if (!child)
          child = base + offset + 1 == limit ? &leaf_ : new Node();
        current = child;
      }
    }
  }

  Node root_;
  unsigned depth_;

  // Instead of allocating a fresh empty leaf node for ever leaf in the tree
  // (there can be a lot of these), we alias all the leaves to this "static"
  // leaf node.
  Node leaf_;
};

}  // namespace blink

#endif  // SuffixTree_h
