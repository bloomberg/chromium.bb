// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// height and width should match the dialog size in EmojiPickerDialog.
export const EMOJI_PICKER_HEIGHT = 454;
export const EMOJI_PICKER_WIDTH = 372;
export const EMOJI_ICON_SIZE = 28;
export const EMOJI_SPACING = 10;
export const EMOJI_PER_ROW = 9;
export const GROUP_PER_ROW = 9;
export const EMOJI_PICKER_SIDE_PADDING = 20;
export const EMOJI_PICKER_TOP_PADDING = 20;
export const GROUP_ICON_SIZE =
    (EMOJI_PICKER_WIDTH - 2 * EMOJI_PICKER_SIDE_PADDING) / EMOJI_PER_ROW;

export const EMOJI_PICKER_HEIGHT_PX = `${EMOJI_PICKER_HEIGHT}px`;
export const EMOJI_PICKER_WIDTH_PX = `${EMOJI_PICKER_WIDTH}px`;
export const EMOJI_SIZE_PX = `${EMOJI_ICON_SIZE}px`;
export const EMOJI_SPACING_PX = `${EMOJI_SPACING}px`;
export const EMOJI_GROUP_SIZE_PX = `${GROUP_ICON_SIZE}px`;
export const EMOJI_PICKER_SIDE_PADDING_PX = `${EMOJI_PICKER_SIDE_PADDING}px`;
export const EMOJI_PICKER_TOP_PADDING_PX = `${EMOJI_PICKER_TOP_PADDING}px`;
export const EMOJI_PICKER_TOTAL_EMOJI_WIDTH_PX =
    `${EMOJI_ICON_SIZE + EMOJI_SPACING}px`;
export const EMOJI_PICKER_TOTAL_EMOJI_WIDTH = EMOJI_ICON_SIZE + EMOJI_SPACING;
