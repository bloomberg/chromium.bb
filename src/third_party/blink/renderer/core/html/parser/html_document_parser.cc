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

#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/parser/atomic_html_token.h"
#include "third_party/blink/renderer/core/html/parser/background_html_parser.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_metrics.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_scheduler.h"
#include "third_party/blink/renderer/core/html/parser/html_resource_preloader.h"
#include "third_party/blink/renderer/core/html/parser/html_tree_builder.h"
#include "third_party/blink/renderer/core/html/parser/pump_session.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/prefetched_signed_exchange_manager.h"
#include "third_party/blink/renderer/core/loader/preload_helper.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/html_parser_script_runner.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

static size_t g_discarded_token_count_for_testing = 0;

void ResetDiscardedTokenCountForTesting() {
  g_discarded_token_count_for_testing = 0;
}

size_t GetDiscardedTokenCountForTesting() {
  return g_discarded_token_count_for_testing;
}

// This is a direct transcription of step 4 from:
// http://www.whatwg.org/specs/web-apps/current-work/multipage/the-end.html#fragment-case
static HTMLTokenizer::State TokenizerStateForContextElement(
    Element* context_element,
    bool report_errors,
    const HTMLParserOptions& options) {
  if (!context_element)
    return HTMLTokenizer::kDataState;

  const QualifiedName& context_tag = context_element->TagQName();

  if (context_tag.Matches(html_names::kTitleTag) ||
      context_tag.Matches(html_names::kTextareaTag))
    return HTMLTokenizer::kRCDATAState;
  if (context_tag.Matches(html_names::kStyleTag) ||
      context_tag.Matches(html_names::kXmpTag) ||
      context_tag.Matches(html_names::kIFrameTag) ||
      context_tag.Matches(html_names::kNoembedTag) ||
      (context_tag.Matches(html_names::kNoscriptTag) &&
       options.scripting_flag) ||
      context_tag.Matches(html_names::kNoframesTag))
    return report_errors ? HTMLTokenizer::kRAWTEXTState
                         : HTMLTokenizer::kPLAINTEXTState;
  if (context_tag.Matches(html_names::kScriptTag))
    return report_errors ? HTMLTokenizer::kScriptDataState
                         : HTMLTokenizer::kPLAINTEXTState;
  if (context_tag.Matches(html_names::kPlaintextTag))
    return HTMLTokenizer::kPLAINTEXTState;
  return HTMLTokenizer::kDataState;
}

class ScopedYieldTimer {
 public:
  // This object is created at the start of a block of parsing, and will
  // report the time since the last block yielded if known.
  ScopedYieldTimer(std::unique_ptr<base::ElapsedTimer>* timer,
                   HTMLParserMetrics* metrics_reporter)
      : timer_(timer), reporting_metrics_(metrics_reporter) {
    if (!reporting_metrics_ || !(*timer_))
      return;

    metrics_reporter->AddYieldInterval((*timer_)->Elapsed());
    timer_->reset();
  }

  // The destructor creates a new timer, which will keep track of time until
  // the next block starts.
  ~ScopedYieldTimer() {
    if (reporting_metrics_)
      *timer_ = std::make_unique<base::ElapsedTimer>();
  }

 private:
  std::unique_ptr<base::ElapsedTimer>* timer_;
  bool reporting_metrics_;
};

HTMLDocumentParser::HTMLDocumentParser(HTMLDocument& document,
                                       ParserSynchronizationPolicy sync_policy)
    : HTMLDocumentParser(document, kAllowScriptingContent, sync_policy) {
  script_runner_ =
      HTMLParserScriptRunner::Create(ReentryPermit(), &document, this);
  tree_builder_ = MakeGarbageCollected<HTMLTreeBuilder>(
      this, document, kAllowScriptingContent, options_);
}

HTMLDocumentParser::HTMLDocumentParser(
    DocumentFragment* fragment,
    Element* context_element,
    ParserContentPolicy parser_content_policy)
    : HTMLDocumentParser(fragment->GetDocument(),
                         parser_content_policy,
                         kForceSynchronousParsing) {
  // No script_runner_ in fragment parser.
  tree_builder_ = MakeGarbageCollected<HTMLTreeBuilder>(
      this, fragment, context_element, parser_content_policy, options_);

  // For now document fragment parsing never reports errors.
  bool report_errors = false;
  tokenizer_->SetState(TokenizerStateForContextElement(
      context_element, report_errors, options_));
}

HTMLDocumentParser::HTMLDocumentParser(Document& document,
                                       ParserContentPolicy content_policy,
                                       ParserSynchronizationPolicy sync_policy)
    : ScriptableDocumentParser(document, content_policy),
      options_(&document),
      reentry_permit_(HTMLParserReentryPermit::Create()),
      token_(sync_policy == kForceSynchronousParsing
                 ? std::make_unique<HTMLToken>()
                 : nullptr),
      tokenizer_(sync_policy == kForceSynchronousParsing
                     ? std::make_unique<HTMLTokenizer>(options_)
                     : nullptr),
      loading_task_runner_(document.GetTaskRunner(TaskType::kNetworking)),
      parser_scheduler_(sync_policy == kAllowAsynchronousParsing
                            ? MakeGarbageCollected<HTMLParserScheduler>(
                                  this,
                                  loading_task_runner_.get())
                            : nullptr),
      pending_csp_meta_token_(nullptr),
      should_use_threading_(sync_policy == kAllowAsynchronousParsing),
      end_was_delayed_(false),
      have_background_parser_(false),
      pump_session_nesting_level_(0),
      pump_speculations_session_nesting_level_(0),
      is_parsing_at_line_number_(false),
      tried_loading_link_headers_(false),
      added_pending_parser_blocking_stylesheet_(false),
      is_waiting_for_stylesheets_(false) {
  DCHECK(ShouldUseThreading() || (token_ && tokenizer_));
  // Threading is not allowed in prefetch mode.
  DCHECK(!document.IsPrefetchOnly() || !ShouldUseThreading());

  // Report metrics for async document parsing only. The document
  // must be main frame to meet UKM requirements, and must have a high
  // resolution clock for high quality data.
  if (sync_policy == kAllowAsynchronousParsing && document.GetFrame() &&
      document.GetFrame()->IsMainFrame() &&
      base::TimeTicks::IsHighResolution()) {
    metrics_reporter_ = std::make_unique<HTMLParserMetrics>(
        document.UkmSourceID(), document.UkmRecorder());
  }

  // Don't create preloader for parsing clipboard content.
  if (content_policy == kDisallowScriptingAndPluginContent)
    return;

  // Create preloader only when the document is:
  // - attached to a frame (likely the prefetched resources will be loaded
  // soon),
  // - a HTML import document (blocks rendering and also resources will be
  // loaded soon), or
  // - is for no-state prefetch (made specifically for running preloader).
  if (!document.GetFrame() && !document.IsHTMLImport() &&
      !document.IsPrefetchOnly())
    return;

  preloader_ = MakeGarbageCollected<HTMLResourcePreloader>(document);
}

HTMLDocumentParser::~HTMLDocumentParser() = default;

void HTMLDocumentParser::Dispose() {
  // In Oilpan, HTMLDocumentParser can die together with Document, and detach()
  // is not called in this case.
  if (have_background_parser_)
    StopBackgroundParser();
}

void HTMLDocumentParser::Trace(Visitor* visitor) {
  visitor->Trace(tree_builder_);
  visitor->Trace(parser_scheduler_);
  visitor->Trace(script_runner_);
  visitor->Trace(preloader_);
  ScriptableDocumentParser::Trace(visitor);
  HTMLParserScriptRunnerHost::Trace(visitor);
}

void HTMLDocumentParser::Detach() {
  if (have_background_parser_)
    StopBackgroundParser();
  DocumentParser::Detach();
  if (script_runner_)
    script_runner_->Detach();
  tree_builder_->Detach();
  // FIXME: It seems wrong that we would have a preload scanner here. Yet during
  // fast/dom/HTMLScriptElement/script-load-events.html we do.
  preload_scanner_.reset();
  insertion_preload_scanner_.reset();
  if (parser_scheduler_) {
    parser_scheduler_->Detach();
    parser_scheduler_.Clear();
  }
  // Oilpan: It is important to clear token_ to deallocate backing memory of
  // HTMLToken::data_ and let the allocator reuse the memory for
  // HTMLToken::data_ of a next HTMLDocumentParser. We need to clear
  // tokenizer_ first because tokenizer_ has a raw pointer to token_.
  tokenizer_.reset();
  token_.reset();
}

void HTMLDocumentParser::StopParsing() {
  DocumentParser::StopParsing();
  if (parser_scheduler_) {
    parser_scheduler_->Detach();
    parser_scheduler_.Clear();
  }
  if (have_background_parser_)
    StopBackgroundParser();
}

// This kicks off "Once the user agent stops parsing" as described by:
// http://www.whatwg.org/specs/web-apps/current-work/multipage/the-end.html#the-end
void HTMLDocumentParser::PrepareToStopParsing() {
  // FIXME: It may not be correct to disable this for the background parser.
  // That means hasInsertionPoint() may not be correct in some cases.
  DCHECK(!HasInsertionPoint() || have_background_parser_);

  // NOTE: This pump should only ever emit buffered character tokens.
  if (tokenizer_ && !GetDocument()->IsPrefetchOnly()) {
    DCHECK(!have_background_parser_);
    PumpTokenizerIfPossible();
  }

  if (IsStopped())
    return;

  DocumentParser::PrepareToStopParsing();

  // We will not have a scriptRunner when parsing a DocumentFragment.
  if (script_runner_)
    GetDocument()->SetReadyState(Document::kInteractive);

  // Setting the ready state above can fire mutation event and detach us from
  // underneath. In that case, just bail out.
  if (IsDetached())
    return;

  if (script_runner_)
    script_runner_->RecordMetricsAtParseEnd();

  AttemptToRunDeferredScriptsAndEnd();
}

bool HTMLDocumentParser::IsParsingFragment() const {
  return tree_builder_->IsParsingFragment();
}

void HTMLDocumentParser::PumpTokenizerIfPossible() {
  CheckIfBlockingStylesheetAdded();
  if (IsStopped() || IsPaused())
    return;

  PumpTokenizer();
}

bool HTMLDocumentParser::IsScheduledForUnpause() const {
  return parser_scheduler_ && parser_scheduler_->IsScheduledForUnpause();
}

// Used by HTMLParserScheduler
void HTMLDocumentParser::ResumeParsingAfterYield() {
  DCHECK(ShouldUseThreading());
  DCHECK(have_background_parser_);

  ScopedYieldTimer(&yield_timer_, metrics_reporter_.get());

  CheckIfBlockingStylesheetAdded();
  if (IsStopped() || IsPaused())
    return;

  PumpPendingSpeculations();
}

void HTMLDocumentParser::RunScriptsForPausedTreeBuilder() {
  DCHECK(ScriptingContentIsAllowed(GetParserContentPolicy()));

  TextPosition script_start_position = TextPosition::BelowRangePosition();
  Element* script_element =
      tree_builder_->TakeScriptToProcess(script_start_position);
  // We will not have a scriptRunner when parsing a DocumentFragment.
  if (script_runner_)
    script_runner_->ProcessScriptElement(script_element, script_start_position);
  CheckIfBlockingStylesheetAdded();
}

bool HTMLDocumentParser::CanTakeNextToken() {
  if (IsStopped())
    return false;

  // If we're paused waiting for a script, we try to execute scripts before
  // continuing.
  if (tree_builder_->HasParserBlockingScript())
    RunScriptsForPausedTreeBuilder();
  if (IsStopped() || IsPaused())
    return false;
  return true;
}

void HTMLDocumentParser::EnqueueTokenizedChunk(
    std::unique_ptr<TokenizedChunk> chunk) {
  TRACE_EVENT0("blink", "HTMLDocumentParser::EnqueueTokenizedChunk");

  DCHECK(chunk);
  DCHECK(GetDocument());

  if (!IsParsing())
    return;

  // ApplicationCache needs to be initialized before issuing preloads. We
  // suspend preload until HTMLHTMLElement is inserted and ApplicationCache is
  // initialized. Note: link rel preloads don't follow this policy per the spec.
  // These directives should initiate a fetch as fast as possible.
  if (!tried_loading_link_headers_ && GetDocument()->Loader()) {
    // Note that on commit, the loader dispatched preloads for all the non-media
    // links.
    GetDocument()->Loader()->DispatchLinkHeaderPreloads(
        base::OptionalOrNullptr(chunk->viewport),
        PreloadHelper::kOnlyLoadMedia);
    tried_loading_link_headers_ = true;
    if (GetDocument()->Loader()->GetPrefetchedSignedExchangeManager()) {
      // Link header preloads for prefetched signed exchanges won't be started
      // until StartPrefetchedLinkHeaderPreloads() is called. See the header
      // comment of PrefetchedSignedExchangeManager.
      GetDocument()
          ->Loader()
          ->GetPrefetchedSignedExchangeManager()
          ->StartPrefetchedLinkHeaderPreloads();
    }
  }

  // Defer preloads if any of the chunks contains a <meta> csp tag.
  if (chunk->pending_csp_meta_token_index != TokenizedChunk::kNoPendingToken) {
    pending_csp_meta_token_ =
        &chunk->tokens.at(chunk->pending_csp_meta_token_index);
  }

  if (preloader_) {
    bool appcache_fetched = false;
    if (GetDocument()->Loader()) {
      appcache_fetched = (GetDocument()->Loader()->GetResponse().AppCacheID() !=
                          mojom::blink::kAppCacheNoCacheId);
    }
    bool appcache_initialized = GetDocument()->documentElement();
    // Delay sending some requests if meta tag based CSP is present or
    // if AppCache was used to fetch the HTML but was not yet initialized for
    // this document.
    if (pending_csp_meta_token_ ||
        ((!base::FeatureList::IsEnabled(
              blink::features::kVerifyHTMLFetchedFromAppCacheBeforeDelay) ||
          appcache_fetched) &&
         !appcache_initialized)) {
      PreloadRequestStream link_rel_preloads;
      for (auto& request : chunk->preloads) {
        // Link rel preloads don't need to wait for AppCache but they
        // should probably wait for CSP.
        if (!pending_csp_meta_token_ && request->IsLinkRelPreload())
          link_rel_preloads.push_back(std::move(request));
        else
          queued_preloads_.push_back(std::move(request));
      }
      preloader_->TakeAndPreload(link_rel_preloads);
    } else {
      // We can safely assume that there are no queued preloads request after
      // the document element is available, as we empty the queue immediately
      // after the document element is created in documentElementAvailable().
      DCHECK(queued_preloads_.IsEmpty());
      preloader_->TakeAndPreload(chunk->preloads);
    }
  }

  speculations_.push_back(std::move(chunk));

  if (!IsPaused() && !IsScheduledForUnpause())
    parser_scheduler_->ScheduleForUnpause();
}

void HTMLDocumentParser::DidReceiveEncodingDataFromBackgroundParser(
    const DocumentEncodingData& data) {
  GetDocument()->SetEncodingData(data);
}

void HTMLDocumentParser::ValidateSpeculations(
    std::unique_ptr<TokenizedChunk> chunk) {
  DCHECK(chunk);
  // TODO(kouhei): We should simplify codepath here by disallowing
  // ValidateSpeculations
  // while IsPaused, and last_chunk_before_pause_ can simply be
  // pushed to speculations_.
  if (IsPaused()) {
    // We're waiting on a network script or stylesheet, just save the chunk,
    // we'll get a second ValidateSpeculations call after the script or
    // stylesheet completes. This call should have been made immediately after
    // RunScriptsForPausedTreeBuilder in the script case which may have started
    // a network load and left us waiting.
    DCHECK(!last_chunk_before_pause_);
    last_chunk_before_pause_ = std::move(chunk);
    return;
  }

  DCHECK(!last_chunk_before_pause_);
  std::unique_ptr<HTMLTokenizer> tokenizer = std::move(tokenizer_);
  std::unique_ptr<HTMLToken> token = std::move(token_);

  if (!tokenizer) {
    // There must not have been any changes to the HTMLTokenizer state on the
    // main thread, which means the speculation buffer is correct.
    return;
  }

  // Currently we're only smart enough to reuse the speculation buffer if the
  // tokenizer both starts and ends in the DataState. That state is simplest
  // because the HTMLToken is always in the Uninitialized state. We should
  // consider whether we can reuse the speculation buffer in other states, but
  // we'd likely need to do something more sophisticated with the HTMLToken.
  if (chunk->tokenizer_state == HTMLTokenizer::kDataState &&
      tokenizer->GetState() == HTMLTokenizer::kDataState &&
      input_.Current().IsEmpty() &&
      chunk->tree_builder_state ==
          HTMLTreeBuilderSimulator::StateFor(tree_builder_.Get())) {
    DCHECK(token->IsUninitialized());
    return;
  }

  DiscardSpeculationsAndResumeFrom(std::move(chunk), std::move(token),
                                   std::move(tokenizer));
}

void HTMLDocumentParser::DiscardSpeculationsAndResumeFrom(
    std::unique_ptr<TokenizedChunk> last_chunk_before_script,
    std::unique_ptr<HTMLToken> token,
    std::unique_ptr<HTMLTokenizer> tokenizer) {
  // Clear back ref.
  background_parser_->ClearParser();

  size_t discarded_token_count = 0;
  for (const auto& speculation : speculations_) {
    discarded_token_count += speculation->tokens.size();
  }
  g_discarded_token_count_for_testing += discarded_token_count;

  speculations_.clear();
  pending_csp_meta_token_ = nullptr;
  queued_preloads_.clear();

  std::unique_ptr<BackgroundHTMLParser::Checkpoint> checkpoint =
      std::make_unique<BackgroundHTMLParser::Checkpoint>();
  checkpoint->parser = this;
  checkpoint->token = std::move(token);
  checkpoint->tokenizer = std::move(tokenizer);
  checkpoint->tree_builder_state =
      HTMLTreeBuilderSimulator::StateFor(tree_builder_.Get());
  checkpoint->input_checkpoint = last_chunk_before_script->input_checkpoint;
  checkpoint->preload_scanner_checkpoint =
      last_chunk_before_script->preload_scanner_checkpoint;
  checkpoint->unparsed_input = input_.Current().ToString().IsolatedCopy();
  // FIXME: This should be passed in instead of cleared.
  input_.Current().Clear();

  DCHECK(checkpoint->unparsed_input.IsSafeToSendToAnotherThread());
  loading_task_runner_->PostTask(
      FROM_HERE,
      WTF::Bind(&BackgroundHTMLParser::ResumeFrom, background_parser_,
                WTF::Passed(std::move(checkpoint))));
}

size_t HTMLDocumentParser::ProcessTokenizedChunkFromBackgroundParser(
    std::unique_ptr<TokenizedChunk> pop_chunk,
    bool* reached_end_of_file) {
  TRACE_EVENT_WITH_FLOW0(
      "blink,loading",
      "HTMLDocumentParser::processTokenizedChunkFromBackgroundParser",
      pop_chunk.get(), TRACE_EVENT_FLAG_FLOW_IN);
  base::AutoReset<bool> has_line_number(&is_parsing_at_line_number_, true);

  SECURITY_DCHECK(pump_speculations_session_nesting_level_ == 1);
  SECURITY_DCHECK(!InPumpSession());
  DCHECK(!IsParsingFragment());
  DCHECK(!IsPaused());
  DCHECK(!IsStopped());
  DCHECK(ShouldUseThreading());
  DCHECK(!tokenizer_);
  DCHECK(!token_);
  DCHECK(!last_chunk_before_pause_);

  std::unique_ptr<TokenizedChunk> chunk(std::move(pop_chunk));
  const CompactHTMLTokenStream& tokens = chunk->tokens;
  size_t element_token_count = 0;

  loading_task_runner_->PostTask(
      FROM_HERE, WTF::Bind(&BackgroundHTMLParser::StartedChunkWithCheckpoint,
                           background_parser_, chunk->input_checkpoint));

  for (const auto& token : tokens) {
    DCHECK(!IsWaitingForScripts());

    if (!chunk->starting_script && (token.GetType() == HTMLToken::kStartTag ||
                                    token.GetType() == HTMLToken::kEndTag))
      element_token_count++;

    text_position_ = token.GetTextPosition();

    ConstructTreeFromCompactHTMLToken(token);

    if (IsStopped())
      break;

    // Preloads were queued if there was a <meta> csp token in a tokenized
    // chunk.
    if (pending_csp_meta_token_ && &token == pending_csp_meta_token_) {
      pending_csp_meta_token_ = nullptr;
      FetchQueuedPreloads();
    }

    if (IsPaused()) {
      // The script or stylesheet should be the last token of this bunch.
      DCHECK_EQ(&token, &tokens.back());
      if (IsWaitingForScripts())
        RunScriptsForPausedTreeBuilder();
      ValidateSpeculations(std::move(chunk));
      break;
    }

    if (token.GetType() == HTMLToken::kEndOfFile) {
      // The EOF is assumed to be the last token of this bunch.
      DCHECK_EQ(&token, &tokens.back());
      // There should never be any chunks after the EOF.
      DCHECK(speculations_.IsEmpty());
      PrepareToStopParsing();
      *reached_end_of_file = true;
      break;
    }

    DCHECK(!tokenizer_);
    DCHECK(!token_);
  }

  // Make sure all required pending text nodes are emitted before returning.
  // This leaves "script", "style" and "svg" nodes text nodes intact.
  if (!IsStopped())
    tree_builder_->Flush(kFlushIfAtTextLimit);

  is_parsing_at_line_number_ = false;

  return element_token_count;
}

void HTMLDocumentParser::PumpPendingSpeculations() {
  // If this assert fails, you need to call ValidateSpeculations to make sure
  // tokenizer_ and token_ don't have state that invalidates speculations_.
  DCHECK(!tokenizer_);
  DCHECK(!token_);
  DCHECK(!last_chunk_before_pause_);
  DCHECK(!IsPaused());
  DCHECK(!IsStopped());
  DCHECK(!IsScheduledForUnpause());
  DCHECK(!InPumpSession());

  // FIXME: Here should never be reached when there is a blocking script,
  // but it happens in unknown scenarios. See https://crbug.com/440901
  if (IsWaitingForScripts()) {
    parser_scheduler_->ScheduleForUnpause();
    return;
  }

  // Do not allow pumping speculations in nested event loops.
  if (pump_speculations_session_nesting_level_) {
    parser_scheduler_->ScheduleForUnpause();
    return;
  }

  probe::ParseHTML probe(GetDocument(), this);

  SpeculationsPumpSession session(pump_speculations_session_nesting_level_);
  bool reached_end_of_file = false;
  while (!speculations_.IsEmpty()) {
    DCHECK(!IsScheduledForUnpause());
    size_t element_token_count = ProcessTokenizedChunkFromBackgroundParser(
        speculations_.TakeFirst(), &reached_end_of_file);
    session.AddedElementTokens(element_token_count);

    // Always check IsParsing first as document_ may be null. Surprisingly,
    // IsScheduledForUnpause() may be set here as a result of
    // ProcessTokenizedChunkFromBackgroundParser running arbitrary javascript
    // which invokes nested event loops. (e.g. inspector breakpoints)
    CheckIfBlockingStylesheetAdded();
    if (!IsParsing() || IsPaused() || IsScheduledForUnpause())
      break;

    if (speculations_.IsEmpty() ||
        parser_scheduler_->YieldIfNeeded(
            session, speculations_.front()->starting_script))
      break;
  }

  if (metrics_reporter_) {
    metrics_reporter_->AddChunk(session.ElapsedTime(),
                                session.ProcessedElementTokens());
    if (reached_end_of_file)
      metrics_reporter_->ReportMetricsAtParseEnd();
  }
}

void HTMLDocumentParser::ForcePlaintextForTextDocument() {
  if (ShouldUseThreading()) {
    // This method is called before any data is appended, so we have to start
    // the background parser ourselves.
    if (!have_background_parser_)
      StartBackgroundParser();

    // This task should be synchronous, because otherwise synchronous
    // tokenizing can happen before plaintext is forced.
    background_parser_->ForcePlaintextForTextDocument();
  } else
    tokenizer_->SetState(HTMLTokenizer::kPLAINTEXTState);
}

void HTMLDocumentParser::PumpTokenizer() {
  DCHECK(!GetDocument()->IsPrefetchOnly());
  DCHECK(!IsStopped());
  DCHECK(tokenizer_);
  DCHECK(token_);

  PumpSession session(pump_session_nesting_level_);

  // We tell the InspectorInstrumentation about every pump, even if we end up
  // pumping nothing.  It can filter out empty pumps itself.
  // FIXME: input_.Current().length() is only accurate if we end up parsing the
  // whole buffer in this pump.  We should pass how much we parsed as part of
  // DidWriteHTML instead of WillWriteHTML.
  probe::ParseHTML probe(GetDocument(), this);

  while (CanTakeNextToken()) {
    {
      RUNTIME_CALL_TIMER_SCOPE(
          V8PerIsolateData::MainThreadIsolate(),
          RuntimeCallStats::CounterId::kHTMLTokenizerNextToken);
      if (!tokenizer_->NextToken(input_.Current(), Token()))
        break;
    }
    ConstructTreeFromHTMLToken();
    DCHECK(IsStopped() || Token().IsUninitialized());
  }

  if (IsStopped())
    return;

  // There should only be PendingText left since the tree-builder always flushes
  // the task queue before returning. In case that ever changes, crash.
  tree_builder_->Flush(kFlushAlways);
  CHECK(!IsStopped());

  if (IsPaused()) {
    DCHECK_EQ(tokenizer_->GetState(), HTMLTokenizer::kDataState);

    if (preloader_) {
      if (!preload_scanner_) {
        preload_scanner_ = CreatePreloadScanner(
            TokenPreloadScanner::ScannerType::kMainDocument);
        preload_scanner_->AppendToEnd(input_.Current());
      }
      ScanAndPreload(preload_scanner_.get());
    }
  }
}

void HTMLDocumentParser::ConstructTreeFromHTMLToken() {
  DCHECK(!GetDocument()->IsPrefetchOnly());
  AtomicHTMLToken atomic_token(Token());

  // We clear the token_ in case ConstructTreeFromAtomicToken
  // synchronously re-enters the parser. We don't clear the token immedately
  // for kCharacter tokens because the AtomicHTMLToken avoids copying the
  // characters by keeping a pointer to the underlying buffer in the
  // HTMLToken. Fortunately, kCharacter tokens can't cause us to re-enter
  // the parser.
  //
  // FIXME: Stop clearing the token_ once we start running the parser off
  // the main thread or once we stop allowing synchronous JavaScript
  // execution from ParseAttribute.
  if (Token().GetType() != HTMLToken::kCharacter)
    Token().Clear();

  tree_builder_->ConstructTree(&atomic_token);
  CheckIfBlockingStylesheetAdded();

  // FIXME: ConstructTree may synchronously cause Document to be detached.
  if (!token_)
    return;

  if (!Token().IsUninitialized()) {
    DCHECK_EQ(Token().GetType(), HTMLToken::kCharacter);
    Token().Clear();
  }
}

void HTMLDocumentParser::ConstructTreeFromCompactHTMLToken(
    const CompactHTMLToken& compact_token) {
  DCHECK(!GetDocument()->IsPrefetchOnly());
  AtomicHTMLToken token(compact_token);
  tree_builder_->ConstructTree(&token);
  CheckIfBlockingStylesheetAdded();
}

bool HTMLDocumentParser::HasInsertionPoint() {
  // FIXME: The wasCreatedByScript() branch here might not be fully correct. Our
  // model of the EOF character differs slightly from the one in the spec
  // because our treatment is uniform between network-sourced and script-sourced
  // input streams whereas the spec treats them differently.
  return input_.HasInsertionPoint() ||
         (WasCreatedByScript() && !input_.HaveSeenEndOfFile());
}

void HTMLDocumentParser::insert(const String& source) {
  if (IsStopped())
    return;

  TRACE_EVENT1("blink", "HTMLDocumentParser::insert", "source_length",
               source.length());

  if (!tokenizer_) {
    DCHECK(!InPumpSession());
    DCHECK(have_background_parser_ || WasCreatedByScript());
    token_ = std::make_unique<HTMLToken>();
    tokenizer_ = std::make_unique<HTMLTokenizer>(options_);
  }

  SegmentedString excluded_line_number_source(source);
  excluded_line_number_source.SetExcludeLineNumbers();
  input_.InsertAtCurrentInsertionPoint(excluded_line_number_source);
  PumpTokenizerIfPossible();

  if (IsPaused()) {
    // Check the document.write() output with a separate preload scanner as
    // the main scanner can't deal with insertions.
    if (!insertion_preload_scanner_) {
      insertion_preload_scanner_ =
          CreatePreloadScanner(TokenPreloadScanner::ScannerType::kInsertion);
    }
    insertion_preload_scanner_->AppendToEnd(source);
    ScanAndPreload(insertion_preload_scanner_.get());
  }

  EndIfDelayed();
}

void HTMLDocumentParser::StartBackgroundParser() {
  TRACE_EVENT0("blink,loading", "HTMLDocumentParser::StartBackgroundParser");
  DCHECK(!IsStopped());
  DCHECK(ShouldUseThreading());
  DCHECK(!have_background_parser_);
  DCHECK(GetDocument());
  have_background_parser_ = true;

  // Make sure that a resolver is set up, so that the correct viewport
  // dimensions will be fed to the background parser and preload scanner.
  if (GetDocument()->Loader())
    GetDocument()->EnsureStyleResolver();

  std::unique_ptr<BackgroundHTMLParser::Configuration> config =
      std::make_unique<BackgroundHTMLParser::Configuration>();
  config->options = options_;
  config->parser = this;
  config->decoder = TakeDecoder();

  // The background parser is created on the main thread, but may otherwise
  // only be used from the parser thread.
  background_parser_ =
      BackgroundHTMLParser::Create(std::move(config), loading_task_runner_);
  // TODO(csharrison): This is a hack to initialize MediaValuesCached on the
  // correct thread. We should get rid of it.

  // TODO(domfarolino): Remove this once Priority Hints is no longer in Origin
  // Trial. This currently exists because the TokenPreloadScanner needs to know
  // the status of the Priority Hints Origin Trial, and has no way of figuring
  // this out on its own. See https://crbug.com/821464.
  bool priority_hints_origin_trial_enabled =
      RuntimeEnabledFeatures::PriorityHintsEnabled(GetDocument());

  background_parser_->Init(
      GetDocument()->Url(),
      std::make_unique<CachedDocumentParameters>(GetDocument()),
      MediaValuesCached::MediaValuesCachedData(*GetDocument()),
      priority_hints_origin_trial_enabled);
}

void HTMLDocumentParser::StopBackgroundParser() {
  DCHECK(ShouldUseThreading());
  DCHECK(have_background_parser_);

  have_background_parser_ = false;

  // Make this sync, as lsan triggers on some unittests if the task runner is
  // used.
  background_parser_->Stop();
}

void HTMLDocumentParser::Append(const String& input_source) {
  if (IsStopped())
    return;

  // We should never reach this point if we're using a parser thread, as
  // appendBytes() will directly ship the data to the thread.
  DCHECK(!ShouldUseThreading());

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("blink.debug"),
               "HTMLDocumentParser::append", "size", input_source.length());
  const SegmentedString source(input_source);

  if (GetDocument()->IsPrefetchOnly()) {
    // Do not prefetch if there is an appcache.
    if (GetDocument()->Loader()->GetResponse().AppCacheID() != 0)
      return;

    if (!preload_scanner_) {
      preload_scanner_ =
          CreatePreloadScanner(TokenPreloadScanner::ScannerType::kMainDocument);
    }

    preload_scanner_->AppendToEnd(source);
    ScanAndPreload(preload_scanner_.get());

    // Return after the preload scanner, do not actually parse the document.
    return;
  }

  if (preload_scanner_) {
    if (input_.Current().IsEmpty() && !IsPaused()) {
      // We have parsed until the end of the current input and so are now moving
      // ahead of the preload scanner. Clear the scanner so we know to scan
      // starting from the current input point if we block again.
      preload_scanner_.reset();
    } else {
      preload_scanner_->AppendToEnd(source);
      if (IsPaused())
        ScanAndPreload(preload_scanner_.get());
    }
  }

  input_.AppendToEnd(source);

  if (InPumpSession()) {
    // We've gotten data off the network in a nested write. We don't want to
    // consume any more of the input stream now.  Do not worry.  We'll consume
    // this data in a less-nested write().
    return;
  }

  PumpTokenizerIfPossible();

  EndIfDelayed();
}

void HTMLDocumentParser::end() {
  DCHECK(!IsDetached());
  DCHECK(!IsScheduledForUnpause());

  if (have_background_parser_)
    StopBackgroundParser();

  // Informs the the rest of WebCore that parsing is really finished (and
  // deletes this).
  tree_builder_->Finished();

  // All preloads should be done.
  preloader_ = nullptr;

  DocumentParser::StopParsing();
}

void HTMLDocumentParser::AttemptToRunDeferredScriptsAndEnd() {
  DCHECK(IsStopping());
  // FIXME: It may not be correct to disable this for the background parser.
  // That means hasInsertionPoint() may not be correct in some cases.
  DCHECK(!HasInsertionPoint() || have_background_parser_);
  if (script_runner_ && !script_runner_->ExecuteScriptsWaitingForParsing())
    return;
  end();
}

void HTMLDocumentParser::AttemptToEnd() {
  // finish() indicates we will not receive any more data. If we are waiting on
  // an external script to load, we can't finish parsing quite yet.

  if (ShouldDelayEnd()) {
    end_was_delayed_ = true;
    return;
  }
  PrepareToStopParsing();
}

void HTMLDocumentParser::EndIfDelayed() {
  // If we've already been detached, don't bother ending.
  if (IsDetached())
    return;

  if (!end_was_delayed_ || ShouldDelayEnd())
    return;

  end_was_delayed_ = false;
  PrepareToStopParsing();
}

void HTMLDocumentParser::Finish() {
  // FIXME: We should DCHECK(!parser_stopped_) here, since it does not makes
  // sense to call any methods on DocumentParser once it's been stopped.
  // However, FrameLoader::Stop calls DocumentParser::Finish unconditionally.

  Flush();
  if (IsDetached())
    return;

  // Empty documents never got an append() call, and thus have never started a
  // background parser. In those cases, we ignore shouldUseThreading() and fall
  // through to the non-threading case.
  if (have_background_parser_) {
    if (!input_.HaveSeenEndOfFile())
      input_.CloseWithoutMarkingEndOfFile();
    loading_task_runner_->PostTask(
        FROM_HERE,
        WTF::Bind(&BackgroundHTMLParser::Finish, background_parser_));
    return;
  }

  if (!tokenizer_) {
    DCHECK(!token_);
    // We're finishing before receiving any data. Rather than booting up the
    // background parser just to spin it down, we finish parsing synchronously.
    token_ = std::make_unique<HTMLToken>();
    tokenizer_ = std::make_unique<HTMLTokenizer>(options_);
  }

  // We're not going to get any more data off the network, so we tell the input
  // stream we've reached the end of file. finish() can be called more than
  // once, if the first time does not call end().
  if (!input_.HaveSeenEndOfFile())
    input_.MarkEndOfFile();

  AttemptToEnd();
}

bool HTMLDocumentParser::IsExecutingScript() const {
  if (!script_runner_)
    return false;
  return script_runner_->IsExecutingScript();
}

bool HTMLDocumentParser::IsParsingAtLineNumber() const {
  return is_parsing_at_line_number_ &&
         ScriptableDocumentParser::IsParsingAtLineNumber();
}

OrdinalNumber HTMLDocumentParser::LineNumber() const {
  if (have_background_parser_)
    return text_position_.line_;

  return input_.Current().CurrentLine();
}

TextPosition HTMLDocumentParser::GetTextPosition() const {
  if (have_background_parser_)
    return text_position_;

  const SegmentedString& current_string = input_.Current();
  OrdinalNumber line = current_string.CurrentLine();
  OrdinalNumber column = current_string.CurrentColumn();

  return TextPosition(line, column);
}

bool HTMLDocumentParser::IsWaitingForScripts() const {
  // When the TreeBuilder encounters a </script> tag, it returns to the
  // HTMLDocumentParser where the script is transfered from the treebuilder to
  // the script runner. The script runner will hold the script until its loaded
  // and run. During any of this time, we want to count ourselves as "waiting
  // for a script" and thus run the preload scanner, as well as delay completion
  // of parsing.
  bool tree_builder_has_blocking_script =
      tree_builder_->HasParserBlockingScript();
  bool script_runner_has_blocking_script =
      script_runner_ && script_runner_->HasParserBlockingScript();
  // Since the parser is paused while a script runner has a blocking script, it
  // should never be possible to end up with both objects holding a blocking
  // script.
  DCHECK(
      !(tree_builder_has_blocking_script && script_runner_has_blocking_script));
  // If either object has a blocking script, the parser should be paused.
  return tree_builder_has_blocking_script ||
         script_runner_has_blocking_script ||
         reentry_permit_->ParserPauseFlag();
}

void HTMLDocumentParser::ResumeParsingAfterPause() {
  DCHECK(!IsExecutingScript());
  DCHECK(!IsPaused());

  CheckIfBlockingStylesheetAdded();
  if (IsStopped() || IsPaused())
    return;

  if (have_background_parser_) {
    // If we paused in the middle of processing a token chunk,
    // deal with that before starting to pump.
    if (last_chunk_before_pause_) {
      ValidateSpeculations(std::move(last_chunk_before_pause_));
      DCHECK(!last_chunk_before_pause_);
      PumpPendingSpeculations();
    } else if (!IsScheduledForUnpause()) {
      // Otherwise, start pumping if we're not already scheduled to unpause
      // already.
      PumpPendingSpeculations();
    }
    return;
  }

  insertion_preload_scanner_.reset();
  if (tokenizer_) {
    PumpTokenizerIfPossible();
  }
  EndIfDelayed();
}

void HTMLDocumentParser::AppendCurrentInputStreamToPreloadScannerAndScan() {
  DCHECK(preload_scanner_);
  preload_scanner_->AppendToEnd(input_.Current());
  ScanAndPreload(preload_scanner_.get());
}

void HTMLDocumentParser::NotifyScriptLoaded(PendingScript* pending_script) {
  DCHECK(script_runner_);
  DCHECK(!IsExecutingScript());

  scheduler::CooperativeSchedulingManager::AllowedStackScope
      whitelisted_stack_scope(
          scheduler::CooperativeSchedulingManager::Instance());

  if (IsStopped()) {
    return;
  }

  if (IsStopping()) {
    AttemptToRunDeferredScriptsAndEnd();
    return;
  }

  script_runner_->ExecuteScriptsWaitingForLoad(pending_script);
  if (!IsPaused())
    ResumeParsingAfterPause();
}

void HTMLDocumentParser::ExecuteScriptsWaitingForResources() {
  if (IsStopped())
    return;

  DCHECK(GetDocument()->IsScriptExecutionReady());

  if (is_waiting_for_stylesheets_)
    is_waiting_for_stylesheets_ = false;

  // Document only calls this when the Document owns the DocumentParser so this
  // will not be called in the DocumentFragment case.
  DCHECK(script_runner_);
  script_runner_->ExecuteScriptsWaitingForResources();
  if (!IsPaused())
    ResumeParsingAfterPause();
}

void HTMLDocumentParser::DidAddPendingParserBlockingStylesheet() {
  // In-body CSS doesn't block painting. The parser needs to pause so that
  // the DOM doesn't include any elements that may depend on the CSS for style.
  // The stylesheet can be added and removed during the parsing of a single
  // token so don't actually set the bit to block parsing here, just track
  // the state of the added sheet in case it does persist beyond a single
  // token.
  added_pending_parser_blocking_stylesheet_ = true;
}

void HTMLDocumentParser::DidLoadAllPendingParserBlockingStylesheets() {
  // Just toggle the stylesheet flag here (mostly for synchronous sheets).
  // The document will also call into executeScriptsWaitingForResources
  // which is when the parser will re-start, otherwise it will attempt to
  // resume twice which could cause state machine issues.
  added_pending_parser_blocking_stylesheet_ = false;
}

void HTMLDocumentParser::CheckIfBlockingStylesheetAdded() {
  if (added_pending_parser_blocking_stylesheet_) {
    added_pending_parser_blocking_stylesheet_ = false;
    is_waiting_for_stylesheets_ = true;
  }
}

void HTMLDocumentParser::ParseDocumentFragment(
    const String& source,
    DocumentFragment* fragment,
    Element* context_element,
    ParserContentPolicy parser_content_policy) {
  auto* parser = MakeGarbageCollected<HTMLDocumentParser>(
      fragment, context_element, parser_content_policy);
  parser->Append(source);
  parser->Finish();
  // Allows ~DocumentParser to assert it was detached before destruction.
  parser->Detach();
}

void HTMLDocumentParser::AppendBytes(const char* data, size_t length) {
  if (!length || IsStopped())
    return;

  if (ShouldUseThreading()) {
    if (!have_background_parser_)
      StartBackgroundParser();

    std::unique_ptr<Vector<char>> buffer =
        std::make_unique<Vector<char>>(length);
    memcpy(buffer->data(), data, length);
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("blink.debug"),
                 "HTMLDocumentParser::appendBytes", "size", (unsigned)length);

    loading_task_runner_->PostTask(
        FROM_HERE,
        WTF::Bind(&BackgroundHTMLParser::AppendRawBytesFromMainThread,
                  background_parser_, WTF::Passed(std::move(buffer))));
    return;
  }

  DecodedDataDocumentParser::AppendBytes(data, length);
}

void HTMLDocumentParser::Flush() {
  // If we've got no decoder, we never received any data.
  if (IsDetached() || NeedsDecoder())
    return;

  if (ShouldUseThreading()) {
    // In some cases, flush() is called without any invocation of appendBytes.
    // Fallback to synchronous parsing in that case.
    if (!have_background_parser_) {
      should_use_threading_ = false;
      token_ = std::make_unique<HTMLToken>();
      tokenizer_ = std::make_unique<HTMLTokenizer>(options_);
      DecodedDataDocumentParser::Flush();
      return;
    }

    loading_task_runner_->PostTask(
        FROM_HERE, WTF::Bind(&BackgroundHTMLParser::Flush, background_parser_));
  } else {
    DecodedDataDocumentParser::Flush();
  }
}

void HTMLDocumentParser::SetDecoder(
    std::unique_ptr<TextResourceDecoder> decoder) {
  DCHECK(decoder);
  DecodedDataDocumentParser::SetDecoder(std::move(decoder));

  if (have_background_parser_) {
    loading_task_runner_->PostTask(
        FROM_HERE, WTF::Bind(&BackgroundHTMLParser::SetDecoder,
                             background_parser_, WTF::Passed(TakeDecoder())));
  }
}

void HTMLDocumentParser::DocumentElementAvailable() {
  TRACE_EVENT0("blink,loading", "HTMLDocumentParser::DocumentElementAvailable");
  Document* document = GetDocument();
  DCHECK(document);
  DCHECK(document->documentElement());
  Element* documentElement = GetDocument()->documentElement();
  if (documentElement->hasAttribute(u"\u26A1") ||
      documentElement->hasAttribute("amp") ||
      documentElement->hasAttribute("i-amphtml-layout")) {
    // The DocumentLoader fetches a main resource and handles the result.
    // But it may not be available if JavaScript appends HTML to the page later
    // in the page's lifetime. This can happen both from in-page JavaScript and
    // from extensions. See example callstacks linked from crbug.com/931330.
    if (document->Loader()) {
      document->Loader()->DidObserveLoadingBehavior(
          kLoadingBehaviorAmpDocumentLoaded);
    }
  }
  if (preloader_)
    FetchQueuedPreloads();
}

std::unique_ptr<HTMLPreloadScanner> HTMLDocumentParser::CreatePreloadScanner(
    TokenPreloadScanner::ScannerType scanner_type) {
  return std::make_unique<HTMLPreloadScanner>(
      options_, GetDocument()->Url(),
      std::make_unique<CachedDocumentParameters>(GetDocument()),
      MediaValuesCached::MediaValuesCachedData(*GetDocument()), scanner_type);
}

void HTMLDocumentParser::ScanAndPreload(HTMLPreloadScanner* scanner) {
  if (!preloader_)
    return;

  bool seen_csp_meta_tag = false;
  PreloadRequestStream requests = scanner->Scan(
      GetDocument()->ValidBaseElementURL(), nullptr, seen_csp_meta_tag);
  preloader_->TakeAndPreload(requests);
}

void HTMLDocumentParser::FetchQueuedPreloads() {
  DCHECK(preloader_);

  if (pending_csp_meta_token_ || !GetDocument()->documentElement())
    return;

  if (!queued_preloads_.IsEmpty())
    preloader_->TakeAndPreload(queued_preloads_);
}

}  // namespace blink
