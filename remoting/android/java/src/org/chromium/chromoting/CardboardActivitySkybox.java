// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import static org.chromium.chromoting.CardboardDesktopRenderer.makeFloatBuffer;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.opengl.GLES20;

import org.chromium.base.Log;

import java.nio.ByteBuffer;
import java.nio.FloatBuffer;

/**
 * Cardboard Activity skybox, which is used to draw the activity environment.
 */
public class CardboardActivitySkybox {
    private static final String TAG = "cr.CardboardSkybox";

    private static final String VERTEX_SHADER =
            "uniform mat4 u_CombinedMatrix;"
            + "attribute vec3 a_Position;"
            + "varying vec3 v_Position;"
            + "void main() {"
            + "  v_Position = a_Position;"
            // Make sure to convert from the right-handed coordinate system of the
            // world to the left-handed coordinate system of the cube map, otherwise,
            // our cube map will still work but everything will be flipped.
            + "  v_Position.z = -v_Position.z;"
            + "  gl_Position = u_CombinedMatrix * vec4(a_Position, 1.0);"
            + "  gl_Position = gl_Position.xyww;"
            + "}";

    private static final String FRAGMENT_SHADER =
            "precision mediump float;"
            + "uniform samplerCube u_TextureUnit;"
            + "varying vec3 v_Position;"
            + "void main() {"
            + "  gl_FragColor = textureCube(u_TextureUnit, v_Position);"
            + "}";

    private static final int POSITION_DATA_SIZE = 3;
    private static final float HALF_SKYBOX_SIZE = 100.0f;

    // Number of vertices passed to glDrawArrays().
    private static final int VERTICES_NUMBER = 36;

    private static final FloatBuffer POSITION_COORDINATES = makeFloatBuffer(new float[] {
            -HALF_SKYBOX_SIZE,  HALF_SKYBOX_SIZE,  HALF_SKYBOX_SIZE,  // (0) Top-left near
            HALF_SKYBOX_SIZE,  HALF_SKYBOX_SIZE,  HALF_SKYBOX_SIZE,  // (1) Top-right near
            -HALF_SKYBOX_SIZE, -HALF_SKYBOX_SIZE,  HALF_SKYBOX_SIZE,  // (2) Bottom-left near
            HALF_SKYBOX_SIZE, -HALF_SKYBOX_SIZE,  HALF_SKYBOX_SIZE,  // (3) Bottom-right near
            -HALF_SKYBOX_SIZE,  HALF_SKYBOX_SIZE, -HALF_SKYBOX_SIZE,  // (4) Top-left far
            HALF_SKYBOX_SIZE,  HALF_SKYBOX_SIZE, -HALF_SKYBOX_SIZE,  // (5) Top-right far
            -HALF_SKYBOX_SIZE, -HALF_SKYBOX_SIZE, -HALF_SKYBOX_SIZE,  // (6) Bottom-left far
            HALF_SKYBOX_SIZE, -HALF_SKYBOX_SIZE, -HALF_SKYBOX_SIZE  // (7) Bottom-right far
    });

    private static final ByteBuffer INDICES_BYTE_BUFFER = ByteBuffer.wrap(new byte[] {
            // Front
            1, 3, 0,
            0, 3, 2,

            // Back
            4, 6, 5,
            5, 6, 7,

            // Left
            0, 2, 4,
            4, 2, 6,

            // Right
            5, 7, 1,
            1, 7, 3,

            // Top
            5, 1, 4,
            4, 1, 0,

            // Bottom
            6, 2, 7,
            7, 2, 3
    });

    private static final String[] IMAGE_URIS = new String[] {
        "https://dl.google.com/chrome-remote-desktop/android-assets/room_left.png",
        "https://dl.google.com/chrome-remote-desktop/android-assets/room_right.png",
        "https://dl.google.com/chrome-remote-desktop/android-assets/room_bottom.png",
        "https://dl.google.com/chrome-remote-desktop/android-assets/room_top.png",
        "https://dl.google.com/chrome-remote-desktop/android-assets/room_back.png",
        "https://dl.google.com/chrome-remote-desktop/android-assets/room_front.png"
    };

    private static final String[] IMAGE_NAMES = new String[] {
        "skybox_left", "skybox_right", "skybox_bottom", "skybox_top", "skybox_back", "skybox_front"
    };

    private int mVertexShaderHandle;
    private int mFragmentShaderHandle;
    private int mProgramHandle;
    private int mCombinedMatrixHandle;
    private int mPositionHandle;
    private int mTextureDataHandle;
    private int mTextureUnitHandle;
    private Activity mActivity;

    // Flag to signal that the skybox images are fully decoded and should be loaded
    // into the OpenGL textures.
    private boolean mLoadTexture;

    // Lock to allow multithreaded access to mLoadTexture.
    private final Object mLoadTextureLock = new Object();

    ChromotingDownloadManager mDownloadManager;

    public CardboardActivitySkybox(Activity activity) {
        mActivity = activity;

        GLES20.glEnable(GLES20.GL_TEXTURE_CUBE_MAP);
        mVertexShaderHandle =
                ShaderHelper.compileShader(GLES20.GL_VERTEX_SHADER, VERTEX_SHADER);
        mFragmentShaderHandle =
                ShaderHelper.compileShader(GLES20.GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
        mProgramHandle = ShaderHelper.createAndLinkProgram(mVertexShaderHandle,
                mFragmentShaderHandle, new String[] {"a_Position", "u_CombinedMatrix",
                    "u_TextureUnit"});
        mPositionHandle =
                GLES20.glGetAttribLocation(mProgramHandle, "a_Position");
        mCombinedMatrixHandle =
                GLES20.glGetUniformLocation(mProgramHandle, "u_CombinedMatrix");
        mTextureUnitHandle =
                GLES20.glGetUniformLocation(mProgramHandle, "u_TextureUnit");
        mTextureDataHandle = TextureHelper.createTextureHandle();

        // Download the skybox images.
        mDownloadManager = new ChromotingDownloadManager(mActivity, IMAGE_NAMES,
                IMAGE_URIS, new ChromotingDownloadManager.Callback() {
                    @Override
                    public void onBatchDownloadComplete() {
                        synchronized (mLoadTextureLock) {
                            mLoadTexture = true;
                        }
                    }
                });
        mDownloadManager.download();
    }

    /**
     * Set the textures for skybox and clean temporary decoded images at the end.
     * Only call this method when we have complete skybox images.
     */
    public void maybeLoadTextureAndCleanImages() {
        synchronized (mLoadTextureLock) {
            if (!mLoadTexture) {
                return;
            }
            mLoadTexture = false;
        }

        Bitmap[] images;
        try {
            images = decodeSkyboxImages();
        } catch (DecodeFileException e) {
            Log.i(TAG, "Failed to decode image files.");
            return;
        }
        TextureHelper.linkCubeMap(mTextureDataHandle, images);

        for (Bitmap image : images) {
            image.recycle();
        }
    }

    /**
    * Draw the skybox. Make sure texture, position, and model view projection matrix
    * are passed in before calling this method.
    */
    public void draw(float[] combinedMatrix) {
        GLES20.glUseProgram(mProgramHandle);

        GLES20.glUniformMatrix4fv(mCombinedMatrixHandle, 1, false,
                combinedMatrix, 0);

        GLES20.glVertexAttribPointer(mPositionHandle, POSITION_DATA_SIZE, GLES20.GL_FLOAT,
                false, 0, POSITION_COORDINATES);
        GLES20.glEnableVertexAttribArray(mPositionHandle);

        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_CUBE_MAP, mTextureDataHandle);
        GLES20.glUniform1i(mTextureUnitHandle, 0);

        GLES20.glDrawElements(GLES20.GL_TRIANGLES, VERTICES_NUMBER, GLES20.GL_UNSIGNED_BYTE,
                INDICES_BYTE_BUFFER);
    }

    /**
     * Clean up opengl data.
     */
    public void cleanup() {
        GLES20.glDeleteShader(mVertexShaderHandle);
        GLES20.glDeleteShader(mFragmentShaderHandle);
        GLES20.glDeleteTextures(1, new int[] {mTextureDataHandle}, 0);

        mActivity.runOnUiThread(new Runnable() {
            public void run() {
                mDownloadManager.close();
            }
        });
    }

    /**
     * Decode all skybox images to Bitmap files and return them.
     * Only call this method when we have complete skybox images.
     * @throws DecodeFileException if BitmapFactory fails to decode file.
     */
    private Bitmap[] decodeSkyboxImages() throws DecodeFileException {
        Bitmap[] result = new Bitmap[IMAGE_NAMES.length];
        String fileDirectory = mDownloadManager.getDownloadDirectory();
        for (int i = 0; i < IMAGE_NAMES.length; i++) {
            result[i] = BitmapFactory.decodeFile(fileDirectory + "/" + IMAGE_NAMES[i]);
            if (result[i] == null) {
                throw new DecodeFileException();
            }
        }
        return result;
    }

    /**
     * Exception when BitmapFactory fails to decode file.
     */
    private static class DecodeFileException extends Exception {
    }
}