/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/parser/BackgroundHTMLParser.h"

#include <memory>
#include "core/html/parser/HTMLDocumentParser.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/html/parser/XSSAuditor.h"
#include "core/html_names.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/Histogram.h"
#include "platform/WebTaskRunner.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Time.h"
#include "platform/wtf/text/TextPosition.h"
#include "public/platform/Platform.h"

namespace blink {

// On a network with high latency and high bandwidth, using a device with a fast
// CPU, we could end up speculatively tokenizing the whole document, well ahead
// of when the main-thread actually needs it. This is a waste of memory (and
// potentially time if the speculation fails). So we limit our outstanding
// tokens arbitrarily to 10,000. Our maximal memory spent speculating will be
// approximately:
// (defaultOutstandingTokenLimit + defaultPendingTokenLimit) *
// sizeof(CompactToken)
//
// We use a separate low and high water mark to avoid
// constantly topping off the main thread's token buffer. At time of writing,
// this is (10000 + 1000) * 28 bytes = ~308kb of memory. These numbers have not
// been tuned.
static const size_t kDefaultOutstandingTokenLimit = 10000;

// We limit our chucks to 1000 tokens, to make sure the main thread is never
// waiting on the parser thread for tokens. This was tuned in
// https://bugs.webkit.org/show_bug.cgi?id=110408.
static const size_t kDefaultPendingTokenLimit = 1000;

using namespace HTMLNames;

#if DCHECK_IS_ON()

static void CheckThatTokensAreSafeToSendToAnotherThread(
    const CompactHTMLTokenStream* tokens) {
  for (size_t i = 0; i < tokens->size(); ++i)
    DCHECK(tokens->at(i).IsSafeToSendToAnotherThread());
}

static void CheckThatPreloadsAreSafeToSendToAnotherThread(
    const PreloadRequestStream& preloads) {
  for (size_t i = 0; i < preloads.size(); ++i)
    DCHECK(preloads[i]->IsSafeToSendToAnotherThread());
}

static void CheckThatXSSInfosAreSafeToSendToAnotherThread(
    const XSSInfoStream& infos) {
  for (size_t i = 0; i < infos.size(); ++i)
    DCHECK(infos[i]->IsSafeToSendToAnotherThread());
}

#endif

WeakPtr<BackgroundHTMLParser> BackgroundHTMLParser::Create(
    std::unique_ptr<Configuration> config,
    scoped_refptr<WebTaskRunner> loading_task_runner) {
  auto* background_parser = new BackgroundHTMLParser(
      std::move(config), std::move(loading_task_runner));
  return background_parser->weak_factory_.CreateWeakPtr();
}

void BackgroundHTMLParser::Init(
    const KURL& document_url,
    std::unique_ptr<CachedDocumentParameters> cached_document_parameters,
    const MediaValuesCached::MediaValuesCachedData& media_values_cached_data) {
  preload_scanner_.reset(new TokenPreloadScanner(
      document_url, std::move(cached_document_parameters),
      media_values_cached_data,
      TokenPreloadScanner::ScannerType::kMainDocument));
}

BackgroundHTMLParser::Configuration::Configuration()
    : outstanding_token_limit(kDefaultOutstandingTokenLimit),
      pending_token_limit(kDefaultPendingTokenLimit),
      should_coalesce_chunks(false) {}

BackgroundHTMLParser::BackgroundHTMLParser(
    std::unique_ptr<Configuration> config,
    scoped_refptr<WebTaskRunner> loading_task_runner)
    : weak_factory_(this),
      token_(WTF::WrapUnique(new HTMLToken)),
      tokenizer_(HTMLTokenizer::Create(config->options)),
      tree_builder_simulator_(config->options),
      options_(config->options),
      outstanding_token_limit_(config->outstanding_token_limit),
      parser_(config->parser),
      pending_tokens_(WTF::WrapUnique(new CompactHTMLTokenStream)),
      pending_token_limit_(config->pending_token_limit),
      xss_auditor_(std::move(config->xss_auditor)),
      decoder_(std::move(config->decoder)),
      loading_task_runner_(std::move(loading_task_runner)),
      tokenized_chunk_queue_(std::move(config->tokenized_chunk_queue)),
      pending_csp_meta_token_index_(
          HTMLDocumentParser::TokenizedChunk::kNoPendingToken),
      starting_script_(false),
      should_coalesce_chunks_(config->should_coalesce_chunks) {
  DCHECK_GT(outstanding_token_limit_, 0u);
  DCHECK_GT(pending_token_limit_, 0u);
  DCHECK_GE(outstanding_token_limit_, pending_token_limit_);
}

BackgroundHTMLParser::~BackgroundHTMLParser() {}

void BackgroundHTMLParser::AppendRawBytesFromMainThread(
    std::unique_ptr<Vector<char>> buffer) {
  DCHECK(decoder_);
  UpdateDocument(decoder_->Decode(buffer->data(), buffer->size()));
}

void BackgroundHTMLParser::AppendDecodedBytes(const String& input) {
  DCHECK(!input_.Current().IsClosed());
  input_.Append(input);
  PumpTokenizer();
}

void BackgroundHTMLParser::SetDecoder(
    std::unique_ptr<TextResourceDecoder> decoder) {
  DCHECK(decoder);
  decoder_ = std::move(decoder);
}

void BackgroundHTMLParser::Flush() {
  DCHECK(decoder_);
  UpdateDocument(decoder_->Flush());
}

void BackgroundHTMLParser::UpdateDocument(const String& decoded_data) {
  DocumentEncodingData encoding_data(*decoder_.get());

  if (encoding_data != last_seen_encoding_data_) {
    last_seen_encoding_data_ = encoding_data;

    xss_auditor_->SetEncoding(encoding_data.Encoding());
    RunOnMainThread(
        &HTMLDocumentParser::DidReceiveEncodingDataFromBackgroundParser,
        parser_, encoding_data);
  }

  if (decoded_data.IsEmpty())
    return;

  AppendDecodedBytes(decoded_data);
}

void BackgroundHTMLParser::ResumeFrom(std::unique_ptr<Checkpoint> checkpoint) {
  parser_ = checkpoint->parser;
  token_ = std::move(checkpoint->token);
  tokenizer_ = std::move(checkpoint->tokenizer);
  tree_builder_simulator_.SetState(checkpoint->tree_builder_state);
  input_.RewindTo(checkpoint->input_checkpoint, checkpoint->unparsed_input);
  preload_scanner_->RewindTo(checkpoint->preload_scanner_checkpoint);
  starting_script_ = false;
  tokenized_chunk_queue_->Clear();
  PumpTokenizer();
}

void BackgroundHTMLParser::StartedChunkWithCheckpoint(
    HTMLInputCheckpoint input_checkpoint) {
  // Note, we should not have to worry about the index being invalid as messages
  // from the main thread will be processed in FIFO order.
  input_.InvalidateCheckpointsBefore(input_checkpoint);
  PumpTokenizer();
}

void BackgroundHTMLParser::Finish() {
  MarkEndOfFile();
  PumpTokenizer();
}

void BackgroundHTMLParser::Stop() {
  delete this;
}

void BackgroundHTMLParser::ForcePlaintextForTextDocument() {
  // This is only used by the TextDocumentParser (a subclass of
  // HTMLDocumentParser) to force us into the PLAINTEXT state w/o using a
  // <plaintext> tag. The TextDocumentParser uses a <pre> tag for historical /
  // compatibility reasons.
  tokenizer_->SetState(HTMLTokenizer::kPLAINTEXTState);
}

void BackgroundHTMLParser::MarkEndOfFile() {
  DCHECK(!input_.Current().IsClosed());
  input_.Append(String(&kEndOfFileMarker, 1));
  input_.Close();
}

void BackgroundHTMLParser::PumpTokenizer() {
  TRACE_EVENT0("loading", "BackgroundHTMLParser::pumpTokenizer");
  HTMLTreeBuilderSimulator::SimulatedToken simulated_token =
      HTMLTreeBuilderSimulator::kOtherToken;

  // No need to start speculating until the main thread has almost caught up.
  if (input_.TotalCheckpointTokenCount() > outstanding_token_limit_)
    return;

  bool should_notify_main_thread = false;
  while (true) {
    if (xss_auditor_->IsEnabled())
      source_tracker_.Start(input_.Current(), tokenizer_.get(), *token_);

    if (!tokenizer_->NextToken(input_.Current(), *token_)) {
      // We've reached the end of our current input.
      should_notify_main_thread |= QueueChunkForMainThread();
      break;
    }

    if (xss_auditor_->IsEnabled())
      source_tracker_.end(input_.Current(), tokenizer_.get(), *token_);

    {
      TextPosition position = TextPosition(input_.Current().CurrentLine(),
                                           input_.Current().CurrentColumn());

      if (std::unique_ptr<XSSInfo> xss_info =
              xss_auditor_->FilterToken(FilterTokenRequest(
                  *token_, source_tracker_, tokenizer_->ShouldAllowCDATA()))) {
        xss_info->text_position_ = position;
        pending_xss_infos_.push_back(std::move(xss_info));
      }

      CompactHTMLToken token(token_.get(), position);

      bool is_csp_meta_tag = false;
      preload_scanner_->Scan(token, input_.Current(), pending_preloads_,
                             &viewport_description_, &is_csp_meta_tag);

      simulated_token =
          tree_builder_simulator_.Simulate(token, tokenizer_.get());

      // Break chunks before a script tag is inserted and flag the chunk as
      // starting a script so the main parser can decide if it should yield
      // before processing the chunk.
      if (simulated_token == HTMLTreeBuilderSimulator::kScriptStart) {
        should_notify_main_thread |= QueueChunkForMainThread();
        starting_script_ = true;
      }

      pending_tokens_->push_back(token);
      if (is_csp_meta_tag) {
        pending_csp_meta_token_index_ = pending_tokens_->size() - 1;
      }
    }

    token_->Clear();

    if (simulated_token == HTMLTreeBuilderSimulator::kScriptEnd ||
        simulated_token == HTMLTreeBuilderSimulator::kStyleEnd ||
        simulated_token == HTMLTreeBuilderSimulator::kLink ||
        pending_tokens_->size() >= pending_token_limit_) {
      should_notify_main_thread |= QueueChunkForMainThread();
      // If we're far ahead of the main thread, yield for a bit to avoid
      // consuming too much memory.
      if (input_.TotalCheckpointTokenCount() > outstanding_token_limit_)
        break;
    }

    if (!should_coalesce_chunks_ && should_notify_main_thread) {
      RunOnMainThread(&HTMLDocumentParser::NotifyPendingTokenizedChunks,
                      parser_);
      should_notify_main_thread = false;
    }
  }
  // Wait to notify the main thread about the chunks until we're at the limit.
  // This lets the background parser generate lots of valuable preloads before
  // anything expensive (extensions, scripts) take up time on the main thread. A
  // busy main thread can cause preload delays.
  if (should_notify_main_thread) {
    RunOnMainThread(&HTMLDocumentParser::NotifyPendingTokenizedChunks, parser_);
  }
}

bool BackgroundHTMLParser::QueueChunkForMainThread() {
  if (pending_tokens_->IsEmpty())
    return false;

#if DCHECK_IS_ON()
  CheckThatTokensAreSafeToSendToAnotherThread(pending_tokens_.get());
  CheckThatPreloadsAreSafeToSendToAnotherThread(pending_preloads_);
  CheckThatXSSInfosAreSafeToSendToAnotherThread(pending_xss_infos_);
#endif

  std::unique_ptr<HTMLDocumentParser::TokenizedChunk> chunk =
      WTF::WrapUnique(new HTMLDocumentParser::TokenizedChunk);
  TRACE_EVENT_WITH_FLOW0("blink,loading",
                         "BackgroundHTMLParser::sendTokensToMainThread",
                         chunk.get(), TRACE_EVENT_FLAG_FLOW_OUT);

  chunk->preloads.swap(pending_preloads_);
  if (viewport_description_.set)
    chunk->viewport = viewport_description_;
  chunk->xss_infos.swap(pending_xss_infos_);
  chunk->tokenizer_state = tokenizer_->GetState();
  chunk->tree_builder_state = tree_builder_simulator_.GetState();
  chunk->input_checkpoint = input_.CreateCheckpoint(pending_tokens_->size());
  chunk->preload_scanner_checkpoint = preload_scanner_->CreateCheckpoint();
  chunk->tokens = std::move(pending_tokens_);
  chunk->starting_script = starting_script_;
  chunk->pending_csp_meta_token_index = pending_csp_meta_token_index_;
  starting_script_ = false;
  pending_csp_meta_token_index_ =
      HTMLDocumentParser::TokenizedChunk::kNoPendingToken;

  bool is_empty = tokenized_chunk_queue_->Enqueue(std::move(chunk));

  pending_tokens_ = WTF::WrapUnique(new CompactHTMLTokenStream);
  return is_empty;
}

// If the background parser is already running on the main thread, then it is
// not necessary to post a task to the main thread to run asynchronously. The
// main parser deals with chunking up its own work.
// TODO(csharrison): This is a pretty big hack because we don't actually need a
// CrossThreadClosure in these cases. This is just experimental.
template <typename FunctionType, typename... Ps>
void BackgroundHTMLParser::RunOnMainThread(FunctionType function,
                                           Ps&&... parameters) {
  if (IsMainThread()) {
    WTF::Bind(std::move(function), std::forward<Ps>(parameters)...).Run();
  } else {
    loading_task_runner_->PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(std::move(function), std::forward<Ps>(parameters)...));
  }
}

}  // namespace blink
