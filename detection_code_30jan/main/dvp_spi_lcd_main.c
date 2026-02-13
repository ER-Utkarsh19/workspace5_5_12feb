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
#include "esp_spiffs.h"

#include <unistd.h> // Required for unlink()

#include "model_inference.h" 

/* ================= Configuration ================= */

#define CAM_W   CONFIG_EXAMPLE_CAM_HRES
#define CAM_H   CONFIG_EXAMPLE_CAM_VRES

#define ML_W    96
#define ML_H    96
//#define SAVE_EVERY_N_FRAMES 100
#define SAVE_EVERY_N_FRAMES 2
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

static void spiffs_init(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true,
    };

  esp_err_t ret = esp_vfs_spiffs_register(&conf);
if (ret != ESP_OK) {
    ESP_LOGE("SPIFFS", "Mount failed (%s)", esp_err_to_name(ret));
}
}
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
                        
static void resize_y_bilinear(const uint8_t *src,
                             int src_w, int src_h,
                             uint8_t *dst,
                             int dst_w, int dst_h);
/*static void save_ml_frame_pgm(const uint8_t *img, int w, int h, uint32_t frame_id)
{
    char path[64];
    snprintf(path, sizeof(path), "/spiffs/ml_%06lu.pgm", frame_id);

    FILE *f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE("PGM", "Failed to open %s", path);
        return;
    }

    // PGM header (binary P5)
    fprintf(f, "P5\n%d %d\n255\n", w, h);

    // Write raw grayscale data
    fwrite(img, 1, w * h, f);

    fclose(f);
    ESP_LOGI("PGM", "Saved %s (%d bytes)", path, w * h);
}*/


// ... (Keep your existing Includes and Globals) ...

/* ---- Circular Buffer Save Function ---- */
/*static void save_night_frame(const uint8_t *img, int w, int h, uint32_t frame_id)
{
    char path[64];
    // Saves night_0.pgm to night_4.pgm, then overwrites
  //  snprintf(path, sizeof(path), "/spiffs/night_%lu.pgm", frame_id % 5);
     snprintf(path, sizeof(path), "/spiffs/night_%lu.pgm", frame_id % 50);

    unlink(path); // Delete old file to prevent "Disk Full"

    FILE *f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE("PGM", "Failed to open %s", path);
        return;
    }

    fprintf(f, "P5\n%d %d\n255\n", w, h);
    fwrite(img, 1, w * h, f);
    fclose(f);
    ESP_LOGI("PGM", "Saved %s (Contrast Enhanced)", path);
}*/
/* ---- Circular Buffer Save Function (FIXED) ---- */
/*static void save_night_frame(const uint8_t *img, int w, int h, uint32_t frame_id)
{
    // NEW: Static counter that remembers its value between calls
    static int save_index = 0; 

    char path[64];
    // Use 'save_index' instead of 'frame_id'
    snprintf(path, sizeof(path), "/spiffs/night_%d.pgm", save_index);

    // Increment for next time, wrap around at 50
    save_index++;
    if (save_index >= 50) {
        save_index = 0;
    }

    unlink(path); // Delete old file to prevent "Disk Full"

    FILE *f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE("PGM", "Failed to open %s", path);
        return;
    }

    fprintf(f, "P5\n%d %d\n255\n", w, h);
    fwrite(img, 1, w * h, f);
    fclose(f);
    
    // Log with the SEQUENTIAL number so you know when you hit 50
    ESP_LOGI("PGM", "Saved %s (Frame ID %lu)", path, frame_id);
}*/
/* ---- Circular Buffer Save Function (FIXED) ---- */
static void save_night_frame(const uint8_t *img, int w, int h, uint32_t frame_id)
{
    // NEW: This counter remembers its value forever.
    // It ignores the camera frame rate and just counts 0, 1, 2, 3... every time we save.
    static int file_counter = 0; 

    char path[64];
    // Use 'file_counter', NOT 'frame_id'
    snprintf(path, sizeof(path), "/spiffs/night_%d.pgm", file_counter);

    // Increment and wrap around at 50
    file_counter++;
    if (file_counter >= 50) {
        file_counter = 0;
    }

    unlink(path); // Delete old file to prevent errors

    FILE *f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE("PGM", "Failed to open %s", path);
        return;
    }

    fprintf(f, "P5\n%d %d\n255\n", w, h);
    fwrite(img, 1, w * h, f);
    fclose(f);
    
    ESP_LOGI("PGM", "Saved %s (Src Frame %lu)", path, frame_id);
}
/* ================= Application entry ================= */

void app_main(void)
{
    esp_err_t ret;
    /* ---- SPIFFS MUST COME FIRST ---- */
spiffs_init();


size_t total = 0, used = 0;
esp_spiffs_info(NULL, &total, &used);
ESP_LOGI("SPIFFS", "Total=%d bytes, Used=%d bytes", total, used);
ESP_LOGI(TAG, "Starting Software Night Vision Loop...");            

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
    model_setup();

    /* ================= Processing loop ================= */

/* ================= Processing loop ================= */

/* ================= Processing loop (No AI, Vision Only) ================= */
    while (1) {
        static uint32_t last_processed_frame = 0;

        if (g_frame_counter != last_processed_frame) {
            last_processed_frame = g_frame_counter;

            /* ---- 1. Calculate Stats (Raw Image) ---- */
            y_stats_t s;
            compute_y_stats(g_y_buf, CAM_W * CAM_H, &s);

            /* ---- 2. Night Gate: Accept almost anything ---- */
            // We only reject if it is absolute zero noise (mean < 2)
            if (s.mean < 2) {
                if (last_processed_frame % 50 == 0) 
                    ESP_LOGI(TAG, "Frame %lu too dark (mean=%u)", last_processed_frame, s.mean);
                goto next;
            }

            /* ---- 3. Resize to 96x96 ---- */
            resize_y_bilinear(g_y_buf, CAM_W, CAM_H, ml_y_buf, ML_W, ML_H);

       
            /* ---- 4. Dynamic Contrast Stretching (The "Night Boost") ---- */
            // This is the CRITICAL part for your dark environment
          /*  uint8_t min_val = 255;
            uint8_t max_val = 0;

            // Pass A: Find the darkest and brightest pixels
            for (int i = 0; i < ML_W * ML_H; i++) {
                if (ml_y_buf[i] < min_val) min_val = ml_y_buf[i];
                if (ml_y_buf[i] > max_val) max_val = ml_y_buf[i];
            }

            // Pass B: Stretch the range to fill 0-255
            // If the difference is tiny (solid gray noise), we skip stretching
            if ((max_val - min_val) > 10) {
                float scale = 255.0f / (max_val - min_val);
                for (int i = 0; i < ML_W * ML_H; i++) {
                    float val = (ml_y_buf[i] - min_val) * scale;
                    
                    // Update the buffer so you can SEE the boost in the Hex Dump
                    ml_y_buf[i] = (uint8_t)val;
                    
                    // Prepare for ML (INT8: -128 to 127) - Ready for later
                    ml_input[i] = (int8_t)(val - 128);
                }
            } else {
                // Image is too flat to boost. Just center it.
                for (int i = 0; i < ML_W * ML_H; i++) {
                    ml_input[i] = (int8_t)(ml_y_buf[i] - 128);
                }
            }*/
/* ---- 4. Dynamic Contrast Stretching (Smart Version) ---- */
            
            // NEW CHECK: If the image is already well-lit (Mean > 60), 
            // DON'T stretch it. Just subtract 128 for the AI.
                uint8_t min_val = 0;
                uint8_t max_val = 255;
            if (s.mean > 60) {
                 for (int i = 0; i < ML_W * ML_H; i++) {
                    ml_y_buf[i] = ml_y_buf[i]; // Keep original look
                    ml_input[i] = (int8_t)(ml_y_buf[i] - 128);
                }
                // Log that we skipped the boost
                if (last_processed_frame % 50 == 0)
                    ESP_LOGI(TAG, "Frame %lu Bright (Mean=%u) - Boost Skipped", last_processed_frame, s.mean);

            } else {
                // --- EXISTING DARK MODE BOOST CODE GOES HERE ---
              min_val = 255;
              max_val = 0;

                // Pass A: Find Range
                for (int i = 0; i < ML_W * ML_H; i++) {
                    if (ml_y_buf[i] < min_val) min_val = ml_y_buf[i];
                    if (ml_y_buf[i] > max_val) max_val = ml_y_buf[i];
                }

                // Pass B: Stretch
                if ((max_val - min_val) > 10) {
                    float scale = 255.0f / (max_val - min_val);
                    for (int i = 0; i < ML_W * ML_H; i++) {
                        float val = (ml_y_buf[i] - min_val) * scale;
                        ml_y_buf[i] = (uint8_t)val;
                        ml_input[i] = (int8_t)(val - 128);
                    }
                } else {
                     // Flat/Noise fallback
                     for (int i = 0; i < ML_W * ML_H; i++) {
                        ml_input[i] = (int8_t)(ml_y_buf[i] - 128);
                    }
                }
            }
            /* ---- 5. Hex Dump (Verify the BOOSTED image) ---- */
            if ((last_processed_frame % 2) == 0) {
                printf("IMAGE_START\n");
                for (int i = 0; i < ML_W * ML_H; i++) {
                    printf("%02x", ml_y_buf[i]); 
                    if ((i + 1) % ML_W == 0) printf("\n"); 
                }
                printf("IMAGE_END\n");
                
                save_night_frame(ml_y_buf, ML_W, ML_H, last_processed_frame);
                // Log how much we boosted the image
                ESP_LOGI(TAG, "Frame %lu | Raw Mean: %u | Boost Range: %u->%u", 
                         last_processed_frame, s.mean, min_val, max_val);
            }

            /* ---- 6. Save to SPIFFS (Verification) ---- */
            if ((last_processed_frame % SAVE_EVERY_N_FRAMES) == 0) {
                save_night_frame(ml_y_buf, ML_W, ML_H, last_processed_frame);
            }
            model_run_inference(ml_input);
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
/*
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
}*/
static void resize_y_bilinear(const uint8_t *src,
                             int src_w, int src_h,
                             uint8_t *dst,
                             int dst_w, int dst_h)
{
    float x_ratio = ((float)(src_w - 1)) / dst_w;
    float y_ratio = ((float)(src_h - 1)) / dst_h;

    for (int i = 0; i < dst_h; i++) {
        for (int j = 0; j < dst_w; j++) {
            int x = (int)(x_ratio * j);
            int y = (int)(y_ratio * i);
            
            float x_diff = (x_ratio * j) - x;
            float y_diff = (y_ratio * i) - y;
            
            int index = (y * src_w + x);

            // Get the 4 neighboring pixels
            uint8_t a = src[index];                // Top-left
            uint8_t b = src[index + 1];            // Top-right
            uint8_t c = src[index + src_w];        // Bottom-left
            uint8_t d = src[index + src_w + 1];    // Bottom-right

            // Blue magic: calculate weighted average
            float pixel = (a * (1 - x_diff) * (1 - y_diff) +
                           b * (x_diff) * (1 - y_diff) +
                           c * (y_diff) * (1 - x_diff) +
                           d * (x_diff * y_diff));

            dst[i * dst_w + j] = (uint8_t)pixel;
        }
    }
}