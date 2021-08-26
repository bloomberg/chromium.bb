// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewStub;
import android.widget.ImageButton;
import android.widget.RelativeLayout;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.HomeButton;
import org.chromium.chrome.browser.toolbar.IncognitoToggleTabLayout;
import org.chromium.chrome.browser.toolbar.NewTabButton;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.components.browser_ui.widget.animation.Interpolators;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;

import java.util.ArrayList;
import java.util.List;

/** View of the StartSurfaceToolbar */
class StartSurfaceToolbarView extends RelativeLayout {
    private final long mAnimationDuration;
    private NewTabButton mNewTabButton;
    private HomeButton mHomeButton;
    private View mLogo;
    private View mTabSwitcherButtonView;
    private float mTabSwitcherButtonX;
    // Record this for the animation of moving incognito toggle layout.
    private boolean mShowTabSwitcherButton;
    @Nullable
    private IncognitoToggleTabLayout mIncognitoToggleTabLayout;
    private float mIncognitoToggleX;
    @Nullable
    private ImageButton mIdentityDiscButton;
    private ColorStateList mLightIconTint;
    private ColorStateList mDarkIconTint;

    private Rect mLogoRect = new Rect();
    private Rect mViewRect = new Rect();

    private boolean mShouldShow;
    private boolean mInStartSurfaceMode;
    private boolean mShowAnimation;
    private AnimatorSet mLayoutChangeAnimator;
    private List<Animator> mAnimators = new ArrayList();

    private ObservableSupplier<Boolean> mHomepageEnabledSupplier;
    private ObservableSupplier<Boolean> mHomepageManagedByPolicySupplier;
    private boolean mIsHomeButtonInitialized;
    private boolean mIsShowing;

    public StartSurfaceToolbarView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mAnimationDuration = TopToolbarCoordinator.TAB_SWITCHER_MODE_NORMAL_ANIMATION_DURATION_MS;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mNewTabButton = findViewById(R.id.new_tab_button);
        mHomeButton = findViewById(R.id.home_button_on_tab_switcher);
        ViewStub incognitoToggleTabsStub = findViewById(R.id.incognito_tabs_stub);
        mIncognitoToggleTabLayout = (IncognitoToggleTabLayout) incognitoToggleTabsStub.inflate();
        mLogo = findViewById(R.id.logo);
        mIdentityDiscButton = findViewById(R.id.identity_disc_button);
        mTabSwitcherButtonView = findViewById(R.id.start_tab_switcher_button);
        updatePrimaryColorAndTint(false);
        mNewTabButton.setStartSurfaceEnabled(true);
        setIncognitoToggleTabSwitcherButtonXs();
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        // TODO(https://crbug.com/1040526)

        super.onLayout(changed, l, t, r, b);

        if (mLogo.getVisibility() == View.GONE) return;

        mLogoRect.set(mLogo.getLeft(), mLogo.getTop(), mLogo.getRight(), mLogo.getBottom());
        for (int viewIndex = 0; viewIndex < getChildCount(); viewIndex++) {
            View view = getChildAt(viewIndex);

            // If the view is incognito toggle, it is fading out and will finally disappear.
            if (view == mLogo || view.getVisibility() == View.GONE
                    || view == mIncognitoToggleTabLayout) {
                continue;
            }

            assert view.getVisibility() == View.VISIBLE;
            mViewRect.set(view.getLeft(), view.getTop(), view.getRight(), view.getBottom());
            if (Rect.intersects(mLogoRect, mViewRect)) {
                mLogo.setVisibility(View.GONE);
                break;
            }
        }
    }

    void setGridTabSwitcherEnabled(boolean isGridTabSwitcherEnabled) {
        mNewTabButton.setGridTabSwitcherEnabled(isGridTabSwitcherEnabled);
    }

    /**
     * Sets the {@link OnClickListener} that will be notified when the New Tab button is pressed.
     * @param listener The callback that will be notified when the New Tab button is pressed.
     */
    void setOnNewTabClickHandler(View.OnClickListener listener) {
        mNewTabButton.setOnClickListener(listener);
    }

    /**
     * Sets the Logo visibility. Logo will not show if screen is not wide enough.
     * @param isVisible Whether the Logo should be visible.
     */
    void setLogoVisibility(boolean isVisible) {
        if (mLogo.getVisibility() == getVisibility(isVisible) || !mShowAnimation) {
            finishFadeAnimation(mLogo, isVisible);
            return;
        }

        addFadeAnimator(mLogo, isVisible);
    }

    /**
     * @param isVisible Whether the menu button is visible.
     */
    void setMenuButtonVisibility(boolean isVisible) {
        final int buttonPaddingLeft = getContext().getResources().getDimensionPixelOffset(
                R.dimen.start_surface_toolbar_button_padding_to_button);
        final int buttonPaddingRight =
                (isVisible ? buttonPaddingLeft
                           : getContext().getResources().getDimensionPixelOffset(
                                   R.dimen.start_surface_toolbar_button_padding_to_edge));
        mIdentityDiscButton.setPadding(buttonPaddingLeft, 0, buttonPaddingRight, 0);
        mNewTabButton.setPadding(buttonPaddingLeft, 0, buttonPaddingRight, 0);
    }

    /**
     * @param isVisible Whether the new tab button is visible.
     */
    void setNewTabButtonVisibility(boolean isVisible) {
        if (isVisible && Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            // This is a workaround for the issue that the UrlBar is given the default focus on
            // Android versions before Pie when showing the start surface toolbar with the new tab
            // button (UrlBar is invisible to users). Check crbug.com/1081538 for more details.
            mNewTabButton.getParent().requestChildFocus(mNewTabButton, mNewTabButton);
        }

        if (mNewTabButton.getVisibility() == getVisibility(isVisible) || !mShowAnimation) {
            finishScaleAnimations(mNewTabButton, isVisible);
            return;
        }

        addScaleAnimators(mNewTabButton, isVisible);
    }

    /**
     * @param isVisible Whether the home button is visible.
     */
    void setHomeButtonVisibility(boolean isVisible) {
        mayInitializeHomeButton();

        if (mHomeButton.getVisibility() == getVisibility(isVisible) || !mShowAnimation) {
            finishScaleAnimations(mHomeButton, isVisible);
            return;
        }

        addScaleAnimators(mHomeButton, isVisible);
    }

    /**
     * @param homepageEnabledSupplier Supplier of whether homepage is enabled.
     */
    void setHomepageEnabledSupplier(ObservableSupplier<Boolean> homepageEnabledSupplier) {
        assert mHomepageEnabledSupplier == null;
        mHomepageEnabledSupplier = homepageEnabledSupplier;
    }

    /**
     * @param homepageManagedByPolicySupplier Supplier of whether the homepage is managed by policy.
     */
    void setHomepageManagedByPolicySupplier(
            ObservableSupplier<Boolean> homepageManagedByPolicySupplier) {
        assert mHomepageManagedByPolicySupplier == null;
        mHomepageManagedByPolicySupplier = homepageManagedByPolicySupplier;
    }

    /**
     * Initializes the home button if not yet.
     */
    private void mayInitializeHomeButton() {
        if (mIsHomeButtonInitialized || mHomepageEnabledSupplier == null
                || mHomepageManagedByPolicySupplier == null) {
            return;
        }

        // The long click which shows the change homepage settings is disabled when the Start
        // surface is enabled.
        mHomeButton.init(mHomepageEnabledSupplier, null, mHomepageManagedByPolicySupplier);
        mIsHomeButtonInitialized = true;
    }

    /**
     * @param homeButtonClickHandler The callback that will be notified when the home button is
     *                               pressed.
     */
    void setHomeButtonClickHandler(OnClickListener homeButtonClickHandler) {
        mHomeButton.setOnClickListener(homeButtonClickHandler);
    }

    /**
     * @param isClickable Whether the buttons are clickable.
     */
    void setButtonClickableState(boolean isClickable) {
        mNewTabButton.setClickable(isClickable);
        mIncognitoToggleTabLayout.setClickable(isClickable);
    }

    /**
     * @param isVisible Whether the Incognito toggle tab layout is visible.
     */
    void setIncognitoToggleTabVisibility(boolean isVisible) {
        if (mIncognitoToggleTabLayout.getVisibility() == getVisibility(isVisible)
                || !mShowAnimation) {
            mTabSwitcherButtonView.setVisibility(mShowTabSwitcherButton ? View.VISIBLE : View.GONE);
            mIncognitoToggleTabLayout.setVisibility(isVisible ? View.VISIBLE : View.GONE);
            return;
        }

        addMoveIncognitoToggleAnimator(isVisible);
    }

    /**
     * @param highlight If the new tab button should be highlighted.
     */
    void setNewTabButtonHighlight(boolean highlight) {
        if (mNewTabButton == null) return;
        if (highlight) {
            ViewHighlighter.turnOnHighlight(
                    mNewTabButton, new HighlightParams(HighlightShape.CIRCLE));
        } else {
            ViewHighlighter.turnOffHighlight(mNewTabButton);
        }
    }

    /** Called when incognito mode changes. */
    void updateIncognito(boolean isIncognito) {
        updatePrimaryColorAndTint(isIncognito);
    }

    /**
     * @param provider The {@link IncognitoStateProvider} passed to buttons that need access to it.
     */
    void setIncognitoStateProvider(IncognitoStateProvider provider) {
        mNewTabButton.setIncognitoStateProvider(provider);
    }

    /** Called when accessibility status changes. */
    void onAccessibilityStatusChanged(boolean enabled) {
        mNewTabButton.onAccessibilityStatusChanged();
    }

    /** @return The View for the identity disc. */
    View getIdentityDiscView() {
        return mIdentityDiscButton;
    }

    /**
     * @param isAtStart Whether the identity disc is at start.
     */
    void setIdentityDiscAtStart(boolean isAtStart) {
        ((LayoutParams) mIdentityDiscButton.getLayoutParams()).removeRule(RelativeLayout.START_OF);
    }

    /**
     * @param isVisible Whether the identity disc is visible.
     */
    void setIdentityDiscVisibility(boolean isVisible) {
        if (mIdentityDiscButton.getVisibility() == getVisibility(isVisible) || !mShowAnimation) {
            finishScaleAnimations(mIdentityDiscButton, isVisible);
            return;
        }

        addScaleAnimators(mIdentityDiscButton, isVisible);
    }

    /**
     * Sets the {@link OnClickListener} that will be notified when the identity disc button is
     * pressed.
     * @param listener The callback that will be notified when the identity disc  is pressed.
     */
    void setIdentityDiscClickHandler(View.OnClickListener listener) {
        mIdentityDiscButton.setOnClickListener(listener);
    }

    /**
     * Updates the image displayed on the identity disc button.
     * @param image The new image for the button.
     */
    void setIdentityDiscImage(Drawable image) {
        mIdentityDiscButton.setImageDrawable(image);
    }

    /**
     * Updates idnetity disc content description.
     * @param contentDescriptionResId The new description for the button.
     */
    void setIdentityDiscContentDescription(@StringRes int contentDescriptionResId) {
        mIdentityDiscButton.setContentDescription(
                getContext().getResources().getString(contentDescriptionResId));
    }

    /**
     * Show or hide toolbar from tab.
     * @param inStartSurfaceMode Whether or not toolbar should be shown or hidden.
     * */
    void setStartSurfaceMode(boolean inStartSurfaceMode) {
        mInStartSurfaceMode = inStartSurfaceMode;
        updateToolbarVisibility();
    }

    /**
     * Show or hide toolbar.
     * @param shouldShowStartSurfaceToolbar Whether or not toolbar should be shown or hidden.
     * */
    void setToolbarVisibility(boolean shouldShowStartSurfaceToolbar) {
        mShouldShow = shouldShowStartSurfaceToolbar;
        updateToolbarVisibility();
    }

    /**
     * @param isVisible Whether the tab switcher button is visible.
     */
    void setTabSwitcherButtonVisibility(boolean isVisible) {
        if (mTabSwitcherButtonView.getVisibility() == getVisibility(isVisible)) {
            return;
        }

        if (!mShowAnimation) {
            mTabSwitcherButtonView.setVisibility(getVisibility(isVisible));
        }

        // If the animation should be shown, the visibility of the tab switcher button will be
        // updated in the {@link finishMoveIncognitoToggleAnimation} called when the moving
        // incognito toggle animation ends.
        mShowTabSwitcherButton = isVisible;
    }

    /**
     * Set TabCountProvider for incognito toggle view.
     * @param tabCountProvider The {@link TabCountProvider} to update the incognito toggle view.
     */
    void setTabCountProvider(TabCountProvider tabCountProvider) {
        if (mIncognitoToggleTabLayout != null) {
            mIncognitoToggleTabLayout.setTabCountProvider(tabCountProvider);
        }
    }

    /**
     * Set TabModelSelector for incognito toggle view.
     * @param selector  A {@link TabModelSelector} to provide information about open tabs.
     */
    void setTabModelSelector(TabModelSelector selector) {
        if (mIncognitoToggleTabLayout != null) {
            mIncognitoToggleTabLayout.setTabModelSelector(selector);
        }
    }

    void setShowAnimation(boolean showAnimation) {
        mShowAnimation = showAnimation;
    }

    /**
     * Start animation to show or hide toolbar.
     */
    private void updateToolbarVisibility() {
        boolean shouldShowStartSurfaceToolbar = mInStartSurfaceMode && mShouldShow;

        if (!mShowAnimation) {
            if (shouldShowStartSurfaceToolbar == mIsShowing) return;
            mIsShowing = shouldShowStartSurfaceToolbar;

            if (mLayoutChangeAnimator != null) {
                mLayoutChangeAnimator.cancel();
                finishFadeAnimation(this, shouldShowStartSurfaceToolbar);
                return;
            }

            addFadeAnimator(this, shouldShowStartSurfaceToolbar);
        }

        mLayoutChangeAnimator = new AnimatorSet();
        mLayoutChangeAnimator.playTogether(mAnimators);
        mLayoutChangeAnimator.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onCancel(Animator animator) {
                mAnimators.clear();
                mLayoutChangeAnimator = null;
            }
            @Override
            public void onEnd(Animator animator) {
                mAnimators.clear();
                mLayoutChangeAnimator = null;
            }
        });
        mLayoutChangeAnimator.start();
    }

    private void updatePrimaryColorAndTint(boolean isIncognito) {
        int primaryColor = ChromeColors.getPrimaryBackgroundColor(getContext(), isIncognito);
        setBackgroundColor(primaryColor);

        if (mLightIconTint == null) {
            mLightIconTint = AppCompatResources.getColorStateList(
                    getContext(), R.color.default_icon_color_light_tint_list);
            mDarkIconTint = AppCompatResources.getColorStateList(
                    getContext(), R.color.default_icon_color_tint_list);
        }
    }

    private void addScaleAnimators(View targetView, boolean showTargetView) {
        // Set shrinking and expanding animations for the targetView and they will be started in
        // {@link updateToolbarVisibility}.
        targetView.setVisibility(View.VISIBLE);
        Animator scaleAnimator = ObjectAnimator
                                         .ofFloat(targetView, SCALE_X, showTargetView ? 0.0f : 1.0f,
                                                 showTargetView ? 1.0f : 0.0f)
                                         .setDuration(mAnimationDuration);
        scaleAnimator.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);
        mAnimators.add(scaleAnimator);

        scaleAnimator = ObjectAnimator
                                .ofFloat(targetView, SCALE_Y, showTargetView ? 0.0f : 1.0f,
                                        showTargetView ? 1.0f : 0.0f)
                                .setDuration(mAnimationDuration);
        scaleAnimator.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onCancel(Animator animator) {
                finishScaleAnimations(targetView, showTargetView);
            }
            @Override
            public void onEnd(Animator animator) {
                finishScaleAnimations(targetView, showTargetView);
            }
        });
        mAnimators.add(scaleAnimator);
    }

    private void finishScaleAnimations(View targetView, boolean showTargetView) {
        targetView.setVisibility(showTargetView ? View.VISIBLE : View.GONE);
        targetView.setScaleX(1.0f);
        targetView.setScaleY(1.0f);
    }

    private void addFadeAnimator(View targetView, boolean showTargetView) {
        // Show the fade-in and fade-out animation. Set visibility as VISIBLE here to show the
        // animation. The visibility will be finally set in finishFadeAnimation().
        targetView.setVisibility(View.VISIBLE);
        targetView.setAlpha(showTargetView ? 0.0f : 1.0f);
        Animator opacityAnimator =
                ObjectAnimator.ofFloat(targetView, ALPHA, showTargetView ? 1.0f : 0.0f)
                        .setDuration(mAnimationDuration);
        opacityAnimator.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);
        opacityAnimator.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onCancel(Animator animator) {
                finishFadeAnimation(targetView, showTargetView);
            }
            @Override
            public void onEnd(Animator animator) {
                finishFadeAnimation(targetView, showTargetView);
            }
        });
        mAnimators.add(opacityAnimator);
    }

    private void finishFadeAnimation(View targetView, boolean showTargetView) {
        targetView.setVisibility(showTargetView ? View.VISIBLE : View.GONE);
        targetView.setAlpha(1.0f);
    }

    private void addMoveIncognitoToggleAnimator(boolean showIncognitoToggle) {
        mIncognitoToggleTabLayout.setVisibility(View.VISIBLE);

        mIncognitoToggleTabLayout.setX(
                showIncognitoToggle ? mTabSwitcherButtonX : mIncognitoToggleX);
        Animator xAnimator =
                ObjectAnimator
                        .ofFloat(mIncognitoToggleTabLayout, X,
                                showIncognitoToggle ? mIncognitoToggleX : mTabSwitcherButtonX)
                        .setDuration(mAnimationDuration);
        xAnimator.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);
        xAnimator.addListener(new CancelAwareAnimatorListener() {
            @Override
            public void onCancel(Animator animator) {
                finishMoveIncognitoToggleAnimation(showIncognitoToggle);
            }
            @Override
            public void onEnd(Animator animator) {
                finishMoveIncognitoToggleAnimation(showIncognitoToggle);
            }
        });
        mAnimators.add(xAnimator);
    }

    private void finishMoveIncognitoToggleAnimation(boolean showIncognitoToggle) {
        mTabSwitcherButtonView.setVisibility(mShowTabSwitcherButton ? View.VISIBLE : View.GONE);
        mIncognitoToggleTabLayout.setVisibility(showIncognitoToggle ? View.VISIBLE : View.GONE);
        mIncognitoToggleTabLayout.setX(mIncognitoToggleX);
    }

    private void setIncognitoToggleTabSwitcherButtonXs() {
        int screenWidthPx = dpToPx(getResources().getConfiguration().screenWidthDp);
        int buttonWidthPx = getResources().getDimensionPixelOffset(R.dimen.toolbar_button_width);
        mIncognitoToggleX = (float) screenWidthPx / 2 - buttonWidthPx;
        mTabSwitcherButtonX = screenWidthPx - buttonWidthPx * 2;
    }

    private int getVisibility(boolean isVisible) {
        return isVisible ? View.VISIBLE : View.GONE;
    }

    private int dpToPx(int dp) {
        return (int) TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, dp, getResources().getDisplayMetrics());
    }
}
