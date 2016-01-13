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

#include "platform/EventTracer.h"
#include "platform/HTTPNames.h"
#include "platform/heap/Heap.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "wtf/CryptographicallyRandomNumber.h"
#include "wtf/MainThread.h"
#include "wtf/Partitions.h"
#include "wtf/WTF.h"
#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/test/launcher/unit_test_launcher.h>
#include <base/test/test_suite.h>
#include <cc/blink/web_compositor_support_impl.h>
#include <string.h>

static double CurrentTime()
{
    return 0.0;
}

static int runTestSuite(base::TestSuite* testSuite)
{
    int result = testSuite->Run();
    blink::Heap::collectAllGarbage();
    return result;
}

int main(int argc, char** argv)
{
    WTF::setAlwaysZeroRandomSourceForTesting();
    WTF::initialize(CurrentTime, CurrentTime, nullptr, nullptr);
    WTF::initializeMainThread(0);

    blink::TestingPlatformSupport::Config platformConfig;
    cc_blink::WebCompositorSupportImpl compositorSupport;
    platformConfig.compositorSupport = &compositorSupport;
    blink::TestingPlatformSupport platform(platformConfig);

    blink::Heap::init();
    blink::ThreadState::attachMainThread();
    blink::ThreadState::current()->registerTraceDOMWrappers(nullptr, nullptr);
    blink::EventTracer::initialize();

    blink::HTTPNames::init();

    base::TestSuite testSuite(argc, argv);
    int result = base::LaunchUnitTests(argc, argv, base::Bind(runTestSuite, base::Unretained(&testSuite)));

    blink::ThreadState::detachMainThread();
    blink::Heap::shutdown();

    WTF::shutdown();
    return result;
}
