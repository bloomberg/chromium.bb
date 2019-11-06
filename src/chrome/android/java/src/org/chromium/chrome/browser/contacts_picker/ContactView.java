// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawableFactory;
import android.util.AttributeSet;
import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.selection.SelectableItemView;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * A container class for a view showing a contact in the Contacts Picker.
 */
public class ContactView extends SelectableItemView<ContactDetails> {
    // Our context.
    private Context mContext;

    // Our parent category.
    private PickerCategoryView mCategoryView;

    // Our selection delegate.
    private SelectionDelegate<ContactDetails> mSelectionDelegate;

    // The details of the contact shown.
    private ContactDetails mContactDetails;

    // The display name of the contact.
    public TextView mDisplayName;

    // The contact details for the contact.
    public TextView mDetailsView;

    // The dialog manager to use to show contact details.
    private ModalDialogManager mManager;

    // The property model listing the contents of the contact details dialog.
    private PropertyModel mModel;

    /**
     * Constructor for inflating from XML.
     */
    public ContactView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;

        setSelectionOnLongClick(false);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mDisplayName = findViewById(R.id.title);
        mDetailsView = findViewById(R.id.description);
        mDetailsView.setMaxLines(2);
    }

    @Override
    public void onClick() {
        // Selection is handled in onClick for the parent class.
        assert false;
    }

    @Override
    public boolean onLongClick(View view) {
        mManager = mCategoryView.getActivity().getModalDialogManager();
        ModalDialogProperties.Controller controller = new ModalDialogProperties.Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {
                mManager.dismissDialog(model, buttonType);
                mModel = null;
                mManager = null;
            }

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {}
        };
        mModel = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                         .with(ModalDialogProperties.CONTROLLER, controller)
                         .with(ModalDialogProperties.TITLE, mContactDetails.getDisplayName())
                         .with(ModalDialogProperties.MESSAGE,
                                 mContactDetails.getContactDetailsAsString(true, null))
                         .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, mContext.getResources(),
                                 R.string.close)
                         .build();
        mModel.set(ModalDialogProperties.TITLE_ICON, getIconDrawable());
        mManager.showDialog(mModel, ModalDialogManager.ModalDialogType.APP);
        return true;
    }

    @Override
    public void onSelectionStateChange(List<ContactDetails> selectedItems) {
        // If the user cancels the dialog before this object has initialized, the SelectionDelegate
        // will try to notify us that all selections have been cleared. However, we don't need to
        // process that message.
        if (mContactDetails == null) return;

        // When SelectAll or Undo is used, the underlying UI must be updated
        // to reflect the changes.
        boolean selected = selectedItems.contains(mContactDetails);
        boolean checked = super.isChecked();
        if (selected != checked) super.toggle();
    }

    /**
     * Sets the {@link PickerCategoryView} for this ContactView.
     * @param categoryView The category view showing the images. Used to access
     *     common functionality and sizes and retrieve the {@link SelectionDelegate}.
     */
    public void setCategoryView(PickerCategoryView categoryView) {
        mCategoryView = categoryView;
        mSelectionDelegate = mCategoryView.getSelectionDelegate();
        setSelectionDelegate(mSelectionDelegate);
    }

    /**
     * Completes the initialization of the ContactView. Must be called before the
     * {@link ContactView} can respond to click events.
     * @param contactDetails The details about the contact represented by this ContactView.
     * @param icon The icon to show for the contact (or null if not loaded yet).
     */
    public void initialize(ContactDetails contactDetails, Bitmap icon) {
        resetTile();

        mContactDetails = contactDetails;
        setItem(contactDetails);

        String displayName = contactDetails.getDisplayName();
        mDisplayName.setText(displayName);

        String details = contactDetails.getContactDetailsAsString(
                /*longVersion=*/false, mContext.getResources());
        mDetailsView.setText(details);
        mDetailsView.setVisibility(details.isEmpty() ? View.GONE : View.VISIBLE);

        if (icon == null) {
            icon = mCategoryView.getIconGenerator().generateIconForText(
                    contactDetails.getDisplayNameAbbreviation());
            setIconDrawable(new BitmapDrawable(getResources(), icon));
        } else {
            setIconBitmap(icon);
        }
    }

    /**
     * Sets the icon to display for the contact and fade it into view.
     * @param icon The icon to display.
     */
    public void setIconBitmap(Bitmap icon) {
        Resources resources = mContext.getResources();
        RoundedBitmapDrawable drawable = RoundedBitmapDrawableFactory.create(resources, icon);
        drawable.setCircular(true);
        setIconDrawable(drawable);
    }

    /**
     * Resets the view to its starting state, which is necessary when the view is about to be
     * re-used.
     */
    private void resetTile() {
        setIconDrawable(null);
        mDisplayName.setText("");
        mDetailsView.setText("");
    }
}
