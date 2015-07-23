/*
 * Copyright (c) 2011, Google Inc. All rights reserved.
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

#include "config.h"
#include "bindings/core/v8/ScriptProfiler.h"

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "wtf/ThreadSpecific.h"

#include <v8-profiler.h>
#include <v8.h>

namespace blink {

typedef HashMap<String, double> ProfileNameIdleTimeMap;

void ScriptProfiler::setSamplingInterval(int intervalUs)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::CpuProfiler* profiler = isolate->GetCpuProfiler();
    if (profiler)
        profiler->SetSamplingInterval(intervalUs);
}

void ScriptProfiler::start(const String& title)
{
    ProfileNameIdleTimeMap* profileNameIdleTimeMap = ScriptProfiler::currentProfileNameIdleTimeMap();
    if (profileNameIdleTimeMap->contains(title))
        return;
    profileNameIdleTimeMap->add(title, 0);

    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::CpuProfiler* profiler = isolate->GetCpuProfiler();
    if (!profiler)
        return;
    v8::HandleScope handleScope(isolate);
    profiler->StartProfiling(v8String(isolate, title), true);
}

PassRefPtrWillBeRawPtr<ScriptProfile> ScriptProfiler::stop(const String& title)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::CpuProfiler* profiler = isolate->GetCpuProfiler();
    if (!profiler)
        return nullptr;
    v8::HandleScope handleScope(isolate);
    v8::CpuProfile* profile = profiler->StopProfiling(v8String(isolate, title));
    if (!profile)
        return nullptr;

    String profileTitle = toCoreString(profile->GetTitle());
    double idleTime = 0.0;
    ProfileNameIdleTimeMap* profileNameIdleTimeMap = ScriptProfiler::currentProfileNameIdleTimeMap();
    ProfileNameIdleTimeMap::iterator profileIdleTime = profileNameIdleTimeMap->find(profileTitle);
    if (profileIdleTime != profileNameIdleTimeMap->end()) {
        idleTime = profileIdleTime->value * 1000.0;
        profileNameIdleTimeMap->remove(profileIdleTime);
    }

    return ScriptProfile::create(profile, idleTime);
}

ProfileNameIdleTimeMap* ScriptProfiler::currentProfileNameIdleTimeMap()
{
    AtomicallyInitializedStaticReference(WTF::ThreadSpecific<ProfileNameIdleTimeMap>, map, new WTF::ThreadSpecific<ProfileNameIdleTimeMap>);
    return map;
}

void ScriptProfiler::setIdle(bool isIdle)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (v8::CpuProfiler* profiler = isolate->GetCpuProfiler())
        profiler->SetIdle(isIdle);
}

} // namespace blink
