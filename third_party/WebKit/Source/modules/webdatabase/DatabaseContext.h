/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2011 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef DatabaseContext_h
#define DatabaseContext_h

#include "core/dom/ContextLifecycleObserver.h"
#include "platform/heap/Handle.h"

namespace blink {

class DatabaseContext;
class DatabaseThread;
class ExecutionContext;
class SecurityOrigin;

class DatabaseContext final : public GarbageCollectedFinalized<DatabaseContext>,
                              public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(DatabaseContext);

 public:
  friend class DatabaseManager;

  static DatabaseContext* Create(ExecutionContext*);

  ~DatabaseContext();
  virtual void Trace(blink::Visitor*);

  // For life-cycle management (inherited from ContextLifecycleObserver):
  void ContextDestroyed(ExecutionContext*) override;

  DatabaseContext* Backend();
  DatabaseThread* GetDatabaseThread();
  bool DatabaseThreadAvailable();

  void SetHasOpenDatabases() { has_open_databases_ = true; }
  // Blocks the caller thread until cleanup tasks are completed.
  void StopDatabases();

  bool AllowDatabaseAccess() const;

  SecurityOrigin* GetSecurityOrigin() const;
  bool IsContextThread() const;

 private:
  explicit DatabaseContext(ExecutionContext*);

  Member<DatabaseThread> database_thread_;
  bool has_open_databases_;  // This never changes back to false, even after the
                             // database thread is closed.
  bool has_requested_termination_;
};

}  // namespace blink

#endif  // DatabaseContext_h
