// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modaldialog;

import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The model properties for a modal dialog.
 */
public class ModalDialogProperties {
    /**
     * Interface that controls the actions on the modal dialog.
     */
    public interface Controller {
        /**
         * Handle click event of the buttons on the dialog.
         * @param model The dialog model that is associated with this click event.
         * @param buttonType The type of the button.
         */
        void onClick(PropertyModel model, @ButtonType int buttonType);

        /**
         * Handle dismiss event when the dialog is dismissed by actions on the dialog. Note that it
         * can be dangerous to the {@code dismissalCause} for business logic other than metrics
         * recording, unless the dismissal cause is fully controlled by the client (e.g. button
         * clicked), because the dismissal cause can be different values depending on modal dialog
         * type and mode of presentation (e.g. it could be unknown on VR but a specific value on
         * non-VR).
         * @param model The dialog model that is associated with this dismiss event.
         * @param dismissalCause The reason of the dialog being dismissed.
         * @see DialogDismissalCause
         */
        void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause);
    }

    @IntDef({ModalDialogProperties.ButtonType.POSITIVE, ModalDialogProperties.ButtonType.NEGATIVE,
            ModalDialogProperties.ButtonType.TITLE_ICON})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ButtonType {
        int POSITIVE = 0;
        int NEGATIVE = 1;
        int TITLE_ICON = 2;
    }

    /**
     * Styles of the primary and negative button. Only one of them can be filled at the same time.
     */
    @IntDef({ModalDialogProperties.ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_OUTLINE,
            ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE,
            ModalDialogProperties.ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_FILLED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ButtonStyles {
        int PRIMARY_OUTLINE_NEGATIVE_OUTLINE = 0;
        int PRIMARY_FILLED_NEGATIVE_OUTLINE = 1;
        int PRIMARY_OUTLINE_NEGATIVE_FILLED = 2;
    }

    /** The {@link Controller} that handles events on user actions. */
    public static final ReadableObjectPropertyKey<Controller> CONTROLLER =
            new ReadableObjectPropertyKey<>();

    /** The content description of the dialog for accessibility. */
    public static final ReadableObjectPropertyKey<String> CONTENT_DESCRIPTION =
            new ReadableObjectPropertyKey<>();

    /** The title of the dialog. */
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    /** The title icon of the dialog. */
    public static final WritableObjectPropertyKey<Drawable> TITLE_ICON =
            new WritableObjectPropertyKey<>();

    /** The message of the dialog. */
    public static final WritableObjectPropertyKey<CharSequence> MESSAGE =
            new WritableObjectPropertyKey<>();

    /** The customized content view of the dialog. */
    public static final WritableObjectPropertyKey<View> CUSTOM_VIEW =
            new WritableObjectPropertyKey<>();

    /** The text on the positive button. */
    public static final WritableObjectPropertyKey<String> POSITIVE_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();

    /** Content description for the positive button. */
    public static final WritableObjectPropertyKey<String> POSITIVE_BUTTON_CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    /** The enabled state on the positive button. */
    public static final WritableBooleanPropertyKey POSITIVE_BUTTON_DISABLED =
            new WritableBooleanPropertyKey();

    /** The text on the negative button. */
    public static final WritableObjectPropertyKey<String> NEGATIVE_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();

    /** Content description for the negative button. */
    public static final WritableObjectPropertyKey<String> NEGATIVE_BUTTON_CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    /** The enabled state on the negative button. */
    public static final WritableBooleanPropertyKey NEGATIVE_BUTTON_DISABLED =
            new WritableBooleanPropertyKey();

    /** Whether the dialog should be dismissed on user tapping the scrim. */
    public static final WritableBooleanPropertyKey CANCEL_ON_TOUCH_OUTSIDE =
            new WritableBooleanPropertyKey();

    /**
     * Whether button touch events should be filtered when buttons are obscured by another visible
     * window.
     */
    public static final ReadableBooleanPropertyKey FILTER_TOUCH_FOR_SECURITY =
            new ReadableBooleanPropertyKey();

    /**
     * Callback to be called when the modal dialog filters touch events because the buttons are
     * obscured by another window.
     */
    public static final ReadableObjectPropertyKey<Runnable> TOUCH_FILTERED_CALLBACK =
            new ReadableObjectPropertyKey<>();

    /** Whether the title is scrollable with the message. */
    public static final WritableBooleanPropertyKey TITLE_SCROLLABLE =
            new WritableBooleanPropertyKey();

    /** Whether the primary (positive) or negative button should be a filled button */
    public static final ReadableIntPropertyKey BUTTON_STYLES = new ReadableIntPropertyKey();

    /**
     * Whether the dialog is of fullscreen style.
     */
    public static final ReadableBooleanPropertyKey FULLSCREEN_DIALOG =
            new ReadableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {CONTROLLER, CONTENT_DESCRIPTION,
            TITLE, TITLE_ICON, MESSAGE, CUSTOM_VIEW, POSITIVE_BUTTON_TEXT,
            POSITIVE_BUTTON_CONTENT_DESCRIPTION, POSITIVE_BUTTON_DISABLED, NEGATIVE_BUTTON_TEXT,
            NEGATIVE_BUTTON_CONTENT_DESCRIPTION, NEGATIVE_BUTTON_DISABLED, CANCEL_ON_TOUCH_OUTSIDE,
            FILTER_TOUCH_FOR_SECURITY, TOUCH_FILTERED_CALLBACK, TITLE_SCROLLABLE, BUTTON_STYLES,
            FULLSCREEN_DIALOG};
}