// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import java.net.URLDecoder;
import java.util.HashMap;
import java.util.Map;

/**
 * Parameter class to pass into Autofill Assistant starting procedure.
 */
public class AutofillAssistantArguments {
    /**
     * Builder class for {@link AutofillAssistantArguments}.
     */
    public static class Builder {
        private AutofillAssistantArguments mArguments;

        private Builder(AutofillAssistantArguments arguments) {
            mArguments = arguments;
        }

        public Builder fromBundle(@Nullable Bundle bundle) {
            if (bundle != null) {
                for (String key : bundle.keySet()) {
                    Object value = bundle.get(key);
                    if (value == null) {
                        continue;
                    }
                    if (key.startsWith(INTENT_SPECIAL_PREFIX)) {
                        if (key.equals(INTENT_SPECIAL_PREFIX + PARAMETER_EXPERIMENT_IDS)) {
                            mArguments.addExperimentIds(value.toString());
                        }
                        // There are no other special parameters.
                    } else if (key.startsWith(INTENT_EXTRA_PREFIX)) {
                        if (key.equals(INTENT_EXTRA_PREFIX + PARAMETER_EXPERIMENT_IDS)) {
                            mArguments.addExperimentIds(value.toString());
                        } else {
                            mArguments.mAutofillAssistantParameters.put(
                                    key.substring(INTENT_EXTRA_PREFIX.length()), value);
                        }
                    } else {
                        mArguments.mIntentExtras.put(key, value);
                    }
                }
            }

            return this;
        }

        public Builder withInitialUrl(String url) {
            mArguments.mInitialUrl = url;
            return this;
        }

        public Builder addParameter(String key, String value) {
            mArguments.mAutofillAssistantParameters.put(key, value);
            return this;
        }

        public AutofillAssistantArguments build() {
            return mArguments;
        }
    }

    private static final String UTF8 = "UTF-8";

    /**
     * Prefix for Intent extras relevant to this feature.
     *
     * <p>Intent starting with this prefix are reported to the controller as parameters, except for
     * the ones starting with {@code INTENT_SPECIAL_PREFIX}.
     */
    private static final String INTENT_EXTRA_PREFIX =
            "org.chromium.chrome.browser.autofill_assistant.";

    /** Prefix for intent extras which are not parameters. */
    private static final String INTENT_SPECIAL_PREFIX = INTENT_EXTRA_PREFIX + "special.";

    /** Special parameter that enables the feature. */
    private static final String PARAMETER_ENABLED = "ENABLED";

    /** Special parameter for the calling account. */
    private static final String PARAMETER_CALLER_ACCOUNT = "CALLER_ACCOUNT";

    /** Special parameter for user email. */
    private static final String PARAMETER_USER_EMAIL = "USER_EMAIL";

    /**
     * Identifier used by parameters/or special intent that indicates experiments passed from
     * the caller.
     */
    private static final String PARAMETER_EXPERIMENT_IDS = "EXPERIMENT_IDS";

    private Map<String, Object> mAutofillAssistantParameters;
    private Map<String, Object> mIntentExtras;
    private StringBuilder mExperimentIds;
    private String mInitialUrl;

    private AutofillAssistantArguments() {
        mAutofillAssistantParameters = new HashMap<>();
        mIntentExtras = new HashMap<>();
        mExperimentIds = new StringBuilder();
    }

    public static Builder newBuilder() {
        return new Builder(new AutofillAssistantArguments());
    }

    /**
     * In M74 experiment ids might come from parameters. This function merges both experiment ids
     * from special intent and parameters.
     */
    private void addExperimentIds(@Nullable String experimentIds) {
        if (TextUtils.isEmpty(experimentIds)) {
            return;
        }
        if (mExperimentIds.length() > 0 && !mExperimentIds.toString().endsWith(",")) {
            mExperimentIds.append(",");
        }
        mExperimentIds.append(experimentIds);
    }

    private boolean getBooleanParameter(String key) {
        Object value = mAutofillAssistantParameters.get(key);
        if (!(value instanceof Boolean)) { // Also catches null.
            return false;
        }

        return (Boolean) value;
    }

    @Nullable
    private String getStringParameter(String key) {
        Object value = mAutofillAssistantParameters.get(key);
        if (!(value instanceof String)) { // Also catches null.
            return null;
        }

        return decode((String) value);
    }

    private String decode(String value) {
        try {
            return URLDecoder.decode(value, UTF8);
        } catch (java.io.UnsupportedEncodingException e) {
            throw new IllegalStateException("Encoding not available.", e);
        }
    }

    public Map<String, String> getParameters() {
        Map<String, String> map = new HashMap<>();

        for (String key : mAutofillAssistantParameters.keySet()) {
            if (PARAMETER_ENABLED.equals(key) || PARAMETER_CALLER_ACCOUNT.equals(key)) {
                continue;
            }
            map.put(key, decode(mAutofillAssistantParameters.get(key).toString()));
        }

        return map;
    }

    /**
     * Searches the parameters for the ENABLED flag.
     * @return whether the feature is set as enabled.
     */
    public boolean isEnabled() {
        return getBooleanParameter(PARAMETER_ENABLED);
    }

    /**
     * Searches the merged experiment ids in normal and special parameters.
     * @return comma separated list of experiment ids, or empty string.
     */
    public String getExperimentIds() {
        return mExperimentIds.toString();
    }

    /**
     * Finds the caller account from the CALLER_ACCOUNT entry.
     * @return caller account or null.
     */
    @Nullable
    public String getCallerAccount() {
        return getStringParameter(PARAMETER_CALLER_ACCOUNT);
    }

    /**
     * Finds the user email from the USER_EMAIL entry or from the ACCOUNT_NAME extra.
     * @return user email or null.
     */
    @Nullable
    public String getUserName() {
        String fromParameter = getStringParameter(PARAMETER_USER_EMAIL);
        if (!TextUtils.isEmpty(fromParameter)) {
            return fromParameter;
        }

        for (String extra : mIntentExtras.keySet()) {
            if (extra.endsWith("ACCOUNT_NAME")) {
                return mIntentExtras.get(extra).toString();
            }
        }

        return null;
    }

    public String getInitialUrl() {
        return mInitialUrl;
    }
}
