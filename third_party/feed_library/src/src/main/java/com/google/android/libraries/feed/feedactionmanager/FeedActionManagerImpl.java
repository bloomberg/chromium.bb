// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedactionmanager;

import android.net.Uri;
import android.util.Base64;
import com.google.android.libraries.feed.api.common.MutationContext;
import com.google.android.libraries.feed.api.host.logging.Task;
import com.google.android.libraries.feed.api.internal.actionmanager.ActionManager;
import com.google.android.libraries.feed.api.internal.common.Model;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.api.internal.store.LocalActionMutation;
import com.google.android.libraries.feed.api.internal.store.LocalActionMutation.ActionType;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.concurrent.TaskQueue.TaskType;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.search.now.feed.client.StreamDataProto.StreamDataOperation;
import com.google.search.now.feed.client.StreamDataProto.StreamUploadableAction;
import com.google.search.now.wire.feed.ActionPayloadProto.ActionPayload;
import java.util.HashSet;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Default implementation of {@link ActionManager} */
public class FeedActionManagerImpl implements ActionManager {

  private final FeedSessionManager feedSessionManager;
  private final Store store;
  private final ThreadUtils threadUtils;
  private final TaskQueue taskQueue;
  private final MainThreadRunner mainThreadRunner;
  private final Clock clock;

  public FeedActionManagerImpl(
      FeedSessionManager feedSessionManager,
      Store store,
      ThreadUtils threadUtils,
      TaskQueue taskQueue,
      MainThreadRunner mainThreadRunner,
      Clock clock) {
    this.feedSessionManager = feedSessionManager;
    this.store = store;
    this.threadUtils = threadUtils;
    this.taskQueue = taskQueue;
    this.mainThreadRunner = mainThreadRunner;
    this.clock = clock;
  }

  @Override
  public void dismissLocal(
      List<String> contentIds,
      List<StreamDataOperation> streamDataOperations,
      /*@Nullable*/ String sessionId) {
    executeStreamDataOperations(streamDataOperations, sessionId);
    // Store the dismissLocal actions
    taskQueue.execute(
        Task.DISMISS_LOCAL,
        TaskType.BACKGROUND,
        () -> {
          LocalActionMutation localActionMutation = store.editLocalActions();
          for (String contentId : contentIds) {
            localActionMutation.add(ActionType.DISMISS, contentId);
          }
          localActionMutation.commit();
        });
  }

  @Override
  public void dismiss(List<StreamDataOperation> streamDataOperations, /*@Nullable*/ String sessionId) {
    executeStreamDataOperations(streamDataOperations, sessionId);
  }

  @Override
  public void createAndUploadAction(String contentId, ActionPayload payload) {
    taskQueue.execute(
        Task.CREATE_AND_UPLOAD,
        TaskType.BACKGROUND,
        () -> {
          HashSet<StreamUploadableAction> actionSet = new HashSet<>();
          long currentTime = TimeUnit.MILLISECONDS.toSeconds(clock.currentTimeMillis());
          actionSet.add(
              StreamUploadableAction.newBuilder()
                  .setFeatureContentId(contentId)
                  .setPayload(payload)
                  .setTimestampSeconds(currentTime)
                  .build());
          feedSessionManager.triggerUploadActions(actionSet);
        });
  }

  @Override
  public void uploadAllActionsAndUpdateUrl(
      String url, String consistencyTokenQueryParamName, Consumer<String> consumer) {
    taskQueue.execute(
        Task.UPLOAD_ALL_ACTIONS_FOR_URL,
        TaskType.BACKGROUND,
        () -> {
          // TODO: figure out spinner and/or timeout conditions
          feedSessionManager.fetchActionsAndUpload(
              result -> {
                mainThreadRunner.execute(
                    "Open url",
                    () -> {
                      if (result.isSuccessful()) {
                        consumer.accept(
                            updateParam(
                                url,
                                consistencyTokenQueryParamName,
                                result.getValue().toByteArray()));
                      } else {
                        consumer.accept(url);
                      }
                    });
              });
        });
  }

  static String updateParam(String url, String consistencyTokenQueryParamName, byte[] value) {
    Uri.Builder uriBuilder = Uri.parse(url).buildUpon();
    uriBuilder.appendQueryParameter(
        consistencyTokenQueryParamName,
        Base64.encodeToString(value, Base64.URL_SAFE | Base64.NO_WRAP));
    return uriBuilder.build().toString();
  }

  private void executeStreamDataOperations(
      List<StreamDataOperation> streamDataOperations, /*@Nullable*/ String sessionId) {
    threadUtils.checkMainThread();

    MutationContext.Builder mutationContextBuilder =
        new MutationContext.Builder().setUserInitiated(true);
    if (sessionId != null) {
      mutationContextBuilder.setRequestingSessionId(sessionId);
    }
    feedSessionManager
        .getUpdateConsumer(mutationContextBuilder.build())
        .accept(Result.success(Model.of(streamDataOperations)));
  }
}
