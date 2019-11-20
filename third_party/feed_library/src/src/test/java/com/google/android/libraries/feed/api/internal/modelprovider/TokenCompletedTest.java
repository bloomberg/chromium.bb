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

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests of the {@link TokenCompleted} class. */
@RunWith(RobolectricTestRunner.class)
public class TokenCompletedTest {
  @Mock private ModelCursor modelCursor;

  @Before
  public void setUp() {
    initMocks(this);
  }

  @Test
  public void testTokenChange() {
    TokenCompleted tokenCompleted = new TokenCompleted(modelCursor);
    assertThat(tokenCompleted.getCursor()).isEqualTo(modelCursor);
  }
}
