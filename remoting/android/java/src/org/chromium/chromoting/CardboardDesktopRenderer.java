// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Point;
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

    // Distance to move camera each time.
    private static final float CAMERA_MOTION_STEP = 0.5f;

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

    private float mCameraPosition;

    // Lock to allow multithreaded access to mCameraPosition.
    private final Object mCameraPositionLock = new Object();

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

    private int mSkyboxVertexShaderHandle;
    private int mSkyboxFragmentShaderHandle;
    private int mSkyboxProgramHandle;
    private int mSkyboxPositionHandle;
    private int mSkyboxCombinedMatrixHandle;
    private int mSkyboxTextureUnitHandle;
    private int mSkyboxTextureDataHandle;
    private CardboardActivityDesktop mDesktop;
    private CardboardActivityEyePoint mEyePoint;

    // Flag to indicate whether reload the desktop texture or not.
    private boolean mReloadTexture;

    /** Lock to allow multithreaded access to mReloadTexture. */
    private final Object mReloadTextureLock = new Object();

    // Lock for eye position related operations.
    // This protects access to mEyePositionVector.
    private final Object mEyePositionLock = new Object();

    // Flag to signal that the skybox images are fully decoded and should be loaded
    // into the OpenGL textures.
    private boolean mLoadSkyboxImagesTexture;

    // Lock to allow multithreaded access to mLoadSkyboxImagesTexture.
    private final Object mLoadSkyboxImagesTextureLock = new Object();

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

        mDesktop = new CardboardActivityDesktop();
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
        maybeLoadDesktopTexture();
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
        mDesktop.cleanup();
        mEyePoint.cleanup();
        GLES20.glDeleteShader(mSkyboxVertexShaderHandle);
        GLES20.glDeleteShader(mSkyboxFragmentShaderHandle);
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
        if (!mDesktop.hasVideoFrame()) {
            // This can happen if the client is connected, but a complete
            // video frame has not yet been decoded.
            return;
        }


        Matrix.setIdentityM(mDesktopModelMatrix, 0);
        Matrix.translateM(mDesktopModelMatrix, 0, DESKTOP_POSITION_X,
                DESKTOP_POSITION_Y, DESKTOP_POSITION_Z);

        // Pass in Model View Matrix and Model View Project Matrix.
        Matrix.multiplyMM(mDesktopCombinedMatrix, 0, mViewMatrix, 0, mDesktopModelMatrix, 0);
        Matrix.multiplyMM(mDesktopCombinedMatrix, 0, mProjectionMatrix,
                0, mDesktopCombinedMatrix, 0);
        mDesktop.setCombinedMatrix(mDesktopCombinedMatrix);

        mDesktop.draw();
    }

    private void drawEyePoint() {
        if (!isLookingAtDesktop()) {
            return;
        }

        float eyePointX = clamp(mEyePositionVector[0], -mDesktop.getHalfWidth(),
                mDesktop.getHalfWidth());
        float eyePointY = clamp(mEyePositionVector[1], -mDesktop.getHalfHeight(),
                mDesktop.getHalfHeight());
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
        Point shapePixels = mDesktop.getFrameSizePixels();
        int heightPixels = shapePixels.x;
        int widthPixels = shapePixels.y;

        synchronized (mEyePositionLock) {
            // Due to the coordinate direction, we only have to inverse x.
            result.x = (-mEyePositionVector[0] + mDesktop.getHalfWidth())
                    / (2 * mDesktop.getHalfWidth()) * widthPixels;
            result.y = (mEyePositionVector[1] + mDesktop.getHalfHeight())
                    / (2 * mDesktop.getHalfHeight()) * heightPixels;
            result.x = clamp(result.x, 0, widthPixels);
            result.y = clamp(result.y, 0, heightPixels);
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
            return Math.abs(mEyePositionVector[0]) <= (mDesktop.getHalfWidth() + EDGE_MARGIN)
                && Math.abs(mEyePositionVector[1]) <= (mDesktop.getHalfHeight() + EDGE_MARGIN);
        }
    }

    /**
     * Return true if user is looking at the space to the left of the desktop.
     * This method can be called on any thread.
     */
    public boolean isLookingLeftOfDesktop() {
        synchronized (mEyePositionLock) {
            return mEyePositionVector[0] >= (mDesktop.getHalfWidth() + EDGE_MARGIN);
        }
    }

    /**
     * Return true if user is looking at the space to the right of the desktop.
     * This method can be called on any thread.
     */
    public boolean isLookingRightOfDesktop() {
        synchronized (mEyePositionLock) {
            return mEyePositionVector[0] <= -(mDesktop.getHalfWidth() + EDGE_MARGIN);
        }
    }

    /**
     * Return true if user is looking at the space above the desktop.
     * This method can be called on any thread.
     */
    public boolean isLookingAboveDesktop() {
        synchronized (mEyePositionLock) {
            return mEyePositionVector[1] <= -(mDesktop.getHalfHeight() + EDGE_MARGIN);
        }
    }

    /**
     * Return true if user is looking at the space below the desktop.
     * This method can be called on any thread.
     */
    public boolean isLookingBelowDesktop() {
        synchronized (mEyePositionLock) {
            return mEyePositionVector[1] >= (mDesktop.getHalfHeight() + EDGE_MARGIN);
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
     * Link desktop texture with {@link CardboardActivityDesktop} if {@link mReloadTexture} is true.
     * @param textureDataHandle the handle we want attach texture to
     */
    private void maybeLoadDesktopTexture() {
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

        mDesktop.updateVideoFrame(bitmap);

        synchronized (mReloadTextureLock) {
            mReloadTexture = false;
        }
    }

    /**
     * Convert float array to a FloatBuffer for use in OpenGL calls.
     */
    public static FloatBuffer makeFloatBuffer(float[] data) {
        FloatBuffer result = ByteBuffer
                .allocateDirect(data.length * BYTE_PER_FLOAT)
                .order(ByteOrder.nativeOrder()).asFloatBuffer();
        result.put(data).position(0);
        return result;
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