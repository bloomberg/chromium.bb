// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.cardboard;

import static org.chromium.chromoting.cardboard.CardboardUtil.makeFloatBuffer;
import static org.chromium.chromoting.cardboard.CardboardUtil.makeRectangularTextureBuffer;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.PointF;
import android.graphics.RectF;
import android.opengl.GLES20;

import org.chromium.chromoting.cardboard.MenuBar.MenuItemType;

import java.nio.FloatBuffer;

/**
 * Cardboard activity menu item representing a corresponding function.
 */
public class MenuItem {
    private static final String VERTEX_SHADER =
            "uniform mat4 u_CombinedMatrix;"
            + "attribute vec4 a_Position;"
            + "attribute vec2 a_TexCoordinate;"
            + "varying vec2 v_TexCoordinate;"
            + "attribute float a_selected;"
            + "varying float v_selected;"
            + "void main() {"
            + "  v_selected = a_selected;"
            + "  v_TexCoordinate = a_TexCoordinate;"
            + "  gl_Position = u_CombinedMatrix * a_Position;"
            + "}";

    private static final String FRAGMENT_SHADER =
            "precision mediump float;"
            + "uniform sampler2D u_Texture;"
            + "varying vec2 v_TexCoordinate;"
            + "varying float v_selected;"
            + "void main() {"
            + "  vec4 texture = texture2D(u_Texture, v_TexCoordinate);"
            + "  if (v_selected > 0.5) {"
            + "    gl_FragColor = vec4(texture.r + 0.5, texture.g + 0.5,"
            + "        texture.b + 0.5, texture.a);"
            + "  } else {"
            + "    gl_FragColor = texture;"
            + "  }"
            + "}";

    private static final FloatBuffer TEXTURE_COORDINATES = makeRectangularTextureBuffer(
            0.0f, 1.0f, 0.0f, 1.0f);

    private static final int POSITION_COORDINATE_DATA_SIZE = 3;
    private static final int TEXTURE_COORDINATE_DATA_SIZE = 2;

    // Number of vertices passed to glDrawArrays().
    private static final int VERTICES_NUMBER = 6;

    private final FloatBuffer mPositionCoordinates;

    private int mVertexShaderHandle;
    private int mFragmentShaderHandle;
    private int mProgramHandle;
    private int mCombinedMatrixHandle;
    private int mTextureUniformHandle;
    private int mPositionHandle;
    private int mTextureDataHandle;
    private int mTextureCoordinateHandle;
    private int mItemSelectedHandle;

    private MenuItemType mType;

    private RectF mRect;

    public MenuItem(Context context, MenuItemType type, RectF rect) {
        mType = type;
        mRect = new RectF(rect);
        float halfHeight = mRect.height() / 2;
        float halfWidth = mRect.width() / 2;
        mPositionCoordinates = makeFloatBuffer(new float[] {
            // Desktop model coordinates.
            -halfWidth, halfHeight, 0.0f,
            -halfWidth, -halfHeight, 0.0f,
            halfWidth, halfHeight, 0.0f,
            -halfWidth, -halfHeight, 0.0f,
            halfWidth, -halfHeight, 0.0f,
            halfWidth, halfHeight, 0.0f
        });

        mVertexShaderHandle =
                ShaderHelper.compileShader(GLES20.GL_VERTEX_SHADER, VERTEX_SHADER);
        mFragmentShaderHandle =
                ShaderHelper.compileShader(GLES20.GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
        mProgramHandle = ShaderHelper.createAndLinkProgram(mVertexShaderHandle,
                mFragmentShaderHandle, new String[] {"a_Position", "a_TexCoordinate",
                    "a_selected", "u_CombinedMatrix", "u_Texture"});

        mCombinedMatrixHandle =
                GLES20.glGetUniformLocation(mProgramHandle, "u_CombinedMatrix");
        mTextureUniformHandle = GLES20.glGetUniformLocation(mProgramHandle, "u_Texture");
        mPositionHandle = GLES20.glGetAttribLocation(mProgramHandle, "a_Position");
        mItemSelectedHandle = GLES20.glGetAttribLocation(mProgramHandle, "a_selected");
        mTextureDataHandle = TextureHelper.createTextureHandle();
        mTextureCoordinateHandle = GLES20.glGetAttribLocation(mProgramHandle, "a_TexCoordinate");

        Bitmap texture = BitmapFactory.decodeResource(context.getResources(),
                    type.resourceId());
        TextureHelper.linkTexture(mTextureDataHandle, texture);
        texture.recycle();
    }

    /**
     * Return the type of this menu item.
     */
    public MenuItemType getType() {
        return mType;
    }

    /**
     * Return the position of the center of this menu item.
     */
    public PointF getPosition() {
        return new PointF(mRect.centerX(), mRect.centerY());
    }

    /**
     * Return true if the point is inside this menu item.
     */
    public boolean contains(PointF point) {
        return mRect.contains(point.x, point.y);
    }

    /**
     * Draw menu item according to the given model view projection matrix.
     */
    public void draw(float[] combinedMatrix, boolean selected) {
        GLES20.glUseProgram(mProgramHandle);

        // Pass in model view project matrix.
        GLES20.glUniformMatrix4fv(mCombinedMatrixHandle, 1, false, combinedMatrix, 0);

        // Pass in whether the item is selected
        GLES20.glVertexAttrib1f(mItemSelectedHandle, selected ? 1.0f : 0.0f);

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
     * Clean menu item related opengl data.
     */
    public void clean() {
        GLES20.glDeleteShader(mVertexShaderHandle);
        GLES20.glDeleteShader(mFragmentShaderHandle);
        GLES20.glDeleteTextures(1, new int[]{mTextureDataHandle}, 0);
    }
}
