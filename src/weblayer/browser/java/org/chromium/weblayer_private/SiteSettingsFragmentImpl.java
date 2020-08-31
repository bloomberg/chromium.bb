// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;

import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentController;
import androidx.fragment.app.FragmentHostCallback;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsPreferenceFragment;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory;
import org.chromium.weblayer_private.interfaces.IRemoteFragment;
import org.chromium.weblayer_private.interfaces.IRemoteFragmentClient;
import org.chromium.weblayer_private.interfaces.ISiteSettingsFragment;
import org.chromium.weblayer_private.interfaces.SiteSettingsFragmentArgs;
import org.chromium.weblayer_private.interfaces.SiteSettingsIntentHelper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

/**
 * WebLayer's implementation of the client library's SiteSettingsFragment.
 *
 * This class creates an instance of the Fragment given in its FRAGMENT_NAME argument, and forwards
 * all incoming lifecycle events from SiteSettingsFragment to it. Because Fragments created in
 * WebLayer use the AndroidX library from WebLayer's ClassLoader, we can't attach the Fragment
 * created here directly to the embedder's Fragment tree, and have to create a local
 * FragmentController to manage it.
 */
public class SiteSettingsFragmentImpl extends RemoteFragmentImpl {
    private static final String FRAGMENT_TAG = "site_settings_fragment";

    private final ProfileImpl mProfile;
    private final Class<? extends SiteSettingsPreferenceFragment> mFragmentClass;
    private final Bundle mFragmentArguments;

    // The embedder's original context object.
    private Context mEmbedderContext;

    // The WebLayer-wrapped context object. This context gets assets and resources from WebLayer,
    // not from the embedder. Use this for the most part, especially to resolve WebLayer-specific
    // resource IDs.
    private Context mContext;

    private FragmentController mFragmentController;

    /**
     * A fake FragmentActivity needed to make the Fragment system happy.
     *
     * PreferenceFragmentCompat calls Fragment.getActivity, which casts the Activity given to the
     * FragmentHostCallback to a FragmentActivity. Because of the AndroidX ClassLoader issue
     * mentioned aove, this cast will fail if we use the embedder's Activity because the
     * FragmentActivity it derives from lives in another ClassLoader.  This class exists to provide
     * a FragmentActivity for the Site Settings Fragments to run in, and forwards necessary methods
     * to the remote Activity.
     */
    private static class PassthroughFragmentActivity extends FragmentActivity
            implements PreferenceFragmentCompat.OnPreferenceStartFragmentCallback {
        private final SiteSettingsFragmentImpl mFragmentImpl;

        private PassthroughFragmentActivity(SiteSettingsFragmentImpl fragmentImpl) {
            mFragmentImpl = fragmentImpl;
            attachBaseContext(mFragmentImpl.getWebLayerContext());
        }

        @Override
        public Object getSystemService(String name) {
            if (Context.LAYOUT_INFLATER_SERVICE.equals(name)) {
                return getLayoutInflater();
            }
            return getEmbedderActivity().getSystemService(name);
        }

        @Override
        public LayoutInflater getLayoutInflater() {
            return (LayoutInflater) getBaseContext().getSystemService(
                    Context.LAYOUT_INFLATER_SERVICE);
        }

        @Override
        public Window getWindow() {
            return getEmbedderActivity().getWindow();
        }

        @Override
        public Context getApplicationContext() {
            return getEmbedderActivity().getApplicationContext();
        }

        @Override
        public void startActivity(Intent intent) {
            getEmbedderActivity().startActivity(intent);
        }

        @Override
        public void setTitle(int titleId) {
            getEmbedderActivity().setTitle(mFragmentImpl.getWebLayerContext().getString(titleId));
        }

        @Override
        public void setTitle(CharSequence title) {
            getEmbedderActivity().setTitle(title);
        }

        @Override
        public boolean onPreferenceStartFragment(
                PreferenceFragmentCompat caller, Preference preference) {
            // WebLayer's SiteSettingsActivity structures its arguments differently than the
            // implementation Fragments do. This is to avoid hardcoding implementation class names
            // in //components in the API, and because the Fragments in //components rely on
            // passing serialized Objects to each other, which aren't passable to the embedder
            // because they live in different ClassLoaders. This block of code translates the
            // Fragment arguments provided by the implementation Fragments to what the WebLayer API
            // expects, and then tells the client-side Activity to start the new Site Settings page.
            Intent intent;
            String newFragmentClassName = preference.getFragment();
            Bundle newFragmentArgs = preference.getExtras();
            if (newFragmentClassName.equals(SiteSettings.class.getName())) {
                intent = SiteSettingsIntentHelper.createIntentForCategoryList(
                        mFragmentImpl.getEmbedderContext(), mFragmentImpl.getProfile().getName());
            } else if (newFragmentClassName.equals(SingleCategorySettings.class.getName())) {
                intent = SiteSettingsIntentHelper.createIntentForSingleCategory(
                        mFragmentImpl.getEmbedderContext(), mFragmentImpl.getProfile().getName(),
                        newFragmentArgs.getString(SingleCategorySettings.EXTRA_CATEGORY),
                        newFragmentArgs.getString(SingleCategorySettings.EXTRA_TITLE));
            } else if (newFragmentClassName.equals(SingleWebsiteSettings.class.getName())) {
                WebsiteAddress address;
                if (newFragmentArgs.containsKey(SingleWebsiteSettings.EXTRA_SITE)) {
                    Website website = (Website) newFragmentArgs.getSerializable(
                            SingleWebsiteSettings.EXTRA_SITE);
                    address = website.getAddress();
                } else if (newFragmentArgs.containsKey(SingleWebsiteSettings.EXTRA_SITE_ADDRESS)) {
                    address = (WebsiteAddress) newFragmentArgs.getSerializable(
                            SingleWebsiteSettings.EXTRA_SITE_ADDRESS);
                } else {
                    throw new IllegalArgumentException("No website provided");
                }
                intent = SiteSettingsIntentHelper.createIntentForSingleWebsite(
                        mFragmentImpl.getEmbedderContext(), mFragmentImpl.getProfile().getName(),
                        address.getOrigin());
            } else {
                throw new IllegalArgumentException("Unsupported Fragment: " + newFragmentClassName);
            }
            getEmbedderActivity().startActivity(intent);
            return true;
        }

        private Activity getEmbedderActivity() {
            return mFragmentImpl.getActivity();
        }
    }

    private static class SiteSettingsFragmentHostCallback extends FragmentHostCallback<Context> {
        private final SiteSettingsFragmentImpl mFragmentImpl;

        private SiteSettingsFragmentHostCallback(SiteSettingsFragmentImpl fragmentImpl) {
            super(new PassthroughFragmentActivity(fragmentImpl), new Handler(), 0);
            mFragmentImpl = fragmentImpl;
        }

        @Override
        public Context onGetHost() {
            return mFragmentImpl.getWebLayerContext();
        }

        @Override
        public LayoutInflater onGetLayoutInflater() {
            return (LayoutInflater) mFragmentImpl.getWebLayerContext().getSystemService(
                    Context.LAYOUT_INFLATER_SERVICE);
        }

        @Override
        public boolean onHasView() {
            return mFragmentImpl.getView() != null;
        }

        @Override
        public View onFindViewById(int id) {
            return onHasView() ? mFragmentImpl.getView().findViewById(id) : null;
        }
    }

    public SiteSettingsFragmentImpl(ProfileManager profileManager,
            IRemoteFragmentClient remoteFragmentClient, Bundle intentExtras) {
        super(remoteFragmentClient);
        mProfile = profileManager.getProfile(
                intentExtras.getString(SiteSettingsFragmentArgs.PROFILE_NAME));
        // Convert the WebLayer ABI's Site Settings arguments into the format the Site Settings
        // implementation fragments expect.
        Bundle fragmentArgs = intentExtras.getBundle(SiteSettingsFragmentArgs.FRAGMENT_ARGUMENTS);
        switch (intentExtras.getString(SiteSettingsFragmentArgs.FRAGMENT_NAME)) {
            case SiteSettingsFragmentArgs.CATEGORY_LIST:
                mFragmentClass = SiteSettings.class;
                mFragmentArguments = null;
                break;
            case SiteSettingsFragmentArgs.SINGLE_CATEGORY:
                mFragmentClass = SingleCategorySettings.class;
                mFragmentArguments = new Bundle();
                mFragmentArguments.putString(SingleCategorySettings.EXTRA_TITLE,
                        fragmentArgs.getString(SiteSettingsFragmentArgs.SINGLE_CATEGORY_TITLE));
                mFragmentArguments.putString(SingleCategorySettings.EXTRA_CATEGORY,
                        fragmentArgs.getString(SiteSettingsFragmentArgs.SINGLE_CATEGORY_TYPE));
                break;
            case SiteSettingsFragmentArgs.SINGLE_WEBSITE:
                mFragmentClass = SingleWebsiteSettings.class;
                mFragmentArguments = SingleWebsiteSettings.createFragmentArgsForSite(
                        fragmentArgs.getString(SiteSettingsFragmentArgs.SINGLE_WEBSITE_URL));
                break;
            default:
                throw new IllegalArgumentException("Unknown Site Settings Fragment");
        }
    }

    @Override
    public void onAttach(Context context) {
        StrictModeWorkaround.apply();
        super.onAttach(context);

        mEmbedderContext = context;
        mContext = new ContextThemeWrapper(
                ClassLoaderContextWrapperFactory.get(context), R.style.Theme_WebLayer_SiteSettings);
        mFragmentController =
                FragmentController.createController(new SiteSettingsFragmentHostCallback(this));
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        StrictModeWorkaround.apply();
        mFragmentController.attachHost(null);

        super.onCreate(savedInstanceState);

        mFragmentController.dispatchCreate();
    }

    @Override
    public View onCreateView(ViewGroup container, Bundle savedInstanceState) {
        LayoutInflater inflater = (LayoutInflater) getWebLayerContext().getSystemService(
                Context.LAYOUT_INFLATER_SERVICE);
        View root =
                inflater.inflate(R.layout.site_settings_layout, container, /*attachToRoot=*/false);
        if (mFragmentController.getSupportFragmentManager().findFragmentByTag(FRAGMENT_TAG)
                == null) {
            try {
                SiteSettingsPreferenceFragment siteSettingsFragment = mFragmentClass.newInstance();
                siteSettingsFragment.setArguments(mFragmentArguments);
                siteSettingsFragment.setSiteSettingsClient(
                        new WebLayerSiteSettingsClient(mProfile));
                mFragmentController.getSupportFragmentManager()
                        .beginTransaction()
                        .add(R.id.site_settings_container, siteSettingsFragment, FRAGMENT_TAG)
                        .commitNow();
            } catch (IllegalAccessException | InstantiationException e) {
                throw new RuntimeException("Failed to create Site Settings Fragment", e);
            }
        }
        return root;
    }

    @Override
    public void onDestroyView() {
        StrictModeWorkaround.apply();
        super.onDestroyView();
        mFragmentController.dispatchDestroyView();
    }

    @Override
    public void onDestroy() {
        StrictModeWorkaround.apply();
        super.onDestroy();
        mFragmentController.dispatchDestroy();
    }

    @Override
    public void onDetach() {
        StrictModeWorkaround.apply();
        super.onDetach();
        mContext = null;
    }

    @Override
    public void onStart() {
        super.onStart();
        mFragmentController.dispatchActivityCreated();
        mFragmentController.noteStateNotSaved();
        mFragmentController.execPendingActions();
        mFragmentController.dispatchStart();
    }

    @Override
    public void onStop() {
        super.onStop();
        mFragmentController.dispatchStop();
    }

    @Override
    public void onResume() {
        super.onResume();
        mFragmentController.dispatchResume();
    }

    @Override
    public void onPause() {
        super.onPause();
        mFragmentController.dispatchPause();
    }

    public ISiteSettingsFragment asISiteSettingsFragment() {
        return new ISiteSettingsFragment.Stub() {
            @Override
            public IRemoteFragment asRemoteFragment() {
                StrictModeWorkaround.apply();
                return SiteSettingsFragmentImpl.this;
            }
        };
    }

    private Context getWebLayerContext() {
        return mContext;
    }

    private Context getEmbedderContext() {
        return mEmbedderContext;
    }

    private ProfileImpl getProfile() {
        return mProfile;
    }
}
