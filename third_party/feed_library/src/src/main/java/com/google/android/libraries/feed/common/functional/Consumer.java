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

package com.google.android.libraries.feed.common.functional;

/**
 * A functional interface that accepts an input and returns nothing. Unlike most functional
 * interfaces, Consumer is expected to cause side effects.
 *
 * <p>This interface should be used in a similar way to Java 8's Consumer interface.
 */
public interface Consumer<T> {
  /** Perform an operation using {@code input}. */
  void accept(T input);
}
