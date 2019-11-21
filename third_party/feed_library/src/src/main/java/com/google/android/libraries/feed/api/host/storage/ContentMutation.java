// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.host.storage;

import com.google.android.libraries.feed.api.host.storage.ContentOperation.Delete;
import com.google.android.libraries.feed.api.host.storage.ContentOperation.DeleteAll;
import com.google.android.libraries.feed.api.host.storage.ContentOperation.DeleteByPrefix;
import com.google.android.libraries.feed.api.host.storage.ContentOperation.Upsert;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * A sequence of {@link ContentOperation} instances. This class is used to commit changes to {@link
 * ContentStorage}.
 */
public final class ContentMutation {
  private final List<ContentOperation> operations;

  private ContentMutation(List<ContentOperation> operations) {
    this.operations = Collections.unmodifiableList(operations);
  }

  /** An unmodifiable list of operations to be committed by {@link ContentStorage}. */
  public List<ContentOperation> getOperations() {
    return operations;
  }

  /**
   * Creates a {@link ContentMutation}, which can be used to apply mutations to the underlying
   * {@link ContentStorage}.
   */
  public static final class Builder {
    private final ArrayList<ContentOperation> operations = new ArrayList<>();
    private final ContentOperationListSimplifier simplifier = new ContentOperationListSimplifier();

    /**
     * Sets the key/value pair in {@link ContentStorage}, or inserts it if it doesn't exist.
     *
     * <p>This method can be called repeatedly to assign multiple key/values. If the same key is
     * assigned multiple times, only the last value will be persisted.
     */
    public Builder upsert(String key, byte[] value) {
      operations.add(new Upsert(key, value));
      return this;
    }

    /**
     * Deletes the value from {@link ContentStorage} with a matching key, if it exists.
     *
     * <p>{@link ContentStorage#commit(ContentMutation)} will fulfill with result is {@code TRUE},
     * even if {@code key} is not found in {@link ContentStorage}.
     *
     * <p>If {@link Delete} and {@link Upsert} are committed for the same key, only the last
     * operation will take effect.
     */
    public Builder delete(String key) {
      operations.add(new Delete(key));
      return this;
    }

    /**
     * Deletes all values from {@link ContentStorage} with matching key prefixes.
     *
     * <p>{@link ContentStorage#commit(ContentMutation)} will fulfill with result equal to {@code
     * TRUE}, even if no keys have a matching prefix.
     *
     * <p>If {@link DeleteByPrefix} and {@link Upsert} are committed for the same matching key, only
     * the last operation will take effect.
     */
    public Builder deleteByPrefix(String prefix) {
      operations.add(new DeleteByPrefix(prefix));
      return this;
    }

    /** Deletes all values from {@link ContentStorage}. */
    public Builder deleteAll() {
      operations.add(new DeleteAll());
      return this;
    }

    /**
     * Simplifies the sequence of {@link ContentOperation} instances, and returns a new {@link
     * ContentMutation} the simplified list.
     */
    public ContentMutation build() {
      return new ContentMutation(simplifier.simplify(operations));
    }
  }

  @Override
  public boolean equals(/*@Nullable*/ Object o) {
    if (this == o) {
      return true;
    }
    if (o == null || getClass() != o.getClass()) {
      return false;
    }

    ContentMutation that = (ContentMutation) o;

    return operations != null ? operations.equals(that.operations) : that.operations == null;
  }

  @Override
  public int hashCode() {
    return operations != null ? operations.hashCode() : 0;
  }
}
