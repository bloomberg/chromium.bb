// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import static org.chromium.chromoting.CardboardDesktopRenderer.makeFloatBuffer;

import android.graphics.Bitmap;
import android.graphics.Point;
import android.opengl.GLES20;

import java.nio.FloatBuffer;

/**
 * Chromoting Cardboard activity desktop, which is used to display host desktop.
 */
public class CardboardActivityDesktop {
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

    private static final FloatBuffer TEXTURE_COORDINATES = makeFloatBuffer(new float[] {
            // Texture coordinate data.
            0.0f, 0.0f,
            0.0f, 1.0f,
            1.0f, 0.0f,
            0.0f, 1.0f,
            1.0f, 1.0f,
            1.0f, 0.0f
    });

    private static final int POSITION_DATA_SIZE = 3;
    private static final int TEXTURE_COORDINATE_DATA_SIZE = 2;

    // Fix the desktop height and adjust width accordingly.
    private static final float HALF_HEIGHT = 1.0f;

    // Number of vertices passed to glDrawArrays().
    private static final int VERTICES_NUMBER = 6;

    private int mVertexShaderHandle;
    private int mFragmentShaderHandle;
    private int mProgramHandle;
    private int mCombinedMatrixHandle;
    private int mTextureUniformHandle;
    private int mPositionHandle;
    private int mTextureDataHandle;
    private int mTextureCoordinateHandle;
    private FloatBuffer mPosition;
    private float[] mCombinedMatrix;
    private float mHalfWidth;

    // Lock to allow multithreaded access to mHalfWidth.
    private final Object mHalfWidthLock = new Object();

    private Bitmap mVideoFrame;

    // Lock to allow multithreaded access to mVideoFrame.
    private final Object mVideoFrameLock = new Object();

    public CardboardActivityDesktop() {
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
        mTextureCoordinateHandle = GLES20.glGetAttribLocation(mProgramHandle, "a_TexCoordinate");
        mTextureDataHandle = TextureHelper.createTextureHandle();
    }

    /**
     * Draw the desktop. Make sure texture, position, and model view projection matrix
     * are passed in before calling this method.
     */
    public void draw() {
        GLES20.glUseProgram(mProgramHandle);

        // Pass in model view project matrix.
        GLES20.glUniformMatrix4fv(mCombinedMatrixHandle, 1, false, mCombinedMatrix, 0);

        // Pass in texture coordinate.
        GLES20.glVertexAttribPointer(mTextureCoordinateHandle, TEXTURE_COORDINATE_DATA_SIZE,
                GLES20.GL_FLOAT, false, 0, TEXTURE_COORDINATES);
        GLES20.glEnableVertexAttribArray(mTextureCoordinateHandle);

        GLES20.glVertexAttribPointer(mPositionHandle, POSITION_DATA_SIZE, GLES20.GL_FLOAT, false,
                0, mPosition);
        GLES20.glEnableVertexAttribArray(mPositionHandle);

        // Link texture data with texture unit.
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, mTextureDataHandle);
        GLES20.glUniform1i(mTextureUniformHandle, 0);

        GLES20.glDrawArrays(GLES20.GL_TRIANGLES, 0, VERTICES_NUMBER);
    }

    /**
     *  Update the desktop frame data based on the mVideoFrame. Note here we fix the
     *  height of the desktop and vary width accordingly.
     */
    public void updateVideoFrame(Bitmap videoFrame) {
        float newHalfDesktopWidth;
        synchronized (mVideoFrameLock) {
            mVideoFrame = videoFrame;
            TextureHelper.linkTexture(mTextureDataHandle, videoFrame);
            newHalfDesktopWidth = videoFrame.getWidth() * HALF_HEIGHT / videoFrame.getHeight();
        }

        synchronized (mHalfWidthLock) {
            if (Math.abs(mHalfWidth - newHalfDesktopWidth) > 0.0001) {
                mHalfWidth = newHalfDesktopWidth;
                mPosition = makeFloatBuffer(new float[] {
                        // Desktop model coordinates.
                        -mHalfWidth, HALF_HEIGHT, 0.0f,
                        -mHalfWidth, -HALF_HEIGHT, 0.0f,
                        mHalfWidth, HALF_HEIGHT, 0.0f,
                        -mHalfWidth, -HALF_HEIGHT, 0.0f,
                        mHalfWidth, -HALF_HEIGHT, 0.0f,
                        mHalfWidth, HALF_HEIGHT, 0.0f
                });
            }
        }
    }

    /**
     * Clean up opengl data.
     */
    public void cleanup() {
        GLES20.glDeleteShader(mVertexShaderHandle);
        GLES20.glDeleteShader(mFragmentShaderHandle);
        GLES20.glDeleteTextures(1, new int[] {mTextureDataHandle}, 0);
    }

    public void setCombinedMatrix(float[] combinedMatrix) {
        mCombinedMatrix = combinedMatrix.clone();
    }

    /**
     * Return true if video frame data are already loaded in.
     */
    public boolean hasVideoFrame() {
        synchronized (mVideoFrameLock) {
            return mVideoFrame != null;
        }
    }

    public float getHalfHeight() {
        return HALF_HEIGHT;
    }

    public float getHalfWidth() {
        synchronized (mHalfWidthLock) {
            return mHalfWidth;
        }
    }

    /**
     * Get desktop height and width in pixels.
     */
    public Point getFrameSizePixels() {
        synchronized (mVideoFrameLock) {
            return new Point(mVideoFrame == null ? 0 : mVideoFrame.getHeight(),
                    mVideoFrame == null ? 0 : mVideoFrame.getWidth());
        }
    }
}