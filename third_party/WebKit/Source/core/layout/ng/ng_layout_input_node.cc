// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_layout_input_node.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/ng_block_node.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {
namespace {

#ifndef NDEBUG
void AppendNodeToString(const NGLayoutInputNode* node,
                        StringBuilder* string_builder,
                        unsigned indent = 2) {
  if (!node)
    return;
  DCHECK(string_builder);

  string_builder->Append(node->ToString());
  string_builder->Append("\n");

  StringBuilder indent_builder;
  for (unsigned i = 0; i < indent; i++)
    indent_builder.Append(" ");

  if (node->IsBlock()) {
    auto* first_child =
        ToNGBlockNode(const_cast<NGLayoutInputNode*>(node))->FirstChild();
    for (NGLayoutInputNode* node_runner = first_child; node_runner;
         node_runner = node_runner->NextSibling()) {
      string_builder->Append(indent_builder.ToString());
      AppendNodeToString(node_runner, string_builder, indent + 2);
    }
  }

  if (node->IsInline()) {
    for (const NGInlineItem& inline_item : ToNGInlineNode(node)->Items()) {
      string_builder->Append(indent_builder.ToString());
      string_builder->Append(inline_item.ToString());
      string_builder->Append("\n");
    }
    auto* next_sibling =
        ToNGInlineNode(const_cast<NGLayoutInputNode*>(node))->NextSibling();
    for (NGLayoutInputNode* node_runner = next_sibling; node_runner;
         node_runner = node_runner->NextSibling()) {
      string_builder->Append(indent_builder.ToString());
      AppendNodeToString(node_runner, string_builder, indent + 2);
    }
  }
}
#endif

}  // namespace

#ifndef NDEBUG
void NGLayoutInputNode::ShowNodeTree() const {
  StringBuilder string_builder;
  string_builder.Append("\n.:: LayoutNG Node Tree ::.\n\n");
  AppendNodeToString(this, &string_builder);
  fprintf(stderr, "%s\n", string_builder.ToString().Utf8().data());
}
#endif

}  // namespace blink
