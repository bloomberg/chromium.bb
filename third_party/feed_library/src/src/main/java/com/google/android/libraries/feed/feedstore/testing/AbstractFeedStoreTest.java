// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedstore.testing;

import static com.google.common.truth.Truth.assertThat;

import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.internal.common.PayloadWithId;
import com.google.android.libraries.feed.api.internal.common.SemanticPropertiesWithId;
import com.google.android.libraries.feed.api.internal.common.testing.ContentIdGenerators;
import com.google.android.libraries.feed.api.internal.store.ContentMutation;
import com.google.android.libraries.feed.api.internal.store.LocalActionMutation.ActionType;
import com.google.android.libraries.feed.api.internal.store.SessionMutation;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.testing.FakeMainThreadRunner;
import com.google.android.libraries.feed.common.testing.RunnableSubject;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.protobuf.ByteString;
import com.google.search.now.feed.client.StreamDataProto.StreamDataOperation;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;
import com.google.search.now.feed.client.StreamDataProto.StreamLocalAction;
import com.google.search.now.feed.client.StreamDataProto.StreamPayload;
import com.google.search.now.feed.client.StreamDataProto.StreamPayload.Builder;
import com.google.search.now.feed.client.StreamDataProto.StreamSharedState;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure.Operation;

import org.junit.Test;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Tests of the {@link FeedStore} classes. */
public abstract class AbstractFeedStoreTest {
    protected final FakeClock fakeClock = new FakeClock();
    protected final TimingUtils timingUtils = new TimingUtils();

    private static final long START_TIME = 50;
    private static final long START_TIME_MILLIS = TimeUnit.SECONDS.toMillis(START_TIME);
    private static final long THREE_DAYS_AFTER_START_TIME = START_TIME + TimeUnit.DAYS.toSeconds(3);
    private static final long THREE_DAYS_AFTER_START_TIME_MILLIS =
            TimeUnit.SECONDS.toMillis(THREE_DAYS_AFTER_START_TIME);

    private static final ContentIdGenerators idGenerators = new ContentIdGenerators();
    private static final int PAYLOAD_ID = 12345;
    private static final int OPERATION_ID = 67890;
    private static final String PAYLOAD_CONTENT_ID =
            idGenerators.createFeatureContentId(PAYLOAD_ID);
    private static final String OPERATION_CONTENT_ID =
            idGenerators.createFeatureContentId(OPERATION_ID);
    private static final Builder STREAM_PAYLOAD = StreamPayload.newBuilder().setStreamFeature(
            StreamFeature.newBuilder()
                    .setContentId(PAYLOAD_CONTENT_ID)
                    .setParentId(idGenerators.createRootContentId(0)));
    private static final StreamStructure STREAM_STRUCTURE =
            StreamStructure.newBuilder()
                    .setContentId(OPERATION_CONTENT_ID)
                    .setParentContentId(idGenerators.createRootContentId(0))
                    .setOperation(Operation.UPDATE_OR_APPEND)
                    .build();
    private static final StreamDataOperation STREAM_DATA_OPERATION =
            StreamDataOperation.newBuilder()
                    .setStreamStructure(STREAM_STRUCTURE)
                    .setStreamPayload(STREAM_PAYLOAD)
                    .build();
    private final MainThreadRunner mainThreadRunner = FakeMainThreadRunner.runTasksImmediately();

    /**
     * Provides an instance of the store
     *
     * @param mainThreadRunner
     */
    protected abstract Store getStore(MainThreadRunner mainThreadRunner);

    @Test
    public void testMinimalStore() {
        Store store = getStore(mainThreadRunner);
        Result<List<String>> result = store.getAllSessions();
        assertThat(result.isSuccessful()).isTrue();
        assertThat(result.getValue()).isEmpty();
    }

    @Test
    public void testContentMutation() {
        Store store = getStore(mainThreadRunner);
        ContentMutation contentMutation = store.editContent();
        assertThat(contentMutation).isNotNull();
    }

    @Test
    public void addStructureOperationToSession() {
        Store store = getStore(mainThreadRunner);
        SessionMutation mutation = store.editSession(Store.HEAD_SESSION_ID);
        mutation.add(STREAM_DATA_OPERATION.getStreamStructure());
        mutation.commit();

        Result<List<StreamStructure>> streamStructuresResult =
                store.getStreamStructures(Store.HEAD_SESSION_ID);

        assertThat(streamStructuresResult.isSuccessful()).isTrue();
        List<StreamStructure> streamStructures = streamStructuresResult.getValue();
        assertThat(streamStructures).hasSize(1);
        assertThat(streamStructures.get(0).getContentId()).isEqualTo(OPERATION_CONTENT_ID);
    }

    @Test
    public void addContentOperationToSession() {
        Store store = getStore(mainThreadRunner);
        ContentMutation mutation = store.editContent();
        mutation.add(PAYLOAD_CONTENT_ID, STREAM_DATA_OPERATION.getStreamPayload());
        CommitResult result = mutation.commit();

        assertThat(result).isEqualTo(CommitResult.SUCCESS);
    }

    @Test
    public void createNewSession() {
        Store store = getStore(mainThreadRunner);
        SessionMutation mutation = store.editSession(Store.HEAD_SESSION_ID);
        mutation.add(STREAM_STRUCTURE);
        mutation.commit();

        Result<String> sessionResult = store.createNewSession();
        assertThat(sessionResult.isSuccessful()).isTrue();
        String sessionId = sessionResult.getValue();

        Result<List<StreamStructure>> streamStructuresResult = store.getStreamStructures(sessionId);

        assertThat(streamStructuresResult.isSuccessful()).isTrue();
        List<StreamStructure> streamStructures = streamStructuresResult.getValue();
        assertThat(streamStructures).hasSize(1);
        StreamStructure streamStructure = streamStructures.get(0);
        assertThat(streamStructure.getContentId()).contains(Integer.toString(OPERATION_ID));
    }

    @Test
    public void removeSession() {
        Store store = getStore(mainThreadRunner);
        SessionMutation mutation = store.editSession(Store.HEAD_SESSION_ID);
        mutation.add(STREAM_STRUCTURE);
        mutation.commit();

        Result<String> sessionResult = store.createNewSession();
        assertThat(sessionResult.isSuccessful()).isTrue();
        String sessionId = sessionResult.getValue();

        store.removeSession(sessionId);

        Result<List<StreamStructure>> streamStructuresResult = store.getStreamStructures(sessionId);

        assertThat(streamStructuresResult.isSuccessful()).isTrue();
        assertThat(streamStructuresResult.getValue()).isEmpty();
    }

    @Test
    public void clearHead() {
        Store store = getStore(mainThreadRunner);
        SessionMutation mutation = store.editSession(Store.HEAD_SESSION_ID);
        mutation.add(STREAM_STRUCTURE);
        mutation.commit();

        store.clearHead();

        Result<List<StreamStructure>> streamStructuresResult =
                store.getStreamStructures(Store.HEAD_SESSION_ID);

        assertThat(streamStructuresResult.isSuccessful()).isTrue();
        assertThat(streamStructuresResult.getValue()).isEmpty();
    }

    @Test
    public void getSessions() {
        Store store = getStore(mainThreadRunner);
        SessionMutation mutation = store.editSession(Store.HEAD_SESSION_ID);
        mutation.add(STREAM_STRUCTURE);
        mutation.commit();

        Result<List<String>> sessionsResult = store.getAllSessions();
        assertThat(sessionsResult.isSuccessful()).isTrue();
        List<String> sessions = sessionsResult.getValue();
        assertThat(sessions).isEmpty();

        Result<String> sessionResult = store.createNewSession();
        assertThat(sessionResult.isSuccessful()).isTrue();
        String sessionId = sessionResult.getValue();

        sessionsResult = store.getAllSessions();
        assertThat(sessionsResult.isSuccessful()).isTrue();
        sessions = sessionsResult.getValue();
        assertThat(sessions).hasSize(1);
        assertThat(sessions.get(0)).isEqualTo(sessionId);

        sessionResult = store.createNewSession();
        assertThat(sessionResult.isSuccessful()).isTrue();

        sessionsResult = store.getAllSessions();
        assertThat(sessionsResult.isSuccessful()).isTrue();
        sessions = sessionsResult.getValue();
        assertThat(sessions).hasSize(2);
    }

    @Test
    public void editContent() {
        StreamPayload streamPayload =
                StreamPayload.newBuilder()
                        .setStreamFeature(
                                StreamFeature.newBuilder().setContentId(PAYLOAD_CONTENT_ID))
                        .build();
        Store store = getStore(mainThreadRunner);

        CommitResult commitResult =
                store.editContent().add(PAYLOAD_CONTENT_ID, streamPayload).commit();
        assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

        Result<List<PayloadWithId>> payloadsResult =
                store.getPayloads(Collections.singletonList(PAYLOAD_CONTENT_ID));
        assertThat(payloadsResult.isSuccessful()).isTrue();
        assertThat(payloadsResult.getValue()).hasSize(1);
        assertThat(payloadsResult.getValue().get(0).contentId).isEqualTo(PAYLOAD_CONTENT_ID);
        assertThat(payloadsResult.getValue().get(0).payload).isEqualTo(streamPayload);
    }

    @Test
    public void getSharedStates() {
        StreamSharedState streamSharedState =
                StreamSharedState.newBuilder().setContentId(PAYLOAD_CONTENT_ID).build();
        Store store = getStore(mainThreadRunner);
        store.editContent()
                .add(String.valueOf(PAYLOAD_ID),
                        StreamPayload.newBuilder().setStreamSharedState(streamSharedState).build())
                .commit();
        Result<List<StreamSharedState>> sharedStatesResult = store.getSharedStates();
        assertThat(sharedStatesResult.isSuccessful()).isTrue();
        List<StreamSharedState> sharedStates = sharedStatesResult.getValue();
        assertThat(sharedStates).hasSize(1);
        assertThat(sharedStates.get(0)).isEqualTo(streamSharedState);
    }

    @Test
    public void getPayloads_noPayload() {
        List<String> contentIds = new ArrayList<>();
        contentIds.add(PAYLOAD_CONTENT_ID);

        Store store = getStore(mainThreadRunner);
        Result<List<PayloadWithId>> payloadsResult = store.getPayloads(contentIds);
        assertThat(payloadsResult.isSuccessful()).isTrue();
        List<PayloadWithId> payloads = payloadsResult.getValue();
        assertThat(payloads).isEmpty();
    }

    @Test
    public void deleteHead_notAllowed() {
        RunnableSubject
                .assertThatRunnable(() -> {
                    Store store = getStore(mainThreadRunner);
                    store.removeSession(Store.HEAD_SESSION_ID);
                })
                .throwsAnExceptionOfType(IllegalStateException.class);
    }

    @Test
    public void editSemanticProperties() {
        Store store = getStore(mainThreadRunner);
        assertThat(store.editSemanticProperties()).isNotNull();
    }

    @Test
    public void getSemanticProperties() {
        ByteString semanticData = ByteString.copyFromUtf8("helloWorld");
        Store store = getStore(mainThreadRunner);
        store.editSemanticProperties().add(PAYLOAD_CONTENT_ID, semanticData).commit();
        Result<List<SemanticPropertiesWithId>> semanticPropertiesResult =
                store.getSemanticProperties(Collections.singletonList(PAYLOAD_CONTENT_ID));
        assertThat(semanticPropertiesResult.isSuccessful()).isTrue();
        List<SemanticPropertiesWithId> semanticProperties = semanticPropertiesResult.getValue();
        assertThat(semanticProperties).hasSize(1);
        assertThat(semanticProperties.get(0).contentId).isEqualTo(PAYLOAD_CONTENT_ID);
        assertThat(semanticProperties.get(0).semanticData).isEqualTo(semanticData.toByteArray());
    }

    @Test
    public void getSemanticProperties_requestDifferentKey() {
        ByteString semanticData = ByteString.copyFromUtf8("helloWorld");
        Store store = getStore(mainThreadRunner);
        store.editSemanticProperties().add(PAYLOAD_CONTENT_ID, semanticData).commit();
        Result<List<SemanticPropertiesWithId>> semanticPropertiesResult =
                store.getSemanticProperties(Collections.singletonList(OPERATION_CONTENT_ID));
        assertThat(semanticPropertiesResult.isSuccessful()).isTrue();
        List<SemanticPropertiesWithId> semanticProperties = semanticPropertiesResult.getValue();
        assertThat(semanticProperties).isEmpty();
    }

    @Test
    public void getSemanticProperties_doesNotExist() {
        Store store = getStore(mainThreadRunner);
        Result<List<SemanticPropertiesWithId>> semanticPropertiesResult =
                store.getSemanticProperties(Collections.singletonList(PAYLOAD_CONTENT_ID));
        assertThat(semanticPropertiesResult.isSuccessful()).isTrue();
        List<SemanticPropertiesWithId> semanticProperties = semanticPropertiesResult.getValue();
        assertThat(semanticProperties).isEmpty();
    }

    @Test
    public void getDismissActions() {
        fakeClock.set(START_TIME_MILLIS);
        Store store = getStore(mainThreadRunner);
        store.editLocalActions().add(ActionType.DISMISS, OPERATION_CONTENT_ID).commit();
        Result<List<StreamLocalAction>> dismissActionsResult = store.getAllDismissLocalActions();
        assertThat(dismissActionsResult.isSuccessful()).isTrue();
        List<StreamLocalAction> dismissActions = dismissActionsResult.getValue();
        assertThat(dismissActions).isNotEmpty();
        assertThat(dismissActions.get(0).getAction()).isEqualTo(ActionType.DISMISS);
        assertThat(dismissActions.get(0).getFeatureContentId()).isEqualTo(OPERATION_CONTENT_ID);
        assertThat(dismissActions.get(0).getTimestampSeconds()).isEqualTo(START_TIME);
    }

    @Test
    public void getDismissActions_notIncludedInSessions() {
        fakeClock.set(START_TIME_MILLIS);
        Store store = getStore(mainThreadRunner);
        store.editLocalActions().add(ActionType.DISMISS, OPERATION_CONTENT_ID).commit();
        Result<List<String>> allSessionsResult = store.getAllSessions();
        assertThat(allSessionsResult.isSuccessful()).isTrue();
        List<String> allSessions = allSessionsResult.getValue();
        assertThat(allSessions).isEmpty();
    }

    @Test
    public void getDismissActions_multipleDismisses() {
        fakeClock.set(START_TIME_MILLIS);
        Store store = getStore(mainThreadRunner);
        store.editLocalActions()
                .add(ActionType.DISMISS, OPERATION_CONTENT_ID)
                .add(ActionType.DISMISS, PAYLOAD_CONTENT_ID)
                .commit();
        Result<List<StreamLocalAction>> dismissActionsResult = store.getAllDismissLocalActions();
        assertThat(dismissActionsResult.isSuccessful()).isTrue();
        List<StreamLocalAction> dismissActions = dismissActionsResult.getValue();
        assertThat(dismissActions).isNotEmpty();
        assertThat(dismissActions.get(0).getAction()).isEqualTo(ActionType.DISMISS);
        assertThat(dismissActions.get(0).getFeatureContentId()).isEqualTo(OPERATION_CONTENT_ID);
        assertThat(dismissActions.get(0).getTimestampSeconds()).isEqualTo(START_TIME);
        assertThat(dismissActions.get(1).getAction()).isEqualTo(ActionType.DISMISS);
        assertThat(dismissActions.get(1).getFeatureContentId()).isEqualTo(PAYLOAD_CONTENT_ID);
        assertThat(dismissActions.get(1).getTimestampSeconds()).isEqualTo(START_TIME);
    }

    @Test
    public void getDismissActions_expired() {
        fakeClock.set(START_TIME_MILLIS);
        Store store = getStore(mainThreadRunner);
        store.editLocalActions().add(ActionType.DISMISS, OPERATION_CONTENT_ID).commit();
        fakeClock.set(THREE_DAYS_AFTER_START_TIME_MILLIS);
        Result<List<StreamLocalAction>> dismissActionsResult = store.getAllDismissLocalActions();
        assertThat(dismissActionsResult.isSuccessful()).isTrue();
        List<StreamLocalAction> dismissActions = dismissActionsResult.getValue();

        // Should still return expired actions.
        assertThat(dismissActions).isNotEmpty();
        assertThat(dismissActions.get(0).getAction()).isEqualTo(ActionType.DISMISS);
        assertThat(dismissActions.get(0).getFeatureContentId()).isEqualTo(OPERATION_CONTENT_ID);
        assertThat(dismissActions.get(0).getTimestampSeconds()).isEqualTo(START_TIME);
    }
}
