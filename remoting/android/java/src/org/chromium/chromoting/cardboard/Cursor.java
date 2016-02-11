// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.cardboard;

import static org.chromium.chromoting.cardboard.CardboardUtil.makeFloatBuffer;
import static org.chromium.chromoting.cardboard.CardboardUtil.makeRectangularTextureBuffer;

import android.graphics.Bitmap;
import android.graphics.Point;
import android.graphics.PointF;
import android.opengl.GLES20;

import org.chromium.chromoting.TouchInputHandler;
import org.chromium.chromoting.jni.Client;

import java.nio.FloatBuffer;

/**
 * Cardboard activity desktop cursor that is used to render the image of the mouse
 * cursor onto the desktop.
 */
public class Cursor {
    private static final String VERTEX_SHADER =
            "uniform mat4 u_CombinedMatrix;"
            + "attribute vec4 a_Position;"
            + "attribute vec2 a_TexCoordinate;"
            + "varying vec2 v_TexCoordinate;"
            + "void main() {"
            + "  v_TexCoordinate = a_TexCoordinate;"
            + "  gl_Position = u_CombinedMatrix * a_Position;"
            + "}";

    private static final String FRAGMENT_SHADER =
            "precision mediump float;"
            + "uniform sampler2D u_Texture;"
            + "varying vec2 v_TexCoordinate;"
            + "void main() {"
            + "  gl_FragColor = texture2D(u_Texture, v_TexCoordinate);"
            + "}";

    private static final FloatBuffer TEXTURE_COORDINATES = makeRectangularTextureBuffer(
            0.0f, 1.0f, 0.0f, 1.0f);

    private static final int POSITION_COORDINATE_DATA_SIZE = 3;
    private static final int TEXTURE_COORDINATE_DATA_SIZE = 2;

    // Number of vertices passed to glDrawArrays().
    private static final int VERTICES_NUMBER = 6;

    // Threshold to determine whether to send the mouse move event.
    private static final float CURSOR_MOVE_THRESHOLD = 1.0f;

    private final Client mClient;

    private FloatBuffer mPositionCoordinates;

    private int mVertexShaderHandle;
    private int mFragmentShaderHandle;
    private int mProgramHandle;
    private int mCombinedMatrixHandle;
    private int mTextureUniformHandle;
    private int mPositionHandle;
    private int mTextureDataHandle;
    private int mTextureCoordinateHandle;

    // Flag to indicate whether to reload the desktop texture.
    private boolean mReloadTexture;

    // Lock to allow multithreaded access to mReloadTexture.
    private final Object mReloadTextureLock = new Object();

    private Bitmap mCursorBitmap;

    // Half width and half height of the cursor.
    private PointF mHalfFrameSize;

    private PointF mCursorPosition;

    public Cursor(Client client) {
        mClient = client;
        mHalfFrameSize = new PointF(0.0f, 0.0f);
        mCursorPosition = new PointF(0.0f, 0.0f);

        mVertexShaderHandle =
                ShaderHelper.compileShader(GLES20.GL_VERTEX_SHADER, VERTEX_SHADER);
        mFragmentShaderHandle =
                ShaderHelper.compileShader(GLES20.GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
        mProgramHandle = ShaderHelper.createAndLinkProgram(mVertexShaderHandle,
                mFragmentShaderHandle, new String[] {"a_Position", "a_TexCoordinate",
                    "u_CombinedMatrix", "u_Texture"});

        mCombinedMatrixHandle =
                GLES20.glGetUniformLocation(mProgramHandle, "u_CombinedMatrix");
        mTextureUniformHandle = GLES20.glGetUniformLocation(mProgramHandle, "u_Texture");
        mPositionHandle = GLES20.glGetAttribLocation(mProgramHandle, "a_Position");
        mTextureDataHandle = TextureHelper.createTextureHandle();
        mTextureCoordinateHandle = GLES20.glGetAttribLocation(mProgramHandle, "a_TexCoordinate");
    }

    /**
     * Inform this object that a new cursor should be rendered.
     * Called from native display thread.
     */
    public void reloadTexture() {
        synchronized (mReloadTextureLock) {
            mReloadTexture = true;
        }
    }

    /**
     * Return whether to move cursor. We only want to send the mouse move event
     * when there is obvious change for cursor's location.
     */
    private boolean moveCursor(PointF position) {
        return Math.abs(mCursorPosition.x - position.x) > CURSOR_MOVE_THRESHOLD
                || Math.abs(mCursorPosition.y - position.y) > CURSOR_MOVE_THRESHOLD;
    }

    /**
     * Send the mouse move event to host when {@link moveCursor} returns true.
     */
    public void moveTo(PointF position) {
        if (moveCursor(position)) {
            mClient.sendMouseEvent((int) position.x, (int) position.y,
                    TouchInputHandler.BUTTON_UNDEFINED, false);
        }
        mCursorPosition = position;
    }

    /**
     * Link the texture data for cursor if {@link mReloadTexture} is true.
     * Invoked from {@link com.google.vrtoolkit.cardboard.CardboardView.StereoRenderer.onNewFrame}
     */
    public void maybeLoadTexture(Desktop desktop) {
        synchronized (mReloadTextureLock) {
            if (!mReloadTexture || !desktop.hasVideoFrame()) {
                return;
            }
        }

        Bitmap cursorBitmap = mClient.getCursorBitmap();

        if (cursorBitmap == mCursorBitmap) {
            // Case when cursor image has not changed.
            synchronized (mReloadTextureLock) {
                mReloadTexture = false;
            }
            return;
        }

        mCursorBitmap = cursorBitmap;
        updatePosition(desktop, mCursorBitmap, mClient.getCursorHotspot());

        TextureHelper.linkTexture(mTextureDataHandle, cursorBitmap);

        synchronized (mReloadTextureLock) {
            mReloadTexture = false;
        }
    }

    public boolean hasImageFrame() {
        return mCursorBitmap != null;
    }

    /**
     * Update the cursor position data if the cursor changes.
     */
    private void updatePosition(Desktop desktop, Bitmap cursor, Point hotspot) {
        Point desktopFrameSizePixels = desktop.getFrameSizePixels();
        float newHalfWidth = desktop.getHalfWidth() / desktopFrameSizePixels.x
                * cursor.getWidth();
        float newHalfHeight = desktop.getHalfHeight() / desktopFrameSizePixels.y
                * cursor.getHeight();

        if (Math.abs(mHalfFrameSize.x - newHalfWidth) > 0.00001
                || Math.abs(mHalfFrameSize.y - newHalfHeight) > 0.00001) {
            // Put the hotspot as the center of the cursor.
            float hotspotX = (float) hotspot.x / desktopFrameSizePixels.x;
            float hotspotY = (float) hotspot.y / desktopFrameSizePixels.y;
            float left = -newHalfWidth + hotspotX;
            float right = newHalfWidth + hotspotX;
            float bottom = -newHalfHeight - hotspotY;
            float top = newHalfHeight - hotspotY;

            mPositionCoordinates = makeFloatBuffer(new float[] {
                // Desktop model coordinates.
                left, top, 0.0f,
                left, bottom, 0.0f,
                right, top, 0.0f,
                left, bottom, 0.0f,
                right, bottom, 0.0f,
                right, top, 0.0f
            });

            mHalfFrameSize = new PointF(newHalfWidth, newHalfHeight);
        }
    }

    /**
     * Draw menu item according to the given model view projection matrix.
     */
    public void draw(float[] combinedMatrix) {
        GLES20.glUseProgram(mProgramHandle);

        GLES20.glClear(GLES20.GL_DEPTH_BUFFER_BIT);

        // Pass in model view project matrix.
        GLES20.glUniformMatrix4fv(mCombinedMatrixHandle, 1, false, combinedMatrix, 0);

        // Pass in model position.
        GLES20.glVertexAttribPointer(mPositionHandle, POSITION_COORDINATE_DATA_SIZE,
                GLES20.GL_FLOAT, false, 0, mPositionCoordinates);
        GLES20.glEnableVertexAttribArray(mPositionHandle);

        // Pass in texture coordinate.
        GLES20.glVertexAttribPointer(mTextureCoordinateHandle, TEXTURE_COORDINATE_DATA_SIZE,
                GLES20.GL_FLOAT, false, 0, TEXTURE_COORDINATES);
        GLES20.glEnableVertexAttribArray(mTextureCoordinateHandle);

        // Enable the transparent background.
        GLES20.glEnable(GLES20.GL_BLEND);
        GLES20.glBlendFunc(GLES20.GL_SRC_ALPHA, GLES20.GL_ONE_MINUS_SRC_ALPHA);

        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, mTextureDataHandle);
        GLES20.glUniform1i(mTextureUniformHandle, 0);

        GLES20.glDrawArrays(GLES20.GL_TRIANGLES, 0, VERTICES_NUMBER);

        GLES20.glDisable(GLES20.GL_BLEND);
        GLES20.glDisableVertexAttribArray(mPositionHandle);
        GLES20.glDisableVertexAttribArray(mTextureCoordinateHandle);
    }

    /*
     * Clean cursor related opengl data.
     */
    public void cleanup() {
        GLES20.glDeleteShader(mVertexShaderHandle);
        GLES20.glDeleteShader(mFragmentShaderHandle);
        GLES20.glDeleteTextures(1, new int[] {mTextureDataHandle}, 0);
    }
}
