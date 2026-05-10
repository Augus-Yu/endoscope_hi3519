/**
 * @file endoscope_main.c
 * @brief 主界面实现 - 根据设计图重构
 */

#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include "endoscope_main.h"
#include "endoscope_ui.h"
#include "screen_manager.h"
#include "lang_manager.h"
#include "font_manager.h"
#include "endoscope_dialogs.h"
#include "ui_helpers.h"
#include "display_config.h"
#include "hi3519_port/mpp_record.h"
#include "hi3519_port/mpp_video.h"
#include "sample_comm.h"
#include <sys/stat.h>
#include <sys/types.h>

/*********************
 *      DEFINES
 *********************/
#define MAIN_COLOR_BG       UI_COLOR_BG
#define MAIN_COLOR_SURFACE  UI_COLOR_SURFACE
#define MAIN_COLOR_TEXT     UI_COLOR_TEXT
#define MAIN_COLOR_ACCENT   UI_COLOR_ACCENT
#define MAIN_COLOR_RECORD   UI_COLOR_RECORDING
#define LEFT_PANEL_WIDTH    480
#define RIGHT_PANEL_WIDTH   384
#define TOP_BAR_HEIGHT      80
#define BTN_SIZE            168

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_obj_t * main_screen = NULL;
static lv_obj_t * video_area = NULL;
static lv_obj_t * record_btn = NULL;
static lv_obj_t * record_status_label = NULL;
static lv_obj_t * time_label = NULL;
static lv_timer_t * record_timer = NULL;
static lv_obj_t * patient_info_label = NULL;
static lv_obj_t * date_time_label = NULL;
static lv_timer_t * date_timer = NULL;
static lv_obj_t * led_slider = NULL;
static int g_frozen = 0;

static pthread_mutex_t g_status_mutex = PTHREAD_MUTEX_INITIALIZER;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void create_top_bar(lv_obj_t * parent);
static void create_left_panel(lv_obj_t * parent);
static void create_video_area(lv_obj_t * parent);
static void create_right_panel(lv_obj_t * parent);
static void btn_event_cb(lv_event_t * e);
static void record_timer_cb(lv_timer_t * timer);
static void date_time_timer_cb(lv_timer_t * timer);
static void choose_save_base(void);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void endoscope_main_init(void)
{
    /* 如果已存在，先销毁 */
    if(main_screen) {
        lv_obj_del(main_screen);
        main_screen = NULL;
        video_area = NULL;
        record_btn = NULL;
        time_label = NULL;
        patient_info_label = NULL;
        date_time_label = NULL;
    }

    /* 删除日期定时器 */
    if(date_timer) {
        lv_timer_delete(date_timer);
        date_timer = NULL;
    }

    main_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(main_screen, MAIN_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(main_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(main_screen, LV_SCROLLBAR_MODE_OFF);

    create_left_panel(main_screen);
    create_right_panel(main_screen);
    create_top_bar(main_screen);
    create_video_area(main_screen);
}

/* 外部声明：显示驱动中的视频透明区域控制标志 */
extern volatile int g_video_trans_enable;

void endoscope_main_show(void)
{
    g_video_trans_enable = 1;
    lv_scr_load(main_screen);
}

void endoscope_main_hide(void)
{
    g_video_trans_enable = 0;
}

void endoscope_main_update(void)
{
    /* TODO: Implement screen update logic if needed */
}

lv_obj_t * endoscope_main_get_screen(void)
{
    return main_screen;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void create_top_bar(lv_obj_t * parent)
{
    /* 顶部标题栏 */
    lv_obj_t * bar = lv_obj_create(parent);
    lv_obj_set_size(bar, lv_pct(100), TOP_BAR_HEIGHT);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, MAIN_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(bar, LV_SCROLLBAR_MODE_OFF);
    
    /* 返回按钮 - 48x48 */
    lv_obj_t * btn_back = lv_btn_create(bar);
    lv_obj_set_size(btn_back, 48, 48);
    lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 24, 0);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x333333), 0);
    lv_obj_t * lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_24, 0);
    lv_obj_center(lbl_back);
    lv_obj_add_event_cb(btn_back, btn_event_cb, LV_EVENT_CLICKED, (void *)10);

    /* 标题 */
    lv_obj_t * title = lv_label_create(bar);
    lv_label_set_text(title, _TR("MAIN_TITLE"));
    UI_SET_FONT(title);
    lv_obj_set_style_text_color(title, MAIN_COLOR_TEXT, 0);
    lv_obj_center(title);

    /* 关闭按钮 - 48x48 */
    lv_obj_t * btn_close = lv_btn_create(bar);
    lv_obj_set_size(btn_close, 48, 48);
    lv_obj_align(btn_close, LV_ALIGN_RIGHT_MID, -24, 0);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(0x333333), 0);
    lv_obj_t * lbl_close = lv_label_create(btn_close);
    lv_label_set_text(lbl_close, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(lbl_close, &lv_font_montserrat_24, 0);
    lv_obj_center(lbl_close);
}

static void create_left_panel(lv_obj_t * parent)
{
    /* 左侧面板 */
    lv_obj_t * panel = lv_obj_create(parent);
    lv_obj_set_size(panel, LEFT_PANEL_WIDTH, DISPLAY_HEIGHT - TOP_BAR_HEIGHT);
    lv_obj_align(panel, LV_ALIGN_TOP_LEFT, 0, TOP_BAR_HEIGHT);
    lv_obj_set_style_bg_color(panel, MAIN_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 24, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    int y_pos = 24;  /* 10 * 2.4 */
    
    /* 日期时间 */
    date_time_label = lv_label_create(panel);
    ui_update_label_datetime(date_time_label, _TR("MAIN_DATE_PREFIX"));
    UI_SET_FONT(date_time_label);
    lv_obj_set_style_text_color(date_time_label, MAIN_COLOR_TEXT, 0);
    lv_obj_align(date_time_label, LV_ALIGN_TOP_LEFT, 0, y_pos);

    /* 创建定时器每秒更新时间 */
    date_timer = lv_timer_create(date_time_timer_cb, 500, NULL);
    y_pos += 72;  /* 30 * 2.4 */
    
    /* 分隔线 */
    y_pos += 24;  /* 10 * 2.4 */
    ui_create_separator_line(panel, LEFT_PANEL_WIDTH-48, y_pos);
    y_pos += 48;  /* 20 * 2.4 */
    
    /* 患者信息标题 */
    lv_obj_t * title1 = lv_label_create(panel);
    lv_label_set_text(title1, _TR("MAIN_PATIENT_INFO"));
    UI_SET_FONT(title1);
    lv_obj_set_style_text_color(title1, MAIN_COLOR_ACCENT, 0);
    lv_obj_align(title1, LV_ALIGN_TOP_LEFT, 0, y_pos);
    y_pos += 60;  /* 25 * 2.4 */
    
    /* 患者信息内容 */
    patient_info_label = lv_label_create(panel);
    endoscope_main_update_patient_info();
    UI_SET_FONT(patient_info_label);
    lv_obj_set_style_text_color(patient_info_label, MAIN_COLOR_TEXT, 0);
    lv_obj_align(patient_info_label, LV_ALIGN_TOP_LEFT, 0, y_pos);
    y_pos += 216;  /* 90 * 2.4 */
    
    /* 分隔线 */
    y_pos += 24;  /* 10 * 2.4 */
    ui_create_separator_line(panel, LEFT_PANEL_WIDTH-48, y_pos);
    y_pos += 48;  /* 20 * 2.4 */
    
    /* 镜种 */
    lv_obj_t * title2 = lv_label_create(panel);
    lv_label_set_text(title2, _TR("MAIN_ENDOSCOPE_TYPE"));
    UI_SET_FONT(title2);
    lv_obj_set_style_text_color(title2, MAIN_COLOR_ACCENT, 0);
    lv_obj_align(title2, LV_ALIGN_TOP_LEFT, 0, y_pos);
    y_pos += 60;  /* 25 * 2.4 */

    lv_obj_t * info2 = lv_label_create(panel);
    lv_label_set_text_fmt(info2, "%s\n%s", _TR("MAIN_ENDOSCOPE_ID"), _TR("MAIN_WORK_MODE"));
    UI_SET_FONT(info2);
    lv_obj_set_style_text_color(info2, MAIN_COLOR_TEXT, 0);
    lv_obj_align(info2, LV_ALIGN_TOP_LEFT, 0, y_pos);
    y_pos += 144;  /* 60 * 2.4 */
    
    /* 分隔线 */
    y_pos += 24;  /* 10 * 2.4 */
    ui_create_separator_line(panel, LEFT_PANEL_WIDTH-48, y_pos);
    y_pos += 48;  /* 20 * 2.4 */
    
    /* U盘信息 */
    lv_obj_t * title3 = lv_label_create(panel);
    lv_label_set_text(title3, _TR("MAIN_USB_INFO"));
    UI_SET_FONT(title3);
    lv_obj_set_style_text_color(title3, MAIN_COLOR_ACCENT, 0);
    lv_obj_align(title3, LV_ALIGN_TOP_LEFT, 0, y_pos);
    y_pos += 60;  /* 25 * 2.4 */

    record_status_label = lv_label_create(panel);
    lv_label_set_text(record_status_label, _TR("MAIN_RECORDING"));
    UI_SET_FONT(record_status_label);
    lv_obj_set_style_text_color(record_status_label, MAIN_COLOR_RECORD, 0);
    lv_obj_align(record_status_label, LV_ALIGN_TOP_LEFT, 0, y_pos);
    lv_obj_add_flag(record_status_label, LV_OBJ_FLAG_HIDDEN);
    
    /* 录制时间 */
    y_pos += 72;  /* 30 * 2.4 */
    time_label = lv_label_create(panel);
    lv_label_set_text(time_label, "00:00:00");
    lv_obj_set_style_text_color(time_label, MAIN_COLOR_RECORD, 0);
    lv_obj_align(time_label, LV_ALIGN_TOP_LEFT, 0, y_pos);
    lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
}

static void create_video_area(lv_obj_t * parent)
{
    /* 创建视频背景区域 - 深色背景，中间视频区域透明 */
    lv_obj_t * video_bg = lv_obj_create(parent);
    lv_obj_set_size(video_bg, 1036, 960);
    lv_obj_align(video_bg, LV_ALIGN_TOP_LEFT, LEFT_PANEL_WIDTH, 120);
    lv_obj_set_style_bg_color(video_bg, MAIN_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(video_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(video_bg, 0, 0);
    lv_obj_set_style_radius(video_bg, 0, 0);
    lv_obj_clear_flag(video_bg, LV_OBJ_FLAG_SCROLLABLE);

    /* 视频显示区域 - 800x800，使用Alpha透明显示下方的VO视频 */
    video_area = lv_obj_create(video_bg);
    lv_obj_set_size(video_area, 800, 800);
    lv_obj_center(video_area);
    /* 透明背景 - Alpha=0，显示下方的VO视频 */
    lv_obj_set_style_bg_color(video_area, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(video_area, LV_OPA_TRANSP, 0);
    /* 隐藏边框 */
    lv_obj_set_style_border_width(video_area, 0, 0);
    lv_obj_clear_flag(video_area, LV_OBJ_FLAG_SCROLLABLE);

    printf("[UI] Video area created at center size 800x800 with alpha transparency\n");
}

static void create_right_panel(lv_obj_t * parent)
{
    /* 右侧面板 */
    lv_obj_t * panel = lv_obj_create(parent);
    lv_obj_set_size(panel, RIGHT_PANEL_WIDTH, DISPLAY_HEIGHT - TOP_BAR_HEIGHT);
    lv_obj_align(panel, LV_ALIGN_TOP_RIGHT, -20, TOP_BAR_HEIGHT);
    lv_obj_set_style_bg_color(panel, MAIN_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_border_side(panel, LV_BORDER_SIDE_NONE, 0);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_set_style_pad_all(panel, 24, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    /* 按钮网格布局 */
    const char * btn_keys[8] = {
        "BTN_PATIENT", "BTN_SETTINGS",
        "BTN_BALANCE", "BTN_FREEZE",
        "BTN_CAPTURE", "BTN_RECORD",
        "BTN_ZOOM", "BTN_PLAYBACK"
    };
    const char * btn_icons[8] = {
        LV_SYMBOL_USB, LV_SYMBOL_SETTINGS,
        LV_SYMBOL_REFRESH, LV_SYMBOL_PAUSE,
        LV_SYMBOL_SAVE, LV_SYMBOL_VIDEO,
        LV_SYMBOL_PLUS, LV_SYMBOL_PLAY
    };
    
    int btn_ids[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    
    for(int i = 0; i < 8; i++) {
        lv_obj_t * btn = lv_btn_create(panel);
        lv_obj_set_size(btn, BTN_SIZE, BTN_SIZE);
        
        int col = i % 2;
        int row = i / 2;
        int x = 8 + col * (BTN_SIZE + 8);  /* 水平间距：紧凑 */
        int y = 10 + row * (BTN_SIZE + 30);  /* 垂直间距：紧凑，为LED调节留空间 */
        
        lv_obj_set_pos(btn, x, y);
        lv_obj_set_style_bg_color(btn, MAIN_COLOR_BG, 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)btn_ids[i]);
        
        if(i == 5) {
            record_btn = btn;
        }
        
        /* 图标 - 使用更大的字体 */
        lv_obj_t * icon = lv_label_create(btn);
        lv_label_set_text(icon, btn_icons[i]);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_48, 0);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 20);  /* 稍微上移以适配更大的图标 */
        
        /* 文字 */
        lv_obj_t * lbl = lv_label_create(btn);
        lv_label_set_text(lbl, _TR(btn_keys[i]));
        UI_SET_FONT(lbl);
        lv_obj_set_style_text_color(lbl, MAIN_COLOR_TEXT, 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(lbl, BTN_SIZE - 20);
        lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -12);  /* -5 * 2.4 */
    }
    
    /* LED亮度调节 - 放在按钮下方 */
    lv_obj_t * led_label = lv_label_create(panel);
    lv_label_set_text(led_label, _TR("MAIN_LED_BRIGHTNESS"));
    UI_SET_FONT(led_label);
    lv_obj_set_style_text_color(led_label, MAIN_COLOR_TEXT, 0);
    lv_obj_set_pos(led_label, 8, 10 + 4 * (BTN_SIZE + 30) + 10);  /* 放在第4行按钮下方 */

    /* 滑动条 */
    led_slider = lv_slider_create(panel);
    lv_obj_set_size(led_slider, 340, 24);
    lv_obj_set_pos(led_slider, 15, 10 + 4 * (BTN_SIZE + 30) + 45);  /* 标签下方 */
    lv_obj_set_style_bg_color(led_slider, MAIN_COLOR_BG, 0);
    lv_obj_set_style_bg_color(led_slider, MAIN_COLOR_ACCENT, LV_PART_INDICATOR);
    lv_slider_set_range(led_slider, 0, 100);
    lv_slider_set_value(led_slider, 50, LV_ANIM_OFF);
}

static void choose_save_base(void)
{
    const char *usb_paths[] = {
        "/mnt/sd", "/mnt/usb", "/mnt/sda1",
        "/mnt/sda", "/media/usb", "/media/sda1"
    };
    for (size_t i = 0; i < sizeof(usb_paths) / sizeof(usb_paths[0]); i++) {
        struct stat st;
        if (stat(usb_paths[i], &st) == 0 && S_ISDIR(st.st_mode)) {
            record_set_save_base(usb_paths[i]);
            return;
        }
    }
    record_set_save_base("./endoscope");
}

static void btn_event_cb(lv_event_t * e)
{
    int btn_id = (int)(intptr_t)lv_event_get_user_data(e);
    endoscope_status_t * status = endoscope_get_status();
    
    switch(btn_id) {
    case 1: /* 病人信息录入 */
        endoscope_patient_dialog_show();
        break;
    case 2: /* 设置 */
        screen_manager_navigate_to(ENDOSCOPE_SCREEN_SETTINGS);
        break;
    case 3: /* 白平衡 */
        endoscope_show_dialog(_TR("DLG_TITLE_BALANCE"), _TR("DLG_MSG_CALIBRATING"), _TR("DLG_BTN_OK"));
        break;
    case 4: /* 冻结/解冻 (停 VI + 停 VO 显示) */
        {
            video_context_t *vc = video_get_context();
            lv_obj_t *btn = lv_event_get_target_obj(e);
            lv_obj_t *icon = lv_obj_get_child(btn, 0);
            if (!g_frozen) {
                HI_MPI_VI_DisableChn(vc->vi_pipe, vc->vi_chn);
                HI_MPI_VO_PauseChn(vc->vo_dev, vc->vo_chn);
                g_frozen = 1;
                lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT, 0);
                if (icon) lv_label_set_text(icon, LV_SYMBOL_PLAY);
            } else {
                HI_MPI_VI_EnableChn(vc->vi_pipe, vc->vi_chn);
                HI_MPI_VO_ResumeChn(vc->vo_dev, vc->vo_chn);
                g_frozen = 0;
                lv_obj_set_style_bg_color(btn, MAIN_COLOR_BG, 0);
                if (icon) lv_label_set_text(icon, LV_SYMBOL_PAUSE);
            }
        }
        break;
    case 5: { /* 拍照 */
        int ok = 0;
        if (g_frozen) {
            video_context_t *vc = video_get_context();
            VENC_CHN snap_chn = 1;
            VIDEO_FRAME_INFO_S vo_frame;
            memset(&vo_frame, 0, sizeof(vo_frame));
            if (HI_MPI_VO_GetChnFrame(vc->vo_dev, vc->vo_chn, &vo_frame, 500) == HI_SUCCESS) {
                SIZE_S stSize = {400, 400};
                if (SAMPLE_COMM_VENC_SnapStart(snap_chn, &stSize, HI_FALSE) == HI_SUCCESS) {
                    VENC_RECV_PIC_PARAM_S recv = { .s32RecvPicNum = 1 };
                    HI_MPI_VENC_StartRecvFrame(snap_chn, &recv);
                    HI_MPI_VENC_SendFrame(snap_chn, &vo_frame, 0);
                    VENC_CHN_STATUS_S stStat;
                    for (int w = 0; w < 50; w++) {
                        HI_MPI_VENC_QueryStatus(snap_chn, &stStat);
                        if (stStat.u32CurPacks > 0) break;
                        usleep(50000);
                    }
                    if (stStat.u32CurPacks > 0) {
                        char fname[128];
                        generate_filename(fname, sizeof(fname), ".jpg");
                        char fullpath[512];
                        snprintf(fullpath, sizeof(fullpath), "./endoscope/snapshot/%s", fname);
                        mkdir("./endoscope/snapshot", 0777);
                        FILE *fp = fopen(fullpath, "wb");
                        if (fp) {
                            VENC_STREAM_S stStream;
                            stStream.u32PackCount = stStat.u32CurPacks;
                            stStream.pstPack = malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                            if (HI_MPI_VENC_GetStream(snap_chn, &stStream, HI_TRUE) == HI_SUCCESS) {
                                for (HI_U32 i = 0; i < stStream.u32PackCount; i++)
                                    fwrite(stStream.pstPack[i].pu8Addr + stStream.pstPack[i].u32Offset,
                                           stStream.pstPack[i].u32Len - stStream.pstPack[i].u32Offset, 1, fp);
                                fclose(fp);
                                HI_MPI_VENC_ReleaseStream(snap_chn, &stStream);
                                ok = 1;
                            }
                            free(stStream.pstPack);
                        }
                    }
                    HI_MPI_VENC_StopRecvFrame(snap_chn);
                    SAMPLE_COMM_VENC_SnapStop(snap_chn);
                }
                HI_MPI_VO_ReleaseChnFrame(vc->vo_dev, vc->vo_chn, &vo_frame);
            }
        } else {
            choose_save_base();
            ok = (snapshot_save(NULL) == 0);
        }
        if (ok)
            endoscope_show_dialog(_TR("DLG_TITLE_CAPTURE"), _TR("DLG_MSG_PHOTO_SAVED"), _TR("DLG_BTN_OK"));
        else
            endoscope_show_dialog(_TR("DLG_TITLE_NOTICE"), "拍照失败", _TR("DLG_BTN_OK"));
        break;
    }
    case 6: /* 录像 */
        pthread_mutex_lock(&g_status_mutex);
        status->is_recording = !status->is_recording;
        pthread_mutex_unlock(&g_status_mutex);
        if(status->is_recording) {
            choose_save_base();
            if(record_start(NULL) != 0) {
                pthread_mutex_lock(&g_status_mutex);
                status->is_recording = 0;
                pthread_mutex_unlock(&g_status_mutex);
                endoscope_show_dialog(_TR("DLG_TITLE_NOTICE"), "录像启动失败", _TR("DLG_BTN_OK"));
                break;
            }
            lv_obj_set_style_bg_color(record_btn, MAIN_COLOR_RECORD, 0);
            lv_obj_clear_flag(record_status_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(time_label, LV_OBJ_FLAG_HIDDEN);
            if(record_timer) {
                lv_timer_delete(record_timer);
                record_timer = NULL;
            }
            record_timer = lv_timer_create(record_timer_cb, 1000, NULL);
        } else {
            record_stop();
            lv_obj_set_style_bg_color(record_btn, MAIN_COLOR_BG, 0);
            lv_obj_add_flag(record_status_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
            if(record_timer) {
                lv_timer_delete(record_timer);
                record_timer = NULL;
            }
            pthread_mutex_lock(&g_status_mutex);
            status->recording_time = 0;
            pthread_mutex_unlock(&g_status_mutex);
        }
        break;
    case 7: /* 电子放大 */
        endoscope_show_dialog(_TR("DLG_TITLE_ZOOM"), _TR("DLG_MSG_ZOOM_FUNC"), _TR("DLG_BTN_OK"));
        break;
    case 8: /* 回放 */
        screen_manager_navigate_to(ENDOSCOPE_SCREEN_PLAYBACK);
        break;
    case 10: /* 返回 */
        endoscope_show_dialog(_TR("DLG_TITLE_NOTICE"), _TR("DLG_MSG_GO_BACK"), _TR("DLG_BTN_OK"));
        break;
    }
}

static void record_timer_cb(lv_timer_t * timer)
{
    (void)timer;
    endoscope_status_t * status = endoscope_get_status();
    pthread_mutex_lock(&g_status_mutex);
    status->recording_time++;
    uint32_t recording_time = status->recording_time;
    pthread_mutex_unlock(&g_status_mutex);
    
    uint32_t hours = recording_time / 3600;
    uint32_t mins = (recording_time % 3600) / 60;
    uint32_t secs = recording_time % 60;
    
    lv_label_set_text_fmt(time_label, "%02d:%02d:%02d", hours, mins, secs);
}

static void date_time_timer_cb(lv_timer_t * timer)
{
    (void)timer;

    if(date_time_label == NULL) return;

    ui_update_label_datetime(date_time_label, _TR("MAIN_DATE_PREFIX"));
}

void endoscope_main_update_patient_info(void)
{
    if(!patient_info_label) return;

    patient_info_t * patient = endoscope_get_patient_info();

    if(patient->has_data) {
        lv_label_set_text_fmt(patient_info_label, "%s %s\n%s %s\n%s %s\n%s %s",
            _TR("MAIN_PATIENT_ID"), patient->id,
            _TR("MAIN_PATIENT_NAME"), patient->name,
            _TR("MAIN_PATIENT_GENDER"), patient->gender,
            _TR("MAIN_PATIENT_AGE"), patient->age);
    } else {
        lv_label_set_text_fmt(patient_info_label, "%s\n%s\n%s\n%s",
            _TR("MAIN_PATIENT_ID"),
            _TR("MAIN_PATIENT_NAME"),
            _TR("MAIN_PATIENT_GENDER"),
            _TR("MAIN_PATIENT_AGE"));
    }
}
