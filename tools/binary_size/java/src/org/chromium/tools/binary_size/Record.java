// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.tools.binary_size;

/**
 * A record that is filled in partially by nm and partially by addr2line,
 * along with tracking information about whether or not the lookup in
 * addr2line was successful.
 */
class Record {
    /**
     * The base-16 address, as a string.
     */
    String address;

    /**
     * The symbol type.
     */
    char symbolType;

    /**
     * The name of the symbol. Note that this may include whitespace, but
     * not tabs.
     */
    String symbolName;

    /**
     * The base-10 size in bytes, as a String.
     */
    String size;

    /**
     * The location, if available; may include a file name and, optionally,
     * a colon separator character followed by a line number or a
     * question mark.
     */
    String location;

    /**
     * Whether or not the record was successfully resolved. Records that are
     * successfully resolved should have a non-null location.
     */
    boolean resolvedSuccessfully;
}