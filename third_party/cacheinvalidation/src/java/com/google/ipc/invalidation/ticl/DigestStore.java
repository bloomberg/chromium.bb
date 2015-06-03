/*
 * Copyright 2011 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.ipc.invalidation.ticl;

import java.util.Collection;

/**
 * Interface for a store that allows objects to be added/removed along with
 * the ability to get the digest for the whole or partial set of those objects.
 *
 * @param <ElementType> the type of the element stored in this
 *
 */
public interface DigestStore<ElementType> {

  /** Returns the number of elements. */
  int size();

  /** Returns whether {@code element} is in the store. */
  boolean contains(ElementType element);

  /**
   * Returns a digest of the desired objects.
   * <p>
   * NOTE: the digest computations <b>MUST NOT</b> depend on the order in which the elements
   * were added.
   */
  byte[] getDigest();

  /**
   * Returns the elements whose digest prefixes begin with the bit prefix {@code digestPrefix}.
   * {@code prefixLen} is the length of {@code digestPrefix} in bits, which may be less than
   * {@code digestPrefix.length} (and may be 0). The implementing class can return *more* objects
   * than what has been specified by {@code digestPrefix}, e.g., it could return all the objects
   * in the store.
   */
  Collection<ElementType> getElements(byte[] digestPrefix, int prefixLen);

  /**
   * Adds {@code element} to the store. No-op if {@code element} is already present.
   * @return whether the element was added
   */
  boolean add(ElementType element);

  /**
   * Adds {@code elements} to the store. If any element in {@code elements} is already present,
   * the addition is a no-op for that element.
   * @return the elements that were added
   */
  Collection<ElementType> add(Collection<ElementType> elements);

  /**
   * Removes {@code element} from the store. No-op if {@code element} is not present.
   * @return whether the element was removed
   */
  boolean remove(ElementType element);

  /**
   * Remove {@code elements} to the store. If any element in {@code elements} is not present, the
   * removal is a no-op for that element.
   * @return the elements that were removed
   */
  Collection<ElementType> remove(Collection<ElementType> elements);

  /** Removes all elements in this and returns them. */
  Collection<ElementType> removeAll();
}
