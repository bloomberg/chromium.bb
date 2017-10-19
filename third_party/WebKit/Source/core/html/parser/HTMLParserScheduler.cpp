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

#include "core/html/parser/HTMLParserScheduler.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/parser/HTMLDocumentParser.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"

namespace blink {

PumpSession::PumpSession(unsigned& nesting_level)
    : NestingLevelIncrementer(nesting_level) {}

PumpSession::~PumpSession() {}

SpeculationsPumpSession::SpeculationsPumpSession(unsigned& nesting_level)
    : NestingLevelIncrementer(nesting_level),
      start_time_(CurrentTime()),
      processed_element_tokens_(0) {}

SpeculationsPumpSession::~SpeculationsPumpSession() {}

inline double SpeculationsPumpSession::ElapsedTime() const {
  return CurrentTime() - start_time_;
}

void SpeculationsPumpSession::AddedElementTokens(size_t count) {
  processed_element_tokens_ += count;
}

HTMLParserScheduler::HTMLParserScheduler(
    HTMLDocumentParser* parser,
    scoped_refptr<WebTaskRunner> loading_task_runner)
    : parser_(parser),
      loading_task_runner_(std::move(loading_task_runner)),
      is_suspended_with_active_timer_(false) {}

HTMLParserScheduler::~HTMLParserScheduler() {}

void HTMLParserScheduler::Trace(blink::Visitor* visitor) {
  visitor->Trace(parser_);
}

bool HTMLParserScheduler::IsScheduledForResume() const {
  return is_suspended_with_active_timer_ ||
         cancellable_continue_parse_task_handle_.IsActive();
}

void HTMLParserScheduler::ScheduleForResume() {
  DCHECK(!is_suspended_with_active_timer_);
  cancellable_continue_parse_task_handle_ =
      loading_task_runner_->PostCancellableTask(
          BLINK_FROM_HERE, WTF::Bind(&HTMLParserScheduler::ContinueParsing,
                                     WrapWeakPersistent(this)));
}

void HTMLParserScheduler::Suspend() {
  DCHECK(!is_suspended_with_active_timer_);
  if (!cancellable_continue_parse_task_handle_.IsActive())
    return;
  is_suspended_with_active_timer_ = true;
  cancellable_continue_parse_task_handle_.Cancel();
}

void HTMLParserScheduler::Resume() {
  DCHECK(!cancellable_continue_parse_task_handle_.IsActive());
  if (!is_suspended_with_active_timer_)
    return;
  is_suspended_with_active_timer_ = false;
  ScheduleForResume();
}

void HTMLParserScheduler::Detach() {
  cancellable_continue_parse_task_handle_.Cancel();
  is_suspended_with_active_timer_ = false;
}

inline bool HTMLParserScheduler::ShouldYield(
    const SpeculationsPumpSession& session,
    bool starting_script) const {
  if (Platform::Current()
          ->CurrentThread()
          ->Scheduler()
          ->ShouldYieldForHighPriorityWork())
    return true;

  const double kParserTimeLimit = 0.5;
  if (session.ElapsedTime() > kParserTimeLimit)
    return true;

  // Yield if a lot of DOM work has been done in this session and a script tag
  // is about to be parsed. This significantly improves render performance for
  // documents that place their scripts at the bottom of the page. Yielding too
  // often significantly slows down the parsing so a balance needs to be struck
  // to only yield when enough changes have happened to make it worthwhile.
  // Emperical testing shows that anything > ~40 and < ~200 gives all of the
  // benefit without impacting parser performance, only adding a few yields per
  // page but at just the right times.
  const size_t kSufficientWork = 50;
  if (starting_script && session.ProcessedElementTokens() > kSufficientWork)
    return true;

  return false;
}

bool HTMLParserScheduler::YieldIfNeeded(const SpeculationsPumpSession& session,
                                        bool starting_script) {
  if (ShouldYield(session, starting_script)) {
    ScheduleForResume();
    return true;
  }

  return false;
}

void HTMLParserScheduler::ForceResumeAfterYield() {
  DCHECK(!cancellable_continue_parse_task_handle_.IsActive());
  is_suspended_with_active_timer_ = true;
}

void HTMLParserScheduler::ContinueParsing() {
  parser_->ResumeParsingAfterYield();
}

}  // namespace blink
