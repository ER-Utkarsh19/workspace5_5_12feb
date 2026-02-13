/*
 * tflm_inference.h
 *
 *  Created on: Jan 29, 2026
 *      Author: admin
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 1. Setup the LiteRT Interpreter (Allocate memory, load model)
bool model_setup(void);

// 2. Run Inference
// Input: Pointer to your 96x96 int8_t image buffer
// Output: 0 = Clean, 1 = Dirty, 2 = Dropping (We will map probabilities to these)
int model_run_inference(int8_t *image_data);

#ifdef __cplusplus
}
#endif
 /* MAIN_TFLM_INFERENCE_H_ */
