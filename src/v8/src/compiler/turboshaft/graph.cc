// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/graph.h"

#include <iomanip>

namespace v8::internal::compiler::turboshaft {

std::ostream& operator<<(std::ostream& os, PrintAsBlockHeader block_header) {
  const Block& block = block_header.block;
  os << "\n" << block.kind() << " " << block.index();
  if (block.IsDeferred()) os << " (deferred)";
  if (!block.Predecessors().empty()) {
    os << " <- ";
    bool first = true;
    for (const Block* pred : block.Predecessors()) {
      if (!first) os << ", ";
      os << pred->index();
      first = false;
    }
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const Graph& graph) {
  for (const Block& block : graph.blocks()) {
    os << PrintAsBlockHeader{block} << "\n";
    for (const Operation& op : graph.operations(block)) {
      os << std::setw(5) << graph.Index(op).id() << ": " << op << "\n";
    }
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const Block::Kind& kind) {
  switch (kind) {
    case Block::Kind::kLoopHeader:
      os << "LOOP";
      break;
    case Block::Kind::kMerge:
      os << "MERGE";
      break;
    case Block::Kind::kBranchTarget:
      os << "BLOCK";
      break;
  }
  return os;
}

}  // namespace v8::internal::compiler::turboshaft
