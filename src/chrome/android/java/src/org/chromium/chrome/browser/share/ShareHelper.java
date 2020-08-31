// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.PendingIntent;
import android.content.ActivityNotFoundException;
import android.content.BroadcastReceiver;
import android.content.ClipData;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnDismissListener;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.content.res.Resources.NotFoundException;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.Pair;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.IntentCallback;

import java.util.Collections;
import java.util.List;

/**
 * A helper class that helps to start an intent to share titles and URLs.
 */
public class ShareHelper {
    static final String EXTERNAL_APP_SHARING_PREF_FILE_NAME = "external_app_sharing";

    /** Interface that receives intents for testing (to fake out actually sending them). */
    public interface FakeIntentReceiver {
        /** Sets the intent to send back in the broadcast. */
        public void setIntentToSendBack(Intent intent);

        /** Called when a custom chooser dialog is shown. */
        public void onCustomChooserShown(AlertDialog dialog);

        /**
         * Simulates firing the given intent, without actually doing so.
         *
         * @param context The context that will receive broadcasts from the simulated activity.
         * @param intent The intent to send to the system.
         */
        public void fireIntent(Context context, Intent intent);
    }

    private static final String TAG = "share";

    /** The task ID of the activity that triggered the share action. */
    public static final String EXTRA_TASK_ID = "org.chromium.chrome.extra.TASK_ID";

    private static final String EXTRA_SHARE_SCREENSHOT_AS_STREAM = "share_screenshot_as_stream";

    /** Force the use of a Chrome-specific intent chooser, not the system chooser. */
    private static boolean sForceCustomChooserForTesting;
    private static boolean sIgnoreActivityNotFoundException;

    /** If non-null, will be used instead of the real activity. */
    private static FakeIntentReceiver sFakeIntentReceiverForTesting;

    private ShareHelper() {}

    /*
     * If true, dont throw an ActivityNotFoundException if it is fired when attempting
     * to intent into lens.
     * @param shouldIgnore Whether to catch the exception.
     */
    @VisibleForTesting
    public static void setIgnoreActivityNotFoundExceptionForTesting(boolean shouldIgnore) {
        sIgnoreActivityNotFoundException = shouldIgnore;
    }

    /**
     * Fire the intent to share content with the target app.
     *
     * @param window The current window.
     * @param intent The intent to fire.
     * @param callback The callback to be triggered when the calling activity has finished.  This
     *                 allows the target app to identify Chrome as the source.
     */
    private static void fireIntent(
            WindowAndroid window, Intent intent, @Nullable IntentCallback callback) {
        if (sFakeIntentReceiverForTesting != null) {
            sFakeIntentReceiverForTesting.fireIntent(ContextUtils.getApplicationContext(), intent);
        } else if (callback != null) {
            window.showIntent(intent, callback, null);
        } else {
            // TODO(tedchoc): Allow startActivity w/o intent via Window.
            Activity activity = window.getActivity().get();
            activity.startActivity(intent);
        }
    }

    /**
     * Force the use of a Chrome-specific intent chooser, not the system chooser.
     *
     * This emulates the behavior on pre Lollipop-MR1 systems, where the system chooser is not
     * available.
     */
    public static void setForceCustomChooserForTesting(boolean enabled) {
        sForceCustomChooserForTesting = enabled;
    }

    /**
     * Uses a FakeIntentReceiver instead of actually sending intents to the system.
     *
     * @param receiver The object to send intents to. If null, resets back to the default behavior
     *                 (really send intents).
     */
    public static void setFakeIntentReceiverForTesting(FakeIntentReceiver receiver) {
        sFakeIntentReceiverForTesting = receiver;
    }

    /**
     * Callback interface for when a target is chosen.
     */
    public static interface TargetChosenCallback {
        /**
         * Called when the user chooses a target in the share dialog.
         *
         * Note that if the user cancels the share dialog, this callback is never called.
         */
        public void onTargetChosen(ComponentName chosenComponent);

        /**
         * Called when the user cancels the share dialog.
         *
         * Guaranteed that either this, or onTargetChosen (but not both) will be called, eventually.
         */
        public void onCancel();
    }

    /**
     * Receiver to record the chosen component when sharing an Intent.
     */
    static class TargetChosenReceiver extends BroadcastReceiver implements IntentCallback {
        private static final String EXTRA_RECEIVER_TOKEN = "receiver_token";
        private static final Object LOCK = new Object();

        private static String sTargetChosenReceiveAction;
        private static TargetChosenReceiver sLastRegisteredReceiver;

        private final boolean mSaveLastUsed;
        @Nullable
        private TargetChosenCallback mCallback;

        private TargetChosenReceiver(boolean saveLastUsed,
                                     @Nullable TargetChosenCallback callback) {
            mSaveLastUsed = saveLastUsed;
            mCallback = callback;
        }

        static boolean isSupported() {
            return !sForceCustomChooserForTesting
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP_MR1;
        }

        @TargetApi(Build.VERSION_CODES.LOLLIPOP_MR1)
        static void sendChooserIntent(boolean saveLastUsed, WindowAndroid window,
                Intent sharingIntent, @Nullable TargetChosenCallback callback) {
            final String packageName = ContextUtils.getApplicationContext().getPackageName();
            synchronized (LOCK) {
                if (sTargetChosenReceiveAction == null) {
                    sTargetChosenReceiveAction =
                            packageName + "/" + TargetChosenReceiver.class.getName() + "_ACTION";
                }
                Context context = ContextUtils.getApplicationContext();
                if (sLastRegisteredReceiver != null) {
                    context.unregisterReceiver(sLastRegisteredReceiver);
                    // Must cancel the callback (to satisfy guarantee that exactly one method of
                    // TargetChosenCallback is called).
                    // TODO(mgiuca): This should be called immediately upon cancelling the chooser,
                    // not just when the next share takes place (https://crbug.com/636274).
                    sLastRegisteredReceiver.cancel();
                }
                sLastRegisteredReceiver = new TargetChosenReceiver(saveLastUsed, callback);
                context.registerReceiver(
                        sLastRegisteredReceiver, new IntentFilter(sTargetChosenReceiveAction));
            }

            Intent intent = new Intent(sTargetChosenReceiveAction);
            intent.setPackage(packageName);
            intent.putExtra(EXTRA_RECEIVER_TOKEN, sLastRegisteredReceiver.hashCode());
            Activity activity = window.getActivity().get();
            final PendingIntent pendingIntent = PendingIntent.getBroadcast(activity, 0, intent,
                    PendingIntent.FLAG_CANCEL_CURRENT | PendingIntent.FLAG_ONE_SHOT);
            Intent chooserIntent = Intent.createChooser(sharingIntent,
                    activity.getString(R.string.share_link_chooser_title),
                    pendingIntent.getIntentSender());
            if (sFakeIntentReceiverForTesting != null) {
                sFakeIntentReceiverForTesting.setIntentToSendBack(intent);
            }
            fireIntent(window, chooserIntent, sLastRegisteredReceiver);
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            synchronized (LOCK) {
                if (sLastRegisteredReceiver != this) return;
                ContextUtils.getApplicationContext().unregisterReceiver(sLastRegisteredReceiver);
                sLastRegisteredReceiver = null;
            }
            if (!intent.hasExtra(EXTRA_RECEIVER_TOKEN)
                    || intent.getIntExtra(EXTRA_RECEIVER_TOKEN, 0) != this.hashCode()) {
                return;
            }

            ComponentName target = intent.getParcelableExtra(Intent.EXTRA_CHOSEN_COMPONENT);
            if (mCallback != null) {
                mCallback.onTargetChosen(target);
                mCallback = null;
            }
            if (mSaveLastUsed && target != null) {
                setLastShareComponentName(target);
            }
        }

        @Override
        public void onIntentCompleted(WindowAndroid window, int resultCode, Intent data) {
            if (resultCode == Activity.RESULT_CANCELED) {
                cancel();
            }
        }

        private void cancel() {
            if (mCallback != null) {
                mCallback.onCancel();
                mCallback = null;
            }
        }
    }

    /**
     * Share directly with the last used share target.
     * @param params The container holding the share parameters.
     */
    public static void shareDirectly(ShareParams params) {
        assert params.shareDirectly();
        ComponentName component = getLastShareComponentName();
        if (component == null) return;
        assert params.getCallback() == null;
        makeIntentAndShare(params, component);
    }

    /**
     * Share an image URI with an activity identified by the provided Component Name.
     * @param window The current window.
     * @param name The component name of the activity to share the image with.
     * @param imageUri The url to share with the external activity.
     */
    public static void shareImage(
            final WindowAndroid window, final ComponentName name, Uri imageUri) {
        Intent shareIntent = getShareImageIntent(imageUri);
        if (name == null) {
            if (TargetChosenReceiver.isSupported()) {
                TargetChosenReceiver.sendChooserIntent(true, window, shareIntent, null);
            } else {
                Intent chooserIntent = Intent.createChooser(shareIntent,
                        window.getActivity().get().getString(R.string.share_link_chooser_title));
                fireIntent(window, chooserIntent, null);
            }
        } else {
            shareIntent.setComponent(name);
            fireIntent(window, shareIntent, null);
        }
    }

    /**
     * Share an image URI with Google Lens.
     * @param window The current window.
     * @param imageUri The url to share with the app.
     * @param isIncognito Whether the current tab is in incognito mode.
     * @param srcUrl The 'src' attribute of the image.
     * @param titleOrAltText The 'title' or, if empty, the 'alt' attribute of the image.
     */
    public static void shareImageWithGoogleLens(final WindowAndroid window, Uri imageUri,
            boolean isIncognito, String srcUrl, String titleOrAltText) {
        Intent shareIntent = LensUtils.getShareWithGoogleLensIntent(
                imageUri, isIncognito, SystemClock.elapsedRealtimeNanos(), srcUrl, titleOrAltText);
        try {
            // Pass an empty callback to ensure the triggered activity can identify the source
            // of the intent (startActivityForResult allows app identification).
            fireIntent(window, shareIntent, (w, resultCode, data) -> {});
        } catch (ActivityNotFoundException e) {
            // The initial version check should guarantee that the activity is available. However,
            // the exception may be thrown in test environments after mocking out the version check.
            if (Boolean.TRUE.equals(sIgnoreActivityNotFoundException)) return;
            throw e;
        }
    }

    /**
     * Creates and shows a share intent picker dialog.
     *
     * @param params The container holding the share parameters.
     */
    static void showShareDialog(final ShareParams params) {
        Activity activity = params.getWindow().getActivity().get();
        final TargetChosenCallback callback = params.getCallback();
        Intent intent = getShareLinkAppCompatibilityIntent();
        PackageManager manager = activity.getPackageManager();
        List<ResolveInfo> resolveInfoList = PackageManagerUtils.queryIntentActivities(intent, 0);
        assert resolveInfoList.size() > 0;
        if (resolveInfoList.size() == 0) return;
        Collections.sort(resolveInfoList, new ResolveInfo.DisplayNameComparator(manager));

        final ShareDialogAdapter adapter =
                new ShareDialogAdapter(activity, manager, resolveInfoList);
        AlertDialog.Builder builder = new UiUtils.CompatibleAlertDialogBuilder(
                activity, R.style.Theme_Chromium_AlertDialog);
        builder.setTitle(activity.getString(R.string.share_link_chooser_title));
        builder.setAdapter(adapter, null);

        // Need a mutable object to record whether the callback has been fired.
        final boolean[] callbackCalled = new boolean[1];

        final AlertDialog dialog = builder.create();
        dialog.show();
        dialog.getListView().setOnItemClickListener(new OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                ResolveInfo info = adapter.getItem(position);
                ActivityInfo ai = info.activityInfo;
                ComponentName component =
                        new ComponentName(ai.applicationInfo.packageName, ai.name);

                if (callback != null && !callbackCalled[0]) {
                    callback.onTargetChosen(component);
                    callbackCalled[0] = true;
                }
                if (params.saveLastUsed()) {
                    setLastShareComponentName(component);
                }
                makeIntentAndShare(params, component);
                dialog.dismiss();
            }
        });

        dialog.setOnDismissListener(new OnDismissListener() {
            @Override
            public void onDismiss(DialogInterface dialog) {
                if (callback != null && !callbackCalled[0]) {
                    callback.onCancel();
                    callbackCalled[0] = true;
                }
            }
        });

        if (sFakeIntentReceiverForTesting != null) {
            sFakeIntentReceiverForTesting.onCustomChooserShown(dialog);
        }
    }

    /**
     * Show the default share sheet. On L+ this is the Android system share sheet on K and below
     * this is a custom share dialog.
     * @param params The share parameters.
     */
    static void showDefaultShareUi(ShareParams params) {
        if (TargetChosenReceiver.isSupported()) {
            // On L+ open system share sheet.
            makeIntentAndShare(params, null);
        } else {
            // On K and below open custom share dialog.
            showShareDialog(params);
        }
    }

    static void makeIntentAndShare(ShareParams params, @Nullable ComponentName component) {
        Intent intent = getShareLinkIntent(params);
        intent.addFlags(Intent.FLAG_ACTIVITY_FORWARD_RESULT | Intent.FLAG_ACTIVITY_PREVIOUS_IS_TOP);
        intent.setComponent(component);
        if (intent.getComponent() != null) {
            fireIntent(params.getWindow(), intent, null);
        } else {
            assert TargetChosenReceiver.isSupported();
            TargetChosenReceiver.sendChooserIntent(
                    params.saveLastUsed(), params.getWindow(), intent, params.getCallback());
        }
    }

    /**
     * Set the icon and the title for the menu item used for direct share.
     * @param context The activity context used to retrieve resources.
     * @param item The menu item that is used for direct share
     */
    public static void configureDirectShareMenuItem(Context context, MenuItem item) {
        Intent shareIntent = getShareLinkAppCompatibilityIntent();
        Pair<Drawable, CharSequence> directShare = getShareableIconAndName(shareIntent);
        Drawable directShareIcon = directShare.first;
        CharSequence directShareTitle = directShare.second;

        item.setIcon(directShareIcon);
        if (directShareTitle != null) {
            item.setTitle(
                    context.getString(R.string.accessibility_menu_share_via, directShareTitle));
        }
    }

    /**
     * Get the icon and name of the most recently shared app by certain app.
     * @param shareIntent Intent used to get list of apps support sharing.
     * @return The Image and the String of the recently shared Icon.
     */
    public static Pair<Drawable, CharSequence> getShareableIconAndName(Intent shareIntent) {
        Drawable directShareIcon = null;
        CharSequence directShareTitle = null;

        final ComponentName component = getLastShareComponentName();
        boolean isComponentValid = false;
        if (component != null) {
            shareIntent.setPackage(component.getPackageName());
            List<ResolveInfo> resolveInfoList =
                    PackageManagerUtils.queryIntentActivities(shareIntent, 0);
            for (ResolveInfo info : resolveInfoList) {
                ActivityInfo ai = info.activityInfo;
                if (component.equals(new ComponentName(ai.applicationInfo.packageName, ai.name))) {
                    isComponentValid = true;
                    break;
                }
            }
        }
        if (isComponentValid) {
            boolean retrieved = false;
            final PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
            try {
                // TODO(dtrainor): Make asynchronous and have a callback to update the menu.
                // https://crbug.com/729737
                try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                    directShareIcon = pm.getActivityIcon(component);
                    directShareTitle = pm.getActivityInfo(component, 0).loadLabel(pm);
                }
                retrieved = true;
            } catch (NameNotFoundException exception) {
                // Use the default null values.
            }
            RecordHistogram.recordBooleanHistogram(
                    "Android.IsLastSharedAppInfoRetrieved", retrieved);
        }

        return new Pair<>(directShareIcon, directShareTitle);
    }

    /**
     * Stores the component selected for sharing last time share was called by certain app.
     *
     * This method is public since it is used in tests to avoid creating share dialog.
     * @param component The {@link ComponentName} of the app selected for sharing.
     */
    @VisibleForTesting
    public static void setLastShareComponentName(ComponentName component) {
        SharedPreferencesManager preferencesManager = SharedPreferencesManager.getInstance();
        preferencesManager.writeString(
                ChromePreferenceKeys.SHARING_LAST_SHARED_PACKAGE_NAME, component.getPackageName());
        preferencesManager.writeString(
                ChromePreferenceKeys.SHARING_LAST_SHARED_CLASS_NAME, component.getClassName());
    }

    @VisibleForTesting
    public static Intent getShareLinkIntent(ShareParams params) {
        final boolean isFileShare = (params.getFileUris() != null);
        final boolean isMultipleFileShare = isFileShare && (params.getFileUris().size() > 1);
        final String action =
                isMultipleFileShare ? Intent.ACTION_SEND_MULTIPLE : Intent.ACTION_SEND;
        Intent intent = new Intent(action);
        intent.addFlags(ApiCompatibilityUtils.getActivityNewDocumentFlag());
        intent.putExtra(EXTRA_TASK_ID, params.getWindow().getActivity().get().getTaskId());

        Uri screenshotUri = params.getScreenshotUri();
        if (screenshotUri != null) {
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            // To give read access to an Intent target, we need to put |screenshotUri| in clipData
            // because adding Intent.FLAG_GRANT_READ_URI_PERMISSION doesn't work for
            // EXTRA_SHARE_SCREENSHOT_AS_STREAM.
            intent.setClipData(ClipData.newRawUri("", screenshotUri));
            intent.putExtra(EXTRA_SHARE_SCREENSHOT_AS_STREAM, screenshotUri);
        }

        if (params.getOfflineUri() != null) {
            intent.putExtra(Intent.EXTRA_SUBJECT, params.getTitle());
            intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            intent.putExtra(Intent.EXTRA_STREAM, params.getOfflineUri());
            intent.addCategory(Intent.CATEGORY_DEFAULT);
            intent.setType("multipart/related");
        } else {
            if (!TextUtils.equals(params.getText(), params.getTitle())) {
                intent.putExtra(Intent.EXTRA_SUBJECT, params.getTitle());
            }
            intent.putExtra(Intent.EXTRA_TEXT, params.getText());

            if (isFileShare) {
                intent.setType(params.getFileContentType());
                intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);

                if (isMultipleFileShare) {
                    intent.putParcelableArrayListExtra(Intent.EXTRA_STREAM, params.getFileUris());
                } else {
                    intent.putExtra(Intent.EXTRA_STREAM, params.getFileUris().get(0));
                }
            } else {
                intent.setType("text/plain");
            }
        }

        return intent;
    }

    /**
     * Creates an Intent to share an image.
     * @param imageUri The Uri of the image.
     * @return The Intent used to share the image.
     */
    public static Intent getShareImageIntent(Uri imageUri) {
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.addFlags(ApiCompatibilityUtils.getActivityNewDocumentFlag());
        intent.setType("image/jpeg");
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        intent.putExtra(Intent.EXTRA_STREAM, imageUri);
        return intent;
    }

    /**
     * Convenience method to create an Intent to retrieve all the apps support sharing text.
     */
    public static Intent getShareLinkAppCompatibilityIntent() {
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.addFlags(ApiCompatibilityUtils.getActivityNewDocumentFlag());
        intent.putExtra(Intent.EXTRA_SUBJECT, "");
        intent.putExtra(Intent.EXTRA_TEXT, "");
        intent.setType("text/plain");
        return intent;
    }

    /**
     * Gets the {@link ComponentName} of the app that was used to last share.
     */
    @Nullable
    public static ComponentName getLastShareComponentName() {
        SharedPreferencesManager preferencesManager = SharedPreferencesManager.getInstance();
        String packageName = preferencesManager.readString(
                ChromePreferenceKeys.SHARING_LAST_SHARED_PACKAGE_NAME, null);
        String className = preferencesManager.readString(
                ChromePreferenceKeys.SHARING_LAST_SHARED_CLASS_NAME, null);
        return createComponentName(packageName, className);
    }

    private static ComponentName createComponentName(String packageName, String className) {
        if (packageName == null || className == null) return null;
        return new ComponentName(packageName, className);
    }

    private static SharedPreferences getExternalAppSharingSharedPreferences() {
        return ContextUtils.getApplicationContext().getSharedPreferences(
                EXTERNAL_APP_SHARING_PREF_FILE_NAME, Context.MODE_PRIVATE);
    }

    /**
     * Loads the icon for the provided ResolveInfo.
     * @param info The ResolveInfo to load the icon for.
     * @param manager The package manager to use to load the icon.
     */
    static Drawable loadIconForResolveInfo(ResolveInfo info, PackageManager manager) {
        try {
            final int iconRes = info.getIconResource();
            if (iconRes != 0) {
                Resources res = manager.getResourcesForApplication(info.activityInfo.packageName);
                Drawable icon = ApiCompatibilityUtils.getDrawable(res, iconRes);
                return icon;
            }
        } catch (NameNotFoundException | NotFoundException e) {
            // Could not find the icon. loadIcon call below will return the default app icon.
        }
        return info.loadIcon(manager);
    }
}
