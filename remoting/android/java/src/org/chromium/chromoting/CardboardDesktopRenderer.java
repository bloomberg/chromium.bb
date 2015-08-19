// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.PointF;
import android.opengl.GLES20;
import android.opengl.Matrix;

import com.google.vrtoolkit.cardboard.CardboardView;
import com.google.vrtoolkit.cardboard.Eye;
import com.google.vrtoolkit.cardboard.HeadTransform;
import com.google.vrtoolkit.cardboard.Viewport;

import org.chromium.base.Log;
import org.chromium.chromoting.jni.JniInterface;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

import javax.microedition.khronos.egl.EGLConfig;

/**
 * Renderer for Cardboard view.
 */
public class CardboardDesktopRenderer implements CardboardView.StereoRenderer {
    private static final String TAG = "cr.CardboardRenderer";

    private static final int BYTE_PER_FLOAT = 4;
    private static final int POSITION_DATA_SIZE = 3;
    private static final int TEXTURE_COORDINATE_DATA_SIZE = 2;
    private static final float Z_NEAR = 0.1f;
    private static final float Z_FAR = 100.0f;
    private static final float DESKTOP_POSITION_X = 0.0f;
    private static final float DESKTOP_POSITION_Y = 0.0f;
    private static final float DESKTOP_POSITION_Z = -2.0f;
    private static final float HALF_SKYBOX_SIZE = 100.0f;
    private static final float VIEW_POSITION_MIN = -1.0f;
    private static final float VIEW_POSITION_MAX = 3.0f;

    // Allows user to click even when looking outside the desktop
    // but within edge margin.
    private static final float EDGE_MARGIN = 0.1f;

    // Fix the desktop height and adjust width accordingly.
    private static final float HALF_DESKTOP_HEIGHT = 1.0f;

    // Distance to move camera each time.
    private static final float CAMERA_MOTION_STEP = 0.5f;

    private static final FloatBuffer DESKTOP_TEXTURE_COORDINATES = makeFloatBuffer(new float[] {
            // Texture coordinate data.
            0.0f, 0.0f,
            0.0f, 1.0f,
            1.0f, 0.0f,
            0.0f, 1.0f,
            1.0f, 1.0f,
            1.0f, 0.0f
    });

    private static final FloatBuffer SKYBOX_POSITION_COORDINATES = makeFloatBuffer(new float[] {
            -HALF_SKYBOX_SIZE,  HALF_SKYBOX_SIZE,  HALF_SKYBOX_SIZE,  // (0) Top-left near
            HALF_SKYBOX_SIZE,  HALF_SKYBOX_SIZE,  HALF_SKYBOX_SIZE,  // (1) Top-right near
            -HALF_SKYBOX_SIZE, -HALF_SKYBOX_SIZE,  HALF_SKYBOX_SIZE,  // (2) Bottom-left near
            HALF_SKYBOX_SIZE, -HALF_SKYBOX_SIZE,  HALF_SKYBOX_SIZE,  // (3) Bottom-right near
            -HALF_SKYBOX_SIZE,  HALF_SKYBOX_SIZE, -HALF_SKYBOX_SIZE,  // (4) Top-left far
            HALF_SKYBOX_SIZE,  HALF_SKYBOX_SIZE, -HALF_SKYBOX_SIZE,  // (5) Top-right far
            -HALF_SKYBOX_SIZE, -HALF_SKYBOX_SIZE, -HALF_SKYBOX_SIZE,  // (6) Bottom-left far
            HALF_SKYBOX_SIZE, -HALF_SKYBOX_SIZE, -HALF_SKYBOX_SIZE  // (7) Bottom-right far
    });

    private static final ByteBuffer SKYBOX_INDICES_BYTE_BUFFER = ByteBuffer.wrap(new byte[] {
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

    private static final String DESKTOP_VERTEX_SHADER =
            "uniform mat4 u_CombinedMatrix;"
            + "attribute vec4 a_Position;"
            + "attribute vec2 a_TexCoordinate;"
            + "varying vec2 v_TexCoordinate;"
            + "void main() {"
            + "  v_TexCoordinate = a_TexCoordinate;"
            + "  gl_Position = u_CombinedMatrix * a_Position;"
            + "}";

    private static final String DESKTOP_FRAGMENT_SHADER =
            "precision highp float;"
            + "uniform sampler2D u_Texture;"
            + "varying vec2 v_TexCoordinate;"
            + "const float borderWidth = 0.002;"
            + "void main() {"
            + "  if (v_TexCoordinate.x > (1.0 - borderWidth) || v_TexCoordinate.x < borderWidth"
            + "      || v_TexCoordinate.y > (1.0 - borderWidth)"
            + "      || v_TexCoordinate.y < borderWidth) {"
            + "    gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);"
            + "  } else {"
            + "    gl_FragColor = texture2D(u_Texture, v_TexCoordinate);"
            + "  }"
            + "}";

    private static final String SKYBOX_VERTEX_SHADER =
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

    private static final String SKYBOX_FRAGMENT_SHADER =
            "precision mediump float;"
            + "uniform samplerCube u_TextureUnit;"
            + "varying vec3 v_Position;"
            + "void main() {"
            + "  gl_FragColor = textureCube(u_TextureUnit, v_Position);"
            + "}";

    private static final String ASSETS_URI_PREFIX =
            "https://dl.google.com/chrome-remote-desktop/android-assets/";;

    private static final String[] SKYBOX_IMAGE_URIS = new String[] {
        "https://dl.google.com/chrome-remote-desktop/android-assets/room_left.png",
        "https://dl.google.com/chrome-remote-desktop/android-assets/room_right.png",
        "https://dl.google.com/chrome-remote-desktop/android-assets/room_bottom.png",
        "https://dl.google.com/chrome-remote-desktop/android-assets/room_top.png",
        "https://dl.google.com/chrome-remote-desktop/android-assets/room_back.png",
        "https://dl.google.com/chrome-remote-desktop/android-assets/room_front.png"
    };

    private static final String[] SKYBOX_IMAGE_NAMES = new String[] {
        "skybox_left", "skybox_right", "skybox_bottom", "skybox_top", "skybox_back", "skybox_front"
    };

    private final Activity mActivity;

    private float mHalfDesktopWidth;
    private float mCameraPosition;

    // Lock to allow multithreaded access to mCameraPosition.
    private Object mCameraPositionLock = new Object();

    private float[] mCameraMatrix;
    private float[] mViewMatrix;
    private float[] mProjectionMatrix;

    // Make matrix member variable to avoid unnecessary initialization.
    private float[] mDesktopModelMatrix;
    private float[] mDesktopCombinedMatrix;
    private float[] mEyePointModelMatrix;
    private float[] mEyePointCombinedMatrix;
    private float[] mSkyboxModelMatrix;
    private float[] mSkyboxCombinedMatrix;

    // Direction that user is looking towards.
    private float[] mForwardVector;

    // Eye position in desktop.
    private float[] mEyePositionVector;

    private int mDesktopCombinedMatrixHandle;
    private int mPositionHandle;
    private int mTextureDataHandle;
    private int mTextureUniformHandle;
    private int mTextureCoordinateHandle;
    private int mProgramHandle;
    private int mDesktopVertexShaderHandle;
    private int mDesktopFragmentShaderHandle;
    private int mSkyboxVertexShaderHandle;
    private int mSkyboxFragmentShaderHandle;
    private int mSkyboxProgramHandle;
    private int mSkyboxPositionHandle;
    private int mSkyboxCombinedMatrixHandle;
    private int mSkyboxTextureUnitHandle;
    private int mSkyboxTextureDataHandle;
    private CardboardActivityEyePoint mEyePoint;

    // Flag to indicate whether reload the desktop texture or not.
    private boolean mReloadTexture;

    /** Lock to allow multithreaded access to mReloadTexture. */
    private Object mReloadTextureLock = new Object();

    // Lock for eye position related operations.
    // This protects access to mEyePositionVector as well as mDesktop{Height/Width}Pixels.
    private Object mEyePositionLock = new Object();

    private int mDesktopHeightPixels;
    private int mDesktopWidthPixels;

    private FloatBuffer mDesktopCoordinates;

    // Flag to signal that the skybox images are fully decoded and should be loaded
    // into the OpenGL textures.
    private boolean mLoadSkyboxImagesTexture;

    // Lock to allow multithreaded access to mLoadSkyboxImagesTexture.
    private Object mLoadSkyboxImagesTextureLock = new Object();

    private ChromotingDownloadManager mDownloadManager;

    public CardboardDesktopRenderer(Activity activity) {
        mActivity = activity;
        mReloadTexture = false;
        mCameraPosition = 0.0f;

        mCameraMatrix = new float[16];
        mViewMatrix = new float[16];
        mProjectionMatrix = new float[16];
        mDesktopModelMatrix = new float[16];
        mDesktopCombinedMatrix = new float[16];
        mEyePointModelMatrix = new float[16];
        mEyePointCombinedMatrix = new float[16];
        mSkyboxModelMatrix = new float[16];
        mSkyboxCombinedMatrix = new float[16];

        mForwardVector = new float[3];
        mEyePositionVector = new float[3];

        attachRedrawCallback();

        mDownloadManager = new ChromotingDownloadManager(mActivity, SKYBOX_IMAGE_NAMES,
                SKYBOX_IMAGE_URIS, new ChromotingDownloadManager.Callback() {
                    @Override
                    public void onBatchDownloadComplete() {
                        synchronized (mLoadSkyboxImagesTextureLock) {
                            mLoadSkyboxImagesTexture = true;
                        }
                    }
                });
        mDownloadManager.download();
    }

    // This can be called on any thread.
    public void attachRedrawCallback() {
        JniInterface.provideRedrawCallback(new Runnable() {
            @Override
            public void run() {
                synchronized (mReloadTextureLock) {
                    mReloadTexture = true;
                }
            }
        });
    }

    @Override
    public void onSurfaceCreated(EGLConfig config) {
        // Set the background clear color to black.
        GLES20.glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

        // Use culling to remove back faces.
        GLES20.glEnable(GLES20.GL_CULL_FACE);

        // Enable depth testing.
        GLES20.glEnable(GLES20.GL_DEPTH_TEST);

        // Set handles for desktop drawing.
        mDesktopVertexShaderHandle =
                ShaderHelper.compileShader(GLES20.GL_VERTEX_SHADER, DESKTOP_VERTEX_SHADER);
        mDesktopFragmentShaderHandle =
                ShaderHelper.compileShader(GLES20.GL_FRAGMENT_SHADER, DESKTOP_FRAGMENT_SHADER);
        mProgramHandle = ShaderHelper.createAndLinkProgram(mDesktopVertexShaderHandle,
                mDesktopFragmentShaderHandle, new String[] {"a_Position", "a_TexCoordinate"});
        mDesktopCombinedMatrixHandle =
                GLES20.glGetUniformLocation(mProgramHandle, "u_CombinedMatrix");
        mTextureUniformHandle = GLES20.glGetUniformLocation(mProgramHandle, "u_Texture");
        mPositionHandle = GLES20.glGetAttribLocation(mProgramHandle, "a_Position");
        mTextureCoordinateHandle = GLES20.glGetAttribLocation(mProgramHandle, "a_TexCoordinate");
        mTextureDataHandle = TextureHelper.createTextureHandle();

        // Set handlers for skybox drawing.
        GLES20.glEnable(GLES20.GL_TEXTURE_CUBE_MAP);
        mSkyboxVertexShaderHandle =
                ShaderHelper.compileShader(GLES20.GL_VERTEX_SHADER, SKYBOX_VERTEX_SHADER);
        mSkyboxFragmentShaderHandle =
                ShaderHelper.compileShader(GLES20.GL_FRAGMENT_SHADER, SKYBOX_FRAGMENT_SHADER);
        mSkyboxProgramHandle = ShaderHelper.createAndLinkProgram(mSkyboxVertexShaderHandle,
                mSkyboxFragmentShaderHandle,
                new String[] {"a_Position", "u_CombinedMatrix", "u_TextureUnit"});
        mSkyboxPositionHandle =
                GLES20.glGetAttribLocation(mSkyboxProgramHandle, "a_Position");
        mSkyboxCombinedMatrixHandle =
                GLES20.glGetUniformLocation(mSkyboxProgramHandle, "u_CombinedMatrix");
        mSkyboxTextureUnitHandle =
                GLES20.glGetUniformLocation(mSkyboxProgramHandle, "u_TextureUnit");
        mSkyboxTextureDataHandle = TextureHelper.createTextureHandle();

        mEyePoint = new CardboardActivityEyePoint();
    }

    @Override
    public void onSurfaceChanged(int width, int height) {
    }

    @Override
    public void onNewFrame(HeadTransform headTransform) {
        // Position the eye at the origin.
        float eyeX = 0.0f;
        float eyeY = 0.0f;
        float eyeZ;
        synchronized (mCameraPositionLock) {
            eyeZ = mCameraPosition;
        }

        // We are looking toward the negative Z direction.
        float lookX = DESKTOP_POSITION_X;
        float lookY = DESKTOP_POSITION_Y;
        float lookZ = DESKTOP_POSITION_Z;

        // Set our up vector. This is where our head would be pointing were we holding the camera.
        float upX = 0.0f;
        float upY = 1.0f;
        float upZ = 0.0f;

        Matrix.setLookAtM(mCameraMatrix, 0, eyeX, eyeY, eyeZ, lookX, lookY, lookZ, upX, upY, upZ);

        headTransform.getForwardVector(mForwardVector, 0);
        getLookingPosition();
        maybeLoadTexture(mTextureDataHandle);
        maybeLoadCubeMapAndCleanImages(mSkyboxTextureDataHandle);
    }

    @Override
    public void onDrawEye(Eye eye) {
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT | GLES20.GL_DEPTH_BUFFER_BIT);

        // Apply the eye transformation to the camera.
        Matrix.multiplyMM(mViewMatrix, 0, eye.getEyeView(), 0, mCameraMatrix, 0);

        mProjectionMatrix = eye.getPerspective(Z_NEAR, Z_FAR);

        drawSkybox();
        drawDesktop();
        drawEyePoint();
    }

    @Override
    public void onRendererShutdown() {
        GLES20.glDeleteShader(mDesktopVertexShaderHandle);
        GLES20.glDeleteShader(mDesktopFragmentShaderHandle);
        mEyePoint.cleanup();
        GLES20.glDeleteShader(mSkyboxVertexShaderHandle);
        GLES20.glDeleteShader(mSkyboxFragmentShaderHandle);
        GLES20.glDeleteTextures(1, new int[] {mTextureDataHandle}, 0);
        GLES20.glDeleteTextures(1, new int[] {mSkyboxTextureDataHandle}, 0);
        mActivity.runOnUiThread(new Runnable() {
            public void run() {
                mDownloadManager.close();
            }
        });
    }

    @Override
    public void onFinishFrame(Viewport viewport) {
    }

    private void drawDesktop() {
        GLES20.glUseProgram(mProgramHandle);

         // Translate the desktop model.
        Matrix.setIdentityM(mDesktopModelMatrix, 0);
        Matrix.translateM(mDesktopModelMatrix, 0, DESKTOP_POSITION_X,
                DESKTOP_POSITION_Y, DESKTOP_POSITION_Z);

        // Pass in Model View Matrix and Model View Project Matrix.
        Matrix.multiplyMM(mDesktopCombinedMatrix, 0, mViewMatrix, 0, mDesktopModelMatrix, 0);
        Matrix.multiplyMM(mDesktopCombinedMatrix, 0, mProjectionMatrix,
                0, mDesktopCombinedMatrix, 0);

        // Pass in model view project matrix.
        GLES20.glUniformMatrix4fv(mDesktopCombinedMatrixHandle, 1, false,
                mDesktopCombinedMatrix, 0);

        if (mDesktopCoordinates == null) {
            // This can happen if the client is connected, but a complete video frame has not yet
            // been decoded.
            return;
        }
        // Pass in the desktop position.
        GLES20.glVertexAttribPointer(mPositionHandle, POSITION_DATA_SIZE, GLES20.GL_FLOAT, false,
                0, mDesktopCoordinates);
        GLES20.glEnableVertexAttribArray(mPositionHandle);

        // Pass in texture coordinate.
        GLES20.glVertexAttribPointer(mTextureCoordinateHandle, TEXTURE_COORDINATE_DATA_SIZE,
                GLES20.GL_FLOAT, false, 0, DESKTOP_TEXTURE_COORDINATES);
        GLES20.glEnableVertexAttribArray(mTextureCoordinateHandle);

        // Pass in texture data.
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, mTextureDataHandle);
        GLES20.glUniform1i(mTextureUniformHandle, 0);

        // Draw the desktop.
        int totalPointNumber = 6;
        GLES20.glDrawArrays(GLES20.GL_TRIANGLES, 0, totalPointNumber);
    }

    private void drawEyePoint() {
        if (!isLookingAtDesktop()) {
            return;
        }

        float eyePointX = clamp(mEyePositionVector[0], -mHalfDesktopWidth,
                mHalfDesktopWidth);
        float eyePointY = clamp(mEyePositionVector[1], -HALF_DESKTOP_HEIGHT,
                HALF_DESKTOP_HEIGHT);
        Matrix.setIdentityM(mEyePointModelMatrix, 0);
        Matrix.translateM(mEyePointModelMatrix, 0, -eyePointX, -eyePointY,
                DESKTOP_POSITION_Z);
        Matrix.multiplyMM(mEyePointCombinedMatrix, 0, mViewMatrix, 0, mEyePointModelMatrix, 0);
        Matrix.multiplyMM(mEyePointCombinedMatrix, 0, mProjectionMatrix,
                0, mEyePointCombinedMatrix, 0);

        mEyePoint.setCombinedMatrix(mEyePointCombinedMatrix);
        mEyePoint.draw();
    }

    private void drawSkybox() {
        GLES20.glUseProgram(mSkyboxProgramHandle);

        Matrix.setIdentityM(mSkyboxModelMatrix, 0);
        Matrix.multiplyMM(mSkyboxCombinedMatrix, 0, mViewMatrix, 0, mSkyboxModelMatrix, 0);
        Matrix.multiplyMM(mSkyboxCombinedMatrix, 0, mProjectionMatrix,
                0, mSkyboxCombinedMatrix, 0);

        GLES20.glUniformMatrix4fv(mSkyboxCombinedMatrixHandle, 1, false,
                mSkyboxCombinedMatrix, 0);
        GLES20.glVertexAttribPointer(mSkyboxPositionHandle, POSITION_DATA_SIZE, GLES20.GL_FLOAT,
                false, 0, SKYBOX_POSITION_COORDINATES);
        GLES20.glEnableVertexAttribArray(mSkyboxPositionHandle);

        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_CUBE_MAP, mSkyboxTextureDataHandle);
        GLES20.glUniform1i(mSkyboxTextureUnitHandle, 0);

        GLES20.glDrawElements(GLES20.GL_TRIANGLES, 36, GLES20.GL_UNSIGNED_BYTE,
                SKYBOX_INDICES_BYTE_BUFFER);
    }

    /**
     * Returns coordinates in units of pixels in the desktop bitmap.
     * This can be called on any thread.
     */
    public PointF getMouseCoordinates() {
        PointF result = new PointF();
        synchronized (mEyePositionLock) {
            // Due to the coordinate direction, we only have to inverse x.
            result.x = (-mEyePositionVector[0] + mHalfDesktopWidth)
                    / (2 * mHalfDesktopWidth) * mDesktopWidthPixels;
            result.y = (mEyePositionVector[1] + HALF_DESKTOP_HEIGHT)
                    / (2 * HALF_DESKTOP_HEIGHT) * mDesktopHeightPixels;
            result.x = clamp(result.x, 0, mDesktopWidthPixels);
            result.y = clamp(result.y, 0, mDesktopHeightPixels);
        }
        return result;
    }

    /**
     * Returns the passed in value if it resides within the specified range (inclusive).  If not,
     * it will return the closest boundary from the range.  The ordering of the boundary values
     * does not matter.
     *
     * @param value The value to be compared against the range.
     * @param a First boundary range value.
     * @param b Second boundary range value.
     * @return The passed in value if it is within the range, otherwise the closest boundary value.
     */
    private static float clamp(float value, float a, float b) {
        float min = (a > b) ? b : a;
        float max = (a > b) ? a : b;
        if (value < min) {
            value = min;
        } else if (value > max) {
            value = max;
        }
        return value;
    }

    /**
     * Move the camera towards desktop.
     * This method can be called on any thread.
     */
    public void moveTowardsDesktop() {
        synchronized (mCameraPositionLock) {
            float newPosition = mCameraPosition - CAMERA_MOTION_STEP;
            if (newPosition >= VIEW_POSITION_MIN) {
                mCameraPosition = newPosition;
            }
        }
    }

    /**
     * Move the camera away from desktop.
     * This method can be called on any thread.
     */
    public void moveAwayFromDesktop() {
        synchronized (mCameraPositionLock) {
            float newPosition = mCameraPosition + CAMERA_MOTION_STEP;
            if (newPosition <= VIEW_POSITION_MAX) {
                mCameraPosition = newPosition;
            }
        }
    }

    /**
     * Return true if user is looking at the desktop.
     * This method can be called on any thread.
     */
    public boolean isLookingAtDesktop() {
        synchronized (mEyePositionLock) {
            return Math.abs(mEyePositionVector[0]) <= (mHalfDesktopWidth + EDGE_MARGIN)
                && Math.abs(mEyePositionVector[1]) <= (HALF_DESKTOP_HEIGHT + EDGE_MARGIN);
        }
    }

    /**
     * Return true if user is looking at the space to the left of the desktop.
     * This method can be called on any thread.
     */
    public boolean isLookingLeftOfDesktop() {
        synchronized (mEyePositionLock) {
            return mEyePositionVector[0] >= (mHalfDesktopWidth + EDGE_MARGIN);
        }
    }

    /**
     * Return true if user is looking at the space to the right of the desktop.
     * This method can be called on any thread.
     */
    public boolean isLookingRightOfDesktop() {
        synchronized (mEyePositionLock) {
            return mEyePositionVector[0] <= -(mHalfDesktopWidth + EDGE_MARGIN);
        }
    }

    /**
     * Return true if user is looking at the space above the desktop.
     * This method can be called on any thread.
     */
    public boolean isLookingAboveDesktop() {
        synchronized (mEyePositionLock) {
            return mEyePositionVector[1] <= -(HALF_DESKTOP_HEIGHT + EDGE_MARGIN);
        }
    }

    /**
     * Return true if user is looking at the space below the desktop.
     * This method can be called on any thread.
     */
    public boolean isLookingBelowDesktop() {
        synchronized (mEyePositionLock) {
            return mEyePositionVector[1] >= (HALF_DESKTOP_HEIGHT + EDGE_MARGIN);
        }
    }

    /**
     * Get position on desktop where user is looking at.
     */
    private void getLookingPosition() {
        synchronized (mEyePositionLock) {
            if (Math.abs(mForwardVector[2]) < 0.00001f) {
                mEyePositionVector[0] = Math.signum(mForwardVector[0]) * Float.MAX_VALUE;
                mEyePositionVector[1] = Math.signum(mForwardVector[1]) * Float.MAX_VALUE;
            } else {
                mEyePositionVector[0] = mForwardVector[0] * DESKTOP_POSITION_Z / mForwardVector[2];
                mEyePositionVector[1] = mForwardVector[1] * DESKTOP_POSITION_Z / mForwardVector[2];
            }
            mEyePositionVector[2] = DESKTOP_POSITION_Z;
        }
    }

    /**
     * Link desktop texture with textureDataHandle if {@link mReloadTexture} is true.
     * @param textureDataHandle the handle we want attach texture to
     */
    private void maybeLoadTexture(int textureDataHandle) {
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

        synchronized (mEyePositionLock) {
            mDesktopHeightPixels = bitmap.getHeight();
            mDesktopWidthPixels = bitmap.getWidth();
        }

        updateDesktopCoordinatesBuffer(bitmap);
        TextureHelper.linkTexture(textureDataHandle, bitmap);

        synchronized (mReloadTextureLock) {
            mReloadTexture = false;
        }
    }

    /**
     * Convert float array to a FloatBuffer for use in OpenGL calls.
     */
    private static FloatBuffer makeFloatBuffer(float[] data) {
        FloatBuffer result = ByteBuffer
                .allocateDirect(data.length * BYTE_PER_FLOAT)
                .order(ByteOrder.nativeOrder()).asFloatBuffer();
        result.put(data).position(0);
        return result;
    }

    /**
     *  Update the desktop coordinates based on the new bitmap. Note here we fix the
     *  height of the desktop and vary width accordingly.
     */
    private void updateDesktopCoordinatesBuffer(Bitmap bitmap) {
        int width = bitmap.getWidth();
        int height = bitmap.getHeight();
        float newHalfDesktopWidth = width * HALF_DESKTOP_HEIGHT / height;
        if (Math.abs(mHalfDesktopWidth - newHalfDesktopWidth) > 0.0001) {
            mHalfDesktopWidth = newHalfDesktopWidth;
            mDesktopCoordinates = makeFloatBuffer(new float[] {
                // Desktop model coordinates.
                -mHalfDesktopWidth, HALF_DESKTOP_HEIGHT, 0.0f,
                -mHalfDesktopWidth, -HALF_DESKTOP_HEIGHT, 0.0f,
                mHalfDesktopWidth, HALF_DESKTOP_HEIGHT, 0.0f,
                -mHalfDesktopWidth, -HALF_DESKTOP_HEIGHT, 0.0f,
                mHalfDesktopWidth, -HALF_DESKTOP_HEIGHT, 0.0f,
                mHalfDesktopWidth, HALF_DESKTOP_HEIGHT, 0.0f
            });
        }
    }

    /**
     * Decode all skybox images to Bitmap files and return them.
     * Only call this method when we have complete skybox images.
     * @throws DecodeFileException if BitmapFactory fails to decode file.
     */
    private Bitmap[] decodeSkyboxImages() throws DecodeFileException {
        Bitmap[] result = new Bitmap[SKYBOX_IMAGE_NAMES.length];
        String fileDirectory = mDownloadManager.getDownloadDirectory();
        for (int i = 0; i < SKYBOX_IMAGE_NAMES.length; i++) {
            result[i] = BitmapFactory.decodeFile(fileDirectory + "/" + SKYBOX_IMAGE_NAMES[i]);
            if (result[i] == null) {
                throw new DecodeFileException();
            }
        }
        return result;
    }

    /**
     * Link the skybox images with given texture handle and clean images at the end.
     * Only call this method when we have complete skybox images.
     */
    private void maybeLoadCubeMapAndCleanImages(int textureHandle) {
        synchronized (mLoadSkyboxImagesTextureLock) {
            if (!mLoadSkyboxImagesTexture) {
                return;
            }
            mLoadSkyboxImagesTexture = false;
        }

        Bitmap[] images;
        try {
            images = decodeSkyboxImages();
        } catch (DecodeFileException e) {
            Log.i(TAG, "Failed to decode image files.");
            return;
        }

        TextureHelper.linkCubeMap(textureHandle, images);
        for (Bitmap image : images) {
            image.recycle();
        }
    }

    /**
     * Exception when BitmapFactory fails to decode file.
     */
    private static class DecodeFileException extends Exception {
    }
}