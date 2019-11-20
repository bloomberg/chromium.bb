// Copyright 2018 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.android.libraries.feed.feedstore.internal;

import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.internal.store.LocalActionMutation;
import com.google.android.libraries.feed.common.functional.Committer;
import com.google.android.libraries.feed.common.logging.Logger;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Implementation of {@link LocalActionMutation} */
public final class FeedLocalActionMutation implements LocalActionMutation {
  private static final String TAG = "FeedLocalActionMutation";

  private final Map<Integer, List<String>> actions = new HashMap<>();
  private final Committer<CommitResult, Map<Integer, List<String>>> committer;

  FeedLocalActionMutation(Committer<CommitResult, Map<Integer, List<String>>> committer) {
    this.committer = committer;
  }

  @Override
  public LocalActionMutation add(int action, String contentId) {
    /*@Nullable*/ List<String> actionsForType = actions.get(action);
    if (actionsForType == null) {
      actionsForType = new ArrayList<>();
    }
    actionsForType.add(contentId);
    actions.put(action, actionsForType);
    Logger.i(TAG, "Added action %d with content id %s", action, contentId);
    return this;
  }

  @Override
  public CommitResult commit() {
    return committer.commit(actions);
  }
}
