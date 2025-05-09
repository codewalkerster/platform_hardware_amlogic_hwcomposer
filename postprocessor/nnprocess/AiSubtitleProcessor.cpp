#define LOG_NDEBUG 0
#define LOG_TAG "hwc_aisubtitle"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#include <utils/Trace.h>
#include <inttypes.h>

#include "AiSubtitleProcessor.h"
#include <MesonLog.h>
#include <ui/Fence.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sched.h>
#include <cutils/properties.h>
#include <ui/GraphicBufferAllocator.h>
#include <hardware/gralloc1.h>
#include <math.h>
#include <unistd.h>

#define FENCE_TIMEOUT_MS 1000

#define UVM_IOC_MAGIC 'U'

#define UVM_IOC_ATTACH _IOWR(UVM_IOC_MAGIC, 5, \
                struct uvm_hook_data)
#define UVM_IOC_GET_INFO _IOWR(UVM_IOC_MAGIC, 6, \
                struct uvm_hook_data)
#define UVM_IOC_SET_INFO _IOWR(UVM_IOC_MAGIC, 7, \
                struct uvm_hook_data)

int AiSubtitleProcessor::mInstanceID = 0;
int64_t AiSubtitleProcessor::mTotalDupCount = 0;
int64_t AiSubtitleProcessor::mTotalCloseCount = 0;
struct aisubtitle_time_info_t AiSubtitleProcessor::mTime;
bool AiSubtitleProcessor::mModelLoaded;
void* AiSubtitleProcessor::mNn_qcontext;
int AiSubtitleProcessor::mLogLevel = 0;

int AiSubtitleProcessor::check_D() {
    return (mLogLevel > 0);
}

int AiSubtitleProcessor::PropGetInt(const char* str, int def) {
    char value[PROPERTY_VALUE_MAX];
    int ret = def;
    if (property_get(str, value, NULL) > 0) {
        ret = atoi(value);
        return ret;
    }
    return ret;
}

void AiSubtitleProcessor::threadProcess() {
    int shared_fd = -1;
    int size = 0;
    int cache_index;

    size = mBuf_fd_q.size();
    if (size == 0) {
        waitEvent();
        return;
    }

    if (size > 1)
        ALOGE("%s: more than one buf need process size=%d", __FUNCTION__, size);

    {
        std::lock_guard<std::mutex> lock(mMutex);
        cache_index = mBuf_fd_q.front();
    }
    shared_fd = mAiSubtitleIndex[cache_index].shared_fd;

    ai_subtitle_process(cache_index);

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mBuf_fd_q.pop();
    }

    close(shared_fd);
    mCloseCount++;
    mTotalCloseCount++;
    return;
}

void * AiSubtitleProcessor::threadMain(void * data) {
    AiSubtitleProcessor *pThis = (AiSubtitleProcessor *)data;
    struct sched_param param = {0};

    param.sched_priority = 2;
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        ALOGE("%s: Couldn't set SCHED_FIFO: %d.\n", __FUNCTION__, errno);
    }

    MESON_ASSERT(data, "AiSubtitleProcessor data should not be NULL.\n");

    while (!pThis->mExitThread) {
        pThis->threadProcess();
    }

    ALOGD("%s exit.\n", __FUNCTION__);
    pthread_exit(0);
    return NULL;
}

int AiSubtitleProcessor::LoadNNModel() {
    ALOGD("AiSubtitleProcessor: %s start.\n", __FUNCTION__);
    int ret = 1;
    struct timespec time1, time2;
    uint64_t totalTime;

    clock_gettime(CLOCK_MONOTONIC, &time1);

    mNn_qcontext = adaptive_subtitle_init(AISUBTITLE_NB_NORMAL_PATH);
    if (mNn_qcontext == NULL) {
        ALOGE("ai_subtitle_init fail! %s\n", AISUBTITLE_NB_NORMAL_PATH);
        return -1;
    }

    clock_gettime(CLOCK_MONOTONIC, &time2);
    totalTime = (time2.tv_sec * 1000000LL + time2.tv_nsec / 1000)
                    - (time1.tv_sec * 1000000LL + time1.tv_nsec / 1000);

    mModelLoaded = true;
    ALOGD("%s: load NN model spend %" PRId64" ns.\n", __FUNCTION__, totalTime);

    return ret;
}

AiSubtitleProcessor::AiSubtitleProcessor() {
    ATRACE_CALL();
    ALOGD("%s", __FUNCTION__);
    mNnDoing = false;
    mBuf_Alloced = false;
    mExitThread = true;
    pthread_mutex_init(&m_waitMutex, NULL);
    pthread_cond_init(&m_waitCond, NULL);

    mAiSubtitle_Buf.fd = -1;
    mAiSubtitle_Buf.fd_ptr = NULL;
    mAiSubtitle_Buf.size = -1;
    mAiSubtitle_Buf.buffer_handle = NULL;
    mNnInputWidth = PropGetInt("vendor.hwc.aisubtile.pic_width",
                                     AISUBTITLE_INPUT_WIDTH);
    mNnInputHeight = PropGetInt("vendor.hwc.aisubtile.pic_height",
                                     AISUBTITLE_INPUT_HEIGHT);
    mFixedWidth = PropGetInt("vendor.hwc.aisubtile.area_width",
                                     AISUBTITLE_FIXED_WIDTH);
    mFixedHeight = PropGetInt("vendor.hwc.aisubtile.area_height",
                                     AISUBTITLE_FIXED_HEIGHT);
	mAisubtitleFrameInterval = PropGetInt("vendor.hwc.aisubtile.frame_interval", 1);

    mInited = false;
    mUvmHandler = -1;
    mNn_Index = 0;
    mCacheIndex = 0;
    mBuf_index = 0;
    mThread = 0;

    if (mInstanceID == 0) {
        mTime.count = 0;
        mTime.max_time = 0;
        mTime.min_time = 0;
        mTime.total_time = 0;
        mTime.avg_time = 0;
        mNn_qcontext = NULL;
        mModelLoaded = false;
        isAdaptiveSubtitleInterfaceImplement();
    }

    get_fixed_area();

    if (!mModelLoaded)
        LoadNNModel();

    mUvmHandler = open("/dev/uvm", O_RDWR | O_NONBLOCK);
    if (mUvmHandler < 0) {
        ALOGE("can not open uvm");
    } else {
        ALOGE("open uvm");
    }
}

AiSubtitleProcessor::~AiSubtitleProcessor() {
    ALOGD("%s: mDupCount =%" PRId64", mCloseCount =%" PRId64", total %" PRId64" %" PRId64"",
        __FUNCTION__, mDupCount, mCloseCount, mTotalDupCount, mTotalCloseCount);

    if (mDupCount != mCloseCount)
        ALOGE("%s: count err: %" PRId64" %" PRId64"", __FUNCTION__, mDupCount, mCloseCount);

    if (mTotalDupCount != mTotalCloseCount)
        ALOGE("%s: total count err: %" PRId64" %" PRId64"",
             __FUNCTION__,mTotalDupCount, mTotalCloseCount);

    if (mInited)
        teardown();

    if (mTime.count > 0) {
        mTime.avg_time = mTime.total_time / mTime.count;
    }
    ALOGD("%s: time: count=%" PRId64", max=%" PRId64", min=%" PRId64", avg=%" PRId64"",
        __FUNCTION__, mTime.count, mTime.max_time, mTime.min_time, mTime.avg_time);

    if (mUvmHandler) {
        close(mUvmHandler);
        mUvmHandler = -1;
    }
}

int32_t AiSubtitleProcessor::setup() {
    ATRACE_CALL();
    ALOGD("%s", __FUNCTION__);
    if (!mUvmHandler) {
        ALOGD("%s: init action is not ok.\n", __FUNCTION__);
        return -1;
    }

    if (mExitThread == true) {
            ALOGD("threadMain creat");
            mExitThread = false;
            int ret = pthread_create(&mThread,
                                     NULL,
                                     AiSubtitleProcessor::threadMain,
                                     (void *)this);
            if (ret != 0) {
                ALOGE("failed to start AiSubtitleProcessor main thread: %s",
                      strerror(ret));
                mExitThread = true;
            }
    }

    mInited = true;

    return 0;
}

int32_t AiSubtitleProcessor::process(
    std::shared_ptr<DrmFramebuffer> & inputfb __unused,
    std::shared_ptr<DrmFramebuffer> & outfb __unused) {
    return 0;
}

int32_t AiSubtitleProcessor::asyncProcess(
        std::shared_ptr<DrmFramebuffer> & inputfb,
        std::shared_ptr<DrmFramebuffer> & outfb,
        int & processFence) {
    ATRACE_CALL();
    int ret;
    int ret_attach = 0;
    buffer_handle_t buf = inputfb->mBufferHandle;
    struct uvm_hook_data hook_data;
    struct uvm_aisubtitle_info_t *uvm_info;
    struct uvm_aisubtitle_info *aisubtitle;
    int dup_fd = -1;
    int ready_size = 0;
    int input_fd = -1;

    mLogLevel = PropGetInt("vendor.hwc.aisubtitle_log", 0);

    processFence = -1;
    outfb = inputfb;

    drm_fb_type_t type = inputfb->getFbType();
    if (type == DRM_FB_VIDEO_UVM_DMA ||
        type == DRM_FB_VIDEO_TUNNEL_SIDEBAND) {
        input_fd = inputfb->getBufferFd();
    } else
        ALOGE("%s: get fd fail type=%d.", __FUNCTION__, type);

    if (input_fd == -1) {
        ALOGD_IF(check_D(), "%s: input_fd invalid.", __FUNCTION__);
        goto bypass;
    }

    if (!mUvmHandler) {
        goto bypass;
    }

    if (!mModelLoaded)
        goto bypass;

    if (!mInited) {
        ALOGE("%s: has teardown need bypass", __FUNCTION__);
        goto bypass;
    }

    memset(&hook_data, 0, sizeof(struct uvm_hook_data));

    uvm_info = (struct uvm_aisubtitle_info_t *)&hook_data;
    aisubtitle = &(uvm_info->aisubtitle_info);

    uvm_info->mode_type = PROCESS_AISUBTITLE;
    uvm_info->shared_fd = input_fd;
    aisubtitle->shared_fd = input_fd;
    aisubtitle->need_do_aisubtitle = 0;
    aisubtitle->repeat_frame = 0;
    aisubtitle->nn_input_frame_width = mNnInputWidth;
    aisubtitle->nn_input_frame_height = mNnInputHeight;

    ret_attach = ioctl(mUvmHandler, UVM_IOC_ATTACH, &hook_data);
    if (ret_attach != 0) {
        ALOGE("attach err: ret_attach =%d", ret_attach);
        goto bypass;
    }

	if ((aisubtitle->frame_index % mAisubtitleFrameInterval) != 0) {
		ALOGD_IF(check_D(), "aisubtitle bypass, frame_index=%d", aisubtitle->frame_index);
		goto bypass;
	}

    if (aisubtitle->need_do_aisubtitle == 0) {
        ALOGD_IF(check_D(), "attach: aisubtitle bypass");
        goto error;
    }

    if (!mBuf_Alloced) {
        ret = allocDmaBuffer();
        if (ret) {
            ALOGE("%s: alloc buffer fail", __FUNCTION__);
            goto error;
        }
        mBuf_Alloced = true;
    }

    //reduce system loading by bypass aisubtitle when 4kh264 video//
    if ((aisubtitle->dw_height > AISUBTITLE_SKIP_FRAME_HEIGHT) &&
        ((mBuf_index % 2) == 0)) {
        mBuf_index++;
        goto error;
    }

    ALOGD_IF(check_D(), "set NN_WAIT_DOING: frame_index=%d", aisubtitle->frame_index);
    uvm_info->mode_type = PROCESS_AISUBTITLE;
    uvm_info->shared_fd = input_fd;
    aisubtitle->shared_fd = input_fd;

    dup_fd = dup(input_fd);
    mDupCount++;
    mTotalDupCount++;

    mAiSubtitleIndex[mCacheIndex].buf_index = mBuf_index;
    mAiSubtitleIndex[mCacheIndex].shared_fd = dup_fd;

    ALOGD_IF(check_D(), "dup_fd =%d, mBuf_index=%d",
        dup_fd, mBuf_index);

    if (!mNnDoing) {
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mBuf_fd_q.push(mCacheIndex);
        }
        mBuf_index++;
        mCacheIndex++;
        if (mCacheIndex == AISUBTITLE_MAX_CACHE_COUNT)
            mCacheIndex = 0;
        triggerEvent();
    } else {
        close(dup_fd);
        mCloseCount++;
        mTotalCloseCount++;
    }

    while (1) {
        ready_size = mBuf_fd_q.size();
        if (ready_size >= AISUBTITLE_MAX_CACHE_COUNT) {
            usleep(2*1000);
            ALOGE("too many buf need aisubtitle process, wait ready_size =%d, mNnDoing=%d",
            ready_size, mNnDoing);
        } else
            break;
    }

    return 0;
error:
    ALOGD_IF(check_D(), "set NN_INVALID");

bypass:
    return 0;
}

int32_t AiSubtitleProcessor::onBufferDisplayed(
        std::shared_ptr<DrmFramebuffer> & outfb __unused,
        int releaseFence) {

    if (releaseFence != -1)
        close(releaseFence);
    return 0;
}

int32_t AiSubtitleProcessor::teardown() {
    ATRACE_CALL();
    mExitThread = true;
    int shared_fd = -1;
    int cache_index;

    ALOGD("%s.\n", __FUNCTION__);
    triggerEvent();
    if (mInited && mThread) {
        pthread_join(mThread, NULL);
        mThread = 0;
    }

    while (mBuf_fd_q.size() > 0)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        cache_index = mBuf_fd_q.front();
        shared_fd = mAiSubtitleIndex[cache_index].shared_fd;
        if (shared_fd != -1) {
            close(shared_fd);
            mCloseCount++;
            mTotalCloseCount++;
        }
        mBuf_fd_q.pop();
        ALOGD("%s: close fd =%d\n", __FUNCTION__, shared_fd);
    }

    freeDmaBuffers();
    mBuf_Alloced = false;
    mInited = false;

    return 0;
}

void AiSubtitleProcessor::get_fixed_area() {
    if (mNnInputWidth < mFixedWidth || mNnInputHeight < mFixedHeight) {
        ALOGE("%s:parm err,%d %d %d %d.\n", __FUNCTION__, mNnInputWidth, mNnInputHeight, mFixedWidth, mFixedHeight);
        mFixedWidth = mNnInputWidth;
        mFixedHeight = mNnInputWidth / FIXED_AREA_NUM / 2;
    }

    mFixedArea[0].x1 = (mNnInputWidth - mFixedWidth) / 2;
    mFixedArea[0].y1 = mNnInputHeight - mFixedHeight;
    mFixedArea[0].x2 = (mNnInputWidth + mFixedWidth) / 2;
    mFixedArea[0].y2 = mNnInputHeight;

    mFixedArea[1].x1 = (mNnInputWidth - mFixedWidth) / 2;
    mFixedArea[1].y1 = mNnInputHeight - mFixedHeight * 2;
    mFixedArea[1].x2 = (mNnInputWidth + mFixedWidth) / 2;
    mFixedArea[1].y2 = mNnInputHeight - mFixedHeight;

    mFixedArea[2].x1 = (mNnInputWidth - mFixedWidth) / 2;
    mFixedArea[2].y1 = 0;
    mFixedArea[2].x2 = (mNnInputWidth + mFixedWidth) / 2;
    mFixedArea[2].y2 = mFixedHeight;

    mFixedArea[3].x1 = (mNnInputWidth - mFixedWidth) / 2;
    mFixedArea[3].y1 = mFixedHeight;
    mFixedArea[3].x2 = (mNnInputWidth + mFixedWidth) / 2;
    mFixedArea[3].y2 = mFixedHeight * 2;
}

int32_t AiSubtitleProcessor::intersection_area(const struct rect_t *rect_a, const struct rect_t *rect_b) {
    int ix1, ix2, iy1, iy2;

    ix1 = (rect_a->x1 > rect_b->x1) ? rect_a->x1 : rect_b->x1;
    iy1 = (rect_a->y1 > rect_b->y1) ? rect_a->y1 : rect_b->y1;
    ix2 = (rect_a->x2 < rect_b->x2) ? rect_a->x2 : rect_b->x2;
    iy2 = (rect_a->y2 < rect_b->y2) ? rect_a->y2 : rect_b->y2;

    ALOGD_IF(check_D(), "%s:after cal: (%d,%d) (%d,%d).\n", __FUNCTION__, ix1, iy1, ix2, iy2);

    if (ix1 < ix2 && iy1 < iy2) {
        return (ix2 - ix1) * (iy2 - iy1);
    }

    return 0;
}

int32_t AiSubtitleProcessor::cal_adaptive_subtitle_area(subtitle_out_t *nn_out) {
    int i, j, k;
    int min_x, min_y, max_x, max_y;
    struct rect_t rect_a;
    int index;

    if (nn_out->detNum == MAX_SUBTITLE_DETECT_NUM || nn_out->detNum == 0) {
        return ADAPTIVE_AREA1;
        ALOGD_IF(check_D(), "%s:too many detect num %d.\n", __FUNCTION__, nn_out->detNum);
    } else {
        for (i = 0; i < nn_out->detNum; i++) {
            min_x = nn_out->pos[i][0].x;
            min_y = nn_out->pos[i][0].y;
            max_x = nn_out->pos[i][0].x;
            max_y = nn_out->pos[i][0].y;

            for (k = 1; k < 4; k++) {
                if (nn_out->pos[i][k].x < min_x)
                    min_x = nn_out->pos[i][k].x;

                if (nn_out->pos[i][k].y < min_y)
                    min_y = nn_out->pos[i][k].y;

                if (nn_out->pos[i][k].x > max_x)
                    max_x = nn_out->pos[i][k].x;

                if (nn_out->pos[i][k].y > max_y)
                    max_y = nn_out->pos[i][k].y;
            }

            rect_a.x1 = min_x;
            rect_a.y1 = min_y;
            rect_a.x2 = max_x;
            rect_a.y2 = max_y;

            for (j = 0; j < FIXED_AREA_NUM; j++) {
                mAreaSum[j] += intersection_area(&rect_a, &mFixedArea[j]);
            }
        }
        index = 0;
        for (k = 0; k < 4; k++) {
            if (mAreaSum[index] > mAreaSum[k])
                index = k;
        }
    }

    ALOGD_IF(check_D(), "%s:%d %d %d %d %d.\n", __FUNCTION__, mAreaSum[0], mAreaSum[1], mAreaSum[2], mAreaSum[3], index);

    return index;
}

int32_t AiSubtitleProcessor::ai_subtitle_process(int cache_index) {
    int ret;
    struct timespec tm_0;
    struct timespec tm_1;
    struct timespec tm_2;
    struct timespec tm_3;
    uint64_t mTime_0;
    uint64_t mTime_1;
    uint64_t mTime_2;
    uint64_t mTime_3;
    uint64_t ge2d_time;
    uint64_t nn_time;
    uint64_t cal_time;
    int dump_index;
    subtitle_out_t *nn_out = NULL;
    int input_fd  = mAiSubtitleIndex[cache_index].shared_fd;
    int i;
    int bytesperline;

    struct uvm_hook_data hook_data;
    struct uvm_aisubtitle_info_t *uvm_info;
    struct uvm_aisubtitle_info *aisubtitle_info;

    uvm_info = (struct uvm_aisubtitle_info_t *)&hook_data;
    aisubtitle_info = &(uvm_info->aisubtitle_info);

    uvm_info->mode_type = PROCESS_AISUBTITLE;
    uvm_info->shared_fd = input_fd;

    aisubtitle_info->shared_fd = input_fd;
    aisubtitle_info->aisubtitle_fd = mAiSubtitle_Buf.fd;
    aisubtitle_info->nn_input_frame_width = mNnInputWidth;
    aisubtitle_info->nn_input_frame_height = mNnInputHeight;
    aisubtitle_info->get_info_type = AISUBTITLE_GET_DATA;
    bytesperline = mNnInputWidth * 3;

    ALOGD_IF(check_D(), "get RGB DATA.\n");
    clock_gettime(CLOCK_MONOTONIC, &tm_0);
    ret = ioctl(mUvmHandler, UVM_IOC_GET_INFO, &hook_data);
    if (ret < 0) {
        ALOGD_IF(check_D(),"UVM_IOC_GET_INFO fail =%d.\n", ret);
        return ret;
    }

    mNnDoing = true;
    clock_gettime(CLOCK_MONOTONIC, &tm_1);

    nn_out = (subtitle_out_t *)adaptive_subtitle_process((aai_iva_adaptive_subtitle_handle_t)mNn_qcontext, (unsigned char *)mAiSubtitle_Buf.fd_ptr, mNnInputWidth, mNnInputHeight, 16, bytesperline, &nn_out_temp);

    clock_gettime(CLOCK_MONOTONIC, &tm_2);

    if (nn_out == NULL) {
        ALOGE("nn_process_network: err: ret=%d.\n", ret);
        mNnDoing = false;
        return 0;
    } else {
        if (nn_out->detNum == 0) {
            ALOGD_IF(check_D(), "nn out normal, and no character string in the picture.\n");
        } else {
            ALOGD_IF(check_D(), "%s:detNum=%d.\n", __FUNCTION__, nn_out->detNum);
            for (i = 0; i < nn_out->detNum; i++) {
                ALOGD_IF(check_D(), "num:%d: (%d,%d),(%d,%d),(%d,%d),(%d,%d)\n", i, nn_out->pos[i][0].x, nn_out->pos[i][0].y, nn_out->pos[i][1].x, nn_out->pos[i][1].y, nn_out->pos[i][2].x, nn_out->pos[i][2].y, nn_out->pos[i][3].x, nn_out->pos[i][3].y);
            }
        }

        adaptive_area = cal_adaptive_subtitle_area(nn_out);
        clock_gettime(CLOCK_MONOTONIC, &tm_3);

        dump_index = PropGetInt("vendor.hwc.aisubtitle_dump", 0);
        if (dump_index) {
            if (mDumpIndex < dump_index) {
                dump_nn_info(mDumpIndex + 1);
                mDumpIndex++;
            } else {
                ALOGE("finish dump %d vframe.\n", dump_index);
                property_set("vendor.hwc.aisubtitle_dump", "0");
                mDumpIndex = 0;
            }
        }

        if (nn_out->detNum >= MAX_SUBTITLE_DETECT_NUM)
                nn_out->detNum = MAX_SUBTITLE_DETECT_NUM;

        mTime_0 = tm_0.tv_sec * 1000000LL + tm_0.tv_nsec / 1000;
        mTime_1 = tm_1.tv_sec * 1000000LL + tm_1.tv_nsec / 1000;
        mTime_2 = tm_2.tv_sec * 1000000LL + tm_2.tv_nsec / 1000;
        mTime_3 = tm_3.tv_sec * 1000000LL + tm_3.tv_nsec / 1000;
        ge2d_time = mTime_1 - mTime_0;
        nn_time = mTime_2 - mTime_1;
        cal_time = mTime_3 - mTime_2;
        ALOGD_IF(check_D(), "aipq_process ge2d %" PRId64", nn %" PRId64" cal_time %" PRId64", total %" PRId64" mNn_Index=%d\n",
            ge2d_time, nn_time, cal_time, ge2d_time + nn_time + cal_time, mNn_Index);

        mTime.total_time += nn_time;
        mTime.count++;
        mNnDoing = false;
    }

    aisubtitle_info->out_area = adaptive_area;
    ret = ioctl(mUvmHandler, UVM_IOC_SET_INFO, &hook_data);
    if (ret < 0) {
        ALOGD_IF(check_D(),"UVM_IOC_SET_INFO fail =%d.\n", ret);
        return ret;
    }

    memset(mAreaSum, 0, sizeof(mAreaSum));
    if ((mNn_Index % 3000) == 0) {
        if (mTime.count > 0) {
            mTime.avg_time = mTime.total_time / mTime.count;
        }
        ALOGD("AipqProcessor: time1: count=%" PRId64", max=%" PRId64", min=%" PRId64", avg=%" PRId64"",
            mTime.count,
            mTime.max_time,
            mTime.min_time,
            mTime.avg_time);
    }

    mNn_Index++;

    return ret;
}

void AiSubtitleProcessor::dump_nn_info(int num) {
    char dump_path[32];
    FILE * dump_file = NULL;

    ALOGD("%s: fd_ptr=%p, size=%d",
        __FUNCTION__,
        mAiSubtitle_Buf.fd_ptr,
        mAiSubtitle_Buf.size);

    snprintf(dump_path, sizeof(dump_path), "/data/aisubtitle_in_%d.rgb", num);
    dump_file = fopen(dump_path, "wb");
    if (dump_file != NULL) {
        fwrite(mAiSubtitle_Buf.fd_ptr, mAiSubtitle_Buf.size, 1, dump_file);
        fclose(dump_file);
    } else
        ALOGE("open %s fail.\n", dump_path);
}

int32_t AiSubtitleProcessor::waitEvent()
{
    int ret;

    pthread_mutex_lock(&m_waitMutex);
    while (mBuf_fd_q.size() == 0 && !mExitThread) {
        ret = pthread_cond_wait(&m_waitCond, &m_waitMutex);
        if (ret != 0) {
            pthread_mutex_unlock(&m_waitMutex);
            return -1;
        }
    }
    if (mBuf_fd_q.size() > 0)
        ret = 1;
    if (mExitThread) {
        ALOGD("ExitThread is true, wake up and exit.\n");
        ret = 2;
    }

    pthread_mutex_unlock(&m_waitMutex);
    return ret;
}

void AiSubtitleProcessor::triggerEvent(void) {
    pthread_mutex_lock(&m_waitMutex);
    pthread_cond_signal(&m_waitCond);
    pthread_mutex_unlock(&m_waitMutex);
};

#define ION_FLAG_EXTEND_MESON_HEAP (1 << 30)

int AiSubtitleProcessor::allocDmaBuffer() {
    int buffer_size = mNnInputWidth * mNnInputHeight * 3;
    uint32_t stride;
    int format = 17;
    int gralloc_fd = -1;
    void * cpu_ptr = NULL;
    uint64_t usage = GRALLOC1_PRODUCER_USAGE_CAMERA;
    GraphicBufferAllocator & allocService = GraphicBufferAllocator::get();

    ALOGD("mNnInputWidth=%d, mNnInputHeight=%d.\n", mNnInputWidth, mNnInputHeight);

    if (NO_ERROR != allocService.allocate(
        mNnInputWidth, mNnInputHeight * 2, format, 1, usage,
        &mAiSubtitle_Buf.buffer_handle, &stride, 0, "aisubtitle")) {
        ALOGE("alloc buffer failed");
    }

    if (mAiSubtitle_Buf.buffer_handle) {
        gralloc_fd = am_gralloc_get_buffer_fd((native_handle_t *)mAiSubtitle_Buf.buffer_handle);
        if (gralloc_fd < 0) {
            allocService.free(mAiSubtitle_Buf.buffer_handle);
            ALOGE("get fd fail");
            return -1;
        }

        cpu_ptr = (unsigned char *)mmap(NULL, buffer_size,
            PROT_READ | PROT_WRITE, MAP_SHARED, gralloc_fd, 0);

        if (MAP_FAILED == cpu_ptr) {
            ALOGE("mmap error!");
            freeDmaBuffers();
            return -1;
        } else {
            mAiSubtitle_Buf.fd_ptr = cpu_ptr;
        }
    } else {
        return -1;
    }

    mAiSubtitle_Buf.size = buffer_size;
    mAiSubtitle_Buf.fd = gralloc_fd;
    ALOGD("%s: fd=%d, fd_ptr=%p buffer_size=%d", __FUNCTION__, gralloc_fd, cpu_ptr, buffer_size);

    return 0;
};

int AiSubtitleProcessor::freeDmaBuffers() {
    GraphicBufferAllocator & allocService = GraphicBufferAllocator::get();

    if (mAiSubtitle_Buf.fd_ptr) {
        munmap(mAiSubtitle_Buf.fd_ptr, mAiSubtitle_Buf.size);
        mAiSubtitle_Buf.fd_ptr = NULL;
    }
    if (mAiSubtitle_Buf.fd != -1)
        mAiSubtitle_Buf.fd = -1;

    if (mAiSubtitle_Buf.buffer_handle) {
        allocService.free(mAiSubtitle_Buf.buffer_handle);
        mAiSubtitle_Buf.buffer_handle = NULL;
    }

    return 0;
}

