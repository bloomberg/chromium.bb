/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IDBDatabaseCallbacks_h
#define IDBDatabaseCallbacks_h

#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebVector.h"

#include <unordered_map>

namespace blink {

class DOMException;
class IDBDatabase;
class WebIDBDatabaseCallbacks;
struct WebIDBObservation;

class MODULES_EXPORT IDBDatabaseCallbacks
    : public GarbageCollectedFinalized<IDBDatabaseCallbacks> {
 public:
  // Maps observer to transaction, which needs an id and a scope.
  using TransactionMap =
      std::unordered_map<int32_t, std::pair<int64_t, std::vector<int64_t>>>;

  static IDBDatabaseCallbacks* Create();
  virtual ~IDBDatabaseCallbacks();
  void Trace(blink::Visitor*);

  // IDBDatabaseCallbacks
  virtual void OnForcedClose();
  virtual void OnVersionChange(int64_t old_version, int64_t new_version);

  virtual void OnAbort(int64_t transaction_id, DOMException*);
  virtual void OnComplete(int64_t transaction_id);
  virtual void OnChanges(
      const std::unordered_map<int32_t, std::vector<int32_t>>&
          observation_index_map,
      const WebVector<WebIDBObservation>& observations,
      const TransactionMap& transactions);

  void Connect(IDBDatabase*);

  // Returns a new WebIDBDatabaseCallbacks for this object. Must only be
  // called once.
  std::unique_ptr<WebIDBDatabaseCallbacks> CreateWebCallbacks();
  void DetachWebCallbacks();
  void WebCallbacksDestroyed();

 protected:
  // Exposed to subclasses for unit tests.
  IDBDatabaseCallbacks();

 private:
  // The initial IDBOpenDBRequest, final IDBDatabase, and/or
  // WebIDBDatabaseCallbacks have strong references to an IDBDatabaseCallbacks
  // object.
  // Oilpan: We'd like to delete an IDBDatabase object by a
  // GC. WebIDBDatabaseCallbacks can survive the GC, and IDBDatabaseCallbacks
  // can survive too. |database_| should be a weak reference to avoid that an
  // IDBDatabase survives the GC with the IDBDatabaseCallbacks.
  WeakMember<IDBDatabase> database_;

  // Pointer back to the WebIDBDatabaseCallbacks that holds a persistent
  // reference to this object.
  WebIDBDatabaseCallbacks* web_callbacks_ = nullptr;
};

}  // namespace blink

#endif  // IDBDatabaseCallbacks_h
