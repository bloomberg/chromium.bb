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

/**
 * @file
 * implementing an object detecting filter using deep learning networks.
 */

#include "libavformat/avio.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/avassert.h"
#include "libavutil/imgutils.h"
#include "filters.h"
#include "dnn_filter_common.h"
#include "formats.h"
#include "internal.h"
#include "libavutil/time.h"
#include "libavutil/avstring.h"
#include "libavutil/detection_bbox.h"

typedef struct DnnDetectContext {
    const AVClass *class;
    DnnContext dnnctx;
    float confidence;
    char *labels_filename;
    char **labels;
    int label_count;
} DnnDetectContext;

#define OFFSET(x) offsetof(DnnDetectContext, dnnctx.x)
#define OFFSET2(x) offsetof(DnnDetectContext, x)
#define FLAGS AV_OPT_FLAG_FILTERING_PARAM | AV_OPT_FLAG_VIDEO_PARAM
static const AVOption dnn_detect_options[] = {
    { "dnn_backend", "DNN backend",                OFFSET(backend_type),     AV_OPT_TYPE_INT,       { .i64 = 2 },    INT_MIN, INT_MAX, FLAGS, "backend" },
#if (CONFIG_LIBOPENVINO == 1)
    { "openvino",    "openvino backend flag",      0,                        AV_OPT_TYPE_CONST,     { .i64 = 2 },    0, 0, FLAGS, "backend" },
#endif
    DNN_COMMON_OPTIONS
    { "confidence",  "threshold of confidence",    OFFSET2(confidence),      AV_OPT_TYPE_FLOAT,     { .dbl = 0.5 },  0, 1, FLAGS},
    { "labels",      "path to labels file",        OFFSET2(labels_filename), AV_OPT_TYPE_STRING,    { .str = NULL }, 0, 0, FLAGS },
    { NULL }
};

AVFILTER_DEFINE_CLASS(dnn_detect);

static int dnn_detect_post_proc(AVFrame *frame, DNNData *output, uint32_t nb, AVFilterContext *filter_ctx)
{
    DnnDetectContext *ctx = filter_ctx->priv;
    float conf_threshold = ctx->confidence;
    int proposal_count = output->height;
    int detect_size = output->width;
    float *detections = output->data;
    int nb_bboxes = 0;
    AVFrameSideData *sd;
    AVDetectionBBox *bbox;
    AVDetectionBBoxHeader *header;

    sd = av_frame_get_side_data(frame, AV_FRAME_DATA_DETECTION_BBOXES);
    if (sd) {
        av_log(filter_ctx, AV_LOG_ERROR, "already have bounding boxes in side data.\n");
        return -1;
    }

    for (int i = 0; i < proposal_count; ++i) {
        float conf = detections[i * detect_size + 2];
        if (conf < conf_threshold) {
            continue;
        }
        nb_bboxes++;
    }

    if (nb_bboxes == 0) {
        av_log(filter_ctx, AV_LOG_VERBOSE, "nothing detected in this frame.\n");
        return 0;
    }

    header = av_detection_bbox_create_side_data(frame, nb_bboxes);
    if (!header) {
        av_log(filter_ctx, AV_LOG_ERROR, "failed to create side data with %d bounding boxes\n", nb_bboxes);
        return -1;
    }

    av_strlcpy(header->source, ctx->dnnctx.model_filename, sizeof(header->source));

    for (int i = 0; i < proposal_count; ++i) {
        int av_unused image_id = (int)detections[i * detect_size + 0];
        int label_id = (int)detections[i * detect_size + 1];
        float conf   =      detections[i * detect_size + 2];
        float x0     =      detections[i * detect_size + 3];
        float y0     =      detections[i * detect_size + 4];
        float x1     =      detections[i * detect_size + 5];
        float y1     =      detections[i * detect_size + 6];

        bbox = av_get_detection_bbox(header, i);

        if (conf < conf_threshold) {
            continue;
        }

        bbox->x = (int)(x0 * frame->width);
        bbox->w = (int)(x1 * frame->width) - bbox->x;
        bbox->y = (int)(y0 * frame->height);
        bbox->h = (int)(y1 * frame->height) - bbox->y;

        bbox->detect_confidence = av_make_q((int)(conf * 10000), 10000);
        bbox->classify_count = 0;

        if (ctx->labels && label_id < ctx->label_count) {
            av_strlcpy(bbox->detect_label, ctx->labels[label_id], sizeof(bbox->detect_label));
        } else {
            snprintf(bbox->detect_label, sizeof(bbox->detect_label), "%d", label_id);
        }

        nb_bboxes--;
        if (nb_bboxes == 0) {
            break;
        }
    }

    return 0;
}

static void free_detect_labels(DnnDetectContext *ctx)
{
    for (int i = 0; i < ctx->label_count; i++) {
        av_freep(&ctx->labels[i]);
    }
    ctx->label_count = 0;
    av_freep(&ctx->labels);
}

static int read_detect_label_file(AVFilterContext *context)
{
    int line_len;
    FILE *file;
    DnnDetectContext *ctx = context->priv;

    file = av_fopen_utf8(ctx->labels_filename, "r");
    if (!file){
        av_log(context, AV_LOG_ERROR, "failed to open file %s\n", ctx->labels_filename);
        return AVERROR(EINVAL);
    }

    while (!feof(file)) {
        char *label;
        char buf[256];
        if (!fgets(buf, 256, file)) {
            break;
        }

        line_len = strlen(buf);
        while (line_len) {
            int i = line_len - 1;
            if (buf[i] == '\n' || buf[i] == '\r' || buf[i] == ' ') {
                buf[i] = '\0';
                line_len--;
            } else {
                break;
            }
        }

        if (line_len == 0)  // empty line
            continue;

        if (line_len >= AV_DETECTION_BBOX_LABEL_NAME_MAX_SIZE) {
            av_log(context, AV_LOG_ERROR, "label %s too long\n", buf);
            fclose(file);
            return AVERROR(EINVAL);
        }

        label = av_strdup(buf);
        if (!label) {
            av_log(context, AV_LOG_ERROR, "failed to allocate memory for label %s\n", buf);
            fclose(file);
            return AVERROR(ENOMEM);
        }

        if (av_dynarray_add_nofree(&ctx->labels, &ctx->label_count, label) < 0) {
            av_log(context, AV_LOG_ERROR, "failed to do av_dynarray_add\n");
            fclose(file);
            av_freep(&label);
            return AVERROR(ENOMEM);
        }
    }

    fclose(file);
    return 0;
}

static av_cold int dnn_detect_init(AVFilterContext *context)
{
    DnnDetectContext *ctx = context->priv;
    int ret = ff_dnn_init(&ctx->dnnctx, DFT_ANALYTICS_DETECT, context);
    if (ret < 0)
        return ret;
    ff_dnn_set_detect_post_proc(&ctx->dnnctx, dnn_detect_post_proc);

    if (ctx->labels_filename) {
        return read_detect_label_file(context);
    }
    return 0;
}

static int dnn_detect_query_formats(AVFilterContext *context)
{
    static const enum AVPixelFormat pix_fmts[] = {
        AV_PIX_FMT_RGB24, AV_PIX_FMT_BGR24,
        AV_PIX_FMT_GRAY8, AV_PIX_FMT_GRAYF32,
        AV_PIX_FMT_YUV420P, AV_PIX_FMT_YUV422P,
        AV_PIX_FMT_YUV444P, AV_PIX_FMT_YUV410P, AV_PIX_FMT_YUV411P,
        AV_PIX_FMT_NV12,
        AV_PIX_FMT_NONE
    };
    AVFilterFormats *fmts_list = ff_make_format_list(pix_fmts);
    return ff_set_common_formats(context, fmts_list);
}

static int dnn_detect_filter_frame(AVFilterLink *inlink, AVFrame *in)
{
    AVFilterContext *context  = inlink->dst;
    AVFilterLink *outlink = context->outputs[0];
    DnnDetectContext *ctx = context->priv;
    DNNReturnType dnn_result;

    dnn_result = ff_dnn_execute_model(&ctx->dnnctx, in, in);
    if (dnn_result != DNN_SUCCESS){
        av_log(ctx, AV_LOG_ERROR, "failed to execute model\n");
        av_frame_free(&in);
        return AVERROR(EIO);
    }

    return ff_filter_frame(outlink, in);
}

static int dnn_detect_activate_sync(AVFilterContext *filter_ctx)
{
    AVFilterLink *inlink = filter_ctx->inputs[0];
    AVFilterLink *outlink = filter_ctx->outputs[0];
    AVFrame *in = NULL;
    int64_t pts;
    int ret, status;
    int got_frame = 0;

    FF_FILTER_FORWARD_STATUS_BACK(outlink, inlink);

    do {
        // drain all input frames
        ret = ff_inlink_consume_frame(inlink, &in);
        if (ret < 0)
            return ret;
        if (ret > 0) {
            ret = dnn_detect_filter_frame(inlink, in);
            if (ret < 0)
                return ret;
            got_frame = 1;
        }
    } while (ret > 0);

    // if frame got, schedule to next filter
    if (got_frame)
        return 0;

    if (ff_inlink_acknowledge_status(inlink, &status, &pts)) {
        if (status == AVERROR_EOF) {
            ff_outlink_set_status(outlink, status, pts);
            return ret;
        }
    }

    FF_FILTER_FORWARD_WANTED(outlink, inlink);

    return FFERROR_NOT_READY;
}

static int dnn_detect_flush_frame(AVFilterLink *outlink, int64_t pts, int64_t *out_pts)
{
    DnnDetectContext *ctx = outlink->src->priv;
    int ret;
    DNNAsyncStatusType async_state;

    ret = ff_dnn_flush(&ctx->dnnctx);
    if (ret != DNN_SUCCESS) {
        return -1;
    }

    do {
        AVFrame *in_frame = NULL;
        AVFrame *out_frame = NULL;
        async_state = ff_dnn_get_async_result(&ctx->dnnctx, &in_frame, &out_frame);
        if (out_frame) {
            av_assert0(in_frame == out_frame);
            ret = ff_filter_frame(outlink, out_frame);
            if (ret < 0)
                return ret;
            if (out_pts)
                *out_pts = out_frame->pts + pts;
        }
        av_usleep(5000);
    } while (async_state >= DAST_NOT_READY);

    return 0;
}

static int dnn_detect_activate_async(AVFilterContext *filter_ctx)
{
    AVFilterLink *inlink = filter_ctx->inputs[0];
    AVFilterLink *outlink = filter_ctx->outputs[0];
    DnnDetectContext *ctx = filter_ctx->priv;
    AVFrame *in = NULL;
    int64_t pts;
    int ret, status;
    int got_frame = 0;
    int async_state;

    FF_FILTER_FORWARD_STATUS_BACK(outlink, inlink);

    do {
        // drain all input frames
        ret = ff_inlink_consume_frame(inlink, &in);
        if (ret < 0)
            return ret;
        if (ret > 0) {
            if (ff_dnn_execute_model_async(&ctx->dnnctx, in, in) != DNN_SUCCESS) {
                return AVERROR(EIO);
            }
        }
    } while (ret > 0);

    // drain all processed frames
    do {
        AVFrame *in_frame = NULL;
        AVFrame *out_frame = NULL;
        async_state = ff_dnn_get_async_result(&ctx->dnnctx, &in_frame, &out_frame);
        if (out_frame) {
            av_assert0(in_frame == out_frame);
            ret = ff_filter_frame(outlink, out_frame);
            if (ret < 0)
                return ret;
            got_frame = 1;
        }
    } while (async_state == DAST_SUCCESS);

    // if frame got, schedule to next filter
    if (got_frame)
        return 0;

    if (ff_inlink_acknowledge_status(inlink, &status, &pts)) {
        if (status == AVERROR_EOF) {
            int64_t out_pts = pts;
            ret = dnn_detect_flush_frame(outlink, pts, &out_pts);
            ff_outlink_set_status(outlink, status, out_pts);
            return ret;
        }
    }

    FF_FILTER_FORWARD_WANTED(outlink, inlink);

    return 0;
}

static int dnn_detect_activate(AVFilterContext *filter_ctx)
{
    DnnDetectContext *ctx = filter_ctx->priv;

    if (ctx->dnnctx.async)
        return dnn_detect_activate_async(filter_ctx);
    else
        return dnn_detect_activate_sync(filter_ctx);
}

static av_cold void dnn_detect_uninit(AVFilterContext *context)
{
    DnnDetectContext *ctx = context->priv;
    ff_dnn_uninit(&ctx->dnnctx);
    free_detect_labels(ctx);
}

static const AVFilterPad dnn_detect_inputs[] = {
    {
        .name         = "default",
        .type         = AVMEDIA_TYPE_VIDEO,
    },
    { NULL }
};

static const AVFilterPad dnn_detect_outputs[] = {
    {
        .name = "default",
        .type = AVMEDIA_TYPE_VIDEO,
    },
    { NULL }
};

const AVFilter ff_vf_dnn_detect = {
    .name          = "dnn_detect",
    .description   = NULL_IF_CONFIG_SMALL("Apply DNN detect filter to the input."),
    .priv_size     = sizeof(DnnDetectContext),
    .init          = dnn_detect_init,
    .uninit        = dnn_detect_uninit,
    .query_formats = dnn_detect_query_formats,
    .inputs        = dnn_detect_inputs,
    .outputs       = dnn_detect_outputs,
    .priv_class    = &dnn_detect_class,
    .activate      = dnn_detect_activate,
};
