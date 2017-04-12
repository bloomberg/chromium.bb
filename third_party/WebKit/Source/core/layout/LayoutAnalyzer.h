// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutAnalyzer_h
#define LayoutAnalyzer_h

#include <memory>
#include "platform/LayoutUnit.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class LayoutBlock;
class LayoutObject;
class TracedValue;

// Observes the performance of layout and reports statistics via a TracedValue.
// Usage:
// LayoutAnalyzer::Scope analyzer(*this);
class LayoutAnalyzer {
  USING_FAST_MALLOC(LayoutAnalyzer);
  WTF_MAKE_NONCOPYABLE(LayoutAnalyzer);

 public:
  enum Counter {
    kLayoutBlockWidthChanged,
    kLayoutBlockHeightChanged,
    kLayoutBlockSizeChanged,
    kLayoutBlockSizeDidNotChange,
    kLayoutObjectsThatSpecifyColumns,
    kLayoutAnalyzerStackMaximumDepth,
    kLayoutObjectsThatAreFloating,
    kLayoutObjectsThatHaveALayer,
    kLayoutInlineObjectsThatAlwaysCreateLineBoxes,
    kLayoutObjectsThatHadNeverHadLayout,
    kLayoutObjectsThatAreOutOfFlowPositioned,
    kLayoutObjectsThatNeedPositionedMovementLayout,
    kPerformLayoutRootLayoutObjects,
    kLayoutObjectsThatNeedLayoutForThemselves,
    kLayoutObjectsThatNeedSimplifiedLayout,
    kLayoutObjectsThatAreTableCells,
    kLayoutObjectsThatAreTextAndCanNotUseTheSimpleFontCodePath,
    kCharactersInLayoutObjectsThatAreTextAndCanNotUseTheSimpleFontCodePath,
    kLayoutObjectsThatAreTextAndCanUseTheSimpleFontCodePath,
    kCharactersInLayoutObjectsThatAreTextAndCanUseTheSimpleFontCodePath,
    kTotalLayoutObjectsThatWereLaidOut,
  };
  static const size_t kNumCounters = 21;

  class Scope {
    STACK_ALLOCATED();

   public:
    explicit Scope(const LayoutObject&);
    ~Scope();

   private:
    const LayoutObject& layout_object_;
    LayoutAnalyzer* analyzer_;
  };

  class BlockScope {
    STACK_ALLOCATED();

   public:
    explicit BlockScope(const LayoutBlock&);
    ~BlockScope();

   private:
    const LayoutBlock& block_;
    LayoutUnit width_;
    LayoutUnit height_;
  };

  LayoutAnalyzer() {}

  void Reset();
  void Push(const LayoutObject&);
  void Pop(const LayoutObject&);

  void Increment(Counter counter, unsigned delta = 1) {
    counters_[counter] += delta;
  }

  std::unique_ptr<TracedValue> ToTracedValue();

 private:
  const char* NameForCounter(Counter) const;

  double start_ms_;
  unsigned depth_;
  unsigned counters_[kNumCounters];
};

}  // namespace blink

#endif  // LayoutAnalyzer_h
