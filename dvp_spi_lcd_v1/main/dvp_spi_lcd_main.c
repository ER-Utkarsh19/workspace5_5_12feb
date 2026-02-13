/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_cam_ctlr.h"
#include "esp_cam_ctlr_dvp.h"
#include "example_config.h"
#include "example_sensor_init.h"

/* ================= Configuration ================= */

#define CAM_W   CONFIG_EXAMPLE_CAM_HRES
#define CAM_H   CONFIG_EXAMPLE_CAM_VRES

#define ML_W    96
#define ML_H    96

/* ================= Globals ================= */

static const char *TAG = "cam_y_extract";

static volatile uint32_t g_frame_counter = 0;

static uint8_t *g_y_buf   = NULL;   // Full-res grayscale
static uint8_t *ml_y_buf  = NULL;   // 96x96 grayscale
static int8_t   ml_input[ML_W * ML_H];  //96*96

/* ================= Camera context ================= */

typedef struct {
    esp_cam_ctlr_trans_t cam_trans;
} example_cam_context_t;

/* ================= Stats ================= */

typedef struct {
    uint8_t  min;
    uint8_t  max;
    uint32_t sum;
    uint8_t  mean;
} y_stats_t;

/* ================= Forward declarations ================= */

static bool s_camera_get_new_vb(esp_cam_ctlr_handle_t handle,
                                esp_cam_ctlr_trans_t *trans,
                                void *user_data);

static bool s_camera_get_finished_trans(esp_cam_ctlr_handle_t handle,
                                        esp_cam_ctlr_trans_t *trans,
                                        void *user_data);

static void extract_y_from_yuv422(uint8_t *yuv,
                                  uint8_t *y,
                                  int width,
                                  int height);

static void compute_y_stats(const uint8_t *y,
                            int size,
                            y_stats_t *out);

static void resize_y_nn(const uint8_t *src,
                        int src_w, int src_h,
                        uint8_t *dst,
                        int dst_w, int dst_h);

/* ================= Application entry ================= */

void app_main(void)
{
    esp_err_t ret;

    /* ---------- Camera controller ---------- */

    esp_cam_ctlr_handle_t cam_handle = NULL;

    esp_cam_ctlr_dvp_pin_config_t pin_cfg = {
        .data_width = EXAMPLE_DVP_CAM_DATA_WIDTH,
        .data_io = {
            EXAMPLE_DVP_CAM_D0_IO,
            EXAMPLE_DVP_CAM_D1_IO,
            EXAMPLE_DVP_CAM_D2_IO,
            EXAMPLE_DVP_CAM_D3_IO,
            EXAMPLE_DVP_CAM_D4_IO,
            EXAMPLE_DVP_CAM_D5_IO,
            EXAMPLE_DVP_CAM_D6_IO,
            EXAMPLE_DVP_CAM_D7_IO,
        },
        .vsync_io = EXAMPLE_DVP_CAM_VSYNC_IO,
        .de_io    = EXAMPLE_DVP_CAM_DE_IO,
        .pclk_io  = EXAMPLE_DVP_CAM_PCLK_IO,
        .xclk_io  = EXAMPLE_DVP_CAM_XCLK_IO,
    };

    esp_cam_ctlr_dvp_config_t dvp_config = {
        .ctlr_id = 0,
        .clk_src = CAM_CLK_SRC_DEFAULT,
        .h_res   = CAM_W,
        .v_res   = CAM_H,
        .input_data_color_type = CAM_CTLR_COLOR_YUV422,
        .dma_burst_size = 64,
        .pin = &pin_cfg,
        .bk_buffer_dis = 1,
        .xclk_freq = EXAMPLE_DVP_CAM_XCLK_FREQ_HZ,
    };

    ret = esp_cam_new_dvp_ctlr(&dvp_config, &cam_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera controller init failed (%d)", ret);
        return;
    }

    /* ---------- Camera DMA buffer ---------- */

    size_t cam_buf_size = CAM_W * CAM_H * 2;   // YUV422 = 2 bytes/pixel

    void *cam_buffer = esp_cam_ctlr_alloc_buffer(
        cam_handle,
        cam_buf_size,
        EXAMPLE_DVP_CAM_BUF_ALLOC_CAPS
    );

    if (!cam_buffer) {
        ESP_LOGE(TAG, "Camera buffer alloc failed");
        return;
    }

    /* ---------- Grayscale buffers ---------- */

    g_y_buf = heap_caps_malloc(CAM_W * CAM_H,
                               MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!g_y_buf) {
        ESP_LOGE(TAG, "Y buffer alloc failed");
        return;
    }

    ml_y_buf = heap_caps_malloc(ML_W * ML_H,
                                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!ml_y_buf) {
        ESP_LOGE(TAG, "ML Y buffer alloc failed");
        return;
    }

    ESP_LOGI(TAG, "Buffers allocated: Y=%d bytes, ML=%d bytes",
             CAM_W * CAM_H, ML_W * ML_H);

    /* ---------- Sensor init ---------- */

    example_sensor_config_t cam_sensor_config = {
        .i2c_port_num = I2C_NUM_0,
        .i2c_sda_io_num = EXAMPLE_DVP_CAM_SCCB_SDA_IO,
        .i2c_scl_io_num = EXAMPLE_DVP_CAM_SCCB_SCL_IO,
        .port = ESP_CAM_SENSOR_DVP,
        .format_name = EXAMPLE_CAM_FORMAT,
    };

    example_sensor_handle_t sensor_handle = { 0 };
    example_sensor_init(&cam_sensor_config, &sensor_handle);

    /* ---------- Callbacks ---------- */

    example_cam_context_t cam_ctx = {
        .cam_trans = {
            .buffer = cam_buffer,
            .buflen = cam_buf_size,
        }
    };

    esp_cam_ctlr_evt_cbs_t cbs = {
        .on_get_new_trans   = s_camera_get_new_vb,
        .on_trans_finished = s_camera_get_finished_trans,
    };

    ESP_ERROR_CHECK(
        esp_cam_ctlr_register_event_callbacks(cam_handle, &cbs, &cam_ctx)
    );

    ESP_ERROR_CHECK(esp_cam_ctlr_enable(cam_handle));
    ESP_ERROR_CHECK(esp_cam_ctlr_start(cam_handle));

    ESP_LOGI(TAG, "Camera started (YUV422 to Y)");

    /* ================= Processing loop ================= */

    while (1) {
        static uint32_t last = 0;

        if (g_frame_counter != last) {
            last = g_frame_counter;

            /* ---- Stats ---- */
            y_stats_t s;
            compute_y_stats(g_y_buf, CAM_W * CAM_H, &s);

            /* ---- Heuristic gate ---- */
            if (s.mean < 20 || s.mean > 230) {
                ESP_LOGI(TAG,
                    "Frame %lu rejected (mean=%u)",
                    last, s.mean
                );
                goto next;
            }

            /* ---- Resize ---- */
            resize_y_nn(
                g_y_buf, CAM_W, CAM_H,
                ml_y_buf, ML_W, ML_H
            );

            /* ---- Normalize for ML ---- */
            for (int i = 0; i < ML_W * ML_H; i++) {
                ml_input[i] = ((int8_t)ml_y_buf[i]) - 128;
            }

            ESP_LOGI(TAG,
                "Frame %lu accepted (mean=%u)",
                last, s.mean
            );
        }

next:
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ================= Camera callbacks ================= */

static bool s_camera_get_new_vb(esp_cam_ctlr_handle_t handle,
                                esp_cam_ctlr_trans_t *trans,
                                void *user_data)
{
    example_cam_context_t *ctx = (example_cam_context_t *)user_data;
    *trans = ctx->cam_trans;
    return false;
}

static bool s_camera_get_finished_trans(esp_cam_ctlr_handle_t handle,
                                        esp_cam_ctlr_trans_t *trans,
                                        void *user_data)
{
    (void)handle;
    (void)user_data;

    extract_y_from_yuv422(
        (uint8_t *)trans->buffer,
        g_y_buf,
        CAM_W,
        CAM_H
    );

    g_frame_counter++;
    return false;
}

/* ================= Helpers ================= */

static void extract_y_from_yuv422(uint8_t *yuv,
                                  uint8_t *y,
                                  int width,
                                  int height)
{
    int y_idx = 0;
    int total_bytes = width * height * 2;

    for (int i = 0; i < total_bytes; i += 4) {
        y[y_idx++] = yuv[i];     // Y0
        y[y_idx++] = yuv[i + 2]; // Y1
    }
}

static void compute_y_stats(const uint8_t *y,
                            int size,
                            y_stats_t *out)
{
    uint8_t min = 255, max = 0;
    uint32_t sum = 0;

    for (int i = 0; i < size; i++) {
        uint8_t v = y[i];
        if (v < min) min = v;
        if (v > max) max = v;
        sum += v;
    }

    out->min  = min;
    out->max  = max;
    out->sum  = sum;
    out->mean = sum / size;
}

static void resize_y_nn(const uint8_t *src,
                        int src_w, int src_h,
                        uint8_t *dst,
                        int dst_w, int dst_h)
{
    for (int y = 0; y < dst_h; y++) {
        int sy = y * src_h / dst_h;
        for (int x = 0; x < dst_w; x++) {
            int sx = x * src_w / dst_w;
            dst[y * dst_w + x] = src[sy * src_w + sx];
        }
    }
}
