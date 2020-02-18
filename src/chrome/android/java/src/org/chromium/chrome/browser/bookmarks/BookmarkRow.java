// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.chromium.chrome.browser.ui.widget.listmenu.BasicListMenu.buildMenuListItem;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.ui.widget.listmenu.BasicListMenu;
import org.chromium.chrome.browser.ui.widget.listmenu.ListMenu;
import org.chromium.chrome.browser.ui.widget.listmenu.ListMenuButton;
import org.chromium.chrome.browser.ui.widget.listmenu.ListMenuButton.PopupMenuShownListener;
import org.chromium.chrome.browser.ui.widget.listmenu.ListMenuButtonDelegate;
import org.chromium.chrome.browser.ui.widget.listmenu.ListMenuItemProperties;
import org.chromium.chrome.browser.widget.selection.SelectableItemView;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Common logic for bookmark and folder rows.
 */
abstract class BookmarkRow extends SelectableItemView<BookmarkId> implements BookmarkUIObserver {
    protected ListMenuButton mMoreIcon;
    protected ImageView mDragHandle;
    protected BookmarkDelegate mDelegate;
    protected BookmarkId mBookmarkId;
    private boolean mIsAttachedToWindow;
    private final boolean mReorderBookmarksEnabled;
    private final boolean mShowInFolderEnabled;
    private PopupMenuShownListener mPopupListener;
    @Location
    private int mLocation;

    @IntDef({Location.TOP, Location.MIDDLE, Location.BOTTOM, Location.SOLO})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Location {
        int TOP = 0;
        int MIDDLE = 1;
        int BOTTOM = 2;
        int SOLO = 3;
    }

    /**
     * Constructor for inflating from XML.
     */
    public BookmarkRow(Context context, AttributeSet attrs) {
        super(context, attrs);
        mReorderBookmarksEnabled = ChromeFeatureList.isEnabled(ChromeFeatureList.REORDER_BOOKMARKS);
        mShowInFolderEnabled = mReorderBookmarksEnabled
                && ChromeFeatureList.isEnabled(ChromeFeatureList.BOOKMARKS_SHOW_IN_FOLDER);
    }

    /**
     * Updates this row for the given {@link BookmarkId}.
     *
     * @return The {@link BookmarkItem} corresponding the given {@link BookmarkId}.
     */
    // TODO(crbug.com/160194): Clean up these 2 functions after bookmark reordering launches.
    BookmarkItem setBookmarkId(BookmarkId bookmarkId) {
        mBookmarkId = bookmarkId;
        BookmarkItem bookmarkItem = mDelegate.getModel().getBookmarkById(bookmarkId);
        mMoreIcon.dismiss();
        mMoreIcon.setContentDescriptionContext(bookmarkItem.getTitle());
        setChecked(isItemSelected());
        updateVisualState();

        super.setItem(bookmarkId);
        return bookmarkItem;
    }

    /**
     * Sets the bookmark ID for this BookmarkRow and provides information about its location
     * within the list of bookmarks.
     *
     * @param bookmarkId The BookmarkId that this BookmarkRow now contains.
     * @param location   The location of this BookmarkRow.
     * @return The BookmarkItem corresponding to BookmarkId.
     */
    BookmarkItem setBookmarkId(BookmarkId bookmarkId, @Location int location) {
        mLocation = location;
        return setBookmarkId(bookmarkId);
    }

    private void updateVisualState() {
        // This check is needed because it is possible for updateVisualState to be called between
        // onDelegateInitialized (SelectionDelegate triggers a redraw) and setBookmarkId. View is
        // not currently bound, so we can skip this for now. updateVisualState will run inside of
        // setBookmarkId.
        if (mBookmarkId == null) {
            return;
        }
        BookmarkItem bookmarkItem = mDelegate.getModel().getBookmarkById(mBookmarkId);
        // This check is needed because updateVisualState is called when the item has been deleted
        // in the model but not in the adapter. If we hit this if-block, the
        // item is about to be deleted, and we don't need to do anything.
        if (bookmarkItem == null) {
            return;
        }
        // TODO(jhimawan): Look into using cleanup(). Perhaps unhook the selection state observer?

        // If the visibility of the drag handle or more icon is not set later, it will be gone.
        mDragHandle.setVisibility(GONE);
        mMoreIcon.setVisibility(GONE);

        if (mReorderBookmarksEnabled && mDelegate.getDragStateDelegate().getDragActive()) {
            mDragHandle.setVisibility(bookmarkItem.isMovable() ? VISIBLE : GONE);
            mDragHandle.setEnabled(isItemSelected());
        } else {
            mMoreIcon.setVisibility(bookmarkItem.isEditable() ? VISIBLE : GONE);
            mMoreIcon.setClickable(!isSelectionModeActive());
            mMoreIcon.setEnabled(mMoreIcon.isClickable());
        }
    }

    /**
     * Sets the delegate to use to handle UI actions related to this view.
     *
     * @param delegate A {@link BookmarkDelegate} instance to handle all backend interaction.
     */
    public void onDelegateInitialized(BookmarkDelegate delegate) {
        super.setSelectionDelegate(delegate.getSelectionDelegate());
        mDelegate = delegate;
        if (mIsAttachedToWindow) initialize();
    }

    private void initialize() {
        mDelegate.addUIObserver(this);
        mPopupListener = () -> mDelegate.onBookmarkItemMenuOpened();
        mMoreIcon.addPopupListener(mPopupListener);
    }

    private void cleanup() {
        mMoreIcon.dismiss();
        mMoreIcon.removePopupListener(mPopupListener);
        if (mDelegate != null) mDelegate.removeUIObserver(this);
    }

    // TODO(lazzzis): try to clean up reference to mLocation if position.
    private ModelList getItems() {
        // Rebuild listItems, cause mLocation may be changed anytime.
        boolean canMove = false;
        if (mDelegate != null && mDelegate.getModel() != null) {
            BookmarkItem bookmarkItem = mDelegate.getModel().getBookmarkById(mBookmarkId);
            if (bookmarkItem != null) canMove = bookmarkItem.isMovable();
        }
        ModelList listItems = new ModelList();
        listItems.add(buildMenuListItem(R.string.bookmark_item_select, 0, 0));
        listItems.add(buildMenuListItem(R.string.bookmark_item_edit, 0, 0));
        listItems.add(buildMenuListItem(R.string.bookmark_item_move, 0, 0, canMove));
        listItems.add(buildMenuListItem(R.string.bookmark_item_delete, 0, 0));
        if (mReorderBookmarksEnabled) {
            if (mDelegate.getCurrentState() == BookmarkUIState.STATE_SEARCHING) {
                if (mShowInFolderEnabled) {
                    listItems.add(buildMenuListItem(R.string.bookmark_show_in_folder, 0, 0));
                }
            } else if (mDelegate.getCurrentState() == BookmarkUIState.STATE_FOLDER
                    && mLocation != Location.SOLO && canMove) {
                // Only add move up / move down buttons if there is more than 1 item
                if (mLocation != Location.TOP) {
                    listItems.add(buildMenuListItem(R.string.menu_item_move_up, 0, 0));
                }
                if (mLocation != Location.BOTTOM) {
                    listItems.add(buildMenuListItem(R.string.menu_item_move_down, 0, 0));
                }
            }
        }
        return listItems;
    }

    private ListMenu getListMenu() {
        ModelList listItems = getItems();
        ListMenu.Delegate delegate = item -> {
            int textId = item.get(ListMenuItemProperties.TITLE_ID);
            if (textId == R.string.bookmark_item_select) {
                setChecked(mDelegate.getSelectionDelegate().toggleSelectionForItem(mBookmarkId));
                RecordUserAction.record("Android.BookmarkPage.SelectFromMenu");
            } else if (textId == R.string.bookmark_item_edit) {
                BookmarkItem bookmarkItem = mDelegate.getModel().getBookmarkById(mBookmarkId);
                if (bookmarkItem.isFolder()) {
                    BookmarkAddEditFolderActivity.startEditFolderActivity(
                            getContext(), bookmarkItem.getId());
                } else {
                    BookmarkUtils.startEditActivity(getContext(), bookmarkItem.getId());
                }
            } else if (textId == R.string.bookmark_item_move) {
                BookmarkFolderSelectActivity.startFolderSelectActivity(getContext(), mBookmarkId);
                RecordUserAction.record("MobileBookmarkManagerMoveToFolder");
            } else if (textId == R.string.bookmark_item_delete) {
                if (mDelegate != null && mDelegate.getModel() != null) {
                    mDelegate.getModel().deleteBookmarks(mBookmarkId);
                    RecordUserAction.record("Android.BookmarkPage.RemoveItem");
                }
            } else if (textId == R.string.bookmark_show_in_folder) {
                BookmarkItem bookmarkItem = mDelegate.getModel().getBookmarkById(mBookmarkId);
                mDelegate.openFolder(bookmarkItem.getParentId());
                mDelegate.highlightBookmark(mBookmarkId);
                RecordUserAction.record("MobileBookmarkManagerShowInFolder");
            } else if (textId == R.string.menu_item_move_up) {
                mDelegate.moveUpOne(mBookmarkId);
                RecordUserAction.record("MobileBookmarkManagerMoveUp");
            } else if (textId == R.string.menu_item_move_down) {
                mDelegate.moveDownOne(mBookmarkId);
                RecordUserAction.record("MobileBookmarkManagerMoveDown");
            };
        };
        return new BasicListMenu(getContext(), listItems, delegate);
    }

    // FrameLayout implementation.
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mMoreIcon = (ListMenuButton) findViewById(R.id.more);
        mMoreIcon.setDelegate(getListMenuButtonDelegate());
        mDragHandle = findViewById(R.id.drag_handle);
    }

    private ListMenuButtonDelegate getListMenuButtonDelegate() {
        return this::getListMenu;
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        mIsAttachedToWindow = true;
        if (mDelegate != null) {
            initialize();
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mIsAttachedToWindow = false;
        cleanup();
    }

    // SelectableItem implementation.
    @Override
    public void onSelectionStateChange(List<BookmarkId> selectedBookmarks) {
        super.onSelectionStateChange(selectedBookmarks);
        updateVisualState();
    }

    // BookmarkUIObserver implementation.
    @Override
    public void onDestroy() {
        cleanup();
    }

    @Override
    public void onFolderStateSet(BookmarkId folder) {}

    @Override
    public void onSearchStateSet() {}

    @VisibleForTesting
    public boolean isItemSelected() {
        return mDelegate.getSelectionDelegate().isItemSelected(mBookmarkId);
    }

    void setDragHandleOnTouchListener(OnTouchListener l) {
        mDragHandle.setOnTouchListener(l);
    }

    @VisibleForTesting
    String getTitle() {
        return String.valueOf(mTitleView.getText());
    }

    private boolean isDragActive() {
        if (mReorderBookmarksEnabled) {
            return mDelegate.getDragStateDelegate().getDragActive();
        }
        return false;
    }

    @Override
    public boolean onLongClick(View view) {
        // Override is needed in order to support long-press-to-drag on already-selected items.
        if (isDragActive() && isItemSelected()) return true;
        RecordUserAction.record("MobileBookmarkManagerLongPressToggleSelect");
        return super.onLongClick(view);
    }

    @Override
    public void onClick(View view) {
        // Override is needed in order to allow items to be selected / deselected with a click.
        // Since we override #onLongClick(), we cannot rely on the base class for this behavior.
        if (isDragActive()) {
            toggleSelectionForItem(getItem());
            RecordUserAction.record("MobileBookmarkManagerTapToggleSelect");
        } else {
            super.onClick(view);
        }
    }
}