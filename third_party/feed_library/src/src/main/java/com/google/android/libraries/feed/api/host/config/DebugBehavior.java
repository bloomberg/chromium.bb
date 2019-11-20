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

package com.google.android.libraries.feed.api.host.config;

/**
 * Provides configuration details about the build, such as how much debug logging should occur.
 *
 * <p>Member booleans should control _behavior_ (turning debug features on or off), rather than
 * reporting a build state (like dev or release).
 */
// TODO: This can't be final because we mock it
public class DebugBehavior {
  /** Convenience constant for configuration that enables all debug behavior. */
  public static final DebugBehavior VERBOSE = new DebugBehavior(true);

  /** Convenience constant for configuration that disables all debug behavior. */
  public static final DebugBehavior SILENT = new DebugBehavior(false);

  private final boolean showDebugViews;

  private DebugBehavior(boolean showDebugViews) {
    this.showDebugViews = showDebugViews;
  }

  public boolean getShowDebugViews() {
    return showDebugViews;
  }
}
