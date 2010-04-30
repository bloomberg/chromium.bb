/* packet-spdy.c
 * Routines for SPDY packet disassembly
 * For now, the protocol spec can be found at
 * http://dev.chromium.org/spdy/spdy-protocol
 *
 * Copyright 2010, Google Inc.
 * Eric Shienbrood <ers@google.com>
 *
 * $Id$
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * Originally based on packet-http.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <ctype.h>

#include <glib.h>
#include <epan/conversation.h>
#include <epan/packet.h>
#include <epan/strutil.h>
#include <epan/base64.h>
#include <epan/emem.h>
#include <epan/stats_tree.h>

#include <epan/req_resp_hdrs.h>
#include "packet-spdy.h"
#include <epan/dissectors/packet-tcp.h>
#include <epan/dissectors/packet-ssl.h>
#include <epan/prefs.h>
#include <epan/expert.h>
#include <epan/uat.h>

#define SPDY_FIN  0x01

/* The types of SPDY control frames */
typedef enum _spdy_type {
	SPDY_DATA,
	SPDY_SYN_STREAM,
	SPDY_SYN_REPLY,
	SPDY_FIN_STREAM,
	SPDY_HELLO,
	SPDY_NOOP,
	SPDY_PING,
	SPDY_INVALID
} spdy_frame_type_t;

static const char *frame_type_names[] = {
  "DATA", "SYN_STREAM", "SYN_REPLY", "FIN_STREAM", "HELLO", "NOOP",
  "PING", "INVALID"
};

/*
 * This structure will be tied to each SPDY frame.
 * Note that there may be multiple SPDY frames
 * in one packet.
 */
typedef struct _spdy_frame_info_t {
    guint32 stream_id;
    guint8 *header_block;
    guint   header_block_len;
    guint16 frame_type;
} spdy_frame_info_t;

/*
 * This structures keeps track of all the data frames
 * associated with a stream, so that they can be
 * reassembled into a single chunk.
 */
typedef struct _spdy_data_frame_t {
    guint8 *data;
    guint32 length;
    guint32 framenum;
} spdy_data_frame_t;

typedef struct _spdy_stream_info_t {
    gchar *content_type;
    gchar *content_type_parameters;
    gchar *content_encoding;
    GSList *data_frames;
    tvbuff_t *assembled_data;
    guint num_data_frames;
} spdy_stream_info_t;

#include <epan/tap.h>


static int spdy_tap = -1;
static int spdy_eo_tap = -1;

static int proto_spdy = -1;
static int hf_spdy_syn_stream = -1;
static int hf_spdy_syn_reply = -1;
static int hf_spdy_control_bit = -1;
static int hf_spdy_version = -1;
static int hf_spdy_type = -1;
static int hf_spdy_flags = -1;
static int hf_spdy_flags_fin = -1;
static int hf_spdy_length = -1;
static int hf_spdy_header = -1;
static int hf_spdy_header_name = -1;
static int hf_spdy_header_name_text = -1;
static int hf_spdy_header_value = -1;
static int hf_spdy_header_value_text = -1;
static int hf_spdy_streamid = -1;
static int hf_spdy_associated_streamid = -1;
static int hf_spdy_priority = -1;
static int hf_spdy_num_headers = -1;
static int hf_spdy_num_headers_string = -1;

static gint ett_spdy = -1;
static gint ett_spdy_syn_stream = -1;
static gint ett_spdy_syn_reply = -1;
static gint ett_spdy_fin_stream = -1;
static gint ett_spdy_flags = -1;
static gint ett_spdy_header = -1;
static gint ett_spdy_header_name = -1;
static gint ett_spdy_header_value = -1;

static gint ett_spdy_encoded_entity = -1;

static dissector_handle_t data_handle;
static dissector_handle_t media_handle;
static dissector_handle_t spdy_handle;

/* Stuff for generation/handling of fields for custom HTTP headers */
typedef struct _header_field_t {
	gchar* header_name;
	gchar* header_desc;
} header_field_t;

/*
 * desegmentation of SPDY control frames
 * (when we are over TCP or another protocol providing the desegmentation API)
 */
static gboolean spdy_desegment_control_frames = TRUE;

/*
 * desegmentation of SPDY data frames bodies
 * (when we are over TCP or another protocol providing the desegmentation API)
 * TODO let the user filter on content-type the bodies he wants desegmented
 */
static gboolean spdy_desegment_data_frames = TRUE;

static gboolean spdy_assemble_entity_bodies = TRUE;

/*
 * Decompression of zlib encoded entities.
 */
#ifdef HAVE_LIBZ
static gboolean spdy_decompress_body = TRUE;
static gboolean spdy_decompress_headers = TRUE;
#else
static gboolean spdy_decompress_body = FALSE;
static gboolean spdy_decompress_headers = FALSE;
#endif
static gboolean spdy_debug = FALSE;

#define TCP_PORT_DAAP			3689

/*
 * SSDP is implemented atop HTTP (yes, it really *does* run over UDP).
 */
#define TCP_PORT_SSDP			1900
#define UDP_PORT_SSDP			1900

/*
 * tcp and ssl ports
 */

#define TCP_DEFAULT_RANGE "80,8080"
#define SSL_DEFAULT_RANGE "443"

static range_t *global_spdy_tcp_range = NULL;
static range_t *global_spdy_ssl_range = NULL;

static range_t *spdy_tcp_range = NULL;
static range_t *spdy_ssl_range = NULL;

static const value_string vals_status_code[] = {
	{ 100, "Continue" },
	{ 101, "Switching Protocols" },
	{ 102, "Processing" },
	{ 199, "Informational - Others" },

	{ 200, "OK"},
	{ 201, "Created"},
	{ 202, "Accepted"},
	{ 203, "Non-authoritative Information"},
	{ 204, "No Content"},
	{ 205, "Reset Content"},
	{ 206, "Partial Content"},
	{ 207, "Multi-Status"},
	{ 299, "Success - Others"},

	{ 300, "Multiple Choices"},
	{ 301, "Moved Permanently"},
	{ 302, "Found"},
	{ 303, "See Other"},
	{ 304, "Not Modified"},
	{ 305, "Use Proxy"},
	{ 307, "Temporary Redirect"},
	{ 399, "Redirection - Others"},

	{ 400, "Bad Request"},
	{ 401, "Unauthorized"},
	{ 402, "Payment Required"},
	{ 403, "Forbidden"},
	{ 404, "Not Found"},
	{ 405, "Method Not Allowed"},
	{ 406, "Not Acceptable"},
	{ 407, "Proxy Authentication Required"},
	{ 408, "Request Time-out"},
	{ 409, "Conflict"},
	{ 410, "Gone"},
	{ 411, "Length Required"},
	{ 412, "Precondition Failed"},
	{ 413, "Request Entity Too Large"},
	{ 414, "Request-URI Too Long"},
	{ 415, "Unsupported Media Type"},
	{ 416, "Requested Range Not Satisfiable"},
	{ 417, "Expectation Failed"},
	{ 418, "I'm a teapot"},         /* RFC 2324 */
	{ 422, "Unprocessable Entity"},
	{ 423, "Locked"},
	{ 424, "Failed Dependency"},
	{ 499, "Client Error - Others"},

	{ 500, "Internal Server Error"},
	{ 501, "Not Implemented"},
	{ 502, "Bad Gateway"},
	{ 503, "Service Unavailable"},
	{ 504, "Gateway Time-out"},
	{ 505, "HTTP Version not supported"},
	{ 507, "Insufficient Storage"},
	{ 599, "Server Error - Others"},

	{ 0,	NULL}
};

static const char spdy_dictionary[] =
  "optionsgetheadpostputdeletetraceacceptaccept-charsetaccept-encodingaccept-"
  "languageauthorizationexpectfromhostif-modified-sinceif-matchif-none-matchi"
  "f-rangeif-unmodifiedsincemax-forwardsproxy-authorizationrangerefererteuser"
  "-agent10010120020120220320420520630030130230330430530630740040140240340440"
  "5406407408409410411412413414415416417500501502503504505accept-rangesageeta"
  "glocationproxy-authenticatepublicretry-afterservervarywarningwww-authentic"
  "ateallowcontent-basecontent-encodingcache-controlconnectiondatetrailertran"
  "sfer-encodingupgradeviawarningcontent-languagecontent-lengthcontent-locati"
  "oncontent-md5content-rangecontent-typeetagexpireslast-modifiedset-cookieMo"
  "ndayTuesdayWednesdayThursdayFridaySaturdaySundayJanFebMarAprMayJunJulAugSe"
  "pOctNovDecchunkedtext/htmlimage/pngimage/jpgimage/gifapplication/xmlapplic"
  "ation/xhtmltext/plainpublicmax-agecharset=iso-8859-1utf-8gzipdeflateHTTP/1"
  ".1statusversionurl";

static void reset_decompressors(void)
{
    if (spdy_debug) printf("Should reset SPDY decompressors\n");
}

static spdy_conv_t *
get_spdy_conversation_data(packet_info *pinfo)
{
    conversation_t  *conversation;
    spdy_conv_t *conv_data;
    int retcode;

    conversation = find_conversation(pinfo->fd->num, &pinfo->src, &pinfo->dst, pinfo->ptype, pinfo->srcport, pinfo->destport, 0);
    if (spdy_debug) {
        printf("\n===========================================\n\n");
        printf("Conversation for frame #%d is %p\n", pinfo->fd->num, conversation);
        if (conversation)
            printf("  conv_data=%p\n", conversation_get_proto_data(conversation, proto_spdy));
    }

    if(!conversation)  /* Conversation does not exist yet - create it */
	conversation = conversation_new(pinfo->fd->num, &pinfo->src, &pinfo->dst, pinfo->ptype, pinfo->srcport, pinfo->destport, 0);

    /* Retrieve information from conversation
    */
    conv_data = conversation_get_proto_data(conversation, proto_spdy);
    if(!conv_data) {
	/* Setup the conversation structure itself */
	conv_data = se_alloc0(sizeof(spdy_conv_t));

	conv_data->streams = NULL;
	if (spdy_decompress_headers) {
	    conv_data->rqst_decompressor = se_alloc0(sizeof(z_stream));
	    conv_data->rply_decompressor = se_alloc0(sizeof(z_stream));
	    retcode = inflateInit(conv_data->rqst_decompressor);
	    if (retcode == Z_OK)
		retcode = inflateInit(conv_data->rply_decompressor);
	    if (retcode != Z_OK)
		printf("frame #%d: inflateInit() failed: %d\n", pinfo->fd->num, retcode);
	    else if (spdy_debug)
		printf("created decompressor\n");
	    conv_data->dictionary_id = adler32(0L, Z_NULL, 0);
	    conv_data->dictionary_id = adler32(conv_data->dictionary_id,
					       spdy_dictionary,
					       sizeof(spdy_dictionary));
	}

	conversation_add_proto_data(conversation, proto_spdy, conv_data);
	register_postseq_cleanup_routine(reset_decompressors);
    }
    return conv_data;
}

static void
spdy_save_stream_info(spdy_conv_t *conv_data,
		      guint32 stream_id,
		      gchar *content_type,
                      gchar *content_type_params,
		      gchar *content_encoding)
{
    spdy_stream_info_t *si;

    if (conv_data->streams == NULL)
	conv_data->streams = g_array_new(FALSE, TRUE, sizeof(spdy_stream_info_t *));
    if (stream_id < conv_data->streams->len)
	DISSECTOR_ASSERT(g_array_index(conv_data->streams, spdy_stream_info_t*, stream_id) == NULL);
    else
        g_array_set_size(conv_data->streams, stream_id+1);
    si = se_alloc(sizeof(spdy_stream_info_t));
    si->content_type = content_type;
    si->content_type_parameters = content_type_params;
    si->content_encoding = content_encoding;
    si->data_frames = NULL;
    si->num_data_frames = 0;
    si->assembled_data = NULL;
    g_array_index(conv_data->streams, spdy_stream_info_t*, stream_id) = si;
    if (spdy_debug)
        printf("Saved stream info for ID %u, content type %s\n", stream_id, content_type);
}

static spdy_stream_info_t *
spdy_get_stream_info(spdy_conv_t *conv_data, guint32 stream_id)
{
    if (conv_data->streams == NULL || stream_id >= conv_data->streams->len)
	return NULL;
    else
	return g_array_index(conv_data->streams, spdy_stream_info_t*, stream_id);
}

static void
spdy_add_data_chunk(spdy_conv_t *conv_data, guint32 stream_id, guint32 frame,
		    guint8 *data, guint32 length)
{
    spdy_stream_info_t *si = spdy_get_stream_info(conv_data, stream_id);

    if (si == NULL) {
	if (spdy_debug) printf("No stream_info found for stream %d\n", stream_id);
    } else {
	spdy_data_frame_t *df = g_malloc(sizeof(spdy_data_frame_t));
	df->data = data;
	df->length = length;
	df->framenum = frame;
	si->data_frames = g_slist_append(si->data_frames, df);
	++si->num_data_frames;
	if (spdy_debug)
	    printf("Saved %u bytes of data for stream %u frame %u\n",
		    length, stream_id, df->framenum);
    }
}

static void
spdy_increment_data_chunk_count(spdy_conv_t *conv_data, guint32 stream_id)
{
    spdy_stream_info_t *si = spdy_get_stream_info(conv_data, stream_id);
    if (si != NULL)
	++si->num_data_frames;
}

/*
 * Return the number of data frames saved so far for the specified stream.
 */
static guint
spdy_get_num_data_frames(spdy_conv_t *conv_data, guint32 stream_id)
{
    spdy_stream_info_t *si = spdy_get_stream_info(conv_data, stream_id);

    return si == NULL ? 0 : si->num_data_frames;
}

static spdy_stream_info_t *
spdy_assemble_data_frames(spdy_conv_t *conv_data, guint32 stream_id)
{
    spdy_stream_info_t *si = spdy_get_stream_info(conv_data, stream_id);
    tvbuff_t *tvb;

    if (si == NULL)
	return NULL;

    /*
     * Compute the total amount of data and concatenate the
     * data chunks, if it hasn't already been done.
     */
    if (si->assembled_data == NULL) {
	spdy_data_frame_t *df;
	guint8 *data;
	guint32 datalen;
	guint32 offset;
	guint32 framenum;
	GSList *dflist = si->data_frames;
	if (dflist == NULL)
	    return si;
	dflist = si->data_frames;
	datalen = 0;
	/*
	 * I'd like to use a composite tvbuff here, but since
	 * only a real-data tvbuff can be the child of another
	 * tvb, I can't. It would be nice if this limitation
	 * could be fixed.
	 */
	while (dflist != NULL) {
	    df = dflist->data;
	    datalen += df->length;
	    dflist = g_slist_next(dflist);
	}
	if (datalen != 0) {
	    data = se_alloc(datalen);
	    dflist = si->data_frames;
	    offset = 0;
	    framenum = 0;
	    while (dflist != NULL) {
		df = dflist->data;
		memcpy(data+offset, df->data, df->length);
		offset += df->length;
		dflist = g_slist_next(dflist);
	    }
	    tvb = tvb_new_real_data(data, datalen, datalen);
	    si->assembled_data = tvb;
	}
    }
    return si;
}

static void
spdy_discard_data_frames(spdy_stream_info_t *si)
{
    GSList *dflist = si->data_frames;
    spdy_data_frame_t *df;

    if (dflist == NULL)
	return;
    while (dflist != NULL) {
	df = dflist->data;
	if (df->data != NULL) {
	    g_free(df->data);
	    df->data = NULL;
	}
	dflist = g_slist_next(dflist);
    }
    /*g_slist_free(si->data_frames);
    si->data_frames = NULL; */
}

// TODO(cbentzel): tvb_child_uncompress should be exported by wireshark.
static tvbuff_t* spdy_tvb_child_uncompress(tvbuff_t *parent _U_, tvbuff_t *tvb,
                                           int offset, int comprlen)
{
	tvbuff_t *new_tvb = tvb_uncompress(tvb, offset, comprlen);
	if (new_tvb)
		tvb_set_child_real_data_tvbuff (parent, new_tvb);
	return new_tvb;
}

static int
dissect_spdy_data_frame(tvbuff_t *tvb, int offset,
			packet_info *pinfo,
			proto_tree *top_level_tree,
			proto_tree *spdy_tree,
			proto_item *spdy_proto,
			spdy_conv_t *conv_data)
{
    guint32	stream_id;
    guint8	flags;
    guint32	frame_length;
    proto_item	*ti;
    proto_tree	*flags_tree;
    guint32	reported_datalen;
    guint32	datalen;
    dissector_table_t media_type_subdissector_table;
    dissector_table_t port_subdissector_table;
    dissector_handle_t handle;
    guint	num_data_frames;
    gboolean    dissected;

    stream_id = tvb_get_bits32(tvb, (offset << 3) + 1, 31, FALSE);
    flags = tvb_get_guint8(tvb, offset+4);
    frame_length = tvb_get_ntoh24(tvb, offset+5);

    if (spdy_debug)
	printf("Data frame [stream_id=%u flags=0x%x length=%d]\n",
		stream_id, flags, frame_length);
    if (spdy_tree) proto_item_append_text(spdy_tree, ", data frame");
    col_add_fstr(pinfo->cinfo, COL_INFO, "DATA[%u] length=%d",
	    stream_id, frame_length);

    proto_item_append_text(spdy_proto, ":%s stream=%d length=%d",
	    flags & SPDY_FIN ? " [FIN]" : "",
	    stream_id, frame_length);

    proto_tree_add_boolean(spdy_tree, hf_spdy_control_bit, tvb, offset, 1, 0);
    proto_tree_add_uint(spdy_tree, hf_spdy_streamid, tvb, offset, 4, stream_id);
    ti = proto_tree_add_uint_format(spdy_tree, hf_spdy_flags, tvb, offset+4, 1, flags,
	    "Flags: 0x%02x%s", flags, flags&SPDY_FIN ? " (FIN)" : "");

    flags_tree = proto_item_add_subtree(ti, ett_spdy_flags);
    proto_tree_add_boolean(flags_tree, hf_spdy_flags_fin, tvb, offset+4, 1, flags);
    proto_tree_add_uint(spdy_tree, hf_spdy_length, tvb, offset+5, 3, frame_length);

    datalen = tvb_length_remaining(tvb, offset);
    if (datalen > frame_length)
	datalen = frame_length;

    reported_datalen = tvb_reported_length_remaining(tvb, offset);
    if (reported_datalen > frame_length)
	reported_datalen = frame_length;

    num_data_frames = spdy_get_num_data_frames(conv_data, stream_id);
    if (datalen != 0 || num_data_frames != 0) {
	/*
	 * There's stuff left over; process it.
	 */
	tvbuff_t *next_tvb = NULL;
	tvbuff_t    *data_tvb = NULL;
	spdy_stream_info_t *si = NULL;
	void *save_private_data = NULL;
	guint8 *copied_data;
	gboolean private_data_changed = FALSE;
	gboolean is_single_chunk = FALSE;
	gboolean have_entire_body;

	/*
	 * Create a tvbuff for the payload.
	 */
	if (datalen != 0) {
	    next_tvb = tvb_new_subset(tvb, offset+8, datalen,
				      reported_datalen);
            is_single_chunk = num_data_frames == 0 && (flags & SPDY_FIN) != 0;
            if (!pinfo->fd->flags.visited) {
                if (!is_single_chunk) {
                    if (spdy_assemble_entity_bodies) {
                        copied_data = tvb_memdup(next_tvb, 0, datalen);
                        spdy_add_data_chunk(conv_data, stream_id, pinfo->fd->num,
                                copied_data, datalen);
                    } else
                        spdy_increment_data_chunk_count(conv_data, stream_id);
                }
            }
        } else
            is_single_chunk = (num_data_frames == 1);

	if (!(flags & SPDY_FIN)) {
	    col_set_fence(pinfo->cinfo, COL_INFO);
	    col_add_fstr(pinfo->cinfo, COL_INFO, " (partial entity)");
            proto_item_append_text(spdy_proto, " (partial entity body)");
            /* would like the proto item to say */
            /* " (entity body fragment N of M)" */
	    goto body_dissected;
	}
	have_entire_body = is_single_chunk;
	/*
	 * On seeing the last data frame in a stream, we can
	 * reassemble the frames into one data block.
	 */
	si = spdy_assemble_data_frames(conv_data, stream_id);
	if (si == NULL)
	    goto body_dissected;
	data_tvb = si->assembled_data;
	if (spdy_assemble_entity_bodies)
	    have_entire_body = TRUE;

	if (!have_entire_body)
	    goto body_dissected;

	if (data_tvb == NULL)
	    data_tvb = next_tvb;
	else
	    add_new_data_source(pinfo, data_tvb, "Assembled entity body");

	if (have_entire_body && si->content_encoding != NULL &&
	    g_ascii_strcasecmp(si->content_encoding, "identity") != 0) {
	    /*
	     * We currently can't handle, for example, "compress";
	     * just handle them as data for now.
	     *
	     * After July 7, 2004 the LZW patent expires, so support
	     * might be added then.  However, I don't think that
	     * anybody ever really implemented "compress", due to
	     * the aforementioned patent.
	     */
	    tvbuff_t *uncomp_tvb = NULL;
	    proto_item *e_ti = NULL;
	    proto_item *ce_ti = NULL;
	    proto_tree *e_tree = NULL;

	    if (spdy_decompress_body &&
		    (g_ascii_strcasecmp(si->content_encoding, "gzip") == 0 ||
		     g_ascii_strcasecmp(si->content_encoding, "deflate")
		     == 0)) {
              uncomp_tvb = spdy_tvb_child_uncompress(tvb, data_tvb, 0,
                                                     tvb_length(data_tvb));
	    }
	    /*
	     * Add the encoded entity to the protocol tree
	     */
	    e_ti = proto_tree_add_text(top_level_tree, data_tvb,
		    0, tvb_length(data_tvb),
		    "Content-encoded entity body (%s): %u bytes",
		    si->content_encoding,
		    tvb_length(data_tvb));
	    e_tree = proto_item_add_subtree(e_ti, ett_spdy_encoded_entity);
	    if (si->num_data_frames > 1) {
		GSList *dflist;
		spdy_data_frame_t *df;
		guint32 framenum;
		ce_ti = proto_tree_add_text(e_tree, data_tvb, 0,
			tvb_length(data_tvb),
			"Assembled from %d frames in packet(s)", si->num_data_frames);
		dflist = si->data_frames;
		framenum = 0;
		while (dflist != NULL) {
		    df = dflist->data;
		    if (framenum != df->framenum) {
			proto_item_append_text(ce_ti, " #%u", df->framenum);
			framenum = df->framenum;
		    }
		    dflist = g_slist_next(dflist);
		  }
	    }

	    if (uncomp_tvb != NULL) {
		/*
		 * Decompression worked
		 */

		/* XXX - Don't free this, since it's possible
		 * that the data was only partially
		 * decompressed, such as when desegmentation
		 * isn't enabled.
		 *
		 tvb_free(next_tvb);
		 */
		proto_item_append_text(e_ti, " -> %u bytes", tvb_length(uncomp_tvb));
		data_tvb = uncomp_tvb;
		add_new_data_source(pinfo, data_tvb, "Uncompressed entity body");
	    } else {
		if (spdy_decompress_body)
		    proto_item_append_text(e_ti, " [Error: Decompression failed]");
		call_dissector(data_handle, data_tvb, pinfo, e_tree);

		goto body_dissected;
	    }
	}
	if (si != NULL)
	    spdy_discard_data_frames(si);
	/*
	 * Do subdissector checks.
	 *
	 * First, check whether some subdissector asked that they
	 * be called if something was on some particular port.
	 */

	port_subdissector_table = find_dissector_table("http.port");
	media_type_subdissector_table = find_dissector_table("media_type");
	if (have_entire_body && port_subdissector_table != NULL)
	    handle = dissector_get_port_handle(port_subdissector_table,
		    pinfo->match_port);
	else
	    handle = NULL;
	if (handle == NULL && have_entire_body && si->content_type != NULL &&
		media_type_subdissector_table != NULL) {
	    /*
	     * We didn't find any subdissector that
	     * registered for the port, and we have a
	     * Content-Type value.  Is there any subdissector
	     * for that content type?
	     */
	    save_private_data = pinfo->private_data;
	    private_data_changed = TRUE;

	    if (si->content_type_parameters)
		pinfo->private_data = ep_strdup(si->content_type_parameters);
	    else
		pinfo->private_data = NULL;
	    /*
	     * Calling the string handle for the media type
	     * dissector table will set pinfo->match_string
	     * to si->content_type for us.
	     */
	    pinfo->match_string = si->content_type;
	    handle = dissector_get_string_handle(
		    media_type_subdissector_table,
		    si->content_type);
	}
	if (handle != NULL) {
	    /*
	     * We have a subdissector - call it.
	     */
	    dissected = call_dissector(handle, data_tvb, pinfo, top_level_tree);
	} else
	    dissected = FALSE;

	if (dissected) {
	    /*
	     * The subdissector dissected the body.
	     * Fix up the top-level item so that it doesn't
	     * include the stuff for that protocol.
	     */
	    if (ti != NULL)
		proto_item_set_len(ti, offset);
	} else if (have_entire_body && si->content_type != NULL) {
	    /*
	     * Calling the default media handle if there is a content-type that
	     * wasn't handled above.
	     */
	    call_dissector(media_handle, next_tvb, pinfo, top_level_tree);
	} else {
	    /* Call the default data dissector */
	    call_dissector(data_handle, next_tvb, pinfo, top_level_tree);
	}

body_dissected:
	/*
	 * Do *not* attempt at freeing the private data;
	 * it may be in use by subdissectors.
	 */
	if (private_data_changed) /*restore even NULL value*/
	    pinfo->private_data = save_private_data;
	/*
	 * We've processed "datalen" bytes worth of data
	 * (which may be no data at all); advance the
	 * offset past whatever data we've processed.
	 */
    }
    return frame_length + 8;
}

static guint8 *
spdy_decompress_header_block(tvbuff_t *tvb, z_streamp decomp,
			     guint32 dictionary_id, int offset,
			     guint32 length, guint *uncomp_length)
{
    int retcode;
    size_t bufsize = 16384;
    const guint8 *hptr = tvb_get_ptr(tvb, offset, length);
    guint8 *uncomp_block = ep_alloc(bufsize);
    decomp->next_in = (Bytef *)hptr;
    decomp->avail_in = length;
    decomp->next_out = uncomp_block;
    decomp->avail_out = bufsize;
    retcode = inflate(decomp, Z_SYNC_FLUSH);
    if (retcode == Z_NEED_DICT) {
	if (decomp->adler != dictionary_id) {
	    printf("decompressor wants dictionary %#x, but we have %#x\n",
		   (guint)decomp->adler, dictionary_id);
	} else {
	    retcode = inflateSetDictionary(decomp,
					   spdy_dictionary,
					   sizeof(spdy_dictionary));
	    if (retcode == Z_OK)
		retcode = inflate(decomp, Z_SYNC_FLUSH);
	}
    }

    if (retcode != Z_OK) {
	return NULL;
    } else {
	*uncomp_length = bufsize - decomp->avail_out;
        if (spdy_debug)
            printf("Inflation SUCCEEDED. uncompressed size=%d\n", *uncomp_length);
	if (decomp->avail_in != 0)
	    if (spdy_debug)
		printf("  but there were %d input bytes left over\n", decomp->avail_in);
    }
    return se_memdup(uncomp_block, *uncomp_length);
}

/*
 * Try to determine heuristically whether the header block is
 * compressed. For an uncompressed block, the first two bytes
 * gives the number of headers. Each header name and value is
 * a two-byte length followed by ASCII characters.
 */
static gboolean
spdy_check_header_compression(tvbuff_t *tvb,
				       int offset,
				       guint32 frame_length)
{
    guint16 length;
    if (!tvb_bytes_exist(tvb, offset, 6))
	return 1;
    length = tvb_get_ntohs(tvb, offset);
    if (length > frame_length)
	return 1;
    length = tvb_get_ntohs(tvb, offset+2);
    if (length > frame_length)
	return 1;
    if (spdy_debug) printf("Looks like the header block is not compressed\n");
    return 0;
}

// TODO(cbentzel): Change wireshark to export p_remove_proto_data, rather
// than duplicating code here.
typedef struct _spdy_frame_proto_data {
  int proto;
  void *proto_data;
} spdy_frame_proto_data;

static gint spdy_p_compare(gconstpointer a, gconstpointer b)
{
  const spdy_frame_proto_data *ap = (const spdy_frame_proto_data *)a;
  const spdy_frame_proto_data *bp = (const spdy_frame_proto_data *)b;

  if (ap -> proto > bp -> proto)
    return 1;
  else if (ap -> proto == bp -> proto)
    return 0;
  else
    return -1;

}

static void spdy_p_remove_proto_data(frame_data *fd, int proto)
{
  spdy_frame_proto_data temp;
  GSList *item;

  temp.proto = proto;
  temp.proto_data = NULL;

  item = g_slist_find_custom(fd->pfd, (gpointer *)&temp, spdy_p_compare);

  if (item) {
    fd->pfd = g_slist_remove(fd->pfd, item->data);
  }
}

static spdy_frame_info_t *
spdy_save_header_block(frame_data *fd,
	guint32 stream_id,
	guint frame_type,
	guint8 *header,
	guint length)
{
    GSList *filist = p_get_proto_data(fd, proto_spdy);
    spdy_frame_info_t *frame_info = se_alloc(sizeof(spdy_frame_info_t));
    if (filist != NULL)
      spdy_p_remove_proto_data(fd, proto_spdy);
    frame_info->stream_id = stream_id;
    frame_info->header_block = header;
    frame_info->header_block_len = length;
    frame_info->frame_type = frame_type;
    filist = g_slist_append(filist, frame_info);
    p_add_proto_data(fd, proto_spdy, filist);
    return frame_info;
    /* TODO(ers) these need to get deleted when no longer needed */
}

static spdy_frame_info_t *
spdy_find_saved_header_block(frame_data *fd,
			     guint32 stream_id,
			     guint16 frame_type)
{
    GSList *filist = p_get_proto_data(fd, proto_spdy);
    while (filist != NULL) {
	spdy_frame_info_t *fi = filist->data;
	if (fi->stream_id == stream_id && fi->frame_type == frame_type)
	    return fi;
	filist = g_slist_next(filist);
    }
    return NULL;
}

/*
 * Given a content type string that may contain optional parameters,
 * return the parameter string, if any, otherwise return NULL. This
 * also has the side effect of null terminating the content type
 * part of the original string.
 */
static gchar *
spdy_parse_content_type(gchar *content_type)
{
    gchar *cp = content_type;

    while (*cp != '\0' && *cp != ';' && !isspace(*cp)) {
	*cp = tolower(*cp);
	++cp;
    }
    if (*cp == '\0')
	cp = NULL;

    if (cp != NULL) {
	*cp++ = '\0';
	while (*cp == ';' || isspace(*cp))
	    ++cp;
	if (*cp != '\0')
	    return cp;
    }
    return NULL;
}

static int
dissect_spdy_message(tvbuff_t *tvb, int offset, packet_info *pinfo,
		     proto_tree *tree, spdy_conv_t *conv_data)
{
    guint8		control_bit;
    guint16		version;
    guint16		frame_type;
    guint8		flags;
    guint32		frame_length;
    guint32		stream_id;
    guint32             associated_stream_id;
    gint		priority;
    guint16		num_headers;
    guint32		fin_status;
    guint8		*frame_header;
    const char		*proto_tag;
    const char		*frame_type_name;
    proto_tree		*spdy_tree = NULL;
    proto_item		*ti = NULL;
    proto_item		*spdy_proto = NULL;
    int			orig_offset;
    int			hoffset;
    int			hdr_offset = 0;
    spdy_frame_type_t	spdy_type;
    proto_tree		*sub_tree;
    proto_tree		*flags_tree;
    tvbuff_t		*header_tvb = NULL;
    gboolean		headers_compressed;
    gchar		*hdr_verb = NULL;
    gchar		*hdr_url = NULL;
    gchar		*hdr_version = NULL;
    gchar		*content_type = NULL;
    gchar		*content_encoding = NULL;

    /*
     * Minimum size for a SPDY frame is 8 bytes.
     */
    if (tvb_reported_length_remaining(tvb, offset) < 8)
	return -1;

    proto_tag = "SPDY";

    if (check_col(pinfo->cinfo, COL_PROTOCOL))
	col_set_str(pinfo->cinfo, COL_PROTOCOL, proto_tag);

    /*
     * Is this a control frame or a data frame?
     */
    orig_offset = offset;
    control_bit = tvb_get_bits8(tvb, offset << 3, 1);
    if (control_bit) {
	version = tvb_get_bits16(tvb, (offset << 3) + 1, 15, FALSE);
	frame_type = tvb_get_ntohs(tvb, offset+2);
	if (frame_type >= SPDY_INVALID) {
	    return -1;
	}
	frame_header = ep_tvb_memdup(tvb, offset, 16);
    } else {
        version = 1;  /* avoid gcc warning */
	frame_type = SPDY_DATA;
        frame_header = NULL;    /* avoid gcc warning */
    }
    frame_type_name = frame_type_names[frame_type];
    offset += 4;
    flags = tvb_get_guint8(tvb, offset);
    frame_length = tvb_get_ntoh24(tvb, offset+1);
    offset += 4;
    /*
     * Make sure there's as much data as the frame header says there is.
     */
    if ((guint)tvb_reported_length_remaining(tvb, offset) < frame_length) {
	if (spdy_debug)
	    printf("Not enough header data: %d vs. %d\n",
		    frame_length, tvb_reported_length_remaining(tvb, offset));
	return -1;
    }
    if (tree) {
	spdy_proto = proto_tree_add_item(tree, proto_spdy, tvb, orig_offset, frame_length+8, FALSE);
	spdy_tree = proto_item_add_subtree(spdy_proto, ett_spdy);
    }

    if (control_bit) {
	if (spdy_debug)
	    printf("Control frame [version=%d type=%d flags=0x%x length=%d]\n",
		    version, frame_type, flags, frame_length);
	if (tree) proto_item_append_text(spdy_tree, ", control frame");
    } else {
	return dissect_spdy_data_frame(tvb, orig_offset, pinfo, tree,
				spdy_tree, spdy_proto, conv_data);
    }
    num_headers = 0;
    sub_tree = NULL;    /* avoid gcc warning */
    switch (frame_type) {
	case SPDY_SYN_STREAM:
	case SPDY_SYN_REPLY:
	    if (tree) {
		int hf;
		hf = frame_type == SPDY_SYN_STREAM ? hf_spdy_syn_stream : hf_spdy_syn_reply;
		ti = proto_tree_add_bytes(spdy_tree, hf, tvb,
					  orig_offset, 16, frame_header);
		sub_tree = proto_item_add_subtree(ti, ett_spdy_syn_stream);
	    }
	    stream_id = tvb_get_bits32(tvb, (offset << 3) + 1, 31, FALSE);
	    offset += 4;
            if (frame_type == SPDY_SYN_STREAM) {
                associated_stream_id = tvb_get_bits32(tvb, (offset << 3) + 1, 31, FALSE);
                offset += 4;
                priority = tvb_get_bits8(tvb, offset << 3, 2);
                offset += 2;
            } else {
                // The next two bytes have no meaning in SYN_REPLY
                offset += 2;
            }
            if (tree) {
		proto_tree_add_boolean(sub_tree, hf_spdy_control_bit, tvb, orig_offset, 1, control_bit);
		proto_tree_add_uint(sub_tree, hf_spdy_version, tvb, orig_offset, 2, version);
		proto_tree_add_uint(sub_tree, hf_spdy_type, tvb, orig_offset+2, 2, frame_type);
		ti = proto_tree_add_uint_format(sub_tree, hf_spdy_flags, tvb, orig_offset+4, 1, flags,
						"Flags: 0x%02x%s", flags, flags&SPDY_FIN ? " (FIN)" : "");
		flags_tree = proto_item_add_subtree(ti, ett_spdy_flags);
		proto_tree_add_boolean(flags_tree, hf_spdy_flags_fin, tvb, orig_offset+4, 1, flags);
		proto_tree_add_uint(sub_tree, hf_spdy_length, tvb, orig_offset+5, 3, frame_length);
		proto_tree_add_uint(sub_tree, hf_spdy_streamid, tvb, orig_offset+8, 4, stream_id);
                if (frame_type == SPDY_SYN_STREAM) {
                     proto_tree_add_uint(sub_tree, hf_spdy_associated_streamid, tvb, orig_offset+12, 4, associated_stream_id);
                     proto_tree_add_uint(sub_tree, hf_spdy_priority, tvb, orig_offset+16, 1, priority);
                }
		proto_item_append_text(spdy_proto, ": %s%s stream=%d length=%d",
				       frame_type_name,
				       flags & SPDY_FIN ? " [FIN]" : "",
				       stream_id, frame_length);
                if (spdy_debug)
                    printf("  stream ID=%u priority=%d\n", stream_id, priority);
	    }
	    break;

	case SPDY_FIN_STREAM:
	    stream_id = tvb_get_bits32(tvb, (offset << 3) + 1, 31, FALSE);
	    fin_status = tvb_get_ntohl(tvb, offset);
	    // TODO(ers) fill in tree and summary
	    offset += 8;
	    break;

	case SPDY_HELLO:
	    // TODO(ers) fill in tree and summary
            stream_id = 0;      /* avoid gcc warning */
	    break;

	default:
            stream_id = 0;      /* avoid gcc warning */
	    return -1;
	    break;
    }

    /*
     * Process the name-value pairs one at a time, after possibly
     * decompressing the header block.
     */
    if (frame_type == SPDY_SYN_STREAM || frame_type == SPDY_SYN_REPLY) {
	headers_compressed = spdy_check_header_compression(tvb, offset, frame_length);
	if (!spdy_decompress_headers || !headers_compressed) {
	    header_tvb = tvb;
	    hdr_offset = offset;
	} else {
	    spdy_frame_info_t *per_frame_info =
		    spdy_find_saved_header_block(pinfo->fd,
						 stream_id,
						 frame_type == SPDY_SYN_REPLY);
	    if (per_frame_info == NULL) {
		guint uncomp_length;
		z_streamp decomp = frame_type == SPDY_SYN_STREAM ?
			conv_data->rqst_decompressor : conv_data->rply_decompressor;
		guint8 *uncomp_ptr =
			spdy_decompress_header_block(tvb, decomp,
						     conv_data->dictionary_id,
						     offset,
                                                     frame_length + 8 - (offset - orig_offset),
                                                     &uncomp_length);
		if (uncomp_ptr == NULL) {         /* decompression failed */
                    if (spdy_debug)
                        printf("Frame #%d: Inflation failed\n", pinfo->fd->num);
		    proto_item_append_text(spdy_proto, " [Error: Header decompression failed]");
		    // Should we just bail here?
                } else {
                    if (spdy_debug)
                        printf("Saving %u bytes of uncomp hdr\n", uncomp_length);
                    per_frame_info =
                        spdy_save_header_block(pinfo->fd, stream_id, frame_type == SPDY_SYN_REPLY,
                                uncomp_ptr, uncomp_length);
                }
	    } else if (spdy_debug) {
		printf("Found uncompressed header block len %u for stream %u frame_type=%d\n",
		       per_frame_info->header_block_len,
		       per_frame_info->stream_id,
		       per_frame_info->frame_type);
	    }
            if (per_frame_info != NULL) {
                header_tvb = tvb_new_child_real_data(tvb,
						 per_frame_info->header_block,
						 per_frame_info->header_block_len,
						 per_frame_info->header_block_len);
                add_new_data_source(pinfo, header_tvb, "Uncompressed headers");
                hdr_offset = 0;
            }
	}
        offset = orig_offset + 8 + frame_length;
	num_headers = tvb_get_ntohs(header_tvb, hdr_offset);
	hdr_offset += 2;
	if (header_tvb == NULL ||
                (headers_compressed && !spdy_decompress_headers)) {
	    num_headers = 0;
	    ti = proto_tree_add_string(sub_tree, hf_spdy_num_headers_string,
				  tvb, 
				  frame_type == SPDY_SYN_STREAM ? orig_offset+18 : orig_offset + 14, 
				  2,
				  "Unknown (header block is compressed)");
	} else
	    ti = proto_tree_add_uint(sub_tree, hf_spdy_num_headers,
				tvb, 
				frame_type == SPDY_SYN_STREAM ? orig_offset+18 : orig_offset +14, 
				2, num_headers);
    }
    spdy_type = SPDY_INVALID;		/* type not known yet */
    if (spdy_debug)
        printf("  %d Headers:\n", num_headers);
    if (num_headers > frame_length) {
	printf("Number of headers is greater than frame length!\n");
        proto_item_append_text(ti, " [Error: Number of headers is larger than frame length]");
	col_add_fstr(pinfo->cinfo, COL_INFO, "%s[%d]", frame_type_name, stream_id);
	return frame_length+8;
    }
    hdr_verb = hdr_url = hdr_version = content_type = content_encoding = NULL;
    while (num_headers-- && tvb_reported_length_remaining(header_tvb, hdr_offset) != 0) {
	gchar *header_name;
	gchar *header_value;
	proto_tree *header_tree;
	proto_tree *name_tree;
	proto_tree *value_tree;
	proto_item *header;
	gint16 length;
	gint header_length = 0;

	hoffset = hdr_offset;

	header = proto_tree_add_item(spdy_tree, hf_spdy_header, header_tvb,
				 hdr_offset, frame_length, FALSE);
	header_tree = proto_item_add_subtree(header, ett_spdy_header);

	length = tvb_get_ntohs(header_tvb, hdr_offset);
	hdr_offset += 2;
	header_name = (gchar *)tvb_get_ephemeral_string(header_tvb, hdr_offset, length);
	hdr_offset += length;
	header_length += hdr_offset - hoffset;
	if (tree) {
	    ti = proto_tree_add_text(header_tree, header_tvb, hoffset, length+2, "Name: %s", 
				     header_name);
	    name_tree = proto_item_add_subtree(ti, ett_spdy_header_name);
	    proto_tree_add_uint(name_tree, hf_spdy_length, header_tvb, hoffset, 2, length);
	    proto_tree_add_string_format(name_tree, hf_spdy_header_name_text, header_tvb, hoffset+2, length,
					 header_name, "Text: %s", format_text(header_name, length));
	}

	hoffset = hdr_offset;
	length = tvb_get_ntohs(header_tvb, hdr_offset);
	hdr_offset += 2;
	header_value = (gchar *)tvb_get_ephemeral_string(header_tvb, hdr_offset, length);
	hdr_offset += length;
	header_length += hdr_offset - hoffset;
	if (tree) {
	    ti = proto_tree_add_text(header_tree, header_tvb, hoffset, length+2, "Value: %s", 
				     header_value);
	    value_tree = proto_item_add_subtree(ti, ett_spdy_header_value);
	    proto_tree_add_uint(value_tree, hf_spdy_length, header_tvb, hoffset, 2, length);
	    proto_tree_add_string_format(value_tree, hf_spdy_header_value_text, header_tvb, hoffset+2, length,
					 header_value, "Text: %s", format_text(header_value, length));
	    proto_item_append_text(header, ": %s: %s", header_name, header_value);
	    proto_item_set_len(header, header_length);
	}
	if (spdy_debug) printf("    %s: %s\n", header_name, header_value);
	/*
	 * TODO(ers) check that the header name contains only legal characters.
	 */
	if (g_ascii_strcasecmp(header_name, "method") == 0 ||
	    g_ascii_strcasecmp(header_name, "status") == 0) {
	    hdr_verb = header_value;
	} else if (g_ascii_strcasecmp(header_name, "url") == 0) {
	    hdr_url = header_value;
	} else if (g_ascii_strcasecmp(header_name, "version") == 0) {
	    hdr_version = header_value;
	} else if (g_ascii_strcasecmp(header_name, "content-type") == 0) {
	    content_type = se_strdup(header_value);
	} else if (g_ascii_strcasecmp(header_name, "content-encoding") == 0) {
	    content_encoding = se_strdup(header_value);
	}
    }
    if (hdr_version != NULL) {
	if (hdr_url != NULL) {
	    col_add_fstr(pinfo->cinfo, COL_INFO, "%s[%d]: %s %s %s",
			 frame_type_name, stream_id, hdr_verb, hdr_url, hdr_version);
	} else {
	    col_add_fstr(pinfo->cinfo, COL_INFO, "%s[%d]: %s %s",
			 frame_type_name, stream_id, hdr_verb, hdr_version);
	}
    } else {
	col_add_fstr(pinfo->cinfo, COL_INFO, "%s[%d]", frame_type_name, stream_id);
    }
    /*
     * If we expect data on this stream, we need to remember the content
     * type and content encoding.
     */
    if (content_type != NULL && !pinfo->fd->flags.visited) {
        gchar *content_type_params = spdy_parse_content_type(content_type);
	spdy_save_stream_info(conv_data, stream_id, content_type,
                              content_type_params, content_encoding);
    }

    return offset - orig_offset;
}

static int
dissect_spdy(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
    spdy_conv_t	*conv_data;
    int		offset = 0;
    int		len;
    int		firstpkt = 1;

    /*
     * The first byte of a SPDY packet must be either 0 or
     * 0x80. If it's not, assume that this is not SPDY.
     * (In theory, a data frame could have a stream ID
     * >= 2^24, in which case it won't have 0 for a first
     * byte, but this is a pretty reliable heuristic for
     * now.)
     */
    guint8 first_byte = tvb_get_guint8(tvb, 0);
    if (first_byte != 0x80 && first_byte != 0x0)
	  return 0;

    conv_data = get_spdy_conversation_data(pinfo);

    while (tvb_reported_length_remaining(tvb, offset) != 0) {
	if (!firstpkt) {
	    col_add_fstr(pinfo->cinfo, COL_INFO, " >> ");
	    col_set_fence(pinfo->cinfo, COL_INFO);
	}
	len = dissect_spdy_message(tvb, offset, pinfo, tree, conv_data);
	if (len <= 0)
	    return 0;
	offset += len;
	/*
	 * OK, we've set the Protocol and Info columns for the
	 * first SPDY message; set a fence so that subsequent
	 * SPDY messages don't overwrite the Info column.
	 */
	col_set_fence(pinfo->cinfo, COL_INFO);
	firstpkt = 0;
    }
    return 1;
}

static gboolean
dissect_spdy_heur(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
    if (!value_is_in_range(global_spdy_tcp_range, pinfo->destport) &&
            !value_is_in_range(global_spdy_tcp_range, pinfo->srcport))
        return FALSE;
    return dissect_spdy(tvb, pinfo, tree) != 0;
}

static void reinit_spdy(void)
{
}

// NMAKE complains about flags_set_truth not being constant. Duplicate
// the values inside of it.
static const true_false_string tfs_spdy_set_notset = { "Set", "Not set" };

void
proto_register_spdy(void)
{
    static hf_register_info hf[] = {
	{ &hf_spdy_syn_stream,
	    { "Syn Stream",	"spdy.syn_stream",
		FT_BYTES, BASE_NONE, NULL, 0x0,
		"", HFILL }},
	{ &hf_spdy_syn_reply,
	    { "Syn Reply",	"spdy.syn_reply",
		FT_BYTES, BASE_NONE, NULL, 0x0,
		"", HFILL }},
	{ &hf_spdy_control_bit,
	    { "Control bit",	"spdy.control_bit",
		FT_BOOLEAN, BASE_NONE, NULL, 0x0,
		"TRUE if SPDY control frame", HFILL }},
	{ &hf_spdy_version,
	    { "Version",	"spdy.version",
		FT_UINT16, BASE_DEC, NULL, 0x0,
		"", HFILL }},
	{ &hf_spdy_type,
	    { "Type",		"spdy.type",
		FT_UINT16, BASE_DEC, NULL, 0x0,
		"", HFILL }},
	{ &hf_spdy_flags,
	    { "Flags",		"spdy.flags",
		FT_UINT8, BASE_HEX, NULL, 0x0,
		"", HFILL }},
	{ &hf_spdy_flags_fin,
	    { "Fin",		"spdy.flags.fin",
                FT_BOOLEAN, 8, TFS(&tfs_spdy_set_notset),
                SPDY_FIN, "", HFILL }},
	{ &hf_spdy_length,
	    { "Length",		"spdy.length",
		FT_UINT24, BASE_DEC, NULL, 0x0,
		"", HFILL }},
	{ &hf_spdy_header,
	    { "Header",		"spdy.header",
		FT_NONE, BASE_NONE, NULL, 0x0,
		"", HFILL }},
	{ &hf_spdy_header_name,
	    { "Name",		"spdy.header.name",
		FT_NONE, BASE_NONE, NULL, 0x0,
		"", HFILL }},
	{ &hf_spdy_header_name_text,
	    { "Text",		"spdy.header.name.text",
		FT_STRING, BASE_NONE, NULL, 0x0,
		"", HFILL }},
	{ &hf_spdy_header_value,
	    { "Value",		"spdy.header.value",
		FT_NONE, BASE_NONE, NULL, 0x0,
		"", HFILL }},
	{ &hf_spdy_header_value_text,
	    { "Text",		"spdy.header.value.text",
		FT_STRING, BASE_NONE, NULL, 0x0,
		"", HFILL }},
	{ &hf_spdy_streamid,
	    { "Stream ID",	"spdy.streamid",
		FT_UINT32, BASE_DEC, NULL, 0x0,
		"", HFILL }},
        { &hf_spdy_associated_streamid,
	    { "Associated Stream ID",	"spdy.associated.streamid",
		FT_UINT32, BASE_DEC, NULL, 0x0,
		"", HFILL }},
	{ &hf_spdy_priority,
	    { "Priority",	"spdy.priority",
		FT_UINT8, BASE_DEC, NULL, 0x0,
		"", HFILL }},
	{ &hf_spdy_num_headers,
	    { "Number of headers", "spdy.numheaders",
		FT_UINT16, BASE_DEC, NULL, 0x0,
		"", HFILL }},
	{ &hf_spdy_num_headers_string,
	    { "Number of headers", "spdy.numheaders",
		FT_STRING, BASE_NONE, NULL, 0x0,
		"", HFILL }},
    };
    static gint *ett[] = {
	&ett_spdy,
	&ett_spdy_syn_stream,
	&ett_spdy_syn_reply,
	&ett_spdy_fin_stream,
	&ett_spdy_flags,
	&ett_spdy_header,
	&ett_spdy_header_name,
	&ett_spdy_header_value,
	&ett_spdy_encoded_entity,
    };

    module_t *spdy_module;

    proto_spdy = proto_register_protocol("SPDY", "SPDY", "spdy");
    proto_register_field_array(proto_spdy, hf, array_length(hf));
    proto_register_subtree_array(ett, array_length(ett));
    new_register_dissector("spdy", dissect_spdy, proto_spdy);
    spdy_module = prefs_register_protocol(proto_spdy, reinit_spdy);
    prefs_register_bool_preference(spdy_module, "desegment_headers",
				   "Reassemble SPDY control frames spanning multiple TCP segments",
				   "Whether the SPDY dissector should reassemble control frames "
				   "spanning multiple TCP segments. "
				   "To use this option, you must also enable "
				   "\"Allow subdissectors to reassemble TCP streams\" in the TCP protocol settings.",
				   &spdy_desegment_control_frames);
    prefs_register_bool_preference(spdy_module, "desegment_body",
				   "Reassemble SPDY bodies spanning multiple TCP segments",
				   "Whether the SPDY dissector should reassemble "
				   "data frames spanning multiple TCP segments. "
				   "To use this option, you must also enable "
				   "\"Allow subdissectors to reassemble TCP streams\" in the TCP protocol settings.",
				   &spdy_desegment_data_frames);
    prefs_register_bool_preference(spdy_module, "assemble_data_frames",
				   "Assemble SPDY bodies that consist of multiple DATA frames",
				   "Whether the SPDY dissector should reassemble multiple "
				   "data frames into an entity body.",
				   &spdy_assemble_entity_bodies);
#ifdef HAVE_LIBZ
    prefs_register_bool_preference(spdy_module, "decompress_headers",
				   "Uncompress SPDY headers",
				   "Whether to uncompress SPDY headers.",
				   &spdy_decompress_headers);
    prefs_register_bool_preference(spdy_module, "decompress_body",
				   "Uncompress entity bodies",
				   "Whether to uncompress entity bodies that are compressed "
				   "using \"Content-Encoding: \"",
				   &spdy_decompress_body);
#endif
    prefs_register_bool_preference(spdy_module, "debug_output",
				   "Print debug info on stdout",
				   "Print debug info on stdout",
				   &spdy_debug);
#if 0
    prefs_register_string_preference(ssl_module, "debug_file", "SPDY debug file",
				     "Redirect SPDY debug to file name; "
				     "leave empty to disable debugging, "
				     "or use \"" SPDY_DEBUG_USE_STDOUT "\""
				     " to redirect output to stdout\n",
				     (const gchar **)&sdpy_debug_file_name);
#endif
    prefs_register_obsolete_preference(spdy_module, "tcp_alternate_port");

    range_convert_str(&global_spdy_tcp_range, TCP_DEFAULT_RANGE, 65535);
    spdy_tcp_range = range_empty();
    prefs_register_range_preference(spdy_module, "tcp.port", "TCP Ports",
				    "TCP Ports range",
				    &global_spdy_tcp_range, 65535);

    range_convert_str(&global_spdy_ssl_range, SSL_DEFAULT_RANGE, 65535);
    spdy_ssl_range = range_empty();
    prefs_register_range_preference(spdy_module, "ssl.port", "SSL/TLS Ports",
				    "SSL/TLS Ports range",
				    &global_spdy_ssl_range, 65535);

    spdy_handle = new_create_dissector_handle(dissect_spdy, proto_spdy);
    /*
     * Register for tapping
     */
    spdy_tap = register_tap("spdy"); /* SPDY statistics tap */
    spdy_eo_tap = register_tap("spdy_eo"); /* SPDY Export Object tap */
}

void
proto_reg_handoff_spdy(void)
{
    data_handle = find_dissector("data");
    media_handle = find_dissector("media");
    heur_dissector_add("tcp", dissect_spdy_heur, proto_spdy);
}

/*
 * Content-Type: message/http
 */

static gint proto_message_spdy = -1;
static gint ett_message_spdy = -1;

static void
dissect_message_spdy(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
	proto_tree	*subtree;
	proto_item	*ti;
	gint		offset = 0, next_offset;
	gint		len;

	if (check_col(pinfo->cinfo, COL_INFO))
		col_append_str(pinfo->cinfo, COL_INFO, " (message/spdy)");
	if (tree) {
		ti = proto_tree_add_item(tree, proto_message_spdy,
				tvb, 0, -1, FALSE);
		subtree = proto_item_add_subtree(ti, ett_message_spdy);
		while (tvb_reported_length_remaining(tvb, offset) != 0) {
			len = tvb_find_line_end(tvb, offset,
					tvb_ensure_length_remaining(tvb, offset),
					&next_offset, FALSE);
			if (len == -1)
				break;
			proto_tree_add_text(subtree, tvb, offset, next_offset - offset,
					"%s", tvb_format_text(tvb, offset, len));
			offset = next_offset;
		}
	}
}

void
proto_register_message_spdy(void)
{
	static gint *ett[] = {
		&ett_message_spdy,
	};

	proto_message_spdy = proto_register_protocol(
			"Media Type: message/spdy",
			"message/spdy",
			"message-spdy"
	);
	proto_register_subtree_array(ett, array_length(ett));
}

void
proto_reg_handoff_message_spdy(void)
{
	dissector_handle_t message_spdy_handle;

	message_spdy_handle = create_dissector_handle(dissect_message_spdy,
			proto_message_spdy);

	dissector_add_string("media_type", "message/spdy", message_spdy_handle);

	reinit_spdy();
}
