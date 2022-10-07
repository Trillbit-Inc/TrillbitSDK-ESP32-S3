#include <stdio.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "lvgl.h"

#include "esp_err.h"
#include "esp_log.h"

#include "ui.h"
#include "version.h"

#define LIC_ERROR_MESSAGE \
    "Trillbit SDK license not found or is invalid.\n\n" \
    "Recompile this demo application with\nvalid license in #0707e3 spiffs# partition.\n\n" \
    "Or acquire a license over USB with\nLicense provisioning application.\n\n" \
    "Detected Device ID:\n" \
    "#0707e3 %s#"

#define TITLE   "Trillbit SDK Demo"

static lv_obj_t *lbl_msg;
static lv_style_t style_btn_busy;
static lv_style_t style_start_btn;
static lv_obj_t* lbl_echo_status;
static lv_obj_t* lbl_status;
static lv_obj_t* btn_echo; 
static lv_obj_t* btn_start; 
static lv_obj_t* lbl_start_btn;
static int allow_echo = 1;
static echo_cb_t echo_cb;
static start_cb_t start_cb;
static int msg_count = 0;

#define ECHO_BTN_MIN_WIDTH 80
#define ECHO_BTN_MIN_HEIGHT 40

static void set_echo_button_ready(void);
static void set_echo_button_busy(void);
static void set_echo_button_disable(void);

static void echo_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        if (allow_echo)
        {
            if (echo_cb)
                if (echo_cb() >= 0)
                {
                    allow_echo = 0;
                    set_echo_button_busy();
                }
        }
    }
    else if(code == LV_EVENT_VALUE_CHANGED) {
    }
}

static void start_btn_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    int ret;
    if(code == LV_EVENT_CLICKED) {
        if (start_cb)
        {
            ret = start_cb();

            if (ret > 0) // sdk started
            {
                set_echo_button_ready();
                lv_label_set_text(lbl_start_btn, "Stop");
                lv_style_set_bg_color(&style_start_btn, lv_color_hex(0xFF0000));
                lv_label_set_text_static(lbl_status, "#ff0000 Listening...#");
                lv_label_set_text_static(lbl_msg, "");
                msg_count = 0;

            }
            else //sdk stopped
            {
                set_echo_button_disable();
                lv_label_set_text(lbl_start_btn, "Start");
                lv_style_set_bg_color(&style_start_btn, lv_color_hex(0x0000FF));
                lv_label_set_text_static(lbl_status, "#ff0000 SDK Stopped#");
            }
        }
    }
    else if(code == LV_EVENT_VALUE_CHANGED) {
    }
}

int ui_set_start_callback(start_cb_t cb)
{
    start_cb = cb;
    return 0;
}

int ui_set_echo_callback(echo_cb_t cb)
{
    echo_cb = cb;
    return 0;
}

static void set_echo_button_ready(void)
{
    lv_label_set_text(lbl_echo_status, "Echo");
    lv_style_set_bg_color(&style_btn_busy, lv_color_hex(0x0000FF));
    lv_obj_invalidate(btn_echo);
}

static void set_echo_button_busy(void)
{
    lv_style_set_bg_color(&style_btn_busy, lv_color_hex(0xFF0000));
    lv_label_set_text_static(lbl_echo_status, LV_SYMBOL_VOLUME_MAX);
    lv_obj_invalidate(btn_echo);
}

static void set_echo_button_disable(void)
{
    lv_style_set_bg_color(&style_btn_busy, lv_color_hex(0x808080));
    lv_label_set_text_static(lbl_echo_status, "Echo");
    lv_obj_invalidate(btn_echo);
    allow_echo = 0;
}

int ui_en_echo(int en)
{
    allow_echo = en ? 1: 0;

    if (allow_echo)
    {
        set_echo_button_ready();
    }

    return 0;
}

int ui_start(void)
{
    lv_style_init(&style_btn_busy);
    lv_style_set_bg_color(&style_btn_busy, lv_color_hex(0x0000FF));
    lv_style_set_min_width(&style_btn_busy, ECHO_BTN_MIN_WIDTH);
    lv_style_set_min_height(&style_btn_busy, ECHO_BTN_MIN_HEIGHT);

    lv_style_init(&style_start_btn);
    lv_style_set_bg_color(&style_start_btn, lv_color_hex(0xFF0000));
    lv_style_set_min_width(&style_start_btn, ECHO_BTN_MIN_WIDTH);
    lv_style_set_min_height(&style_start_btn, ECHO_BTN_MIN_HEIGHT);
    
    lv_obj_t *lab_title = lv_label_create(lv_scr_act());
    lv_label_set_text_static(lab_title, TITLE);
    lv_obj_set_style_text_font(lab_title, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_align(lab_title, LV_ALIGN_TOP_MID, 0, 20);


    lv_obj_t *lab_ver = lv_label_create(lv_scr_act());
    lv_label_set_recolor(lab_ver, true);
    lv_label_set_text_fmt(lab_ver, "#969695 Version# #969695 %s#", APP_VERSION);
    lv_obj_set_style_text_font(lab_ver, &lv_font_montserrat_12, LV_STATE_DEFAULT);
    lv_obj_align_to(lab_ver, lab_title, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    lbl_status = lv_label_create(lv_scr_act());
    lv_label_set_recolor(lbl_status, true);
    lv_label_set_text_static(lbl_status, "#ff0000 Listening...#");
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_18, LV_STATE_DEFAULT);
    lv_obj_align_to(lbl_status, lab_ver, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    lbl_msg = lv_label_create(lv_scr_act());
    lv_label_set_text_static(lbl_msg, "");
    lv_obj_set_style_text_font(lbl_msg, &lv_font_montserrat_18, LV_STATE_DEFAULT);
    lv_obj_align_to(lbl_msg, lbl_status, LV_ALIGN_OUT_BOTTOM_LEFT, -50, 10);
    lv_label_set_recolor(lbl_msg, true);

    btn_echo = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(btn_echo, echo_event_handler, LV_EVENT_ALL, btn_echo);
    lv_obj_align(btn_echo, LV_ALIGN_BOTTOM_MID, -50, -10);
    lv_obj_add_style(btn_echo, &style_btn_busy, LV_STATE_DEFAULT);
    lv_obj_add_style(btn_echo, &style_btn_busy, LV_STATE_FOCUSED);
    
    lbl_echo_status = lv_label_create(btn_echo);
    lv_label_set_text(lbl_echo_status, "Echo");
    lv_obj_set_style_text_font(lbl_echo_status, &lv_font_montserrat_20, LV_STATE_DEFAULT);
    lv_obj_center(lbl_echo_status);

    btn_start = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(btn_start, start_btn_event_handler, LV_EVENT_ALL, btn_start);
    lv_obj_align(btn_start, LV_ALIGN_BOTTOM_MID, 50, -10);
    lv_obj_add_style(btn_start, &style_start_btn, LV_STATE_DEFAULT);
    lv_obj_add_style(btn_start, &style_start_btn, LV_STATE_FOCUSED);
    

    lbl_start_btn = lv_label_create(btn_start);
    lv_label_set_text(lbl_start_btn, "Stop");
    lv_obj_set_style_text_font(lbl_start_btn, &lv_font_montserrat_20, LV_STATE_DEFAULT);
    lv_obj_center(lbl_start_btn);

    return 0;
}

int ui_set_rx_msg(const char* txt)
{
    lv_label_set_text_fmt(lbl_msg, "#0000ff %d) %s#", ++msg_count, txt);

    return 0;
}

int ui_lic_error(const char* dev_id)
{
    lv_obj_t* page = lv_obj_create(NULL);
    lv_scr_load(page);

    lv_obj_t *lab_title = lv_label_create(page);
    lv_label_set_text_static(lab_title, TITLE);
    lv_obj_set_style_text_font(lab_title, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_align_to(lab_title, page, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *lab_ver = lv_label_create(page);
    lv_label_set_recolor(lab_ver, true);
    lv_label_set_text_fmt(lab_ver, "#969695 Version# #969695 %s#", APP_VERSION);
    lv_obj_set_style_text_font(lab_ver, &lv_font_montserrat_12, LV_STATE_DEFAULT);
    lv_obj_align_to(lab_ver, lab_title, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    lv_obj_t *lab_msg = lv_label_create(page);
    lv_label_set_recolor(lab_msg, true);
    lv_label_set_text_fmt(lab_msg, LIC_ERROR_MESSAGE, dev_id);
    lv_obj_set_style_text_font(lab_msg, &lv_font_montserrat_14, LV_STATE_DEFAULT);
    lv_obj_align_to(lab_msg, page, LV_ALIGN_TOP_LEFT, 10, 60);

    return 0;
}
