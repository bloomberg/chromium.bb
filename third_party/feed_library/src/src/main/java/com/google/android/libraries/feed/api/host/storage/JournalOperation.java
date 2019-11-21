// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.api.host.storage;

import android.support.annotation.IntDef;

/** A mutation to the underlying {@link JournalStorage}. */
public abstract class JournalOperation {
  /** The types of operations. */
  @IntDef({Type.APPEND, Type.COPY, Type.DELETE})
  public @interface Type {
    /** Append bytes to the provided journal */
    int APPEND = 0;
    /** Copy the provided journal to the given journal name */
    int COPY = 1;
    /** Delete the provided journal */
    int DELETE = 2;
  }

  public @Type int getType() {
    return type;
  }

  private final @Type int type;

  // Only the following classes may extend JournalOperation
  private JournalOperation(@Type int type) {
    this.type = type;
  }

  /**
   * A {@link JournalOperation} created by calling {@link JournalMutation.Builder#append(byte[])}.
   */
  public static final class Append extends JournalOperation {
    private final byte[] value;

    Append(byte[] value) {
      super(Type.APPEND);
      this.value = value;
    }

    public byte[] getValue() {
      return value;
    }
  }

  /** A {@link JournalOperation} created by calling {@link JournalMutation.Builder#copy(String)}. */
  public static final class Copy extends JournalOperation {
    private final String toJournalName;

    Copy(String toJournalName) {
      super(Type.COPY);
      this.toJournalName = toJournalName;
    }

    public String getToJournalName() {
      return toJournalName;
    }
  }

  /** A {@link JournalOperation} created by calling {@link JournalMutation.Builder#delete()}. */
  public static final class Delete extends JournalOperation {
    Delete() {
      super(Type.DELETE);
    }
  }
}
