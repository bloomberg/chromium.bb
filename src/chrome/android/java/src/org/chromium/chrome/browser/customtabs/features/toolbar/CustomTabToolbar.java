// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.chromium.base.MathUtils.interpolate;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.style.ForegroundColorSpan;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.ActionMode;
import android.view.GestureDetector;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.view.MarginLayoutParamsCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator.SelectionState;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.page_info.ChromePageInfo;
import org.chromium.chrome.browser.page_info.ChromePageInfoHighlight;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TrustedCdn;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.LocationBarModel;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.page_info.PageInfoController.OpenedFromSource;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.interpolators.BakedBezierInterpolator;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.widget.Toast;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * The Toolbar layout to be used for a custom tab. This is used for both phone and tablet UIs.
 */
public class CustomTabToolbar extends ToolbarLayout implements View.OnLongClickListener {
    private static final Object ORIGIN_SPAN = new Object();

    /**
     * A simple {@link FrameLayout} that prevents its children from getting touch events. This is
     * especially useful to prevent {@link UrlBar} from running custom touch logic since it is
     * read-only in custom tabs.
     */
    public static class InterceptTouchLayout extends FrameLayout {
        private GestureDetector mGestureDetector;

        public InterceptTouchLayout(Context context, AttributeSet attrs) {
            super(context, attrs);
            mGestureDetector = new GestureDetector(
                    getContext(), new GestureDetector.SimpleOnGestureListener() {
                        @Override
                        public boolean onSingleTapConfirmed(MotionEvent e) {
                            if (LibraryLoader.getInstance().isInitialized()) {
                                RecordUserAction.record("CustomTabs.TapUrlBar");
                            }
                            return super.onSingleTapConfirmed(e);
                        }
                    }, ThreadUtils.getUiThreadHandler());
        }

        @Override
        public boolean onInterceptTouchEvent(MotionEvent ev) {
            return true;
        }

        @Override
        @SuppressLint("ClickableViewAccessibility")
        public boolean onTouchEvent(MotionEvent event) {
            mGestureDetector.onTouchEvent(event);
            return super.onTouchEvent(event);
        }
    }

    private ImageView mIncognitoImageView;
    private LinearLayout mCustomActionButtons;
    private ImageButton mCloseButton;
    // This View will be non-null only for bottom sheet custom tabs.
    private ImageView mHandleView;

    // Whether dark tint should be applied to icons and text.
    private boolean mUseDarkColors;

    private final ColorStateList mDarkModeTint;
    private final ColorStateList mLightModeTint;

    private ValueAnimator mBrandColorTransitionAnimation;
    private boolean mBrandColorTransitionActive;

    private GURL mFirstUrl;

    private final CustomTabLocationBar mLocationBar = new CustomTabLocationBar();
    private LocationBarModel mLocationBarModel;

    /**
     * Whether to use the toolbar as handle to resize the Window height.
     */
    public interface HandleStrategy {
        /**
         * Decide whether we need to intercept the touch events so the events will be passed to the
         * {@link #onTouchEvent()} method.
         *
         * @param event The touch event to be examined.
         * @return whether the event will be passed to {@link #onTouchEvent()}.
         */
        boolean onInterceptTouchEvent(MotionEvent event);

        /**
         * Handling the touch events.
         *
         * @param event The touch event to be handled.
         * @return whether the event is consumed..
         */
        boolean onTouchEvent(MotionEvent event);

        /**
         * Set a handler to close the current tab.
         *
         * @param handler The handler for closing the current tab.
         */
        void setCloseClickHandler(Runnable handler);
    }

    private HandleStrategy mHandleStrategy;

    /**
     * Constructor for getting this class inflated from an xml layout file.
     */
    public CustomTabToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);

        mDarkModeTint = ThemeUtils.getThemedToolbarIconTint(context, false);
        mLightModeTint = ThemeUtils.getThemedToolbarIconTint(context, true);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        final int backgroundColor = ChromeColors.getDefaultThemeColor(getContext(), false);
        setBackground(new ColorDrawable(backgroundColor));
        mUseDarkColors = !ColorUtils.shouldUseLightForegroundOnBackground(backgroundColor);

        mIncognitoImageView = findViewById(R.id.incognito_cct_logo_image_view);
        mCustomActionButtons = findViewById(R.id.action_buttons);
        mCloseButton = findViewById(R.id.close_button);
        mCloseButton.setOnLongClickListener(this);

        mLocationBar.onFinishInflate(this);
    }

    @Override
    public void onNativeLibraryReady() {
        super.onNativeLibraryReady();
        mLocationBar.onNativeLibraryReady();
    }

    @Override
    protected void setCloseButtonImageResource(Drawable drawable) {
        mCloseButton.setVisibility(drawable != null ? View.VISIBLE : View.GONE);
        mCloseButton.setImageDrawable(drawable);
        if (drawable != null) {
            updateButtonTint(mCloseButton);
        }
    }

    @Override
    protected void setCustomTabCloseClickHandler(OnClickListener listener) {
        mCloseButton.setOnClickListener(listener);
    }

    @Override
    protected void addCustomActionButton(
            Drawable drawable, String description, OnClickListener listener) {
        ImageButton button =
                (ImageButton) LayoutInflater.from(getContext())
                        .inflate(R.layout.custom_tabs_toolbar_button, mCustomActionButtons, false);
        button.setOnLongClickListener(this);
        button.setOnClickListener(listener);
        button.setVisibility(VISIBLE);

        updateCustomActionButtonVisuals(button, drawable, description);

        // Add the view at the beginning of the child list.
        mCustomActionButtons.addView(button, 0);
    }

    @Override
    protected void updateCustomActionButton(int index, Drawable drawable, String description) {
        ImageButton button = (ImageButton) mCustomActionButtons.getChildAt(
                mCustomActionButtons.getChildCount() - 1 - index);
        assert button != null;
        updateCustomActionButtonVisuals(button, drawable, description);
    }

    /**
     * Creates and returns a CustomTab-specific LocationBar. This also retains a reference to the
     * passed LocationBarModel.
     * @param locationBarModel {@link LocationBarModel} to be used for accessing LocationBar
     *         state.
     * @param actionModeCallback Callback to handle changes in contextual action Modes.
     * @return The LocationBar implementation for this CustomTabToolbar.
     */
    public LocationBar createLocationBar(LocationBarModel locationBarModel,
            ActionMode.Callback actionModeCallback,
            Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        mLocationBarModel = locationBarModel;
        mLocationBar.init(locationBarModel, modalDialogManagerSupplier, actionModeCallback);
        return mLocationBar;
    }

    private void updateCustomActionButtonVisuals(
            ImageButton button, Drawable drawable, String description) {
        Resources resources = getResources();

        // The height will be scaled to match spec while keeping the aspect ratio, so get the scaled
        // width through that.
        int sourceHeight = drawable.getIntrinsicHeight();
        int sourceScaledHeight = resources.getDimensionPixelSize(R.dimen.toolbar_icon_height);
        int sourceWidth = drawable.getIntrinsicWidth();
        int sourceScaledWidth = sourceWidth * sourceScaledHeight / sourceHeight;
        int minPadding = resources.getDimensionPixelSize(R.dimen.min_toolbar_icon_side_padding);

        int sidePadding = Math.max((2 * sourceScaledHeight - sourceScaledWidth) / 2, minPadding);
        int topPadding = button.getPaddingTop();
        int bottomPadding = button.getPaddingBottom();
        button.setPadding(sidePadding, topPadding, sidePadding, bottomPadding);
        button.setImageDrawable(drawable);
        updateButtonTint(button);

        button.setContentDescription(description);
    }

    /**
     * @return The custom action button with the given {@code index}. For test purpose only.
     * @param index The index of the custom action button to return.
     */
    @VisibleForTesting
    public ImageButton getCustomActionButtonForTest(int index) {
        return (ImageButton) mCustomActionButtons.getChildAt(index);
    }

    @Override
    protected int getTabStripHeight() {
        return 0;
    }

    /** @return The current active {@link Tab}. */
    @Nullable
    private Tab getCurrentTab() {
        return getToolbarDataProvider().getTab();
    }

    @Override
    public void setUrlBarHidden(boolean hideUrlBar) {
        mLocationBar.setUrlBarHidden(hideUrlBar);
    }

    @Override
    protected String getContentPublisher() {
        Tab tab = getToolbarDataProvider().getTab();
        if (tab == null) return null;

        String publisherUrl = TrustedCdn.getPublisherUrl(tab);
        if (publisherUrl != null) {
            return UrlUtilities.extractPublisherFromPublisherUrl(publisherUrl);
        }

        // TODO(bauerb): Remove this once trusted CDN publisher URLs have rolled out completely.
        if (mLocationBar.isShowingTitleOnly()) return parsePublisherNameFromUrl(tab.getUrl());

        return null;
    }

    @Override
    protected void onNavigatedToDifferentPage() {
        super.onNavigatedToDifferentPage();
        mLocationBarModel.notifyTitleChanged();
        if (mLocationBar.isShowingTitleOnly()) {
            if (mFirstUrl == null || mFirstUrl.isEmpty()) {
                mFirstUrl = getToolbarDataProvider().getTab().getUrl();
            } else {
                if (mFirstUrl.equals(getToolbarDataProvider().getTab().getUrl())) return;
                setUrlBarHidden(false);
            }
        }
        mLocationBarModel.notifySecurityStateChanged();
    }

    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouchEvent(MotionEvent event) {
        if (mHandleStrategy != null) {
            return mHandleStrategy.onTouchEvent(event);
        }
        return false;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        if (mHandleStrategy != null) {
            return mHandleStrategy.onInterceptTouchEvent(event);
        }
        return false;
    }

    public void setHandleStrategy(HandleStrategy strategy) {
        mHandleStrategy = strategy;
        mHandleStrategy.setCloseClickHandler(mCloseButton::callOnClick);
        mLocationBar.showBranding();
    }

    private void updateButtonsTint() {
        updateButtonTint(mCloseButton);
        int numCustomActionButtons = mCustomActionButtons.getChildCount();
        for (int i = 0; i < numCustomActionButtons; i++) {
            updateButtonTint((ImageButton) mCustomActionButtons.getChildAt(i));
        }
        updateButtonTint(mLocationBar.getSecurityButton());
    }

    private void updateButtonTint(ImageButton button) {
        Drawable drawable = button.getDrawable();
        if (drawable instanceof TintedDrawable) {
            ((TintedDrawable) drawable).setTint(mUseDarkColors ? mDarkModeTint : mLightModeTint);
        }
    }

    private void updateToolbarLayoutMargin() {
        final boolean shouldShowIncognitoIcon = getToolbarDataProvider().isIncognito();
        mIncognitoImageView.setVisibility(shouldShowIncognitoIcon ? VISIBLE : GONE);

        int startMargin = calculateStartMarginWhenCloseButtonVisibilityGone();

        updateStartMarginOfVisibleElementsUntilLocationBarFrameLayout(startMargin);

        int locationBarLayoutChildIndex = getLocationBarFrameLayoutIndex();
        assert locationBarLayoutChildIndex != -1;
        updateLocationBarLayoutEndMargin(locationBarLayoutChildIndex);

        // Update left margin of mTitleUrlContainer here to make sure the security icon is
        // always placed left of the urlbar.
        mLocationBar.updateLeftMarginOfTitleUrlContainer();
    }

    private int calculateStartMarginWhenCloseButtonVisibilityGone() {
        return (mCloseButton.getVisibility() == GONE) ? getResources().getDimensionPixelSize(
                       R.dimen.custom_tabs_toolbar_horizontal_margin_no_close)
                                                      : 0;
    }

    private void updateStartMarginOfVisibleElementsUntilLocationBarFrameLayout(int startMargin) {
        int locationBarFrameLayoutIndex = getLocationBarFrameLayoutIndex();
        for (int i = 0; i < locationBarFrameLayoutIndex; ++i) {
            View childView = getChildAt(i);
            if (childView.getVisibility() == GONE) continue;

            updateViewLayoutParams(childView, startMargin);

            LayoutParams childLayoutParams = (LayoutParams) childView.getLayoutParams();
            int widthMeasureSpec = calcWidthMeasure(childLayoutParams);
            int heightMeasureSpec = calcHeightMeasure(childLayoutParams);
            childView.measure(widthMeasureSpec, heightMeasureSpec);
            startMargin += childView.getMeasuredWidth();
        }

        updateStartMarginOfLocationBarFrameLayout(startMargin);
    }

    private void updateStartMarginOfLocationBarFrameLayout(int startMargin) {
        int locationBarFrameLayoutIndex = getLocationBarFrameLayoutIndex();
        View locationBarLayoutView = getChildAt(locationBarFrameLayoutIndex);
        updateViewLayoutParams(locationBarLayoutView, startMargin);
    }

    private void updateViewLayoutParams(View view, int margin) {
        LayoutParams layoutParams = (LayoutParams) view.getLayoutParams();
        if (MarginLayoutParamsCompat.getMarginStart(layoutParams) != margin) {
            MarginLayoutParamsCompat.setMarginStart(layoutParams, margin);
            view.setLayoutParams(layoutParams);
        }
    }

    private void updateLocationBarLayoutEndMargin(int startIndex) {
        int locationBarLayoutEndMargin = 0;
        for (int i = startIndex + 1; i < getChildCount(); i++) {
            View childView = getChildAt(i);
            if (childView.getVisibility() != GONE) {
                locationBarLayoutEndMargin += childView.getMeasuredWidth();
            }
        }
        LayoutParams urlLayoutParams = (LayoutParams) mLocationBar.getLayout().getLayoutParams();

        if (MarginLayoutParamsCompat.getMarginEnd(urlLayoutParams) != locationBarLayoutEndMargin) {
            MarginLayoutParamsCompat.setMarginEnd(urlLayoutParams, locationBarLayoutEndMargin);
            mLocationBar.getLayout().setLayoutParams(urlLayoutParams);
        }
    }

    private int getLocationBarFrameLayoutIndex() {
        assert mLocationBar.getLayout().getVisibility() != GONE;
        for (int i = 0; i < getChildCount(); i++) {
            if (getChildAt(i) == mLocationBar.getLayout()) return i;
        }
        return -1;
    }

    private int calcWidthMeasure(LayoutParams childLayoutParams) {
        if (childLayoutParams.width == LayoutParams.WRAP_CONTENT) {
            return MeasureSpec.makeMeasureSpec(getMeasuredWidth(), MeasureSpec.AT_MOST);
        }

        if (childLayoutParams.width == LayoutParams.MATCH_PARENT) {
            return MeasureSpec.makeMeasureSpec(getMeasuredWidth(), MeasureSpec.EXACTLY);
        }

        return MeasureSpec.makeMeasureSpec(childLayoutParams.width, MeasureSpec.EXACTLY);
    }

    private int calcHeightMeasure(LayoutParams childLayoutParams) {
        if (childLayoutParams.height == LayoutParams.WRAP_CONTENT) {
            return MeasureSpec.makeMeasureSpec(getMeasuredHeight(), MeasureSpec.AT_MOST);
        }

        if (childLayoutParams.height == LayoutParams.MATCH_PARENT) {
            return MeasureSpec.makeMeasureSpec(getMeasuredHeight(), MeasureSpec.EXACTLY);
        }

        return MeasureSpec.makeMeasureSpec(childLayoutParams.height, MeasureSpec.EXACTLY);
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        mLocationBarModel.notifyTitleChanged();
        mLocationBarModel.notifyUrlChanged();
        mLocationBarModel.notifyPrimaryColorChanged();
    }

    @Override
    public ColorDrawable getBackground() {
        return (ColorDrawable) super.getBackground();
    }

    /**
     * For extending classes to override and carry out the changes related with the primary color
     * for the current tab changing.
     */
    @Override
    protected void onPrimaryColorChanged(boolean shouldAnimate) {
        if (mBrandColorTransitionActive) mBrandColorTransitionAnimation.cancel();

        final ColorDrawable background = getBackground();
        final int startColor = background.getColor();
        final int endColor = getToolbarDataProvider().getPrimaryColor();

        if (background.getColor() == endColor) return;

        mBrandColorTransitionAnimation = ValueAnimator.ofFloat(0, 1).setDuration(
                ToolbarPhone.THEME_COLOR_TRANSITION_DURATION);
        mBrandColorTransitionAnimation.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        mBrandColorTransitionAnimation.addUpdateListener(animation -> {
            float fraction = animation.getAnimatedFraction();
            int red   = (int) interpolate(Color.red(startColor),   Color.red(endColor),   fraction);
            int blue  = (int) interpolate(Color.blue(startColor),  Color.blue(endColor),  fraction);
            int green = (int) interpolate(Color.green(startColor), Color.green(endColor), fraction);
            int color = Color.rgb(red, green, blue);
            background.setColor(color);
            setHandleViewBackgroundColor(color);
        });
        mBrandColorTransitionAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mBrandColorTransitionActive = false;

                // Using the current background color instead of the final color in case this
                // animation was cancelled.  This ensures the assets are updated to the visible
                // color.
                updateUseDarkColors(background.getColor());
            }
        });
        mBrandColorTransitionAnimation.start();
        mBrandColorTransitionActive = true;
        if (!shouldAnimate) mBrandColorTransitionAnimation.end();
    }

    private void updateUseDarkColors(int backgroundColor) {
        boolean useDarkColors = !ColorUtils.shouldUseLightForegroundOnBackground(backgroundColor);
        if (mUseDarkColors == useDarkColors) return;
        mUseDarkColors = useDarkColors;
        mLocationBar.updateUseDarkColors();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        updateToolbarLayoutMargin();
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    public LocationBar getLocationBar() {
        return mLocationBar;
    }

    public void setHandleView(ImageView view) {
        mHandleView = view;
        setHandleViewBackgroundColor(getBackground().getColor());
    }

    private void setHandleViewBackgroundColor(int color) {
        if (mHandleView == null) return;
        GradientDrawable drawable = (GradientDrawable) mHandleView.getBackground();
        ((GradientDrawable) drawable.mutate()).setColor(color);
    }

    @Override
    public boolean onLongClick(View v) {
        if (v == mCloseButton || v.getParent() == mCustomActionButtons) {
            return Toast.showAnchoredToast(getContext(), v, v.getContentDescription());
        }
        return false;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static String parsePublisherNameFromUrl(GURL url) {
        // TODO(ianwen): Make it generic to parse url from URI path. http://crbug.com/599298
        // The url should look like: https://www.google.com/amp/s/www.nyt.com/ampthml/blogs.html
        // or https://www.google.com/amp/www.nyt.com/ampthml/blogs.html.
        String[] segments = url.getPath().split("/");
        if (segments.length >= 4 && "amp".equals(segments[1])) {
            if (segments[2].length() > 1) return segments[2];
            return segments[3];
        }
        return url.getSpec();
    }

    @Override
    protected void onMenuButtonDisabled() {
        super.onMenuButtonDisabled();
        // In addition to removing the menu button, we also need to remove the margin on the custom
        // action button.
        ViewGroup.MarginLayoutParams p =
                (ViewGroup.MarginLayoutParams) mCustomActionButtons.getLayoutParams();
        p.setMarginEnd(0);
        mCustomActionButtons.setLayoutParams(p);
    }

    private static class NoOpkeyboardVisibilityDelegate extends KeyboardVisibilityDelegate {
        @Override
        public void showKeyboard(View view) {}

        @Override
        public boolean hideKeyboard(View view) {
            return false;
        }

        @Override
        public int calculateKeyboardHeight(View view) {
            return 0;
        }

        @Override
        public boolean isKeyboardShowing(Context context, View view) {
            return false;
        }

        @Override
        public void addKeyboardVisibilityListener(KeyboardVisibilityListener listener) {}

        @Override
        public void removeKeyboardVisibilityListener(KeyboardVisibilityListener listener) {}
    }

    /**
     * Custom tab-specific implementation of the LocationBar interface.
     */
    private class CustomTabLocationBar implements LocationBar, UrlBar.UrlBarDelegate,
                                                  LocationBarDataProvider.Observer,
                                                  View.OnLongClickListener {
        private static final int TITLE_ANIM_DELAY_MS = 800;
        private static final int BRANDING_DELAY_MS = 1800;

        private static final int STATE_DOMAIN_ONLY = 0;
        private static final int STATE_TITLE_ONLY = 1;
        private static final int STATE_DOMAIN_AND_TITLE = 2;
        private int mState = STATE_DOMAIN_ONLY;

        private LocationBarDataProvider mLocationBarDataProvider;
        private Supplier<ModalDialogManager> mModalDialogManagerSupplier;
        private UrlBarCoordinator mUrlCoordinator;

        private TextView mUrlBar;
        private TextView mTitleBar;
        private View mLocationBarFrameLayout;
        private View mTitleUrlContainer;
        private ImageButton mSecurityButton;

        private CustomTabToolbarAnimationDelegate mAnimDelegate;
        private final Runnable mTitleAnimationStarter = new Runnable() {
            @Override
            public void run() {
                mAnimDelegate.startTitleAnimation(getContext());
            }
        };

        private boolean mCurrentlyShowingBranding;
        private List<Runnable> mAfterBrandingRunnables;

        public View getLayout() {
            return mLocationBarFrameLayout;
        }

        public ImageButton getSecurityButton() {
            return mSecurityButton;
        }

        public boolean isShowingTitleOnly() {
            return mState == STATE_TITLE_ONLY;
        }

        public void showBranding() {
            mCurrentlyShowingBranding = true;
            mAfterBrandingRunnables = new ArrayList<>();

            // Store the title and domain setting.
            final boolean showTitle = mState != STATE_DOMAIN_ONLY;
            final boolean showUrlBar = mState != STATE_TITLE_ONLY;

            // We use title bar to show the branding text and hide the url bar so the text will
            // align with the security icon.
            setShowTitle(true);
            setUrlBarHidden(true);
            showBrandingIconAndText();

            PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT, () -> {
                mCurrentlyShowingBranding = false;
                setUrlBarHidden(!showUrlBar);
                setShowTitle(showTitle);
                for (Runnable runnable : mAfterBrandingRunnables) {
                    runnable.run();
                }
                mAfterBrandingRunnables.clear();
            }, BRANDING_DELAY_MS);
        }

        public void onFinishInflate(View container) {
            mUrlBar = (TextView) container.findViewById(R.id.url_bar);
            mUrlBar.setHint("");
            mUrlBar.setEnabled(false);
            mTitleBar = container.findViewById(R.id.title_bar);
            mLocationBarFrameLayout = container.findViewById(R.id.location_bar_frame_layout);
            mTitleUrlContainer = container.findViewById(R.id.title_url_container);
            mTitleUrlContainer.setOnLongClickListener(this);
            mSecurityButton = container.findViewById(R.id.security_button);
            mAnimDelegate = new CustomTabToolbarAnimationDelegate(
                    mSecurityButton, mTitleUrlContainer, R.dimen.location_bar_icon_width);
        }

        public void init(LocationBarDataProvider locationBarDataProvider,
                Supplier<ModalDialogManager> modalDialogManagerSupplier,
                ActionMode.Callback actionModeCallback) {
            mLocationBarDataProvider = locationBarDataProvider;
            mLocationBarDataProvider.addObserver(this);
            mModalDialogManagerSupplier = modalDialogManagerSupplier;
            mUrlCoordinator = new UrlBarCoordinator((UrlBar) mUrlBar, /*windowDelegate=*/null,
                    actionModeCallback,
                    /*focusChangeCallback=*/
                    (unused)
                            -> {},
                    this, new NoOpkeyboardVisibilityDelegate(),
                    locationBarDataProvider.isIncognito());
            updateUseDarkColors();
            updateSecurityIcon();
            updateProgressBarColors();
            updateUrlBar();
        }

        public void setUrlBarHidden(boolean hideUrlBar) {
            // Urlbar visibility cannot be toggled if it is the only visible element.
            if (mState == STATE_DOMAIN_ONLY) return;

            if (hideUrlBar && mState == STATE_DOMAIN_AND_TITLE) {
                mState = STATE_TITLE_ONLY;
                mAnimDelegate.setTitleAnimationEnabled(false);
                mUrlBar.setVisibility(View.GONE);
                mTitleBar.setVisibility(View.VISIBLE);
                LayoutParams lp = (LayoutParams) mTitleBar.getLayoutParams();
                lp.bottomMargin = 0;
                mTitleBar.setLayoutParams(lp);
                mTitleBar.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                        getResources().getDimension(R.dimen.location_bar_url_text_size));
            } else if (!hideUrlBar && mState == STATE_TITLE_ONLY) {
                mState = STATE_DOMAIN_AND_TITLE;
                mTitleBar.setVisibility(View.VISIBLE);
                mUrlBar.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                        getResources().getDimension(R.dimen.custom_tabs_url_text_size));
                mUrlBar.setVisibility(View.VISIBLE);
                LayoutParams lp = (LayoutParams) mTitleBar.getLayoutParams();
                lp.bottomMargin = getResources().getDimensionPixelSize(
                        R.dimen.custom_tabs_toolbar_vertical_padding);
                mTitleBar.setLayoutParams(lp);
                mTitleBar.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                        getResources().getDimension(R.dimen.custom_tabs_title_text_size));
                // Refresh the status icon and url bar.
                mLocationBarModel.notifyUrlChanged();
                mLocationBarModel.notifySecurityStateChanged();
            } else {
                assert false : "Unreached state";
            }
        }

        public void onNativeLibraryReady() {
            mSecurityButton.setOnClickListener(v -> {
                Tab currentTab = mLocationBarDataProvider.getTab();
                if (currentTab == null) return;
                WebContents webContents = currentTab.getWebContents();
                if (webContents == null) return;
                Activity activity = currentTab.getWindowAndroid().getActivity().get();
                if (activity == null) return;
                if (mCurrentlyShowingBranding) return;
                // For now we don't show "store info" row for custom tab.
                new ChromePageInfo(mModalDialogManagerSupplier, getContentPublisher(),
                        OpenedFromSource.TOOLBAR, /*storeInfoActionHandlerSupplier=*/null)
                        .show(currentTab, ChromePageInfoHighlight.noHighlight());
            });
        }

        @Override
        public View getViewForUrlBackFocus() {
            Tab tab = getCurrentTab();
            if (tab == null) return null;
            return tab.getView();
        }

        @Override
        public boolean allowKeyboardLearning() {
            return !CustomTabToolbar.this.isIncognito();
        }

        @Override
        public void gestureDetected(boolean isLongPress) {}

        // LocationBarDataProvider.Observer implementation
        // Using the default empty onIncognitoStateChanged.
        // Using the default empty onNtpStartedLoading.

        @Override
        public void onPrimaryColorChanged() {
            updateUseDarkColors();
            updateSecurityIcon();
            updateProgressBarColors();
        }

        @Override
        public void onSecurityStateChanged() {
            updateSecurityIcon();
        }

        @Override
        public void onTitleChanged() {
            updateTitleBar();
        }

        @Override
        public void onUrlChanged() {
            updateUrlBar();
        }

        @Override
        public void updateVisualsForState() {
            updateSecurityIcon();
            updateProgressBarColors();
            updateUrlBar();
        }

        private void updateLeftMarginOfTitleUrlContainer() {
            int leftMargin = mSecurityButton.getMeasuredWidth();
            LayoutParams lp = (LayoutParams) mTitleUrlContainer.getLayoutParams();

            if (mSecurityButton.getVisibility() == View.GONE) {
                leftMargin -= mSecurityButton.getMeasuredWidth();
            }

            lp.leftMargin = leftMargin;
            mTitleUrlContainer.setLayoutParams(lp);
        }

        private void updateProgressBarColors() {
            final ToolbarProgressBar progressBar = getProgressBar();
            if (progressBar == null) return;
            final Context context = getContext();
            final int backgroundColor = getBackground().getColor();
            if (ThemeUtils.isUsingDefaultToolbarColor(
                        context, /*isIncognito=*/false, backgroundColor)) {
                progressBar.setBackgroundColor(context.getColor(R.color.progress_bar_bg_color));
                progressBar.setForegroundColor(
                        SemanticColorUtils.getProgressBarForeground(context));
            } else {
                progressBar.setThemeColor(backgroundColor, /*isIncognito=*/false);
            }
        }

        private void showBrandingIconAndText() {
            ColorStateList colorStateList = AppCompatResources.getColorStateList(
                    getContext(), mLocationBarDataProvider.getSecurityIconColorStateList());
            ApiCompatibilityUtils.setImageTintList(mSecurityButton, colorStateList);
            mAnimDelegate.updateSecurityButton(R.drawable.chromelogo16);

            mTitleBar.setText(R.string.twa_running_in_chrome);
            mTitleAnimationStarter.run();
        }

        private void updateSecurityIcon() {
            if (mState == STATE_TITLE_ONLY) return;
            if (mCurrentlyShowingBranding) {
                mAfterBrandingRunnables.add(this::updateSecurityIcon);
                return;
            }

            int securityIconResource = mLocationBarDataProvider.getSecurityIconResource(
                    DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext()));
            if (securityIconResource != 0) {
                ColorStateList colorStateList = AppCompatResources.getColorStateList(
                        getContext(), mLocationBarDataProvider.getSecurityIconColorStateList());
                ApiCompatibilityUtils.setImageTintList(mSecurityButton, colorStateList);
            }
            mAnimDelegate.updateSecurityButton(securityIconResource);

            int contentDescriptionId =
                    mLocationBarDataProvider.getSecurityIconContentDescriptionResourceId();
            String contentDescription = getContext().getString(contentDescriptionId);
            mSecurityButton.setContentDescription(contentDescription);
        }

        private void updateTitleBar() {
            if (mCurrentlyShowingBranding) {
                mAfterBrandingRunnables.add(this::updateTitleBar);
                return;
            }
            String title = mLocationBarDataProvider.getTitle();
            if (!mLocationBarDataProvider.hasTab() || TextUtils.isEmpty(title)) {
                mTitleBar.setText("");
                return;
            }

            // It takes some time to parse the title of the webcontent, and before that
            // LocationBarDataProvider#getTitle always returns the url. We postpone the title
            // animation until the title is authentic.
            if ((mState == STATE_DOMAIN_AND_TITLE || mState == STATE_TITLE_ONLY)
                    && !title.equals(mLocationBarDataProvider.getCurrentUrl())
                    && !title.equals(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL)) {
                // Delay the title animation until security icon animation finishes.
                // If this is updated after branding, we don't need to wait.
                PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT, mTitleAnimationStarter,
                        mAfterBrandingRunnables != null && mAfterBrandingRunnables.size() > 0
                                ? 0
                                : TITLE_ANIM_DELAY_MS);
            }

            mTitleBar.setText(title);
        }

        private void updateUrlBar() {
            if (mCurrentlyShowingBranding) {
                mAfterBrandingRunnables.add(this::updateUrlBar);
                return;
            }
            Tab tab = getCurrentTab();
            if (tab == null) {
                mUrlCoordinator.setUrlBarData(
                        UrlBarData.EMPTY, UrlBar.ScrollType.NO_SCROLL, SelectionState.SELECT_ALL);
                return;
            }

            String publisherUrl = TrustedCdn.getPublisherUrl(tab);
            String url = publisherUrl != null ? publisherUrl : tab.getUrl().getSpec().trim();
            if (mState == STATE_TITLE_ONLY) {
                if (!TextUtils.isEmpty(mLocationBarDataProvider.getTitle())) {
                    updateTitleBar();
                }
            }

            // Don't show anything for Chrome URLs and "about:blank".
            // If we have taken a pre-initialized WebContents, then the starting URL
            // is "about:blank". We should not display it.
            if (NativePage.isNativePageUrl(url, getCurrentTab().isIncognito())
                    || ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL.equals(url)) {
                mUrlCoordinator.setUrlBarData(
                        UrlBarData.EMPTY, UrlBar.ScrollType.NO_SCROLL, SelectionState.SELECT_ALL);
                return;
            }
            final CharSequence displayText;
            final int originStart;
            final int originEnd;
            if (publisherUrl != null) {
                String plainDisplayText =
                        getContext().getString(R.string.custom_tab_amp_publisher_url,
                                UrlUtilities.extractPublisherFromPublisherUrl(publisherUrl));
                ColorStateList tint = mUseDarkColors ? mDarkModeTint : mLightModeTint;
                SpannableString formattedDisplayText = SpanApplier.applySpans(plainDisplayText,
                        new SpanInfo("<pub>", "</pub>", ORIGIN_SPAN),
                        new SpanInfo(
                                "<bg>", "</bg>", new ForegroundColorSpan(tint.getDefaultColor())));
                originStart = formattedDisplayText.getSpanStart(ORIGIN_SPAN);
                originEnd = formattedDisplayText.getSpanEnd(ORIGIN_SPAN);
                formattedDisplayText.removeSpan(ORIGIN_SPAN);
                displayText = formattedDisplayText;
            } else {
                UrlBarData urlBarData = mLocationBarDataProvider.getUrlBarData();
                displayText = urlBarData.displayText.subSequence(
                        urlBarData.originStartIndex, urlBarData.originEndIndex);
                originStart = 0;
                originEnd = displayText.length();
            }

            mUrlCoordinator.setUrlBarData(
                    UrlBarData.create(url, displayText, originStart, originEnd, url),
                    UrlBar.ScrollType.SCROLL_TO_TLD, SelectionState.SELECT_ALL);
        }

        private void updateUseDarkColors() {
            updateButtonsTint();
            @BrandedColorScheme
            int brandedColorScheme = mUseDarkColors ? BrandedColorScheme.LIGHT_BRANDED_THEME
                                                    : BrandedColorScheme.DARK_BRANDED_THEME;
            if (mUrlCoordinator.setBrandedColorScheme(brandedColorScheme)) {
                // Update the URL to make it use the new color scheme.
                updateUrlBar();
            }

            mTitleBar.setTextColor(ApiCompatibilityUtils.getColor(getResources(),
                    mUseDarkColors ? R.color.branded_url_text_on_light_bg
                                   : R.color.branded_url_text_on_dark_bg));
        }

        @Override
        public void setShowTitle(boolean showTitle) {
            if (showTitle) {
                mState = STATE_DOMAIN_AND_TITLE;
                mAnimDelegate.prepareTitleAnim(mUrlBar, mTitleBar);
            } else {
                mState = STATE_DOMAIN_ONLY;
                mTitleBar.setVisibility(View.GONE);
            }
            mLocationBarModel.notifyTitleChanged();
        }

        @Override
        public View getContainerView() {
            return CustomTabToolbar.this;
        }

        @Override
        public View getSecurityIconView() {
            return mSecurityButton;
        }

        @Override
        public void backKeyPressed() {
            assert false : "The URL bar should never take focus in CCTs.";
        }

        @Override
        public void destroy() {
            if (mLocationBarDataProvider != null) {
                mLocationBarDataProvider.removeObserver(this);
                mLocationBarDataProvider = null;
            }
        }

        @Override
        public void showUrlBarCursorWithoutFocusAnimations() {}

        @Override
        public void selectAll() {}

        @Override
        public void revertChanges() {}

        @Nullable
        @Override
        public OmniboxStub getOmniboxStub() {
            return null;
        }

        @Override
        public boolean onLongClick(View v) {
            if (v == mTitleUrlContainer) {
                Tab tab = getCurrentTab();
                if (tab == null) return false;
                Clipboard.getInstance().copyUrlToClipboard(tab.getOriginalUrl());
                return true;
            }
            return false;
        }
    }
}
