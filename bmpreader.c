/*
  bmpreader.c: Windows Bitmap image reader for VapourSynth

  This file is a part of vsbmpreader

  Copyright (C) 2012  Oka Motofumi

  Author: Oka Motofumi (chikuzen.mo at gmail dot com)

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with Libav; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include "bmpreader.h"
#include "VapourSynth.h"

#ifdef _MSC_VER
#pragma warning(disable:4996)
#define snprintf _snprintf
#endif

#define VS_BMPR_VERSION "0.1.0"

typedef struct {
    const char **src_files;
    color_palette_t *palettes;
    uint8_t *read_buff;
    VSVideoInfo vi;
} bmp_hnd_t;

typedef struct {
    const VSMap *in;
    VSMap *out;
    VSCore *core;
    const VSAPI *vsapi;
} vs_args_t;


static inline uint32_t
bitor8to32(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3)
{
    return (((uint32_t)b0) << 24) | (((uint32_t)b1) << 16) |
           (((uint32_t)b2) << 8) | (uint32_t)b3;
}


static void VS_CC
write_bgr32_frame(VSFrameRef *dst, bmp_hnd_t *bh, const VSAPI *vsapi)
{
    typedef struct {
        uint8_t c[16];
    } bgr32_t;

    uint8_t *srcp_orig = bh->read_buff;
    int row_size = (bh->vi.width + 3) >> 2;
    int src_stride = bh->vi.width << 2;
    int height = bh->vi.height;

    uint32_t *dstp_b = (uint32_t *)vsapi->getWritePtr(dst, 2);
    uint32_t *dstp_g = (uint32_t *)vsapi->getWritePtr(dst, 1);
    uint32_t *dstp_r = (uint32_t *)vsapi->getWritePtr(dst, 0);
    int dst_stride = vsapi->getStride(dst, 0) >> 2;

    for (int y = 0; y < height; y++) {
        bgr32_t *srcp = (bgr32_t *)(srcp_orig + y * src_stride);
        for (int x = 0; x < row_size; x++) {
            dstp_b[x] = bitor8to32(srcp[x].c[12], srcp[x].c[8],
                                  srcp[x].c[4], srcp[x].c[0]);
            dstp_g[x] = bitor8to32(srcp[x].c[13], srcp[x].c[9],
                                  srcp[x].c[5], srcp[x].c[1]);
            dstp_r[x] = bitor8to32(srcp[x].c[14], srcp[x].c[10],
                                  srcp[x].c[6], srcp[x].c[2]);
        }
        dstp_b += dst_stride;
        dstp_g += dst_stride;
        dstp_r += dst_stride;
    }
}


static void VS_CC
write_bgr24_frame(VSFrameRef *dst, bmp_hnd_t *bh, const VSAPI *vsapi)
{
    typedef struct {
        uint8_t c[12];
    } bgr24_t;

    uint8_t *srcp_orig = bh->read_buff;
    int row_size = (bh->vi.width + 3) >> 2;
    int src_stride = (bh->vi.width * 3 + 3) & (~3);
    int height = bh->vi.height;

    uint8_t *dstp_b_orig = vsapi->getWritePtr(dst, 2);
    uint8_t *dstp_g_orig = vsapi->getWritePtr(dst, 1);
    uint8_t *dstp_r_orig = vsapi->getWritePtr(dst, 0);
    int dst_stride = vsapi->getStride(dst, 0);

    for (int y = 0; y < height; y++) {
        bgr24_t *srcp = (bgr24_t *)(srcp_orig + y * src_stride);
        uint32_t *dstp_b = (uint32_t *)(dstp_b_orig + y * dst_stride);
        uint32_t *dstp_g = (uint32_t *)(dstp_g_orig + y * dst_stride);
        uint32_t *dstp_r = (uint32_t *)(dstp_r_orig + y * dst_stride);
        for (int x = 0; x < row_size; x++) {
            dstp_b[x] = bitor8to32(srcp[x].c[9], srcp[x].c[6],
                                  srcp[x].c[3], srcp[x].c[0]);
            dstp_g[x] = bitor8to32(srcp[x].c[10], srcp[x].c[7],
                                  srcp[x].c[4], srcp[x].c[1]);
            dstp_r[x] = bitor8to32(srcp[x].c[11], srcp[x].c[8],
                                  srcp[x].c[5], srcp[x].c[2]);
        }
    }
}


static void VS_CC
write_palette_frame(VSFrameRef *dst, bmp_hnd_t *bh, int bits_per_pix,
                    const VSAPI *vsapi)
{
    color_palette_t *palette = bh->palettes;

    uint8_t *srcp_orig = bh->read_buff;
    int row_size = bh->vi.width;
    int src_stride = (((bh->vi.width * bits_per_pix + 7) >> 3) + 3) & (~3);
    int height = bh->vi.height;

    uint8_t *dstp_b = vsapi->getWritePtr(dst, 2);
    uint8_t *dstp_g = vsapi->getWritePtr(dst, 1);
    uint8_t *dstp_r = vsapi->getWritePtr(dst, 0);
    int dst_stride = vsapi->getStride(dst, 0);

    uint8_t mask = (1 << bits_per_pix) - 1;

    for (int y = 0; y < height; y++) {
        uint8_t *srcp = srcp_orig + y * src_stride;
        for (int x = 0, shift = 8; x < row_size; x++) {
            shift -= bits_per_pix;
            dstp_b[x] = palette[(*srcp >> shift) & mask].blue;
            dstp_g[x] = palette[(*srcp >> shift) & mask].green;
            dstp_r[x] = palette[(*srcp >> shift) & mask].red;
            if (shift < 1) {
                shift = 8;
                srcp++;
            }
        }
        dstp_b += dst_stride;
        dstp_g += dst_stride;
        dstp_r += dst_stride;
    }
}


static const VSFrameRef * VS_CC
bmp_get_frame(int n, int activation_reason, void **instance_data,
              void **frame_data, VSFrameContext *frame_ctx, VSCore *core,
              const VSAPI *vsapi)
{
    if (activation_reason != arInitial) {
        return NULL;
    }

    bmp_hnd_t *bh = (bmp_hnd_t *)*instance_data;
    bmp_header_t h;

    int frame_number = n;
    if (n >= bh->vi.numFrames) {
        frame_number = bh->vi.numFrames - 1;
    }

    FILE *fp = fopen(bh->src_files[frame_number], "rb");
    if (!fp) {
        return NULL;
    }
    if (fread(&h, 1, sizeof(bmp_header_t), fp) != sizeof(bmp_header_t)) {
        fclose(fp);
        return NULL;
    };

    uint32_t frame_size =
        ((((h.width * h.bits_per_pix + 7) >> 3) + 3) & ~3) * h.height;
    
    if (h.bits_per_pix < 24) {
        fread(bh->palettes, sizeof(color_palette_t),
              1 << h.bits_per_pix, fp);
    }
    fseek(fp, h.offset_data, SEEK_SET);
    uint32_t read = fread(bh->read_buff, 1, frame_size, fp);
    fclose(fp);
    if (read != frame_size) {
        return NULL;
    }

    VSFrameRef *dst = vsapi->newVideoFrame(bh->vi.format, bh->vi.width,
                                           bh->vi.height, NULL, core);

    VSMap *props = vsapi->getFramePropsRW(dst);
    vsapi->propSetInt(props, "_DurationNum", bh->vi.fpsDen, 0);
    vsapi->propSetInt(props, "_DurationDen", bh->vi.fpsNum, 0);

    switch (h.bits_per_pix) {
    case 32:
        write_bgr32_frame(dst, bh, vsapi);
        break;
    case 24:
        write_bgr24_frame(dst, bh, vsapi);
        break;
    default:
        write_palette_frame(dst, bh, h.bits_per_pix, vsapi);
    }

    return dst;
}


static void VS_CC close_handler(bmp_hnd_t *bh)
{
    if (!bh) {
        return;
    }
    free(bh->palettes);
    free(bh->read_buff);
    free(bh);
}


static void VS_CC
vs_init(VSMap *in, VSMap *out, void **instance_data, VSNode *node,
        VSCore *core, const VSAPI *vsapi)
{
    bmp_hnd_t *bh = (bmp_hnd_t *)*instance_data;
    vsapi->setVideoInfo(&bh->vi, node);
}


static void VS_CC
vs_close(void *instance_data, VSCore *core, const VSAPI *vsapi)
{
    bmp_hnd_t *bh = (bmp_hnd_t *)instance_data;
    close_handler(bh);
}


static char * VS_CC
check_srcs(bmp_hnd_t *bh, int n)
{
    FILE *fp = fopen(bh->src_files[n], "rb");
    if (!fp) {
        return "failed to open file";
    }

    bmp_header_t header = { 0 };
    fread(&header, 1, sizeof(bmp_header_t), fp);
    if (header.file_type != BMP_HEADER_MAGIC || header.header_size != 40 ||
        header.num_planes != 1 || header.fourcc != 0 ||
        (header.bits_per_pix != 1 && header.bits_per_pix != 2 &&
         header.bits_per_pix != 4 && header.bits_per_pix != 8 &&
         header.bits_per_pix != 24 && header.bits_per_pix != 32)) {
        fclose(fp);
        return "unsupported format";
    }

    if (header.width != bh->vi.width || header.height != bh->vi.height) {
        if (n > 0) {
            fclose(fp);
            return "found a file which has diffrent resolution from the first one";
        }
        bh->vi.width = header.width;
        bh->vi.height = header.height;
    }

    fclose(fp);

    return NULL;
}


static void VS_CC
set_args_int64(int64_t *p, int default_value, const char *arg, vs_args_t *va)
{
    int err;
    *p = va->vsapi->propGetInt(va->in, arg, 0, &err);
    if (err) {
        *p = default_value;
    }
}


#define RET_IF_ERR(cond, ...) \
{\
    if (cond) {\
        close_handler(bh);\
        snprintf(msg, 240, __VA_ARGS__);\
        vsapi->setError(out, msg_buff);\
        return;\
    }\
}

static void VS_CC
create_reader(const VSMap *in, VSMap *out, void *user_data, VSCore *core,
              const VSAPI *vsapi)
{
    char msg_buff[256] = "bmpr: ";
    char *msg = msg_buff + strlen(msg_buff);
    vs_args_t va = {in, out, core, vsapi};

    bmp_hnd_t *bh = (bmp_hnd_t *)calloc(sizeof(bmp_hnd_t), 1);
    RET_IF_ERR(!bh, "failed to create handler");

    int num_srcs = vsapi->propNumElements(in, "files");
    RET_IF_ERR(num_srcs < 1, "no source file");
    bh->vi.numFrames = num_srcs;

    bh->src_files = (const char **)calloc(sizeof(char *), num_srcs);
    RET_IF_ERR(!bh->src_files, "failed to allocate array of src files");

    int err;
    for (int i = 0; i < num_srcs; i++) {
        bh->src_files[i] = vsapi->propGetData(in, "files", i, &err);
        RET_IF_ERR(err || strlen(bh->src_files[i]) == 0,
                   "zero length file name was found");
    }

    bh->palettes = (color_palette_t *)calloc(sizeof(color_palette_t), 256);
    RET_IF_ERR(!bh->palettes, "failed to allocate color palette buffer");

    for (int i = 0; i < num_srcs; i++) {
        char *cs = check_srcs(bh, i);
        RET_IF_ERR(cs, "file %i: %s", i, cs);
    }

    bh->vi.format = vsapi->getFormatPreset(pfRGB24, core);
    set_args_int64(&bh->vi.fpsNum, 24, "fpsnum", &va);
    set_args_int64(&bh->vi.fpsDen, 1, "fpsden", &va);

    bh->read_buff = (uint8_t *)malloc(bh->vi.width * bh->vi.height * 4 + 32);
    RET_IF_ERR(!bh->read_buff, "failed to allocate read buffer");

    const VSNodeRef *node =
        vsapi->createFilter(in, out, "Read", vs_init, bmp_get_frame, vs_close,
                            fmSerial, 0, bh, core);

    vsapi->propSetNode(out, "clip", node, 0);
}


VS_EXTERNAL_API(void) VapourSynthPluginInit(
    VSConfigPlugin f_config, VSRegisterFunction f_register, VSPlugin *plugin)
{
    f_config("chikuzen.does.not.have.his.own.domain.bmpr", "bmpr",
             "Windows Bitmap reader for VapourSynth " VS_BMPR_VERSION,
             VAPOURSYNTH_API_VERSION, 1, plugin);
    f_register("Read", "files:data[];fpsnum:int:opt;fpsden:int:opt",
               create_reader, NULL, plugin);
}
