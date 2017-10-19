/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HTMLParserScheduler_h
#define HTMLParserScheduler_h

#include <memory>
#include "core/html/parser/NestingLevelIncrementer.h"
#include "platform/WebTaskRunner.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class HTMLDocumentParser;
class WebTaskRunner;

class PumpSession : public NestingLevelIncrementer {
  STACK_ALLOCATED();

 public:
  PumpSession(unsigned& nesting_level);
  ~PumpSession();
};

class SpeculationsPumpSession : public NestingLevelIncrementer {
  STACK_ALLOCATED();

 public:
  SpeculationsPumpSession(unsigned& nesting_level);
  ~SpeculationsPumpSession();

  double ElapsedTime() const;
  void AddedElementTokens(size_t count);
  size_t ProcessedElementTokens() const { return processed_element_tokens_; }

 private:
  double start_time_;
  size_t processed_element_tokens_;
};

class HTMLParserScheduler final
    : public GarbageCollectedFinalized<HTMLParserScheduler> {
  WTF_MAKE_NONCOPYABLE(HTMLParserScheduler);

 public:
  static HTMLParserScheduler* Create(
      HTMLDocumentParser* parser,
      scoped_refptr<WebTaskRunner> loading_task_runner) {
    return new HTMLParserScheduler(parser, std::move(loading_task_runner));
  }
  ~HTMLParserScheduler();

  bool IsScheduledForResume() const;
  void ScheduleForResume();
  bool YieldIfNeeded(const SpeculationsPumpSession&, bool starting_script);

  /**
     * Can only be called if this scheduler is suspended. If this is called,
     * then after the scheduler is resumed by calling resume(), this call
     * ensures that HTMLDocumentParser::resumeAfterYield will be called. Used to
     * signal this scheduler that the background html parser sent chunks to
     * HTMLDocumentParser while it was suspended.
     */
  void ForceResumeAfterYield();

  void Suspend();
  void Resume();

  void Detach();  // Clear active tasks if any.

  void Trace(blink::Visitor*);

 private:
  HTMLParserScheduler(HTMLDocumentParser*, scoped_refptr<WebTaskRunner>);

  bool ShouldYield(const SpeculationsPumpSession&, bool starting_script) const;
  void ContinueParsing();

  Member<HTMLDocumentParser> parser_;
  scoped_refptr<WebTaskRunner> loading_task_runner_;

  TaskHandle cancellable_continue_parse_task_handle_;
  bool is_suspended_with_active_timer_;
};

}  // namespace blink

#endif
