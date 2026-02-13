#include "model_inference.h"
#include "model_data.h"
#include "esp_log.h"
#include "esp_heap_caps.h" // Required for SPIRAM allocation

#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"

// Globals
static const tflite::Model* model = nullptr;
static tflite::MicroInterpreter* interpreter = nullptr;
static TfLiteTensor* input = nullptr;
static TfLiteTensor* output = nullptr;

// 1. Memory Arena (80KB to be safe, allocated in SPIRAM)
const int kTensorArenaSize = 120 * 1024;
static uint8_t *tensor_arena = nullptr;

// 2. Operation Resolver (The Toolbox)
// Increased to 25 to hold the "Kitchen Sink" list
static tflite::MicroMutableOpResolver<25> micro_op_resolver;

void model_setup(void) {
    // A. Allocate Arena in SPIRAM (External RAM)
    // This prevents "Out of Memory" errors
    tensor_arena = (uint8_t *)heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (tensor_arena == nullptr) {
        ESP_LOGW("AI", "SPIRAM allocation failed, trying Internal RAM...");
        tensor_arena = (uint8_t *)heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    
    if (tensor_arena == nullptr) {
        ESP_LOGE("AI", "CRITICAL: Could not allocate Tensor Arena!");
        return;
    }

    // B. Load Model
    model = tflite::GetModel(g_model_data);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE("AI", "Model schema mismatch! Expected %d, got %ld", 
                 TFLITE_SCHEMA_VERSION, model->version());
        return;
    }

    // C. Register Ops (The "Kitchen Sink" List)
    // We add everything to prevent further "Didn't find op" errors.
    micro_op_resolver.AddAbs();
    micro_op_resolver.AddAdd();
    micro_op_resolver.AddAveragePool2D();
    micro_op_resolver.AddConcatenation(); // Common in vision models
    micro_op_resolver.AddConv2D();
    micro_op_resolver.AddDepthwiseConv2D();
    micro_op_resolver.AddDequantize();
    micro_op_resolver.AddFullyConnected();
    micro_op_resolver.AddLogistic();
    micro_op_resolver.AddMaxPool2D();
    micro_op_resolver.AddMean();
    micro_op_resolver.AddMul();
    micro_op_resolver.AddPad();
    micro_op_resolver.AddPack();          // Often appears with Shape
    micro_op_resolver.AddQuantize();
    micro_op_resolver.AddRelu();
    micro_op_resolver.AddReshape();
    micro_op_resolver.AddShape();
    micro_op_resolver.AddSoftmax();
    micro_op_resolver.AddStridedSlice();  // <--- THE FIX FOR YOUR ERROR
    micro_op_resolver.AddSub();
    micro_op_resolver.AddUnpack();

    // D. Init Interpreter
    static tflite::MicroInterpreter static_interpreter(
        model, micro_op_resolver, tensor_arena, kTensorArenaSize);
    interpreter = &static_interpreter;

    // E. Allocate Tensors
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        ESP_LOGE("AI", "AllocateTensors failed!");
        return;
    }

    // F. Get Pointers
    input = interpreter->input(0);
    output = interpreter->output(0);

    ESP_LOGI("AI", "Model Loaded Successfully.");
    ESP_LOGI("AI", "Input Shape: %d x %d x %d", 
             input->dims->data[1], input->dims->data[2], input->dims->data[3]);
}

float model_run_inference(const int8_t *img_data) {
    if (input == nullptr) {
        // Quietly return if not initialized
        return 0.0f;
    }

    // Copy image data to model input buffer
    memcpy(input->data.int8, img_data, input->bytes);

    // Run!
    if (interpreter->Invoke() != kTfLiteOk) {
        ESP_LOGE("AI", "Invoke failed!");
        return 0.0f;
    }

    // Get Result (Index 1 is "Dirty")
    int8_t dirty_score_raw = output->data.int8[1];
    
    // De-quantize
    float dirty_score = (dirty_score_raw - output->params.zero_point) * output->params.scale;

    return dirty_score;
}