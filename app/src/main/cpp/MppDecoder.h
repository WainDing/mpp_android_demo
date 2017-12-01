//
// Created by root on 17-11-30.
//

#ifndef MPP_ANDROID_DEMO_MPPDECODER_H
#define MPP_ANDROID_DEMO_MPPDECODER_H

#include <fstream>
#include "android/log.h"

extern "C" {
#include "rk_mpi.h"
#include "unistd.h"
};

#define LOGE(...)       __android_log_print(ANDROID_LOG_ERROR,"sliver",__VA_ARGS__)
#define SIZE_ALIGN(x,a) (((x)+(a)-1)&~((a)-1))

class MppDecoder {
public:
    MppDecoder();
    ~MppDecoder();
    MPP_RET init(MppCtxType codec, MppCodingType type, int width, int height,
                 int display_width, int display_height);
    MPP_RET deinit();
    MPP_RET decode(char *pkt_buf, int pkt_size, RK_U32 eos);
private:
    void internal_write_frame(MppFrame frame);
    void internal_write_argb(MppFrame frame);

    RK_S32    width_;
    RK_S32    height_;
    RK_U32    eos_;
    RK_U32    frm_cnt_;
    char      *pkt_buf_;
    MppCtx    ctx_;
    MppApi    *mpi_;
    MppPacket packet_;
    MppFrame  frame_;
    MppFrame  frame_rga_;
    MppBuffer rga_buffer_;
    MppBufferGroup frm_grp_;
    MppBufferGroup rga_grp_;
    std::ofstream  *fout_;
};

#endif //MPP_ANDROID_DEMO_MPPDECODER_H
