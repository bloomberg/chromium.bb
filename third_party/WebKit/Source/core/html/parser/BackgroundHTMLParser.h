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

#ifndef BackgroundHTMLParser_h
#define BackgroundHTMLParser_h

#include <memory>
#include "core/dom/DocumentEncodingData.h"
#include "core/html/parser/BackgroundHTMLInputStream.h"
#include "core/html/parser/CompactHTMLToken.h"
#include "core/html/parser/HTMLParserOptions.h"
#include "core/html/parser/HTMLPreloadScanner.h"
#include "core/html/parser/HTMLSourceTracker.h"
#include "core/html/parser/HTMLTreeBuilderSimulator.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/html/parser/TokenizedChunkQueue.h"
#include "core/html/parser/XSSAuditorDelegate.h"
#include "platform/wtf/WeakPtr.h"

namespace blink {

class HTMLDocumentParser;
class XSSAuditor;
class WebTaskRunner;

class BackgroundHTMLParser {
  USING_FAST_MALLOC(BackgroundHTMLParser);
  WTF_MAKE_NONCOPYABLE(BackgroundHTMLParser);

 public:
  struct Configuration {
    USING_FAST_MALLOC(Configuration);

   public:
    Configuration();
    HTMLParserOptions options;
    WeakPtr<HTMLDocumentParser> parser;
    std::unique_ptr<XSSAuditor> xss_auditor;
    std::unique_ptr<TextResourceDecoder> decoder;
    RefPtr<TokenizedChunkQueue> tokenized_chunk_queue;
    // outstandingTokenLimit must be greater than or equal to
    // pendingTokenLimit
    size_t outstanding_token_limit;
    size_t pending_token_limit;
    bool should_coalesce_chunks;
  };

  // The returned BackgroundHTMLParser should only be used on the parser
  // thread: it must first be initialized by calling init(), and free by
  // calling stop().
  static WeakPtr<BackgroundHTMLParser> Create(std::unique_ptr<Configuration>,
                                              RefPtr<WebTaskRunner>);
  void Init(const KURL& document_url,
            std::unique_ptr<CachedDocumentParameters>,
            const MediaValuesCached::MediaValuesCachedData&);

  struct Checkpoint {
    USING_FAST_MALLOC(Checkpoint);

   public:
    WeakPtr<HTMLDocumentParser> parser;
    std::unique_ptr<HTMLToken> token;
    std::unique_ptr<HTMLTokenizer> tokenizer;
    HTMLTreeBuilderSimulator::State tree_builder_state;
    HTMLInputCheckpoint input_checkpoint;
    TokenPreloadScannerCheckpoint preload_scanner_checkpoint;
    String unparsed_input;
  };

  void AppendRawBytesFromMainThread(std::unique_ptr<Vector<char>>);
  void SetDecoder(std::unique_ptr<TextResourceDecoder>);
  void Flush();
  void ResumeFrom(std::unique_ptr<Checkpoint>);
  void StartedChunkWithCheckpoint(HTMLInputCheckpoint);
  void Finish();
  void Stop();

  void ForcePlaintextForTextDocument();

 private:
  BackgroundHTMLParser(std::unique_ptr<Configuration>, RefPtr<WebTaskRunner>);
  ~BackgroundHTMLParser();

  void AppendDecodedBytes(const String&);
  void MarkEndOfFile();
  void PumpTokenizer();

  // Returns whether or not the HTMLDocumentParser should be notified of
  // pending chunks.
  bool QueueChunkForMainThread();
  void NotifyMainThreadOfNewChunks();
  void UpdateDocument(const String& decoded_data);

  template <typename FunctionType, typename... Ps>
  void RunOnMainThread(FunctionType, Ps&&...);

  WeakPtrFactory<BackgroundHTMLParser> weak_factory_;
  BackgroundHTMLInputStream input_;
  HTMLSourceTracker source_tracker_;
  std::unique_ptr<HTMLToken> token_;
  std::unique_ptr<HTMLTokenizer> tokenizer_;
  HTMLTreeBuilderSimulator tree_builder_simulator_;
  HTMLParserOptions options_;
  const size_t outstanding_token_limit_;
  WeakPtr<HTMLDocumentParser> parser_;

  std::unique_ptr<CompactHTMLTokenStream> pending_tokens_;
  const size_t pending_token_limit_;
  PreloadRequestStream pending_preloads_;
  ViewportDescriptionWrapper viewport_description_;
  XSSInfoStream pending_xss_infos_;

  std::unique_ptr<XSSAuditor> xss_auditor_;
  std::unique_ptr<TokenPreloadScanner> preload_scanner_;
  std::unique_ptr<TextResourceDecoder> decoder_;
  DocumentEncodingData last_seen_encoding_data_;
  RefPtr<WebTaskRunner> loading_task_runner_;
  RefPtr<TokenizedChunkQueue> tokenized_chunk_queue_;

  // Index into |m_pendingTokens| of the last <meta> csp token found. Will be
  // |TokenizedChunk::noPendingToken| if none have been found.
  int pending_csp_meta_token_index_;

  bool starting_script_;
  bool should_coalesce_chunks_;
};

}  // namespace blink

#endif
