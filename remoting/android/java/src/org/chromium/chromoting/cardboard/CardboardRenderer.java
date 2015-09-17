// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.cardboard;

import android.app.Activity;
import android.graphics.Point;
import android.graphics.PointF;
import android.opengl.GLES20;
import android.opengl.Matrix;

import com.google.vrtoolkit.cardboard.CardboardView;
import com.google.vrtoolkit.cardboard.Eye;
import com.google.vrtoolkit.cardboard.HeadTransform;
import com.google.vrtoolkit.cardboard.Viewport;

import org.chromium.chromoting.jni.JniInterface;

import javax.microedition.khronos.egl.EGLConfig;

/**
 * Renderer for Cardboard view.
 */
public class CardboardRenderer implements CardboardView.StereoRenderer {
    private static final String TAG = "cr.CardboardRenderer";

    private static final int BYTE_PER_FLOAT = 4;
    private static final int POSITION_DATA_SIZE = 3;
    private static final int TEXTURE_COORDINATE_DATA_SIZE = 2;
    private static final float Z_NEAR = 0.1f;
    private static final float Z_FAR = 1000.0f;

    // Desktop position is fixed in world coordinates.
    private static final float DESKTOP_POSITION_X = 0.0f;
    private static final float DESKTOP_POSITION_Y = 0.0f;
    private static final float DESKTOP_POSITION_Z = -2.0f;

    // Menu bar position is relative to the view point.
    private static final float MENU_BAR_POSITION_X = 0.0f;
    private static final float MENU_BAR_POSITION_Y = 0.0f;
    private static final float MENU_BAR_POSITION_Z = -0.9f;

    private static final float HALF_SKYBOX_SIZE = 100.0f;
    private static final float VIEW_POSITION_MIN = -1.0f;
    private static final float VIEW_POSITION_MAX = 3.0f;

    // Allows user to click even when looking outside the desktop
    // but within edge margin.
    private static final float EDGE_MARGIN = 0.1f;

    // Distance to move camera each time.
    private static final float CAMERA_MOTION_STEP = 0.5f;

    // This ratio is used by {@link isLookingFarawayFromDesktop()} to determine the
    // angle beyond which the user is looking faraway from the desktop.
    // The ratio is based on half of the desktop's angular width, as seen from
    // the camera position.
    // If the user triggers the button while looking faraway, this will cause the
    // desktop to be re-positioned in the center of the view.
    private static final float FARAWAY_ANGLE_RATIO = 1.6777f;

    // Small number used to avoid division-overflow or other problems with
    // floating-point imprecision.
    private static final float EPSILON = 1e-5f;

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
    private float[] mPhotosphereCombinedMatrix;

    // Direction that user is looking towards.
    private float[] mForwardVector;

    // Eye position at the desktop distance.
    private PointF mEyeDesktopPosition;

    // Eye position at the menu bar distance;
    private PointF mEyeMenuBarPosition;

    private Desktop mDesktop;
    private MenuBar mMenuBar;
    private Photosphere mPhotosphere;
    private Cursor mCursor;

    // Lock for eye position related operations.
    // This protects access to mEyeDesktopPosition.
    private final Object mEyeDesktopPositionLock = new Object();

    // Flag to indicate whether to show menu bar.
    private boolean mMenuBarVisible;

    public CardboardRenderer(Activity activity) {
        mActivity = activity;
        mCameraPosition = 0.0f;

        mCameraMatrix = new float[16];
        mViewMatrix = new float[16];
        mProjectionMatrix = new float[16];
        mDesktopModelMatrix = new float[16];
        mDesktopCombinedMatrix = new float[16];
        mEyePointModelMatrix = new float[16];
        mEyePointCombinedMatrix = new float[16];
        mPhotosphereCombinedMatrix = new float[16];

        mForwardVector = new float[3];
    }

    private void initializeRedrawCallback() {
        mActivity.runOnUiThread(new Runnable() {
            public void run() {
                JniInterface.provideRedrawCallback(new Runnable() {
                    @Override
                    public void run() {
                        mDesktop.reloadTexture();
                        mCursor.reloadTexture();
                    }
                });

                JniInterface.redrawGraphics();
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

        mDesktop = new Desktop();
        mMenuBar = new MenuBar(mActivity);
        mPhotosphere = new Photosphere(mActivity);
        mCursor = new Cursor();

        initializeRedrawCallback();
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
        mEyeDesktopPosition = getLookingPosition(Math.abs(DESKTOP_POSITION_Z - eyeZ));
        mEyeMenuBarPosition = getLookingPosition(Math.abs(MENU_BAR_POSITION_Z));
        mDesktop.maybeLoadDesktopTexture();
        mPhotosphere.maybeLoadTextureAndCleanImage();
        mCursor.maybeLoadTexture(mDesktop);
        mCursor.moveTo(getMouseCoordinates());
    }

    @Override
    public void onDrawEye(Eye eye) {
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT | GLES20.GL_DEPTH_BUFFER_BIT);

        // Apply the eye transformation to the camera.
        Matrix.multiplyMM(mViewMatrix, 0, eye.getEyeView(), 0, mCameraMatrix, 0);

        mProjectionMatrix = eye.getPerspective(Z_NEAR, Z_FAR);

        drawDesktop();
        drawPhotosphere(eye.getType());
        drawMenuBar();
        drawCursor();
    }

    @Override
    public void onRendererShutdown() {
        mDesktop.cleanup();
        mMenuBar.cleanup();
        mPhotosphere.cleanup();
        mCursor.cleanup();
    }

    @Override
    public void onFinishFrame(Viewport viewport) {
    }

    private void drawCursor() {
        if (!isLookingAtDesktop() || (isMenuBarVisible() && isLookingAtMenuBar())
                || !mCursor.hasImageFrame()) {
            return;
        }

        float eyePointX = clamp(mEyeDesktopPosition.x, -mDesktop.getHalfWidth(),
                mDesktop.getHalfWidth());
        float eyePointY = clamp(mEyeDesktopPosition.y, -mDesktop.getHalfHeight(),
                mDesktop.getHalfHeight());

        Matrix.setIdentityM(mEyePointModelMatrix, 0);
        Matrix.translateM(mEyePointModelMatrix, 0, eyePointX , eyePointY, DESKTOP_POSITION_Z);

        Matrix.multiplyMM(mEyePointCombinedMatrix, 0, mViewMatrix, 0, mEyePointModelMatrix, 0);
        Matrix.multiplyMM(mEyePointCombinedMatrix, 0, mProjectionMatrix,
                0, mEyePointCombinedMatrix, 0);

        mCursor.draw(mEyePointCombinedMatrix);
    }

    private void drawPhotosphere(int eyeType) {
        // Since we will always put the photosphere center in the origin, the
        // model matrix will always be identity matrix which we can ignore.
        Matrix.multiplyMM(mPhotosphereCombinedMatrix, 0, mProjectionMatrix,
                0, mViewMatrix, 0);

        mPhotosphere.draw(mPhotosphereCombinedMatrix, eyeType);
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

        mDesktop.draw(mDesktopCombinedMatrix, mMenuBarVisible);
    }

    private void drawMenuBar() {
        if (!mMenuBarVisible) {
            return;
        }

        float menuBarZ;
        synchronized (mCameraPositionLock) {
            menuBarZ = mCameraPosition + MENU_BAR_POSITION_Z;
        }


        mMenuBar.draw(mViewMatrix, mProjectionMatrix, mEyeMenuBarPosition, MENU_BAR_POSITION_X,
                MENU_BAR_POSITION_Y, menuBarZ);
    }

    /**
     * Return menu item that is currently looking at or null if not looking at menu bar.
     */
    public MenuItem getMenuItem() {
        // Transform world view to model view.
        return mMenuBar.getLookingItem(new PointF(mEyeMenuBarPosition.x - MENU_BAR_POSITION_X,
                mEyeMenuBarPosition.y - MENU_BAR_POSITION_Y));
    }

    /**
     * Returns coordinates in units of pixels in the desktop bitmap.
     * This can be called on any thread.
     */
    public PointF getMouseCoordinates() {
        PointF result = new PointF();
        Point shapePixels = mDesktop.getFrameSizePixels();
        int widthPixels = shapePixels.x;
        int heightPixels = shapePixels.y;

        synchronized (mEyeDesktopPositionLock) {
            // Due to the coordinate direction, we only have to inverse x.
            result.x = (mEyeDesktopPosition.x + mDesktop.getHalfWidth())
                    / (2 * mDesktop.getHalfWidth()) * widthPixels;
            result.y = (-mEyeDesktopPosition.y + mDesktop.getHalfHeight())
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
        synchronized (mEyeDesktopPositionLock) {
            // TODO(shichengfeng): Move logic to CardboardActivityDesktop.
            return mForwardVector[2] < 0
                    && Math.abs(mEyeDesktopPosition.x) <= (mDesktop.getHalfWidth() + EDGE_MARGIN)
                    && Math.abs(mEyeDesktopPosition.y) <= (mDesktop.getHalfHeight() + EDGE_MARGIN);
        }
    }

    /**
     * Return true if user is looking at the menu bar.
     */
    public boolean isLookingAtMenuBar() {
        return mForwardVector[2] < 0
                && mMenuBar.contains(new PointF(mEyeMenuBarPosition.x - MENU_BAR_POSITION_X,
                mEyeMenuBarPosition.y - MENU_BAR_POSITION_Y));
    }

    /**
     * Get eye position at the given distance.
     */
    private PointF getLookingPosition(float distance) {
        if (Math.abs(mForwardVector[2]) < EPSILON) {
            return new PointF(Math.copySign(Float.MAX_VALUE, mForwardVector[0]),
                    Math.copySign(Float.MAX_VALUE, mForwardVector[1]));
        } else {
            return new PointF(mForwardVector[0] * distance / mForwardVector[2],
                    mForwardVector[1] * distance / mForwardVector[2]);
        }
    }

    /**
     * Set the visibility of the menu bar.
     */
    public void setMenuBarVisible(boolean visible) {
        mMenuBarVisible = visible;
    }

    /**
     * Return true if menu bar is visible.
     */
    public boolean isMenuBarVisible() {
        return mMenuBarVisible;
    }


    /**
     * Return true if user is looking faraway from desktop.
     */
    public boolean isLookingFarawayFromDesktop() {
        if (mForwardVector[2] > -EPSILON) {
            // If user is looking towards the back.
            return true;
        }

        // Calculate half desktop looking angle.
        double theta;
        synchronized (mCameraPositionLock) {
            theta = Math.atan(mDesktop.getHalfWidth()
                    / (DESKTOP_POSITION_Z - mCameraPosition));
        }

        // Calculate current looking angle.
        double phi = Math.atan(mForwardVector[0] / mForwardVector[2]);

        return Math.abs(phi) > FARAWAY_ANGLE_RATIO * Math.abs(theta);
    }
}