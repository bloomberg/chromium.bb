/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HTMLImportsController_h
#define HTMLImportsController_h

#include "core/dom/Document.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"

namespace blink {

class FetchParameters;
class HTMLImport;
class HTMLImportChild;
class HTMLImportChildClient;
class HTMLImportLoader;
class HTMLImportTreeRoot;
class KURL;

class HTMLImportsController final
    : public GarbageCollected<HTMLImportsController>,
      public TraceWrapperBase {
 public:
  static HTMLImportsController* Create(Document& master) {
    return new HTMLImportsController(master);
  }

  HTMLImportTreeRoot* Root() const { return root_; }

  bool ShouldBlockScriptExecution(const Document&) const;
  HTMLImportChild* Load(HTMLImport* parent,
                        HTMLImportChildClient*,
                        FetchParameters&);

  Document* Master() const;

  HTMLImportLoader* CreateLoader();

  size_t LoaderCount() const { return loaders_.size(); }
  HTMLImportLoader* LoaderAt(size_t i) const { return loaders_[i]; }
  HTMLImportLoader* LoaderFor(const Document&) const;

  DECLARE_TRACE();

  void Dispose();

  DECLARE_TRACE_WRAPPERS();

 private:
  explicit HTMLImportsController(Document&);

  HTMLImportChild* CreateChild(const KURL&,
                               HTMLImportLoader*,
                               HTMLImport* parent,
                               HTMLImportChildClient*);

  TraceWrapperMember<HTMLImportTreeRoot> root_;
  using LoaderList = HeapVector<Member<HTMLImportLoader>>;
  LoaderList loaders_;
};

}  // namespace blink

#endif  // HTMLImportsController_h
