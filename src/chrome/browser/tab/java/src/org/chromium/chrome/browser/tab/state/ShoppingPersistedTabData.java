// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactory;
import org.chromium.chrome.browser.page_annotations.BuyableProductPageAnnotation;
import org.chromium.chrome.browser.page_annotations.PageAnnotation;
import org.chromium.chrome.browser.page_annotations.PageAnnotationUtils;
import org.chromium.chrome.browser.page_annotations.PageAnnotationsServiceFactory;
import org.chromium.chrome.browser.page_annotations.ProductPriceUpdatePageAnnotation;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.proto.PriceTracking.BuyableProduct;
import org.chromium.chrome.browser.tab.proto.PriceTracking.PriceTrackingData;
import org.chromium.chrome.browser.tab.proto.PriceTracking.ProductPriceUpdate;
import org.chromium.chrome.browser.tab.proto.ShoppingPersistedTabData.ShoppingPersistedTabDataProto;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.HintsProto;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.TimeUnit;

/**
 * {@link PersistedTabData} for Shopping related websites
 */
public class ShoppingPersistedTabData extends PersistedTabData {
    private static final String TAG = "SPTD";
    private static final String STALE_TAB_THRESHOLD_SECONDS_PARAM =
            "price_tracking_stale_tab_threshold_seconds";
    private static final String TIME_TO_LIVE_MS_PARAM = "price_tracking_time_to_live_ms";
    private static final String DISPLAY_TIME_MS_PARAM = "price_tracking_display_time_ms";
    private static final String PRICE_TRACKING_WITH_OPTIMIZATION_GUIDE_PARAM =
            "price_tracking_with_optimization_guide";
    private static final String METRICS_IDENTIFIER_PREFIX = "NavigationComplete";

    private static final int FRACTIONAL_DIGITS_LESS_THAN_TEN_UNITS = 2;
    private static final int FRACTIONAL_DIGITS_GREATER_THAN_TEN_UNITS = 0;

    private static final Class<ShoppingPersistedTabData> USER_DATA_KEY =
            ShoppingPersistedTabData.class;
    private static final int MICROS_TO_UNITS = 1000000;
    private static final long TWO_UNITS = 2 * MICROS_TO_UNITS;
    private static final long TEN_UNITS = 10 * MICROS_TO_UNITS;
    private static final int MINIMUM_DROP_PERCENTAGE = 10;
    private static final int ONE_WEEK_MS = (int) TimeUnit.DAYS.toMillis(7);

    @VisibleForTesting
    public static final long ONE_HOUR_MS = TimeUnit.HOURS.toMillis(1);

    private static final int NINETY_DAYS_SECONDS = (int) TimeUnit.DAYS.toSeconds(90);

    @VisibleForTesting
    public static final long NO_TRANSITIONS_OCCURRED = -1;

    @VisibleForTesting
    public static final long NO_PRICE_KNOWN = -1;

    @VisibleForTesting
    protected static PageAnnotationsServiceFactory sPageAnnotationsServiceFactory =
            new PageAnnotationsServiceFactory();

    private static boolean sPriceTrackingWithOptimizationGuideForTesting;

    public long mLastPriceChangeTimeMs = NO_TRANSITIONS_OCCURRED;

    private PriceDropData mPriceDropData = new PriceDropData();
    private PriceDropMetricsLogger mPriceDropMetricsLogger;

    @VisibleForTesting
    protected ObservableSupplierImpl<Boolean> mIsTabSaveEnabledSupplier =
            new ObservableSupplierImpl<>();

    @VisibleForTesting
    protected EmptyTabObserver mUrlUpdatedObserver;

    @IntDef({PriceDropMethod.NONE, PriceDropMethod.LEGACY, PriceDropMethod.NEW})
    @Retention(RetentionPolicy.SOURCE)
    protected @interface PriceDropMethod {
        int NONE = 0;
        int LEGACY = 1;
        int NEW = 2;
    }

    @VisibleForTesting
    protected @PriceDropMethod int mPriceDropMethod = PriceDropMethod.NEW;

    // Lazy initialization of OptimizationGuideBridgeFactory
    private static class OptimizationGuideBridgeFactoryHolder {
        private static final OptimizationGuideBridgeFactory sOptimizationGuideBridgeFactory;
        static {
            List<HintsProto.OptimizationType> optimizationTypes;
            if (isPriceTrackingWithOptimizationGuideEnabled()) {
                optimizationTypes =
                        Arrays.asList(HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR,
                                HintsProto.OptimizationType.PRICE_TRACKING);
            } else {
                optimizationTypes =
                        Arrays.asList(HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR);
            }
            sOptimizationGuideBridgeFactory = new OptimizationGuideBridgeFactory(optimizationTypes);
        }
    }

    /**
     * Raw price drop data acquired from backend service. This is converted to formatted
     * price strings in the public API getPriceDrop().
     */
    private static class PriceDropData {
        public long priceMicros;
        public long previousPriceMicros;
        public String currencyCode;
        public String offerId;

        PriceDropData() {
            this.priceMicros = NO_PRICE_KNOWN;
            this.previousPriceMicros = NO_PRICE_KNOWN;
        }

        /**
         * @param priceMicros the price in micros
         * @param previousPriceMicros previous price in micros
         * @param currencyCode the currency code of the price
         * @param offerId the offer id associated with the price drop
         */
        PriceDropData(
                long priceMicros, long previousPriceMicros, String currencyCode, String offerId) {
            this.priceMicros = priceMicros;
            this.previousPriceMicros = previousPriceMicros;
            this.currencyCode = currencyCode;
            this.offerId = offerId;
        }
    }

    /**
     * A price drop for the offer {@link ShoppingPersistedTabData}
     * refers to
     */
    public static class PriceDrop {
        public final String price;
        public final String previousPrice;

        /**
         * @param price {@link String} representation of the price
         * @param previousPrice {@link String} representation of the previous price
         */
        public PriceDrop(String price, String previousPrice) {
            this.price = price;
            this.previousPrice = previousPrice;
        }

        @Override
        public boolean equals(Object object) {
            if (!(object instanceof PriceDrop)) return false;
            PriceDrop priceDrop = (PriceDrop) object;
            return this.price.equals(priceDrop.price)
                    && this.previousPrice.equals(priceDrop.previousPrice);
        }

        @Override
        public int hashCode() {
            int result = 17;
            result = 31 * result + (price == null ? 0 : price.hashCode());
            result = 31 * result + (previousPrice == null ? 0 : previousPrice.hashCode());
            return result;
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public ShoppingPersistedTabData(Tab tab) {
        super(tab,
                PersistedTabDataConfiguration.get(ShoppingPersistedTabData.class, tab.isIncognito())
                        .getStorage(),
                PersistedTabDataConfiguration.get(ShoppingPersistedTabData.class, tab.isIncognito())
                        .getId());
        setupPersistence(tab);
    }

    private void prefetchOnNewNavigation(Tab tab, NavigationHandle navigationHandle) {
        prefetchOnNewNavigation(tab, navigationHandle, null);
    }

    /**
     * @param tab {@link Tab} associated with {@link ShoppingPersistedTabData}
     * @param navigationHandle the {@link NavigationHandle} associated with the new navigation
     * @param onCompleteForTesting runnable which indicates when the callback has completed for test
     *         synchronization
     */
    @VisibleForTesting
    protected void prefetchOnNewNavigation(
            Tab tab, NavigationHandle navigationHandle, Runnable onCompleteForTesting) {
        if (!navigationHandle.isInMainFrame()) {
            return;
        }
        OptimizationGuideBridgeFactoryHolder.sOptimizationGuideBridgeFactory.create()
                .canApplyOptimizationAsync(navigationHandle,
                        HintsProto.OptimizationType.PRICE_TRACKING, (decision, metadata) -> {
                            if (!tab.isInitialized()
                                    || !tab.getUrl().equals(navigationHandle.getUrl())
                                    || decision != OptimizationGuideDecision.TRUE) {
                                if (onCompleteForTesting != null) {
                                    onCompleteForTesting.run();
                                }
                                return;
                            }
                            try {
                                PriceTrackingData priceTrackingDataProto =
                                        PriceTrackingData.parseFrom(metadata.getValue());
                                parsePriceTrackingDataProto(tab, priceTrackingDataProto, null);
                                setLastUpdatedMs(System.currentTimeMillis());
                                mPriceDropMetricsLogger = new PriceDropMetricsLogger(this);
                                mPriceDropMetricsLogger.logPriceDropMetrics(
                                        METRICS_IDENTIFIER_PREFIX);
                            } catch (InvalidProtocolBufferException e) {
                                Log.i(TAG,
                                        String.format(Locale.US,
                                                "There was a problem "
                                                        + "parsing "
                                                        + "PriceTracking"
                                                        + "DataProto. "
                                                        + "Details %s.",
                                                e));
                            }
                            if (onCompleteForTesting != null) {
                                onCompleteForTesting.run();
                            }
                        });
    }

    /**
     * Log price drop metrics, if we have price drop data
     * @param locationIdentifier where in the user experience the metrics were
     * called from.
     */
    public void logPriceDropMetrics(String locationIdentifier) {
        if (mPriceDropMetricsLogger != null) {
            mPriceDropMetricsLogger.logPriceDropMetrics(locationIdentifier);
        }
    }

    @VisibleForTesting
    protected PriceDropMetricsLogger getPriceDropMetricsLoggerForTesting() {
        return mPriceDropMetricsLogger;
    }

    @VisibleForTesting
    protected ShoppingPersistedTabData(
            Tab tab, ByteBuffer data, PersistedTabDataStorage storage, String persistedTabDataId) {
        super(tab, storage, persistedTabDataId);
        deserializeAndLog(data);
        setupPersistence(tab);
    }

    private void setupPersistence(Tab tab) {
        // ShoppingPersistedTabData is not saved by default - only when its fields are populated
        // (after a successful endpoint repsonse)
        disableSaving();
        registerIsTabSaveEnabledSupplier(mIsTabSaveEnabledSupplier);
        mUrlUpdatedObserver = new EmptyTabObserver() {
            @Override
            public void onUrlUpdated(Tab tab) {
                // When the URL is updated, the pricing data is stale, no longer
                // relevant and should be cleaned up.
                delete();
                mPriceDropData = new PriceDropData();
                mPriceDropMetricsLogger = null;
            }

            @Override
            public void onDidFinishNavigation(Tab tab, NavigationHandle navigationHandle) {
                if (isPriceTrackingWithOptimizationGuideEnabled()) {
                    prefetchOnNewNavigation(tab, navigationHandle);
                }
            }
        };
        tab.addObserver(mUrlUpdatedObserver);
    }

    @VisibleForTesting
    public EmptyTabObserver getUrlUpdatedObserverForTesting() {
        return mUrlUpdatedObserver;
    }

    @VisibleForTesting
    public PriceDropData getPriceDropDataForTesting() {
        return mPriceDropData;
    }

    /**
     * Acquire {@link ShoppingPersistedTabData} for a {@link Tab}
     * @param tab {@link Tab} ShoppingPersistedTabData is acquired for
     * @param callback {@link Callback} receiving the Tab's {@link ShoppingPersistedTabData}
     * The result in the callback wil be null for a:
     * - Custom Tab
     * - Incognito Tab
     * - Tab greater than 90 days old
     * - Tab with a non-shopping related page currently navigated to
     * - Tab with a shopping related page for which no shopping related data was found
     */
    public static void from(Tab tab, Callback<ShoppingPersistedTabData> callback) {
        // Shopping related data is not available for incognito or Custom Tabs. For example,
        // for incognito Tabs it is not possible to call a backend service with the user's URL.
        if (tab.isIncognito() || tab.isCustomTab()) {
            PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> { callback.onResult(null); });
            return;
        }
        PersistedTabData.from(tab,
                (data, storage, id)
                        -> { return new ShoppingPersistedTabData(tab, data, storage, id); },
                (supplierCallback)
                        -> {
                    if (getTimeSinceTabLastOpenedMs(tab)
                            > TimeUnit.SECONDS.toMillis(getStaleTabThresholdSeconds())) {
                        supplierCallback.onResult(null);
                        return;
                    }
                    ShoppingPersistedTabData previous = PersistedTabData.from(tab, USER_DATA_KEY);
                    ShoppingPersistedTabData.isShoppingPage(tab.getUrl(), (isShoppingPage) -> {
                        if (!isShoppingPage) {
                            supplierCallback.onResult(null);
                            return;
                        }

                        if (isPriceTrackingWithOptimizationGuideEnabled()) {
                            OptimizationGuideBridgeFactoryHolder.sOptimizationGuideBridgeFactory
                                    .create()
                                    .canApplyOptimization(tab.getUrl(),
                                            HintsProto.OptimizationType.PRICE_TRACKING,
                                            (decision, metadata) -> {
                                                if (!tab.isInitialized()
                                                        || decision
                                                                != OptimizationGuideDecision.TRUE) {
                                                    supplierCallback.onResult(null);
                                                    return;
                                                }
                                                try {
                                                    PriceTrackingData priceTrackingDataProto =
                                                            PriceTrackingData.parseFrom(
                                                                    metadata.getValue());
                                                    ShoppingPersistedTabData
                                                            shoppingPersistedTabData =
                                                                    new ShoppingPersistedTabData(
                                                                            tab);
                                                    shoppingPersistedTabData
                                                            .parsePriceTrackingDataProto(tab,
                                                                    priceTrackingDataProto,
                                                                    previous);
                                                    supplierCallback.onResult(
                                                            shoppingPersistedTabData);
                                                } catch (InvalidProtocolBufferException e) {
                                                    Log.i(TAG,
                                                            String.format(Locale.US,
                                                                    "There was a problem "
                                                                            + "parsing "
                                                                            + "PriceTracking"
                                                                            + "DataProto. "
                                                                            + "Details %s.",
                                                                    e));
                                                    supplierCallback.onResult(null);
                                                }
                                            });
                        } else {
                            sPageAnnotationsServiceFactory.getForLastUsedProfile().getAnnotations(
                                    tab.getUrl(), (result) -> {
                                        supplierCallback.onResult(build(tab, result, previous));
                                    });
                        }
                    });
                },
                ShoppingPersistedTabData.class, callback);
    }

    /**
     * @return {@link ShoppingPersistedTabData} from {@link UserDataHost}
     */
    public static ShoppingPersistedTabData from(Tab tab) {
        ShoppingPersistedTabData shoppingPersistedTabData =
                PersistedTabData.from(tab, USER_DATA_KEY);
        if (shoppingPersistedTabData == null) {
            shoppingPersistedTabData = tab.getUserDataHost().setUserData(
                    USER_DATA_KEY, new ShoppingPersistedTabData(tab));
        }
        return shoppingPersistedTabData;
    }

    /**
     * @param tab {@link Tab} of interest.
     * @return true if the {@link Tab} has a price drop associated with it.
     */
    public static boolean hasPriceDrop(Tab tab) {
        ShoppingPersistedTabData shoppingPersistedTabData =
                PersistedTabData.from(tab, USER_DATA_KEY);
        if (shoppingPersistedTabData == null) {
            return false;
        }
        return shoppingPersistedTabData.getPriceDrop() != null;
    }

    /**
     * Whether a BuyableProductAnnotation was found or not
     */
    @IntDef({FoundBuyableProduct.NOT_FOUND, FoundBuyableProduct.FOUND,
            FoundBuyableProduct.FOUND_WITH_PRICE_UPDATE})
    @Retention(RetentionPolicy.SOURCE)
    @interface FoundBuyableProduct {
        int NOT_FOUND = 0;
        int FOUND = 1;
        int FOUND_WITH_PRICE_UPDATE = 2;
        int NUM_ENTRIES = 3;
    }

    @IntDef({FoundBuyableProductAnnotation.NOT_FOUND, FoundBuyableProductAnnotation.FOUND,
            FoundBuyableProductAnnotation.FOUND_WITH_PRICE_UPDATE})
    @Retention(RetentionPolicy.SOURCE)
    @interface FoundBuyableProductAnnotation {
        int NOT_FOUND = 0;
        int FOUND = 1;
        int FOUND_WITH_PRICE_UPDATE = 2;
        int NUM_ENTRIES = 3;
    }

    /**
     * Enable saving of {@link ShoppingPersistedTabData}
     */
    protected void enableSaving() {
        mIsTabSaveEnabledSupplier.set(true);
    }

    /**
     * Disable saving of {@link ShoppingPersistedTabData}. Deletes previously saved {@link
     * ShoppingPersistedTabData} as well.
     */
    public void disableSaving() {
        mIsTabSaveEnabledSupplier.set(false);
    }

    private static ShoppingPersistedTabData build(Tab tab, List<PageAnnotation> annotations,
            ShoppingPersistedTabData previousShoppingPersistedTabData) {
        ShoppingPersistedTabData res = new ShoppingPersistedTabData(tab);
        @FoundBuyableProductAnnotation
        int foundBuyableProductAnnotation = FoundBuyableProductAnnotation.NOT_FOUND;

        BuyableProductPageAnnotation buyableProduct =
                PageAnnotationUtils.getAnnotation(annotations, BuyableProductPageAnnotation.class);

        ProductPriceUpdatePageAnnotation productPriceUpdate = PageAnnotationUtils.getAnnotation(
                annotations, ProductPriceUpdatePageAnnotation.class);

        if (buyableProduct != null && productPriceUpdate != null) {
            res.setPriceMicros(productPriceUpdate.getNewPriceMicros());
            res.setPreviousPriceMicros(productPriceUpdate.getOldPriceMicros());
            res.setCurrencyCode(productPriceUpdate.getCurrencyCode());
            res.setLastUpdatedMs(System.currentTimeMillis());
            res.setMainOfferId(buyableProduct.getOfferId());
            foundBuyableProductAnnotation = FoundBuyableProductAnnotation.FOUND_WITH_PRICE_UPDATE;
        } else if (buyableProduct != null) {
            res.setPriceMicros(
                    buyableProduct.getCurrentPriceMicros(), previousShoppingPersistedTabData);
            res.setCurrencyCode(buyableProduct.getCurrencyCode());
            res.setLastUpdatedMs(System.currentTimeMillis());
            res.setMainOfferId(buyableProduct.getOfferId());
            foundBuyableProductAnnotation = FoundBuyableProductAnnotation.FOUND;
        }

        RecordHistogram.recordEnumeratedHistogram(
                "Tabs.ShoppingPersistedTabData.FoundBuyableProductAnnotation",
                foundBuyableProductAnnotation, FoundBuyableProductAnnotation.NUM_ENTRIES);
        // Only persist this ShoppingPersistedTabData if it was correctly populated from the
        // response
        if (foundBuyableProductAnnotation == FoundBuyableProductAnnotation.FOUND
                || foundBuyableProductAnnotation
                        == FoundBuyableProductAnnotation.FOUND_WITH_PRICE_UPDATE) {
            res.enableSaving();
            return res;
        }
        return null;
    }

    @VisibleForTesting
    protected void parsePriceTrackingDataProto(Tab tab, PriceTrackingData priceTrackingData,
            ShoppingPersistedTabData previousShoppingPersistedTabData) {
        @FoundBuyableProduct
        int foundBuyableProduct = FoundBuyableProduct.NOT_FOUND;

        ProductPriceUpdate productUpdate = priceTrackingData.getProductUpdate();
        BuyableProduct buyableProduct = priceTrackingData.getBuyableProduct();

        if (hasPriceUpdate(priceTrackingData)) {
            setPriceMicros(productUpdate.getNewPrice().getAmountMicros());
            setPreviousPriceMicros(productUpdate.getOldPrice().getAmountMicros());
            setCurrencyCode(productUpdate.getOldPrice().getCurrencyCode());
            setLastUpdatedMs(System.currentTimeMillis());
            setMainOfferId(String.valueOf(buyableProduct.getOfferId()));
            foundBuyableProduct = FoundBuyableProduct.FOUND_WITH_PRICE_UPDATE;
        } else if (hasPrice(priceTrackingData)) {
            setPriceMicros(buyableProduct.getCurrentPrice().getAmountMicros(),
                    previousShoppingPersistedTabData);
            setCurrencyCode(buyableProduct.getCurrentPrice().getCurrencyCode());
            setLastUpdatedMs(System.currentTimeMillis());
            setMainOfferId(String.valueOf(buyableProduct.getOfferId()));
            foundBuyableProduct = FoundBuyableProduct.FOUND;
        }

        RecordHistogram.recordEnumeratedHistogram(
                "Tabs.ShoppingPersistedTabData.FoundBuyableProduct", foundBuyableProduct,
                FoundBuyableProduct.NUM_ENTRIES);
        // Only persist this ShoppingPersistedTabData if it was correctly populated from the
        // response
        if (foundBuyableProduct == FoundBuyableProduct.FOUND
                || foundBuyableProduct == FoundBuyableProduct.FOUND_WITH_PRICE_UPDATE) {
            enableSaving();
        }
    }

    private static boolean hasPriceUpdate(PriceTrackingData priceTrackingDataProto) {
        if (!priceTrackingDataProto.hasBuyableProduct()
                || !priceTrackingDataProto.hasProductUpdate()) {
            return false;
        }
        ProductPriceUpdate productUpdateProto = priceTrackingDataProto.getProductUpdate();
        if (!productUpdateProto.hasNewPrice() || !productUpdateProto.hasOldPrice()) {
            return false;
        }
        if (!productUpdateProto.getNewPrice().hasCurrencyCode()
                || !productUpdateProto.getOldPrice().hasCurrencyCode()) {
            return false;
        }
        if (!productUpdateProto.getNewPrice().getCurrencyCode().equals(
                    productUpdateProto.getOldPrice().getCurrencyCode())) {
            return false;
        }
        return true;
    }

    private static boolean hasPrice(PriceTrackingData priceTrackingDataProto) {
        if (!priceTrackingDataProto.hasBuyableProduct()) {
            return false;
        }
        if (!priceTrackingDataProto.getBuyableProduct().hasCurrentPrice()) {
            return false;
        }
        if (!priceTrackingDataProto.getBuyableProduct().getCurrentPrice().hasAmountMicros()
                || !priceTrackingDataProto.getBuyableProduct()
                            .getCurrentPrice()
                            .hasCurrencyCode()) {
            return false;
        }
        return true;
    }

    /**
     * Set the price string
     * @param priceString a string representing the price of the shopping offer
     * @param previousShoppingPersistedTabData {@link ShoppingPersistedTabData} from previous fetch
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public void setPriceMicros(
            long priceMicros, ShoppingPersistedTabData previousShoppingPersistedTabData) {
        mPriceDropData.priceMicros = priceMicros;
        // Detect price transition
        if (previousShoppingPersistedTabData != null && priceMicros != NO_PRICE_KNOWN
                && previousShoppingPersistedTabData.getPriceMicros() != NO_PRICE_KNOWN
                && priceMicros != previousShoppingPersistedTabData.getPriceMicros()) {
            mPriceDropData.previousPriceMicros = previousShoppingPersistedTabData.getPriceMicros();
            mLastPriceChangeTimeMs = System.currentTimeMillis();
        } else if (previousShoppingPersistedTabData != null) {
            mPriceDropData.previousPriceMicros =
                    previousShoppingPersistedTabData.getPreviousPriceMicros();
            mLastPriceChangeTimeMs = previousShoppingPersistedTabData.getLastPriceChangeTimeMs();
        }
        save();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public void setPriceMicros(long priceMicros) {
        mPriceDropData.priceMicros = priceMicros;
        save();
    }

    protected void setCurrencyCode(String currencyCode) {
        mPriceDropData.currencyCode = currencyCode;
        save();
    }

    @VisibleForTesting
    protected String getCurrencyCode() {
        return mPriceDropData.currencyCode;
    }

    @VisibleForTesting
    protected void setPreviousPriceMicros(long previousPriceMicros) {
        mPriceDropData.previousPriceMicros = previousPriceMicros;
        save();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public long getPriceMicros() {
        return mPriceDropData.priceMicros;
    }

    /**
     * @return true if there is a price
     */
    protected boolean hasPriceMicros() {
        return mPriceDropData.priceMicros != NO_PRICE_KNOWN;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public long getPreviousPriceMicros() {
        return mPriceDropData.previousPriceMicros;
    }

    /**
     * @return true if there is a previous price
     */
    protected boolean hasPreviousPriceMicros() {
        return mPriceDropData.previousPriceMicros != NO_PRICE_KNOWN;
    }

    @VisibleForTesting
    public void setPriceMicrosForTesting(long priceMicros) {
        mPriceDropData.priceMicros = priceMicros;
    }

    @VisibleForTesting
    public void setPreviousPriceMicrosForTesting(long previousPriceMicros) {
        mPriceDropData.previousPriceMicros = previousPriceMicros;
    }

    public void setMainOfferId(String offerId) {
        mPriceDropData.offerId = offerId;
        save();
    }

    public String getMainOfferId() {
        return mPriceDropData.offerId;
    }

    /**
     * @return {@link PriceDrop} relating to the offer for the {@link ShoppingPersistedTabData}
     * TODO(crbug.com/1145770) Implement getPriceDrop to only return a result if there is
     * actually a price drop. Ensure priceString and previousPriceString are integers.
     * Deprecate getPrice and getPriceString(). Change price and previousPriceString
     * representations to be numeric to make drop comparison easier.
     */
    public PriceDrop getPriceDropLegacy() {
        assert mPriceDropMethod == PriceDropMethod.LEGACY;
        if (mPriceDropData.priceMicros == NO_PRICE_KNOWN
                || mPriceDropData.previousPriceMicros == NO_PRICE_KNOWN || !isQualifyingPriceDrop()
                || isPriceChangeStale()) {
            return null;
        }
        String formattedPrice = formatPrice(mPriceDropData.priceMicros);
        String formattedPreviousPrice = formatPrice(mPriceDropData.previousPriceMicros);
        if (formattedPrice.equals(formattedPreviousPrice)) {
            return null;
        }
        return new PriceDrop(formattedPrice, formattedPreviousPrice);
    }

    /**
     * @return {@link PriceDrop} relating to the main offer in the page.
     */
    public PriceDrop getPriceDrop() {
        assert mPriceDropMethod == PriceDropMethod.NEW;
        if (!isValidPriceDropUpdate() || isPriceChangeStale()) {
            return null;
        }
        return createPriceDrop(mPriceDropData.priceMicros, mPriceDropData.previousPriceMicros);
    }

    private boolean isValidPriceDropUpdate() {
        return mPriceDropData.priceMicros != NO_PRICE_KNOWN
                && mPriceDropData.previousPriceMicros != NO_PRICE_KNOWN
                && mPriceDropData.priceMicros < mPriceDropData.previousPriceMicros;
    }

    private PriceDrop createPriceDrop(long priceMicros, long previousPriceMicros) {
        String formattedPrice = formatPrice(priceMicros);
        String formattedPreviousPrice = formatPrice(previousPriceMicros);
        if (formattedPrice.equals(formattedPreviousPrice)) {
            return null;
        }

        return new PriceDrop(formattedPrice, formattedPreviousPrice);
    }

    private boolean isPriceChangeStale() {
        return mLastPriceChangeTimeMs != NO_TRANSITIONS_OCCURRED
                && System.currentTimeMillis() - mLastPriceChangeTimeMs > getDisplayTimeMs();
    }

    private boolean isQualifyingPriceDrop() {
        if (mPriceDropData.previousPriceMicros - mPriceDropData.priceMicros
                < getMinimumDropThresholdAbsolute()) {
            return false;
        }
        if ((100L * mPriceDropData.priceMicros) / mPriceDropData.previousPriceMicros
                > (100 - getMinimumDroppedThresholdPercentage())) {
            return false;
        }
        return true;
    }

    // TODO(crbug.com/1151156) Make parameters finch configurable
    private static int getMinimumDroppedThresholdPercentage() {
        return MINIMUM_DROP_PERCENTAGE;
    }

    private static long getMinimumDropThresholdAbsolute() {
        return TWO_UNITS;
    }

    // TODO(crbug.com/1130068) support all currencies
    private String formatPrice(long priceMicros) {
        CurrencyFormatter currencyFormatter =
                new CurrencyFormatter(mPriceDropData.currencyCode, Locale.getDefault());
        String formattedPrice;
        if (priceMicros < TEN_UNITS) {
            currencyFormatter.setMaximumFractionalDigits(FRACTIONAL_DIGITS_LESS_THAN_TEN_UNITS);
            formattedPrice = String.format(
                    Locale.getDefault(), "%.2f", (100 * priceMicros / MICROS_TO_UNITS) / 100.0);
        } else {
            currencyFormatter.setMaximumFractionalDigits(FRACTIONAL_DIGITS_GREATER_THAN_TEN_UNITS);
            formattedPrice = String.format(Locale.getDefault(), "%d",
                    (long) Math.floor(
                            (double) (priceMicros + MICROS_TO_UNITS / 2) / MICROS_TO_UNITS));
        }
        return currencyFormatter.format(formattedPrice);
    }

    @Override
    public Supplier<ByteBuffer> getSerializeSupplier() {
        ShoppingPersistedTabDataProto.Builder builder =
                ShoppingPersistedTabDataProto.newBuilder()
                        .setPriceMicros(mPriceDropData.priceMicros)
                        .setPreviousPriceMicros(mPriceDropData.previousPriceMicros)
                        .setLastUpdatedMs(getLastUpdatedMs())
                        .setLastPriceChangeTimeMs(mLastPriceChangeTimeMs);
        if (mPriceDropData.offerId != null) {
            builder.setMainOfferId(mPriceDropData.offerId);
        }

        return () -> {
            return builder.build().toByteString().asReadOnlyByteBuffer();
        };
    }

    @Override
    public boolean deserialize(@Nullable ByteBuffer bytes) {
        // TODO(crbug.com/1135573) add in metrics for serialize and deserialize
        // Do not attempt to deserialize if the bytes are null
        if (bytes == null) {
            return false;
        }
        try {
            ShoppingPersistedTabDataProto shoppingPersistedTabDataProto =
                    ShoppingPersistedTabDataProto.parseFrom(bytes);
            mPriceDropData.priceMicros = shoppingPersistedTabDataProto.getPriceMicros();
            mPriceDropData.previousPriceMicros =
                    shoppingPersistedTabDataProto.getPreviousPriceMicros();
            setLastUpdatedMs(shoppingPersistedTabDataProto.getLastUpdatedMs());
            mLastPriceChangeTimeMs = shoppingPersistedTabDataProto.getLastPriceChangeTimeMs();
            mPriceDropData.offerId = shoppingPersistedTabDataProto.getMainOfferId();
            mPriceDropMetricsLogger = new PriceDropMetricsLogger(this);
            return true;
        } catch (InvalidProtocolBufferException e) {
            Log.e(TAG,
                    String.format(Locale.US,
                            "There was a problem deserializing "
                                    + "ShoppingPersistedTabData. Details: %s",
                            e.getMessage()));
        }
        return false;
    }

    @Override
    public String getUmaTag() {
        return "SPTD";
    }

    @Override
    public long getTimeToLiveMs() {
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.COMMERCE_PRICE_TRACKING, TIME_TO_LIVE_MS_PARAM,
                    (int) ONE_HOUR_MS);
        }
        return (int) ONE_HOUR_MS;
    }

    @VisibleForTesting
    public long getLastPriceChangeTimeMs() {
        return mLastPriceChangeTimeMs;
    }

    @VisibleForTesting
    public void setLastPriceChangeTimeMsForTesting(long lastPriceChangeTimeMs) {
        mLastPriceChangeTimeMs = lastPriceChangeTimeMs;
    }

    @Override
    public void destroy() {
        mTab.removeObserver(mUrlUpdatedObserver);
        super.destroy();
    }

    /**
     * @return the threshold in seconds at which a {@link Tab} is considered stale
     * (and we no longer acquire shopping data for stale tabs).
     */
    public static int getStaleTabThresholdSeconds() {
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.COMMERCE_PRICE_TRACKING, STALE_TAB_THRESHOLD_SECONDS_PARAM,
                    NINETY_DAYS_SECONDS);
        }
        return NINETY_DAYS_SECONDS;
    }

    /**
     * @return true is acquiring pricing data using OptimizationGuide is enabled.
     */
    public static boolean isPriceTrackingWithOptimizationGuideEnabled() {
        if (sPriceTrackingWithOptimizationGuideForTesting) {
            return true;
        }
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.COMMERCE_PRICE_TRACKING,
                    PRICE_TRACKING_WITH_OPTIMIZATION_GUIDE_PARAM, false);
        }
        return false;
    }

    @VisibleForTesting
    public static void enablePriceTrackingWithOptimizationGuideForTesting() {
        sPriceTrackingWithOptimizationGuideForTesting = true;
    }

    private static int getDisplayTimeMs() {
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.COMMERCE_PRICE_TRACKING, DISPLAY_TIME_MS_PARAM, ONE_WEEK_MS);
        }
        return ONE_WEEK_MS;
    }

    private static long getTimeSinceTabLastOpenedMs(Tab tab) {
        return System.currentTimeMillis() - CriticalPersistedTabData.from(tab).getTimestampMillis();
    }

    // TODO(crbug.com/1196860) remove as OptimizationType.PRICE_TRACKING deprecates the need for
    // this
    private static void isShoppingPage(GURL url, Callback<Boolean> callback) {
        OptimizationGuideBridgeFactoryHolder.sOptimizationGuideBridgeFactory.create()
                .canApplyOptimization(url, HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR,
                        (decision, metadata) -> {
                            callback.onResult(decision == OptimizationGuideDecision.TRUE
                                    || decision == OptimizationGuideDecision.UNKNOWN);
                        });
    }
}