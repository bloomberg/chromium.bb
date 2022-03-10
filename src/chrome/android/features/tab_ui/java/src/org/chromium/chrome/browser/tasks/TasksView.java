// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.RippleDrawable;
import android.os.Build;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.coordinatorlayout.widget.CoordinatorLayout;
import androidx.core.view.ViewCompat;

import com.google.android.material.appbar.AppBarLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.feed.FeedStreamViewResizer;
import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.ntp.IncognitoDescriptionView;
import org.chromium.chrome.browser.ntp.search.SearchBoxCoordinator;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.CoordinatorLayoutForPointer;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.ui.base.WindowAndroid;

// The view of the tasks surface.
class TasksView extends CoordinatorLayoutForPointer {
    private static final int OMNIBOX_BOTTOM_PADDING_DP = 4;

    private final Context mContext;
    private FrameLayout mCarouselTabSwitcherContainer;
    private AppBarLayout mHeaderView;
    private SearchBoxCoordinator mSearchBoxCoordinator;
    private IncognitoDescriptionView mIncognitoDescriptionView;
    private View.OnClickListener mIncognitoDescriptionLearnMoreListener;
    private boolean mIncognitoCookieControlsToggleIsChecked;
    private OnCheckedChangeListener mIncognitoCookieControlsToggleCheckedListener;
    private @CookieControlsEnforcement int mIncognitoCookieControlsToggleEnforcement =
            CookieControlsEnforcement.NO_ENFORCEMENT;
    private View.OnClickListener mIncognitoCookieControlsIconClickListener;
    private UiConfig mUiConfig;
    private boolean mIsIncognito;

    /** Default constructor needed to inflate via XML. */
    public TasksView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    public void initialize(ActivityLifecycleDispatcher activityLifecycleDispatcher,
            boolean isIncognito, WindowAndroid windowAndroid) {
        assert mSearchBoxCoordinator
                != null : "#onFinishInflate should be completed before the call to initialize.";

        mSearchBoxCoordinator.initialize(activityLifecycleDispatcher, isIncognito, windowAndroid);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mCarouselTabSwitcherContainer =
                (FrameLayout) findViewById(R.id.carousel_tab_switcher_container);
        mSearchBoxCoordinator = new SearchBoxCoordinator(getContext(), this);

        mHeaderView = (AppBarLayout) findViewById(R.id.task_surface_header);

        forceHeaderScrollable();

        mUiConfig = new UiConfig(this);
        setHeaderPadding();
        setTabCarouselTitleStyle();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        mUiConfig.updateDisplayStyle();
    }

    private void setTabCarouselTitleStyle() {
        // Match the tab carousel title style with the feed header.
        // There are many places checking FeedFeatures.isReportingUserActions, like in
        // ExploreSurfaceCoordinator.
        TextView titleDescription = (TextView) findViewById(R.id.tab_switcher_title_description);
        TextView moreTabs = (TextView) findViewById(R.id.more_tabs);
        ApiCompatibilityUtils.setTextAppearance(
                titleDescription, R.style.TextAppearance_TextAccentMediumThick_Secondary);
        ApiCompatibilityUtils.setTextAppearance(moreTabs, R.style.TextAppearance_Button_Text_Blue);
        ViewCompat.setPaddingRelative(titleDescription, titleDescription.getPaddingStart(),
                titleDescription.getPaddingTop(), titleDescription.getPaddingEnd(),
                titleDescription.getPaddingBottom());
    }

    ViewGroup getCarouselTabSwitcherContainer() {
        return mCarouselTabSwitcherContainer;
    }

    ViewGroup getBodyViewContainer() {
        return findViewById(R.id.tasks_surface_body);
    }

    /**
     * Set the visibility of the tasks surface body.
     * @param isVisible Whether it's visible.
     */
    void setSurfaceBodyVisibility(boolean isVisible) {
        getBodyViewContainer().setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /**
     * Set the visibility of the tab carousel.
     * @param isVisible Whether it's visible.
     */
    void setTabCarouselVisibility(boolean isVisible) {
        mCarouselTabSwitcherContainer.setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /**
     * Set the visibility of the tab carousel title.
     * @param isVisible Whether it's visible.
     */
    void setTabCarouselTitleVisibility(boolean isVisible) {
        findViewById(R.id.tab_switcher_title).setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /**
     * @return The {@link SearchBoxCoordinator} representing the fake search box.
     */
    SearchBoxCoordinator getSearchBoxCoordinator() {
        return mSearchBoxCoordinator;
    }

    /**
     * Set the visibility of the Most Visited Tiles.
     */
    void setMostVisitedVisibility(int visibility) {
        findViewById(R.id.mv_tiles_container).setVisibility(visibility);
    }

    /**
     * Set the visibility of the Most Visited Tiles.
     */
    void setQueryTilesVisibility(int visibility) {
        findViewById(R.id.query_tiles_container).setVisibility(visibility);
    }

    /**
     * Set the {@link android.view.View.OnClickListener} for More Tabs.
     */
    void setMoreTabsOnClickListener(@Nullable View.OnClickListener listener) {
        findViewById(R.id.more_tabs).setOnClickListener(listener);
    }

    /**
     * Set the incognito state.
     * @param isIncognito Whether it's in incognito mode.
     */
    void setIncognitoMode(boolean isIncognito) {
        int backgroundColor = ChromeColors.getPrimaryBackgroundColor(mContext, isIncognito);
        setBackgroundColor(backgroundColor);
        mHeaderView.setBackgroundColor(backgroundColor);

        mSearchBoxCoordinator.setIncognitoMode(isIncognito);
        Drawable searchBackground = AppCompatResources.getDrawable(mContext,
                isIncognito ? R.drawable.fake_search_box_bg_incognito : R.drawable.ntp_search_box);
        if (searchBackground instanceof RippleDrawable) {
            Drawable shapeDrawable = ((RippleDrawable) searchBackground)
                                             .findDrawableByLayerId(R.id.fake_search_box_bg_shape);
            if (shapeDrawable != null) {
                @ColorInt
                int searchBackgroundColor = isIncognito
                        ? getResources().getColor(R.color.toolbar_text_box_background_incognito)
                        : ChromeColors.getSurfaceColor(
                                mContext, R.dimen.toolbar_text_box_elevation);
                shapeDrawable.mutate();
                // TODO(https://crbug.com/1239289): Change back to #setTint once our min API level
                // is 23.
                shapeDrawable.setColorFilter(searchBackgroundColor, PorterDuff.Mode.SRC_IN);
            }
        }
        mSearchBoxCoordinator.setBackground(searchBackground);
        int hintTextColor = mContext.getColor(isIncognito ? R.color.locationbar_light_hint_text
                                                          : R.color.locationbar_dark_hint_text);
        mSearchBoxCoordinator.setSearchBoxHintColor(hintTextColor);
        mIsIncognito = isIncognito;
    }

    /**
     * Initialize incognito description view.
     * Note that this interface is supposed to be called only once.
     */
    void initializeIncognitoDescriptionView() {
        assert mIncognitoDescriptionView == null;
        ViewStub containerStub =
                (ViewStub) findViewById(R.id.incognito_description_container_layout_stub);
        View containerView = containerStub.inflate();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            containerView.setFocusable(true);
            containerView.setFocusableInTouchMode(true);
        }

        ViewStub incognitoDescriptionViewStub =
                (ViewStub) findViewById(R.id.task_view_incognito_layout_stub);
        if (FeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.INCOGNITO_NTP_REVAMP)) {
            incognitoDescriptionViewStub.setLayoutResource(
                    R.layout.revamped_incognito_description_layout);
        } else {
            incognitoDescriptionViewStub.setLayoutResource(R.layout.incognito_description_layout);
        }

        mIncognitoDescriptionView =
                (IncognitoDescriptionView) incognitoDescriptionViewStub.inflate();
        if (mIncognitoDescriptionLearnMoreListener != null) {
            setIncognitoDescriptionLearnMoreClickListener(mIncognitoDescriptionLearnMoreListener);
        }
        setIncognitoCookieControlsToggleChecked(mIncognitoCookieControlsToggleIsChecked);
        if (mIncognitoCookieControlsToggleCheckedListener != null) {
            setIncognitoCookieControlsToggleCheckedListener(
                    mIncognitoCookieControlsToggleCheckedListener);
        }
        setIncognitoCookieControlsToggleEnforcement(mIncognitoCookieControlsToggleEnforcement);
        if (mIncognitoCookieControlsIconClickListener != null) {
            setIncognitoCookieControlsIconClickListener(mIncognitoCookieControlsIconClickListener);
        }
    }

    /**
     * Set the visibility of the incognito description.
     * @param isVisible Whether it's visible or not.
     */
    void setIncognitoDescriptionVisibility(boolean isVisible) {
        ((View) mIncognitoDescriptionView).setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /**
     * Set the incognito description learn more click listener.
     * @param listener The given click listener.
     */
    void setIncognitoDescriptionLearnMoreClickListener(View.OnClickListener listener) {
        mIncognitoDescriptionLearnMoreListener = listener;
        if (mIncognitoDescriptionView != null) {
            mIncognitoDescriptionView.setLearnMoreOnclickListener(listener);
            mIncognitoDescriptionLearnMoreListener = null;
        }
    }

    /**
     * Set the toggle on the cookie controls card.
     * @param isChecked Whether it's checked or not.
     */
    void setIncognitoCookieControlsToggleChecked(boolean isChecked) {
        mIncognitoCookieControlsToggleIsChecked = isChecked;
        if (mIncognitoDescriptionView != null) {
            mIncognitoDescriptionView.setCookieControlsToggle(isChecked);
        }
    }

    /**
     * Set the incognito cookie controls toggle checked change listener.
     * @param listener The given checked change listener.
     */
    void setIncognitoCookieControlsToggleCheckedListener(OnCheckedChangeListener listener) {
        mIncognitoCookieControlsToggleCheckedListener = listener;
        if (mIncognitoDescriptionView != null) {
            mIncognitoDescriptionView.setCookieControlsToggleOnCheckedChangeListener(listener);
            mIncognitoCookieControlsToggleCheckedListener = null;
        }
    }

    /**
     * Set the enforcement rule for the incognito cookie controls toggle.
     * @param enforcement The enforcement enum to set.
     */
    void setIncognitoCookieControlsToggleEnforcement(@CookieControlsEnforcement int enforcement) {
        mIncognitoCookieControlsToggleEnforcement = enforcement;
        if (mIncognitoDescriptionView != null) {
            mIncognitoDescriptionView.setCookieControlsEnforcement(enforcement);
        }
    }

    /**
     * Set the incognito cookie controls icon click listener.
     * @param listener The given onclick listener.
     */
    void setIncognitoCookieControlsIconClickListener(OnClickListener listener) {
        mIncognitoCookieControlsIconClickListener = listener;
        if (mIncognitoDescriptionView != null) {
            mIncognitoDescriptionView.setCookieControlsIconOnclickListener(listener);
            mIncognitoCookieControlsIconClickListener = null;
        }
    }

    /**
     * Set the top margin for the tasks surface body.
     * @param topMargin The top margin to set.
     */
    void setTasksSurfaceBodyTopMargin(int topMargin) {
        MarginLayoutParams params = (MarginLayoutParams) getBodyViewContainer().getLayoutParams();
        params.topMargin = topMargin;
    }

    /**
     * Set the top margin for the mv tiles container.
     * @param topMargin The top margin to set.
     */
    void setMVTilesContainerTopMargin(int topMargin) {
        MarginLayoutParams params =
                (MarginLayoutParams) mHeaderView.findViewById(R.id.mv_tiles_container)
                        .getLayoutParams();
        params.topMargin = topMargin;
    }

    /**
     * Set the top margin for the tab switcher title.
     * @param topMargin The top margin to set.
     */
    void setTabSwitcherTitleTopMargin(int topMargin) {
        MarginLayoutParams params =
                (MarginLayoutParams) mHeaderView.findViewById(R.id.tab_switcher_title)
                        .getLayoutParams();
        params.topMargin = topMargin;
    }

    /**
     * Set the height of the top toolbar placeholder layout.
     */
    void setTopToolbarPlaceholderHeight(int height) {
        View topToolbarPlaceholder = findViewById(R.id.top_toolbar_placeholder);
        ViewGroup.LayoutParams lp = topToolbarPlaceholder.getLayoutParams();
        lp.height = height;
        topToolbarPlaceholder.setLayoutParams(lp);
    }

    /**
     * Reset the scrolling position by expanding the {@link #mHeaderView}.
     */
    void resetScrollPosition() {
        if (mHeaderView != null && mHeaderView.getHeight() != mHeaderView.getBottom()) {
            mHeaderView.setExpanded(true);
        }
    }
    /**
     * Add a header offset change listener.
     * @param onOffsetChangedListener The given header offset change listener.
     */
    void addHeaderOffsetChangeListener(
            AppBarLayout.OnOffsetChangedListener onOffsetChangedListener) {
        if (mHeaderView != null) {
            mHeaderView.addOnOffsetChangedListener(onOffsetChangedListener);
        }
    }

    /**
     * Remove the given header offset change listener.
     * @param onOffsetChangedListener The header offset change listener which should be removed.
     */
    void removeHeaderOffsetChangeListener(
            AppBarLayout.OnOffsetChangedListener onOffsetChangedListener) {
        if (mHeaderView != null) {
            mHeaderView.removeOnOffsetChangedListener(onOffsetChangedListener);
        }
    }

    /**
     * Update the fake search box layout.
     * @param height Current height of the fake search box layout.
     * @param topMargin Current top margin of the fake search box layout.
     * @param endPadding Current end padding of the fake search box layout.
     * @param textSize Current text size of text view in fake search box layout.
     * @param translationX Current translationX of text view in fake search box layout.
     * @param buttonSize Current height and width of the buttons in fake search box layout.
     * @param lensButtonLeftMargin Current left margin of the lens button in fake search box layout.
     */
    void updateFakeSearchBox(int height, int topMargin, int endPadding, float textSize,
            float translationX, int buttonSize, int lensButtonLeftMargin) {
        if (mSearchBoxCoordinator.getView().getVisibility() != View.VISIBLE) return;
        mSearchBoxCoordinator.setHeight(height);
        mSearchBoxCoordinator.setTopMargin(topMargin);
        mSearchBoxCoordinator.setEndPadding(endPadding);
        mSearchBoxCoordinator.setTextSize(textSize);
        mSearchBoxCoordinator.setTextViewTranslationX(translationX);
        mSearchBoxCoordinator.setButtonsHeight(buttonSize);
        mSearchBoxCoordinator.setButtonsWidth(buttonSize);
        mSearchBoxCoordinator.setLensButtonLeftMargin(lensButtonLeftMargin);
    }

    private void forceHeaderScrollable() {
        // TODO(https://crbug.com/1251632): Find out why scrolling was broken after
        // crrev.com/c/3025127. Force the header view to be draggable as a workaround.
        CoordinatorLayout.LayoutParams params =
                (CoordinatorLayout.LayoutParams) mHeaderView.getLayoutParams();
        AppBarLayout.Behavior behavior = new AppBarLayout.Behavior();
        behavior.setDragCallback(new AppBarLayout.Behavior.DragCallback() {
            @Override
            public boolean canDrag(AppBarLayout appBarLayout) {
                return true;
            }
        });
        params.setBehavior(behavior);
    }

    /**
     * Make the padding of header consistent with that of Feed recyclerview which is sized by {@link
     * FeedStreamViewResizer} in {@link FeedSurfaceCoordinator}
     */
    private void setHeaderPadding() {
        int defaultPadding = getResources().getDimensionPixelSize(
                org.chromium.chrome.R.dimen.content_suggestions_card_modern_margin);
        int widePadding =
                getResources().getDimensionPixelSize(R.dimen.ntp_wide_card_lateral_margins);

        FeedStreamViewResizer.createAndAttach(
                (Activity) mContext, mHeaderView, mUiConfig, defaultPadding, widePadding);
    }
}
