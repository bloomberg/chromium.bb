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

package com.google.android.libraries.feed.piet;

import static com.google.android.libraries.feed.common.testing.RunnableSubject.assertThatRunnable;

import com.google.search.now.ui.piet.ErrorsProto.ErrorCode;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Tests of the NoKeyOverwriteHashMap */
@RunWith(JUnit4.class)
public class NoKeyOverwriteHashMapTest {
  private final NoKeyOverwriteHashMap<String, String> map =
      new NoKeyOverwriteHashMap<>("Acronym", ErrorCode.ERR_DUPLICATE_BINDING_VALUE);

  @Test
  public void testPutTwoDifferentKeys() {
    map.put("CPA", "Certified Public Accountant");
    map.put("CPU", "Central Processing Unit");
  }

  @Test
  public void testPutTwoSameKeysThrows() {
    map.put("CD", "Compact Disc");
    assertThatRunnable(() -> map.put("CD", "Certificate of Deposit"))
        .throwsAnExceptionOfType(PietFatalException.class)
        .that()
        .hasMessageThat()
        .contains("Acronym key 'CD' already defined");
  }
}
