// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RunSegmenter_h
#define RunSegmenter_h

#include <unicode/uscript.h>
#include <memory>
#include "platform/fonts/FontOrientation.h"
#include "platform/fonts/OrientationIterator.h"
#include "platform/fonts/ScriptRunIterator.h"
#include "platform/fonts/SmallCapsIterator.h"
#include "platform/fonts/SymbolsIterator.h"
#include "platform/fonts/UTF16TextIterator.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

// A tool for segmenting runs prior to shaping, combining ScriptIterator,
// OrientationIterator and SmallCapsIterator, depending on orientaton and
// font-variant of the text run.
class PLATFORM_EXPORT RunSegmenter {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(RunSegmenter);

 public:
  // Indices into the UTF-16 buffer that is passed in
  struct RunSegmenterRange {
    DISALLOW_NEW();
    unsigned start;
    unsigned end;
    UScriptCode script;
    OrientationIterator::RenderOrientation render_orientation;
    FontFallbackPriority font_fallback_priority;
  };

  RunSegmenter(const UChar* buffer, unsigned buffer_size, FontOrientation);

  bool Consume(RunSegmenterRange*);

  static RunSegmenterRange NullRange() {
    return {0, 0, USCRIPT_INVALID_CODE, OrientationIterator::kOrientationKeep,
            FontFallbackPriority::kText};
  }

 private:
  void ConsumeOrientationIteratorPastLastSplit();
  void ConsumeScriptIteratorPastLastSplit();
  void ConsumeSymbolsIteratorPastLastSplit();

  unsigned buffer_size_;
  RunSegmenterRange candidate_range_;
  std::unique_ptr<ScriptRunIterator> script_run_iterator_;
  std::unique_ptr<OrientationIterator> orientation_iterator_;
  std::unique_ptr<SymbolsIterator> symbols_iterator_;
  unsigned last_split_;
  unsigned script_run_iterator_position_;
  unsigned orientation_iterator_position_;
  unsigned symbols_iterator_position_;
  bool at_end_;
};

}  // namespace blink

#endif
