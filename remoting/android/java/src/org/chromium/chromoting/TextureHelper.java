// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.graphics.Bitmap;
import android.opengl.GLES20;
import android.opengl.GLUtils;

/**
 * Helper class for working with OpenGL textures.
 */
public class TextureHelper {
    /**
     * Create new texture handle.
     * @return New texture handle.
     */
    public static int createTextureHandle() {
        int[] textureDataHandle = new int[1];
        GLES20.glGenTextures(1, textureDataHandle, 0);
        if (textureDataHandle[0] != 0) {
            return textureDataHandle[0];
        } else {
            throw new RuntimeException("Error generating texture handle.");
        }
    }

    /**
     * Link desktop texture with a handle.
     * @param textureDataHandle the handle to attach texture to
     */
    public static void linkTexture(int textureDataHandle, Bitmap bitmap) {
        // Delete previously attached texture.
        GLES20.glDeleteTextures(1, new int[]{textureDataHandle}, 0);

        // Bind to the texture in OpenGL.
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureDataHandle);

        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,
                GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_NEAREST);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,
                GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_NEAREST);

        // Load the bitmap into the bound texture.
        GLUtils.texImage2D(GLES20.GL_TEXTURE_2D, 0, bitmap, 0);
    }
}