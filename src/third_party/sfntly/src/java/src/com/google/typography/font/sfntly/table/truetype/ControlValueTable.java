/*
 * Copyright 2010 Google Inc. All Rights Reserved.
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

package com.google.typography.font.sfntly.table.truetype;

import com.google.typography.font.sfntly.data.FontData;
import com.google.typography.font.sfntly.data.ReadableFontData;
import com.google.typography.font.sfntly.data.WritableFontData;
import com.google.typography.font.sfntly.table.ByteArrayTableBuilder;
import com.google.typography.font.sfntly.table.Header;
import com.google.typography.font.sfntly.table.Table;

/**
 * A Control Value table.
 * 
 * @author Stuart Gill
 */
public final class ControlValueTable extends Table {

  /**
   * Constructor.
   *
   * @param header table header
   * @param data the font data block for this table
   */
  protected ControlValueTable(Header header, ReadableFontData data) {
    super(header, data);
  }

  /**
   * Get the data value at the specified index.
   * @param index the location to get the data from
   * @return the data at the index
   */
  public int fword(int index) {
    return this.data.readFWord(index);
  }

  /**
   * Get the number of FWORDs in the data.
   * @return the number of FWORDs in the data
   */
  public int fwordCount() {
    return this.dataLength() / FontData.DataSize.FWORD.size();
  }

  /**
   * Builder for Control Value tables.
   *
   */
  public static class Builder extends ByteArrayTableBuilder<ControlValueTable> {

    /**
     * Create a new builder using the header information and data provided.
     *
     * @param header the header information
     * @param data the data holding the table
     * @return a new builder
     */
    public static Builder createBuilder(Header header, WritableFontData data) {
      return new Builder(header, data);
    }

    /**
     * Constructor.
     *
     * @param header the table header
     * @param data the writable data for the table
     */
    protected Builder(Header header, WritableFontData data) {
      super(header, data);
    }

    /**
     * Constructor.
     *
     * @param header the table header
     * @param data the readable data for the table
     */
    protected Builder(Header header, ReadableFontData data) {
      super(header, data);
    }

    @Override
    protected ControlValueTable subBuildTable(ReadableFontData data) {
      return new ControlValueTable(this.header(), data);
    }
  }
}
