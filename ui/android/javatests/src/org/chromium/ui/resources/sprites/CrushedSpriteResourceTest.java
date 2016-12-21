// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.sprites;

import android.support.test.filters.MediumTest;
import android.test.InstrumentationTestCase;
import android.test.mock.MockResources;
import android.util.DisplayMetrics;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;

/**
 * Tests for CrushedSpriteResource.
 */
public class CrushedSpriteResourceTest extends InstrumentationTestCase {
    private static final int METADATA_RESOURCE_ID = 1;

    /**
     * Tests that the metadata is parsed correctly for a dummy CrushedSpriteResource.
     */
    @MediumTest
    public void testParseMetadata() {
        MockCrushedSpriteResources mockResources = new MockCrushedSpriteResources();
        CrushedSpriteResource crushedSpriteResource = new CrushedSpriteResource(
                0, METADATA_RESOURCE_ID, mockResources);
        try {
            crushedSpriteResource.parseMetadata(METADATA_RESOURCE_ID, DisplayMetrics.DENSITY_XXHIGH,
                    mockResources);

            int[][] expectedRectangles = {{35, 30, 38, 165, 18, 12, 0, 70, 0, 146, 72, 2},
                    {}, {0, 0, 73, 0, 72, 72}};
            int[][] actualRectangles = crushedSpriteResource.getFrameRectangles();
            float dpToPx = mockResources.getDisplayMetrics().density;

            assertEquals(108, crushedSpriteResource.getUnscaledSpriteWidth());
            assertEquals(108, crushedSpriteResource.getUnscaledSpriteHeight());
            assertEquals(36 * dpToPx, crushedSpriteResource.getScaledSpriteWidth());
            assertEquals(36 * dpToPx, crushedSpriteResource.getScaledSpriteHeight());
            assertEquals(expectedRectangles.length, actualRectangles.length);
            for (int i = 0; i < expectedRectangles.length; i++) {
                assertEquals(expectedRectangles[i].length, actualRectangles[i].length);
                for (int j = 0; j < expectedRectangles[i].length; j++) {
                    assertEquals(expectedRectangles[i][j], actualRectangles[i][j]);
                }
            }
        } catch (IOException e) {
            e.printStackTrace();
            fail();
        }
    }

    private static class MockCrushedSpriteResources extends MockResources {

        @Override
        public InputStream openRawResource(int id) {
            if (id == METADATA_RESOURCE_ID) {
                return new ByteArrayInputStream(METADATA.getBytes());
            } else {
                return null;
            }
        }

        @Override
        public DisplayMetrics getDisplayMetrics() {
            DisplayMetrics metrics = new DisplayMetrics();
            metrics.density = 2;
            return metrics;
        }

        private static final String METADATA =
                "{"
                + "\"apiVersion\": \"1.0\","
                + "\"scaledSpriteWidthDp\": 36,"
                + "\"scaledSpriteHeightDp\": 36,"
                + "\"densities\":"
                + "["
                + "  {"
                + "    \"density\": 160,"
                + "    \"width\": 36,"
                + "    \"height\": 36,"
                + "    \"rectangles\": ["
                + "       [17, 14, 47, 69, 10, 8, 0, 34, 37, 0, 36, 2],"
                + "       [],"
                + "       [0, 0, 0, 0, 36, 36]"
                + "     ]"
                + "  },"
                + "  {"
                + "    \"density\": 480,"
                + "    \"width\": 108,"
                + "    \"height\": 108,"
                + "    \"rectangles\": ["
                + "       [35, 30, 38, 165, 18, 12, 0, 70, 0, 146, 72, 2],"
                + "       [],"
                + "       [0, 0, 73, 0, 72, 72]"
                + "     ]"
                + "  },"
                + "  {"
                + "    \"density\": 640,"
                + "    \"width\": 144,"
                + "    \"height\": 144,"
                + "    \"rectangles\": ["
                + "       [71, 62, 820, 3, 34, 21, 0, 142, 725, 0, 144, 2],"
                + "       [],"
                + "       [0, 0, 145, 0, 144, 144]"
                + "     ]"
                + "  }"
                + "]}";
    }

}
