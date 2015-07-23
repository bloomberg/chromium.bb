// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.content.Context;
import android.graphics.Bitmap;
import android.opengl.GLES20;
import android.opengl.Matrix;

import com.google.vrtoolkit.cardboard.CardboardView;
import com.google.vrtoolkit.cardboard.Eye;
import com.google.vrtoolkit.cardboard.HeadTransform;
import com.google.vrtoolkit.cardboard.Viewport;

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
    private static final float INIT_DESKTOP_POSITION_X = 0.0f;
    private static final float INIT_DESKTOP_POSITION_Y = 0.0f;
    private static final float INIT_DESKTOP_POSITION_Z = -3.0f;

    private static final FloatBuffer DESKTOP_COORDINATES = makeFloatBuffer(new float[] {
            // Desktop model coordinates.
            -1.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, 1.0f,
            1.0f, -1.0f, 1.0f,
            1.0f, 1.0f, 1.0f
    });

    private static final FloatBuffer DESKTOP_TEXTURE_COORDINATES = makeFloatBuffer(new float[] {
            // Texture coordinate data.
            0.0f, 0.0f,
            0.0f, 1.0f,
            1.0f, 0.0f,
            0.0f, 1.0f,
            1.0f, 1.0f,
            1.0f, 0.0f
    });

    private static final String VERTEX_SHADER =
            "uniform mat4 u_CombinedMatrix;"
            + "attribute vec4 a_Position;"
            + "attribute vec2 a_TexCoordinate;"
            + "varying vec2 v_TexCoordinate;"
            + "void main() {"
            + "   v_TexCoordinate = a_TexCoordinate;"
            + "   gl_Position = u_CombinedMatrix * a_Position;"
            + "}";

    private static final String FRAGMENT_SHADER =
            "precision mediump float;"
            + "uniform sampler2D u_Texture;"
            + "varying vec2 v_TexCoordinate;"
            + "void main() {"
            + "   gl_FragColor = texture2D(u_Texture, v_TexCoordinate);"
            + "}";

    private final Context mActivityContext;

    // Flag to indicate whether reload the desktop texture or not.
    private boolean mReloadTexture;

    private float[] mCameraMatrix;
    private float[] mModelMatrix;
    private float[] mViewMatrix;
    private float[] mProjectionMatrix;
    private float[] mCombinedMatrix;

    private int mCombinedMatrixHandle;
    private int mPositionHandle;
    private int mTextureDataHandle;
    private int mTextureUniformHandle;
    private int mTextureCoordinateHandle;
    private int mProgramHandle;
    private int mVertexShaderHandle;
    private int mFragmentShaderHandle;

    /** Lock to allow multithreaded access to mReloadTexture. */
    private Object mReloadTextureLock = new Object();

    public CardboardDesktopRenderer(Context context) {
        mActivityContext = context;
        mReloadTexture = false;

        mCameraMatrix = new float[16];
        mModelMatrix = new float[16];
        mViewMatrix = new float[16];
        mProjectionMatrix = new float[16];
        mCombinedMatrix = new float[16];

        // Provide the callback for JniInterface.
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
        mVertexShaderHandle =
                ShaderHelper.compileShader(GLES20.GL_VERTEX_SHADER, VERTEX_SHADER);
        mFragmentShaderHandle =
                ShaderHelper.compileShader(GLES20.GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
        mProgramHandle = ShaderHelper.createAndLinkProgram(mVertexShaderHandle,
                mFragmentShaderHandle, new String[] {"a_Position", "a_TexCoordinate"});
        mCombinedMatrixHandle = GLES20.glGetUniformLocation(mProgramHandle, "u_CombinedMatrix");
        mTextureUniformHandle = GLES20.glGetUniformLocation(mProgramHandle, "u_Texture");
        mPositionHandle = GLES20.glGetAttribLocation(mProgramHandle, "a_Position");
        mTextureCoordinateHandle = GLES20.glGetAttribLocation(mProgramHandle, "a_TexCoordinate");
        mTextureDataHandle = TextureHelper.createTextureHandle();

        // Position the eye at the origin.
        float eyeX = 0.0f;
        float eyeY = 0.0f;
        float eyeZ = 0.0f;

        // We are looking toward the negative Z direction.
        float lookX = 0.0f;
        float lookY = 0.0f;
        float lookZ = -1.0f;

        // Set our up vector. This is where our head would be pointing were we holding the camera.
        float upX = 0.0f;
        float upY = 1.0f;
        float upZ = 0.0f;

        Matrix.setLookAtM(mCameraMatrix, 0, eyeX, eyeY, eyeZ, lookX, lookY, lookZ, upX, upY, upZ);

        JniInterface.redrawGraphics();
    }

    @Override
    public void onSurfaceChanged(int width, int height) {
    }

    @Override
    public void onNewFrame(HeadTransform headTransform) {
        maybeLoadTexture(mTextureDataHandle);
    }

    @Override
    public void onDrawEye(Eye eye) {
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT | GLES20.GL_DEPTH_BUFFER_BIT);

        // Apply the eye transformation to the camera.
        Matrix.multiplyMM(mViewMatrix, 0, eye.getEyeView(), 0, mCameraMatrix, 0);

        mProjectionMatrix = eye.getPerspective(Z_NEAR, Z_FAR);

        // Translate the desktop model
        Matrix.setIdentityM(mModelMatrix, 0);
        Matrix.translateM(mModelMatrix, 0, INIT_DESKTOP_POSITION_X,
                INIT_DESKTOP_POSITION_Y, INIT_DESKTOP_POSITION_Z);

        // Pass in Model View Matrix and Model View Project Matrix
        Matrix.multiplyMM(mCombinedMatrix, 0, mViewMatrix, 0, mModelMatrix, 0);
        Matrix.multiplyMM(mCombinedMatrix, 0, mProjectionMatrix, 0, mCombinedMatrix, 0);

        drawDesktop();
    }

    @Override
    public void onRendererShutdown() {
        GLES20.glDeleteShader(mVertexShaderHandle);
        GLES20.glDeleteShader(mFragmentShaderHandle);
        GLES20.glDeleteTextures(1, new int[]{mTextureDataHandle}, 0);
    }

    @Override
    public void onFinishFrame(Viewport viewport) {
    }

    private void drawDesktop() {
        GLES20.glUseProgram(mProgramHandle);

        // Pass in model view project matrix.
        GLES20.glUniformMatrix4fv(mCombinedMatrixHandle, 1, false, mCombinedMatrix, 0);

        // Pass in texture data.
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, mTextureDataHandle);
        GLES20.glUniform1i(mTextureUniformHandle, 0);

        // Pass in the desktop position.
        GLES20.glVertexAttribPointer(mPositionHandle, POSITION_DATA_SIZE, GLES20.GL_FLOAT, false,
                0, DESKTOP_COORDINATES);
        GLES20.glEnableVertexAttribArray(mPositionHandle);

        // Pass in texture coordinate.
        GLES20.glVertexAttribPointer(mTextureCoordinateHandle, TEXTURE_COORDINATE_DATA_SIZE,
                GLES20.GL_FLOAT, false, 0, DESKTOP_TEXTURE_COORDINATES);
        GLES20.glEnableVertexAttribArray(mTextureCoordinateHandle);

        // Draw the desktop.
        int totalPointNumber = 6;
        GLES20.glDrawArrays(GLES20.GL_TRIANGLES, 0, totalPointNumber);
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
}