// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.cardboard;

import static org.chromium.chromoting.cardboard.CardboardUtil.makeFloatBuffer;
import static org.chromium.chromoting.cardboard.CardboardUtil.makeRectangularTextureBuffer;

import android.graphics.Bitmap;
import android.graphics.Point;
import android.opengl.GLES20;

import org.chromium.chromoting.jni.JniInterface;

import java.nio.FloatBuffer;

/**
 * Chromoting Cardboard activity desktop, which is used to display host desktop.
 */
public class Desktop {
    private static final String VERTEX_SHADER =
            "uniform mat4 u_CombinedMatrix;"
            + "attribute vec4 a_Position;"
            + "attribute vec2 a_TexCoordinate;"
            + "varying vec2 v_TexCoordinate;"
            + "attribute float a_transparency;"
            + "varying float v_transparency;"
            + "void main() {"
            + "  v_transparency = a_transparency;"
            + "  v_TexCoordinate = a_TexCoordinate;"
            + "  gl_Position = u_CombinedMatrix * a_Position;"
            + "}";

    private static final String FRAGMENT_SHADER =
            "precision highp float;"
            + "uniform sampler2D u_Texture;"
            + "varying vec2 v_TexCoordinate;"
            + "const float borderWidth = 0.002;"
            + "varying float v_transparency;"
            + "void main() {"
            + "  if (v_TexCoordinate.x > (1.0 - borderWidth) || v_TexCoordinate.x < borderWidth"
            + "      || v_TexCoordinate.y > (1.0 - borderWidth)"
            + "      || v_TexCoordinate.y < borderWidth) {"
            + "    gl_FragColor = vec4(1.0, 1.0, 1.0, v_transparency);"
            + "  } else {"
            + "    vec4 texture = texture2D(u_Texture, v_TexCoordinate);"
            + "    gl_FragColor = vec4(texture.r, texture.g, texture.b, v_transparency);"
            + "  }"
            + "}";

    private static final FloatBuffer TEXTURE_COORDINATES = makeRectangularTextureBuffer(
            0.0f, 1.0f, 0.0f, 1.0f);

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
    private int mTransparentHandle;
    private int mTextureDataHandle;
    private int mTextureCoordinateHandle;
    private FloatBuffer mPosition;
    private float mHalfWidth;

    // Lock to allow multithreaded access to mHalfWidth.
    private final Object mHalfWidthLock = new Object();

    private Bitmap mVideoFrame;

    // Lock to allow multithreaded access to mVideoFrame.
    private final Object mVideoFrameLock = new Object();

    // Flag to indicate whether to reload the desktop texture.
    private boolean mReloadTexture;

    // Lock to allow multithreaded access to mReloadTexture.
    private final Object mReloadTextureLock = new Object();

    public Desktop() {
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
        mTransparentHandle = GLES20.glGetAttribLocation(mProgramHandle, "a_transparency");
        mTextureCoordinateHandle = GLES20.glGetAttribLocation(mProgramHandle, "a_TexCoordinate");
        mTextureDataHandle = TextureHelper.createTextureHandle();
    }

    /**
     * Draw the desktop. Make sure {@link hasVideoFrame} returns true.
     */
    public void draw(float[] combinedMatrix, boolean isTransparent) {
        GLES20.glUseProgram(mProgramHandle);

        // Pass in model view project matrix.
        GLES20.glUniformMatrix4fv(mCombinedMatrixHandle, 1, false, combinedMatrix, 0);

        // Pass in texture coordinate.
        GLES20.glVertexAttribPointer(mTextureCoordinateHandle, TEXTURE_COORDINATE_DATA_SIZE,
                GLES20.GL_FLOAT, false, 0, TEXTURE_COORDINATES);
        GLES20.glEnableVertexAttribArray(mTextureCoordinateHandle);

        GLES20.glVertexAttribPointer(mPositionHandle, POSITION_DATA_SIZE, GLES20.GL_FLOAT, false,
                0, mPosition);
        GLES20.glEnableVertexAttribArray(mPositionHandle);

        // Enable transparency.
        GLES20.glEnable(GLES20.GL_BLEND);
        GLES20.glBlendFunc(GLES20.GL_SRC_ALPHA, GLES20.GL_ONE_MINUS_SRC_ALPHA);

        // Pass in transparency.
        GLES20.glVertexAttrib1f(mTransparentHandle, isTransparent ? 0.5f : 1.0f);

        // Link texture data with texture unit.
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, mTextureDataHandle);
        GLES20.glUniform1i(mTextureUniformHandle, 0);

        GLES20.glDrawArrays(GLES20.GL_TRIANGLES, 0, VERTICES_NUMBER);

        GLES20.glDisable(GLES20.GL_BLEND);
        GLES20.glDisableVertexAttribArray(mPositionHandle);
        GLES20.glDisableVertexAttribArray(mTextureCoordinateHandle);
    }

    /**
     *  Update the desktop frame data based on the mVideoFrame. Note here we fix the
     *  height of the desktop and vary width accordingly.
     */
    private void updateVideoFrame(Bitmap videoFrame) {
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
            return new Point(mVideoFrame == null ? 0 : mVideoFrame.getWidth(),
                    mVideoFrame == null ? 0 : mVideoFrame.getHeight());
        }
    }

    /**
     * Link desktop texture if {@link reloadTexture} was previously called.
     * Invoked from {@link com.google.vrtoolkit.cardboard.CardboardView.StereoRenderer.onNewFrame}
     * so that both eyes will have the same texture.
     */
    public void maybeLoadDesktopTexture() {
        synchronized (mReloadTextureLock) {
            if (!mReloadTexture) {
                return;
            }
        }

        // TODO(shichengfeng): Record the time desktop drawing takes.
        Bitmap bitmap = JniInterface.getVideoFrame();

        if (bitmap == null) {
            // This can happen if the client is connected, but a complete video frame has not yet
            // been decoded.
            return;
        }

        updateVideoFrame(bitmap);

        synchronized (mReloadTextureLock) {
            mReloadTexture = false;
        }
    }

    /**
     * Inform this object that a new video frame should be rendered.
     * Called from native display thread.
     */
    public void reloadTexture() {
        synchronized (mReloadTextureLock) {
            mReloadTexture = true;
        }
    }
}