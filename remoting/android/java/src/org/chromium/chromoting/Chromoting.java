// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.app.Activity;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import org.chromium.chromoting.jni.JniInterface;

import java.io.IOException;
import java.util.Arrays;

/**
 * The user interface for querying and displaying a user's host list from the directory server. It
 * also requests and renews authentication tokens using the system account manager.
 */
public class Chromoting extends Activity implements JniInterface.ConnectionListener,
        AccountManagerCallback<Bundle>, HostListLoader.Callback {
    /** Only accounts of this type will be selectable for authentication. */
    private static final String ACCOUNT_TYPE = "com.google";

    /** Scopes at which the authentication token we request will be valid. */
    private static final String TOKEN_SCOPE = "oauth2:https://www.googleapis.com/auth/chromoting " +
            "https://www.googleapis.com/auth/googletalk";

    /** User's account details. */
    private Account mAccount;

    /** Account auth token. */
    private String mToken;

    /** Helper for fetching the host list. */
    private HostListLoader mHostListLoader;

    /** List of hosts. */
    private HostInfo[] mHosts;

    /** Refresh button. */
    private MenuItem mRefreshButton;

    /** Account switcher. */
    private MenuItem mAccountSwitcher;

    /** Greeting at the top of the displayed list. */
    private TextView mGreeting;

    /** Host list as it appears to the user. */
    private ListView mList;

    /** Dialog for reporting connection progress. */
    private ProgressDialog mProgressIndicator;

    /**
     * This is set when receiving an authentication error from the HostListLoader. If that occurs,
     * this flag is set and a fresh authentication token is fetched from the AccountsService, and
     * used to request the host list a second time.
     */
    boolean mAlreadyTried;

    /**
     * Called when the activity is first created. Loads the native library and requests an
     * authentication token from the system.
     */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

        mAlreadyTried = false;
        mHostListLoader = new HostListLoader();

        // Get ahold of our view widgets.
        mGreeting = (TextView)findViewById(R.id.hostList_greeting);
        mList = (ListView)findViewById(R.id.hostList_chooser);

        // Bring native components online.
        JniInterface.loadLibrary(this);

        SharedPreferences prefs = getPreferences(MODE_PRIVATE);
        if (prefs.contains("account_name") && prefs.contains("account_type")) {
            // Perform authentication using saved account selection.
            mAccount = new Account(prefs.getString("account_name", null),
                    prefs.getString("account_type", null));
            AccountManager.get(this).getAuthToken(mAccount, TOKEN_SCOPE, null, this, this, null);
            if (mAccountSwitcher != null) {
                mAccountSwitcher.setTitle(mAccount.name);
            }
        } else {
            // Request auth callback once user has chosen an account.
            Log.i("auth", "Requesting auth token from system");
            AccountManager.get(this).getAuthTokenByFeatures(ACCOUNT_TYPE, TOKEN_SCOPE, null, this,
                    null, null, this, null);
        }
    }

    /** Called when the activity is finally finished. */
    @Override
    public void onDestroy() {
        super.onDestroy();
        JniInterface.disconnectFromHost();
    }

    /** Called to initialize the action bar. */
    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.chromoting_actionbar, menu);
        mRefreshButton = menu.findItem(R.id.actionbar_directoryrefresh);
        mAccountSwitcher = menu.findItem(R.id.actionbar_accountswitcher);

        Account[] usableAccounts = AccountManager.get(this).getAccountsByType(ACCOUNT_TYPE);
        if (usableAccounts.length == 1 && usableAccounts[0].equals(mAccount)) {
            // If we're using the only available account, don't offer account switching.
            // (If there are *no* accounts available, clicking this allows you to add a new one.)
            mAccountSwitcher.setEnabled(false);
        }

        if (mAccount == null) {
            // If no account has been chosen, don't allow the user to refresh the listing.
            mRefreshButton.setEnabled(false);
        } else {
            // If the user has picked an account, show its name directly on the account switcher.
            mAccountSwitcher.setTitle(mAccount.name);
        }

        return super.onCreateOptionsMenu(menu);
    }

    /** Called whenever an action bar button is pressed. */
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        mAlreadyTried = false;
        if (item == mAccountSwitcher) {
            // The account switcher triggers a listing of all available accounts.
            AccountManager.get(this).getAuthTokenByFeatures(ACCOUNT_TYPE, TOKEN_SCOPE, null, this,
                    null, null, this, null);
        } else {
            // The refresh button simply makes use of the currently-chosen account.
            AccountManager.get(this).getAuthToken(mAccount, TOKEN_SCOPE, null, this, this, null);
        }

        return true;
    }

    /** Called when the user taps on a host entry. */
    public void connectToHost(HostInfo host) {
        if (host.jabberId.isEmpty() || host.publicKey.isEmpty()) {
            // TODO(lambroslambrou): If these keys are not present, treat this as a connection
            // failure and reload the host list (see crbug.com/304719).
            Toast.makeText(this, getString(R.string.error_reading_host),
                    Toast.LENGTH_LONG).show();
            return;
        }

        JniInterface.connectToHost(mAccount.name, mToken, host.jabberId, host.id, host.publicKey,
                this);
    }

    @Override
    public void run(AccountManagerFuture<Bundle> future) {
        Log.i("auth", "User finished with auth dialogs");
        Bundle result = null;
        String explanation = null;
        try {
            // Here comes our auth token from the Android system.
            result = future.getResult();
        } catch (OperationCanceledException ex) {
            explanation = getString(R.string.error_auth_canceled);
        } catch (AuthenticatorException ex) {
            explanation = getString(R.string.error_no_accounts);
        } catch (IOException ex) {
            explanation = getString(R.string.error_bad_connection);
        }

        if (result == null) {
            Toast.makeText(this, explanation, Toast.LENGTH_LONG).show();
            return;
        }

        String accountName = result.getString(AccountManager.KEY_ACCOUNT_NAME);
        String accountType = result.getString(AccountManager.KEY_ACCOUNT_TYPE);
        String authToken = result.getString(AccountManager.KEY_AUTHTOKEN);
        Log.i("auth", "Received an auth token from system");

        mAccount = new Account(accountName, accountType);
        mToken = authToken;
        getPreferences(MODE_PRIVATE).edit().putString("account_name", accountName).
                putString("account_type", accountType).apply();

        mHostListLoader.retrieveHostList(authToken, this);
    }

    @Override
    public void onHostListReceived(HostInfo[] hosts) {
        // Store a copy of the array, so that it can't be mutated by the HostListLoader. HostInfo
        // is an immutable type, so a shallow copy of the array is sufficient here.
        mHosts = Arrays.copyOf(hosts, hosts.length);
        updateUi();
    }

    @Override
    public void onError(HostListLoader.Error error) {
        String explanation = null;
        switch (error) {
            case AUTH_FAILED:
                break;
            case NETWORK_ERROR:
                explanation = getString(R.string.error_bad_connection);
                break;
            case SERVICE_UNAVAILABLE:
            case UNEXPECTED_RESPONSE:
                explanation = getString(R.string.error_unexpected_response);
                break;
            case UNKNOWN:
                explanation = getString(R.string.error_unknown);
                break;
            default:
                // Unreachable.
                return;
        }

        if (explanation != null) {
            Toast.makeText(this, explanation, Toast.LENGTH_LONG).show();
            return;
        }

        // This is the AUTH_FAILED case.

        if (!mAlreadyTried) {
            // This was our first connection attempt.

            AccountManager authenticator = AccountManager.get(this);
            mAlreadyTried = true;

            Log.w("auth", "Requesting renewal of rejected auth token");
            authenticator.invalidateAuthToken(mAccount.type, mToken);
            mToken = null;
            authenticator.getAuthToken(mAccount, TOKEN_SCOPE, null, this, this, null);

            // We're not in an error state *yet*.
            return;
        } else {
            // Authentication truly failed.
            Log.e("auth", "Fresh auth token was also rejected");
            explanation = getString(R.string.error_auth_failed);
            Toast.makeText(this, explanation, Toast.LENGTH_LONG).show();
        }
    }

    /**
     * Updates the infotext and host list display.
     */
    private void updateUi() {
        mRefreshButton.setEnabled(mAccount != null);
        if (mAccount != null) {
            mAccountSwitcher.setTitle(mAccount.name);
        }

        if (mHosts == null) {
            mGreeting.setText(getString(R.string.inst_empty_list));
            mList.setAdapter(null);
            return;
        }

        mGreeting.setText(getString(R.string.inst_host_list));

        ArrayAdapter<HostInfo> displayer = new HostListAdapter(this, R.layout.host, mHosts);
        Log.i("hostlist", "About to populate host list display");
        mList.setAdapter(displayer);
    }

    @Override
    public void onConnectionState(JniInterface.ConnectionListener.State state,
            JniInterface.ConnectionListener.Error error) {
        String stateText = getResources().getStringArray(R.array.protoc_states)[state.value()];
        boolean dismissProgress = false;
        switch (state) {
            case INITIALIZING:
            case CONNECTING:
            case AUTHENTICATED:
                // The connection is still being established, so we'll report the current progress.
                if (mProgressIndicator == null) {
                    mProgressIndicator = ProgressDialog.show(this,
                            getString(R.string.progress_title), stateText, true, true,
                            new DialogInterface.OnCancelListener() {
                                @Override
                                public void onCancel(DialogInterface dialog) {
                                    JniInterface.disconnectFromHost();
                                }
                            });
                } else {
                    mProgressIndicator.setMessage(stateText);
                }
                break;

            case CONNECTED:
                dismissProgress = true;
                Toast.makeText(this, stateText, Toast.LENGTH_SHORT).show();

                // Display the remote desktop.
                startActivityForResult(new Intent(this, Desktop.class), 0);
                break;

            case FAILED:
                dismissProgress = true;
                Toast.makeText(this, stateText + ": "
                        + getResources().getStringArray(R.array.protoc_errors)[error.value()],
                        Toast.LENGTH_LONG).show();

                // Close the Desktop view, if it is currently running.
                finishActivity(0);
                break;

            case CLOSED:
                // No need to show toast in this case. Either the connection will have failed
                // because of an error, which will trigger toast already. Or the disconnection will
                // have been initiated by the user.
                dismissProgress = true;
                finishActivity(0);
                break;

            default:
                // Unreachable, but required by Google Java style and findbugs.
                assert false : "Unreached";
        }

        if (dismissProgress && mProgressIndicator != null) {
            mProgressIndicator.dismiss();
            mProgressIndicator = null;
        }
    }
}
