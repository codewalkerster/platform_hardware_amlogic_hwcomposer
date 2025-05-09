#ifndef AISUBTITLE_PROCESSOR_H
#define AISUBTITLE_PROCESSOR_H

#include <FbProcessor.h>
#include <queue>
#include <linux/ion.h>
#include <ion/ion.h>
#include <UvmDev.h>
#include "adaptive_subtitle.h"

#define AISUBTITLE_INPUT_WIDTH    640
#define AISUBTITLE_INPUT_HEIGHT   480
#define AISUBTITLE_FIXED_WIDTH    400
#define AISUBTITLE_FIXED_HEIGHT   50

#define AISUBTITLE_MAX_CACHE_COUNT 5

#define AISUBTITLE_NB_NORMAL_PATH "/vendor/etc/"
#define AISUBTITLE_SKIP_FRAME_HEIGHT    1088

#define FIXED_AREA_NUM 4

struct aisubtitle_buffer_t {
    int fd;
    void *fd_ptr; //only for non-nativebuffer!
    int size;
    buffer_handle_t buffer_handle;
};

struct rect_t {
    int x1;
    int y1;
    int x2;
    int y2;
};

struct aisubtitle_time_info_t {
    int64_t count;
    uint64_t max_time;
    uint64_t min_time;
    uint64_t total_time;
    uint64_t avg_time;
};

struct aisubtitle_index_value_t {
    int buf_index;
    int shared_fd;
};

enum aisubtitle_get_info_type_e {
    AISUBTITLE_GET_INVALID = 0,
    AISUBTITLE_GET_DATA = 1,
    AISUBTITLE_GET_INDEX_INFO = 2,
    AISUBTITLE_GET_BASIC_INFO = 3,
};

enum out_adaptive_area_e {
    ADAPTIVE_AREA1,
    ADAPTIVE_AREA2,
    ADAPTIVE_AREA3,
    ADAPTIVE_AREA4,
};

/*hwc attach aisubtitle info*/
struct uvm_aisubtitle_info {
    int32_t shared_fd;
    int32_t aisubtitle_fd;
    int32_t need_do_aisubtitle;
    int32_t repeat_frame;
    int32_t dw_width;
    int32_t dw_height;
    int32_t nn_input_frame_width;
    int32_t nn_input_frame_height;
    int32_t frame_index;
    int32_t is_secure_source;
    int32_t get_info_type;
    int32_t out_area;
    int32_t reserved[5];
};

struct uvm_aisubtitle_info_t {
    enum uvm_hook_mod_type mode_type;
    int shared_fd;
    struct uvm_aisubtitle_info aisubtitle_info;
};

union uvm_aisubtitle_ioctl_arg {
    struct uvm_hook_data hook_data;
    struct uvm_aisubtitle_info uvm_info;
};

class AiSubtitleProcessor : public FbProcessor {
public:
    AiSubtitleProcessor();
    ~AiSubtitleProcessor();

    int32_t setup();
    int32_t process(
        std::shared_ptr<DrmFramebuffer> & inputfb,
        std::shared_ptr<DrmFramebuffer> & outfb);
    int32_t asyncProcess(
        std::shared_ptr<DrmFramebuffer> & inputfb,
        std::shared_ptr<DrmFramebuffer> & outfb,
        int & processFence);
    int32_t onBufferDisplayed(
        std::shared_ptr<DrmFramebuffer> & outfb,
        int releaseFence);
    int32_t teardown();
    meson_fb_processor_t getFbProcessorType() {return FB_AISUBTITLE_PROCESSOR;};
    int allocDmaBuffer();
    int freeDmaBuffers();
    void triggerEvent();
    void threadProcess();
    int32_t waitEvent();
    static void *mNn_qcontext;
    static bool mModelLoaded;
    mutable std::mutex mMutex;
    std::queue<int> mBuf_fd_q;
    static void * threadMain(void * data);
    int LoadNNModel();
    pthread_t mThread;
    bool mExitThread;
    bool mInited;
    int32_t ai_subtitle_process(int input_fd);
    int32_t intersection_area(const struct rect_t *rect_a, const struct rect_t *rect_b);
    int32_t cal_adaptive_subtitle_area(subtitle_out_t *nn_out);
    void get_fixed_area();
    void dump_nn_info(int num);
    int PropGetInt(const char* str, int def);
    bool PropSetInt(const char* str, int value);
    int check_D();
    pthread_mutex_t m_waitMutex;
    pthread_cond_t m_waitCond;
    int mUvmHandler;
    int mNn_Index;
    static struct aisubtitle_time_info_t mTime;
    static int mInstanceID;
    static int mLogLevel;
    bool mBuf_Alloced;
    bool mNnDoing;
    aisubtitle_buffer_t mAiSubtitle_Buf;
    int mNnInputWidth;
    int mNnInputHeight;
    int mDumpIndex;
    int64_t mDupCount;
    int64_t mCloseCount;
    static int64_t mTotalDupCount;
    static int64_t mTotalCloseCount;
    struct aisubtitle_index_value_t mAiSubtitleIndex[AISUBTITLE_MAX_CACHE_COUNT];
    int mCacheIndex;
    int mBuf_index;
    subtitle_out_t nn_out_temp;
    int adaptive_area;
    struct rect_t mFixedArea[FIXED_AREA_NUM];
    int mAreaSum[FIXED_AREA_NUM];
    int mFixedWidth;
    int mFixedHeight;
    int mAisubtitleFrameInterval;
};

#endif
