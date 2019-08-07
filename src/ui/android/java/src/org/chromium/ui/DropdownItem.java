// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

/**
 * Dropdown item interface used to access all the information needed to show the item.
 */
public interface DropdownItem {
    // A stand in for a resource ID which indicates no icon should be shown.
    public static final int NO_ICON = 0;

    /**
     * Returns the label that should be shown in the dropdown.
     */
    String getLabel();
    /**
     * Returns the sublabel that should be shown in the dropdown.
     */
    String getSublabel();
    /**
     * Returns the drawable id of the icon that should be shown in the dropdown, or NO_ICON.
     */
    int getIconId();
    /**
     * Returns true if the item should be enabled in the dropdown.
     */
    boolean isEnabled();
    /**
     * Returns true if the item should be a group header in the dropdown.
     */
    boolean isGroupHeader();
    /**
     * Returns whether the label should be displayed over multiple lines.
     */
    boolean isMultilineLabel();
    /**
     * Returns whether the label should be displayed in bold.
     */
    boolean isBoldLabel();
    /**
     * Returns resource ID of label's font color.
     */
    int getLabelFontColorResId();
    /**
     * Returns resource ID of sublabel's font size.
     */
    int getSublabelFontSizeResId();
    /**
     * Returns whether the icon should be displayed at the start, before label
     * and sublabel.
     */
    boolean isIconAtStart();
    /**
     * Returns the resource ID of the icon's size, or 0 to use WRAP_CONTENT.
     */
    int getIconSizeResId();
    /**
     * Returns the resource ID of the icon's margin size.
     */
    int getIconMarginResId();
}
