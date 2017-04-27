// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ConditionalFeatures_h
#define ConditionalFeatures_h

#include "platform/PlatformExport.h"
#include "platform/wtf/text/WTFString.h"
#include "v8/include/v8.h"

namespace blink {

class ScriptState;
struct WrapperTypeInfo;

using InstallConditionalFeaturesFunction = void (*)(const WrapperTypeInfo*,
                                                    const ScriptState*,
                                                    v8::Local<v8::Object>,
                                                    v8::Local<v8::Function>);

using InstallPendingConditionalFeatureFunction = void (*)(const String&,
                                                          const ScriptState*);

// Sets the function to be called by |installConditionalFeatures|. The function
// is initially set to the private |installConditionalFeaturesDefault| function,
// but can be overridden by this function. A pointer to the previously set
// function is returned, so that functions can be chained.
PLATFORM_EXPORT InstallConditionalFeaturesFunction
    SetInstallConditionalFeaturesFunction(InstallConditionalFeaturesFunction);

// Sets the function to be called by |installPendingConditionalFeature|. This
// is initially set to the private |installPendingConditionalFeatureDefault|
// function, but can be overridden by this function. A pointer to the previously
// set function is returned, so that functions can be chained.
PLATFORM_EXPORT InstallPendingConditionalFeatureFunction
    SetInstallPendingConditionalFeatureFunction(
        InstallPendingConditionalFeatureFunction);

// Installs all of the conditionally enabled V8 bindings for the given type, in
// a specific context. This is called in V8PerContextData, after the constructor
// and prototype for the type have been created. It indirectly calls the
// function set by |setInstallConditionalFeaturesFunction|.
PLATFORM_EXPORT void InstallConditionalFeatures(const WrapperTypeInfo*,
                                                const ScriptState*,
                                                v8::Local<v8::Object>,
                                                v8::Local<v8::Function>);

// Installs all of the conditionally enabled V8 bindings for a feature, if
// needed. This is called to install a newly-enabled feature on any existing
// objects. If the target object hasn't been created, nothing is installed. The
// enabled feature will be instead be installed when the object is created
// (avoids forcing the creation of objects prematurely).
PLATFORM_EXPORT void InstallPendingConditionalFeature(const String&,
                                                      const ScriptState*);

}  // namespace blink

#endif  // ConditionalFeatures_h
