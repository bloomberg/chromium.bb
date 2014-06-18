// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.util.Base64;
import android.util.Log;

import java.security.SecureRandom;
import java.util.HashMap;

/**
 * This class is responsible for fetching a third party token from the user using the OAuth2
 * implicit flow.  It pops up a third party login page located at |tokenurl|.  It relies on the
 * |ThirdPartyTokenFetcher$OAuthRedirectActivity| to intercept the access token from the redirect at
 * |REDIRECT_URI_SCHEME|://|REDIRECT_URI_HOST| upon successful login.
 */
public class ThirdPartyTokenFetcher {
    /** Callback for receiving the token. */
    public interface Callback {
        void onTokenFetched(String code, String accessToken);
    }

    /** Redirect URI. See http://tools.ietf.org/html/rfc6749#section-3.1.2. */
    private static final String REDIRECT_URI_HOST = "oauthredirect";

    /**
     * Request both the authorization code and access token from the server.  See
     * http://tools.ietf.org/html/rfc6749#section-3.1.1.
     */
    private static final String RESPONSE_TYPE = "code token";

    /** This is used to securely generate an opaque 128 bit for the |mState| variable. */
    private static SecureRandom sSecureRandom = new SecureRandom();

    /** This is used to launch the third party login page in the browser. */
    private Activity mContext;

    /**
     * An opaque value used by the client to maintain state between the request and callback.  The
     * authorization server includes this value when redirecting the user-agent back to the client.
     * The parameter is used for preventing cross-site request forgery. See
     * http://tools.ietf.org/html/rfc6749#section-10.12.
     */
    private final String mState;

    /** URL of the third party login page. */
    private final String mTokenUrl;

    /** The client identifier. See http://tools.ietf.org/html/rfc6749#section-2.2. */
    private final String mClientId;

    /** The scope of access request. See http://tools.ietf.org/html/rfc6749#section-3.3. */
    private final String mScope;

    private final Callback mCallback;

    private final String mRedirectUriScheme;

    private final String mRedirectUri;

    public ThirdPartyTokenFetcher(Activity context,
                                  String tokenUrl,
                                  String clientId,
                                  String scope,
                                  Callback callback) {
        this.mContext = context;
        this.mTokenUrl = tokenUrl;
        this.mClientId = clientId;
        this.mState = generateXsrfToken();
        this.mScope = scope;
        this.mCallback = callback;

        this.mRedirectUriScheme = context.getApplicationContext().getPackageName();
        this.mRedirectUri = mRedirectUriScheme + "://" + REDIRECT_URI_HOST;
    }

    public void fetchToken() {
        Uri.Builder uriBuilder = Uri.parse(mTokenUrl).buildUpon();
        uriBuilder.appendQueryParameter("redirect_uri", this.mRedirectUri);
        uriBuilder.appendQueryParameter("scope", mScope);
        uriBuilder.appendQueryParameter("client_id", mClientId);
        uriBuilder.appendQueryParameter("state", mState);
        uriBuilder.appendQueryParameter("response_type", RESPONSE_TYPE);

        Uri uri = uriBuilder.build();
        Intent intent = new Intent(Intent.ACTION_VIEW, uri);
        Log.i("ThirdPartyAuth", "fetchToken() url:" + uri);
        OAuthRedirectActivity.setEnabled(mContext, true);

        try {
            mContext.startActivity(intent);
        } catch (ActivityNotFoundException e) {
            failFetchToken("No browser is installed to open the third party authentication page.");
        }
    }

    private boolean isValidIntent(Intent intent) {
        assert intent != null;

        String action = intent.getAction();

        Uri data = intent.getData();
        if (data != null) {
            return Intent.ACTION_VIEW.equals(action) &&
                   this.mRedirectUriScheme.equals(data.getScheme()) &&
                   REDIRECT_URI_HOST.equals(data.getHost());
        }
        return false;
    }

    public boolean handleTokenFetched(Intent intent) {
        assert intent != null;

        if (!isValidIntent(intent)) {
            Log.w("ThirdPartyAuth", "Ignoring unmatched intent.");
            return false;
        }

        Uri data = intent.getData();
        HashMap<String, String> params = getFragmentParameters(data);

        String accessToken = params.get("access_token");
        String code = params.get("code");
        String state = params.get("state");

        if (!mState.equals(state)) {
            failFetchToken("Ignoring redirect with invalid state.");
            return false;
        }

        if (code == null || accessToken == null) {
            failFetchToken("Ignoring redirect with missing code or token.");
            return false;
        }

        Log.i("ThirdPartyAuth", "handleTokenFetched().");
        mCallback.onTokenFetched(code, accessToken);
        OAuthRedirectActivity.setEnabled(mContext, false);
        return true;
    }

    private void failFetchToken(String errorMessage) {
        Log.e("ThirdPartyAuth", errorMessage);
        mCallback.onTokenFetched("", "");
        OAuthRedirectActivity.setEnabled(mContext, false);
    }

    /** Generate a 128 bit URL-safe opaque string to prevent cross site request forgery (XSRF).*/
    private static String generateXsrfToken() {
        byte[] bytes = new byte[16];
        sSecureRandom.nextBytes(bytes);
        // Uses a variant of Base64 to make sure the URL is URL safe:
        // URL_SAFE replaces - with _ and + with /.
        // NO_WRAP removes the trailing newline character.
        // NO_PADDING removes any trailing =.
        return Base64.encodeToString(bytes, Base64.URL_SAFE | Base64.NO_WRAP | Base64.NO_PADDING);
    }

    /** Parses the fragment string into a key value pair. */
    private static HashMap<String, String> getFragmentParameters(Uri uri) {
        assert uri != null;
        HashMap<String, String> result = new HashMap<String, String>();

        String fragment = uri.getFragment();

        if (fragment != null) {
            String[] parts = fragment.split("&");

            for (String part : parts) {
                String keyValuePair[] = part.split("=", 2);
                if (keyValuePair.length == 2) {
                    result.put(keyValuePair[0], keyValuePair[1]);
                }
            }
        }
        return result;
    };

    /**
     * In the OAuth2 implicit flow, the browser will be redirected to
     * |REDIRECT_URI_SCHEME|://|REDIRECT_URI_HOST| upon a successful login. OAuthRedirectActivity
     * uses an intent filter in the manifest to intercept the URL and launch the chromoting app.
     *
     * Unfortunately, most browsers on Android, e.g. chrome, reload the URL when a browser
     * tab is activated.  As a result, chromoting is launched unintentionally when the user restarts
     * chrome or closes other tabs that causes the redirect URL to become the topmost tab.
     *
     * To solve the problem, the redirect intent-filter is declared in a separate activity,
     * |OAuthRedirectActivity| instead of the MainActivity.  In this way, we can disable it,
     * together with its intent filter, by default. |OAuthRedirectActivity| is only enabled when
     * there is a pending token fetch request.
     */
    public static class OAuthRedirectActivity extends Activity {
        @Override
        public void onStart() {
            super.onStart();
            // |OAuthRedirectActivity| runs in its own task, it needs to route the intent back
            // to Chromoting.java to access the state of the current request.
            Intent intent = getIntent();
            intent.setClass(this, Chromoting.class);
            startActivity(intent);
            finishActivity(0);
        }

        public static void setEnabled(Activity context, boolean enabled) {
            int enabledState = enabled ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                                       : PackageManager.COMPONENT_ENABLED_STATE_DEFAULT;
            ComponentName component = new ComponentName(
                    context.getApplicationContext(),
                    ThirdPartyTokenFetcher.OAuthRedirectActivity.class);
            context.getPackageManager().setComponentEnabledSetting(
                    component,
                    enabledState,
                    PackageManager.DONT_KILL_APP);
        }
    }
}
