// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import org.chromium.mojo.system.Core;

/**
 * Base class for all mojo structs.
 */
public abstract class Struct {

    /**
     * The header for a mojo complex element.
     */
    public static final class DataHeader {
        /**
         * The size of a serialized header, in bytes.
         */
        public static final int HEADER_SIZE = 8;

        /**
         * The offset of the size field.
         */
        public static final int SIZE_OFFSET = 0;

        /**
         * The offset of the number of fields field.
         */
        public static final int ELEMENTS_OR_VERSION_OFFSET = 4;

        /**
         * The size of the object owning this header.
         */
        public final int size;

        /**
         * Number of element (for an array) or version (for a struct) of the object owning this
         * header.
         */
        public final int elementsOrVersion;

        /**
         * Constructor.
         */
        public DataHeader(int size, int elementsOrVersion) {
            super();
            this.size = size;
            this.elementsOrVersion = elementsOrVersion;
        }

        /**
         * @see Object#hashCode()
         */
        @Override
        public int hashCode() {
            final int prime = 31;
            int result = 1;
            result = prime * result + elementsOrVersion;
            result = prime * result + size;
            return result;
        }

        /**
         * @see Object#equals(Object)
         */
        @Override
        public boolean equals(Object object) {
            if (object == this)
                return true;
            if (object == null)
                return false;
            if (getClass() != object.getClass())
                return false;

            DataHeader other = (DataHeader) object;
            return (elementsOrVersion == other.elementsOrVersion && size == other.size);
        }
    }

    /**
     * The base size of the struct.
     */
    protected final int mEncodedBaseSize;

    /**
     * Constructor.
     */
    protected Struct(int encodedBaseSize) {
        this.mEncodedBaseSize = encodedBaseSize;
    }

    /**
     * Use the given encoder to serialized this struct.
     */
    protected abstract void encode(Encoder encoder);

    /**
     * Returns the serialization of the struct. This method can close Handles.
     *
     * @param core the |Core| implementation used to generate handles. Only used if the |Struct|
     *            being encoded contains interfaces, can be |null| otherwise.
     */
    public Message serialize(Core core) {
        Encoder encoder = new Encoder(core, mEncodedBaseSize);
        encode(encoder);
        return encoder.getMessage();
    }

    /**
     * Returns the serialization of the struct prepended with the given header.
     *
     * @param header the header to prepend to the returned message.
     * @param core the |Core| implementation used to generate handles. Only used if the |Struct|
     *            being encoded contains interfaces, can be |null| otherwise.
     */
    public ServiceMessage serializeWithHeader(Core core, MessageHeader header) {
        Encoder encoder = new Encoder(core, mEncodedBaseSize + header.getSize());
        header.encode(encoder);
        encode(encoder);
        return new ServiceMessage(encoder.getMessage(), header);
    }

}
