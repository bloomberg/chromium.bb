/*
 * Copyright 2011 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.typography.font.tools.subsetter;

import com.google.typography.font.sfntly.Font;
import com.google.typography.font.sfntly.FontFactory;
import com.google.typography.font.sfntly.Tag;
import com.google.typography.font.sfntly.table.core.CMap;
import com.google.typography.font.sfntly.table.core.CMapTable;
import com.google.typography.font.sfntly.table.core.HorizontalHeaderTable;
import com.google.typography.font.sfntly.table.core.HorizontalMetricsTable;
import com.google.typography.font.sfntly.table.core.MaximumProfileTable;
import com.google.typography.font.sfntly.table.core.PostScriptTable;
import com.google.typography.font.sfntly.table.truetype.CompositeGlyph;
import com.google.typography.font.sfntly.table.truetype.Glyph;
import com.google.typography.font.sfntly.table.truetype.Glyph.GlyphType;
import com.google.typography.font.sfntly.table.truetype.GlyphTable;
import com.google.typography.font.sfntly.table.truetype.LocaTable;
import com.google.typography.font.sfntly.table.truetype.SimpleGlyph;
import com.google.typography.font.sfntly.testutils.TestFont.TestFontNames;
import com.google.typography.font.sfntly.testutils.TestFontUtils;

import junit.framework.TestCase;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * @author Raph Levien
 */
public class RenumberingSubsetTest extends TestCase {

  private static final File fontFile = TestFontNames.OPENSANS.getFile();

  // The subsetted font - individual tests will query and validate aspects of it
  Font dstFont;
  
  @Override
  public void setUp() throws IOException {
    Font srcFont = TestFontUtils.loadFont(fontFile)[0];

    FontFactory factory = FontFactory.getInstance();
    Subsetter subsetter = new RenumberingSubsetter(srcFont, factory);
    List<Integer> glyphs = new ArrayList<Integer>();
    glyphs.add(0);
    glyphs.add(67); // grave, u+0060
    glyphs.add(68); // a, u+0061
    glyphs.add(162); // agrave, u+00e0
    subsetter.setGlyphs(glyphs);

    Set<Integer> removeTables = new HashSet<Integer>();
    removeTables.add(Tag.GPOS);
    removeTables.add(Tag.GSUB);
    removeTables.add(Tag.kern);
    subsetter.setRemoveTables(removeTables);
    Font.Builder dstFontBuilder = subsetter.subset();

    dstFont = dstFontBuilder.build();
  }
  
  public void testNumGlyphs() {
    MaximumProfileTable maxpTable = dstFont.getTable(Tag.maxp);
    assertEquals(4, maxpTable.numGlyphs());

    LocaTable locaTable = dstFont.getTable(Tag.loca);
    assertEquals(4, locaTable.numGlyphs());
  }
  
  public void testCmap() throws IOException {
    CMapTable cmapTable = dstFont.getTable(Tag.cmap);
    assertEquals(1, cmapTable.numCMaps(), 1);
    CMap cmap = cmapTable.cmap(0);
    assertEquals(CMapTable.CMapId.WINDOWS_BMP, cmap.cmapId());
    assertEquals(1, cmap.glyphId(0x60));
    assertEquals(2, cmap.glyphId(0x61));
    assertEquals(3, cmap.glyphId(0xe0));
  }
  
  public void testHorizontalMetrics() {
    HorizontalMetricsTable hmtxTable = dstFont.getTable(Tag.hmtx);
    assertEquals(1229, hmtxTable.advanceWidth(0));
    assertEquals(193, hmtxTable.leftSideBearing(0));
    assertEquals(1182, hmtxTable.advanceWidth(1));
    assertEquals(393, hmtxTable.leftSideBearing(1));
    assertEquals(1139, hmtxTable.advanceWidth(2));
    assertEquals(94, hmtxTable.leftSideBearing(2));
    assertEquals(1139, hmtxTable.advanceWidth(3));
    assertEquals(94, hmtxTable.leftSideBearing(3));
  }
  
  public void testHorizontalHeader() {
    HorizontalHeaderTable hheaTable = dstFont.getTable(Tag.hhea);
    assertEquals(3, hheaTable.numberOfHMetrics());
    assertEquals(1229, hheaTable.advanceWidthMax());
  }
  
  public void testPostScriptTable() {
    PostScriptTable postTable = dstFont.getTable(Tag.post);
    assertEquals(4, postTable.numberOfGlyphs());
    assertEquals(".notdef", postTable.glyphName(0));
    assertEquals("grave", postTable.glyphName(1));
    assertEquals("a", postTable.glyphName(2));
    assertEquals("agrave", postTable.glyphName(3));
  }
  
  public void testSimpleGlyph1() {
    // grave
    Glyph glyph = getGlyph(dstFont, 1);
    assertEquals(GlyphType.Simple, glyph.glyphType());
    SimpleGlyph simple = (SimpleGlyph) glyph;
    assertEquals(1, simple.numberOfContours());
    assertEquals(10, simple.numberOfPoints(0));
    assertEquals(19, simple.instructionSize());
    assertTrue(simple.onCurve(0, 0));
    assertEquals(786, simple.xCoordinate(0, 0));
    assertEquals(1241, simple.yCoordinate(0, 0));
    assertTrue(simple.onCurve(0, 1));
    assertEquals(676, simple.xCoordinate(0, 1));
    assertEquals(1241, simple.yCoordinate(0, 1));
    // ... (no need to test every last point)
    assertTrue(simple.onCurve(0, 9));
    assertEquals(786, simple.xCoordinate(0, 9));
    assertEquals(1266, simple.yCoordinate(0, 9));
    assertEquals(0, simple.padding());
  }

  public void testSimpleGlyph2() {
    // lowercase a
    Glyph glyph = getGlyph(dstFont, 2);
    assertEquals(GlyphType.Simple, glyph.glyphType());
    SimpleGlyph simple = (SimpleGlyph) glyph;
    assertEquals(2, simple.numberOfContours());
    assertEquals(26, simple.numberOfPoints(0));
    assertEquals(11, simple.numberOfPoints(1));
    assertEquals(71, simple.instructionSize());
    // ...
    assertEquals(0, simple.padding());
  }

  public void testCompositeGlyph() {
    // agrave
    Glyph glyph = getGlyph(dstFont, 3);
    assertEquals(GlyphType.Composite, glyph.glyphType());
    CompositeGlyph composite = (CompositeGlyph) glyph;
    assertEquals(2, composite.numGlyphs());
    assertEquals(2, composite.glyphIndex(0));  // a
    assertEquals(0, composite.argument1(0));
    assertEquals(0, composite.argument2(0));
    assertEquals(1, composite.glyphIndex(1));  // grave
    assertEquals(-114, composite.argument1(1));
    assertEquals(0, composite.argument2(1));
    assertEquals(8, composite.instructionSize());
    assertEquals(0, composite.padding());
  }
  
  public void testTablesRemoved() {
    assertNull(dstFont.getTable(Tag.GPOS));
    assertNull(dstFont.getTable(Tag.GSUB));
    assertNull(dstFont.getTable(Tag.kern));
  }
  
  // TODO: this really needs to be a utility method somewhere
  private static Glyph getGlyph(Font font, int glyphId) {
    LocaTable locaTable = font.getTable(Tag.loca);
    GlyphTable glyfTable = font.getTable(Tag.glyf);
    int offset = locaTable.glyphOffset(glyphId);
    int length = locaTable.glyphLength(glyphId);
    return glyfTable.glyph(offset, length);
  }
}
