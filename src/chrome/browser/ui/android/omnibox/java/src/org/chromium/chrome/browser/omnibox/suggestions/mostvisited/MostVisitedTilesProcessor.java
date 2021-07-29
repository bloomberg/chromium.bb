// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.mostvisited;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.drawable.RoundedBitmapDrawable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxTheme;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionViewProperties;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.components.browser_ui.widget.tile.TileViewBinder;
import org.chromium.components.browser_ui.widget.tile.TileViewProperties;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * SuggestionProcessor for Most Visited URL tiles.
 * TODO(crbug.com/1106109): Write unit tests.
 */
public class MostVisitedTilesProcessor extends BaseCarouselSuggestionProcessor {
    private final @NonNull Context mContext;
    private final @NonNull SuggestionHost mSuggestionHost;
    private final @NonNull Supplier<LargeIconBridge> mIconBridgeSupplier;
    private final int mMinCarouselItemViewHeight;
    private final int mDesiredFaviconWidthPx;
    private final RoundedIconGenerator mIconGenerator;
    private static final int DEFAULT_TILE_TYPE = 0;

    private @NonNull ExploreIconProvider mExploreIconProvider;
    private @NonNull DiscardableReferencePool mReferencePool;
    private @Nullable DiscardableReferencePool
            .DiscardableReference<RoundedBitmapDrawable> mExploreSitesIcon;

    /**
     * Constructor.
     *
     * @param context An Android context.
     * @param host SuggestionHost receiving notifications about user actions.
     * @param iconBridgeSupplier Supplier of the LargeIconBridge used to fetch site favicons.
     * @param exploreIconProvider The provider to get the large icon of explore sites.
     */
    public MostVisitedTilesProcessor(@NonNull Context context, @NonNull SuggestionHost host,
            @NonNull Supplier<LargeIconBridge> iconBridgeSupplier,
            @NonNull ExploreIconProvider exploreIconProvider,
            @NonNull DiscardableReferencePool referencePool) {
        super(context);
        mContext = context;
        mSuggestionHost = host;
        mIconBridgeSupplier = iconBridgeSupplier;
        mExploreIconProvider = exploreIconProvider;
        mReferencePool = referencePool;
        mMinCarouselItemViewHeight =
                mContext.getResources().getDimensionPixelSize(R.dimen.tile_view_min_height);
        mDesiredFaviconWidthPx = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_favicon_size);

        int fallbackIconSize =
                mContext.getResources().getDimensionPixelSize(R.dimen.tile_view_icon_size);
        int fallbackIconColor = ApiCompatibilityUtils.getColor(
                mContext.getResources(), R.color.default_favicon_background_color);
        int fallbackIconTextSize =
                mContext.getResources().getDimensionPixelSize(R.dimen.tile_view_icon_text_size);
        mIconGenerator = new RoundedIconGenerator(fallbackIconSize, fallbackIconSize,
                fallbackIconSize / 2, fallbackIconColor, fallbackIconTextSize);
        mExploreSitesIcon = null;
    }

    @Override
    public boolean doesProcessSuggestion(AutocompleteMatch suggestion, int position) {
        return suggestion.getType() == OmniboxSuggestionType.TILE_NAVSUGGEST;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.TILE_NAVSUGGEST;
    }

    @Override
    public PropertyModel createModel() {
        return new PropertyModel(BaseCarouselSuggestionViewProperties.ALL_KEYS);
    }

    @Override
    public int getMinimumCarouselItemViewHeight() {
        return mMinCarouselItemViewHeight;
    }

    @Override
    public void populateModel(AutocompleteMatch suggestion, PropertyModel model, int position) {
        final List<AutocompleteMatch.NavsuggestTile> tiles = suggestion.getNavsuggestTiles();
        final int tilesCount = tiles.size();
        final List<ListItem> tileList = new ArrayList<>(tilesCount);
        final LargeIconBridge iconBridge = mIconBridgeSupplier.get();

        for (int index = 0; index < tilesCount; index++) {
            final PropertyModel tileModel = new PropertyModel(TileViewProperties.ALL_KEYS);
            final String title = tiles.get(index).title;
            final GURL url = tiles.get(index).url;
            tileModel.set(TileViewProperties.TITLE, title);
            tileModel.set(TileViewProperties.TITLE_LINES, 1);
            tileModel.set(TileViewProperties.ON_FOCUS_VIA_SELECTION,
                    () -> mSuggestionHost.setOmniboxEditingText(url.getSpec()));
            tileModel.set(TileViewProperties.ON_CLICK,
                    v -> mSuggestionHost.onSuggestionClicked(suggestion, position, url));
            tileModel.set(TileViewProperties.CONTENT_DESCRIPTION,
                    mContext.getString(R.string.accessibility_omnibox_most_visited_tile, title,
                            url.getHost()));

            // TODO(https://crbug.com/1106109): Cache the generated bitmaps.
            // This is not needed for the experimentation purposes (this block is currently
            // used very infrequently - only to offer zero-prefix URL suggestions on URL visit).
            // This must be addressed before MV Carousel can be offered in more contexts.
            // Consider unifying cache with LargeIconBridge if possible.
            Bitmap fallbackIcon = mIconGenerator.generateIconForUrl(url);
            tileModel.set(TileViewProperties.ICON, new BitmapDrawable(fallbackIcon));

            if (TextUtils.equals(url.getSpec(), UrlConstants.EXPLORE_URL)) {
                setExploreSitesIcon(tileModel);
            } else if (iconBridge != null) {
                iconBridge.getLargeIconForUrl(tiles.get(index).url, mDesiredFaviconWidthPx,
                        (Bitmap icon, int fallbackColor, boolean isFallbackColorDefault,
                                int iconType) -> {
                            if (icon == null) return;
                            tileModel.set(TileViewProperties.ICON, new BitmapDrawable(icon));
                        });
            }

            tileList.add(new ListItem(DEFAULT_TILE_TYPE, tileModel));
        }

        model.set(BaseCarouselSuggestionViewProperties.TILES, tileList);
        model.set(BaseCarouselSuggestionViewProperties.SHOW_TITLE, false);
    }

    /**
     * Sets the Explore sites icon to the property model.
     */
    void setExploreSitesIcon(PropertyModel tileModel) {
        if (mExploreSitesIcon == null || mExploreSitesIcon.get() == null) {
            // Only fetches the Explore sites icon once per Chrome launch, since the icon
            // is updated once a month or less.
            mExploreIconProvider.getSummaryImage(mDesiredFaviconWidthPx, (Bitmap icon) -> {
                if (icon == null) return;

                RoundedBitmapDrawable roundIcon = getRoundIconFromBitmap(icon);
                mExploreSitesIcon = mReferencePool.put(roundIcon);
                setLargeIcon(tileModel, roundIcon);
            });
        } else {
            setLargeIcon(tileModel, mExploreSitesIcon.get());
        }
    }

    /**
     * Gets a round icon from the given bitmap.
     */
    private RoundedBitmapDrawable getRoundIconFromBitmap(Bitmap icon) {
        RoundedBitmapDrawable roundedIcon = ViewUtils.createRoundedBitmapDrawable(
                mContext.getResources(), icon, mDesiredFaviconWidthPx);
        roundedIcon.setAntiAlias(true);
        roundedIcon.setFilterBitmap(true);
        return roundedIcon;
    }

    /**
     * Sets the large icon to the given property model.
     */
    private void setLargeIcon(PropertyModel tileModel, RoundedBitmapDrawable icon) {
        tileModel.set(TileViewProperties.SHOW_LARGE_ICON, true);
        tileModel.set(TileViewProperties.ICON, icon);
    }

    /**
     * Create Carousel Suggestion View presenting the Most Visited URL tiles.
     *
     * @param parent ViewGroup that will host the Carousel view.
     * @return BaseCarouselSuggestionView for the Most Visited URL suggestions.
     */
    public static BaseCarouselSuggestionView createView(ViewGroup parent) {
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(new ModelList());
        adapter.registerType(
                DEFAULT_TILE_TYPE, MostVisitedTilesProcessor::buildTile, TileViewBinder::bind);
        return new BaseCarouselSuggestionView(parent.getContext(), adapter);
    }

    /**
     * Create a Tile element for the Most Visited URL suggestions.
     *
     * @param parent ViewGroup that will host the Tile.
     * @return A TileView element for the individual URL suggestion.
     */
    private static TileView buildTile(ViewGroup parent) {
        TileView tile = (TileView) LayoutInflater.from(parent.getContext())
                                .inflate(R.layout.suggestions_tile_view, parent, false);
        tile.setClickable(true);

        Drawable background = OmniboxResourceProvider.resolveAttributeToDrawable(
                parent.getContext(), OmniboxTheme.LIGHT_THEME, R.attr.selectableItemBackground);
        tile.setBackgroundDrawable(background);
        return tile;
    }

    @VisibleForTesting
    RoundedBitmapDrawable getExploreSitesIconForTesting() {
        return mExploreSitesIcon != null ? mExploreSitesIcon.get() : null;
    }
}
