// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.cardboard;

import android.content.Context;
import android.graphics.PointF;
import android.graphics.RectF;
import android.opengl.Matrix;

import org.chromium.chromoting.R;

/**
 * Cardboard activity menu bar that contains multiple menu items.
 */
public class MenuBar {
    public enum MenuItemType {
        BACK(R.drawable.ic_backspace),
        VOICE_INPUT(R.drawable.ic_voice_input),
        ZOOM_IN(R.drawable.ic_zoom_in),
        ZOOM_OUT(R.drawable.ic_zoom_out);

        private final int mResourceId;

        MenuItemType(int resourceId) {
            mResourceId = resourceId;
        }

        public int resourceId() {
            return mResourceId;
        }
    }

    public static final float MENU_ITEM_SIZE = 0.1f;

    private final RectF mMenuBarRect;

    private final MenuItem[] mItems;

    private float[] mModelMatrix;
    private float[] mCombinedMatrix;

    public MenuBar(Context context) {
        MenuItemType[] menuItemTypes = MenuItemType.values();
        final int numItem = menuItemTypes.length;
        mCombinedMatrix = new float[16];
        mModelMatrix = new float[16];

        final float halfMenuWidth = numItem * MENU_ITEM_SIZE / 2;
        final float halfMenuHeight = MENU_ITEM_SIZE / 2;
        mMenuBarRect = new RectF(-halfMenuWidth, -halfMenuHeight, halfMenuWidth, halfMenuHeight);

        RectF currentRect = new RectF(-halfMenuWidth, -halfMenuHeight,
                -halfMenuWidth + MENU_ITEM_SIZE, halfMenuHeight);
        mItems = new MenuItem[numItem];
        for (int i = 0; i < numItem; i++) {
            mItems[i] = new MenuItem(context, menuItemTypes[i], currentRect);
            currentRect.offset(MENU_ITEM_SIZE, 0);
        }
    }

    /**
     * Get menu item that user is looking at.
     * Return the CardboardActivity menu item that contains the passed in coordinates or
     * null if none of menu items contains the passed in coordinates.
     */
    public MenuItem getLookingItem(PointF lookingPosition) {
        for (MenuItem item : mItems) {
            if (item.contains(lookingPosition)) {
                return item;
            }
        }

        return null;
    }

    public void draw(float[] viewMatrix, float[] projectionMatrix, PointF eyeMenuBarPosition,
            float centerX, float centerY, float centerZ) {
        for (MenuItem item : mItems) {
            Matrix.setIdentityM(mModelMatrix, 0);
            Matrix.translateM(mModelMatrix, 0, item.getPosition().x + centerX,
                    item.getPosition().y + centerY, centerZ);
            Matrix.multiplyMM(mCombinedMatrix, 0, viewMatrix, 0, mModelMatrix, 0);
            Matrix.multiplyMM(mCombinedMatrix, 0, projectionMatrix, 0, mCombinedMatrix, 0);
            item.draw(mCombinedMatrix, item.contains(eyeMenuBarPosition));
        }
    }

    /**
     * Return true if the passed in point is inside the menu bar.
     * @param x x coordinate user is looking at.
     * @param y y coordinate user is looking at.
     */
    public boolean contains(PointF lookingPosition) {
        return mMenuBarRect.contains(lookingPosition.x, lookingPosition.y);
    }

    /**
     * Clean up opengl data.
     */
    public void cleanup() {
        for (MenuItem item : mItems) {
            item.clean();
        }
    }
}