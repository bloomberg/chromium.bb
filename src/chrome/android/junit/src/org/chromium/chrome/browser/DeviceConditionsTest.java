// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doReturn;

import android.app.KeyguardManager;
import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.BatteryManager;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.os.PowerManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.net.ConnectionType;
import org.chromium.net.NetworkChangeNotifier;

/** Unit tests for {@link DeviceConditions} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config
public class DeviceConditionsTest {
    @Mock
    private Context mContext;
    @Mock
    private ConnectivityManager mConnectivityManager;
    @Mock
    private PowerManager mPowerManager;
    @Mock
    private NetworkChangeNotifier mNetworkChangeNotifier;
    @Mock
    private NetworkInfo mNetworkInfo;
    @Mock
    private KeyguardManager mKeyguardManager;

    private Intent mBatteryStatus;

    /** Helps change the SDK version in testing to one of the supported versions. */
    private static class BuildVersionHelper implements AutoCloseable {
        private int mOriginalSdkInt;

        public BuildVersionHelper() {
            mOriginalSdkInt = VERSION.SDK_INT;
        }

        /**
         * Sets Build.VERSION.SDK_INT to provided version Code.
         * @param versionCode Version code to set. Needs to be at least K and at most Q.
         */
        public void setSdkVersion(int versionCode) {
            assert VERSION_CODES.KITKAT <= versionCode && versionCode <= VERSION_CODES.Q;

            ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", versionCode);
        }

        /** Reverts the SDK version to original value. */
        @Override
        public void close() {
            setSdkVersion(mOriginalSdkInt);
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // Set up the battery to be at 50% by default.
        mBatteryStatus = new Intent();
        mBatteryStatus.putExtra(BatteryManager.EXTRA_SCALE, 100);
        mBatteryStatus.putExtra(BatteryManager.EXTRA_LEVEL, 50);
        setBatteryStatus(BatteryManager.BATTERY_STATUS_UNKNOWN);
        doReturn(mBatteryStatus).when(mContext).registerReceiver(isNull(), any());

        // Set up connectivity manager.
        doReturn(null).when(mConnectivityManager).getActiveNetworkInfo();
        doReturn(mConnectivityManager)
                .when(mContext)
                .getSystemService(eq(Context.CONNECTIVITY_SERVICE));

        // Set up network info.
        setNetworkInfoConnectionType(ConnectivityManager.TYPE_DUMMY);

        // Set up network change notifier.
        NetworkChangeNotifier.resetInstanceForTests(mNetworkChangeNotifier);
        setNcnConnectionType(ConnectionType.CONNECTION_UNKNOWN);

        // Set up power manager.
        setPowerSaveMode(false);
        doReturn(mPowerManager).when(mContext).getSystemService(eq(Context.POWER_SERVICE));

        // Set up keyguard manager.
        setKeyguardManagerToNull();

        // Make sure context is shared with ContextUtils.
        ContextUtils.initApplicationContextForTests(mContext);
    }

    private void setBatteryStatus(int batteryStatus) {
        assert batteryStatus == BatteryManager.BATTERY_STATUS_CHARGING
                || batteryStatus == BatteryManager.BATTERY_STATUS_DISCHARGING
                || batteryStatus == BatteryManager.BATTERY_STATUS_FULL
                || batteryStatus == BatteryManager.BATTERY_STATUS_NOT_CHARGING
                || batteryStatus == BatteryManager.BATTERY_STATUS_UNKNOWN;

        mBatteryStatus.putExtra(BatteryManager.EXTRA_STATUS, batteryStatus);
    }

    private void setDeviceInIdleMode(boolean isIdle) {
        doReturn(isIdle).when(mPowerManager).isDeviceIdleMode();
    }

    private void setNcnConnectionType(@ConnectionType int connectionType) {
        doReturn(connectionType).when(mNetworkChangeNotifier).getCurrentConnectionType();
    }

    private void setPowerSaveMode(boolean isPowerSaveMode) {
        doReturn(isPowerSaveMode).when(mPowerManager).isPowerSaveMode();
    }

    private void setNetworkInfoConnectionType(int connectionType) {
        assert connectionType == ConnectivityManager.TYPE_WIFI
                || connectionType == ConnectivityManager.TYPE_MOBILE
                || connectionType == ConnectivityManager.TYPE_BLUETOOTH
                || connectionType == ConnectivityManager.TYPE_DUMMY;

        doReturn(true).when(mNetworkInfo).isConnectedOrConnecting();
        doReturn(connectionType).when(mNetworkInfo).getType();
    }

    private void setNetworkInfoToNull() {
        doReturn(null).when(mConnectivityManager).getActiveNetworkInfo();
    }

    private void setNetworkInfoDisconnected() {
        doReturn(mNetworkInfo).when(mConnectivityManager).getActiveNetworkInfo();
        doReturn(false).when(mNetworkInfo).isConnectedOrConnecting();
    }

    private void setIsKeyguardLocked(boolean isLocked) {
        doReturn(mKeyguardManager).when(mContext).getSystemService(eq(Context.KEYGUARD_SERVICE));
        doReturn(isLocked).when(mKeyguardManager).isKeyguardLocked();
    }

    private void setKeyguardManagerToNull() {
        doReturn(null).when(mContext).getSystemService(eq(Context.KEYGUARD_SERVICE));
    }

    private void setIsInteractive(boolean isInteractive) {
        assert VERSION.SDK_INT >= VERSION_CODES.KITKAT_WATCH;
        doReturn(isInteractive).when(mPowerManager).isInteractive();
    }

    @Test
    public void testDefaultConstructor() {
        DeviceConditions deviceConditions = new DeviceConditions();

        // Default values are equivalent to most restrictive conditions.
        assertEquals(ConnectionType.CONNECTION_UNKNOWN, deviceConditions.getNetConnectionType());
        assertTrue(deviceConditions.isActiveNetworkMetered());
        assertTrue(deviceConditions.isInPowerSaveMode());
        assertTrue(deviceConditions.isScreenOnAndUnlocked());
        assertFalse(deviceConditions.isPowerConnected());
        assertEquals(0, deviceConditions.getBatteryPercentage());
    }

    @Test
    public void testNoNpeOnNullBatteryStatus() {
        doReturn(null).when(mContext).registerReceiver(isNull(), any());

        DeviceConditions deviceConditions = DeviceConditions.getCurrent(mContext);

        assertNotNull("Device conditions should not be null.", deviceConditions);
        assertEquals(ConnectionType.CONNECTION_UNKNOWN, deviceConditions.getNetConnectionType());
        assertTrue(deviceConditions.isActiveNetworkMetered());
        assertTrue(deviceConditions.isInPowerSaveMode());
        assertTrue(deviceConditions.isScreenOnAndUnlocked());
        assertFalse(deviceConditions.isPowerConnected());
        assertEquals(0, deviceConditions.getBatteryPercentage());

        assertFalse(DeviceConditions.isCurrentlyPowerConnected(mContext));
        assertEquals(0, DeviceConditions.getCurrentBatteryPercentage(mContext));
    }

    @Test
    public void testBatteryPercentage() {
        // Uses default setup of 50%.
        DeviceConditions deviceConditions = DeviceConditions.getCurrent(mContext);
        assertNotNull(deviceConditions);
        assertEquals(50, deviceConditions.getBatteryPercentage());
        assertEquals(50, DeviceConditions.getCurrentBatteryPercentage(mContext));

        Intent intent = new Intent();
        intent.putExtra(BatteryManager.EXTRA_SCALE, 0);
        intent.putExtra(BatteryManager.EXTRA_LEVEL, 50);
        doReturn(intent).when(mContext).registerReceiver(isNull(), any());

        deviceConditions = DeviceConditions.getCurrent(mContext);
        assertNotNull(deviceConditions);
        assertEquals(0, deviceConditions.getBatteryPercentage());
        assertEquals(0, DeviceConditions.getCurrentBatteryPercentage(mContext));
    }

    @Test
    public void testPowerSaveMode() {
        try (BuildVersionHelper sdkHelper = new BuildVersionHelper()) {
            // We expect KitKat to never turn on the power safe mode.
            sdkHelper.setSdkVersion(VERSION_CODES.KITKAT);
            setPowerSaveMode(false);
            assertFalse(DeviceConditions.isCurrentlyInPowerSaveMode(mContext));
            assertFalse(DeviceConditions.getCurrent(mContext).isInPowerSaveMode());

            setPowerSaveMode(true);
            assertFalse(DeviceConditions.isCurrentlyInPowerSaveMode(mContext));
            assertFalse(DeviceConditions.getCurrent(mContext).isInPowerSaveMode());

            // But it should be on LOLLIPOP+.
            sdkHelper.setSdkVersion(VERSION_CODES.LOLLIPOP);
            setPowerSaveMode(true);
            assertTrue(DeviceConditions.isCurrentlyInPowerSaveMode(mContext));
            assertTrue(DeviceConditions.getCurrent(mContext).isInPowerSaveMode());

            // Unless PowerManager says it is not.
            setPowerSaveMode(false);
            assertFalse(DeviceConditions.isCurrentlyInPowerSaveMode(mContext));
            assertFalse(DeviceConditions.getCurrent(mContext).isInPowerSaveMode());
        }
    }

    @Test
    public void testPowerConnected() {
        setBatteryStatus(BatteryManager.BATTERY_STATUS_UNKNOWN);
        assertFalse(DeviceConditions.isCurrentlyPowerConnected(mContext));
        assertFalse(DeviceConditions.getCurrent(mContext).isPowerConnected());

        setBatteryStatus(BatteryManager.BATTERY_STATUS_NOT_CHARGING);
        assertFalse(DeviceConditions.isCurrentlyPowerConnected(mContext));
        assertFalse(DeviceConditions.getCurrent(mContext).isPowerConnected());

        setBatteryStatus(BatteryManager.BATTERY_STATUS_DISCHARGING);
        assertFalse(DeviceConditions.isCurrentlyPowerConnected(mContext));
        assertFalse(DeviceConditions.getCurrent(mContext).isPowerConnected());

        setBatteryStatus(BatteryManager.BATTERY_STATUS_CHARGING);
        assertTrue(DeviceConditions.isCurrentlyPowerConnected(mContext));
        assertTrue(DeviceConditions.getCurrent(mContext).isPowerConnected());

        setBatteryStatus(BatteryManager.BATTERY_STATUS_FULL);
        assertTrue(DeviceConditions.isCurrentlyPowerConnected(mContext));
        assertTrue(DeviceConditions.getCurrent(mContext).isPowerConnected());
    }

    @Test
    public void testIsInIdleMode() {
        try (BuildVersionHelper sdkHelper = new BuildVersionHelper()) {
            // We expect KitKat to never indicate being in idle mode.
            sdkHelper.setSdkVersion(VERSION_CODES.KITKAT);
            setDeviceInIdleMode(false);
            assertFalse(DeviceConditions.isCurrentlyInIdleMode(mContext));

            setDeviceInIdleMode(true);
            assertFalse(DeviceConditions.isCurrentlyInIdleMode(mContext));

            // We expect LOLLIPOP to never indicate being in idle mode.
            sdkHelper.setSdkVersion(VERSION_CODES.LOLLIPOP);
            setDeviceInIdleMode(false);
            assertFalse(DeviceConditions.isCurrentlyInIdleMode(mContext));

            setDeviceInIdleMode(true);
            assertFalse(DeviceConditions.isCurrentlyInIdleMode(mContext));

            // We expect LOLLIPOP to never indicate being in idle mode.
            sdkHelper.setSdkVersion(VERSION_CODES.LOLLIPOP_MR1);
            setDeviceInIdleMode(false);
            assertFalse(DeviceConditions.isCurrentlyInIdleMode(mContext));

            setDeviceInIdleMode(true);
            assertFalse(DeviceConditions.isCurrentlyInIdleMode(mContext));

            // But it should be on MARSHMALLOW+.
            sdkHelper.setSdkVersion(VERSION_CODES.M);
            setDeviceInIdleMode(false);
            assertFalse(DeviceConditions.isCurrentlyInIdleMode(mContext));

            setDeviceInIdleMode(true);
            assertTrue(DeviceConditions.isCurrentlyInIdleMode(mContext));
        }
    }

    @Test
    public void testForceConnectionTypeNoneForTesting() {
        DeviceConditions.sForceNoConnectionForTesting = true;
        assertEquals(ConnectionType.CONNECTION_NONE,
                DeviceConditions.getCurrentNetConnectionType(mContext));
        assertEquals(ConnectionType.CONNECTION_NONE,
                DeviceConditions.getCurrent(mContext).getNetConnectionType());
        DeviceConditions.sForceNoConnectionForTesting = false;
    }

    @Test
    public void testNcnConnectionType() {
        @ConnectionType
        int[] connectionTypes =
                new int[] {ConnectionType.CONNECTION_UNKNOWN, ConnectionType.CONNECTION_ETHERNET,
                        ConnectionType.CONNECTION_WIFI, ConnectionType.CONNECTION_2G,
                        ConnectionType.CONNECTION_3G, ConnectionType.CONNECTION_4G,
                        ConnectionType.CONNECTION_NONE, ConnectionType.CONNECTION_BLUETOOTH};
        assertEquals(ConnectionType.CONNECTION_LAST + 1, connectionTypes.length);

        for (@ConnectionType int connectionType : connectionTypes) {
            setNcnConnectionType(connectionType);
            assertEquals(connectionType, DeviceConditions.getCurrentNetConnectionType(mContext));
            assertEquals(
                    connectionType, DeviceConditions.getCurrent(mContext).getNetConnectionType());
        }
    }

    @Test
    public void testActiveNetworkConnectionType_NoNative() {
        // Make sure NCN is null, which simulates native not being loaded.
        NetworkChangeNotifier.resetInstanceForTests(null);

        setNetworkInfoToNull();
        assertEquals(ConnectionType.CONNECTION_NONE,
                DeviceConditions.getCurrentNetConnectionType(mContext));
        assertEquals(ConnectionType.CONNECTION_NONE,
                DeviceConditions.getCurrent(mContext).getNetConnectionType());

        setNetworkInfoDisconnected();
        assertEquals(ConnectionType.CONNECTION_NONE,
                DeviceConditions.getCurrentNetConnectionType(mContext));
        assertEquals(ConnectionType.CONNECTION_NONE,
                DeviceConditions.getCurrent(mContext).getNetConnectionType());

        setNetworkInfoConnectionType(ConnectivityManager.TYPE_WIFI);
        assertEquals(ConnectionType.CONNECTION_WIFI,
                DeviceConditions.getCurrentNetConnectionType(mContext));
        assertEquals(ConnectionType.CONNECTION_WIFI,
                DeviceConditions.getCurrent(mContext).getNetConnectionType());

        // Connection of type mobile is presumed to have lowest possible quality, i.e. 2G.
        // This inference is only relevant when native is not loaded.
        setNetworkInfoConnectionType(ConnectivityManager.TYPE_MOBILE);
        assertEquals(ConnectionType.CONNECTION_2G,
                DeviceConditions.getCurrentNetConnectionType(mContext));
        assertEquals(ConnectionType.CONNECTION_2G,
                DeviceConditions.getCurrent(mContext).getNetConnectionType());

        setNetworkInfoConnectionType(ConnectivityManager.TYPE_BLUETOOTH);
        assertEquals(ConnectionType.CONNECTION_BLUETOOTH,
                DeviceConditions.getCurrentNetConnectionType(mContext));
        assertEquals(ConnectionType.CONNECTION_BLUETOOTH,
                DeviceConditions.getCurrent(mContext).getNetConnectionType());

        setNetworkInfoConnectionType(ConnectivityManager.TYPE_DUMMY);
        assertEquals(ConnectionType.CONNECTION_UNKNOWN,
                DeviceConditions.getCurrentNetConnectionType(mContext));
        assertEquals(ConnectionType.CONNECTION_UNKNOWN,
                DeviceConditions.getCurrent(mContext).getNetConnectionType());
    }

    @Test
    public void testIsScreenOnAndUnlocked() {
        try (BuildVersionHelper sdkHelper = new BuildVersionHelper()) {
            setKeyguardManagerToNull();
            assertFalse(DeviceConditions.isCurrentlyScreenOnAndUnlocked(mContext));

            setIsKeyguardLocked(true);
            assertFalse(DeviceConditions.isCurrentlyScreenOnAndUnlocked(mContext));

            setIsKeyguardLocked(false);
            // Version code has to be above KIT_KAT for this.
            sdkHelper.setSdkVersion(VERSION_CODES.LOLLIPOP);
            setIsInteractive(false);
            assertFalse(DeviceConditions.isCurrentlyScreenOnAndUnlocked(mContext));

            setIsInteractive(true);
            assertTrue(DeviceConditions.isCurrentlyScreenOnAndUnlocked(mContext));
        }
    }

    @Test
    public void testSettingConnectionType() {
        // This is used by ShadowDeviceConditions.
        DeviceConditions conditions = DeviceConditions.getCurrent(mContext);
        conditions.setNetworkConnectionType(ConnectionType.CONNECTION_ETHERNET);
        assertEquals(ConnectionType.CONNECTION_ETHERNET, conditions.getNetConnectionType());
    }
}