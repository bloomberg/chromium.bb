// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static android.support.test.espresso.Espresso.openActionBarOverflowOrOptionsMenu;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.action.ViewActions.closeSoftKeyboard;
import static android.support.test.espresso.action.ViewActions.typeText;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.intent.Intents.intended;
import static android.support.test.espresso.intent.Intents.intending;
import static android.support.test.espresso.intent.matcher.BundleMatchers.hasEntry;
import static android.support.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static android.support.test.espresso.intent.matcher.IntentMatchers.hasData;
import static android.support.test.espresso.intent.matcher.IntentMatchers.hasExtras;
import static android.support.test.espresso.intent.matcher.IntentMatchers.hasType;
import static android.support.test.espresso.intent.matcher.UriMatchers.hasHost;
import static android.support.test.espresso.matcher.RootMatchers.withDecorView;
import static android.support.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isEnabled;
import static android.support.test.espresso.matcher.ViewMatchers.isRoot;
import static android.support.test.espresso.matcher.ViewMatchers.withContentDescription;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withParent;
import static android.support.test.espresso.matcher.ViewMatchers.withText;
import static android.text.InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.anyOf;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.nullValue;
import static org.hamcrest.Matchers.sameInstance;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.test.util.ViewUtils.VIEW_GONE;
import static org.chromium.chrome.test.util.ViewUtils.VIEW_INVISIBLE;
import static org.chromium.chrome.test.util.ViewUtils.VIEW_NULL;
import static org.chromium.chrome.test.util.ViewUtils.onViewWaiting;
import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.ColorFilter;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.Espresso;
import android.support.test.espresso.intent.Intents;
import android.support.test.espresso.intent.rule.IntentsTestRule;
import android.support.test.espresso.matcher.BoundedMatcher;
import android.support.test.filters.SmallTest;
import android.view.MenuItem;
import android.view.View;
import android.widget.EditText;
import android.widget.LinearLayout;

import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.appcompat.view.menu.ActionMenuItemView;
import androidx.core.graphics.drawable.DrawableCompat;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.CollectionUtil;
import org.chromium.base.IntStringCallback;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.history.HistoryActivity;
import org.chromium.chrome.browser.history.HistoryManager;
import org.chromium.chrome.browser.history.StubbedHistoryProvider;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.browser_ui.settings.ChromeBaseCheckBoxPreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.sync.ModelType;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.File;
import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.Set;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests for the "Save Passwords" settings screen.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class PasswordSettingsTest {
    private static final long UI_UPDATING_TIMEOUT_MS = 3000;
    @Mock
    private PasswordEditingDelegate mMockPasswordEditingDelegate;

    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Rule
    public IntentsTestRule<HistoryActivity> mHistoryActivityTestRule =
            new IntentsTestRule<>(HistoryActivity.class, false, false);

    @Rule
    public SettingsActivityTestRule<PasswordSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PasswordSettings.class);

    @Rule
    public SettingsActivityTestRule<PasswordEntryEditor> mEditorActivityTestRule =
            new SettingsActivityTestRule<>(PasswordEntryEditor.class);

    private static final class FakePasswordManagerHandler implements PasswordManagerHandler {
        // This class has exactly one observer, set on construction and expected to last at least as
        // long as this object (a good candidate is the owner of this object).
        private final PasswordListObserver mObserver;

        // The faked contents of the password store to be displayed.
        private ArrayList<SavedPasswordEntry> mSavedPasswords = new ArrayList<SavedPasswordEntry>();

        // The faked contents of the saves password exceptions to be displayed.
        private ArrayList<String> mSavedPasswordExeptions = new ArrayList<>();

        // The following three data members are set once {@link #serializePasswords()} is called.
        @Nullable
        private IntStringCallback mExportSuccessCallback;

        @Nullable
        private Callback<String> mExportErrorCallback;

        @Nullable
        private String mExportTargetPath;

        // This is set to the last entry index {@link #showPasswordEntryEditingView()} was called
        // with.
        private int mLastEntryIndex;

        public void setSavedPasswords(ArrayList<SavedPasswordEntry> savedPasswords) {
            mSavedPasswords = savedPasswords;
        }

        public void setSavedPasswordExceptions(ArrayList<String> savedPasswordExceptions) {
            mSavedPasswordExeptions = savedPasswordExceptions;
        }

        public IntStringCallback getExportSuccessCallback() {
            return mExportSuccessCallback;
        }

        public Callback<String> getExportErrorCallback() {
            return mExportErrorCallback;
        }

        public String getExportTargetPath() {
            return mExportTargetPath;
        }

        public int getLastEntryIndex() {
            return mLastEntryIndex;
        }

        /**
         * Constructor.
         * @param PasswordListObserver The only observer.
         */
        public FakePasswordManagerHandler(PasswordListObserver observer) {
            mObserver = observer;
        }

        // Pretends that the updated lists are |mSavedPasswords| for the saved passwords and an
        // empty list for exceptions and immediately calls the observer.
        @Override
        public void updatePasswordLists() {
            mObserver.passwordListAvailable(mSavedPasswords.size());
            mObserver.passwordExceptionListAvailable(mSavedPasswordExeptions.size());
        }

        @Override
        public SavedPasswordEntry getSavedPasswordEntry(int index) {
            return mSavedPasswords.get(index);
        }

        @Override
        public String getSavedPasswordException(int index) {
            return mSavedPasswordExeptions.get(index);
        }

        @Override
        public void removeSavedPasswordEntry(int index) {
            assert false : "Define this method before starting to use it in tests.";
        }

        @Override
        public void removeSavedPasswordException(int index) {
            assert false : "Define this method before starting to use it in tests.";
        }

        @Override
        public void serializePasswords(String targetPath, IntStringCallback successCallback,
                Callback<String> errorCallback) {
            mExportSuccessCallback = successCallback;
            mExportErrorCallback = errorCallback;
            mExportTargetPath = targetPath;
        }

        @Override
        public void showPasswordEntryEditingView(Context context, int index) {
            mLastEntryIndex = index;
            Bundle fragmentArgs = new Bundle();
            fragmentArgs.putString(
                    PasswordEntryEditor.CREDENTIAL_URL, getSavedPasswordEntry(index).getUrl());
            fragmentArgs.putString(PasswordEntryEditor.CREDENTIAL_NAME,
                    getSavedPasswordEntry(index).getUserName());
            fragmentArgs.putString(PasswordEntryEditor.CREDENTIAL_PASSWORD,
                    getSavedPasswordEntry(index).getPassword());
            SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
            settingsLauncher.launchSettingsActivity(
                    context, PasswordEntryEditor.class, fragmentArgs);
        }
    }

    private static final SavedPasswordEntry ZEUS_ON_EARTH =
            new SavedPasswordEntry("http://www.phoenicia.gr", "Zeus", "Europa");
    private static final SavedPasswordEntry ARES_AT_OLYMP =
            new SavedPasswordEntry("https://1-of-12.olymp.gr", "Ares", "God-o'w@r");
    private static final SavedPasswordEntry PHOBOS_AT_OLYMP =
            new SavedPasswordEntry("https://visitor.olymp.gr", "Phobos-son-of-ares", "G0d0fF34r");
    private static final SavedPasswordEntry DEIMOS_AT_OLYMP =
            new SavedPasswordEntry("https://visitor.olymp.gr", "Deimops-Ares-son", "G0d0fT3rr0r");
    private static final SavedPasswordEntry HADES_AT_UNDERWORLD =
            new SavedPasswordEntry("https://underworld.gr", "", "C3rb3rus");
    private static final SavedPasswordEntry[] GREEK_GODS = {
            ZEUS_ON_EARTH,
            ARES_AT_OLYMP,
            PHOBOS_AT_OLYMP,
            DEIMOS_AT_OLYMP,
            HADES_AT_UNDERWORLD,
    };

    // Used to provide fake lists of stored passwords. Tests which need it can use setPasswordSource
    // to instantiate it.
    FakePasswordManagerHandler mHandler;

    /**
     * Delayer controling hiding the progress bar during exporting passwords. This replaces a time
     * delay used in production.
     */
    private final ManualCallbackDelayer mManualDelayer = new ManualCallbackDelayer();

    public PasswordSettingsTest() {
        MockitoAnnotations.initMocks(this);
    }

    private void overrideProfileSyncService(
            final boolean usingPassphrase, final boolean syncingPasswords) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ProfileSyncService.overrideForTests(new ProfileSyncService() {
                @Override
                public boolean isUsingSecondaryPassphrase() {
                    return usingPassphrase;
                }

                @Override
                public Set<Integer> getActiveDataTypes() {
                    if (syncingPasswords) return CollectionUtil.newHashSet(ModelType.PASSWORDS);
                    return CollectionUtil.newHashSet(ModelType.AUTOFILL);
                }
            });
        });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> ProfileSyncService.resetForTests());
    }

    /**
     * Helper to set up a fake source of displayed passwords.
     * @param entry An entry to be added to saved passwords. Can be null.
     */
    private void setPasswordSource(SavedPasswordEntry entry) {
        SavedPasswordEntry[] entries = {};
        if (entry != null) {
            entries = new SavedPasswordEntry[] {entry};
        }
        setPasswordSourceWithMultipleEntries(entries);
    }

    /**
     * Helper to set up a fake source of displayed passwords with multiple initial passwords.
     * @param initialEntries All entries to be added to saved passwords. Can not be null.
     */
    private void setPasswordSourceWithMultipleEntries(SavedPasswordEntry[] initialEntries) {
        if (mHandler == null) {
            mHandler = new FakePasswordManagerHandler(PasswordManagerHandlerProvider.getInstance());
        }
        ArrayList<SavedPasswordEntry> entries = new ArrayList<>(Arrays.asList(initialEntries));
        mHandler.setSavedPasswords(entries);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> PasswordManagerHandlerProvider.getInstance()
                                   .setPasswordManagerHandlerForTest(mHandler));
    }

    /**
     * Helper to set up a fake source of displayed passwords without passwords but with exceptions.
     * @param exceptions All exceptions to be added to saved exceptions. Can not be null.
     */
    private void setPasswordExceptions(String[] exceptions) {
        if (mHandler == null) {
            mHandler = new FakePasswordManagerHandler(PasswordManagerHandlerProvider.getInstance());
        }
        mHandler.setSavedPasswordExceptions(new ArrayList<>(Arrays.asList(exceptions)));
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> PasswordManagerHandlerProvider.getInstance()
                                   .setPasswordManagerHandlerForTest(mHandler));
    }

    /**
     * Matches any {@link EditText} which has the content visibility matching to |shouldBeVisible|.
     * @return The matcher checking the input type.
     */
    private static Matcher<View> isVisiblePasswordInput(final boolean shouldBeVisible) {
        return new BoundedMatcher<View, EditText>(EditText.class) {
            @Override
            public boolean matchesSafely(EditText editText) {
                return ((editText.getInputType() & TYPE_TEXT_VARIATION_VISIBLE_PASSWORD)
                               == TYPE_TEXT_VARIATION_VISIBLE_PASSWORD)
                        == shouldBeVisible;
            }

            @Override
            public void describeTo(Description description) {
                if (shouldBeVisible) {
                    description.appendText("The content should be visible.");
                } else {
                    description.appendText("The content should not be visible.");
                }
            }
        };
    }

    /**
     * Looks for the icon by id. If it cannot be found, it's probably hidden in the overflow
     * menu. In that case, open the menu and search for its title.
     * @return Returns either the icon button or the menu option.
     */
    public static Matcher<View> withMenuIdOrText(@IdRes int actionId, @StringRes int actionLabel) {
        Matcher<View> matcher = withId(actionId);
        try {
            Espresso.onView(matcher).check(matches(isDisplayed()));
            return matcher;
        } catch (Exception NoMatchingViewException) {
            openActionBarOverflowOrOptionsMenu(
                    InstrumentationRegistry.getInstrumentation().getTargetContext());
            return withText(actionLabel);
        }
    }

    /**
     * Looks for the search icon by id or by its title.
     * @return Returns either the icon button or the menu option.
     */
    public static Matcher<View> withSearchMenuIdOrText() {
        return withMenuIdOrText(R.id.menu_id_search, R.string.search);
    }

    /**
     * Looks for the edit saved password icon by id or by its title.
     * @return Returns either the icon button or the menu option.
     */
    public static Matcher<View> withEditMenuIdOrText() {
        return withMenuIdOrText(R.id.action_edit_saved_password,
                R.string.password_entry_viewer_edit_stored_password_action_title);
    }

    /**
     * Looks for the save edited password icon by id or by its title.
     * @return Returns either the icon button or the menu option.
     */
    public static Matcher<View> withSaveMenuIdOrText() {
        return withMenuIdOrText(R.id.action_save_edited_password, R.string.save);
    }

    /**
     * Taps the menu item to trigger exporting and ensures that reauthentication passes.
     * It also disables the timer in {@link DialogManager} which is used to allow hiding the
     * progress bar after an initial period. Hiding can be later allowed manually in tests with
     * {@link #allowProgressBarToBeHidden}, to avoid time-dependent flakiness.
     */
    private void reauthenticateAndRequestExport(SettingsActivity settingsActivity) {
        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());

        // Avoid launching the Android-provided reauthentication challenge, which cannot be
        // completed in the test.
        ReauthenticationManager.setSkipSystemReauth(true);
        Espresso.onView(withText(R.string.password_settings_export_action_title)).perform(click());

        // Now Chrome thinks it triggered the challenge and is waiting to be resumed. Once resumed
        // it will check the reauthentication result. First, update the reauth timestamp to indicate
        // a successful reauth:
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthenticationManager.ReauthScope.BULK);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Disable the timer for progress bar.
            PasswordSettings fragment = mSettingsActivityTestRule.getFragment();
            fragment.getExportFlowForTesting()
                    .getDialogManagerForTesting()
                    .replaceCallbackDelayerForTesting(mManualDelayer);
            // Now call onResume to nudge Chrome into continuing the export flow.
            settingsActivity.getMainFragment().onResume();
        });
    }

    @IntDef({MenuItemState.DISABLED, MenuItemState.ENABLED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface MenuItemState {
        /** Represents the state of an enabled menu item. */
        int DISABLED = 0;
        /** Represents the state of a disabled menu item. */
        int ENABLED = 1;
    }

    /**
     * Checks that the menu item for exporting passwords is enabled or disabled as expected.
     * @param expectedState The expected state of the menu item.
     */
    private void checkExportMenuItemState(@MenuItemState int expectedState) {
        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        final Matcher<View> stateMatcher =
                expectedState == MenuItemState.ENABLED ? isEnabled() : not(isEnabled());
        // The text matches a text view, but the disabled entity is some wrapper two levels up in
        // the view hierarchy, hence the two withParent matchers.
        Espresso.onView(allOf(withText(R.string.password_settings_export_action_title),
                                withParent(withParent(withParent(stateMatcher)))))
                .check(matches(isDisplayed()));
    }

    /** Requests showing an arbitrary password export error. */
    private void requestShowingExportError() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mHandler.getExportErrorCallback().onResult("Arbitrary error"); });
    }

    /**
     * Requests showing an arbitrary password export error with a particular positive button to be
     * shown. If you don't care about the button, just call {@link #requestShowingExportError}.
     * @param settingsActivity is the PasswordSettings instance being tested.
     * @param positiveButtonLabelId controls which label the positive button ends up having.
     */
    private void requestShowingExportErrorWithButton(
            SettingsActivity settingsActivity, int positiveButtonLabelId) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PasswordSettings fragment = mSettingsActivityTestRule.getFragment();
            // To show an error, the error type for UMA needs to be specified. Because it is not
            // relevant for cases when the error is forcibly displayed in tests,
            // HistogramExportResult.NO_CONSUMER is passed as an arbitrarily chosen value.
            fragment.getExportFlowForTesting().showExportErrorAndAbort(
                    R.string.password_settings_export_no_app, null, positiveButtonLabelId,
                    ExportFlow.HistogramExportResult.NO_CONSUMER);
        });
    }

    /**
     * Sends the signal to {@link DialogManager} that the minimal time for showing the progress
     * bar has passed. This results in the progress bar getting hidden as soon as requested.
     */
    private void allowProgressBarToBeHidden(SettingsActivity settingsActivity) {
        TestThreadUtils.runOnUiThreadBlocking(mManualDelayer::runCallbacksSynchronously);
    }
    /**
     * Call after activity.finish() to wait for the wrap up to complete. If it was already completed
     * or could be finished within |timeout_ms|, stop waiting anyways.
     * @param activity The activity to wait for.
     * @param timeout The timeout in ms after which the waiting will end anyways.
     * @throws InterruptedException
     */
    private void waitToFinish(Activity activity, long timeout) throws InterruptedException {
        long start_time = System.currentTimeMillis();
        while (activity.isFinishing() && (System.currentTimeMillis() - start_time < timeout)) {
            Thread.sleep(100);
        }
    }

    /**
     * Create a temporary file in the cache sub-directory for exported passwords, which the test can
     * try to use for sharing.
     * @return The {@link File} handle for such temporary file.
     */
    private File createFakeExportedPasswordsFile() throws IOException {
        File passwordsDir = new File(ExportFlow.getTargetDirectory());
        // Ensure that the directory exists.
        passwordsDir.mkdir();
        return File.createTempFile("test", ".csv", passwordsDir);
    }

    /**
     * Ensure that resetting of empty passwords list works.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testResetListEmpty() {
        // Load the preferences, they should show the empty list.
        mSettingsActivityTestRule.startSettingsActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PasswordSettings savePasswordPreferences = mSettingsActivityTestRule.getFragment();
            // Emulate an update from PasswordStore. This should not crash.
            savePasswordPreferences.passwordListAvailable(0);
        });
    }

    /**
     * Ensure that the on/off switch in "Save Passwords" settings actually enables and disables
     * password saving.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSavePasswordsSwitch() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefServiceBridge.getInstance().setBoolean(Pref.REMEMBER_PASSWORDS_ENABLED, true);
        });

        final SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PasswordSettings savedPasswordPrefs = mSettingsActivityTestRule.getFragment();
            ChromeSwitchPreference onOffSwitch =
                    (ChromeSwitchPreference) savedPasswordPrefs.findPreference(
                            PasswordSettings.PREF_SAVE_PASSWORDS_SWITCH);
            Assert.assertTrue(onOffSwitch.isChecked());

            onOffSwitch.performClick();
            Assert.assertFalse(
                    PrefServiceBridge.getInstance().getBoolean(Pref.REMEMBER_PASSWORDS_ENABLED));
            onOffSwitch.performClick();
            Assert.assertTrue(
                    PrefServiceBridge.getInstance().getBoolean(Pref.REMEMBER_PASSWORDS_ENABLED));

            settingsActivity.finish();

            PrefServiceBridge.getInstance().setBoolean(Pref.REMEMBER_PASSWORDS_ENABLED, false);
        });

        mSettingsActivityTestRule.startSettingsActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PasswordSettings savedPasswordPrefs = mSettingsActivityTestRule.getFragment();
            ChromeSwitchPreference onOffSwitch =
                    (ChromeSwitchPreference) savedPasswordPrefs.findPreference(
                            PasswordSettings.PREF_SAVE_PASSWORDS_SWITCH);
            Assert.assertFalse(onOffSwitch.isChecked());
        });
    }

    /**
     *  Tests that the link pointing to managing passwords in the user's account is not displayed
     *  for non signed in users.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testManageAccountLinkNotSignedIn() {
        // Add a password entry, because the link is only displayed if the password list is not
        // empty.
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));
        mSettingsActivityTestRule.startSettingsActivity();
        PasswordSettings savedPasswordPrefs = mSettingsActivityTestRule.getFragment();
        Assert.assertNull(
                savedPasswordPrefs.findPreference(PasswordSettings.PREF_KEY_MANAGE_ACCOUNT_LINK));
    }

    /**
     *  Tests that the link pointing to managing passwords in the user's account is not displayed
     *  for signed in users, not syncing passwords.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testManageAccountLinkSignedInNotSyncing() {
        // Add a password entry, because the link is only displayed if the password list is not
        // empty.
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));
        overrideProfileSyncService(false, false);
        SigninTestUtil.addAndSignInTestAccount();

        mSettingsActivityTestRule.startSettingsActivity();
        PasswordSettings savedPasswordPrefs = mSettingsActivityTestRule.getFragment();

        Assert.assertNull(
                savedPasswordPrefs.findPreference(PasswordSettings.PREF_KEY_MANAGE_ACCOUNT_LINK));
    }

    /**
     *  Tests that the link pointing to managing passwords in the user's account is displayed for
     *  users syncing passwords.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testManageAccountLinkSyncing() {
        // Add a password entry, because the link is only displayed if the password list is not
        // empty.
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));
        overrideProfileSyncService(false, true);
        SigninTestUtil.addAndSignInTestAccount();

        mSettingsActivityTestRule.startSettingsActivity();
        PasswordSettings savedPasswordPrefs = mSettingsActivityTestRule.getFragment();

        Assert.assertNotNull(
                savedPasswordPrefs.findPreference(PasswordSettings.PREF_KEY_MANAGE_ACCOUNT_LINK));
    }

    /**
     *  Tests that the link pointing to managing passwords in the user's account is not displayed
     *  for users syncing passwords with custom passphrase.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testManageAccountLinkSyncingWithPassphrase() {
        // Add a password entry, because the link is only displayed if the password list is not
        // empty.
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));
        overrideProfileSyncService(true, true);
        SigninTestUtil.addAndSignInTestAccount();

        mSettingsActivityTestRule.startSettingsActivity();
        PasswordSettings savedPasswordPrefs = mSettingsActivityTestRule.getFragment();

        Assert.assertNull(
                savedPasswordPrefs.findPreference(PasswordSettings.PREF_KEY_MANAGE_ACCOUNT_LINK));
    }

    /**
     * Ensure that the "Auto Sign-in" switch in "Save Passwords" settings actually enables and
     * disables auto sign-in.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAutoSignInCheckbox() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefServiceBridge.getInstance().setBoolean(
                    Pref.PASSWORD_MANAGER_AUTO_SIGNIN_ENABLED, true);
        });

        final SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PasswordSettings passwordPrefs = mSettingsActivityTestRule.getFragment();
            ChromeBaseCheckBoxPreference onOffSwitch =
                    (ChromeBaseCheckBoxPreference) passwordPrefs.findPreference(
                            PasswordSettings.PREF_AUTOSIGNIN_SWITCH);
            Assert.assertTrue(onOffSwitch.isChecked());

            onOffSwitch.performClick();
            Assert.assertFalse(PrefServiceBridge.getInstance().getBoolean(
                    Pref.PASSWORD_MANAGER_AUTO_SIGNIN_ENABLED));
            onOffSwitch.performClick();
            Assert.assertTrue(PrefServiceBridge.getInstance().getBoolean(
                    Pref.PASSWORD_MANAGER_AUTO_SIGNIN_ENABLED));

            settingsActivity.finish();

            PrefServiceBridge.getInstance().setBoolean(
                    Pref.PASSWORD_MANAGER_AUTO_SIGNIN_ENABLED, false);
        });

        mSettingsActivityTestRule.startSettingsActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PasswordSettings passwordPrefs = mSettingsActivityTestRule.getFragment();
            ChromeBaseCheckBoxPreference onOffSwitch =
                    (ChromeBaseCheckBoxPreference) passwordPrefs.findPreference(
                            PasswordSettings.PREF_AUTOSIGNIN_SWITCH);
            Assert.assertFalse(onOffSwitch.isChecked());
        });
    }

    /**
     * Check that {@link #showPasswordEntryEditingView()} was called with the index matching the one
     * of the password that was clicked.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Features.EnableFeatures(ChromeFeatureList.PASSWORD_EDITING_ANDROID)
    public void testSelectedStoredPasswordIndexIsSameAsInShowPasswordEntryEditingView() {
        PasswordEditingDelegateProvider.getInstance().setPasswordEditingDelegate(
                mMockPasswordEditingDelegate);
        setPasswordSourceWithMultipleEntries( // Initialize preferences
                new SavedPasswordEntry[] {new SavedPasswordEntry("https://example.com",
                                                  "example user", "example password"),
                        new SavedPasswordEntry("https://test.com", "test user", "test password")});

        mSettingsActivityTestRule.startSettingsActivity();

        Espresso.onView(withText(containsString("test user"))).perform(click());

        Espresso.onView(withEditMenuIdOrText()).perform(click());

        Assert.assertEquals(mHandler.getLastEntryIndex(), 1);
    }

    /**
     * Check that the password editing activity displays the data received through arguments.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Features.EnableFeatures(ChromeFeatureList.PASSWORD_EDITING_ANDROID)
    public void testPasswordDataDisplayedInEditingActivity() {
        PasswordEditingDelegateProvider.getInstance().setPasswordEditingDelegate(
                mMockPasswordEditingDelegate);
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(PasswordEntryEditor.CREDENTIAL_URL, "https://example.com");
        fragmentArgs.putString(PasswordEntryEditor.CREDENTIAL_NAME, "test user");
        fragmentArgs.putString(PasswordEntryEditor.CREDENTIAL_PASSWORD, "test password");
        mEditorActivityTestRule.startSettingsActivity(fragmentArgs);

        Espresso.onView(withId(R.id.site_edit)).check(matches(withText("https://example.com")));
        Espresso.onView(withId(R.id.username_edit)).check(matches(withText("test user")));
        Espresso.onView(withId(R.id.password_edit)).check(matches(withText("test password")));
    }

    /**
     * Check that the password editing method from the PasswordEditingDelegate was called when the
     * save button in the password editing activity was clicked.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Features.EnableFeatures(ChromeFeatureList.PASSWORD_EDITING_ANDROID)
    public void testPasswordEditingMethodWasCalled() throws Exception {
        PasswordEditingDelegateProvider.getInstance().setPasswordEditingDelegate(
                mMockPasswordEditingDelegate);
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        mSettingsActivityTestRule.startSettingsActivity();

        Espresso.onView(withText(containsString("test user"))).perform(click());

        Espresso.onView(withEditMenuIdOrText()).perform(click());

        Espresso.onView(withId(R.id.username_edit)).perform(typeText(" new"));

        Espresso.onView(withSaveMenuIdOrText()).perform(click());

        verify(mMockPasswordEditingDelegate).editSavedPasswordEntry("test user new", "password");

        // Verify that the delegate was destroyed when the password editing activity finished.
        waitForEvent().destroy();
    }

    /**
     * Check that the changes of password data are shown in the password viewing activity and in the
     * list of passwords after the save button was clicked.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Features.EnableFeatures(ChromeFeatureList.PASSWORD_EDITING_ANDROID)
    public void testChangeOfStoredPasswordDataIsPropagated() throws Exception {
        PasswordEditingDelegateProvider.getInstance().setPasswordEditingDelegate(
                mMockPasswordEditingDelegate);
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        mSettingsActivityTestRule.startSettingsActivity();

        Espresso.onView(withText(containsString("test user"))).perform(click());

        Espresso.onView(withEditMenuIdOrText()).perform(click());

        // Performing a change of saved credentials.
        mHandler.mSavedPasswords.set(
                0, new SavedPasswordEntry("https://example.com", "test user new", "password"));

        Espresso.onView(withSaveMenuIdOrText()).perform(click());

        // Check if the password viewing activity has the updated data.
        Espresso.onView(withText("test user new")).check(matches(isDisplayed()));

        Espresso.pressBack();
        // Check if the password preferences activity has the updated data in the list of passwords.
        Espresso.onView(withText("test user new")).check(matches(isDisplayed()));
    }

    /**
     * Check that the stored password is visible after clicking the unmasking icon and invisible
     * after another click.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Features.EnableFeatures(ChromeFeatureList.PASSWORD_EDITING_ANDROID)
    public void testStoredPasswordCanBeUnmaskedAndMaskedAgain() {
        PasswordEditingDelegateProvider.getInstance().setPasswordEditingDelegate(
                mMockPasswordEditingDelegate);
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(PasswordSettings.PASSWORD_LIST_NAME, "test user");
        fragmentArgs.putString(PasswordSettings.PASSWORD_LIST_URL, "https://example.com");
        fragmentArgs.putString(PasswordSettings.PASSWORD_LIST_PASSWORD, "test password");
        mEditorActivityTestRule.startSettingsActivity(fragmentArgs);

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthenticationManager.ReauthScope.BULK);

        Espresso.onView(withId(R.id.password_entry_editor_view_password)).perform(click());

        Espresso.onView(withId(R.id.password_edit)).check(matches(isVisiblePasswordInput(true)));

        Espresso.onView(withId(R.id.password_entry_editor_view_password)).perform(click());

        Espresso.onView(withId(R.id.password_edit)).check(matches(isVisiblePasswordInput(false)));
    }

    /**
     * Check that if there are no saved passwords, the export menu item is disabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportMenuDisabled() {
        // Ensure there are no saved passwords reported to settings.
        setPasswordSource(null);

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);

        mSettingsActivityTestRule.startSettingsActivity();

        checkExportMenuItemState(MenuItemState.DISABLED);
    }

    /**
     * Check that if there are saved passwords, the export menu item is enabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportMenuEnabled() {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);

        mSettingsActivityTestRule.startSettingsActivity();

        checkExportMenuItemState(MenuItemState.ENABLED);
    }

    /**
     * Check that tapping the export menu requests the passwords to be serialised in the background.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportTriggersSerialization() {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        mSettingsActivityTestRule.startSettingsActivity();

        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        // Before tapping the menu item for export, pretend that the last successful
        // reauthentication just happened. This will allow the export flow to continue.
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthenticationManager.ReauthScope.BULK);
        Espresso.onView(withText(R.string.password_settings_export_action_title)).perform(click());

        Assert.assertNotNull(mHandler.getExportTargetPath());
        Assert.assertFalse(mHandler.getExportTargetPath().isEmpty());
    }

    /**
     * Check that the export menu item is included and hidden behind the overflow menu. Check that
     * the menu displays the warning before letting the user export passwords.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportMenuItem() {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();

        reauthenticateAndRequestExport(settingsActivity);

        // Check that the warning dialog is displayed.
        Espresso.onView(withText(R.string.settings_passwords_export_description))
                .check(matches(isDisplayed()));
    }

    /**
     * Check that if export is canceled by the user after a successful reauthentication, then
     * re-triggering the export and failing the second reauthentication aborts the export as well.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportReauthAfterCancel() {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();

        reauthenticateAndRequestExport(settingsActivity);

        // Hit the Cancel button on the warning dialog to cancel the flow.
        Espresso.onView(withText(R.string.cancel)).perform(click());

        // Now repeat the steps almost like in |reauthenticateAndRequestExport| but simulate failing
        // the reauthentication challenge.
        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());

        // Avoid launching the Android-provided reauthentication challenge, which cannot be
        // completed in the test.
        ReauthenticationManager.setSkipSystemReauth(true);
        Espresso.onView(withText(R.string.password_settings_export_action_title)).perform(click());

        // Now Chrome thinks it triggered the challenge and is waiting to be resumed. Once resumed
        // it will check the reauthentication result. First, update the reauth timestamp to indicate
        // a cancelled reauth:
        ReauthenticationManager.resetLastReauth();

        // Now call onResume to nudge Chrome into continuing the export flow.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { settingsActivity.getMainFragment().onResume(); });

        // Check that the warning dialog is not displayed.
        Espresso.onView(withText(R.string.settings_passwords_export_description))
                .check(doesNotExist());

        // Check that the export menu item is enabled, because the current export was cancelled.
        checkExportMenuItemState(MenuItemState.ENABLED);
    }

    /**
     * Check whether the user is asked to set up a screen lock if attempting to export passwords.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportMenuItemNoLock() {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.UNAVAILABLE);

        final SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();

        View mainDecorView = settingsActivity.getWindow().getDecorView();
        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        Espresso.onView(withText(R.string.password_settings_export_action_title)).perform(click());
        Espresso.onView(withText(R.string.password_export_set_lock_screen))
                .inRoot(withDecorView(not(is(mainDecorView))))
                .check(matches(isDisplayed()));
    }

    /**
     * Check that if exporting is cancelled for the absence of the screen lock, the menu item is
     * enabled for a retry.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportMenuItemReenabledNoLock() {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.UNAVAILABLE);

        mSettingsActivityTestRule.startSettingsActivity();

        // Trigger exporting and let it fail on the unavailable lock.
        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        Espresso.onView(withText(R.string.password_settings_export_action_title)).perform(click());

        // Check that for re-triggering, the export menu item is enabled.
        checkExportMenuItemState(MenuItemState.ENABLED);
    }

    /**
     * Check that if exporting is cancelled for the user's failure to reauthenticate, the menu item
     * is enabled for a retry.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportMenuItemReenabledReauthFailure() {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setSkipSystemReauth(true);

        final SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();

        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        Espresso.onView(withText(R.string.password_settings_export_action_title)).perform(click());
        // The reauthentication dialog is skipped and the last reauthentication timestamp is not
        // reset. This looks like a failed reauthentication to PasswordSettings' onResume.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { settingsActivity.getMainFragment().onResume(); });
        checkExportMenuItemState(MenuItemState.ENABLED);
    }

    /**
     * Check that the export always requires a reauthentication, even if the last one happened
     * recently.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportRequiresReauth() {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        mSettingsActivityTestRule.startSettingsActivity();

        // Ensure that the last reauthentication time stamp is recent enough.
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthenticationManager.ReauthScope.BULK);

        // Start export.
        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());

        // Avoid launching the Android-provided reauthentication challenge, which cannot be
        // completed in the test.
        ReauthenticationManager.setSkipSystemReauth(true);
        Espresso.onView(withText(R.string.password_settings_export_action_title)).perform(click());

        // Check that Chrome indeed issued an (ignored) request to reauthenticate the user rather
        // than re-using the recent reauthentication, by observing that the next step in the flow
        // (progress bar) is not shown.
        Espresso.onView(withText(R.string.settings_passwords_preparing_export))
                .check(doesNotExist());
    }

    /**
     * Check that the export flow ends up with sending off a share intent with the exported
     * passwords.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportIntent() throws Exception {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();

        Intents.init();

        reauthenticateAndRequestExport(settingsActivity);
        File tempFile = createFakeExportedPasswordsFile();
        // Pretend that passwords have been serialized to go directly to the intent.
        mHandler.getExportSuccessCallback().onResult(123, tempFile.getPath());

        // Before triggering the sharing intent chooser, stub it out to avoid leaving system UI open
        // after the test is finished.
        intending(hasAction(equalTo(Intent.ACTION_CHOOSER)))
                .respondWith(new Instrumentation.ActivityResult(Activity.RESULT_OK, null));

        // Confirm the export warning to fire the sharing intent.
        Espresso.onView(withText(R.string.password_settings_export_action_title)).perform(click());

        intended(allOf(hasAction(equalTo(Intent.ACTION_CHOOSER)),
                hasExtras(hasEntry(equalTo(Intent.EXTRA_INTENT),
                        allOf(hasAction(equalTo(Intent.ACTION_SEND)), hasType("text/csv"))))));

        Intents.release();

        tempFile.delete();
    }

    /**
     * Check that the export flow ends up with sending off a share intent with the exported
     * passwords, even if the flow gets interrupted by pausing Chrome.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportIntentPaused() throws Exception {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();

        Intents.init();

        reauthenticateAndRequestExport(settingsActivity);

        // Call onResume to simulate that the user put Chrome into background by opening "recent
        // apps" and then restored Chrome by choosing it from the list.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { settingsActivity.getMainFragment().onResume(); });

        File tempFile = createFakeExportedPasswordsFile();
        // Pretend that passwords have been serialized to go directly to the intent.
        mHandler.getExportSuccessCallback().onResult(56, tempFile.getPath());

        // Before triggering the sharing intent chooser, stub it out to avoid leaving system UI open
        // after the test is finished.
        intending(hasAction(equalTo(Intent.ACTION_CHOOSER)))
                .respondWith(new Instrumentation.ActivityResult(Activity.RESULT_OK, null));

        // Confirm the export warning to fire the sharing intent.
        Espresso.onView(withText(R.string.password_settings_export_action_title)).perform(click());

        intended(allOf(hasAction(equalTo(Intent.ACTION_CHOOSER)),
                hasExtras(hasEntry(equalTo(Intent.EXTRA_INTENT),
                        allOf(hasAction(equalTo(Intent.ACTION_SEND)), hasType("text/csv"))))));

        Intents.release();

        tempFile.delete();
    }

    /**
     * Check that the export flow can be canceled in the warning dialogue and that upon cancellation
     * the export menu item gets re-enabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportCancelOnWarning() {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();

        reauthenticateAndRequestExport(settingsActivity);

        // Cancel the export warning.
        Espresso.onView(withText(R.string.cancel)).perform(click());

        // Check that the cancellation succeeded by checking that the export menu is available and
        // enabled.
        checkExportMenuItemState(MenuItemState.ENABLED);
    }

    /**
     * Check that the export warning is not duplicated when onResume is called on the settings.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportWarningOnResume() {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();

        reauthenticateAndRequestExport(settingsActivity);

        // Call onResume to simulate that the user put Chrome into background by opening "recent
        // apps" and then restored Chrome by choosing it from the list.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { settingsActivity.getMainFragment().onResume(); });

        // Cancel the export warning.
        Espresso.onView(withText(R.string.cancel)).perform(click());

        // Check that export warning is not visible again.
        Espresso.onView(withText(R.string.cancel)).check(doesNotExist());

        // Check that the cancellation succeeded by checking that the export menu is available and
        // enabled.
        checkExportMenuItemState(MenuItemState.ENABLED);
    }

    /**
     * Check that the export warning is dismissed after onResume if the last reauthentication
     * happened too long ago.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportWarningTimeoutOnResume() {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();

        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());

        // Before exporting, pretend that the last successful reauthentication happend too long ago.
        ReauthenticationManager.recordLastReauth(System.currentTimeMillis()
                        - ReauthenticationManager.VALID_REAUTHENTICATION_TIME_INTERVAL_MILLIS - 1,
                ReauthenticationManager.ReauthScope.BULK);

        Espresso.onView(withText(R.string.password_settings_export_action_title)).perform(click());

        // Call onResume to simulate that the user put Chrome into background by opening "recent
        // apps" and then restored Chrome by choosing it from the list.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { settingsActivity.getMainFragment().onResume(); });

        // Check that export warning is not visible again.
        Espresso.onView(withText(R.string.cancel)).check(doesNotExist());

        // Check that the export flow was cancelled automatically by checking that the export menu
        // is available and enabled.
        checkExportMenuItemState(MenuItemState.ENABLED);
    }

    /**
     * Check that the export flow can be canceled by dismissing the warning dialogue (tapping
     * outside of it, as opposed to tapping "Cancel") and that upon cancellation the export menu
     * item gets re-enabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportCancelOnWarningDismissal() {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();

        reauthenticateAndRequestExport(settingsActivity);

        // Verify that the warning dialog is shown and then dismiss it through pressing back (as
        // opposed to the cancel button).
        Espresso.onView(withText(R.string.password_settings_export_action_title))
                .check(matches(isDisplayed()));
        Espresso.pressBack();

        // Check that the cancellation succeeded by checking that the export menu is available and
        // enabled.
        checkExportMenuItemState(MenuItemState.ENABLED);
    }

    /**
     * Check that a progressbar is displayed for a minimal time duration to avoid flickering.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportProgressMinimalTime() throws Exception {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();

        Intents.init();

        // This also disables the timer for keeping the progress bar up. The test can thus emulate
        // that timer going off by calling {@link allowProgressBarToBeHidden}.
        reauthenticateAndRequestExport(settingsActivity);

        // Before triggering the sharing intent chooser, stub it out to avoid leaving system UI open
        // after the test is finished.
        intending(hasAction(equalTo(Intent.ACTION_CHOOSER)))
                .respondWith(new Instrumentation.ActivityResult(Activity.RESULT_OK, null));

        // Confirm the export warning to fire the sharing intent.
        Espresso.onView(withText(R.string.password_settings_export_action_title)).perform(click());

        // Before simulating the serialized passwords being received, check that the progress bar is
        // shown.
        Espresso.onView(withText(R.string.settings_passwords_preparing_export))
                .check(matches(isDisplayed()));

        File tempFile = createFakeExportedPasswordsFile();
        // Now pretend that passwords have been serialized.
        mHandler.getExportSuccessCallback().onResult(12, tempFile.getPath());

        // Check that the progress bar is still shown, though, because the timer has not gone off
        // yet.
        Espresso.onView(withText(R.string.settings_passwords_preparing_export))
                .check(matches(isDisplayed()));

        // Now mark the timer as gone off and check that the progress bar is hidden.
        allowProgressBarToBeHidden(settingsActivity);
        Espresso.onView(withText(R.string.settings_passwords_preparing_export))
                .check(doesNotExist());

        intended(allOf(hasAction(equalTo(Intent.ACTION_CHOOSER)),
                hasExtras(hasEntry(equalTo(Intent.EXTRA_INTENT),
                        allOf(hasAction(equalTo(Intent.ACTION_SEND)), hasType("text/csv"))))));

        Intents.release();

        tempFile.delete();
    }

    /**
     * Check that a progressbar is displayed when the user confirms the export and the serialized
     * passwords are not ready yet.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportProgress() throws Exception {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();

        Intents.init();

        reauthenticateAndRequestExport(settingsActivity);

        // Before triggering the sharing intent chooser, stub it out to avoid leaving system UI open
        // after the test is finished.
        intending(hasAction(equalTo(Intent.ACTION_CHOOSER)))
                .respondWith(new Instrumentation.ActivityResult(Activity.RESULT_OK, null));

        // Confirm the export warning to fire the sharing intent.
        Espresso.onView(withText(R.string.password_settings_export_action_title)).perform(click());

        // Before simulating the serialized passwords being received, check that the progress bar is
        // shown.
        Espresso.onView(withText(R.string.settings_passwords_preparing_export))
                .check(matches(isDisplayed()));

        File tempFile = createFakeExportedPasswordsFile();

        // Now pretend that passwords have been serialized.
        allowProgressBarToBeHidden(settingsActivity);
        mHandler.getExportSuccessCallback().onResult(12, tempFile.getPath());

        // After simulating the serialized passwords being received, check that the progress bar is
        // hidden.
        Espresso.onView(withText(R.string.settings_passwords_preparing_export))
                .check(doesNotExist());

        intended(allOf(hasAction(equalTo(Intent.ACTION_CHOOSER)),
                hasExtras(hasEntry(equalTo(Intent.EXTRA_INTENT),
                        allOf(hasAction(equalTo(Intent.ACTION_SEND)), hasType("text/csv"))))));

        Intents.release();

        tempFile.delete();
    }

    /**
     * Check that the user can cancel exporting with the "Cancel" button on the progressbar.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportCancelOnProgress() {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();

        reauthenticateAndRequestExport(settingsActivity);

        // Confirm the export warning to fire the sharing intent.
        Espresso.onView(withText(R.string.password_settings_export_action_title)).perform(click());

        // Simulate the minimal time for showing the progress bar to have passed, to ensure that it
        // is kept live because of the pending serialization.
        allowProgressBarToBeHidden(settingsActivity);

        // Check that the progress bar is shown.
        Espresso.onView(withText(R.string.settings_passwords_preparing_export))
                .check(matches(isDisplayed()));

        // Hit the Cancel button.
        Espresso.onView(withText(R.string.cancel)).perform(click());

        // Check that the cancellation succeeded by checking that the export menu is available and
        // enabled.
        checkExportMenuItemState(MenuItemState.ENABLED);
    }

    /**
     * Check that the user can cancel exporting with the negative button on the error message.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportCancelOnError() {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();

        reauthenticateAndRequestExport(settingsActivity);

        // Confirm the export warning.
        Espresso.onView(withText(R.string.password_settings_export_action_title)).perform(click());

        // Show an arbitrary error. This should replace the progress bar if that has been shown in
        // the meantime.
        allowProgressBarToBeHidden(settingsActivity);
        requestShowingExportError();

        // Check that the error prompt is showing.
        Espresso.onView(withText(R.string.password_settings_export_error_title))
                .check(matches(isDisplayed()));

        // Hit the negative button on the error prompt.
        Espresso.onView(withText(R.string.close)).perform(click());

        // Check that the cancellation succeeded by checking that the export menu is available and
        // enabled.
        checkExportMenuItemState(MenuItemState.ENABLED);
    }

    /**
     * Check that the user can re-trigger the export from an error dialog which has a "retry"
     * button.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportRetry() {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();

        reauthenticateAndRequestExport(settingsActivity);

        // Confirm the export warning.
        Espresso.onView(withText(R.string.password_settings_export_action_title)).perform(click());

        // Show an arbitrary error but ensure that the positive button label is the one for "try
        // again".
        allowProgressBarToBeHidden(settingsActivity);
        requestShowingExportErrorWithButton(settingsActivity, R.string.try_again);

        // Hit the positive button to try again.
        Espresso.onView(withText(R.string.try_again)).perform(click());

        // Check that there is again the export warning.
        Espresso.onView(withText(R.string.password_settings_export_action_title))
                .check(matches(isDisplayed()));
    }

    /**
     * Check that the error dialog lets the user visit a help page to install Google Drive if they
     * need an app to consume the exported passwords.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportHelpSite() {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();

        reauthenticateAndRequestExport(settingsActivity);

        // Confirm the export warning.
        Espresso.onView(withText(R.string.password_settings_export_action_title)).perform(click());

        // Show an arbitrary error but ensure that the positive button label is the one for the
        // Google Drive help site.
        allowProgressBarToBeHidden(settingsActivity);
        requestShowingExportErrorWithButton(
                settingsActivity, R.string.password_settings_export_learn_google_drive);

        Intents.init();

        // Before triggering the viewing intent, stub it out to avoid cascading that into further
        // intents and opening the web browser.
        intending(hasAction(equalTo(Intent.ACTION_VIEW)))
                .respondWith(new Instrumentation.ActivityResult(Activity.RESULT_OK, null));

        // Hit the positive button to navigate to the help site.
        Espresso.onView(withText(R.string.password_settings_export_learn_google_drive))
                .perform(click());

        intended(allOf(hasAction(equalTo(Intent.ACTION_VIEW)),
                hasData(hasHost(equalTo("support.google.com")))));

        Intents.release();
    }

    /**
     * Check that if errors are encountered when user is busy confirming the export, the error UI is
     * shown after the confirmation is completed, so that the user does not see UI changing
     * unexpectedly under their fingers.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testExportErrorUiAfterConfirmation() {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        final SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();

        reauthenticateAndRequestExport(settingsActivity);

        // Request showing an arbitrary error while the confirmation dialog is still up.
        requestShowingExportError();

        // Check that the confirmation dialog is showing and dismiss it.
        Espresso.onView(withText(R.string.password_settings_export_action_title)).perform(click());

        // Check that now the error is displayed, instead of the progress bar.
        allowProgressBarToBeHidden(settingsActivity);
        Espresso.onView(withText(R.string.settings_passwords_preparing_export))
                .check(doesNotExist());
        Espresso.onView(withText(R.string.password_settings_export_error_title))
                .check(matches(isDisplayed()));

        // Close the error dialog and abort the export.
        Espresso.onView(withText(R.string.close)).perform(click());

        // Ensure that there is still no progress bar.
        Espresso.onView(withText(R.string.settings_passwords_preparing_export))
                .check(doesNotExist());
    }

    /**
     * Check whether the user is asked to set up a screen lock if attempting to view passwords.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testViewPasswordNoLock() {
        setPasswordSource(new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.UNAVAILABLE);

        final SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();

        View mainDecorView = settingsActivity.getWindow().getDecorView();
        Espresso.onView(withText(containsString("test user"))).perform(click());
        Espresso.onView(withContentDescription(R.string.password_entry_viewer_copy_stored_password))
                .perform(click());
        Espresso.onView(withText(R.string.password_entry_viewer_set_lock_screen))
                .inRoot(withDecorView(not(is(mainDecorView))))
                .check(matches(isDisplayed()));
    }

    /**
     * Check whether the user can view a saved password.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testViewPassword() {
        setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "test password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        mSettingsActivityTestRule.startSettingsActivity();

        Espresso.onView(withText(containsString("test user"))).perform(click());

        // Before tapping the view button, pretend that the last successful reauthentication just
        // happened. This will allow showing the password.
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthenticationManager.ReauthScope.ONE_AT_A_TIME);
        Espresso.onView(withContentDescription(R.string.password_entry_viewer_view_stored_password))
                .perform(click());
        Espresso.onView(withText("test password")).check(matches(isDisplayed()));
    }

    /**
     * Check that the search item is visible if the Feature is enabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @SuppressWarnings("AlwaysShowAction") // We need to ensure the icon is in the action bar.
    public void testSearchIconVisibleInActionBarWithFeature() {
        setPasswordSource(null); // Initialize empty preferences.
        mSettingsActivityTestRule.startSettingsActivity();
        PasswordSettings f = mSettingsActivityTestRule.getFragment();

        // Force the search option into the action bar.
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> f.getMenuForTesting()
                                   .findItem(R.id.menu_id_search)
                                   .setShowAsAction(MenuItem.SHOW_AS_ACTION_ALWAYS));

        Espresso.onView(withId(R.id.menu_id_search)).check(matches(isDisplayed()));
    }

    /**
     * Check that the icon for editing saved passwords is visible if the Feature is enabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Features.EnableFeatures(ChromeFeatureList.PASSWORD_EDITING_ANDROID)
    public void testEditSavedPasswordIconVisibleInActionBarWithFeature() {
        setPasswordSource( // Initialize preferences
                new SavedPasswordEntry("https://example.com", "test user", "test password"));

        mSettingsActivityTestRule.startSettingsActivity();

        Espresso.onView(withText(containsString("test user"))).perform(click());

        Espresso.onView(withEditMenuIdOrText()).check(matches(isDisplayed()));
    }

    /**
     * Check that the search item is visible if the Feature is enabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSearchTextInOverflowMenuVisibleWithFeature() {
        setPasswordSource(null); // Initialize empty preferences.mSettingsActivityTestRule
        mSettingsActivityTestRule.startSettingsActivity();
        PasswordSettings f = mSettingsActivityTestRule.getFragment();

        // Force the search option into the overflow menu.
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> f.getMenuForTesting()
                                   .findItem(R.id.menu_id_search)
                                   .setShowAsAction(MenuItem.SHOW_AS_ACTION_NEVER));

        // Open the overflow menu.
        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());

        Espresso.onView(withText(R.string.search)).check(matches(isDisplayed()));
    }

    /**
     * Check that searching doesn't push the help icon into the overflow menu permanently.
     * On screen sizes where the help item starts out in the overflow menu, ensure it stays there.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testTriggeringSearchRestoresHelpIcon() {
        setPasswordSource(null);
        mSettingsActivityTestRule.startSettingsActivity();
        onViewWaiting(withText(R.string.password_settings_title));

        // Retrieve the initial status and ensure that the help option is there at all.
        final AtomicReference<Boolean> helpInOverflowMenu = new AtomicReference<>(false);
        Espresso.onView(withId(R.id.menu_id_targeted_help)).check((helpMenuItem, e) -> {
            ActionMenuItemView view = (ActionMenuItemView) helpMenuItem;
            helpInOverflowMenu.set(view == null || !view.showsIcon());
        });
        if (helpInOverflowMenu.get()) {
            openActionBarOverflowOrOptionsMenu(
                    InstrumentationRegistry.getInstrumentation().getTargetContext());
            Espresso.onView(withText(R.string.menu_help)).check(matches(isDisplayed()));
            Espresso.pressBack(); // to close the Overflow menu.
        } else {
            Espresso.onView(withId(R.id.menu_id_targeted_help)).check(matches(isDisplayed()));
        }

        // Trigger the search, close it and wait for UI to be restored.
        Espresso.onView(withSearchMenuIdOrText()).perform(click());
        Espresso.onView(withContentDescription(R.string.abc_action_bar_up_description))
                .perform(click());
        onViewWaiting(withText(R.string.password_settings_title));

        // Check that the help option is exactly where it was to begin with.
        if (helpInOverflowMenu.get()) {
            openActionBarOverflowOrOptionsMenu(
                    InstrumentationRegistry.getInstrumentation().getTargetContext());
            Espresso.onView(withText(R.string.menu_help)).check(matches(isDisplayed()));
            Espresso.onView(withId(R.id.menu_id_targeted_help)).check(doesNotExist());
        } else {
            Espresso.onView(withText(R.string.menu_help)).check(doesNotExist());
            Espresso.onView(withId(R.id.menu_id_targeted_help)).check(matches(isDisplayed()));
        }
    }

    /**
     * Check that the search filters the list by name.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSearchFiltersByUserName() {
        setPasswordSourceWithMultipleEntries(GREEK_GODS);
        mSettingsActivityTestRule.startSettingsActivity();

        // Search for a string matching multiple user names. Case doesn't need to match.
        Espresso.onView(withSearchMenuIdOrText()).perform(click());
        Espresso.onView(withId(R.id.search_src_text))
                .perform(click(), typeText("aREs"), closeSoftKeyboard());

        Espresso.onView(withText(ARES_AT_OLYMP.getUserName())).check(matches(isDisplayed()));
        Espresso.onView(withText(PHOBOS_AT_OLYMP.getUserName())).check(matches(isDisplayed()));
        Espresso.onView(withText(DEIMOS_AT_OLYMP.getUserName())).check(matches(isDisplayed()));
        Espresso.onView(withText(ZEUS_ON_EARTH.getUserName())).check(doesNotExist());
        Espresso.onView(withText(HADES_AT_UNDERWORLD.getUrl())).check(doesNotExist());
    }

    /**
     * Check that the search filters the list by URL.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSearchFiltersByUrl() {
        setPasswordSourceWithMultipleEntries(GREEK_GODS);
        mSettingsActivityTestRule.startSettingsActivity();

        // Search for a string that matches multiple URLs. Case doesn't need to match.
        Espresso.onView(withSearchMenuIdOrText()).perform(click());
        Espresso.onView(withId(R.id.search_src_text))
                .perform(click(), typeText("Olymp"), closeSoftKeyboard());

        Espresso.onView(withText(ARES_AT_OLYMP.getUserName())).check(matches(isDisplayed()));
        Espresso.onView(withText(PHOBOS_AT_OLYMP.getUserName())).check(matches(isDisplayed()));
        Espresso.onView(withText(DEIMOS_AT_OLYMP.getUserName())).check(matches(isDisplayed()));
        Espresso.onView(withText(ZEUS_ON_EARTH.getUserName())).check(doesNotExist());
        Espresso.onView(withText(HADES_AT_UNDERWORLD.getUrl())).check(doesNotExist());
    }

    /**
     * Check that the search filters the list by URL.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSearchDisplaysNoResultMessageIfSearchTurnsUpEmpty() {
        setPasswordSourceWithMultipleEntries(GREEK_GODS);
        mSettingsActivityTestRule.startSettingsActivity();

        // Open the search which should hide the Account link.
        Espresso.onView(withSearchMenuIdOrText()).perform(click());

        // Search for a string that matches nothing which should leave the results entirely blank.
        Espresso.onView(withId(R.id.search_src_text))
                .perform(click(), typeText("Mars"), closeSoftKeyboard());

        for (SavedPasswordEntry god : GREEK_GODS) {
            Espresso.onView(allOf(withText(god.getUserName()), withText(god.getUrl())))
                    .check(doesNotExist());
        }
        Espresso.onView(withText(R.string.saved_passwords_none_text)).check(doesNotExist());
        // Check that the section header for saved passwords is not present. Do not confuse it with
        // the toolbar label which contains the same string, look for the one inside a linear
        // layout.
        Espresso.onView(allOf(withParent(isAssignableFrom(LinearLayout.class)),
                                withText(R.string.password_settings_title)))
                .check(doesNotExist());
        // Check the message for no result.
        Espresso.onView(withText(R.string.password_no_result)).check(matches(isDisplayed()));
    }

    /**
     * Check that triggering the search hides all non-password prefs.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSearchIconClickedHidesExceptionsTemporarily() {
        setPasswordExceptions(new String[] {"http://exclu.de", "http://not-inclu.de"});
        mSettingsActivityTestRule.startSettingsActivity();

        Espresso.onView(withText(R.string.section_saved_passwords_exceptions))
                .check(matches(isDisplayed()));

        Espresso.onView(withSearchMenuIdOrText()).perform(click());
        Espresso.onView(withId(R.id.search_src_text)).perform(click(), closeSoftKeyboard());

        Espresso.onView(withText(R.string.section_saved_passwords_exceptions))
                .check(doesNotExist());

        Espresso.onView(withContentDescription(R.string.abc_action_bar_up_description))
                .perform(click());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync(); // Close search view.

        Espresso.onView(withText(R.string.section_saved_passwords_exceptions))
                .check(matches(isDisplayed()));
    }

    /**
     * Check that triggering the search hides all non-password prefs.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSearchIconClickedHidesGeneralPrefs() {
        setPasswordSource(ZEUS_ON_EARTH);
        mSettingsActivityTestRule.startSettingsActivity();
        final PasswordSettings prefs = mSettingsActivityTestRule.getFragment();
        final AtomicReference<Boolean> menuInitiallyVisible = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> menuInitiallyVisible.set(
                                prefs.getToolbarForTesting().isOverflowMenuShowing()));

        Espresso.onView(withText(R.string.passwords_auto_signin_title))
                .check(matches(isDisplayed()));
        if (menuInitiallyVisible.get()) { // Check overflow menu only on large screens that have it.
            Espresso.onView(withContentDescription(R.string.abc_action_menu_overflow_description))
                    .check(matches(isDisplayed()));
        }

        Espresso.onView(withSearchMenuIdOrText()).perform(click());

        Espresso.onView(withText(R.string.passwords_auto_signin_title)).check(doesNotExist());
        Espresso.onView(isRoot()).check(waitForView(
                withParent(withContentDescription(R.string.abc_action_menu_overflow_description)),
                VIEW_INVISIBLE | VIEW_GONE | VIEW_NULL));

        Espresso.onView(withContentDescription(R.string.abc_action_bar_up_description))
                .perform(click());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        if (menuInitiallyVisible.get()) { // If the overflow menu was there, it should be restored.
            Espresso.onView(withContentDescription(R.string.abc_action_menu_overflow_description))
                    .check(matches(isDisplayed()));
        }
    }

    /**
     * Check that closing the search via back button brings back all non-password prefs.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSearchBarBackButtonRestoresGeneralPrefs() {
        setPasswordSourceWithMultipleEntries(GREEK_GODS);
        mSettingsActivityTestRule.startSettingsActivity();

        Espresso.onView(withSearchMenuIdOrText()).perform(click());
        Espresso.onView(withId(R.id.search_src_text))
                .perform(click(), typeText("Zeu"), closeSoftKeyboard());

        Espresso.onView(withText(R.string.passwords_auto_signin_title)).check(doesNotExist());

        Espresso.onView(withContentDescription(R.string.abc_action_bar_up_description))
                .perform(click());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Espresso.onView(withText(R.string.passwords_auto_signin_title))
                .check(matches(isDisplayed()));
        Espresso.onView(withId(R.id.menu_id_search)).check(matches(isDisplayed()));
    }

    /**
     * Check that clearing the search also hides the clear button.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSearchViewCloseIconExistsOnlyToClearQueries() {
        setPasswordSourceWithMultipleEntries(GREEK_GODS);
        mSettingsActivityTestRule.startSettingsActivity();

        // Trigger search which shouldn't have the button yet.
        Espresso.onView(withSearchMenuIdOrText()).perform(click());
        Espresso.onView(isRoot()).check(
                waitForView(withId(R.id.search_close_btn), VIEW_INVISIBLE | VIEW_GONE | VIEW_NULL));

        // Type something and see the button appear.
        Espresso.onView(withId(R.id.search_src_text))
                // Trigger search which shouldn't have the button yet.
                .perform(click(), typeText("Zeu"), closeSoftKeyboard());
        Espresso.onView(withId(R.id.search_close_btn)).check(matches(isDisplayed()));

        // Clear the search which should hide the button again.
        Espresso.onView(withId(R.id.search_close_btn)).perform(click()); // Clear search.
        Espresso.onView(isRoot()).check(
                waitForView(withId(R.id.search_close_btn), VIEW_INVISIBLE | VIEW_GONE | VIEW_NULL));
    }

    /**
     * Check that the changed color of the loaded Drawable does not persist for other uses of the
     * drawable. This is not implicitly true as a loaded Drawable is by default only a reference to
     * the globally defined resource.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSearchIconColorAffectsOnlyLocalSearchDrawable() {
        // Open the password preferences and remember the applied color filter.
        mSettingsActivityTestRule.startSettingsActivity();
        final PasswordSettings f = mSettingsActivityTestRule.getFragment();
        Espresso.onView(withId(R.id.search_button)).check(matches(isDisplayed()));
        final AtomicReference<ColorFilter> passwordSearchFilter = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Drawable drawable = f.getMenuForTesting().findItem(R.id.menu_id_search).getIcon();
            passwordSearchFilter.set(DrawableCompat.getColorFilter(drawable));
        });

        // Now launch a non-empty History activity.
        StubbedHistoryProvider mHistoryProvider = new StubbedHistoryProvider();
        mHistoryProvider.addItem(StubbedHistoryProvider.createHistoryItem(0, new Date().getTime()));
        mHistoryProvider.addItem(StubbedHistoryProvider.createHistoryItem(1, new Date().getTime()));
        HistoryManager.setProviderForTests(mHistoryProvider);
        mHistoryActivityTestRule.launchActivity(null);

        // Find the search view to ensure that the set color filter is different from the saved one.
        final AtomicReference<ColorFilter> historySearchFilter = new AtomicReference<>();
        Espresso.onView(withId(R.id.search_menu_id)).check(matches(isDisplayed()));
        Espresso.onView(withId(R.id.search_menu_id)).check((searchMenuItem, e) -> {
            Drawable drawable = ((ActionMenuItemView) searchMenuItem).getItemData().getIcon();
            historySearchFilter.set(DrawableCompat.getColorFilter(drawable));
            Assert.assertThat(historySearchFilter.get(),
                    anyOf(is(nullValue()), is(not(sameInstance(passwordSearchFilter.get())))));
        });

        // Close the activity and check that the icon in the password preferences has not changed.
        mHistoryActivityTestRule.getActivity().finish();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ColorFilter colorFilter = DrawableCompat.getColorFilter(
                    f.getMenuForTesting().findItem(R.id.menu_id_search).getIcon());
            Assert.assertThat(colorFilter,
                    anyOf(is(nullValue()), is(sameInstance(passwordSearchFilter.get()))));
            Assert.assertThat(colorFilter,
                    anyOf(is(nullValue()), is(not(sameInstance(historySearchFilter.get())))));
        });
    }

    /**
     * Check that the filtered password list persists after the user had inspected a single result.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSearchResultsPersistAfterEntryInspection() {
        setPasswordSourceWithMultipleEntries(GREEK_GODS);
        setPasswordExceptions(new String[] {"http://exclu.de", "http://not-inclu.de"});
        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);
        mSettingsActivityTestRule.startSettingsActivity();

        // Open the search and filter all but "Zeus".
        Espresso.onView(withSearchMenuIdOrText()).perform(click());

        onViewWaiting(withId(R.id.search_src_text));
        Espresso.onView(withId(R.id.search_src_text))
                .perform(click(), typeText("Zeu"), closeSoftKeyboard());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Espresso.onView(withText(R.string.passwords_auto_signin_title)).check(doesNotExist());
        Espresso.onView(withText(ZEUS_ON_EARTH.getUserName())).check(matches(isDisplayed()));
        Espresso.onView(withText(PHOBOS_AT_OLYMP.getUserName())).check(doesNotExist());
        Espresso.onView(withText(HADES_AT_UNDERWORLD.getUrl())).check(doesNotExist());

        // Click "Zeus" to open edit field and verify the password. Pretend the user just passed the
        // reauthentication challenge.
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthenticationManager.ReauthScope.ONE_AT_A_TIME);
        Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(
                        new IntentFilter(Intent.ACTION_VIEW), null, false);
        Espresso.onView(withText(ZEUS_ON_EARTH.getUserName())).perform(click());
        monitor.waitForActivityWithTimeout(UI_UPDATING_TIMEOUT_MS);
        Assert.assertEquals("Monitor for has not been called", 1, monitor.getHits());
        InstrumentationRegistry.getInstrumentation().removeMonitor(monitor);
        Espresso.onView(withContentDescription(R.string.password_entry_viewer_view_stored_password))
                .perform(click());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Espresso.onView(withText(ZEUS_ON_EARTH.getPassword())).check(matches(isDisplayed()));
        Espresso.onView(withContentDescription(R.string.abc_action_bar_up_description))
                .perform(click()); // Go back to the search list.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Espresso.onView(withText(R.string.passwords_auto_signin_title)).check(doesNotExist());
        Espresso.onView(withText(ZEUS_ON_EARTH.getUserName())).check(matches(isDisplayed()));
        Espresso.onView(withText(PHOBOS_AT_OLYMP.getUserName())).check(doesNotExist());
        Espresso.onView(withText(HADES_AT_UNDERWORLD.getUrl())).check(doesNotExist());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // The search bar should still be open and still display the search query.
        onViewWaiting(allOf(withId(R.id.search_src_text), withText("Zeu")));
        Espresso.onView(withId(R.id.search_src_text)).check(matches(withText("Zeu")));
    }

    PasswordEditingDelegate waitForEvent() {
        return verify(mMockPasswordEditingDelegate,
                timeout(ScalableTimeout.scaleTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL)));
    }
}
