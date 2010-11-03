/*
 * TLS Snap Start
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adam Langley, Google Inc.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* $Id: ssl3snap.c,v 1.0 2010/08/09 13:00:00 agl%google.com Exp $ */


/* TODO(agl): Refactor ssl3_CompressMACEncryptRecord so that it can write to
** |sendBuf| directly and fix ssl3_AppendSnapStartHandshakeRecord and
**  ssl3_AppendSnapStartApplicationData.
*/

/* TODO(agl): Add support for snap starting with compression. */

#include "pk11pub.h"
#include "ssl.h"
#include "sslimpl.h"
#include "sslproto.h"

static unsigned int GetBE16(const void *in)
{
    const unsigned char *p = in;
    return ((unsigned) p[0]) << 8 |
                       p[1];
}

static unsigned int GetBE24(const void *in)
{
    const unsigned char *p = in;
    return ((unsigned) p[0]) << 16 |
           ((unsigned) p[1]) << 8 |
                       p[2];
}

static void PutBE16(void *out, unsigned int value)
{
    unsigned char *p = out;
    p[0] = value >> 8;
    p[1] = value;
}

static void PutBE24(void *out, unsigned int value)
{
    unsigned char *p = out;
    p[0] = value >> 16;
    p[1] = value >> 8;
    p[2] = value;
}

/* ssl3_ForEachExtension calls a callback for each TLS extension in |extensions|
**   extensions: points to a block of extensions which includes the two prefix
**       length bytes
**   is_resuming: if true, certain extensions will be omitted
**   f: a function which is called with the data of each extension, which
**      includes the four type and length bytes at the beginning.
*/
static PRBool
ssl3_ForEachExtension(const SECItem *extensions, PRBool is_resuming,
                      void (*f) (const unsigned char *data, unsigned int length,
                                 void *ctx),
                      void *ctx) {
    unsigned int extensions_len, offset;

    if (extensions->len == 0)
        return PR_TRUE;

    if (extensions->len < 2)
        goto loser;

    extensions_len = GetBE16(extensions->data);
    offset = 2;

    if (extensions->len != 2 + extensions_len)
        goto loser;

    while (extensions_len) {
        unsigned int extension_num, extension_len;

        if (extensions->len - offset < 4)
            goto loser;

        extension_num = GetBE16(extensions->data + offset);
        extension_len = GetBE16(extensions->data + offset + 2);

        if (extensions->len - offset < 4 + extension_len)
            goto loser;

        /* When resuming, the server will omit some extensions from the
         * previous non-resume ServerHello. */
        if (!is_resuming ||
            (extension_num != ssl_server_name_xtn &&
             extension_num != ssl_session_ticket_xtn)) {
            f(extensions->data + offset, 4 + extension_len, ctx);
        }

        offset += 4 + extension_len;
        extensions_len -= 4 + extension_len;
    }

    return PR_TRUE;

loser:
    PORT_SetError(SEC_ERROR_INPUT_LEN);
    return PR_FALSE;
}

static void
ssl3_AccumlateLengths(const unsigned char *data, unsigned int length, void *ptr)
{
    unsigned int *sum = (unsigned int *) ptr;
    *sum += length;
}

/* ssl3_PredictServerResponse predicts the contents of the server's
** ServerHello...ServerHelloDone (inclusive) and progressively calls a callback
** with the contents of those messages.
**   previous_server_hello: the contents of a previous ServerHello from the
**       server where the 'random' field has been replaced with our suggested
**       server random.
**   is_resuming: if false, Certificate and ServerHelloDone messages will be
**      predicted
**   hashUpdate: a callback which is called repeated with the contents of the
**      predicted messages.
*/
static PRBool
ssl3_PredictServerResponse(
        sslSocket *ss, SECItem *previous_server_hello, PRBool is_resuming,
        void (*hashUpdate) (const unsigned char *data, unsigned int length,
                            void *ctx),
        void *ctx) {
    unsigned int old_session_id_length, old_extensions_len;
    unsigned int extensions_len, server_hello_len;
    unsigned char session_id_len, header[4];
    SECItem extensions;

    /* Keep the structure of a ServerHello in mind when reading the following:
     *
     * struct ServerHello {
     *   uint16_t version;
     *   uint8_t  random[32];
     *   uint8_t  session_id_len;
     *   uint8_t  session_id[session_id_len];
     *   uint16_t cipher
     *   uint8_t  compression
     *
     *   // Optional:
     *   uint16_t extensions_len;
     *   struct Extension {
     *     uint16_t num;
     *     uint16_t len;
     *     uint8_t  payload[len];
     *   }
     */

    /* 38 bytes is the shortest possible ServerHello with a zero-length
     * session_id and no extensions. */
    if (previous_server_hello->len < 38) {
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return PR_FALSE;
    }

    /* First we need to figure out the length of the predicted ServerHello. Any
     * session id in |previous_server_hello| needs to be removed
     * (or replaced). */
    old_session_id_length = previous_server_hello->data[34];

    extensions.len = 0;

    if (previous_server_hello->len >= 35 + old_session_id_length + 3 + 2) {
        /* Extensions present */
        unsigned int offset = 35 + old_session_id_length + 3;
        extensions.data = previous_server_hello->data + offset;
        extensions.len = previous_server_hello->len - offset;
    }

    /* Sum the lengths of all the extensions that we wish to include */
    extensions_len = 0;
    if (!ssl3_ForEachExtension(&extensions, is_resuming, ssl3_AccumlateLengths,
                               &extensions_len)) {
        return PR_FALSE;
    }

    old_extensions_len =
        (previous_server_hello->len - 35 - old_session_id_length - 3 - 2);

    session_id_len = 0;
    if (ss->sec.ci.sid)
        session_id_len = ss->sec.ci.sid->u.ssl3.sessionIDLength;
    server_hello_len = previous_server_hello->len +
                       session_id_len - old_session_id_length +
                       extensions_len - old_extensions_len;

    header[0] = server_hello;
    PutBE24(header + 1, server_hello_len);
    hashUpdate(header, 4, ctx);

    hashUpdate(previous_server_hello->data, 34, ctx);
    hashUpdate(&session_id_len, sizeof(session_id_len), ctx);
    if (session_id_len)
        hashUpdate(ss->sec.ci.sid->u.ssl3.sessionID, session_id_len, ctx);
    hashUpdate(previous_server_hello->data + 35 + old_session_id_length, 3,
               ctx);

    if (extensions.len) {
        PutBE16(header, extensions_len);
        hashUpdate(header, 2, ctx);

        if (!ssl3_ForEachExtension(&extensions, is_resuming, hashUpdate, ctx))
            return PR_FALSE;
    }

    if (!is_resuming) {
        unsigned int certificate_message_len = 3, i;
        for (i = 0; ss->ssl3.predictedCertChain[i]; i++) {
            certificate_message_len += 3;
            certificate_message_len +=
                ss->ssl3.predictedCertChain[i]->derCert.len;
        }

        header[0] = certificate;
        PutBE24(header + 1, certificate_message_len);
        hashUpdate(header, 4, ctx);

        PutBE24(header, certificate_message_len - 3);
        hashUpdate(header, 3, ctx);

        for (i = 0; ss->ssl3.predictedCertChain[i]; i++) {
            unsigned int len = ss->ssl3.predictedCertChain[i]->derCert.len;
            PutBE24(header, len);
            hashUpdate(header, 3, ctx);
            hashUpdate(ss->ssl3.predictedCertChain[i]->derCert.data, len, ctx);
        }

        header[0] = server_hello_done;
        header[1] = header[2] = header[3] = 0;
        hashUpdate(header, 4, ctx);
    }

    return PR_TRUE;
}

/* ssl3_SnapStartHash is called with the contents of the server's predicted
 * response and updates both the Finished hash and an FNV641a hash. */
static void
ssl3_SnapStartHash(const unsigned char *data, unsigned int len, void *ctx)
{
    SECStatus rv;
    void **ptrs = (void **) ctx;
    sslSocket *ss = ptrs[0];
    PRUint64 *fnv = ptrs[1];

    FNV1A64_Update(fnv, data, len);
    rv = ssl3_UpdateHandshakeHashes(ss, (unsigned char *) data, len);
    if (rv != SECSuccess)
        PR_Assert("rv == SECSuccess", __FILE__, __LINE__);
}

static PRInt32
ssl3_SendEmptySnapStartXtn(sslSocket *ss, PRBool append, PRUint32 maxBytes)
{
    SECStatus rv;

    if (maxBytes < 4)
        return 0;

    if (append) {
        rv = ssl3_AppendHandshakeNumber(ss, ssl_snap_start_xtn, 2);
        if (rv != SECSuccess)
            return -1;
        rv = ssl3_AppendHandshakeNumber(ss, 0 /* empty extension */, 2);
        if (rv != SECSuccess)
            return -1;
        if (!ss->sec.isServer) {
            TLSExtensionData *xtnData = &ss->xtnData;
            xtnData->advertised[xtnData->numAdvertised++] = ssl_snap_start_xtn;
        }
    }
    return 4;
}

static SECStatus
ssl3_BufferEnsure(sslBuffer *buf, unsigned int extra_bytes)
{
    if (buf->space < buf->len + extra_bytes)
        return sslBuffer_Grow(buf, buf->len + extra_bytes);
    return SECSuccess;
}

/* ssl3_AppendSnapStartRecordHeader appends a 5 byte TLS record header to the
 * sendBuf of the given sslSocket. */
static SECStatus
ssl3_AppendSnapStartRecordHeader(sslSocket *ss, SSL3ContentType type,
                                 unsigned int len)
{
    SECStatus rv;

    rv = ssl3_BufferEnsure(&ss->sec.ci.sendBuf, 5);
    if (rv != SECSuccess)
        return rv;
    ss->sec.ci.sendBuf.buf[ss->sec.ci.sendBuf.len + 0] = type;
    PutBE16(&ss->sec.ci.sendBuf.buf[ss->sec.ci.sendBuf.len + 1], ss->version);
    PutBE16(&ss->sec.ci.sendBuf.buf[ss->sec.ci.sendBuf.len + 3], len);
    ss->sec.ci.sendBuf.len += 5;
    return SECSuccess;
}

/* ssl3_AppendSnapStartHandshakeRecord appends a (possibly encrypted) record to
** the sendBuf of the given sslSocket.
**   f: a function which will append the bytes of the record (not including the
**      5 byte header) to the sendBuf.
*/
static SECStatus
ssl3_AppendSnapStartHandshakeRecord(sslSocket *ss, SECStatus (*f) (sslSocket*),
                                    PRBool encrypt)
{
    SECStatus rv;
    unsigned int record_offset, record_len;

    /* ssl3_CompressMACEncryptRecord will deal with the record header if we are
     * encrypting. */
    if (!encrypt) {
        /* The zero length argument here is a dummy value. We write the real
         * length once we know it, below. */
        rv = ssl3_AppendSnapStartRecordHeader(ss, content_handshake, 0);
        if (rv != SECSuccess)
            return rv;
    }

    record_offset = ss->sec.ci.sendBuf.len;
    rv = f(ss);
    if (rv != SECSuccess)
        return rv;
    record_len = ss->sec.ci.sendBuf.len - record_offset;
    if (!encrypt) {
        PutBE16(&ss->sec.ci.sendBuf.buf[record_offset - 2], record_len);
    } else {
        /* ssl3_CompressMACEncryptRecord writes to |ss->sec.writeBuf|
         * so we copy it back to |ss->sec.ci.sendBuf| */
        /* TODO(agl): the buffer copy here is a bodge. See TODO at the top of
         * the file. */
        rv = ssl3_CompressMACEncryptRecord(
            ss, content_handshake,
            &ss->sec.ci.sendBuf.buf[record_offset], record_len);
        if (rv != SECSuccess)
            return rv;
        ss->sec.ci.sendBuf.len -= record_len;
        ssl3_BufferEnsure(&ss->sec.ci.sendBuf, ss->sec.writeBuf.len);
        memcpy(&ss->sec.ci.sendBuf.buf[record_offset], ss->sec.writeBuf.buf,
               ss->sec.writeBuf.len);
        ss->sec.ci.sendBuf.len += ss->sec.writeBuf.len;
        ss->sec.writeBuf.len = 0;
    }

    return SECSuccess;
}

/* ssl3_AppendSnapStartApplicationData appends an encrypted Application Data
** record the sendBuf of the given sslSocket.
*/
static SECStatus ssl3_AppendSnapStartApplicationData(
    sslSocket *ss, const SSL3Opaque *data, unsigned int len)
{
    SECStatus rv;

    /* TODO(agl): the buffer copy here is a bodge. See TODO at the top of the
     * file. */
    rv = ssl3_CompressMACEncryptRecord(ss, content_application_data, data, len);
    if (rv != SECSuccess)
        return rv;
    rv = ssl3_BufferEnsure(&ss->sec.ci.sendBuf, ss->sec.writeBuf.len);
    if (rv != SECSuccess)
        return rv;
    memcpy(&ss->sec.ci.sendBuf.buf[ss->sec.ci.sendBuf.len],
           ss->sec.writeBuf.buf, ss->sec.writeBuf.len);
    ss->sec.ci.sendBuf.len += ss->sec.writeBuf.len;
    ss->sec.writeBuf.len = 0;

    return SECSuccess;
}

static SECStatus ssl3_SendSnapStartFinished(sslSocket *ss)
{
    /* We use ssl_SEND_FLAG_NO_FLUSH here because this finished message is
     * going to end up in the middle of the Snap Start extension. So
     * transmitting |sendBuf| at this point would result in an incomplete
     * ClientHello. */
    return ssl3_SendFinished(ss, ssl_SEND_FLAG_NO_FLUSH);
}

/* ssl3_FindOrbit is called for each extension in a ServerHello message. It
** tests for a Snap Start extension and records the server's orbit when found.
**   data: the extension data (including the four type and length bytes)
**   length: the length, in bytes, of |data|
**   ptr: a pointer to a uint8_t[9]. The orbit, if found, is copied into the
**       first 8 bytes and then the ninth byte is set to one.
*/
static void
ssl3_FindOrbit(const unsigned char *data, unsigned int length, void *ptr)
{
    unsigned char *orbit = (unsigned char *) ptr;

    unsigned int extension_num = GetBE16(data);
    if (extension_num == ssl_snap_start_xtn &&
        length == 4 + 8 /* orbit */ + 2 /* snap start cipher suite */) {
        memcpy(orbit, data + 4, 8);
        /* A last byte of 1 indicates that the previous eight are valid. */
        orbit[8] = 1;
    }
}

/* ssl3_CanSnapStart returns true if we are able to perform Snap Start on
** the given socket.
**   extensions: on successful return, this is filled in with the contents of
**       the server's predicted extensions. This points within
**       |ss->ssl3.serverHelloPredictionData|.
**   resuming: PR_TRUE iff we wish to attempt a Snap Start resume
**   out_orbit: if this function returns PR_TRUE, then |out_orbit| is filled
**       with the server's predicted orbit value.
** The |hs.cipher_suite|, |hs.cipher_def| and |hs.compression| fields of |ss|
** are set to match the predicted ServerHello on successful exit (and may still
** be modified on failure).
*/
static PRBool
ssl3_CanSnapStart(sslSocket *ss, SECItem *extensions, PRBool resuming,
                  unsigned char out_orbit[8]) {
    const unsigned char *server_hello;
    unsigned int server_hello_len, session_id_len, cipher_suite_offset;
    unsigned int extensions_offset, cipher_suite, compression_method;
    unsigned char orbit[9];
    SECStatus rv;
    SSL3ProtocolVersion version;

    /* If we don't have the information needed then we can't perform a Snap
     * Start. */
    if (!ss->ssl3.predictedCertChain || !ss->ssl3.serverHelloPredictionData.data)
        return PR_FALSE;

    /* When the sizes of the fields in the ClientHello are calculated, they'll
     * take the length of the Snap Start extension to be zero, so currently
     * it's as if this extension didn't exist, which is the state that we
     * need. */

    server_hello = ss->ssl3.serverHelloPredictionData.data;
    server_hello_len = ss->ssl3.serverHelloPredictionData.len;

    if (server_hello_len < 2 + 32 + 1)
        return PR_FALSE;
    session_id_len = server_hello[2 + 32];
    cipher_suite_offset = 2 + 32 + 1 + session_id_len;
    if (server_hello_len < cipher_suite_offset + 3)
        return PR_FALSE;
    extensions_offset = cipher_suite_offset + 3;

    version = (SSL3ProtocolVersion) GetBE16(server_hello);
    if (MSB(version) < MSB(SSL_LIBRARY_VERSION_3_0))
        return PR_FALSE;
    rv = ssl3_NegotiateVersion(ss, version);
    if (rv != SECSuccess)
        return PR_FALSE;

    cipher_suite = GetBE16(&server_hello[cipher_suite_offset]);
    ss->ssl3.hs.cipher_suite = (ssl3CipherSuite)cipher_suite;
    ss->ssl3.hs.suite_def = ssl_LookupCipherSuiteDef(ss->ssl3.hs.cipher_suite);
    if (!ss->ssl3.hs.suite_def)
        return PR_FALSE;
    compression_method = server_hello[cipher_suite_offset + 2];
    if (compression_method != ssl_compression_null) {
        /* TODO(agl): support compression. */
        return PR_FALSE;
    }
    ss->ssl3.hs.compression = ssl_compression_null;

    extensions->data = (unsigned char *) server_hello + extensions_offset;
    extensions->len = server_hello_len - extensions_offset;

    /* The last byte is used to indictate that the previous eight are valid. */
    orbit[8] = 0;
    if (!ssl3_ForEachExtension(extensions, resuming, ssl3_FindOrbit, orbit))
        return PR_FALSE;

    if (!orbit[8])
        return PR_FALSE;

    memcpy(out_orbit, orbit, 8);

    return PR_TRUE;
}

/* ssl3_UpdateClientHelloLengths rewrites the handshake header length and
** extensions length in the ClientHello to reflect the addition of the Snap
** Start extension.
**   snap_start_extension_len_offset: the number of bytes of the ClientHello to
**       skip in order to find the embedded length of the Snap Start extension.
*/
static void
ssl3_UpdateClientHelloLengths(sslSocket *ss,
                              unsigned int snap_start_extension_len_offset,
                              sslSessionID *sid) {
    unsigned int extension_length, old_length, new_length, new_session_id_len;
    unsigned int offset, ciphers_length, compressions_len;

    extension_length =
        ss->sec.ci.sendBuf.len - snap_start_extension_len_offset - 2;
    PutBE16(&ss->sec.ci.sendBuf.buf[snap_start_extension_len_offset],
            extension_length);

    /* The length in the handshake header is short by extension_length + 4
     * bytes. */
    old_length = GetBE24(&ss->sec.ci.sendBuf.buf[1]);
    new_length = old_length + extension_length + 4;
    PutBE24(&ss->sec.ci.sendBuf.buf[1], new_length);

    /* The length of the extensions block is similarly wrong. */
    new_session_id_len = 0;
    if (sid)
        new_session_id_len = sid->u.ssl3.sessionIDLength;
    offset = 4 + 2 + 32 + 1 + new_session_id_len;
    ciphers_length = GetBE16(&ss->sec.ci.sendBuf.buf[offset]);
    offset += 2 + ciphers_length;
    compressions_len = ss->sec.ci.sendBuf.buf[offset];
    offset += 1 + compressions_len;
    old_length = GetBE16(&ss->sec.ci.sendBuf.buf[offset]);
    new_length = old_length + extension_length + 4;
    PutBE16(&ss->sec.ci.sendBuf.buf[offset], new_length);
}

/* ssl3_FindServerNPNExtension is a callback function for ssl3_ForEachExtension.
 * It looks for a Next Protocol Negotiation and saves the payload of the
 * extension in the given SECItem */
static void
ssl3_FindServerNPNExtension(const unsigned char* data, unsigned int length,
                            void *ctx)
{
    SECItem *server_npn_extension = (SECItem*) ctx;

    unsigned int extension_num = GetBE16(data);
    if (extension_num == ssl_next_proto_neg_xtn && length >= 4) {
        server_npn_extension->data = (unsigned char*)data + 4;
        server_npn_extension->len = length - 4;
    }
}

/* ssl3_MaybeWriteNextProtocol deals with the interaction of Next Protocol
 * Negotiation and Snap Start. It's called just before we serialise the embedded
 * Finished message in the extension. At this point, if NPN is enabled, we have
 * to include a NextProtocol message. */
static SECStatus
ssl3_MaybeWriteNextProtocol(sslSocket *ss, SECItem *server_hello_extensions)
{
    PRUint16 i16;
    SECItem server_npn_extension;

    for (i16 = 0; i16 < ss->xtnData.numAdvertised; i16++) {
        if (ss->xtnData.advertised[i16] == ssl_next_proto_neg_xtn)
            break;
    }

    if (i16 == ss->xtnData.numAdvertised) {
        /* We didn't send an NPN extension, so no need to do anything here. */
        return SECSuccess;
    }

    memset(&server_npn_extension, 0, sizeof(server_npn_extension));

    ssl3_ForEachExtension(
        server_hello_extensions, PR_FALSE /* is_resuming: value doesn't matter
        in this case */, ssl3_FindServerNPNExtension, &server_npn_extension);

    if (server_npn_extension.data == NULL) {
        /* We predicted that the server doesn't support NPN, so nothing to do
         * here. */
        return SECSuccess;
    }

    ssl3_ClientHandleNextProtoNegoXtn(ss, ssl_next_proto_neg_xtn,
                                      &server_npn_extension);

    if (ss->ssl3.nextProtoState == SSL_NEXT_PROTO_NO_SUPPORT) {
        /* The server's predicted NPN extension was malformed. We're didn't pick
         * a protocol so we won't send a NextProtocol message. However, this is
         * probably fatal to the connection. */
        return SECSuccess;
    }

    return ssl3_AppendSnapStartHandshakeRecord(ss, ssl3_SendNextProto,
                                               PR_TRUE /* encrypt */);
}

/* ssl3_SendSnapStartXtn appends a Snap Start extension. It assumes that the
 * inchoate ClientHello is in |ss->sec.ci.sendBuf|. */
PRInt32
ssl3_SendSnapStartXtn(sslSocket *ss, PRBool append, PRUint32 maxBytes)
{
    unsigned char orbit[8];
    PRBool resuming = PR_FALSE;
    unsigned char suggested_server_random[32];
    SECStatus rv;
    PRUint64 fnv;
    /* The context for |ssl3_SnapStartHash|. The first pointer points to |ss|
     * and the second to the running FNV1A64 hash of the predicted server
     * response. */
    void *ctx[2];
    unsigned int snap_start_extension_len_offset, original_sendbuf_len;
    ssl3CipherSpec *temp;
    sslSessionID *sid;
    SECItem server_extensions;

    if (!ss->opt.enableSnapStart)
        return 0;

    original_sendbuf_len = ss->sec.ci.sendBuf.len;

    /* This function is called twice for each ClientHello emitted. The first
     * time around is to calculate the sizes of the extension (|append| is
     * false). The second time around is to actually write out the bytes
     * (|append| is true).
     *
     * We always return 0 bytes in each case because we want to be able to hash
     * the inchoate ClientHello as if this extension was missing: that's why
     * it's important that this always be the last extension serialised. */
    sid = ss->sec.ci.sid;

    if (!ss->opt.enableSessionTickets || ss->sec.isServer)
        return 0;

    /* If we are sending a SessionTicket then the first time around
     * ticketTimestampVerified will be true but it's reset after serialising
     * the session ticket extension, so we have
     * |clientSentNonEmptySessionTicket|. */
    if (ss->xtnData.clientSentNonEmptySessionTicket) {
        resuming = PR_TRUE;
    } else if (sid->u.ssl3.sessionTicket.ticket.data &&
               ss->xtnData.ticketTimestampVerified) {
        resuming = PR_TRUE;
    }

    if (!ssl3_CanSnapStart(ss, &server_extensions, resuming, orbit))
        return ssl3_SendEmptySnapStartXtn(ss, append, maxBytes);

    /* At this point we are happy that we are going to send a non-empty Snap
     * Start extension. If we are still calculating length then we lie and
     * return 0 so that everything is set up as if the extension didn't exist
     * when this function is called again later. */

    if (!append)
        return 0;

    /* |ss->sec.ci.sendBuf.buf| contains the inchoate ClientHello. This copies
     * the ClientHello's gmt_unix_time into the suggested server random. */
    memcpy(suggested_server_random, ss->sec.ci.sendBuf.buf + 6, 4);
    memcpy(suggested_server_random + 4, orbit, 8);
    rv = PK11_GenerateRandom(suggested_server_random + 12, 20);
    if (rv != SECSuccess)
        goto loser;
    memcpy(ss->ssl3.serverHelloPredictionData.data + 2, suggested_server_random,
           32);

    memcpy(&ss->ssl3.hs.server_random, suggested_server_random, 32);

    PORT_Assert( ss->opt.noLocks || ssl_HaveSSL3HandshakeLock(ss) );
    rv = ssl3_SetupPendingCipherSpec(ss);
    if (rv != SECSuccess)
        goto loser;
    ss->ssl3.hs.isResuming = resuming;

    FNV1A64_Init(&fnv);
    ctx[0] = ss;
    ctx[1] = &fnv;

    if (!ssl3_PredictServerResponse(ss, &ss->ssl3.serverHelloPredictionData,
                                    resuming, ssl3_SnapStartHash, ctx)) {
        /* It's not a fatal error if the predicted ServerHello was invalid. */
        return 0;
    }
    FNV1A64_Final(&fnv);

    /* Now we grow the send buffer to accomodate the extension type and length,
     * orbit, suggested random and predicted server response hash without
     * calling ssl3_AppendHandshake (which would also update the Finished
     * hash). */
    if (ssl3_BufferEnsure(&ss->sec.ci.sendBuf, 4 + 8 + 20 + 8) != SECSuccess)
        goto loser;

    PutBE16(&ss->sec.ci.sendBuf.buf[ss->sec.ci.sendBuf.len],
            ssl_snap_start_xtn);
    ss->sec.ci.sendBuf.len += 2;
    /* Skip over the length for now. */
    snap_start_extension_len_offset = ss->sec.ci.sendBuf.len;
    ss->sec.ci.sendBuf.len += 2;
    memcpy(ss->sec.ci.sendBuf.buf + ss->sec.ci.sendBuf.len, orbit, 8);
    ss->sec.ci.sendBuf.len += 8;
    memcpy(&ss->sec.ci.sendBuf.buf[ss->sec.ci.sendBuf.len],
           suggested_server_random + 12, 20);
    ss->sec.ci.sendBuf.len += 20;
    memcpy(ss->sec.ci.sendBuf.buf + ss->sec.ci.sendBuf.len, &fnv, 8);
    ss->sec.ci.sendBuf.len += 8;

    if (!resuming) {
        /* Write ClientKeyExchange */
        ss->sec.peerCert =
            CERT_DupCertificate(ss->ssl3.predictedCertChain[0]);
        rv = ssl3_AppendSnapStartHandshakeRecord(
                ss, ssl3_SendClientKeyExchange, PR_FALSE /* do not encrypt */);
        if (rv != SECSuccess)
            goto loser;
    } else {
        SSL3Finished hashes;
        TLSFinished tlsFinished;
        unsigned char hdr[4];

        rv = ssl3_SetupMasterSecretFromSessionID(ss);
        if (rv == SECFailure)
            goto loser;

        if (sid->peerCert != NULL) {
            ss->sec.peerCert = CERT_DupCertificate(sid->peerCert);
            ssl3_CopyPeerCertsFromSID(ss, sid);
        }

        rv = ssl3_InitPendingCipherSpec(ss, NULL /* re-use master secret */);
        if (rv != SECSuccess)
            goto loser;

        /* Need to add the server's predicted Finished message to our handshake
         * hash in order to be able to produce our own Finished message. */
        rv = ssl3_ComputeHandshakeHashes(ss, ss->ssl3.pwSpec, &hashes,
                                         0 /* only for SSL3 */);
        if (rv != SECSuccess)
            goto loser;

        rv = ssl3_ComputeTLSFinished(ss->ssl3.pwSpec, PR_TRUE /* isServer */,
                                     &hashes, &tlsFinished);
        if (rv != SECSuccess)
            goto loser;

        hdr[0] = (unsigned char) finished;
        hdr[1] = hdr[2] = 0;
        hdr[3] = sizeof(tlsFinished.verify_data);
        ssl3_UpdateHandshakeHashes(ss, hdr, sizeof(hdr));
        ssl3_UpdateHandshakeHashes(ss, tlsFinished.verify_data,
                                   sizeof(tlsFinished.verify_data));

        /* Store the Finished message so that we can verify it later */
        memcpy(&ss->ssl3.hs.finishedMsgs.tFinished[1], tlsFinished.verify_data,
               sizeof(tlsFinished.verify_data));
    }

    /* Write ChangeCipherSpec */
    rv = ssl3_AppendSnapStartRecordHeader(ss, content_change_cipher_spec, 1);
    if (rv != SECSuccess)
        goto loser;
    rv = ssl3_BufferEnsure(&ss->sec.ci.sendBuf, 1);
    if (rv != SECSuccess)
        goto loser;
    ss->sec.ci.sendBuf.buf[ss->sec.ci.sendBuf.len] = change_cipher_spec_choice;
    ss->sec.ci.sendBuf.len++;

    /* We swap |cwSpec| and |pwSpec| temporarily in order to encrypt some
     * records before switching them back so that the whole ClientHello doesn't
     * get encrypted. */
    ssl_GetSpecWriteLock(ss);
    temp = ss->ssl3.cwSpec;
    ss->ssl3.cwSpec = ss->ssl3.pwSpec;
    ss->ssl3.pwSpec = temp;
    ss->ssl3.cwSpec->write_seq_num.high = 0;
    ss->ssl3.cwSpec->write_seq_num.low  = 0;
    ssl_ReleaseSpecWriteLock(ss);

    rv = ssl3_MaybeWriteNextProtocol(ss, &server_extensions);
    if (rv != SECSuccess)
        goto loser;

    /* Write Finished */
    rv = ssl3_AppendSnapStartHandshakeRecord(ss, ssl3_SendSnapStartFinished,
                                             PR_TRUE /* encrypt */);
    if (rv != SECSuccess)
        goto loser;

    /* Write application data */
    if (ss->ssl3.snapStartApplicationData.data) {
        rv = ssl3_AppendSnapStartApplicationData(
                 ss, ss->ssl3.snapStartApplicationData.data,
                 ss->ssl3.snapStartApplicationData.len);
        SECITEM_FreeItem(&ss->ssl3.snapStartApplicationData, PR_FALSE);
        if (rv != SECSuccess)
            goto loser;
    }

    /* Revert the write cipher spec because the ClientHello will get encrypted
     * with it otherwise. */
    ssl_GetSpecWriteLock(ss);
    temp = ss->ssl3.cwSpec;
    ss->ssl3.cwSpec = ss->ssl3.pwSpec;
    ss->ssl3.pwSpec = temp;
    ssl_ReleaseSpecWriteLock(ss);

    /* Update the lengths in the ClientHello to reflect this extension. */
    ssl3_UpdateClientHelloLengths(ss, snap_start_extension_len_offset, sid);

    /* Keep a copy of the ClientHello around so that we can hash it in the case
     * the the Snap Start handshake is rejected. */

    if (SECITEM_AllocItem(NULL, &ss->ssl3.hs.origClientHello,
                          ss->sec.ci.sendBuf.len) == NULL) {
        goto loser;
    }
    memcpy(ss->ssl3.hs.origClientHello.data, ss->sec.ci.sendBuf.buf,
           ss->sec.ci.sendBuf.len);
    ss->ssl3.hs.origClientHello.len = ss->sec.ci.sendBuf.len;

    ss->xtnData.advertised[ss->xtnData.numAdvertised++] = ssl_snap_start_xtn;

    if (resuming) {
        ss->ssl3.hs.snapStartType = snap_start_resume;
    } else {
        ss->ssl3.hs.snapStartType = snap_start_full;
    }

    return 0;

loser:
    /* In the case of an error we revert the length of the sendBuf to remove
     * any partial data that we may have appended. */
    ss->sec.ci.sendBuf.len = original_sendbuf_len;
    return -1;
}

SECStatus ssl3_ClientHandleSnapStartXtn(sslSocket *ss, PRUint16 ex_type,
                                        SECItem *data) {
    /* The work of saving the ServerHello is done in ssl3_HandleServerHello,
     * where its contents are available. Here we renognise that the saved
     * ServerHello message contains a Snap Start extension and mark it as
     * valid. */
    ss->ssl3.serverHelloPredictionDataValid = PR_TRUE;
    return SECSuccess;
}

SECStatus
SSL_SetPredictedPeerCertificates(PRFileDesc *fd, CERTCertificate **certs,
                                 unsigned int numCerts)
{
    sslSocket *ss;
    unsigned int i;

    ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in SSL_SetPredictedPeerCertificates",
                 SSL_GETPID(), fd));
        return SECFailure;
    }

    ss->ssl3.predictedCertChain =
        PORT_NewArray(CERTCertificate*, numCerts + 1);
    if (!ss->ssl3.predictedCertChain)
        return SECFailure;        /* error code was set */
    for (i = 0; i < numCerts; i++)
        ss->ssl3.predictedCertChain[i] = CERT_DupCertificate(certs[i]);
    ss->ssl3.predictedCertChain[numCerts] = NULL;

    return SECSuccess;
}

void
ssl3_CleanupPredictedPeerCertificates(sslSocket *ss) {
    unsigned int i;

    if (!ss->ssl3.predictedCertChain)
        return;

    for (i = 0; ss->ssl3.predictedCertChain[i]; i++) {
        CERT_DestroyCertificate(ss->ssl3.predictedCertChain[i]);
    }

    PORT_Free(ss->ssl3.predictedCertChain);
    ss->ssl3.predictedCertChain = NULL;
}

SECStatus
SSL_GetPredictedServerHelloData(PRFileDesc *fd, const unsigned char **data,
                                unsigned int *data_len)
{
    sslSocket *ss;

    ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in SSL_GetPredictedServerHelloData",
                 SSL_GETPID(), fd));
        *data = NULL;
        *data_len = 0;
        return SECFailure;
    }

    if (!ss->ssl3.serverHelloPredictionDataValid) {
        *data = NULL;
        *data_len = 0;
    } else {
        *data = ss->ssl3.serverHelloPredictionData.data;
        *data_len = ss->ssl3.serverHelloPredictionData.len;
    }
    return SECSuccess;
}

SECStatus
SSL_SetPredictedServerHelloData(PRFileDesc *fd, const unsigned char *data,
                                unsigned int data_len)
{
    sslSocket *ss;

    ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in SSL_SetPredictedServerHelloData",
                 SSL_GETPID(), fd));
        return SECFailure;
    }

    if (ss->ssl3.serverHelloPredictionData.data)
        SECITEM_FreeItem(&ss->ssl3.serverHelloPredictionData, PR_FALSE);
    if (!SECITEM_AllocItem(NULL, &ss->ssl3.serverHelloPredictionData, data_len))
        return SECFailure;
    memcpy(ss->ssl3.serverHelloPredictionData.data, data, data_len);
    return SECSuccess;
}

SECStatus
SSL_SetSnapStartApplicationData(PRFileDesc *fd, const unsigned char *data,
                                unsigned int data_len)
{
    sslSocket *ss;

    ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in SSL_SetSnapStartApplicationData",
                 SSL_GETPID(), fd));
        return SECFailure;
    }

    if (ss->ssl3.snapStartApplicationData.data)
        SECITEM_FreeItem(&ss->ssl3.snapStartApplicationData, PR_FALSE);
    if (!SECITEM_AllocItem(NULL, &ss->ssl3.snapStartApplicationData, data_len))
        return SECFailure;
    memcpy(ss->ssl3.snapStartApplicationData.data, data, data_len);
    return SECSuccess;
}

SECStatus
SSL_GetSnapStartResult(PRFileDesc *fd, SSLSnapStartResult *result)
{
    sslSocket *ss;

    ss = ssl_FindSocket(fd);
    if (!ss) {
        SSL_DBG(("%d: SSL[%d]: bad socket in SSL_GetSnapStartResult",
                 SSL_GETPID(), fd));
        return SECFailure;
    }

    switch (ss->ssl3.hs.snapStartType) {
        case snap_start_full:
            *result = SSL_SNAP_START_FULL;
            break;
        case snap_start_recovery:
            *result = SSL_SNAP_START_RECOVERY;
            break;
        case snap_start_resume:
            *result = SSL_SNAP_START_RESUME;
            break;
        case snap_start_resume_recovery:
            *result = SSL_SNAP_START_RESUME_RECOVERY;
            break;
        default:
            PORT_Assert(ss->ssl3.hs.snapStartType == snap_start_none);
            *result = SSL_SNAP_START_NONE;
            break;
    }

    return SECSuccess;
}

/* Called form ssl3_HandleServerHello in the case that we sent a Snap Start
** ClientHello but received a ServerHello in reply.
*/
SECStatus
ssl3_ResetForSnapStartRecovery(sslSocket *ss, SSL3Opaque *b, PRUint32 length)
{
    SECStatus rv;
    PRUint8 hdr[4];

    ss->ssl3.hs.ws = wait_server_hello;

    /* Need to reset the Finished hashes to include the full ClientHello
     * message. */

    rv = ssl3_RestartHandshakeHashes(ss);
    if (rv != SECSuccess)
        return rv;
    rv = ssl3_UpdateHandshakeHashes(ss, ss->ssl3.hs.origClientHello.data,
                                    ss->ssl3.hs.origClientHello.len);
    SECITEM_FreeItem(&ss->ssl3.hs.origClientHello, PR_FALSE);
    if (rv != SECSuccess)
        return rv;

    hdr[0] = (PRUint8)server_hello;
    hdr[1] = (PRUint8)(length >> 16);
    hdr[2] = (PRUint8)(length >>  8);
    hdr[3] = (PRUint8)(length      );

    rv = ssl3_UpdateHandshakeHashes(ss, hdr, sizeof(hdr));
    if (rv != SECSuccess)
        return rv;
    rv = ssl3_UpdateHandshakeHashes(ss, b, length);
    if (rv != SECSuccess)
        return rv;

    if (ss->ssl3.hs.snapStartType == snap_start_full) {
        ss->ssl3.hs.snapStartType = snap_start_recovery;
    } else {
        ss->ssl3.hs.snapStartType = snap_start_resume_recovery;
    }

    ss->ssl3.nextProtoState = SSL_NEXT_PROTO_NO_SUPPORT;

    ssl3_DestroyCipherSpec(ss->ssl3.pwSpec, PR_TRUE/*freeSrvName*/);

    return SECSuccess;
}
