// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBlockChildIterator_h
#define NGBlockChildIterator_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_layout_input_node.h"

namespace blink {

class NGBreakToken;
class NGBlockBreakToken;

// A utility class for block-flow layout which given the first child and a
// break token will iterate through unfinished children.
//
// This class does not handle modifications to its arguments after it has been
// constructed.
class CORE_EXPORT NGBlockChildIterator {
  STACK_ALLOCATED();

 public:
  NGBlockChildIterator(NGLayoutInputNode first_child,
                       NGBlockBreakToken* break_token);

  // Returns the next input node which should be laid out, along with its
  // respective break token.
  struct Entry;
  Entry NextChild();

 private:
  NGLayoutInputNode child_;
  NGBlockBreakToken* break_token_;

  // An index into break_token_'s ChildBreakTokens() vector. Used for keeping
  // track of the next child break token to inspect.
  size_t child_token_idx_;
};

struct NGBlockChildIterator::Entry {
  STACK_ALLOCATED();

  Entry(NGLayoutInputNode node, NGBreakToken* token)
      : node(node), token(token) {}

  NGLayoutInputNode node;
  NGBreakToken* token;

  bool operator==(const NGBlockChildIterator::Entry& other) const {
    return node == other.node && token == other.token;
  }
};

}  // namespace blink

#endif  // NGBlockChildIterator_h
