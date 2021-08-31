// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.translate;

import org.chromium.base.FeatureList;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Exposes translate-specific features to Java since files in org.chromium.components.translate
 * package cannot depend on org.chromium.chrome.browser.flags.ChromeFeatureList.
 */
// TODO(crbug.com/1060097): Remove/update this once a generalized FeatureList exists.
@JNINamespace("translate::android")
public class TranslateFeatureList {
    /** Alphabetical: */
    public static final String CONTENT_LANGUAGES_IN_LANGUAGE_PICKER =
            "ContentLanguagesInLanguagePicker";
    public static final String DETECTED_SOURCE_LANGUAGE_OPTION = "DetectedSourceLanguageOption";

    // Do not instantiate this class.
    private TranslateFeatureList() {}

    /**
     * Returns whether the specified feature is enabled or not.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in components/translate/content/android/translate_feature_list.cc
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public static boolean isEnabled(String featureName) {
        assert FeatureList.isNativeInitialized();
        return TranslateFeatureListJni.get().isEnabled(featureName);
    }

    /**
     * The interface implemented by the automatically generated JNI bindings class
     * TranslateFeatureListJni.
     */
    @NativeMethods
    /* package */ interface Natives {
        boolean isEnabled(String featureName);
    }
}
