// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauthn;

import android.annotation.TargetApi;
import android.app.KeyguardManager;
import android.app.Notification;
import android.app.PendingIntent;
import android.bluetooth.BluetoothAdapter;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.os.Build;
import android.os.Bundle;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.core.app.NotificationCompat;
import androidx.core.app.NotificationManagerCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentTransaction;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.modules.cablev2_authenticator.Cablev2AuthenticatorModule;

/**
 * Provides a UI that attempts to install the caBLEv2 Authenticator module. If already installed, or
 * successfully installed, it replaces itself in the back-stack with the authenticator UI.
 *
 * This code lives in the base module, i.e. is _not_ part of the dynamically-loaded module.
 *
 * This does not use {@link ModuleInstallUi} because it needs to integrate into the Fragment-based
 * settings UI, while {@link ModuleInstallUi} assumes that the UI does in a {@link Tab}.
 */
public class CableAuthenticatorModuleProvider extends Fragment {
    // TAG is subject to a 20 character limit.
    private static final String TAG = "CableAuthModuleProv";

    // NOTIFICATION_TIMEOUT_SECS is the number of seconds that a notification
    // will exist for. This stop ignored notifications hanging around.
    private static final int NOTIFICATION_TIMEOUT_SECS = 60;

    // NETWORK_CONTEXT_KEY is the key under which a pointer to a NetworkContext
    // is passed (as a long) in the arguments {@link Bundle} to the {@link
    // Fragment} in the module.
    private static final String NETWORK_CONTEXT_KEY =
            "org.chromium.chrome.modules.cablev2_authenticator.NetworkContext";
    private static final String REGISTRATION_KEY =
            "org.chromium.chrome.modules.cablev2_authenticator.Registration";
    private static final String ACTIVITY_CLASS_NAME =
            "org.chromium.chrome.browser.webauth.authenticator.CableAuthenticatorActivity";
    private static final String SECRET_KEY =
            "org.chromium.chrome.modules.cablev2_authenticator.Secret";

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        final Context context = getContext();

        LinearLayout layout = new LinearLayout(context);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER_HORIZONTAL);

        if (Cablev2AuthenticatorModule.isInstalled()) {
            showModule();
        } else {
            Cablev2AuthenticatorModule.install((success) -> {
                if (!success) {
                    Log.e(TAG, "Failed to install caBLE DFM");
                    getActivity().finish();
                    return;
                }
                showModule();
            });
        }

        return layout;
    }

    private void showModule() {
        FragmentTransaction transaction = getParentFragmentManager().beginTransaction();
        Fragment fragment = Cablev2AuthenticatorModule.getImpl().getFragment();
        Bundle arguments = getArguments();
        if (arguments == null) {
            arguments = new Bundle();
        }
        arguments.putLong(NETWORK_CONTEXT_KEY,
                CableAuthenticatorModuleProviderJni.get().getSystemNetworkContext());
        arguments.putLong(
                REGISTRATION_KEY, CableAuthenticatorModuleProviderJni.get().getRegistration());
        arguments.putByteArray(SECRET_KEY, CableAuthenticatorModuleProviderJni.get().getSecret());
        fragment.setArguments(arguments);
        transaction.replace(getId(), fragment);
        // This fragment is deliberately not added to the back-stack here so
        // that it appears to have been "replaced" by the authenticator UI.
        transaction.commit();
    }

    /**
     * onCloudMessage is called by native code when a GCM message is received.
     *
     * @param event a pointer to a |device::cablev2::authenticator::Registration::Event| which this
     *         code takes ownership of.
     */
    @CalledByNative
    public static void onCloudMessage(byte[] serializedEvent, boolean isMakeCredential) {
        // Show a notification to the user. If tapped then an instance of this
        // class will be created in FCM mode.
        Context context = ContextUtils.getApplicationContext();
        Resources resources = context.getResources();

        Intent intent;
        try {
            intent = new Intent(context, Class.forName(ACTIVITY_CLASS_NAME));
        } catch (ClassNotFoundException e) {
            Log.e(TAG, "Failed to find class " + ACTIVITY_CLASS_NAME);
            return;
        }

        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        Bundle bundle = new Bundle();
        bundle.putBoolean("org.chromium.chrome.modules.cablev2_authenticator.FCM", true);
        bundle.putByteArray(
                "org.chromium.chrome.modules.cablev2_authenticator.EVENT", serializedEvent);
        intent.putExtra("show_fragment_args", bundle);
        // Notifications must have a process-global ID. We never use this, but it prevents multiple
        // notifications from existing at once.
        final int id = 424386536;

        PendingIntent pendingIntent = PendingIntent.getActivity(context, id, intent,
                PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);

        String title = null;
        String body = null;
        if (isMakeCredential) {
            title = resources.getString(R.string.cablev2_make_credential_notification_title);
            body = resources.getString(R.string.cablev2_make_credential_notification_body);
        } else {
            title = resources.getString(R.string.cablev2_get_assertion_notification_title);
            body = resources.getString(R.string.cablev2_get_assertion_notification_body);
        }

        Notification notification = NotificationWrapperBuilderFactory
                                            .createNotificationWrapperBuilder(
                                                    /*preferCompat=*/true,
                                                    ChromeChannelDefinitions.ChannelId.SECURITY_KEY)
                                            .setAutoCancel(true)
                                            .setCategory(Notification.CATEGORY_MESSAGE)
                                            .setContentIntent(pendingIntent)
                                            .setContentText(body)
                                            .setContentTitle(title)
                                            .setPriorityBeforeO(NotificationCompat.PRIORITY_MAX)
                                            .setSmallIcon(R.drawable.ic_chrome)
                                            .setTimeoutAfter(NOTIFICATION_TIMEOUT_SECS * 1000)
                                            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
                                            .build();

        NotificationManagerCompat notificationManager = NotificationManagerCompat.from(context);
        notificationManager.notify(
                NotificationConstants.NOTIFICATION_ID_SECURITY_KEY, notification);
    }

    @CalledByNative
    public static boolean canDeviceSupportCable() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N
                || BluetoothAdapter.getDefaultAdapter() == null) {
            return false;
        }

        // GMSCore will immediately fail all requests if a screenlock
        // isn't configured.
        return hasScreenLockConfigured();
    }

    // canDeviceSupportCable has checked that the system is >= N (API level 24)
    // before calling this function.
    @TargetApi(24)
    private static boolean hasScreenLockConfigured() {
        KeyguardManager km =
                (KeyguardManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.KEYGUARD_SERVICE);
        return km.isDeviceSecure();
    }

    @NativeMethods
    interface Natives {
        // getSystemNetworkContext returns a pointer, encoded in a long, to the
        // global NetworkContext for system services that hangs off
        // |g_browser|. This is needed because //chrome/browser, being a
        // static_library, cannot be depended on by another component thus we
        // pass this value into the feature module.
        long getSystemNetworkContext();
        // getRegistration returns a pointer to the global
        // device::cablev2::authenticator::Registration.
        long getRegistration();
        // getSecret returns a 32-byte secret from which can be derived the
        // key and shared secret that were advertised via Sync.
        byte[] getSecret();
        // freeEvent releases resources used by the given event.
        void freeEvent(long event);
    }
}
