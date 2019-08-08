// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.support.annotation.StringDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * List of attributes used for {@link TabAttributes}.
 */
@StringDef({TabAttributeKeys.GROUPED_WITH_PARENT, TabAttributeKeys.MODAL_DIALOG_SHOWING})
@Retention(RetentionPolicy.SOURCE)
public @interface TabAttributeKeys {
    /** Whether the tab should be grouped with its parent tab. True by default. */
    public static final String GROUPED_WITH_PARENT = "isTabGroupedWithParent";

    /** Whether tab modal dialog is showing or not. */
    public static final String MODAL_DIALOG_SHOWING = "isTabModalDialogShowing";

    /** Parent Tab Task Id. See ContextRecordTaskId (context_record_task_id.h) for definition */
    public static final String PARENT_TAB_TASK_ID = "ParentTaskId";

    /**
     * Parent Tab Root Task Id. See ContextRecordTaskId (context_record_task_id.h) for definition
     */
    public static final String PARENT_TAB_ROOT_TASK_ID = "ParentRootTaskId";

    /** A runnable to delay the enabling of fullscreen mode if necessary. */
    public static final String ENTER_FULLSCREEN = "EnterFullscreen";
}
