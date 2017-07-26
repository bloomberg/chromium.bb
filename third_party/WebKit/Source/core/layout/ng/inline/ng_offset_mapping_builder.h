// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGOffsetMappingBuilder_h
#define NGOffsetMappingBuilder_h

#include "core/CoreExport.h"
#include "core/layout/LayoutText.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class NGOffsetMappingResult;

// This is the helper class for constructing the DOM-to-TextContent offset
// mapping. It holds an offset mapping, and provides APIs to modify the mapping
// step by step until the construction is finished.
// Design doc: https://goo.gl/CJbxky
// TODO(xiaochengh): Change the mock implemetation to a real one.
class CORE_EXPORT NGOffsetMappingBuilder {
  STACK_ALLOCATED();

 public:
  NGOffsetMappingBuilder();

  // Associate the offset mapping with a simple annotation with the given node
  // as its value.
  void Annotate(const LayoutText*);

  // Append an identity offset mapping of the specified length with null
  // annotation to the builder.
  void AppendIdentityMapping(unsigned length);

  // Append a collapsed offset mapping from the specified length with null
  // annotation to the builder.
  void AppendCollapsedMapping(unsigned length);

  // TODO(xiaochengh): Add the following API when we start to fix offset mapping
  // for text-transform.
  // Append an expanded offset mapping to the specified length with null
  // annotation to the builder.
  // void AppendExpandedMapping(unsigned length);

  // This function should only be called by NGInlineItemsBuilder during
  // whitespace collapsing, and in the case that the target string of the
  // currently held mapping:
  //   (i) has at least |skip_length + 1| characters,
  //  (ii) has the last |skip_length| characters being non-text extra
  //       characters, and
  // (iii) has the |skip_length + 1|-st last character being a space.
  // This function changes the space into collapsed.
  void CollapseTrailingSpace(unsigned index);

  // Concatenate the offset mapping held by another builder to this builder.
  void Concatenate(const NGOffsetMappingBuilder&);

  // Composite the offset mapping held by another builder to this builder.
  void Composite(const NGOffsetMappingBuilder&);

  // Finalize and return the offset mapping.
  NGOffsetMappingResult Build() const;

  // Exposed for testing only.
  Vector<unsigned> DumpOffsetMappingForTesting() const;
  Vector<const LayoutText*> DumpAnnotationForTesting() const;

 private:
  // A mock implementation of the offset mapping builder that stores the mapping
  // value of every offset in the plain way. This is simple but inefficient, and
  // will be replaced by a real efficient implementation.
  Vector<unsigned> mapping_;

  // A mock implementation that stores the annotation value of all offsets in
  // the plain way. It will be replaced by a real implementation for efficiency.
  Vector<const LayoutText*> annotation_;

  DISALLOW_COPY_AND_ASSIGN(NGOffsetMappingBuilder);
};

}  // namespace blink

#endif  // OffsetMappingBuilder_h
