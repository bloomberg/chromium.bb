// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_dialog;

import android.Manifest;
import android.app.Dialog;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.location.LocationManager;
import android.support.test.filters.LargeTest;
import android.view.View;
import android.widget.Button;
import android.widget.ListView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.location.LocationUtils;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.base.PermissionCallback;
import org.chromium.ui.widget.TextViewWithClickableSpans;

import java.util.concurrent.Callable;

/**
 * Tests for the BluetoothChooserDialog class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class BluetoothChooserDialogTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Rule
    public JniMocker mocker = new JniMocker();

    private ActivityWindowAndroid mWindowAndroid;
    private FakeLocationUtils mLocationUtils;
    private BluetoothChooserDialog mChooserDialog;

    private int mFinishedEventType = -1;
    private String mFinishedDeviceId;
    private int mRestartSearchCount;

    private class TestBluetoothChooserDialogJni implements BluetoothChooserDialog.Natives {
        private BluetoothChooserDialog mBluetoothChooserDialog;

        TestBluetoothChooserDialogJni(BluetoothChooserDialog dialog) {
            mBluetoothChooserDialog = dialog;
        }

        @Override
        public void onDialogFinished(
                long nativeBluetoothChooserAndroid, int eventType, String deviceId) {
            Assert.assertEquals(nativeBluetoothChooserAndroid,
                    mBluetoothChooserDialog.mNativeBluetoothChooserDialogPtr);
            Assert.assertEquals(mFinishedEventType, -1);
            mFinishedEventType = eventType;
            mFinishedDeviceId = deviceId;
            // The native code calls closeDialog() when OnDialogFinished is called.
            mBluetoothChooserDialog.closeDialog();
        }

        @Override
        public void restartSearch(long nativeBluetoothChooserAndroid) {
            Assert.assertTrue(mBluetoothChooserDialog.mNativeBluetoothChooserDialogPtr != 0);
            mRestartSearchCount++;
        }

        @Override
        public void showBluetoothOverviewLink(long nativeBluetoothChooserAndroid) {
            // We shouldn't be running native functions if the native class has been destroyed.
            Assert.assertTrue(mBluetoothChooserDialog.mNativeBluetoothChooserDialogPtr != 0);
        }

        @Override
        public void showBluetoothAdapterOffLink(long nativeBluetoothChooserAndroid) {
            // We shouldn't be running native functions if the native class has been destroyed.
            Assert.assertTrue(mBluetoothChooserDialog.mNativeBluetoothChooserDialogPtr != 0);
        }

        @Override
        public void showNeedLocationPermissionLink(long nativeBluetoothChooserAndroid) {
            // We shouldn't be running native functions if the native class has been destroyed.
            Assert.assertTrue(mBluetoothChooserDialog.mNativeBluetoothChooserDialogPtr != 0);
        }
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mLocationUtils = new FakeLocationUtils();
        LocationUtils.setFactory(() -> mLocationUtils);
        mChooserDialog = createDialog();
        mocker.mock(BluetoothChooserDialogJni.TEST_HOOKS,
                new TestBluetoothChooserDialogJni(mChooserDialog));
    }

    @After
    public void tearDown() {
        LocationUtils.setFactory(null);
    }

    private BluetoothChooserDialog createDialog() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> {
                    mWindowAndroid = new ActivityWindowAndroid(mActivityTestRule.getActivity());
                    BluetoothChooserDialog dialog = new BluetoothChooserDialog(mWindowAndroid,
                            "https://origin.example.com/", ConnectionSecurityLevel.SECURE,
                            /*nativeBluetoothChooserDialogPtr=*/42);
                    dialog.show();
                    return dialog;
                });
    }

    private void selectItem(int position) {
        final Dialog dialog = mChooserDialog.mItemChooserDialog.getDialogForTesting();
        final ListView items = (ListView) dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);

        clickItemAtPosition(items, position - 1);

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return button.isEnabled();
            }
        });

        TouchCommon.singleClickView(button);

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mFinishedEventType != -1;
            }
        });
    }

    private static void clickItemAtPosition(ListView listView, int position) {
        CriteriaHelper.pollUiThread(() -> listView.getChildAt(0) != null);

        Callable<Boolean> isVisible = () -> {
            int visibleStart = listView.getFirstVisiblePosition();
            int visibleEnd = visibleStart + listView.getChildCount() - 1;
            return position >= visibleStart && position <= visibleEnd;
        };

        if (!TestThreadUtils.runOnUiThreadBlockingNoException(isVisible)) {
            TestThreadUtils.runOnUiThreadBlocking(() -> listView.setSelection(position));
            CriteriaHelper.pollUiThread(isVisible);
        }

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TouchCommon.singleClickView(
                    listView.getChildAt(position - listView.getFirstVisiblePosition()));
        });
    }

    /**
     * The messages include <*link*> ... </*link*> sections that are used to create clickable spans.
     * For testing the messages, this function returns the raw string without the tags.
     */
    private static String removeLinkTags(String message) {
        return message.replaceAll("</?[^>]*link[^>]*>", "");
    }

    @Test
    @LargeTest
    public void testCancel() {
        ItemChooserDialog itemChooser = mChooserDialog.mItemChooserDialog;
        Dialog dialog = itemChooser.getDialogForTesting();
        Assert.assertTrue(dialog.isShowing());

        TextViewWithClickableSpans statusView =
                (TextViewWithClickableSpans) dialog.findViewById(R.id.status);
        final ListView items = (ListView) dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);

        // Before we add items to the dialog, the 'searching' message should be
        // showing, the Commit button should be disabled and the list view hidden.
        Assert.assertEquals(removeLinkTags(mActivityTestRule.getActivity().getString(
                                    R.string.bluetooth_searching)),
                statusView.getText().toString());
        Assert.assertFalse(button.isEnabled());
        Assert.assertEquals(View.GONE, items.getVisibility());

        dialog.cancel();

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mFinishedEventType != -1;
            }
        });

        Assert.assertEquals(BluetoothChooserDialog.DialogFinished.CANCELLED, mFinishedEventType);
        Assert.assertEquals("", mFinishedDeviceId);
    }

    @Test
    @LargeTest
    public void testSelectItem() {
        Dialog dialog = mChooserDialog.mItemChooserDialog.getDialogForTesting();

        TextViewWithClickableSpans statusView =
                (TextViewWithClickableSpans) dialog.findViewById(R.id.status);
        final View items = dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);
        final View progress = dialog.findViewById(R.id.progress);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Add non-connected device with no signal strength.
            mChooserDialog.addOrUpdateDevice("id-1", "Name 1", false /* isGATTConnected */,
                    -1 /* signalStrengthLevel */);
            // Add connected device with no signal strength.
            mChooserDialog.addOrUpdateDevice(
                    "id-2", "Name 2", true /* isGATTConnected */, -1 /* signalStrengthLevel */);
            // Add non-connected device with signal strength level 1.
            mChooserDialog.addOrUpdateDevice(
                    "id-3", "Name 3", false /* isGATTConnected */, 1 /* signalStrengthLevel */);
            // Add connected device with signal strength level 1.
            mChooserDialog.addOrUpdateDevice(
                    "id-4", "Name 4", true /* isGATTConnected */, 1 /* signalStrengthLevel */);
        });

        // After adding items to the dialog, the help message should be showing,
        // the progress spinner should disappear, the Commit button should still
        // be disabled (since nothing's selected), and the list view should
        // show.
        Assert.assertEquals(removeLinkTags(mActivityTestRule.getActivity().getString(
                                    R.string.bluetooth_searching)),
                statusView.getText().toString());
        Assert.assertFalse(button.isEnabled());
        Assert.assertEquals(View.VISIBLE, items.getVisibility());
        Assert.assertEquals(View.GONE, progress.getVisibility());

        DeviceItemAdapter itemAdapter =
                mChooserDialog.mItemChooserDialog.getItemAdapterForTesting();
        Assert.assertTrue(itemAdapter.getItem(0).hasSameContents(
                "id-1", "Name 1", null /* icon */, null /* iconDescription */));
        Assert.assertTrue(itemAdapter.getItem(1).hasSameContents("id-2", "Name 2",
                mChooserDialog.mConnectedIcon, mChooserDialog.mConnectedIconDescription));
        Assert.assertTrue(itemAdapter.getItem(2).hasSameContents("id-3", "Name 3",
                mChooserDialog.mSignalStrengthLevelIcon[1],
                mActivityTestRule.getActivity().getResources().getQuantityString(
                        R.plurals.signal_strength_level_n_bars, 1, 1)));
        // We show the connected icon even if the device has a signal strength.
        Assert.assertTrue(itemAdapter.getItem(3).hasSameContents("id-4", "Name 4",
                mChooserDialog.mConnectedIcon, mChooserDialog.mConnectedIconDescription));

        selectItem(2);

        Assert.assertEquals(BluetoothChooserDialog.DialogFinished.SELECTED, mFinishedEventType);
        Assert.assertEquals("id-2", mFinishedDeviceId);
    }

    @Test
    @LargeTest
    public void testNoLocationPermission() {
        ItemChooserDialog itemChooser = mChooserDialog.mItemChooserDialog;
        Dialog dialog = itemChooser.getDialogForTesting();
        Assert.assertTrue(dialog.isShowing());

        final TextViewWithClickableSpans statusView =
                (TextViewWithClickableSpans) dialog.findViewById(R.id.status);
        final TextViewWithClickableSpans errorView =
                (TextViewWithClickableSpans) dialog.findViewById(R.id.not_found_message);
        final View items = dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);
        final View progress = dialog.findViewById(R.id.progress);

        final TestAndroidPermissionDelegate permissionDelegate =
                new TestAndroidPermissionDelegate(dialog);
        mWindowAndroid.setAndroidPermissionDelegate(permissionDelegate);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mChooserDialog.notifyDiscoveryState(
                                BluetoothChooserDialog.DiscoveryMode.DISCOVERY_FAILED_TO_START));

        Assert.assertEquals(removeLinkTags(mActivityTestRule.getActivity().getString(
                                    R.string.bluetooth_need_location_permission)),
                errorView.getText().toString());
        Assert.assertEquals(removeLinkTags(mActivityTestRule.getActivity().getString(
                                    R.string.bluetooth_adapter_off_help)),
                statusView.getText().toString());
        Assert.assertFalse(button.isEnabled());
        Assert.assertEquals(View.VISIBLE, errorView.getVisibility());
        Assert.assertEquals(View.GONE, items.getVisibility());
        Assert.assertEquals(View.GONE, progress.getVisibility());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> errorView.getClickableSpans()[0].onClick(errorView));

        // Permission was requested.
        Assert.assertArrayEquals(permissionDelegate.mPermissionsRequested,
                new String[] {Manifest.permission.ACCESS_FINE_LOCATION});
        Assert.assertNotNull(permissionDelegate.mCallback);
        // Grant permission.
        permissionDelegate.mLocationGranted = true;
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> permissionDelegate.mCallback.onRequestPermissionsResult(
                                new String[] {Manifest.permission.ACCESS_FINE_LOCATION},
                                new int[] {PackageManager.PERMISSION_GRANTED}));

        Assert.assertEquals(1, mRestartSearchCount);
        Assert.assertEquals(removeLinkTags(mActivityTestRule.getActivity().getString(
                                    R.string.bluetooth_searching)),
                statusView.getText().toString());
        mChooserDialog.closeDialog();
    }

    @Test
    @LargeTest
    public void testNoLocationServices() {
        ItemChooserDialog itemChooser = mChooserDialog.mItemChooserDialog;
        Dialog dialog = itemChooser.getDialogForTesting();
        Assert.assertTrue(dialog.isShowing());

        final TextViewWithClickableSpans statusView =
                (TextViewWithClickableSpans) dialog.findViewById(R.id.status);
        final TextViewWithClickableSpans errorView =
                (TextViewWithClickableSpans) dialog.findViewById(R.id.not_found_message);
        final View items = dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);
        final View progress = dialog.findViewById(R.id.progress);

        final TestAndroidPermissionDelegate permissionDelegate =
                new TestAndroidPermissionDelegate(dialog);
        mWindowAndroid.setAndroidPermissionDelegate(permissionDelegate);

        // Grant permissions, and turn off location services.
        permissionDelegate.mLocationGranted = true;
        mLocationUtils.mSystemLocationSettingsEnabled = false;

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mChooserDialog.notifyDiscoveryState(
                                BluetoothChooserDialog.DiscoveryMode.DISCOVERY_FAILED_TO_START));

        Assert.assertEquals(removeLinkTags(mActivityTestRule.getActivity().getString(
                                    R.string.bluetooth_need_location_services_on)),
                errorView.getText().toString());
        Assert.assertEquals(removeLinkTags(mActivityTestRule.getActivity().getString(
                                    R.string.bluetooth_need_location_permission_help)),
                statusView.getText().toString());
        Assert.assertFalse(button.isEnabled());
        Assert.assertEquals(View.VISIBLE, errorView.getVisibility());
        Assert.assertEquals(View.GONE, items.getVisibility());
        Assert.assertEquals(View.GONE, progress.getVisibility());

        // Turn on Location Services.
        mLocationUtils.mSystemLocationSettingsEnabled = true;
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mChooserDialog.mLocationModeBroadcastReceiver.onReceive(
                                mActivityTestRule.getActivity(),
                                new Intent(LocationManager.MODE_CHANGED_ACTION)));

        Assert.assertEquals(1, mRestartSearchCount);
        Assert.assertEquals(removeLinkTags(mActivityTestRule.getActivity().getString(
                                    R.string.bluetooth_searching)),
                statusView.getText().toString());

        mChooserDialog.closeDialog();
    }

    // TODO(jyasskin): Test when the user denies Chrome the ability to ask for permission.

    @Test
    @LargeTest
    public void testTurnOnAdapter() {
        final ItemChooserDialog itemChooser = mChooserDialog.mItemChooserDialog;
        Dialog dialog = itemChooser.getDialogForTesting();
        Assert.assertTrue(dialog.isShowing());

        final TextViewWithClickableSpans statusView =
                (TextViewWithClickableSpans) dialog.findViewById(R.id.status);
        final TextViewWithClickableSpans errorView =
                (TextViewWithClickableSpans) dialog.findViewById(R.id.not_found_message);
        final View items = dialog.findViewById(R.id.items);
        final Button button = (Button) dialog.findViewById(R.id.positive);
        final View progress = dialog.findViewById(R.id.progress);

        // Turn off adapter.
        TestThreadUtils.runOnUiThreadBlocking(() -> mChooserDialog.notifyAdapterTurnedOff());

        Assert.assertEquals(removeLinkTags(mActivityTestRule.getActivity().getString(
                                    R.string.bluetooth_adapter_off)),
                errorView.getText().toString());
        Assert.assertEquals(removeLinkTags(mActivityTestRule.getActivity().getString(
                                    R.string.bluetooth_adapter_off_help)),
                statusView.getText().toString());
        Assert.assertFalse(button.isEnabled());
        Assert.assertEquals(View.VISIBLE, errorView.getVisibility());
        Assert.assertEquals(View.GONE, items.getVisibility());
        Assert.assertEquals(View.GONE, progress.getVisibility());

        // Turn on adapter.
        TestThreadUtils.runOnUiThreadBlocking(() -> itemChooser.signalInitializingAdapter());

        Assert.assertEquals(View.GONE, errorView.getVisibility());
        Assert.assertEquals(View.GONE, items.getVisibility());
        Assert.assertEquals(View.VISIBLE, progress.getVisibility());

        mChooserDialog.closeDialog();
    }

    private static class TestAndroidPermissionDelegate implements AndroidPermissionDelegate {
        Dialog mDialog;
        PermissionCallback mCallback;
        String[] mPermissionsRequested;
        public boolean mLocationGranted;

        public TestAndroidPermissionDelegate(Dialog dialog) {
            mLocationGranted = false;
            mDialog = dialog;
        }

        @Override
        public boolean hasPermission(String permission) {
            return mLocationGranted;
        }

        @Override
        public boolean canRequestPermission(String permission) {
            return true;
        }

        @Override
        public boolean isPermissionRevokedByPolicy(String permission) {
            return false;
        }

        @Override
        public void requestPermissions(String[] permissions, PermissionCallback callback) {
            // Requesting for permission takes away focus from the window.
            mDialog.onWindowFocusChanged(false /* hasFocus */);
            mPermissionsRequested = permissions;
            if (permissions.length == 1
                    && permissions[0].equals(Manifest.permission.ACCESS_FINE_LOCATION)) {
                mCallback = callback;
            }
        }

        @Override
        public boolean handlePermissionResult(
                int requestCode, String[] permissions, int[] grantResults) {
            return false;
        }
    }

    private static class FakeLocationUtils extends LocationUtils {
        public boolean mLocationGranted;

        @Override
        public boolean hasAndroidLocationPermission() {
            return mLocationGranted;
        }

        public boolean mSystemLocationSettingsEnabled = true;

        @Override
        public boolean isSystemLocationSettingEnabled() {
            return mSystemLocationSettingsEnabled;
        }
    }
}
