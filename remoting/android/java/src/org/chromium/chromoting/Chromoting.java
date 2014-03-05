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
import android.app.ActionBar;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Settings;
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
        AccountManagerCallback<Bundle>, ActionBar.OnNavigationListener,
        HostListLoader.Callback {
    /** Only accounts of this type will be selectable for authentication. */
    private static final String ACCOUNT_TYPE = "com.google";

    /** Scopes at which the authentication token we request will be valid. */
    private static final String TOKEN_SCOPE = "oauth2:https://www.googleapis.com/auth/chromoting " +
            "https://www.googleapis.com/auth/googletalk";

    /** Web page to be displayed in the Help screen when launched from this activity. */
    private static final String HELP_URL =
            "http://support.google.com/chrome/?p=mobile_crd_hostslist";

    /** User's account details. */
    private Account mAccount;

    /** List of accounts on the system. */
    private Account[] mAccounts;

    /** Account auth token. */
    private String mToken;

    /** Helper for fetching the host list. */
    private HostListLoader mHostListLoader;

    /** List of hosts. */
    private HostInfo[] mHosts;

    /** Refresh button. */
    private MenuItem mRefreshButton;

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
    boolean mTriedNewAuthToken;

    /** Shows a warning explaining that a Google account is required, then closes the activity. */
    private void showNoAccountsDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setMessage(R.string.noaccounts_message);
        builder.setPositiveButton(R.string.noaccounts_add_account,
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                        Intent intent = new Intent(Settings.ACTION_ADD_ACCOUNT);
                        intent.putExtra(Settings.EXTRA_ACCOUNT_TYPES,
                                new String[] { ACCOUNT_TYPE });
                        if (intent.resolveActivity(getPackageManager()) != null) {
                            startActivity(intent);
                        }
                        finish();
                    }
                });
        builder.setNegativeButton(R.string.close, new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int id) {
                    finish();
                }
            });
        builder.setOnCancelListener(new DialogInterface.OnCancelListener() {
                @Override
                public void onCancel(DialogInterface dialog) {
                    finish();
                }
            });

        AlertDialog dialog = builder.create();
        dialog.show();
    }

    /**
     * Called when the activity is first created. Loads the native library and requests an
     * authentication token from the system.
     */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

        mTriedNewAuthToken = false;
        mHostListLoader = new HostListLoader();

        // Get ahold of our view widgets.
        mGreeting = (TextView)findViewById(R.id.hostList_greeting);
        mList = (ListView)findViewById(R.id.hostList_chooser);

        // Bring native components online.
        JniInterface.loadLibrary(this);

        mAccounts = AccountManager.get(this).getAccountsByType(ACCOUNT_TYPE);
        if (mAccounts.length == 0) {
            showNoAccountsDialog();
            return;
        }

        SharedPreferences prefs = getPreferences(MODE_PRIVATE);
        int index = -1;
        if (prefs.contains("account_name") && prefs.contains("account_type")) {
            mAccount = new Account(prefs.getString("account_name", null),
                    prefs.getString("account_type", null));
            index = Arrays.asList(mAccounts).indexOf(mAccount);
        }
        if (index == -1) {
            // Preference not loaded, or does not correspond to a valid account, so just pick the
            // first account arbitrarily.
            index = 0;
            mAccount = mAccounts[0];
        }

        if (mAccounts.length == 1) {
            getActionBar().setDisplayShowTitleEnabled(true);
            getActionBar().setSubtitle(mAccount.name);
        } else {
            AccountsAdapter adapter = new AccountsAdapter(this, mAccounts);
            getActionBar().setNavigationMode(ActionBar.NAVIGATION_MODE_LIST);
            getActionBar().setListNavigationCallbacks(adapter, this);
            getActionBar().setSelectedNavigationItem(index);
        }

        refreshHostList();
    }

    /** Called when the activity is finally finished. */
    @Override
    public void onDestroy() {
        super.onDestroy();
        JniInterface.disconnectFromHost();
    }

    /** Called when the display is rotated (as registered in the manifest). */
    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        // Reload the spinner resources, since the font sizes are dependent on the screen
        // orientation.
        if (mAccounts.length != 1) {
            AccountsAdapter adapter = new AccountsAdapter(this, mAccounts);
            getActionBar().setListNavigationCallbacks(adapter, this);
        }
    }

    /** Called to initialize the action bar. */
    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.chromoting_actionbar, menu);
        mRefreshButton = menu.findItem(R.id.actionbar_directoryrefresh);

        if (mAccount == null) {
            // If there is no account, don't allow the user to refresh the listing.
            mRefreshButton.setEnabled(false);
        }

        return super.onCreateOptionsMenu(menu);
    }

    /** Called whenever an action bar button is pressed. */
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.actionbar_directoryrefresh:
                refreshHostList();
                return true;

            case R.id.actionbar_help:
                {
                    Intent intent = new Intent(this, HelpActivity.class);
                    intent.setData(Uri.parse(HELP_URL));
                    startActivity(intent);
                }
                return true;

            default:
                return super.onOptionsItemSelected(item);
        }
    }

    /** Called when the user taps on a host entry. */
    public void connectToHost(HostInfo host) {
        SessionConnector connector = new SessionConnector(this, this, mHostListLoader);
        connector.connectToHost(mAccount.name, mToken, host);
    }

    private void refreshHostList() {
        mTriedNewAuthToken = false;

        // The refresh button simply makes use of the currently-chosen account.
        AccountManager.get(this).getAuthToken(mAccount, TOKEN_SCOPE, null, this, this, null);
    }

    @Override
    public void run(AccountManagerFuture<Bundle> future) {
        Log.i("auth", "User finished with auth dialogs");
        Bundle result = null;
        String explanation = null;
        try {
            // Here comes our auth token from the Android system.
            result = future.getResult();
            String authToken = result.getString(AccountManager.KEY_AUTHTOKEN);
            Log.i("auth", "Received an auth token from system");

            mToken = authToken;

            mHostListLoader.retrieveHostList(authToken, this);
        } catch (OperationCanceledException ex) {
            // User canceled authentication. No need to report an error.
        } catch (AuthenticatorException ex) {
            explanation = getString(R.string.error_unexpected);
        } catch (IOException ex) {
            explanation = getString(R.string.error_network_error);
        }

        if (result == null) {
            if (explanation != null) {
                Toast.makeText(this, explanation, Toast.LENGTH_LONG).show();
            }
            return;
        }

        String authToken = result.getString(AccountManager.KEY_AUTHTOKEN);
        Log.i("auth", "Received an auth token from system");

        mToken = authToken;

        mHostListLoader.retrieveHostList(authToken, this);
    }

    @Override
    public boolean onNavigationItemSelected(int itemPosition, long itemId) {
        mAccount = mAccounts[itemPosition];

        getPreferences(MODE_PRIVATE).edit().putString("account_name", mAccount.name).
                    putString("account_type", mAccount.type).apply();

        refreshHostList();
        return true;
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
                explanation = getString(R.string.error_network_error);
                break;
            case UNEXPECTED_RESPONSE:
            case SERVICE_UNAVAILABLE:
            case UNKNOWN:
                explanation = getString(R.string.error_unexpected);
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

        if (!mTriedNewAuthToken) {
            // This was our first connection attempt.

            AccountManager authenticator = AccountManager.get(this);
            mTriedNewAuthToken = true;

            Log.w("auth", "Requesting renewal of rejected auth token");
            authenticator.invalidateAuthToken(mAccount.type, mToken);
            mToken = null;
            authenticator.getAuthToken(mAccount, TOKEN_SCOPE, null, this, this, null);

            // We're not in an error state *yet*.
            return;
        } else {
            // Authentication truly failed.
            Log.e("auth", "Fresh auth token was also rejected");
            explanation = getString(R.string.error_authentication_failed);
            Toast.makeText(this, explanation, Toast.LENGTH_LONG).show();
        }
    }

    /**
     * Updates the infotext and host list display.
     */
    private void updateUi() {
        mRefreshButton.setEnabled(mAccount != null);

        if (mHosts == null) {
            mGreeting.setText(getString(R.string.host_list_empty_android));
            mList.setAdapter(null);
            return;
        }

        mGreeting.setText(getString(R.string.mode_me2me));

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
                            getString(R.string.footer_connecting), stateText, true, true,
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
