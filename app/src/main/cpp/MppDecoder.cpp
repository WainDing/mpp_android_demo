//
// Created by root on 17-11-30.
//

#include "MppDecoder.h"

#define DEFAULT_PKT_SIZE    (0xfff)
//#define DEBUG_WRITE_RAW     1

MppDecoder::MppDecoder() : pkt_buf_(NULL),
                           eos_(0),
                           frm_cnt_(0),
                           ctx_(NULL),
                           mpi_(NULL),
                           packet_(NULL),
                           frame_(NULL),
                           frame_rga_(NULL),
                           frm_grp_(NULL),
                           width_(0),
                           height_(0)
{

}

MppDecoder::~MppDecoder() {

}

MPP_RET MppDecoder::init(MppCtxType codec, MppCodingType type, int width, int height,
                         int display_width, int display_height) {
    MPP_RET  ret = MPP_OK;
    MpiCmd   mpi_cmd = MPP_CMD_BASE;
    MppParam param = NULL;
    RK_S32   need_split = 1;
    int      result = 0;
    int      size = display_width * display_height * 4;

    width_  = width;
    height_ = height;

    ret = mpp_create(&ctx_, &mpi_);
    if (ret) {
        LOGE("failed to create mpp ctx ret:%d.\n", ret);
        goto RET;
    }

    mpi_cmd = MPP_DEC_SET_PARSER_SPLIT_MODE;
    param = &need_split;
    ret = mpi_->control(ctx_, mpi_cmd, param);
    if (ret != MPP_OK) {
        LOGE("failed to set control param MPP_DEC_SET_PARSER_SPLIT_MODE ret:%d.\n", ret);
        goto RET;
    }

    ret = mpp_init(ctx_, codec, type);
    if (ret) {
        LOGE("failed to init mpp ctx ret:%d.\n", ret);
        goto RET;
    }

    pkt_buf_ = new char[DEFAULT_PKT_SIZE];
    if (!pkt_buf_) {
        LOGE("failed to alloc pkt_buf_\n");
        ret = MPP_ERR_MALLOC;
        goto RET;
    }

    ret = mpp_packet_init(&packet_, pkt_buf_, DEFAULT_PKT_SIZE);
    if (ret != MPP_OK) {
        LOGE("mpp_packet_init failed\n");
        goto RET;
    }

    ret = mpp_buffer_group_get_internal(&rga_grp_, MPP_BUFFER_TYPE_ION);
    if (ret) {
        LOGE("get mpp buffer group rga_grp_ failed ret %d\n", ret);
        goto RET;
    }

    ret = mpp_buffer_get(rga_grp_, &rga_buffer_, size);
    if (ret) {
        LOGE("get mpp buffer rga_grp_ failed ret:%d\n", ret);
        goto RET;
    }

    ret = mpp_frame_init(&frame_rga_);
    if (ret) {
        LOGE("failed to init frame frame_rga_ ret:%d\n", ret);
        goto RET;
    }

    mpp_frame_set_width(frame_rga_, display_width);
    mpp_frame_set_height(frame_rga_, display_height);
    mpp_frame_set_hor_stride(frame_rga_, SIZE_ALIGN(display_width, 16));
    mpp_frame_set_ver_stride(frame_rga_, SIZE_ALIGN(display_height, 16));
    mpp_frame_set_fmt(frame_rga_, MPP_FMT_ARGB8888);
    mpp_frame_set_buffer(frame_rga_, rga_buffer_);

#ifdef DEBUG_WRITE_RAW
    fout_ = new std::ofstream("/sdcard/Movies/debug.yuv", std::ios::binary);
    if (!fout_) {
        LOGE("failed to open file /sdcard/Movies/debug.yuv.\n");
        ret = MPP_ERR_INIT;
        goto RET;
    }
#endif
RET:
    return ret;
}

MPP_RET MppDecoder::deinit() {
    MPP_RET ret = MPP_OK;

    ret = mpi_->reset(ctx_);
    if (ret != MPP_OK) {
        LOGE("mpi_->reset failed\n");
        goto DEINIT;
    }

    if (packet_) {
        mpp_packet_deinit(&packet_);
        packet_ = NULL;
    }

    if (rga_buffer_) {
        mpp_buffer_put(rga_buffer_);
        rga_buffer_ = NULL;
    }

    if (frame_) {
        mpp_frame_deinit(&frame_);
        frame_ = NULL;
    }

    if (frame_rga_) {
        mpp_frame_deinit(&frame_rga_);
        frame_rga_ = NULL;
    }

    if (pkt_buf_) {
        delete pkt_buf_;
        pkt_buf_ = NULL;
    }

    if (frm_grp_) {
        mpp_buffer_group_put(frm_grp_);
        frm_grp_ = NULL;
    }

    if (rga_grp_) {
        mpp_buffer_group_put(rga_grp_);
        rga_grp_ = NULL;
    }

    if (ctx_) {
        mpp_destroy(ctx_);
        ctx_ = NULL;
    }

#ifdef DEBUG_WRITE_RAW
    if (fout_) {
        fout_->close();
    }
#endif
DEINIT:
    return ret;
}

MPP_RET MppDecoder::decode(char *pkt_buf, int pkt_size, RK_U32 eos) {
    MPP_RET ret = MPP_OK;
    RK_S32 result = 0;
    RK_S32 pkt_done = 0;
    char *out_buf = NULL;

    memcpy(pkt_buf_, pkt_buf, pkt_size);

    mpp_packet_write(packet_, 0, pkt_buf_, pkt_size);
    mpp_packet_set_pos(packet_, pkt_buf_);
    mpp_packet_set_length(packet_, pkt_size);

    if (eos) {
        eos_ = eos;
        mpp_packet_set_eos(packet_);
    }

    do {
        RK_S32 times = 5;

        if (!pkt_done) {
            ret = mpi_->decode_put_packet(ctx_, packet_);
            if (ret == MPP_OK)
                pkt_done = 1;
        }

        do {
            RK_S32 get_frm = 0;
            RK_U32 frm_eos = 0;
 try_again:
            ret = mpi_->decode_get_frame(ctx_, &frame_);
            if (ret == MPP_ERR_TIMEOUT) {
                if (times > 0) {
                    times--;
                    usleep(2 * 1000);
                    goto try_again;
                }
                LOGE("decode_get_frame failed too much time\n");
            }

            if (ret != MPP_OK) {
                LOGE("decode_get_frame failed ret %d\n", ret);
                break;
            }

            if (frame_) {
                if (mpp_frame_get_info_change(frame_)) {
                    RK_U32 width  = mpp_frame_get_width(frame_);
                    RK_U32 height = mpp_frame_get_height(frame_);
                    RK_U32 hor_stride = mpp_frame_get_hor_stride(frame_);
                    RK_U32 ver_stride = mpp_frame_get_ver_stride(frame_);

                    LOGE("decode_get_frame get info changed found\n");
                    LOGE("deocder require buffer w:h [%d %d] stride [%d:%d]",
                         width, height, hor_stride, ver_stride);

                    ret = mpp_buffer_group_get_internal(&frm_grp_, MPP_BUFFER_TYPE_ION);
                    if (ret) {
                        LOGE("get mpp buffer group frm_grp_ failed ret %d\n", ret);
                        break;
                    }

                    mpi_->control(ctx_, MPP_DEC_SET_EXT_BUF_GROUP, frm_grp_);
                    mpi_->control(ctx_, MPP_DEC_SET_INFO_CHANGE_READY, NULL);
                } else {
#ifdef DEBUG_WRITE_RAW
                    internal_write_frame(frame_);
#endif
                    LOGE("ecode_get_frame get frame %d\n", frm_cnt_++);
                }

                frm_eos = mpp_frame_get_eos(frame_);
                mpp_frame_deinit(&frame_);
                frame_ = NULL;
                get_frm = 1;
            }

            if (eos_ && pkt_done && !frm_eos) {
                usleep(10 * 1000);
                continue;
            }

            if (frm_eos) {
                LOGE("found last frame %d\n", pkt_done);
                break;
            }

            if (get_frm)
                continue;
            break;
        } while (1);

        if (pkt_done)
            break;
        usleep(50 * 1000);
    } while (1);

EXIT:
    return ret;
}

void MppDecoder::internal_write_frame(MppFrame frame) {
    RK_U32 width    = 0;
    RK_U32 height   = 0;
    RK_U32 h_stride = 0;
    RK_U32 v_stride = 0;
    MppFrameFormat fmt  = MPP_FMT_YUV420SP;
    MppBuffer buffer    = NULL;
    RK_U8 *base = NULL;

    if (NULL == frame)
        return ;

    width    = mpp_frame_get_width(frame);
    height   = mpp_frame_get_height(frame);
    h_stride = mpp_frame_get_hor_stride(frame);
    v_stride = mpp_frame_get_ver_stride(frame);
    fmt      = mpp_frame_get_fmt(frame);
    buffer   = mpp_frame_get_buffer(frame);

    if (NULL == buffer)
        return ;

    base = (RK_U8 *)mpp_buffer_get_ptr(buffer);

    switch (fmt) {
        case MPP_FMT_YUV420SP : {
            RK_U32 i;
            RK_U8 *base_y = base;
            RK_U8 *base_c = base + h_stride * v_stride;

            for (i = 0; i < height; i++, base_y += h_stride) {
                fout_->write((char *)base_y, width);
            }
            for (i = 0; i < height / 2; i++, base_c += h_stride) {
                fout_->write((char *)base_c, width);
            }
        } break;
        default : {
            LOGE("not supported format %d\n", fmt);
        } break;
    }
}

void MppDecoder::internal_write_argb(MppFrame frame) {
    RK_U32 width    = 0;
    RK_U32 height   = 0;
    RK_U32 h_stride = 0;
    RK_U32 v_stride = 0;
    RK_U32 write_size;
    MppFrameFormat fmt  = MPP_FMT_ARGB8888;
    MppBuffer buffer    = NULL;
    RK_U8 *base = NULL;
    char *buf = NULL;

    std::ofstream fdbg("/sdcard/rga.yuv", std::ios::binary);
    if (!fdbg) {
        LOGE("failed to open rga.yuv.\n");
    }

    if (NULL == frame)
        return ;

    width    = mpp_frame_get_width(frame);
    height   = mpp_frame_get_height(frame);
    h_stride = mpp_frame_get_hor_stride(frame);
    v_stride = mpp_frame_get_ver_stride(frame);
    fmt      = mpp_frame_get_fmt(frame);
    buffer   = mpp_frame_get_buffer(frame);

    buf = new char[h_stride * v_stride * 4];

    if (NULL == buffer)
        return ;

    base = (RK_U8 *)mpp_buffer_get_ptr(buffer);
    switch (fmt) {
        case MPP_FMT_ARGB8888 : {
            RK_U32 i;
            RK_U8 *base_y = base;
            for (i = 0; i < height; i++) {
                memcpy(buf + i * width * 4, base_y + i * h_stride * 4, width * 4);
                write_size += width * 4;
            }
            fdbg.write(buf, write_size);
        } break;
        default : {
            LOGE("not supported format %d\n", fmt);
        } break;
    }
}