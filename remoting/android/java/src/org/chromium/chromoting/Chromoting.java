// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.annotation.SuppressLint;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.provider.Settings;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v4.widget.DrawerLayout;
import android.support.v7.app.ActionBarDrawerToggle;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.view.Gravity;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.Toast;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.chromoting.accountswitcher.AccountSwitcher;
import org.chromium.chromoting.accountswitcher.AccountSwitcherFactory;
import org.chromium.chromoting.help.HelpContext;
import org.chromium.chromoting.help.HelpSingleton;
import org.chromium.chromoting.jni.ConnectionListener;
import org.chromium.chromoting.jni.JniInterface;

import java.util.ArrayList;
import java.util.Arrays;

/**
 * The user interface for querying and displaying a user's host list from the directory server. It
 * also requests and renews authentication tokens using the system account manager.
 */
public class Chromoting extends AppCompatActivity implements ConnectionListener,
        AccountSwitcher.Callback, HostListLoader.Callback, View.OnClickListener {
    private static final String TAG = "Chromoting";

    /** Only accounts of this type will be selectable for authentication. */
    private static final String ACCOUNT_TYPE = "com.google";

    /** Result code used for starting {@link DesktopActivity}. */
    public static final int DESKTOP_ACTIVITY = 0;

    /** Result code used for starting {@link CardboardDesktopActivity}. */
    public static final int CARDBOARD_DESKTOP_ACTIVITY = 1;

    /** Preference names for storing selected and recent accounts. */
    private static final String PREFERENCE_SELECTED_ACCOUNT = "account_name";
    private static final String PREFERENCE_RECENT_ACCOUNT_PREFIX = "recent_account_";
    private static final String PREFERENCE_EXPERIMENTAL_FLAGS = "flags";

    /** User's account name (email). */
    private String mAccount;

    /** Account auth token. */
    private String mToken;

    /** Helper for fetching the host list. */
    private HostListLoader mHostListLoader;

    /** List of hosts. */
    private HostInfo[] mHosts = new HostInfo[0];

    /** Refresh button. */
    private MenuItem mRefreshButton;

    /** Host list chooser view shown when at least one host is configured. */
    private ListView mHostListView;

    /** View shown when the user has no configured hosts or host list couldn't be retrieved. */
    private View mEmptyView;

    /** Progress view shown instead of the host list when the host list is loading. */
    private View mProgressView;

    /** Dialog for reporting connection progress. */
    private ProgressDialog mProgressIndicator;

    /**
     * Helper used by SessionConnection for session authentication. Receives onNewIntent()
     * notifications to handle third-party authentication.
     */
    private SessionAuthenticator mAuthenticator;

    /**
     * This is set when receiving an authentication error from the HostListLoader. If that occurs,
     * this flag is set and a fresh authentication token is fetched from the AccountsService, and
     * used to request the host list a second time.
     */
    boolean mTriedNewAuthToken;

    /**
     * Flag to track whether a call to AccountManager.getAuthToken() is currently pending.
     * This avoids infinitely-nested calls in case onStart() gets triggered a second time
     * while a token is being fetched.
     */
    private boolean mWaitingForAuthToken = false;

    private DrawerLayout mDrawerLayout;

    private ActionBarDrawerToggle mDrawerToggle;

    private AccountSwitcher mAccountSwitcher;

    /** Shows a warning explaining that a Google account is required, then closes the activity. */
    private void showNoAccountsDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setMessage(R.string.noaccounts_message);
        builder.setPositiveButton(R.string.noaccounts_add_account,
                new DialogInterface.OnClickListener() {
                    @SuppressLint("InlinedApi")
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
     * Displays the loading indicator. Currently this also hides the host list, but that may
     * change.
     */
    private void showHostListLoadingIndicator() {
        mHostListView.setVisibility(View.GONE);
        mEmptyView.setVisibility(View.GONE);
        mProgressView.setVisibility(View.VISIBLE);
    }

    /**
     * Shows the appropriate view for the host list and hides the loading indicator. Shows either
     * the host list chooser or the host list empty view, depending on whether mHosts contains any
     * hosts.
     */
    private void updateHostListView() {
        mHostListView.setVisibility(mHosts.length == 0 ? View.GONE : View.VISIBLE);
        mEmptyView.setVisibility(mHosts.length == 0 ? View.VISIBLE : View.GONE);
        mProgressView.setVisibility(View.GONE);
    }

    /**
     * Called when the activity is first created. Loads the native library and requests an
     * authentication token from the system.
     */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        mTriedNewAuthToken = false;
        mHostListLoader = new HostListLoader();

        // Get ahold of our view widgets.
        mHostListView = (ListView) findViewById(R.id.hostList_chooser);
        mEmptyView = findViewById(R.id.hostList_empty);
        mHostListView.setOnItemClickListener(
                new AdapterView.OnItemClickListener() {
                    @Override
                    public void onItemClick(AdapterView<?> parent, View view, int position,
                            long id) {
                        onHostClicked(position);
                    }
                });

        mProgressView = findViewById(R.id.hostList_progress);

        findViewById(R.id.host_setup_link_android).setOnClickListener(this);

        mDrawerLayout = (DrawerLayout) findViewById(R.id.drawer_layout);
        mDrawerToggle = new ActionBarDrawerToggle(this, mDrawerLayout, toolbar,
                R.string.open_navigation_drawer, R.string.close_navigation_drawer);
        mDrawerLayout.setDrawerListener(mDrawerToggle);

        // Disable the hamburger icon animation. This is more complex than it ought to be.
        // The animation can be customized by tweaking some style parameters - see
        // http://developer.android.com/reference/android/support/v7/appcompat/R.styleable.html#DrawerArrowToggle .
        // But these can't disable the animation completely.
        // The icon can only be changed by disabling the drawer indicator, which has side-effects
        // that must be worked around. It disables the built-in click listener, so this has to be
        // implemented and added. This also requires that the toolbar be passed to the
        // ActionBarDrawerToggle ctor above (otherwise the listener is ignored and warnings are
        // logged).
        // Also, the animation itself is a private implementation detail - it is not possible to
        // simply access the first frame of the animation. And the hamburger menu icon doesn't
        // exist as a builtin Android resource, so it has to be provided as an application
        // resource instead (R.drawable.ic_menu). And, on Lollipop devices and above, it should be
        // tinted to match the colorControlNormal theme attribute.
        mDrawerToggle.setDrawerIndicatorEnabled(false);
        mDrawerToggle.setToolbarNavigationClickListener(
                new View.OnClickListener() {
                    @Override
                    public void onClick(View view) {
                        if (mDrawerLayout.isDrawerOpen(Gravity.START)) {
                            mDrawerLayout.closeDrawer(Gravity.START);
                        } else {
                            mDrawerLayout.openDrawer(Gravity.START);
                        }
                    }
                });

        // Set the three-line icon instead of the default which is a tinted arrow icon.
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        Drawable menuIcon = ApiCompatibilityUtils.getDrawable(getResources(), R.drawable.ic_menu);
        DrawableCompat.setTint(menuIcon.mutate(),
                ChromotingUtil.getColorAttribute(this, R.attr.colorControlNormal));
        getSupportActionBar().setHomeAsUpIndicator(menuIcon);

        ListView navigationMenu = new ListView(this);
        navigationMenu.setChoiceMode(ListView.CHOICE_MODE_SINGLE);
        navigationMenu.setLayoutParams(new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.MATCH_PARENT));

        String[] navigationMenuItems = new String[] {
            getString(R.string.actionbar_help)
        };
        ArrayAdapter<String> adapter = new ArrayAdapter<String>(this, R.layout.navigation_list_item,
                navigationMenuItems);
        navigationMenu.setAdapter(adapter);
        navigationMenu.setOnItemClickListener(
                new AdapterView.OnItemClickListener() {
                    @Override
                    public void onItemClick(AdapterView<?> parent, View view, int position,
                            long id) {
                        HelpSingleton.getInstance().launchHelp(Chromoting.this,
                                HelpContext.HOST_LIST);
                    }
                });

        mAccountSwitcher = AccountSwitcherFactory.getInstance().createAccountSwitcher(this, this);
        mAccountSwitcher.setNavigation(navigationMenu);
        LinearLayout navigationDrawer = (LinearLayout) findViewById(R.id.navigation_drawer);
        mAccountSwitcher.setDrawer(navigationDrawer);
        View switcherView = mAccountSwitcher.getView();
        switcherView.setLayoutParams(new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT));
        navigationDrawer.addView(switcherView, 0);

        // Bring native components online.
        JniInterface.loadLibrary(this);
    }

    @Override
    protected void onPostCreate(Bundle savedInstanceState) {
        super.onPostCreate(savedInstanceState);
        mDrawerToggle.syncState();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);

        if (mAuthenticator != null) {
            mAuthenticator.onNewIntent(intent);
        }
    }

    /**
     * Called when the activity becomes visible. This happens on initial launch and whenever the
     * user switches to the activity, for example, by using the window-switcher or when coming from
     * the device's lock screen.
     */
    @Override
    public void onStart() {
        super.onStart();

        getSupportActionBar().setTitle(R.string.mode_me2me);

        // Load any previously-selected account and recents from Preferences.
        SharedPreferences prefs = getPreferences(MODE_PRIVATE);

        String selected = prefs.getString(PREFERENCE_SELECTED_ACCOUNT, null);

        ArrayList<String> recents = new ArrayList<String>();
        for (int i = 0;; i++) {
            String prefName = PREFERENCE_RECENT_ACCOUNT_PREFIX + i;
            String recent = prefs.getString(prefName, null);
            if (recent != null) {
                recents.add(recent);
            } else {
                break;
            }
        }

        String[] recentsArray = recents.toArray(new String[recents.size()]);
        mAccountSwitcher.setSelectedAndRecentAccounts(selected, recentsArray);
        mAccountSwitcher.reloadAccounts();
    }

    @Override
    protected void onPause() {
        super.onPause();

        String[] recents = mAccountSwitcher.getRecentAccounts();

        SharedPreferences.Editor preferences = getPreferences(MODE_PRIVATE).edit();
        if (mAccount != null) {
            preferences.putString(PREFERENCE_SELECTED_ACCOUNT, mAccount);
        }

        for (int i = 0; i < recents.length; i++) {
            String prefName = PREFERENCE_RECENT_ACCOUNT_PREFIX + i;
            preferences.putString(prefName, recents[i]);
        }

        preferences.apply();
    }

    /** Called when the activity is finally finished. */
    @Override
    public void onDestroy() {
        super.onDestroy();
        JniInterface.disconnectFromHost();
        mAccountSwitcher.destroy();
    }

    /** Called when a child Activity exits and sends a result back to this Activity. */
    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        mAccountSwitcher.onActivityResult(requestCode, resultCode, data);

        if (requestCode == OAuthTokenFetcher.REQUEST_CODE_RECOVER_FROM_OAUTH_ERROR) {
            if (resultCode == RESULT_OK) {
                // User gave OAuth permission to this app (or recovered from any OAuth failure),
                // so retry fetching the token.
                requestAuthToken(false);
            } else {
                // User denied permission or cancelled the dialog, so cancel the request.
                mWaitingForAuthToken = false;
                updateHostListView();
            }
        }
    }

    /** Called when a permissions request has returned. */
    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions,
            int[] grantResults) {
        // This is currently only used by AccountSwitcherBasic.
        // Check that the user has granted the needed permission, and reload the accounts.
        // Otherwise, assume something unexpected occurred, or the user cancelled the request.
        if (grantResults.length == 1 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            mAccountSwitcher.reloadAccounts();
        } else if (permissions.length == 0) {
            Log.e(TAG, "User cancelled the permission request.");
        } else {
            Log.e(TAG, "Permission %s was not granted.", permissions[0]);
        }
    }

    /** Called when the display is rotated (as registered in the manifest). */
    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        mDrawerToggle.onConfigurationChanged(newConfig);
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

        ChromotingUtil.tintMenuIcons(this, menu);

        return super.onCreateOptionsMenu(menu);
    }

    /** Called whenever an action bar button is pressed. */
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (mDrawerToggle.onOptionsItemSelected(item)) {
            return true;
        }

        int id = item.getItemId();
        if (id == R.id.actionbar_directoryrefresh) {
            refreshHostList();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    /** Called when the user touches hyperlinked text. */
    @Override
    public void onClick(View view) {
        HelpSingleton.getInstance().launchHelp(this, HelpContext.HOST_SETUP);
    }

    /** Called when the user taps on a host entry. */
    private void onHostClicked(int index) {
        HostInfo host = mHosts[index];
        if (host.isOnline) {
            connectToHost(host);
        } else {
            String tooltip = host.getHostOfflineReasonText(this);
            Toast.makeText(this, tooltip, Toast.LENGTH_SHORT).show();
        }
    }

    private void connectToHost(HostInfo host) {
        mProgressIndicator = ProgressDialog.show(
                this,
                host.name,
                getString(R.string.footer_connecting),
                true,
                true,
                new DialogInterface.OnCancelListener() {
                    @Override
                    public void onCancel(DialogInterface dialog) {
                        JniInterface.disconnectFromHost();
                    }
                });
        SessionConnector connector = new SessionConnector(this, this, mHostListLoader);
        mAuthenticator = new SessionAuthenticator(this, host);
        connector.connectToHost(mAccount, mToken, host, mAuthenticator,
                getPreferences(MODE_PRIVATE).getString(PREFERENCE_EXPERIMENTAL_FLAGS, ""));
    }

    private void refreshHostList() {
        if (mWaitingForAuthToken) {
            return;
        }

        mTriedNewAuthToken = false;
        showHostListLoadingIndicator();

        // The refresh button simply makes use of the currently-chosen account.
        requestAuthToken(false);
    }

    private void requestAuthToken(boolean expireCurrentToken) {
        mWaitingForAuthToken = true;

        OAuthTokenFetcher fetcher = new OAuthTokenFetcher(this, mAccount,
                new OAuthTokenFetcher.Callback() {
                    @Override
                    public void onTokenFetched(String token) {
                        mWaitingForAuthToken = false;
                        mToken = token;
                        mHostListLoader.retrieveHostList(mToken, Chromoting.this);
                    }

                    @Override
                    public void onError(int errorResource) {
                        mWaitingForAuthToken = false;
                        updateHostListView();
                        String explanation = getString(errorResource);
                        Toast.makeText(Chromoting.this, explanation, Toast.LENGTH_LONG).show();
                    }
                });

        if (expireCurrentToken) {
            fetcher.clearAndFetch(mToken);
            mToken = null;
        } else {
            fetcher.fetch();
        }
    }

    @Override
    public void onAccountSelected(String accountName) {
        mAccount = accountName;

        // The current host list is no longer valid for the new account, so clear the list.
        mHosts = new HostInfo[0];
        updateUi();
        refreshHostList();
    }

    @Override
    public void onAccountsListEmpty() {
        showNoAccountsDialog();
    }

    @Override
    public void onRequestCloseDrawer() {
        mDrawerLayout.closeDrawers();
    }

    @Override
    public void onHostListReceived(HostInfo[] hosts) {
        // Store a copy of the array, so that it can't be mutated by the HostListLoader. HostInfo
        // is an immutable type, so a shallow copy of the array is sufficient here.
        mHosts = Arrays.copyOf(hosts, hosts.length);
        updateHostListView();
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
            updateHostListView();
            return;
        }

        // This is the AUTH_FAILED case.

        if (!mTriedNewAuthToken) {
            // This was our first connection attempt.
            mTriedNewAuthToken = true;
            requestAuthToken(true);

            // We're not in an error state *yet*.
            return;
        } else {
            // Authentication truly failed.
            Log.e(TAG, "Fresh auth token was rejected.");
            explanation = getString(R.string.error_authentication_failed);
            Toast.makeText(this, explanation, Toast.LENGTH_LONG).show();
            updateHostListView();
        }
    }

    /**
     * Updates the infotext and host list display.
     */
    private void updateUi() {
        if (mRefreshButton != null) {
            mRefreshButton.setEnabled(mAccount != null);
        }
        ArrayAdapter<HostInfo> displayer = new HostListAdapter(this, mHosts);
        mHostListView.setAdapter(displayer);
    }

    @Override
    public void onConnectionState(ConnectionListener.State state, ConnectionListener.Error error) {
        boolean dismissProgress = false;
        switch (state) {
            case INITIALIZING:
            case CONNECTING:
            case AUTHENTICATED:
                // The connection is still being established.
                break;

            case CONNECTED:
                dismissProgress = true;
                // Display the remote desktop.
                startActivityForResult(new Intent(this, Desktop.class), DESKTOP_ACTIVITY);
                break;

            case FAILED:
                dismissProgress = true;
                Toast.makeText(this, getString(error.message()), Toast.LENGTH_LONG).show();
                // Close the Desktop view, if it is currently running.
                finishActivity(DESKTOP_ACTIVITY);
                break;

            case CLOSED:
                // No need to show toast in this case. Either the connection will have failed
                // because of an error, which will trigger toast already. Or the disconnection will
                // have been initiated by the user.
                dismissProgress = true;
                finishActivity(DESKTOP_ACTIVITY);
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
