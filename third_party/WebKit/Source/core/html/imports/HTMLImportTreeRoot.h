// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLImportTreeRoot_h
#define HTMLImportTreeRoot_h

#include "core/dom/TaskRunnerHelper.h"
#include "core/html/imports/HTMLImport.h"
#include "platform/Timer.h"

namespace blink {

class HTMLImportChild;
class KURL;

class HTMLImportTreeRoot final : public HTMLImport {
 public:
  static HTMLImportTreeRoot* Create(Document*);

  ~HTMLImportTreeRoot() final;
  void Dispose();

  // HTMLImport overrides:
  Document* GetDocument() const final;
  bool HasFinishedLoading() const final;
  void StateWillChange() final;
  void StateDidChange() final;

  void ScheduleRecalcState();

  HTMLImportChild* Add(HTMLImportChild*);
  HTMLImportChild* Find(const KURL&) const;

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit HTMLImportTreeRoot(Document*);

  void RecalcTimerFired(TimerBase*);

  Member<Document> document_;
  TaskRunnerTimer<HTMLImportTreeRoot> recalc_timer_;

  // List of import which has been loaded or being loaded.
  typedef HeapVector<Member<HTMLImportChild>> ImportList;
  ImportList imports_;
};

DEFINE_TYPE_CASTS(HTMLImportTreeRoot,
                  HTMLImport,
                  import,
                  import->IsRoot(),
                  import.IsRoot());

}  // namespace blink

#endif
