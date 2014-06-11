// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIAccessInitializer_h
#define MIDIAccessInitializer_h

#include "bindings/v8/ScriptPromise.h"
#include "modules/webmidi/MIDIAccessor.h"
#include "modules/webmidi/MIDIAccessorClient.h"
#include "modules/webmidi/MIDIOptions.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

class MIDIAccess;
class ScriptState;
class ScriptPromiseResolverWithContext;

class MIDIAccessInitializer : public MIDIAccessorClient {
public:
    static PassOwnPtr<MIDIAccessInitializer> create(const MIDIOptions& options, MIDIAccess* access)
    {
        return adoptPtr(new MIDIAccessInitializer(options, access));
    }
    virtual ~MIDIAccessInitializer();

    ScriptPromise initialize(ScriptState*);
    void cancel();

    bool hasPendingActivity() const { return m_state == Requesting; }

    // MIDIAccessorClient
    virtual void didAddInputPort(const String& id, const String& manufacturer, const String& name, const String& version) OVERRIDE;
    virtual void didAddOutputPort(const String& id, const String& manufacturer, const String& name, const String& version) OVERRIDE;
    virtual void didStartSession(bool success, const String& error, const String& message) OVERRIDE;
    virtual void didReceiveMIDIData(unsigned portIndex, const unsigned char* data, size_t length, double timeStamp) OVERRIDE { }

    void setSysexEnabled(bool value);
    SecurityOrigin* securityOrigin() const;

private:
    class PostAction;
    enum State {
        Requesting,
        Resolved,
        Stopped,
    };

    MIDIAccessInitializer(const MIDIOptions&, MIDIAccess*);

    ExecutionContext* executionContext() const;
    void permissionDenied();
    void doPostAction(State);

    State m_state;
    WeakPtrFactory<MIDIAccessInitializer> m_weakPtrFactory;
    RefPtr<ScriptPromiseResolverWithContext> m_resolver;
    OwnPtr<MIDIAccessor> m_accessor;
    MIDIOptions m_options;
    bool m_sysexEnabled;
    // m_access has this object, so it's safe to have the raw pointer.
    MIDIAccess* m_access;
};

} // namespace WebCore


#endif // MIDIAccessInitializer_h
