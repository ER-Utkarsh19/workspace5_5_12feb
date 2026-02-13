/*
 * SPDX-FileCopyrightText: 2016-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <sys/queue.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "mdns.h"
#include "protocol_examples_common.h"

#include "modbus_params.h"  // Ensure this matches your .h file exactly
#include "mbcontroller.h"
#include "sdkconfig.h"

#define MB_TCP_PORT                     (CONFIG_FMB_TCP_PORT_DEFAULT)
#define MASTER_MAX_CIDS                 num_device_parameters
#define MASTER_MAX_RETRY                (30)
#define UPDATE_CIDS_TIMEOUT_MS          (5000)
#define UPDATE_CIDS_TIMEOUT_TICS        (UPDATE_CIDS_TIMEOUT_MS / portTICK_PERIOD_MS)
#define POLL_TIMEOUT_MS                 (2000)
#define POLL_TIMEOUT_TICS               (POLL_TIMEOUT_MS / portTICK_PERIOD_MS)
#define MB_MDNS_PORT                    (502)

// Macro to get offset
#define HOLD_OFFSET(field) ((uint16_t)(offsetof(holding_reg_params_t, field) + 1))
#define STR(fieldname) ((const char*)( fieldname ))
#define OPTS(min_val, max_val, step_val) { .opt1 = min_val, .opt2 = max_val, .opt3 = step_val }

static const char *TAG = "MASTER_TEST";

// Modbus Address Enum
enum {
    MB_DEVICE_ADDR1 = 1, 
};

// CID Enum (Must match the order in device_parameters)
enum {
    CID_HOLD_DATA_0 = 0,
    CID_HOLD_DATA_1,
    CID_HOLD_DATA_2,
    CID_HOLD_DATA_3,
    CID_HOLD_DATA_4,
    CID_HOLD_DATA_5,
    CID_HOLD_DATA_6,
    CID_HOLD_DATA_7,
    CID_HOLD_DATA_8,
    CID_HOLD_DATA_9,
    CID_HOLD_DATA_10,
    CID_HOLD_DATA_11,
    CID_HOLD_DATA_12,
    CID_HOLD_DATA_13,
    CID_HOLD_DATA_14,
    CID_HOLD_DATA_15,
    CID_HOLD_DATA_16,
    CID_HOLD_DATA_17,
    CID_HOLD_DATA_18,
    CID_HOLD_DATA_19,
    CID_COUNT
};

// Parameter Descriptor
const mb_parameter_descriptor_t device_parameters[] = {
    { CID_HOLD_DATA_0, STR("AC UNIT"), STR("ON/OFF"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 0, 1,
      HOLD_OFFSET(holding_data0), PARAM_TYPE_U16, 2, OPTS(0, 1, 1), PAR_PERMS_READ_WRITE_TRIGGER },
    { CID_HOLD_DATA_1, STR("AC UNIT mode"), STR("auto,heat,dry,fan,cool"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 1, 1,
      HOLD_OFFSET(holding_data1), PARAM_TYPE_U16, 2, OPTS(0, 4, 1), PAR_PERMS_READ_WRITE_TRIGGER },
    { CID_HOLD_DATA_2, STR("AC UNIT fan speed"), STR("auto low mid high super high"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 2, 1,
      HOLD_OFFSET(holding_data2), PARAM_TYPE_U16, 2, OPTS(0, 4, 1), PAR_PERMS_READ_WRITE_TRIGGER },
    { CID_HOLD_DATA_3, STR("AC UNIT vane position"), STR("pos1 to pos7,swing"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 3, 1,
      HOLD_OFFSET(holding_data3), PARAM_TYPE_U16, 2, OPTS(1, 8, 1), PAR_PERMS_READ_WRITE_TRIGGER },
    { CID_HOLD_DATA_4, STR("AC UNIT temp setpoint"), STR("ac uint temp setpoint"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 4, 1,
      HOLD_OFFSET(holding_data4), PARAM_TYPE_U16, 2, OPTS(1, 3, 1), PAR_PERMS_READ_WRITE_TRIGGER },
    { CID_HOLD_DATA_5, STR("AC UNIT temp reference"), STR("reference"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 5, 1,
      HOLD_OFFSET(holding_data5), PARAM_TYPE_U16, 2, OPTS(1, 3, 1), PAR_PERMS_READ_TRIGGER },
    { CID_HOLD_DATA_6, STR("window contact"), STR("open/close"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 6, 1,
      HOLD_OFFSET(holding_data6), PARAM_TYPE_U16, 2, OPTS(0, 1, 1), PAR_PERMS_READ_WRITE_TRIGGER },
    { CID_HOLD_DATA_7, STR("adapter enable/ disable"), STR("enable/ disable"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 7, 1,
      HOLD_OFFSET(holding_data7), PARAM_TYPE_U16, 2, OPTS(0, 1, 1), PAR_PERMS_READ_WRITE_TRIGGER },
    { CID_HOLD_DATA_8, STR("ac remote control"), STR("enable/ disable"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 8, 1,
      HOLD_OFFSET(holding_data8), PARAM_TYPE_U16, 2, OPTS(0, 1, 1), PAR_PERMS_READ_WRITE_TRIGGER },
    { CID_HOLD_DATA_9, STR("ac unit operation time"), STR("enable/ disable"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 9, 1,
      HOLD_OFFSET(holding_data9), PARAM_TYPE_U16, 2, OPTS(1, 1, 1), PAR_PERMS_READ_WRITE_TRIGGER },
    { CID_HOLD_DATA_10, STR("ac unit alarm status"), STR("enable/ disable"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 10, 1,
      HOLD_OFFSET(holding_data10), PARAM_TYPE_U16, 2, OPTS(0, 1, 1), PAR_PERMS_READ_TRIGGER },
    { CID_HOLD_DATA_11, STR("error code"), STR("enable/ disable"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 11, 1,
      HOLD_OFFSET(holding_data11), PARAM_TYPE_U16, 2, OPTS(0, 2, 1), PAR_PERMS_READ_TRIGGER },
    { CID_HOLD_DATA_12, STR("ambient temp"), STR("enable/ disable"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 22, 1,
      HOLD_OFFSET(holding_data12), PARAM_TYPE_U16, 2, OPTS(0, 1, 1), PAR_PERMS_READ_WRITE_TRIGGER },
    { CID_HOLD_DATA_13, STR("ac real temp setpoint"), STR("enable/ disable"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 23, 1,
      HOLD_OFFSET(holding_data13), PARAM_TYPE_U16, 2, OPTS(1, 3, 1), PAR_PERMS_READ_TRIGGER },
    { CID_HOLD_DATA_14, STR("current ac max setpoint"), STR("enable/ disable"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 24, 1,
      HOLD_OFFSET(holding_data14), PARAM_TYPE_U16, 2, OPTS(1, 2, 1), PAR_PERMS_READ_TRIGGER },
    { CID_HOLD_DATA_15, STR("current ac min setpoint"), STR("enable/ disable"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 25, 1,
      HOLD_OFFSET(holding_data15), PARAM_TYPE_U16, 2, OPTS(1, 2, 1), PAR_PERMS_READ_TRIGGER },
    { CID_HOLD_DATA_16, STR("status"), STR("feedback"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 31, 1,
      HOLD_OFFSET(holding_data16), PARAM_TYPE_U16, 2, OPTS(0, 1, 1), PAR_PERMS_READ_TRIGGER },
    { CID_HOLD_DATA_17, STR("lef/right vane pulse"), STR("feedback"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 34, 1,
      HOLD_OFFSET(holding_data17), PARAM_TYPE_U16, 2, OPTS(0, 1, 1), PAR_PERMS_WRITE_TRIGGER },
    { CID_HOLD_DATA_18, STR("return path temp"), STR("feedback"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 66, 1,
      HOLD_OFFSET(holding_data18), PARAM_TYPE_U16, 2, OPTS(0, 1, 1), PAR_PERMS_READ_TRIGGER },
    { CID_HOLD_DATA_19, STR("gateway"), STR("slave"), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 98, 1,
      HOLD_OFFSET(holding_data19), PARAM_TYPE_U16, 2, OPTS(0, 1, 1), PAR_PERMS_READ_WRITE_TRIGGER },
};

const uint16_t num_device_parameters = (sizeof(device_parameters) / sizeof(device_parameters[0]));

// IP Address Table - Configured to ask user via STDIN
char* slave_ip_address_table[] = {
    "FROM_STDIN", 
    NULL
};
const size_t ip_table_sz = (size_t)(sizeof(slave_ip_address_table) / sizeof(slave_ip_address_table[0]));


// --- IP SCANNING LOGIC ---
char* master_scan_addr(int* index, char* buffer)
{
    char* ip_str = NULL;
    int a[8] = {0};
    int buf_cnt = 0;
    buf_cnt = sscanf(buffer, "IP%d="IPSTR, index, &a[0], &a[1], &a[2], &a[3]);
    if (buf_cnt == 5) {
        if (-1 == asprintf(&ip_str, IPSTR, a[0], a[1], a[2], a[3])) {
            abort();
        }
    }
    return ip_str;
}

static int master_get_slave_ip_stdin(char** addr_table)
{
    char buf[128];
    int index;
    char* ip_str = NULL;
    int buf_cnt = 0;
    int ip_cnt = 0;

    if (!addr_table) return 0;

    ESP_ERROR_CHECK(example_configure_stdin_stdout());
    
    while(1) {
        if (addr_table[ip_cnt] && strcmp(addr_table[ip_cnt], "FROM_STDIN") == 0) {
            printf("Waiting IP%d from stdin:\r\n", ip_cnt);
            while (fgets(buf, sizeof(buf), stdin) == NULL) {
                fputs(buf, stdout);
            }
            buf_cnt = strlen(buf);
            if(buf_cnt > 0) buf[buf_cnt - 1] = '\0';
            
            ip_str = master_scan_addr(&index, buf);
            if (ip_str != NULL) {
                if(index == ip_cnt) {
                    ESP_LOGI(TAG, "IP(%d) = [%s] set.", ip_cnt, ip_str);
                    addr_table[ip_cnt++] = ip_str;
                } else {
                    printf("Index mismatch. Expected IP%d\n", ip_cnt);
                    free(ip_str);
                }
            } else {
                 printf("Format Error. Use IP%d=192.168.x.x\n", ip_cnt);
            }

            if (ip_cnt >= ip_table_sz) break;
        } else {
            if (addr_table[ip_cnt]) {
                ip_cnt++;
            } else {
                break;
            }
        }
    }
    return ip_cnt;
}

static void master_destroy_slave_list(char** table, size_t ip_table_size)
{
    for (int i = 0; ((i < ip_table_size) && table[i] != NULL); i++) {
        if (table[i] && strcmp(table[i], "FROM_STDIN") != 0) {
            free(table[i]);
            table[i] = "FROM_STDIN";
        }
    }
}

// --- MASTER OPERATION TASK ---
static void master_operation_func(void *arg) {
    esp_err_t err = ESP_OK;
    const mb_parameter_descriptor_t* param_descriptor = NULL;
    
    // FIX 1: The 'type' variable must be uint8_t for the library functions
    uint8_t type = 0; 
    
    ESP_LOGI(TAG, "START OPERATIONS");

    while(1) {
        
        // --- 1. Holding Data 0 (AC ON/OFF) ---
        err = mbc_master_get_cid_info(CID_HOLD_DATA_0, &param_descriptor);
        if ((err == ESP_OK) && (param_descriptor != NULL)) {
            uint16_t ac_on = 1;
            // FIX 2: Cast the value pointer to (uint8_t*)
            err = mbc_master_set_parameter(CID_HOLD_DATA_0, (char*)param_descriptor->param_key, (uint8_t*)&ac_on, &type);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "AC IS ON AT %u (CID #%u)", ac_on, CID_HOLD_DATA_0);
            } else {
                ESP_LOGE(TAG, "Failed to write CID #%u", CID_HOLD_DATA_0);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));

        // --- 2. Holding Data 1 (Mode) ---
        err = mbc_master_get_cid_info(CID_HOLD_DATA_1, &param_descriptor);
        if ((err == ESP_OK) && (param_descriptor != NULL)) {
            uint16_t unit_mode = 4;
            err = mbc_master_set_parameter(CID_HOLD_DATA_1, (char*)param_descriptor->param_key, (uint8_t*)&unit_mode, &type);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Mode set to %u (CID #%u)", unit_mode, CID_HOLD_DATA_1);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));

        // --- 3. Holding Data 4 (Temp Setpoint) ---
        err = mbc_master_get_cid_info(CID_HOLD_DATA_4, &param_descriptor);
        if ((err == ESP_OK) && (param_descriptor != NULL)) {
            uint16_t ac_unit_setpt = 24; 
            err = mbc_master_set_parameter(CID_HOLD_DATA_4, (char*)param_descriptor->param_key, (uint8_t*)&ac_unit_setpt, &type);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Temp set to %u (CID #%u)", ac_unit_setpt, CID_HOLD_DATA_4);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));

        // --- 4. Holding Data 5 (Temp Read) ---
        err = mbc_master_get_cid_info(CID_HOLD_DATA_5, &param_descriptor);
        if ((err == ESP_OK) && (param_descriptor != NULL)) {
            uint16_t ac_unit_temp_read = 0;
            // FIX 3: Cast the value pointer to (uint8_t*)
            err = mbc_master_get_parameter(CID_HOLD_DATA_5, (char*)param_descriptor->param_key, (uint8_t*)&ac_unit_temp_read, &type);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Current Temp Read: %u (CID #%u)", ac_unit_temp_read, CID_HOLD_DATA_5);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// --- INIT SERVICES ---
static esp_err_t init_services(mb_tcp_addr_type_t ip_addr_type)
{
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);

    result = esp_netif_init();
    ESP_ERROR_CHECK(result);
    
    result = esp_event_loop_create_default();
    ESP_ERROR_CHECK(result);

    // This calls the example_connect() which uses the menuconfig Ethernet settings
    result = example_connect(); 
    ESP_ERROR_CHECK(result);

    // Handle IP Stdin input
    int ip_cnt = master_get_slave_ip_stdin(slave_ip_address_table);
    if (ip_cnt) {
        ESP_LOGI(TAG, "Configured %d IP addresse(s).", ip_cnt);
    } else {
        ESP_LOGE(TAG, "Fail to get IP address. Continue.");
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}

static esp_err_t destroy_services(void)
{
    master_destroy_slave_list(slave_ip_address_table, ip_table_sz);
    example_disconnect();
    esp_event_loop_delete_default();
    esp_netif_deinit();
    nvs_flash_deinit();
    return ESP_OK;
}

// --- MASTER INIT ---
static esp_err_t master_init(mb_communication_info_t* comm_info)
{
    void* master_handler = NULL;
    esp_err_t err = mbc_master_init_tcp(&master_handler);
    MB_RETURN_ON_FALSE((master_handler != NULL), ESP_ERR_INVALID_STATE, TAG, "mb init fail");
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG, "mb init fail");

    err = mbc_master_setup((void*)comm_info);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG, "mb setup fail");

    err = mbc_master_set_descriptor(&device_parameters[0], num_device_parameters);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG, "mb set descriptor fail");

    ESP_LOGI(TAG, "Modbus master stack initialized...");
    err = mbc_master_start();
    return err;
}

static esp_err_t master_destroy(void)
{
    mbc_master_destroy();
    return ESP_OK;
}

void app_main(void)
{
    mb_tcp_addr_type_t ip_addr_type = MB_IPV4;
    ESP_ERROR_CHECK(init_services(ip_addr_type));

    mb_communication_info_t comm_info = { 0 };
    comm_info.ip_port = MB_TCP_PORT;
    comm_info.ip_addr_type = ip_addr_type;
    comm_info.ip_mode = MB_MODE_TCP;
    comm_info.ip_addr = (void*)slave_ip_address_table;
    comm_info.ip_netif_ptr = (void*)get_example_netif();

    ESP_ERROR_CHECK(master_init(&comm_info));

    master_operation_func(NULL);
    
    master_destroy();
    destroy_services();
}