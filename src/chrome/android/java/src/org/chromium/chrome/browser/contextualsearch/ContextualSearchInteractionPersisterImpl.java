// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import org.chromium.chrome.browser.contextualsearch.ContextualSearchInteractionRecorder.Feature;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * Implements persisting an interaction event and outcomes to local storage.
 * Call {@link #persistInteractions} to persist a pair of values given ID and outcomes.
 * Call the static getAndResetPersistedInteractionOutcomes to get the previous persisted pair
 * and reset the storage (so we can't get the same value twice).
 * A pair with an Event ID of {@link ContextualSearchInteractionPersister#NO_EVENT_ID} represents no
 * persisted value.
 */
class ContextualSearchInteractionPersisterImpl implements ContextualSearchInteractionPersister {
    static final int BITMASK_PANEL_OPENED = 1 << 0;
    static final int BITMASK_QUICK_ACTION_CLICKED = 1 << 1;
    static final int BITMASK_QUICK_ANSWER_SEEN = 1 << 2;
    static final int BITMASK_CARDS_DATA_SHOWN = 1 << 3;
    static final Map<Integer, Integer> OUTCOME_ENCODING_BIT_MAP;
    static {
        // Map keys should contain @Feature int values only.
        // Map values should be int powers of 2 only.
        Map<Integer, Integer> outcome_encoding_bit_map = new HashMap<Integer, Integer>();
        outcome_encoding_bit_map.put(Feature.OUTCOME_WAS_PANEL_OPENED, BITMASK_PANEL_OPENED);
        outcome_encoding_bit_map.put(
                Feature.OUTCOME_WAS_QUICK_ACTION_CLICKED, BITMASK_QUICK_ACTION_CLICKED);
        outcome_encoding_bit_map.put(
                Feature.OUTCOME_WAS_QUICK_ANSWER_SEEN, BITMASK_QUICK_ANSWER_SEEN);
        outcome_encoding_bit_map.put(
                Feature.OUTCOME_WAS_CARDS_DATA_SHOWN, BITMASK_CARDS_DATA_SHOWN);
        OUTCOME_ENCODING_BIT_MAP = Collections.unmodifiableMap(outcome_encoding_bit_map);
    }

    @Override
    public PersistedInteraction getAndClearPersistedInteraction() {
        long previousEventId = readEventIdFromPersistantStorage();
        if (previousEventId == NO_EVENT_ID) return new PersistedInteractionImpl();

        int encodedInteractionOutcomes = readOutcomesFromPersistantStorage();
        long timestamp = readTimestampFromPersistantStorage();
        assert timestamp != 0;
        ContextualSearchUma.logOutcomesTimestamp(System.currentTimeMillis() - timestamp);
        writeEventIDToPersistantStorage(NO_EVENT_ID);
        writeOutcomesToPersistantStorage(NO_INTERACTION);
        writeTimestampToPersistantStorage(0);
        return new PersistedInteractionImpl(previousEventId, encodedInteractionOutcomes, timestamp);
    }

    @Override
    public void persistInteractions(long eventId, Map<Integer, Object> outcomesMap) {
        assert eventId != NO_EVENT_ID;
        assert outcomesMap != null;
        int encodedInteractionResults = NO_INTERACTION;
        // Only the outcomes will be present, since we logged inference features at
        // inference time.
        for (Map.Entry<Integer, Object> entry : outcomesMap.entrySet()) {
            // Bit-wise encode into an int with all the boolean outcomes.
            if ((boolean) entry.getValue()) {
                encodedInteractionResults |= OUTCOME_ENCODING_BIT_MAP.get(entry.getKey());
            }
        }
        writeOutcomesToPersistantStorage(encodedInteractionResults);
        writeEventIDToPersistantStorage(eventId);
        writeTimestampToPersistantStorage(System.currentTimeMillis());
    }

    /** @param eventId An event ID to write to local storage. */
    private void writeEventIDToPersistantStorage(long eventId) {
        ChromePreferenceManager.getInstance().writeLong(
                ChromePreferenceManager.CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_EVENT_ID, eventId);
    }

    /** @return The event ID from local storage. */
    private long readEventIdFromPersistantStorage() {
        return ChromePreferenceManager.getInstance().readLong(
                ChromePreferenceManager.CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_EVENT_ID,
                NO_EVENT_ID);
    }

    /** @param bitEncodedValue An encoded outcome to write to local storage. */
    private void writeOutcomesToPersistantStorage(int bitEncodedValue) {
        ChromePreferenceManager.getInstance().writeInt(
                ChromePreferenceManager.CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_ENCODED_OUTCOMES,
                bitEncodedValue);
    }

    /** @return The encoded outcome from local storage. */
    private int readOutcomesFromPersistantStorage() {
        return ChromePreferenceManager.getInstance().readInt(
                ChromePreferenceManager.CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_ENCODED_OUTCOMES);
    }

    /** Writes the current time stamp to local storage. */
    private void writeTimestampToPersistantStorage(long timestamp) {
        ChromePreferenceManager.getInstance().writeLong(
                ChromePreferenceManager.CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_TIMESTAMP,
                timestamp);
    }

    /** @return The time stamp when we wrote the outcome to local storage. */
    private long readTimestampFromPersistantStorage() {
        return ChromePreferenceManager.getInstance().readLong(
                ChromePreferenceManager.CONTEXTUAL_SEARCH_PREVIOUS_INTERACTION_TIMESTAMP, 0);
    }

    /**
     * Provides a read-only encapsulation of a persisted user interaction and the ID from the
     * server used to identify it, and a timestamp.
     */
    static class PersistedInteractionImpl implements PersistedInteraction {
        /**
         * The identifier from the server for this interaction outcome, or
         * {@link ContextualSearchInteractionPersister#NO_EVENT_ID} if none.
         */
        private final long mEventId;

        /**
         * The encoded results of the user interaction, often
         * {@link ContextualSearchInteractionPersister#NO_INTERACTION} meaning no interaction.
         */
        private final int mEncodedUserInteractions;

        /**
         * The time stamp of the user interaction, in milliseconds, or 0 if not known.
         */
        private final long mTimestampMs;

        /** Creates an empty interaction. */
        PersistedInteractionImpl() {
            this(NO_EVENT_ID, NO_INTERACTION, 0);
        }

        /** Creates an interaction with the given EventID and encoded user interactions. */
        PersistedInteractionImpl(long eventId, int encodedUserInteractions, long timestamp) {
            mEventId = eventId;
            mEncodedUserInteractions = encodedUserInteractions;
            mTimestampMs = timestamp;
        }

        @Override
        public long getEventId() {
            return mEventId;
        }

        @Override
        public int getEncodedUserInteractions() {
            return mEncodedUserInteractions;
        }

        @Override
        public long getTimestampMs() {
            return mTimestampMs;
        }
    }
}
