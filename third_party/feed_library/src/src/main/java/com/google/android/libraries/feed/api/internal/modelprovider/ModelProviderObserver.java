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

package com.google.android.libraries.feed.api.internal.modelprovider;

import com.google.search.now.feed.client.StreamDataProto.UiContext;

/** Interface used to observe events on the {@link ModelProvider}. */
public interface ModelProviderObserver {

  /**
   * This event is triggered when the ModelProvider is fully initialized and it can be accessed by
   * the UI. If you register for the ModelProvider after the Session is Ready, we will call the
   * Observer. Otherwise the Observer is called once initialization is finished.
   */
  void onSessionStart(UiContext uiContext);

  /**
   * This event is triggered when the Session is invalidated. Once this is called, the UI should no
   * longer call this model. The ModelProvider will free all resources assocated with it including
   * invalidating all existing Cursors.
   *
   * @param uiContext If the session is being finished because of the UI, then this will be the
   *     context given by the UI, otherwise it will be {@link UiContext#getDefaultInstance()}.
   */
  void onSessionFinished(UiContext uiContext);

  /**
   * This is called in the event of an error. For example, if we are making a request and it fails
   * due to network connectivity issues, this event will indicate the error.
   */
  void onError(ModelError modelError);
}
