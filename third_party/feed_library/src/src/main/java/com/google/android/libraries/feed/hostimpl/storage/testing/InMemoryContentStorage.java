// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.hostimpl.storage.testing;

import static com.google.android.libraries.feed.api.host.storage.ContentOperation.Type.DELETE;
import static com.google.android.libraries.feed.api.host.storage.ContentOperation.Type.DELETE_BY_PREFIX;
import static com.google.android.libraries.feed.api.host.storage.ContentOperation.Type.UPSERT;

import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.host.storage.ContentMutation;
import com.google.android.libraries.feed.api.host.storage.ContentOperation;
import com.google.android.libraries.feed.api.host.storage.ContentOperation.Delete;
import com.google.android.libraries.feed.api.host.storage.ContentOperation.DeleteByPrefix;
import com.google.android.libraries.feed.api.host.storage.ContentOperation.Type;
import com.google.android.libraries.feed.api.host.storage.ContentOperation.Upsert;
import com.google.android.libraries.feed.api.host.storage.ContentStorage;
import com.google.android.libraries.feed.api.host.storage.ContentStorageDirect;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.logging.Dumpable;
import com.google.android.libraries.feed.common.logging.Dumper;
import com.google.android.libraries.feed.common.logging.Logger;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

/** A {@link ContentStorage} that holds data in memory. */
public final class InMemoryContentStorage implements ContentStorageDirect, Dumpable {
    private static final String TAG = "InMemoryContentStorage";

    private final Map<String, byte[]> store = new HashMap<>();

    private int getCount = 0;
    private int getAllCount = 0;
    private int insertCount = 0;
    private int updateCount = 0;

    @Override
    public Result<Map<String, byte[]>> get(List<String> keys) {
        getCount++;

        Map<String, byte[]> valueMap = new HashMap<>(keys.size());
        for (String key : keys) {
            byte[] value = store.get(key);
            if (value == null || value.length == 0) {
                Logger.w(TAG, "Didn't find value for %s, not adding to map", key);
            } else {
                valueMap.put(key, value);
            }
        }
        return Result.success(valueMap);
    }

    @Override
    public Result<Map<String, byte[]>> getAll(String prefix) {
        getAllCount++;
        Map<String, byte[]> results = new HashMap<>();
        for (Entry<String, byte[]> entry : store.entrySet()) {
            if (entry.getKey().startsWith(prefix)) {
                results.put(entry.getKey(), entry.getValue());
            }
        }
        return Result.success(results);
    }

    @Override
    public CommitResult commit(ContentMutation mutation) {
        for (ContentOperation operation : mutation.getOperations()) {
            if (operation.getType() == UPSERT) {
                if (!upsert((Upsert) operation)) {
                    return CommitResult.FAILURE;
                }
            } else if (operation.getType() == DELETE) {
                if (!delete((Delete) operation)) {
                    return CommitResult.FAILURE;
                }
            } else if (operation.getType() == DELETE_BY_PREFIX) {
                if (!deleteByPrefix((DeleteByPrefix) operation)) {
                    return CommitResult.FAILURE;
                }
            } else if (operation.getType() == Type.DELETE_ALL) {
                if (!deleteAll()) {
                    return CommitResult.FAILURE;
                }
            } else {
                Logger.e(TAG, "Invalid ContentMutation: unexpected operation: %s",
                        operation.getType());
                return CommitResult.FAILURE;
            }
        }

        return CommitResult.SUCCESS;
    }

    @Override
    public Result<List<String>> getAllKeys() {
        return Result.success(new ArrayList<>(store.keySet()));
    }

    private boolean deleteAll() {
        store.clear();
        return true;
    }

    private boolean deleteByPrefix(DeleteByPrefix operation) {
        List<String> keysToRemove = new ArrayList<>();
        for (String key : store.keySet()) {
            if (key.startsWith(operation.getPrefix())) {
                keysToRemove.add(key);
            }
        }
        store.keySet().removeAll(keysToRemove);
        return true;
    }

    private boolean delete(Delete operation) {
        store.remove(operation.getKey());
        return true;
    }

    private boolean upsert(Upsert operation) {
        String key = operation.getKey();
        // TODO: remove unneeded null checks.
        if (key == null) {
            Logger.e(TAG, "Invalid ContentMutation: null key");
            return false;
        }
        byte[] value = operation.getValue();
        if (value == null) {
            Logger.e(TAG, "Invalid ContentMutation: null value");
            return false;
        }
        if (value.length == 0) {
            Logger.e(TAG, "Invalid ContentMutation: empty value");
            return false;
        }
        byte[] currentValue = store.put(key, value);
        if (currentValue == null) {
            insertCount++;
        } else {
            updateCount++;
        }
        return true;
    }

    @Override
    public void dump(Dumper dumper) {
        dumper.title(TAG);
        dumper.forKey("contentItems").value(store.size());
        dumper.forKey("getCount").value(getCount);
        dumper.forKey("getAllCount").value(getAllCount).compactPrevious();
        dumper.forKey("insertCount").value(insertCount).compactPrevious();
        dumper.forKey("updateCount").value(updateCount).compactPrevious();
    }
}
