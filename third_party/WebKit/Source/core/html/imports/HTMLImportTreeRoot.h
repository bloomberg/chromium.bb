// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLImportTreeRoot_h
#define HTMLImportTreeRoot_h

#include "core/html/imports/HTMLImport.h"
#include "platform/Timer.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

class HTMLImportTreeRoot : public HTMLImport {
public:
    static PassOwnPtr<HTMLImportTreeRoot> create(Document*);

    // HTMLImport
    virtual Document* document() const OVERRIDE;
    virtual bool isDone() const OVERRIDE;
    virtual void stateWillChange() OVERRIDE;
    virtual void stateDidChange() OVERRIDE;

    void scheduleRecalcState();

private:
    explicit HTMLImportTreeRoot(Document*);

    void recalcTimerFired(Timer<HTMLImportTreeRoot>*);

    Document* m_document;
    Timer<HTMLImportTreeRoot> m_recalcTimer;
};

DEFINE_TYPE_CASTS(HTMLImportTreeRoot, HTMLImport, import, import->isRoot(), import.isRoot());

}

#endif
