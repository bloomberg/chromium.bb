// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;
import android.text.format.DateUtils;

import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ResourceId;
import org.chromium.chrome.browser.preferences.MainPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.List;
import java.util.Locale;

/**
 * Android wrapper of the PersonalDataManager which provides access from the Java
 * layer.
 *
 * Only usable from the UI thread as it's primary purpose is for supporting the Android
 * preferences UI.
 *
 * See chrome/browser/autofill/personal_data_manager.h for more details.
 */
@JNINamespace("autofill")
public class PersonalDataManager {

    /**
     * Observer of PersonalDataManager events.
     */
    public interface PersonalDataManagerObserver {
        /**
         * Called when the data is changed.
         */
        void onPersonalDataChanged();
    }

    /**
     * Callback for full card request.
     */
    public interface FullCardRequestDelegate {
        /**
         * Called when user provided the full card details, including the CVC and the full PAN.
         *
         * @param card The full card.
         * @param cvc The CVC for the card.
         */
        @CalledByNative("FullCardRequestDelegate")
        void onFullCardDetails(CreditCard card, String cvc);

        /**
         * Called when user did not provide full card details.
         */
        @CalledByNative("FullCardRequestDelegate")
        void onFullCardError();
    }

    /**
     * Callback for subKeys request.
     */
    public interface GetSubKeysRequestDelegate {
        /**
         * Called when the subkeys are received sucessfully.
         * Here the subkeys are admin areas.
         *
         * @param subKeysCodes The subkeys' codes.
         * @param subKeysNames The subkeys' names.
         */
        @CalledByNative("GetSubKeysRequestDelegate")
        void onSubKeysReceived(String[] subKeysCodes, String[] subKeysNames);
    }

    /**
     * Callback for normalized addresses.
     */
    public interface NormalizedAddressRequestDelegate {
        /**
         * Called when the address has been sucessfully normalized.
         *
         * @param profile The profile with the normalized address.
         */
        @CalledByNative("NormalizedAddressRequestDelegate")
        void onAddressNormalized(AutofillProfile profile);

        /**
         * Called when the address could not be normalized.
         *
         * @param profile The non normalized profile.
         */
        @CalledByNative("NormalizedAddressRequestDelegate")
        void onCouldNotNormalize(AutofillProfile profile);
    }

    /**
     * Autofill address information.
     */
    public static class AutofillProfile {
        private String mGUID;
        private String mOrigin;
        private boolean mIsLocal;
        private String mFullName;
        private String mCompanyName;
        private String mStreetAddress;
        private String mRegion;
        private String mLocality;
        private String mDependentLocality;
        private String mPostalCode;
        private String mSortingCode;
        private String mCountryCode;
        private String mPhoneNumber;
        private String mEmailAddress;
        private String mLabel;
        private String mLanguageCode;

        @CalledByNative("AutofillProfile")
        public static AutofillProfile create(String guid, String origin, boolean isLocal,
                String fullName, String companyName, String streetAddress, String region,
                String locality, String dependentLocality, String postalCode, String sortingCode,
                String country, String phoneNumber, String emailAddress, String languageCode) {
            return new AutofillProfile(guid, origin, isLocal, fullName, companyName, streetAddress,
                    region, locality, dependentLocality, postalCode, sortingCode, country,
                    phoneNumber, emailAddress, languageCode);
        }

        public AutofillProfile(String guid, String origin, boolean isLocal, String fullName,
                String companyName, String streetAddress, String region, String locality,
                String dependentLocality, String postalCode, String sortingCode, String countryCode,
                String phoneNumber, String emailAddress, String languageCode) {
            mGUID = guid;
            mOrigin = origin;
            mIsLocal = isLocal;
            mFullName = fullName;
            mCompanyName = companyName;
            mStreetAddress = streetAddress;
            mRegion = region;
            mLocality = locality;
            mDependentLocality = dependentLocality;
            mPostalCode = postalCode;
            mSortingCode = sortingCode;
            mCountryCode = countryCode;
            mPhoneNumber = phoneNumber;
            mEmailAddress = emailAddress;
            mLanguageCode = languageCode;
        }

        /**
         * Builds an empty local profile with "settings" origin and country code from the default
         * locale. All other fields are empty strings, because JNI does not handle null strings.
         */
        public AutofillProfile() {
            this("" /* guid */, MainPreferences.SETTINGS_ORIGIN /* origin */, true /* isLocal */,
                    "" /* fullName */, "" /* companyName */, "" /* streetAddress */,
                    "" /* region */, "" /* locality */, "" /* dependentLocality */,
                    "" /* postalCode */, "" /* sortingCode */,
                    Locale.getDefault().getCountry() /* country */, "" /* phoneNumber */,
                    "" /* emailAddress */, "" /* languageCode */);
        }

        /* Builds an AutofillProfile that is an exact copy of the one passed as parameter. */
        public AutofillProfile(AutofillProfile profile) {
            mGUID = profile.getGUID();
            mOrigin = profile.getOrigin();
            mIsLocal = profile.getIsLocal();
            mFullName = profile.getFullName();
            mCompanyName = profile.getCompanyName();
            mStreetAddress = profile.getStreetAddress();
            mRegion = profile.getRegion();
            mLocality = profile.getLocality();
            mDependentLocality = profile.getDependentLocality();
            mPostalCode = profile.getPostalCode();
            mSortingCode = profile.getSortingCode();
            mCountryCode = profile.getCountryCode();
            mPhoneNumber = profile.getPhoneNumber();
            mEmailAddress = profile.getEmailAddress();
            mLanguageCode = profile.getLanguageCode();
            mLabel = profile.getLabel();
        }

        /** TODO(estade): remove this constructor. */
        @VisibleForTesting
        public AutofillProfile(String guid, String origin, String fullName, String companyName,
                String streetAddress, String region, String locality, String dependentLocality,
                String postalCode, String sortingCode, String countryCode, String phoneNumber,
                String emailAddress, String languageCode) {
            this(guid, origin, true /* isLocal */, fullName, companyName, streetAddress, region,
                    locality, dependentLocality, postalCode, sortingCode, countryCode, phoneNumber,
                    emailAddress, languageCode);
        }

        @CalledByNative("AutofillProfile")
        public String getGUID() {
            return mGUID;
        }

        @CalledByNative("AutofillProfile")
        public String getOrigin() {
            return mOrigin;
        }

        @CalledByNative("AutofillProfile")
        public String getFullName() {
            return mFullName;
        }

        @CalledByNative("AutofillProfile")
        public String getCompanyName() {
            return mCompanyName;
        }

        @CalledByNative("AutofillProfile")
        public String getStreetAddress() {
            return mStreetAddress;
        }

        @CalledByNative("AutofillProfile")
        public String getRegion() {
            return mRegion;
        }

        @CalledByNative("AutofillProfile")
        public String getLocality() {
            return mLocality;
        }

        @CalledByNative("AutofillProfile")
        public String getDependentLocality() {
            return mDependentLocality;
        }

        public String getLabel() {
            return mLabel;
        }

        @CalledByNative("AutofillProfile")
        public String getPostalCode() {
            return mPostalCode;
        }

        @CalledByNative("AutofillProfile")
        public String getSortingCode() {
            return mSortingCode;
        }

        @CalledByNative("AutofillProfile")
        public String getCountryCode() {
            return mCountryCode;
        }

        @CalledByNative("AutofillProfile")
        public String getPhoneNumber() {
            return mPhoneNumber;
        }

        @CalledByNative("AutofillProfile")
        public String getEmailAddress() {
            return mEmailAddress;
        }

        @CalledByNative("AutofillProfile")
        public String getLanguageCode() {
            return mLanguageCode;
        }

        public boolean getIsLocal() {
            return mIsLocal;
        }

        @VisibleForTesting
        public void setGUID(String guid) {
            mGUID = guid;
        }

        public void setLabel(String label) {
            mLabel = label;
        }

        public void setOrigin(String origin) {
            mOrigin = origin;
        }

        public void setFullName(String fullName) {
            mFullName = fullName;
        }

        public void setCompanyName(String companyName) {
            mCompanyName = companyName;
        }

        @VisibleForTesting
        public void setStreetAddress(String streetAddress) {
            mStreetAddress = streetAddress;
        }

        public void setRegion(String region) {
            mRegion = region;
        }

        public void setLocality(String locality) {
            mLocality = locality;
        }

        public void setDependentLocality(String dependentLocality) {
            mDependentLocality = dependentLocality;
        }

        public void setPostalCode(String postalCode) {
            mPostalCode = postalCode;
        }

        public void setSortingCode(String sortingCode) {
            mSortingCode = sortingCode;
        }

        @VisibleForTesting
        public void setCountryCode(String countryCode) {
            mCountryCode = countryCode;
        }

        public void setPhoneNumber(String phoneNumber) {
            mPhoneNumber = phoneNumber;
        }

        public void setEmailAddress(String emailAddress) {
            mEmailAddress = emailAddress;
        }

        @VisibleForTesting
        public void setLanguageCode(String languageCode) {
            mLanguageCode = languageCode;
        }

        public void setIsLocal(boolean isLocal) {
            mIsLocal = isLocal;
        }

        /** Used by ArrayAdapter in credit card settings. */
        @Override
        public String toString() {
            return mLabel;
        }
    }

    /**
     * Autofill credit card information.
     */
    public static class CreditCard {
        // Note that while some of these fields are numbers, they're predominantly read,
        // marshaled and compared as strings. To save conversions, we sometimes use strings.
        @CardType
        private final int mCardType;
        private String mGUID;
        private String mOrigin;
        private boolean mIsLocal;
        private boolean mIsCached;
        private String mName;
        private String mNumber;
        private String mObfuscatedNumber;
        private String mMonth;
        private String mYear;
        private String mBasicCardIssuerNetwork;
        private int mIssuerIconDrawableId;
        private String mBillingAddressId;
        private String mServerId;

        @CalledByNative("CreditCard")
        public static CreditCard create(String guid, String origin, boolean isLocal,
                boolean isCached, String name, String number, String obfuscatedNumber, String month,
                String year, String basicCardIssuerNetwork, int enumeratedIconId,
                @CardType int cardType, String billingAddressId, String serverId) {
            return new CreditCard(guid, origin, isLocal, isCached, name, number, obfuscatedNumber,
                    month, year, basicCardIssuerNetwork,
                    ResourceId.mapToDrawableId(enumeratedIconId), cardType, billingAddressId,
                    serverId);
        }

        public CreditCard(String guid, String origin, boolean isLocal, boolean isCached,
                String name, String number, String obfuscatedNumber, String month, String year,
                String basicCardIssuerNetwork, int issuerIconDrawableId, @CardType int cardType,
                String billingAddressId, String serverId) {
            mGUID = guid;
            mOrigin = origin;
            mIsLocal = isLocal;
            mIsCached = isCached;
            mName = name;
            mNumber = number;
            mObfuscatedNumber = obfuscatedNumber;
            mMonth = month;
            mYear = year;
            mBasicCardIssuerNetwork = basicCardIssuerNetwork;
            mIssuerIconDrawableId = issuerIconDrawableId;
            mCardType = cardType;
            mBillingAddressId = billingAddressId;
            mServerId = serverId;
        }

        public CreditCard() {
            this("" /* guid */, MainPreferences.SETTINGS_ORIGIN /*origin */, true /* isLocal */,
                    false /* isCached */, "" /* name */, "" /* number */, "" /* obfuscatedNumber */,
                    "" /* month */, "" /* year */, "" /* basicCardIssuerNetwork */,
                    0 /* issuerIconDrawableId */, CardType.UNKNOWN, "" /* billingAddressId */,
                    "" /* serverId */);
        }

        /** TODO(estade): remove this constructor. */
        @VisibleForTesting
        public CreditCard(String guid, String origin, String name, String number,
                String obfuscatedNumber, String month, String year) {
            this(guid, origin, true /* isLocal */, false /* isCached */, name, number,
                    obfuscatedNumber, month, year, "" /* basicCardIssuerNetwork */,
                    0 /* issuerIconDrawableId */, CardType.UNKNOWN, "" /* billingAddressId */,
                    "" /* serverId */);
        }

        @CalledByNative("CreditCard")
        public String getGUID() {
            return mGUID;
        }

        @CalledByNative("CreditCard")
        public String getOrigin() {
            return mOrigin;
        }

        @CalledByNative("CreditCard")
        public String getName() {
            return mName;
        }

        @CalledByNative("CreditCard")
        public String getNumber() {
            return mNumber;
        }

        public String getObfuscatedNumber() {
            return mObfuscatedNumber;
        }

        @CalledByNative("CreditCard")
        public String getMonth() {
            return mMonth;
        }

        @CalledByNative("CreditCard")
        public String getYear() {
            return mYear;
        }

        public String getFormattedExpirationDate(Context context) {
            return getMonth()
                    + context.getResources().getString(R.string.autofill_expiration_date_separator)
                    + getYear();
        }

        @CalledByNative("CreditCard")
        public boolean getIsLocal() {
            return mIsLocal;
        }

        @CalledByNative("CreditCard")
        public boolean getIsCached() {
            return mIsCached;
        }

        @CalledByNative("CreditCard")
        public String getBasicCardIssuerNetwork() {
            return mBasicCardIssuerNetwork;
        }

        public int getIssuerIconDrawableId() {
            return mIssuerIconDrawableId;
        }

        @CardType
        @CalledByNative("CreditCard")
        public int getCardType() {
            return mCardType;
        }

        @CalledByNative("CreditCard")
        public String getBillingAddressId() {
            return mBillingAddressId;
        }

        @CalledByNative("CreditCard")
        public String getServerId() {
            return mServerId;
        }

        @VisibleForTesting
        public void setGUID(String guid) {
            mGUID = guid;
        }

        public void setOrigin(String origin) {
            mOrigin = origin;
        }

        public void setName(String name) {
            mName = name;
        }

        @VisibleForTesting
        public void setNumber(String number) {
            mNumber = number;
        }

        public void setObfuscatedNumber(String obfuscatedNumber) {
            mObfuscatedNumber = obfuscatedNumber;
        }

        @VisibleForTesting
        public void setMonth(String month) {
            mMonth = month;
        }

        public void setYear(String year) {
            mYear = year;
        }

        public void setBasicCardIssuerNetwork(String network) {
            mBasicCardIssuerNetwork = network;
        }

        public void setIssuerIconDrawableId(int id) {
            mIssuerIconDrawableId = id;
        }

        public void setBillingAddressId(String id) {
            mBillingAddressId = id;
        }

        public boolean hasValidCreditCardExpirationDate() {
            if (mMonth.isEmpty() || mYear.isEmpty()) return false;

            Calendar expiryDate = Calendar.getInstance();
            // The mMonth value is 1 based but the month in calendar is 0 based.
            expiryDate.set(Calendar.MONTH, Integer.parseInt(mMonth) - 1);
            expiryDate.set(Calendar.YEAR, Integer.parseInt(mYear));
            // Add 1 minute to the expiry instance to ensure that the card is still valid on its
            // exact expiration date.
            expiryDate.add(Calendar.MINUTE, 1);
            return Calendar.getInstance().before(expiryDate);
        }
    }

    private static PersonalDataManager sManager;

    // Suppress FindBugs warning, since |sManager| is only used on the UI thread.
    public static PersonalDataManager getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sManager == null) {
            sManager = new PersonalDataManager();
        }
        return sManager;
    }

    private static int sRequestTimeoutSeconds = 5;

    private final long mPersonalDataManagerAndroid;
    private final List<PersonalDataManagerObserver> mDataObservers =
            new ArrayList<PersonalDataManagerObserver>();

    private PersonalDataManager() {
        // Note that this technically leaks the native object, however, PersonalDataManager
        // is a singleton that lives forever and there's no clean shutdown of Chrome on Android
        mPersonalDataManagerAndroid = PersonalDataManagerJni.get().init(PersonalDataManager.this);
    }

    /**
     * Called from native when template URL service is done loading.
     */
    @CalledByNative
    private void personalDataChanged() {
        ThreadUtils.assertOnUiThread();
        for (PersonalDataManagerObserver observer : mDataObservers) {
            observer.onPersonalDataChanged();
        }
    }

    /**
     * Registers a PersonalDataManagerObserver on the native side.
     */
    public boolean registerDataObserver(PersonalDataManagerObserver observer) {
        ThreadUtils.assertOnUiThread();
        assert !mDataObservers.contains(observer);
        mDataObservers.add(observer);
        return PersonalDataManagerJni.get().isDataLoaded(
                mPersonalDataManagerAndroid, PersonalDataManager.this);
    }

    /**
     * Unregisters the provided observer.
     */
    public void unregisterDataObserver(PersonalDataManagerObserver observer) {
        ThreadUtils.assertOnUiThread();
        assert (mDataObservers.size() > 0);
        assert (mDataObservers.contains(observer));
        mDataObservers.remove(observer);
    }

    /**
     * TODO(crbug.com/616102): Reduce the number of Java to Native calls when getting profiles.
     *
     * Gets the profiles to show in the settings page. Returns all the profiles without any
     * processing.
     *
     * @return The list of profiles to show in the settings.
     */
    public List<AutofillProfile> getProfilesForSettings() {
        ThreadUtils.assertOnUiThread();
        return getProfilesWithLabels(PersonalDataManagerJni.get().getProfileLabelsForSettings(
                                             mPersonalDataManagerAndroid, PersonalDataManager.this),
                PersonalDataManagerJni.get().getProfileGUIDsForSettings(
                        mPersonalDataManagerAndroid, PersonalDataManager.this));
    }

    /**
     * TODO(crbug.com/616102): Reduce the number of Java to Native calls when getting profiles
     *
     * Gets the profiles to suggest when filling a form or completing a transaction. The profiles
     * will have been processed to be more relevant to the user.
     *
     * @param includeNameInLabel Whether to include the name in the profile's label.
     * @return The list of profiles to suggest to the user.
     */
    public ArrayList<AutofillProfile> getProfilesToSuggest(boolean includeNameInLabel) {
        ThreadUtils.assertOnUiThread();
        return getProfilesWithLabels(
                PersonalDataManagerJni.get().getProfileLabelsToSuggest(mPersonalDataManagerAndroid,
                        PersonalDataManager.this, includeNameInLabel,
                        true /* includeOrganizationInLabel */, true /* includeCountryInLabel */),
                PersonalDataManagerJni.get().getProfileGUIDsToSuggest(
                        mPersonalDataManagerAndroid, PersonalDataManager.this));
    }

    /**
     * TODO(crbug.com/616102): Reduce the number of Java to Native calls when getting profiles.
     *
     * Gets the profiles to suggest when associating a billing address to a credit card. The
     * profiles will have been processed to be more relevant to the user.
     *
     * @param includeOrganizationInLabel Whether the organization name should be included in the
     *                                   label.
     *
     * @return The list of billing addresses to suggest to the user.
     */
    public ArrayList<AutofillProfile> getBillingAddressesToSuggest(
            boolean includeOrganizationInLabel) {
        ThreadUtils.assertOnUiThread();
        return getProfilesWithLabels(
                PersonalDataManagerJni.get().getProfileLabelsToSuggest(mPersonalDataManagerAndroid,
                        PersonalDataManager.this, true /* includeNameInLabel */,
                        includeOrganizationInLabel, false /* includeCountryInLabel */),
                PersonalDataManagerJni.get().getProfileGUIDsToSuggest(
                        mPersonalDataManagerAndroid, PersonalDataManager.this));
    }

    private ArrayList<AutofillProfile> getProfilesWithLabels(
            String[] profileLabels, String[] profileGUIDs) {
        ArrayList<AutofillProfile> profiles = new ArrayList<AutofillProfile>(profileGUIDs.length);
        for (int i = 0; i < profileGUIDs.length; i++) {
            AutofillProfile profile = PersonalDataManagerJni.get().getProfileByGUID(
                    mPersonalDataManagerAndroid, PersonalDataManager.this, profileGUIDs[i]);
            profile.setLabel(profileLabels[i]);
            profiles.add(profile);
        }

        return profiles;
    }

    public AutofillProfile getProfile(String guid) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getProfileByGUID(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    public void deleteProfile(String guid) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().removeByGUID(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    public String setProfile(AutofillProfile profile) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().setProfile(
                mPersonalDataManagerAndroid, PersonalDataManager.this, profile);
    }

    public String setProfileToLocal(AutofillProfile profile) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().setProfileToLocal(
                mPersonalDataManagerAndroid, PersonalDataManager.this, profile);
    }

    /**
     * Gets the credit cards to show in the settings page. Returns all the cards without any
     * processing.
     */
    public List<CreditCard> getCreditCardsForSettings() {
        ThreadUtils.assertOnUiThread();
        return getCreditCards(PersonalDataManagerJni.get().getCreditCardGUIDsForSettings(
                mPersonalDataManagerAndroid, PersonalDataManager.this));
    }

    /**
     * Gets the credit cards to suggest when filling a form or completing a transaction. The cards
     * will have been processed to be more relevant to the user.
     * @param includeServerCards Whether server cards should be included in the response.
     */
    public ArrayList<CreditCard> getCreditCardsToSuggest(boolean includeServerCards) {
        ThreadUtils.assertOnUiThread();
        return getCreditCards(PersonalDataManagerJni.get().getCreditCardGUIDsToSuggest(
                mPersonalDataManagerAndroid, PersonalDataManager.this, includeServerCards));
    }

    private ArrayList<CreditCard> getCreditCards(String[] creditCardGUIDs) {
        ArrayList<CreditCard> cards = new ArrayList<CreditCard>(creditCardGUIDs.length);
        for (int i = 0; i < creditCardGUIDs.length; i++) {
            cards.add(PersonalDataManagerJni.get().getCreditCardByGUID(
                    mPersonalDataManagerAndroid, PersonalDataManager.this, creditCardGUIDs[i]));
        }
        return cards;
    }

    public CreditCard getCreditCard(String guid) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getCreditCardByGUID(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    public CreditCard getCreditCardForNumber(String cardNumber) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getCreditCardForNumber(
                mPersonalDataManagerAndroid, PersonalDataManager.this, cardNumber);
    }

    public String setCreditCard(CreditCard card) {
        ThreadUtils.assertOnUiThread();
        assert card.getIsLocal();
        return PersonalDataManagerJni.get().setCreditCard(
                mPersonalDataManagerAndroid, PersonalDataManager.this, card);
    }

    public void updateServerCardBillingAddress(CreditCard card) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().updateServerCardBillingAddress(
                mPersonalDataManagerAndroid, PersonalDataManager.this, card);
    }

    public String getBasicCardIssuerNetwork(String cardNumber, boolean emptyIfInvalid) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getBasicCardIssuerNetwork(
                mPersonalDataManagerAndroid, PersonalDataManager.this, cardNumber, emptyIfInvalid);
    }

    @VisibleForTesting
    public void addServerCreditCardForTest(CreditCard card) {
        ThreadUtils.assertOnUiThread();
        assert !card.getIsLocal();
        PersonalDataManagerJni.get().addServerCreditCardForTest(
                mPersonalDataManagerAndroid, PersonalDataManager.this, card);
    }

    public void deleteCreditCard(String guid) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().removeByGUID(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    public void clearUnmaskedCache(String guid) {
        PersonalDataManagerJni.get().clearUnmaskedCache(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    public String getShippingAddressLabelWithCountryForPaymentRequest(AutofillProfile profile) {
        return PersonalDataManagerJni.get().getShippingAddressLabelWithCountryForPaymentRequest(
                mPersonalDataManagerAndroid, PersonalDataManager.this, profile);
    }

    public String getShippingAddressLabelWithoutCountryForPaymentRequest(AutofillProfile profile) {
        return PersonalDataManagerJni.get().getShippingAddressLabelWithoutCountryForPaymentRequest(
                mPersonalDataManagerAndroid, PersonalDataManager.this, profile);
    }

    public String getBillingAddressLabelForPaymentRequest(AutofillProfile profile) {
        return PersonalDataManagerJni.get().getBillingAddressLabelForPaymentRequest(
                mPersonalDataManagerAndroid, PersonalDataManager.this, profile);
    }

    public void getFullCard(WebContents webContents, CreditCard card,
            FullCardRequestDelegate delegate) {
        PersonalDataManagerJni.get().getFullCardForPaymentRequest(
                mPersonalDataManagerAndroid, PersonalDataManager.this, webContents, card, delegate);
    }

    /**
     * Records the use of the profile associated with the specified {@code guid}. Effectively
     * increments the use count of the profile and sets its use date to the current time. Also logs
     * usage metrics.
     *
     * @param guid The GUID of the profile.
     */
    public void recordAndLogProfileUse(String guid) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().recordAndLogProfileUse(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    @VisibleForTesting
    protected void setProfileUseStatsForTesting(String guid, int count, long date) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().setProfileUseStatsForTesting(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid, count, date);
    }

    @VisibleForTesting
    int getProfileUseCountForTesting(String guid) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getProfileUseCountForTesting(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    @VisibleForTesting
    long getProfileUseDateForTesting(String guid) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getProfileUseDateForTesting(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    /**
     * Records the use of the credit card associated with the specified {@code guid}. Effectively
     * increments the use count of the credit card and set its use date to the current time. Also
     * logs usage metrics.
     *
     * @param guid The GUID of the credit card.
     */
    public void recordAndLogCreditCardUse(String guid) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().recordAndLogCreditCardUse(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    @VisibleForTesting
    protected void setCreditCardUseStatsForTesting(String guid, int count, long date) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().setCreditCardUseStatsForTesting(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid, count, date);
    }

    @VisibleForTesting
    int getCreditCardUseCountForTesting(String guid) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getCreditCardUseCountForTesting(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    @VisibleForTesting
    long getCreditCardUseDateForTesting(String guid) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getCreditCardUseDateForTesting(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    @VisibleForTesting
    long getCurrentDateForTesting() {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getCurrentDateForTesting(
                mPersonalDataManagerAndroid, PersonalDataManager.this);
    }

    /**
     * Starts loading the address validation rules for the specified {@code regionCode}.
     *
     * @param regionCode The code of the region for which to load the rules.
     */
    public void loadRulesForAddressNormalization(String regionCode) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().loadRulesForAddressNormalization(
                mPersonalDataManagerAndroid, PersonalDataManager.this, regionCode);
    }

    /**
     * Starts loading the sub-key request rules for the specified {@code regionCode}.
     *
     * @param regionCode The code of the region for which to load the rules.
     */
    public void loadRulesForSubKeys(String regionCode) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().loadRulesForSubKeys(
                mPersonalDataManagerAndroid, PersonalDataManager.this, regionCode);
    }

    /**
     * Starts requesting the subkeys for the specified {@code regionCode}, if the rules
     * associated with the {@code regionCode} are done loading. Otherwise sets up the callback to
     * start loading the subkeys when the rules are loaded. The received subkeys will be sent
     * to the {@code delegate}. If the subkeys are not received in the specified
     * {@code sRequestTimeoutSeconds}, the {@code delegate} will be notified.
     *
     * @param regionCode The code of the region for which to load the subkeys.
     * @param delegate The object requesting the subkeys.
     */
    public void getRegionSubKeys(String regionCode, GetSubKeysRequestDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().startRegionSubKeysRequest(mPersonalDataManagerAndroid,
                PersonalDataManager.this, regionCode, sRequestTimeoutSeconds, delegate);
    }

    /** Cancels the pending subkeys request. */
    public void cancelPendingGetSubKeys() {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().cancelPendingGetSubKeys(mPersonalDataManagerAndroid);
    }

    /**
     * Normalizes the address of the profile associated with the {@code guid} if the rules
     * associated with the profile's region are done loading. Otherwise sets up the callback to
     * start normalizing the address when the rules are loaded. The normalized profile will be sent
     * to the {@code delegate}. If the profile is not normalized in the specified
     * {@code sRequestTimeoutSeconds}, the {@code delegate} will be notified.
     *
     * @param profile The profile to normalize.
     * @param delegate The object requesting the normalization.
     */
    public void normalizeAddress(
            AutofillProfile profile, NormalizedAddressRequestDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().startAddressNormalization(mPersonalDataManagerAndroid,
                PersonalDataManager.this, profile, sRequestTimeoutSeconds, delegate);
    }

    /**
     * Checks whether the Autofill PersonalDataManager has profiles.
     *
     * @return True If there are profiles.
     */
    public boolean hasProfiles() {
        return PersonalDataManagerJni.get().hasProfiles(mPersonalDataManagerAndroid);
    }

    /**
     * Checks whether the Autofill PersonalDataManager has credit cards.
     *
     * @return True If there are credit cards.
     */
    public boolean hasCreditCards() {
        return PersonalDataManagerJni.get().hasCreditCards(mPersonalDataManagerAndroid);
    }

    /**
     * @return Whether the Autofill feature for Profiles (addresses) is enabled.
     */
    public static boolean isAutofillProfileEnabled() {
        return PersonalDataManagerJni.get().getPref(Pref.AUTOFILL_PROFILE_ENABLED);
    }

    /**
     * @return Whether the Autofill feature for Credit Cards is enabled.
     */
    public static boolean isAutofillCreditCardEnabled() {
        return PersonalDataManagerJni.get().getPref(Pref.AUTOFILL_CREDIT_CARD_ENABLED);
    }

    /**
     * Enables or disables the Autofill feature for Profiles.
     * @param enable True to disable profile Autofill, false otherwise.
     */
    public static void setAutofillProfileEnabled(boolean enable) {
        PersonalDataManagerJni.get().setPref(Pref.AUTOFILL_PROFILE_ENABLED, enable);
    }

    /**
     * Enables or disables the Autofill feature for Credit Cards.
     * @param enable True to disable credit card Autofill, false otherwise.
     */
    public static void setAutofillCreditCardEnabled(boolean enable) {
        PersonalDataManagerJni.get().setPref(Pref.AUTOFILL_CREDIT_CARD_ENABLED, enable);
    }

    /**
     * @return Whether the Autofill feature is managed.
     */
    public static boolean isAutofillManaged() {
        return PersonalDataManagerJni.get().isAutofillManaged();
    }

    /**
     * @return Whether the Autofill feature for Profiles (addresses) is managed.
     */
    public static boolean isAutofillProfileManaged() {
        return PersonalDataManagerJni.get().isAutofillProfileManaged();
    }

    /**
     * @return Whether the Autofill feature for Credit Cards is managed.
     */
    public static boolean isAutofillCreditCardManaged() {
        return PersonalDataManagerJni.get().isAutofillCreditCardManaged();
    }

    /**
     * @return Whether the Payments integration feature is enabled.
     */
    public static boolean isPaymentsIntegrationEnabled() {
        return PersonalDataManagerJni.get().isPaymentsIntegrationEnabled();
    }

    /**
     * Enables or disables the Payments integration.
     * @param enable True to enable Payments data import.
     */
    public static void setPaymentsIntegrationEnabled(boolean enable) {
        PersonalDataManagerJni.get().setPaymentsIntegrationEnabled(enable);
    }

    @VisibleForTesting
    public static void setRequestTimeoutForTesting(int timeout) {
        sRequestTimeoutSeconds = timeout;
    }

    @VisibleForTesting
    public void setSyncServiceForTesting() {
        PersonalDataManagerJni.get().setSyncServiceForTesting(mPersonalDataManagerAndroid);
    }

    /**
     * @return The sub-key request timeout in milliseconds.
     */
    public static long getRequestTimeoutMS() {
        return DateUtils.SECOND_IN_MILLIS * sRequestTimeoutSeconds;
    }

    @NativeMethods
    interface Natives {
        long init(PersonalDataManager caller);
        boolean isDataLoaded(long nativePersonalDataManagerAndroid, PersonalDataManager caller);
        String[] getProfileGUIDsForSettings(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller);
        String[] getProfileGUIDsToSuggest(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller);
        String[] getProfileLabelsForSettings(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller);
        String[] getProfileLabelsToSuggest(long nativePersonalDataManagerAndroid,
                PersonalDataManager caller, boolean includeNameInLabel,
                boolean includeOrganizationInLabel, boolean includeCountryInLabel);
        AutofillProfile getProfileByGUID(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, String guid);
        String setProfile(long nativePersonalDataManagerAndroid, PersonalDataManager caller,
                AutofillProfile profile);
        String setProfileToLocal(long nativePersonalDataManagerAndroid, PersonalDataManager caller,
                AutofillProfile profile);
        String getShippingAddressLabelWithCountryForPaymentRequest(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller,
                AutofillProfile profile);
        String getShippingAddressLabelWithoutCountryForPaymentRequest(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller,
                AutofillProfile profile);
        String getBillingAddressLabelForPaymentRequest(long nativePersonalDataManagerAndroid,
                PersonalDataManager caller, AutofillProfile profile);
        String[] getCreditCardGUIDsForSettings(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller);
        String[] getCreditCardGUIDsToSuggest(long nativePersonalDataManagerAndroid,
                PersonalDataManager caller, boolean includeServerCards);
        CreditCard getCreditCardByGUID(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, String guid);
        CreditCard getCreditCardForNumber(long nativePersonalDataManagerAndroid,
                PersonalDataManager caller, String cardNumber);
        String setCreditCard(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, CreditCard card);
        void updateServerCardBillingAddress(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, CreditCard card);
        String getBasicCardIssuerNetwork(long nativePersonalDataManagerAndroid,
                PersonalDataManager caller, String cardNumber, boolean emptyIfInvalid);
        void addServerCreditCardForTest(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, CreditCard card);
        void removeByGUID(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, String guid);
        void recordAndLogProfileUse(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, String guid);
        void setProfileUseStatsForTesting(long nativePersonalDataManagerAndroid,
                PersonalDataManager caller, String guid, int count, long date);
        int getProfileUseCountForTesting(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, String guid);
        long getProfileUseDateForTesting(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, String guid);
        void recordAndLogCreditCardUse(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, String guid);
        void setCreditCardUseStatsForTesting(long nativePersonalDataManagerAndroid,
                PersonalDataManager caller, String guid, int count, long date);
        int getCreditCardUseCountForTesting(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, String guid);
        long getCreditCardUseDateForTesting(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, String guid);
        long getCurrentDateForTesting(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller);
        void clearUnmaskedCache(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, String guid);
        void getFullCardForPaymentRequest(long nativePersonalDataManagerAndroid,
                PersonalDataManager caller, WebContents webContents, CreditCard card,
                FullCardRequestDelegate delegate);
        void loadRulesForAddressNormalization(long nativePersonalDataManagerAndroid,
                PersonalDataManager caller, String regionCode);
        void loadRulesForSubKeys(long nativePersonalDataManagerAndroid, PersonalDataManager caller,
                String regionCode);
        void startAddressNormalization(long nativePersonalDataManagerAndroid,
                PersonalDataManager caller, AutofillProfile profile, int timeoutSeconds,
                NormalizedAddressRequestDelegate delegate);
        void startRegionSubKeysRequest(long nativePersonalDataManagerAndroid,
                PersonalDataManager caller, String regionCode, int timeoutSeconds,
                GetSubKeysRequestDelegate delegate);
        boolean hasProfiles(long nativePersonalDataManagerAndroid);
        boolean hasCreditCards(long nativePersonalDataManagerAndroid);
        boolean isAutofillManaged();
        boolean isAutofillProfileManaged();
        boolean isAutofillCreditCardManaged();
        boolean isPaymentsIntegrationEnabled();
        void setPaymentsIntegrationEnabled(boolean enable);
        String toCountryCode(String countryName);
        void cancelPendingGetSubKeys(long nativePersonalDataManagerAndroid);
        void setSyncServiceForTesting(long nativePersonalDataManagerAndroid);
        boolean getPref(int preference);
        void setPref(int preference, boolean enable);
    }
}
