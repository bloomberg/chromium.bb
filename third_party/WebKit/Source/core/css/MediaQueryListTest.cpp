// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/MediaQueryList.h"

#include "bindings/core/v8/V8Binding.h"
#include "core/css/MediaList.h"
#include "core/css/MediaQueryListListener.h"
#include "core/css/MediaQueryMatcher.h"
#include "core/dom/Document.h"
#include <gtest/gtest.h>

namespace blink {

TEST(MediaQueryListTest, CrashInStop)
{
    V8TestingScope scope(v8::Isolate::GetCurrent());
    RefPtrWillBeRawPtr<Document> document = Document::create();
    RefPtrWillBeRawPtr<MediaQueryList> list = MediaQueryList::create(document.get(), MediaQueryMatcher::create(*document), MediaQuerySet::create());
    v8::Local<v8::FunctionTemplate> templ = v8::FunctionTemplate::New(scope.isolate());
    list->addListener(MediaQueryListListener::create(scope.scriptState(), ScriptValue(scope.scriptState(), templ->GetFunction())));
    // Now, MediaQueryList and MediaQueryListListener have reference cycle. We
    // can clear |list|.
    MediaQueryList* rawList = list.release().get();
    rawList->stop();
    // This test passes if it's not crashed.
}

}
