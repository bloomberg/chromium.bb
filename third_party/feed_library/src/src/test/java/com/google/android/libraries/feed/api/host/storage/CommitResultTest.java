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

package com.google.android.libraries.feed.api.host.storage;

import static com.google.common.truth.Truth.assertThat;

import com.google.android.libraries.feed.api.host.storage.CommitResult.Result;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Test class for {@link CommitResult} */
@RunWith(RobolectricTestRunner.class)
public class CommitResultTest {
  @Test
  public void testSuccess() {
    assertThat(CommitResult.SUCCESS.getResult()).isEqualTo(Result.SUCCESS);
  }

  @Test
  public void testSuccessSingleton() {
    CommitResult success = CommitResult.SUCCESS;
    assertThat(success).isEqualTo(CommitResult.SUCCESS);
  }

  @Test
  public void testFailure() {
    assertThat(CommitResult.FAILURE.getResult()).isEqualTo(Result.FAILURE);
  }

  @Test
  public void testFailureSingleton() {
    CommitResult failure = CommitResult.FAILURE;
    assertThat(failure).isEqualTo(CommitResult.FAILURE);
  }
}
