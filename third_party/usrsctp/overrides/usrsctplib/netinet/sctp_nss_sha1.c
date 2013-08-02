/*-
 * Copyright (c) 2001-2007, by Cisco Systems, Inc. All rights reserved.
 * Copyright (c) 2008-2012, by Randall Stewart. All rights reserved.
 * Copyright (c) 2008-2012, by Michael Tuexen. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * a) Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * b) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the distribution.
 *
 * c) Neither the name of Cisco Systems, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef __FreeBSD__
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#endif

#ifdef SCTP_USE_NSS_SHA1
#include <netinet/sctp_nss_sha1.h>

/* A SHA-1 Digest is 160 bits, or 20 bytes */
#define SHA_DIGEST_LENGTH (20)

void
SCTP_NSS_SHA1_Init(struct sha1_context *ctx)
{
	ctx->pk11_ctx = PK11_CreateDigestContext(SEC_OID_SHA1);
	PK11_DigestBegin(ctx->pk11_ctx);
}

void
SCTP_NSS_SHA1_Update(struct sha1_context *ctx, const unsigned char *ptr, int siz)
{
	PK11_DigestOp(ctx->pk11_ctx, ptr, siz);
}

void
SCTP_NSS_SHA1_Final(unsigned char *digest, struct sha1_context *ctx)
{
	unsigned int output_len = 0;
	PK11_DigestFinal(ctx->pk11_ctx, digest, &output_len, SHA_DIGEST_LENGTH);
	PK11_DestroyContext(ctx->pk11_ctx, PR_TRUE);
}
#endif
