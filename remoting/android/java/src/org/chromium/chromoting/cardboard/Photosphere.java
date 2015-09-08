// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.cardboard;

import static org.chromium.chromoting.cardboard.CardboardUtil.makeFloatBuffer;
import static org.chromium.chromoting.cardboard.CardboardUtil.makeShortBuffer;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.opengl.GLES20;

import org.chromium.base.Log;
import org.chromium.chromoting.ChromotingDownloadManager;

import java.nio.FloatBuffer;
import java.nio.ShortBuffer;

/**
 * Cardboard Activity photosphere, which is used to draw the activity environment.
 */
public class Photosphere {
    private static final String TAG = "cr.Photosphere";

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

    private int mVertexShaderHandle;
    private int mFragmentShaderHandle;
    private int mProgramHandle;
    private int mCombinedMatrixHandle;
    private int mPositionHandle;
    private int mTextureDataHandle;
    private int mTextureCoordinateHandle;
    private int mTextureUniformHandle;

    private static final float RADIUS = 100.0f;

    // Subdivision of the texture.
    // Note: (COLUMS + 1) * (ROWS + 1), which is the number of vertices, should be within the
    // range of short.
    private static final int COLUMNS = 100;
    private static final int ROWS = 50;

    private static final int POSITION_DATA_SIZE = 3;
    private static final int TEXTURE_COORDINATE_DATA_SIZE = 2;

    // Number of drawing vertices needed to draw a rectangle surface.
    private static final int NUM_DRAWING_VERTICES_PER_RECT = 6;

    private static final int NUM_VERTICES = (ROWS + 1) * (COLUMNS + 1);

    // Number of drawing vertices passed to glDrawElements().
    private static final int NUM_DRAWING_VERTICES = ROWS * COLUMNS * NUM_DRAWING_VERTICES_PER_RECT;

    // TODO(shichengfeng): Find a photosphere image and upload it.
    private static final String IMAGE_URI =
            "https://dl.google.com/chrome-remote-desktop/android-assets/room_left.png";
    private static final String IMAGE_NAME = "photosphere";

    private static final ShortBuffer INDICES_BUFFER = calculateIndicesBuffer();

    private FloatBuffer mPositionBuffer;
    private FloatBuffer mTextureCoordinatesBuffer;

    private Activity mActivity;

    private String mDownloadDirectory;

    // Lock to allow multithreaded access to mDownloadDirectory.
    private final Object mDownloadDirectoryLock = new Object();

    // Flag to signal that the image is fully decoded and should be loaded
    // into the OpenGL textures.
    private boolean mLoadTexture;

    // Lock to allow multithreaded access to mLoadTexture.
    private final Object mLoadTextureLock = new Object();

    ChromotingDownloadManager mDownloadManager;

    public Photosphere(Activity activity) {
        mActivity = activity;

        calculatePosition();
        calculateIndicesBuffer();

        // Set handlers for eye point drawing.
        mVertexShaderHandle =
                ShaderHelper.compileShader(GLES20.GL_VERTEX_SHADER, VERTEX_SHADER);
        mFragmentShaderHandle =
                ShaderHelper.compileShader(GLES20.GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
        mProgramHandle = ShaderHelper.createAndLinkProgram(mVertexShaderHandle,
                mFragmentShaderHandle, new String[] {"a_Position", "u_CombinedMatrix",
                    "a_TexCoordinate", "u_Texture"});
        mPositionHandle =
                GLES20.glGetAttribLocation(mProgramHandle, "a_Position");
        mCombinedMatrixHandle =
                GLES20.glGetUniformLocation(mProgramHandle, "u_CombinedMatrix");
        mTextureCoordinateHandle =
                GLES20.glGetAttribLocation(mProgramHandle, "a_TexCoordinate");
        mTextureUniformHandle =
                GLES20.glGetUniformLocation(mProgramHandle, "u_TextureUnit");
        mTextureDataHandle = TextureHelper.createTextureHandle();

        mActivity.runOnUiThread(new Runnable() {
            public void run() {
                // Download the phtosphere image.
                mDownloadManager = new ChromotingDownloadManager(mActivity, IMAGE_NAME,
                        IMAGE_URI, new ChromotingDownloadManager.Callback() {
                            @Override
                            public void onBatchDownloadComplete() {
                                synchronized (mLoadTextureLock) {
                                    mLoadTexture = true;
                                }
                            }
                        });
                synchronized (mDownloadDirectoryLock) {
                    mDownloadDirectory = mDownloadManager.getDownloadDirectory();
                }
                mDownloadManager.download();
            }
        });
    }

    /**
     * Set the texture for photosphere and clean temporary decoded image at the end.
     * Only call this method when we have completely downloaded photosphere image.
     */
    public void maybeLoadTextureAndCleanImage() {
        synchronized (mLoadTextureLock) {
            if (!mLoadTexture) {
                return;
            }
            mLoadTexture = false;
        }

        Bitmap image;
        synchronized (mDownloadDirectoryLock) {
            // This will only be executed when download finishes, which runs on main thread.
            // On the main thread, we first initialize mDownloadDirectory and then start download,
            // so it is safe to use mDownloadDirectory here.
            image = BitmapFactory.decodeFile(mDownloadDirectory + "/" + IMAGE_NAME);
        }

        if (image == null) {
            Log.i(TAG, "Failed to decode image files.");
            return;
        }

        TextureHelper.linkTexture(mTextureDataHandle, image);
        image.recycle();
    }

    public void draw(float[] combinedMatrix) {
        GLES20.glUseProgram(mProgramHandle);

        // Pass in model view project matrix.
        GLES20.glUniformMatrix4fv(mCombinedMatrixHandle, 1, false, combinedMatrix, 0);

        GLES20.glVertexAttribPointer(mPositionHandle, POSITION_DATA_SIZE, GLES20.GL_FLOAT,
                false, 0, mPositionBuffer);
        GLES20.glEnableVertexAttribArray(mPositionHandle);

        // Pass in texture coordinate.
        GLES20.glVertexAttribPointer(mTextureCoordinateHandle, TEXTURE_COORDINATE_DATA_SIZE,
                GLES20.GL_FLOAT, false, 0, mTextureCoordinatesBuffer);
        GLES20.glEnableVertexAttribArray(mTextureCoordinateHandle);

        // Link texture data with texture unit.
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, mTextureDataHandle);
        GLES20.glUniform1i(mTextureUniformHandle, 0);

        GLES20.glDrawElements(GLES20.GL_TRIANGLES, NUM_DRAWING_VERTICES, GLES20.GL_UNSIGNED_SHORT,
                INDICES_BUFFER);

        GLES20.glDisableVertexAttribArray(mPositionHandle);
        GLES20.glDisableVertexAttribArray(mTextureCoordinateHandle);
    }

    /**
     * Calculate the sphere position coordinates and texture coordinates.
     */
    private void calculatePosition() {
        float[] vertexCoordinates = new float[NUM_VERTICES * POSITION_DATA_SIZE];
        float[] textureCoordinates = new float[NUM_VERTICES * TEXTURE_COORDINATE_DATA_SIZE];

        int vertexIndex = 0;
        int textureCoordinateIndex = 0;
        for (int row = 0; row <= ROWS; row++) {
            for (int column = 0; column <= COLUMNS; column++) {
                double theta = (ROWS / 2.0 - row) / (ROWS / 2.0) * Math.PI / 2;
                double phi = 2 * Math.PI * column / COLUMNS;
                float x = (float) (RADIUS * Math.cos(theta) * Math.cos(phi));
                float y = (float) (RADIUS * Math.sin(theta));
                float z = (float) (RADIUS * Math.cos(theta) * Math.sin(phi));

                vertexCoordinates[vertexIndex++] = x;
                vertexCoordinates[vertexIndex++] = y;
                vertexCoordinates[vertexIndex++] = z;

                textureCoordinates[textureCoordinateIndex++] = (float) column / COLUMNS;
                textureCoordinates[textureCoordinateIndex++] = (float) row / ROWS;
            }
        }

        mPositionBuffer = makeFloatBuffer(vertexCoordinates);
        mTextureCoordinatesBuffer = makeFloatBuffer(textureCoordinates);
    }

    /**
     * Calculate the drawing indices buffer.
     */
    private static ShortBuffer calculateIndicesBuffer() {
        short[] indices = new short[NUM_DRAWING_VERTICES];
        int currentIndex = 0;
        for (int row = 0; row < ROWS; row++) {
            for (int column = 0; column < COLUMNS; column++) {
                int topLeft = row * (COLUMNS + 1) + column;
                int topRight = topLeft + 1;
                int bottomLeft = topLeft + (COLUMNS + 1);
                int bottomRight = bottomLeft + 1;
                indices[currentIndex++] = (short) topLeft;
                indices[currentIndex++] = (short) bottomLeft;
                indices[currentIndex++] = (short) topRight;
                indices[currentIndex++] = (short) bottomLeft;
                indices[currentIndex++] = (short) bottomRight;
                indices[currentIndex++] = (short) topRight;
            }
        }
        return makeShortBuffer(indices);
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
}
