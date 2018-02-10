// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentParserTiming_h
#define DocumentParserTiming_h

#include "base/macros.h"
#include "core/dom/Document.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

// DocumentParserTiming is responsible for tracking parser-related timings for a
// given document.
class DocumentParserTiming final
    : public GarbageCollectedFinalized<DocumentParserTiming>,
      public Supplement<Document> {
  USING_GARBAGE_COLLECTED_MIXIN(DocumentParserTiming);

 public:
  static const char kSupplementName[];

  virtual ~DocumentParserTiming() = default;

  static DocumentParserTiming& From(Document&);

  // markParserStart and markParserStop methods record the time that the
  // parser was first started/stopped, and notify that the document parser
  // timing has changed. These methods do nothing (early return) if a time has
  // already been recorded for the given parser event, or if a parser has
  // already been detached.
  void MarkParserStart();
  void MarkParserStop();

  // markParserDetached records that the parser is detached from the
  // document. A single document may have multiple parsers, if e.g. the
  // document is re-opened using document.write. DocumentParserTiming only
  // wants to record parser start and stop time for the first parser. To avoid
  // recording parser start and stop times for re-opened documents, we keep
  // track of whether a parser has been detached, and avoid recording
  // start/stop times for subsequent parsers, after the first parser has been
  // detached.
  void MarkParserDetached();

  // Record a duration of time that the parser yielded due to loading a
  // script, in seconds. scriptInsertedViaDocumentWrite indicates whether the
  // script causing blocking was inserted via document.write. This may be
  // called multiple times, once for each time the parser yields on a script
  // load.
  void RecordParserBlockedOnScriptLoadDuration(
      double duration,
      bool script_inserted_via_document_write);

  // Record a duration of time that the parser spent executing a script, in
  // seconds. scriptInsertedViaDocumentWrite indicates whether the script
  // being executed was inserted via document.write. This may be called
  // multiple times, once for each time the parser executes a script.
  void RecordParserBlockedOnScriptExecutionDuration(
      double duration,
      bool script_inserted_via_document_write);

  // The getters below return monotonically-increasing seconds, or zero if the
  // given parser event has not yet occurred.  See the comments for
  // monotonicallyIncreasingTime in wtf/Time.h for additional details.

  TimeTicks ParserStart() const { return parser_start_; }
  TimeTicks ParserStop() const { return parser_stop_; }

  // Returns the sum of all blocking script load durations reported via
  // recordParseBlockedOnScriptLoadDuration.
  double ParserBlockedOnScriptLoadDuration() const {
    return parser_blocked_on_script_load_duration_;
  }

  // Returns the sum of all blocking script load durations due to
  // document.write reported via recordParseBlockedOnScriptLoadDuration. Note
  // that some uncommon cases are not currently covered by this method. See
  // crbug/600711 for details.
  double ParserBlockedOnScriptLoadFromDocumentWriteDuration() const {
    return parser_blocked_on_script_load_from_document_write_duration_;
  }

  // Returns the sum of all script execution durations reported via
  // recordParseBlockedOnScriptExecutionDuration.
  double ParserBlockedOnScriptExecutionDuration() const {
    return parser_blocked_on_script_execution_duration_;
  }

  // Returns the sum of all script execution durations due to
  // document.write reported via recordParseBlockedOnScriptExecutionDuration.
  // Note that some uncommon cases are not currently covered by this method. See
  // crbug/600711 for details.
  double ParserBlockedOnScriptExecutionFromDocumentWriteDuration() const {
    return parser_blocked_on_script_execution_from_document_write_duration_;
  }

  virtual void Trace(blink::Visitor*);

 private:
  explicit DocumentParserTiming(Document&);
  void NotifyDocumentParserTimingChanged();

  TimeTicks parser_start_;
  TimeTicks parser_stop_;
  double parser_blocked_on_script_load_duration_ = 0.0;
  double parser_blocked_on_script_load_from_document_write_duration_ = 0.0;
  double parser_blocked_on_script_execution_duration_ = 0.0;
  double parser_blocked_on_script_execution_from_document_write_duration_ = 0.0;
  bool parser_detached_ = false;
  DISALLOW_COPY_AND_ASSIGN(DocumentParserTiming);
};

}  // namespace blink

#endif
