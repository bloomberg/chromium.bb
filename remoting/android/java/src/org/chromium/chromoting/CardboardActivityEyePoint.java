// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.opengl.GLES20;

/**
 * Chromoting Cardboard activity eye point, which represents the location on the desktop
 * where user is looking at.
 */
public class CardboardActivityEyePoint {
    private static final String VERTEX_SHADER =
            "uniform mat4 u_CombinedMatrix;"
            + "attribute vec4 a_EyePosition;"
            + "void main() {"
            + "  gl_Position = u_CombinedMatrix * a_EyePosition;"
            + "  gl_PointSize = 3.0;"
            + "}";

    private static final String FRAGMENT_SHADER =
            "precision mediump float;"
            + "void main() {"
            + "  gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);"
            + "}";

    // Size of the vertexes to draw eye point.
    private static final int VERTEXES_NUMBER = 1;

    private int mVertexShaderHandle;
    private int mFragmentShaderHandle;
    private int mProgramHandle;
    private int mCombinedMatrixHandle;
    private int mPositionHandle;
    private float[] mCombinedMatrix;

    public CardboardActivityEyePoint() {
        // Set handlers for eye point drawing.
        mVertexShaderHandle =
                ShaderHelper.compileShader(GLES20.GL_VERTEX_SHADER, VERTEX_SHADER);
        mFragmentShaderHandle =
                ShaderHelper.compileShader(GLES20.GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
        mProgramHandle = ShaderHelper.createAndLinkProgram(mVertexShaderHandle,
                mFragmentShaderHandle, new String[] {"a_EyePosition", "u_CombinedMatrix"});
        mPositionHandle =
                GLES20.glGetAttribLocation(mProgramHandle, "a_EyePosition");
        mCombinedMatrixHandle =
                GLES20.glGetUniformLocation(mProgramHandle, "u_CombinedMatrix");
    }

    /**
     * Set up model view projection matrix.
     */
    public void setCombinedMatrix(float[] combinedMatrix) {
        mCombinedMatrix = combinedMatrix.clone();
    }

    /**
     * Draw the eye point based on given model view projection matrix.
     */
    public void draw() {
        GLES20.glUseProgram(mProgramHandle);

        // Set the eye point in front of desktop.
        GLES20.glClear(GLES20.GL_DEPTH_BUFFER_BIT);

        GLES20.glUniformMatrix4fv(mCombinedMatrixHandle, 1, false, mCombinedMatrix, 0);

        GLES20.glVertexAttrib4f(mPositionHandle, 0.0f, 0.0f, 0.0f, 1.0f);

        // Since we are not using a buffer object, disable vertex arrays for this attribute.
        GLES20.glDisableVertexAttribArray(mPositionHandle);

        GLES20.glDrawArrays(GLES20.GL_POINTS, 0, VERTEXES_NUMBER);
    }

    /**
     * Clean up opengl data.
     */
    public void cleanup() {
        GLES20.glDeleteShader(mVertexShaderHandle);
        GLES20.glDeleteShader(mFragmentShaderHandle);
    }
}