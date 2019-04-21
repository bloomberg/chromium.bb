// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.nfc;

import android.net.Uri;
import android.os.Build;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.device.mojom.NdefMessage;
import org.chromium.device.mojom.NdefRecord;
import org.chromium.device.mojom.NdefRecordType;

import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Utility class that provides convesion between Android NdefMessage
 * and mojo NdefMessage data structures.
 */
public final class NfcTypeConverter {
    private static final String TAG = "NfcTypeConverter";
    private static final String DOMAIN = "w3.org";
    private static final String TYPE = "webnfc";
    private static final String WEBNFC_URN = DOMAIN + ":" + TYPE;
    private static final String TEXT_MIME = "text/plain";
    private static final String JSON_MIME = "application/json";
    private static final String CHARSET_UTF8 = ";charset=UTF-8";
    private static final String CHARSET_UTF16 = ";charset=UTF-16";

    /**
     * Converts mojo NdefMessage to android.nfc.NdefMessage
     */
    public static android.nfc.NdefMessage toNdefMessage(NdefMessage message)
            throws InvalidNdefMessageException {
        try {
            List<android.nfc.NdefRecord> records = new ArrayList<android.nfc.NdefRecord>();
            for (int i = 0; i < message.data.length; ++i) {
                records.add(toNdefRecord(message.data[i]));
            }
            records.add(android.nfc.NdefRecord.createExternal(
                    DOMAIN, TYPE, ApiCompatibilityUtils.getBytesUtf8(message.url)));
            android.nfc.NdefRecord[] ndefRecords = new android.nfc.NdefRecord[records.size()];
            records.toArray(ndefRecords);
            return new android.nfc.NdefMessage(ndefRecords);
        } catch (UnsupportedEncodingException | InvalidNdefMessageException
                | IllegalArgumentException e) {
            throw new InvalidNdefMessageException();
        }
    }

    /**
     * Converts android.nfc.NdefMessage to mojo NdefMessage
     */
    public static NdefMessage toNdefMessage(android.nfc.NdefMessage ndefMessage)
            throws UnsupportedEncodingException {
        android.nfc.NdefRecord[] ndefRecords = ndefMessage.getRecords();
        NdefMessage webNdefMessage = new NdefMessage();
        List<NdefRecord> nfcRecords = new ArrayList<NdefRecord>();

        for (int i = 0; i < ndefRecords.length; i++) {
            if ((ndefRecords[i].getTnf() == android.nfc.NdefRecord.TNF_EXTERNAL_TYPE)
                    && (Arrays.equals(ndefRecords[i].getType(),
                            ApiCompatibilityUtils.getBytesUtf8(WEBNFC_URN)))) {
                webNdefMessage.url = new String(ndefRecords[i].getPayload(), "UTF-8");
                continue;
            }

            NdefRecord nfcRecord = toNdefRecord(ndefRecords[i]);
            if (nfcRecord != null) nfcRecords.add(nfcRecord);
        }

        webNdefMessage.data = new NdefRecord[nfcRecords.size()];
        nfcRecords.toArray(webNdefMessage.data);
        return webNdefMessage;
    }

    /**
     * Returns charset of mojo NdefRecord. Only applicable for URL and TEXT records.
     * If charset cannot be determined, UTF-8 charset is used by default.
     */
    private static String getCharset(NdefRecord record) {
        if (record.mediaType.endsWith(CHARSET_UTF8)) return "UTF-8";

        // When 16bit WTF::String data is converted to bytearray, it is in LE byte order, without
        // BOM. By default, Android interprets UTF-16 charset without BOM as UTF-16BE, thus, use
        // UTF-16LE as encoding for text data.

        if (record.mediaType.endsWith(CHARSET_UTF16)) return "UTF-16LE";

        Log.w(TAG, "Unknown charset, defaulting to UTF-8.");
        return "UTF-8";
    }

    /**
     * Converts mojo NdefRecord to android.nfc.NdefRecord
     */
    private static android.nfc.NdefRecord toNdefRecord(NdefRecord record)
            throws InvalidNdefMessageException, IllegalArgumentException,
                   UnsupportedEncodingException {
        switch (record.recordType) {
            case NdefRecordType.URL:
                return android.nfc.NdefRecord.createUri(
                        new String(record.data, getCharset(record)));
            case NdefRecordType.TEXT:
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                    return android.nfc.NdefRecord.createTextRecord(
                            "en-US", new String(record.data, getCharset(record)));
                } else {
                    return android.nfc.NdefRecord.createMime(TEXT_MIME, record.data);
                }
            case NdefRecordType.JSON:
            case NdefRecordType.OPAQUE_RECORD:
                return android.nfc.NdefRecord.createMime(record.mediaType, record.data);
            case NdefRecordType.EMPTY:
                return new android.nfc.NdefRecord(
                        android.nfc.NdefRecord.TNF_EMPTY, null, null, null);
            default:
                throw new InvalidNdefMessageException();
        }
    }

    /**
     * Converts android.nfc.NdefRecord to mojo NdefRecord
     */
    private static NdefRecord toNdefRecord(android.nfc.NdefRecord ndefRecord)
            throws UnsupportedEncodingException {
        switch (ndefRecord.getTnf()) {
            case android.nfc.NdefRecord.TNF_EMPTY:
                return createEmptyRecord();
            case android.nfc.NdefRecord.TNF_MIME_MEDIA:
                return createMIMERecord(
                        new String(ndefRecord.getType(), "UTF-8"), ndefRecord.getPayload());
            case android.nfc.NdefRecord.TNF_ABSOLUTE_URI:
                return createURLRecord(ndefRecord.toUri());
            case android.nfc.NdefRecord.TNF_WELL_KNOWN:
                return createWellKnownRecord(ndefRecord);
        }
        return null;
    }

    /**
     * Constructs empty NdefMessage
     */
    public static android.nfc.NdefMessage emptyNdefMessage() {
        return new android.nfc.NdefMessage(
                new android.nfc.NdefRecord(android.nfc.NdefRecord.TNF_EMPTY, null, null, null));
    }

    /**
     * Constructs empty NdefRecord
     */
    private static NdefRecord createEmptyRecord() {
        NdefRecord nfcRecord = new NdefRecord();
        nfcRecord.recordType = NdefRecordType.EMPTY;
        nfcRecord.mediaType = "";
        nfcRecord.data = new byte[0];
        return nfcRecord;
    }

    /**
     * Constructs URL NdefRecord
     */
    private static NdefRecord createURLRecord(Uri uri) {
        if (uri == null) return null;
        NdefRecord nfcRecord = new NdefRecord();
        nfcRecord.recordType = NdefRecordType.URL;
        nfcRecord.mediaType = TEXT_MIME;
        nfcRecord.data = ApiCompatibilityUtils.getBytesUtf8(uri.toString());
        return nfcRecord;
    }

    /**
     * Constructs MIME or JSON NdefRecord
     */
    private static NdefRecord createMIMERecord(String mediaType, byte[] payload) {
        NdefRecord nfcRecord = new NdefRecord();
        if (mediaType.equals(JSON_MIME)) {
            nfcRecord.recordType = NdefRecordType.JSON;
        } else {
            nfcRecord.recordType = NdefRecordType.OPAQUE_RECORD;
        }
        nfcRecord.mediaType = mediaType;
        nfcRecord.data = payload;
        return nfcRecord;
    }

    /**
     * Constructs TEXT NdefRecord
     */
    private static NdefRecord createTextRecord(byte[] text) {
        // Check that text byte array is not empty.
        if (text.length == 0) {
            return null;
        }

        NdefRecord nfcRecord = new NdefRecord();
        nfcRecord.recordType = NdefRecordType.TEXT;
        nfcRecord.mediaType = TEXT_MIME;
        // According to NFCForum-TS-RTD_Text_1.0 specification, section 3.2.1 Syntax.
        // First byte of the payload is status byte, defined in Table 3: Status Byte Encodings.
        // 0-5: lang code length
        // 6  : must be zero
        // 8  : 0 - text is in UTF-8 encoding, 1 - text is in UTF-16 encoding.
        int langCodeLength = (text[0] & (byte) 0x3F);
        int textBodyStartPos = langCodeLength + 1;
        if (textBodyStartPos > text.length) {
            return null;
        }
        nfcRecord.data = Arrays.copyOfRange(text, textBodyStartPos, text.length);
        return nfcRecord;
    }

    /**
     * Constructs well known type (TEXT or URI) NdefRecord
     */
    private static NdefRecord createWellKnownRecord(android.nfc.NdefRecord record) {
        if (Arrays.equals(record.getType(), android.nfc.NdefRecord.RTD_URI)) {
            return createURLRecord(record.toUri());
        }

        if (Arrays.equals(record.getType(), android.nfc.NdefRecord.RTD_TEXT)) {
            return createTextRecord(record.getPayload());
        }

        return null;
    }
}
