#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Setup the AI engine (Call once at startup)
void model_setup(void);

// Run the AI on an image
// input: pointer to your 96x96 int8 image buffer
// returns: float score (0.0 to 1.0) for the "Dirty" class
float model_run_inference(const int8_t *input);

#ifdef __cplusplus
}
#endif