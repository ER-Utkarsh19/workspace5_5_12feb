/*
 * SPDX-FileCopyrightText: 2016-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*=====================================================================================
 * Description:
 *   The Modbus parameter structures used to define Modbus instances that
 *   can be addressed by Modbus protocol. Define these structures per your needs in
 *   your application. Below is just an example of possible parameters.
 *====================================================================================*/
#ifndef _DEVICE_PARAMS
#define _DEVICE_PARAMS

#include <stdint.h>

// This file defines structure of modbus parameters which reflect correspond modbus address space
// for each modbus register type (coils, discreet inputs, holding registers, input registers)
#pragma pack(push, 1)
typedef struct
{
    uint8_t discrete_input0:1;
    uint8_t discrete_input1:1;
    uint8_t discrete_input2:1;
    uint8_t discrete_input3:1;
    uint8_t discrete_input4:1;
    uint8_t discrete_input5:1;
    uint8_t discrete_input6:1;
    uint8_t discrete_input7:1;
    uint8_t discrete_input_port1;
    uint8_t discrete_input_port2;
} discrete_reg_params_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    uint8_t coils_port0;
    uint8_t coils_port1;
    uint8_t coils_port2;
} coil_reg_params_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    float input_data0; // 0
    float input_data1; // 2
    float input_data2; // 4
    float input_data3; // 6
    uint16_t data[150]; // 8 + 150 = 158
    float input_data4; // 158
    float input_data5;
    float input_data6;
    float input_data7;
    uint16_t data_block1[150];
} input_reg_params_t;
#pragma pack(pop)


/*
typedef struct
{
    float holding_data0;
    float holding_data1;
    float holding_data2;
    float holding_data3;
    uint16_t test_regs[150];
    float holding_data4;
    float holding_data5;
    float holding_data6;
    float holding_data7;
} holding_reg_params_t;
*/
#pragma pack(push, 1)
typedef struct
{
    /* Holding registers 0–11 */
    uint16_t holding_data0;    // AC UNIT ON/OFF
    uint16_t holding_data1;    // AC UNIT mode
    uint16_t holding_data2;    // Fan speed
    uint16_t holding_data3;    // Vane position
    uint16_t holding_data4;    // Temp setpoint
    uint16_t holding_data5;    // Temp reference
    uint16_t holding_data6;    // Window contact
    uint16_t holding_data7;    // Adapter enable
    uint16_t holding_data8;    // Remote control enable
    uint16_t holding_data9;    // Operation time
    uint16_t holding_data10;   // Alarm status
    uint16_t holding_data11;   // Error code

    /* Holding registers 12–21 (unused gap, Modbus addresses only) */
    uint16_t reserved_12_21[10];

    /* Holding registers 22–25 */
    uint16_t holding_data12;   // Ambient temp
    uint16_t holding_data13;   // Real temp setpoint
    uint16_t holding_data14;   // Max setpoint
    uint16_t holding_data15;   // Min setpoint

    /* Holding registers 26–30 (unused gap) */
    uint16_t reserved_26_30[5];

    /* Holding register 31 */
    uint16_t holding_data16;   // Status feedback

    /* Holding registers 32–33 (unused gap) */
    uint16_t reserved_32_33[2];

    /* Holding register 34 */
    uint16_t holding_data17;   // Left/right vane pulse

    /* Holding registers 35–65 (unused gap) */
    uint16_t reserved_35_65[31];

    /* Holding register 66 */
    uint16_t holding_data18;   // Return path temperature

    /* Holding registers 67–97 (unused gap) */
    uint16_t reserved_67_97[31];

    /* Holding register 98 */
    uint16_t holding_data19;   // Gateway / slave mode

} holding_reg_params_t;
#pragma pack(pop)



extern holding_reg_params_t holding_reg_params;
extern input_reg_params_t input_reg_params;
extern coil_reg_params_t coil_reg_params;
extern discrete_reg_params_t discrete_reg_params;

#endif // !defined(_DEVICE_PARAMS)

