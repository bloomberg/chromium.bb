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

#ifndef PendingScript_h
#define PendingScript_h

#include "bindings/core/v8/ScriptStreamer.h"
#include "core/CoreExport.h"
#include "core/dom/Script.h"
#include "core/dom/ScriptElementBase.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/TextPosition.h"

namespace blink {

class PendingScript;

class CORE_EXPORT PendingScriptClient : public GarbageCollectedMixin {
 public:
  virtual ~PendingScriptClient() {}

  // Invoked when the pending script is ready. This could be during
  // WatchForLoad() (if the pending script was already ready), or when the
  // resource loads (if script streaming is not occurring), or when script
  // streaming finishes.
  virtual void PendingScriptFinished(PendingScript*) = 0;

  DEFINE_INLINE_VIRTUAL_TRACE() {}
};

// A container for an external script which may be loaded and executed.
// This is used to receive a notification of "script is ready"
// https://html.spec.whatwg.org/#the-script-is-ready via PendingScriptClient.
class CORE_EXPORT PendingScript
    : public GarbageCollectedFinalized<PendingScript>,
      public TraceWrapperBase {
  WTF_MAKE_NONCOPYABLE(PendingScript);

 public:
  virtual ~PendingScript();

  TextPosition StartingPosition() const { return starting_position_; }
  void MarkParserBlockingLoadStartTime();
  // Returns the time the load of this script started blocking the parser, or
  // zero if this script hasn't yet blocked the parser, in
  // monotonicallyIncreasingTime.
  double ParserBlockingLoadStartTime() const {
    return parser_blocking_load_start_time_;
  }

  void WatchForLoad(PendingScriptClient*);
  void StopWatchingForLoad();

  ScriptElementBase* GetElement() const;

  virtual ScriptType GetScriptType() const = 0;

  DECLARE_VIRTUAL_TRACE();

  virtual Script* GetSource(const KURL& document_url,
                            bool& error_occurred) const = 0;

  // https://html.spec.whatwg.org/#the-script-is-ready
  virtual bool IsReady() const = 0;
  virtual bool IsExternal() const = 0;
  virtual bool ErrorOccurred() const = 0;
  virtual bool WasCanceled() const = 0;

  // Support for script streaming.
  virtual bool StartStreamingIfPossible(ScriptStreamer::Type, WTF::Closure) = 0;
  virtual bool IsCurrentlyStreaming() const = 0;

  // The following two methods are used for document.write() intervention and
  // have effects only for classic scripts.
  virtual KURL UrlForClassicScript() const = 0;
  virtual void RemoveFromMemoryCache() = 0;

  // Used for DCHECK()s.
  bool IsExternalOrModule() const {
    return IsExternal() || GetScriptType() == ScriptType::kModule;
  }

  void Dispose();

 protected:
  PendingScript(ScriptElementBase*, const TextPosition& starting_position);

  virtual void DisposeInternal() = 0;

  PendingScriptClient* Client() { return client_; }
  bool IsWatchingForLoad() const { return client_; }

  virtual void CheckState() const = 0;

 private:
  // |m_element| must points to the corresponding ScriptLoader's
  // ScriptElementBase and thus must be non-null before dispose() is called
  // (except for unit tests).
  Member<ScriptElementBase> element_;

  TextPosition starting_position_;  // Only used for inline script tags.
  double parser_blocking_load_start_time_;

  Member<PendingScriptClient> client_;
};

}  // namespace blink

#endif  // PendingScript_h
