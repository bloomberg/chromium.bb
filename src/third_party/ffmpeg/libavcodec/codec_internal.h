/*
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVCODEC_CODEC_INTERNAL_H
#define AVCODEC_CODEC_INTERNAL_H

#include <stdint.h>

#include "libavutil/attributes.h"
#include "codec.h"

/**
 * The codec does not modify any global variables in the init function,
 * allowing to call the init function without locking any global mutexes.
 */
#define FF_CODEC_CAP_INIT_THREADSAFE        (1 << 0)
/**
 * The codec allows calling the close function for deallocation even if
 * the init function returned a failure. Without this capability flag, a
 * codec does such cleanup internally when returning failures from the
 * init function and does not expect the close function to be called at
 * all.
 */
#define FF_CODEC_CAP_INIT_CLEANUP           (1 << 1)
/**
 * Decoders marked with FF_CODEC_CAP_SETS_PKT_DTS want to set
 * AVFrame.pkt_dts manually. If the flag is set, decode.c won't overwrite
 * this field. If it's unset, decode.c tries to guess the pkt_dts field
 * from the input AVPacket.
 */
#define FF_CODEC_CAP_SETS_PKT_DTS           (1 << 2)
/**
 * The decoder extracts and fills its parameters even if the frame is
 * skipped due to the skip_frame setting.
 */
#define FF_CODEC_CAP_SKIP_FRAME_FILL_PARAM  (1 << 3)
/**
 * The decoder sets the cropping fields in the output frames manually.
 * If this cap is set, the generic code will initialize output frame
 * dimensions to coded rather than display values.
 */
#define FF_CODEC_CAP_EXPORTS_CROPPING       (1 << 4)
/**
 * Codec initializes slice-based threading with a main function
 */
#define FF_CODEC_CAP_SLICE_THREAD_HAS_MF    (1 << 5)
/*
 * The codec supports frame threading and has inter-frame dependencies, so it
 * uses ff_thread_report/await_progress().
 */
#define FF_CODEC_CAP_ALLOCATE_PROGRESS      (1 << 6)
/**
 * Codec handles avctx->thread_count == 0 (auto) internally.
 */
#define FF_CODEC_CAP_AUTO_THREADS           (1 << 7)
/**
 * Codec handles output frame properties internally instead of letting the
 * internal logic derive them from AVCodecInternal.last_pkt_props.
 */
#define FF_CODEC_CAP_SETS_FRAME_PROPS       (1 << 8)

/**
 * FFCodec.codec_tags termination value
 */
#define FF_CODEC_TAGS_END -1

typedef struct FFCodecDefault {
    const char *key;
    const char *value;
} FFCodecDefault;

struct AVCodecContext;
struct AVSubtitle;
struct AVPacket;

typedef struct FFCodec {
    /**
     * The public AVCodec. See codec.h for it.
     */
    AVCodec p;

    /**
     * Internal codec capabilities FF_CODEC_CAP_*.
     */
    int caps_internal;

    int priv_data_size;
    /**
     * @name Frame-level threading support functions
     * @{
     */
    /**
     * Copy necessary context variables from a previous thread context to the current one.
     * If not defined, the next thread will start automatically; otherwise, the codec
     * must call ff_thread_finish_setup().
     *
     * dst and src will (rarely) point to the same context, in which case memcpy should be skipped.
     */
    int (*update_thread_context)(struct AVCodecContext *dst, const struct AVCodecContext *src);

    /**
     * Copy variables back to the user-facing context
     */
    int (*update_thread_context_for_user)(struct AVCodecContext *dst, const struct AVCodecContext *src);
    /** @} */

    /**
     * Private codec-specific defaults.
     */
    const FFCodecDefault *defaults;

    /**
     * Initialize codec static data, called from av_codec_iterate().
     *
     * This is not intended for time consuming operations as it is
     * run for every codec regardless of that codec being used.
     */
    void (*init_static_data)(struct FFCodec *codec);

    int (*init)(struct AVCodecContext *);
    int (*encode_sub)(struct AVCodecContext *, uint8_t *buf, int buf_size,
                      const struct AVSubtitle *sub);
    /**
     * Encode data to an AVPacket.
     *
     * @param      avctx          codec context
     * @param      avpkt          output AVPacket
     * @param[in]  frame          AVFrame containing the raw data to be encoded
     * @param[out] got_packet_ptr encoder sets to 0 or 1 to indicate that a
     *                            non-empty packet was returned in avpkt.
     * @return 0 on success, negative error code on failure
     */
    int (*encode2)(struct AVCodecContext *avctx, struct AVPacket *avpkt,
                   const struct AVFrame *frame, int *got_packet_ptr);
    /**
     * Decode picture or subtitle data.
     *
     * @param      avctx          codec context
     * @param      outdata        codec type dependent output struct
     * @param[out] got_frame_ptr  decoder sets to 0 or 1 to indicate that a
     *                            non-empty frame or subtitle was returned in
     *                            outdata.
     * @param[in]  avpkt          AVPacket containing the data to be decoded
     * @return amount of bytes read from the packet on success, negative error
     *         code on failure
     */
    int (*decode)(struct AVCodecContext *avctx, void *outdata,
                  int *got_frame_ptr, struct AVPacket *avpkt);
    int (*close)(struct AVCodecContext *);
    /**
     * Encode API with decoupled frame/packet dataflow. This function is called
     * to get one output packet. It should call ff_encode_get_frame() to obtain
     * input data.
     */
    int (*receive_packet)(struct AVCodecContext *avctx, struct AVPacket *avpkt);

    /**
     * Decode API with decoupled packet/frame dataflow. This function is called
     * to get one output frame. It should call ff_decode_get_packet() to obtain
     * input data.
     */
    int (*receive_frame)(struct AVCodecContext *avctx, struct AVFrame *frame);
    /**
     * Flush buffers.
     * Will be called when seeking
     */
    void (*flush)(struct AVCodecContext *);

    /**
     * Decoding only, a comma-separated list of bitstream filters to apply to
     * packets before decoding.
     */
    const char *bsfs;

    /**
     * Array of pointers to hardware configurations supported by the codec,
     * or NULL if no hardware supported.  The array is terminated by a NULL
     * pointer.
     *
     * The user can only access this field via avcodec_get_hw_config().
     */
    const struct AVCodecHWConfigInternal *const *hw_configs;

    /**
     * List of supported codec_tags, terminated by FF_CODEC_TAGS_END.
     */
    const uint32_t *codec_tags;
} FFCodec;

static av_always_inline const FFCodec *ffcodec(const AVCodec *codec)
{
    return (const FFCodec*)codec;
}

#endif /* AVCODEC_CODEC_INTERNAL_H */
