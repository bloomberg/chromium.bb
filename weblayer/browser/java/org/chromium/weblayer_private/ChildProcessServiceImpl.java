// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.base.process_launcher.ChildProcessService;
import org.chromium.content_public.app.ChildProcessServiceFactory;
import org.chromium.weblayer_private.aidl.IChildProcessService;
import org.chromium.weblayer_private.aidl.IObjectWrapper;
import org.chromium.weblayer_private.aidl.ObjectWrapper;

@UsedByReflection("WebLayer")
public final class ChildProcessServiceImpl extends IChildProcessService.Stub {
    private ChildProcessService mService;

    @UsedByReflection("WebLayer")
    public static IBinder create(Service service, Context context) {
        return new ChildProcessServiceImpl(service, context);
    }

    @Override
    public void onCreate() {
        mService.onCreate();
    }

    @Override
    public void onDestroy() {
        mService.onDestroy();
        mService = null;
    }

    @Override
    public IObjectWrapper onBind(IObjectWrapper intent) {
        return ObjectWrapper.wrap(mService.onBind(ObjectWrapper.unwrap(intent, Intent.class)));
    }

    private ChildProcessServiceImpl(Service service, Context context) {
        mService = ChildProcessServiceFactory.create(service, context);
    }
}
