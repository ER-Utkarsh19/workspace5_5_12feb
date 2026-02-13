import os
import numpy as np
import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers

# --- Configuration ---
DATASET_DIR = "dataset" 
IMG_HEIGHT = 96
IMG_WIDTH = 96
BATCH_SIZE = 8
EPOCHS = 20

def train():
    # 1. Check if data exists
    if not os.path.exists(DATASET_DIR):
        print(f"ERROR: '{DATASET_DIR}' folder not found.")
        return

    print("Loading images...")
    # Load Training Data
    train_ds = tf.keras.utils.image_dataset_from_directory(
        DATASET_DIR,
        validation_split=0.2,
        subset="training",
        seed=123,
        image_size=(IMG_HEIGHT, IMG_WIDTH),
        batch_size=BATCH_SIZE,
        color_mode='grayscale'
    )

    # Load Validation Data
    val_ds = tf.keras.utils.image_dataset_from_directory(
        DATASET_DIR,
        validation_split=0.2,
        subset="validation",
        seed=123,
        image_size=(IMG_HEIGHT, IMG_WIDTH),
        batch_size=BATCH_SIZE,
        color_mode='grayscale'
    )

    class_names = train_ds.class_names
    print(f"Classes found: {class_names}")

    # 2. Preprocessing (Normalize 0-255 to -1 to 1)
    normalization_layer = layers.Rescaling(1./127.5, offset=-1)
    train_ds = train_ds.map(lambda x, y: (normalization_layer(x), y))
    val_ds = val_ds.map(lambda x, y: (normalization_layer(x), y))

    # 3. Build CNN Model (Small for ESP32)
    model = keras.Sequential([
        layers.InputLayer(input_shape=(IMG_HEIGHT, IMG_WIDTH, 1)),
        layers.Conv2D(8, 3, padding='same', activation='relu'),
        layers.MaxPooling2D(),
        layers.Conv2D(16, 3, padding='same', activation='relu'),
        layers.MaxPooling2D(),
        layers.Flatten(),
        layers.Dense(32, activation='relu'),
        layers.Dropout(0.2),
        layers.Dense(len(class_names), activation='softmax')
    ])

    model.compile(optimizer='adam',
                  loss=tf.keras.losses.SparseCategoricalCrossentropy(),
                  metrics=['accuracy'])

    # 4. Train
    print("\nStarting training...")
    model.fit(train_ds, validation_data=val_ds, epochs=EPOCHS)

    # 5. Convert to TFLite (FIXED QUANTIZATION)
    print("\nConverting to TFLite Micro...")
    
    def representative_data_gen():
        for input_value, _ in train_ds.take(100):
            yield [tf.cast(input_value, tf.float32)]

    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = representative_data_gen
    
    # --- CRITICAL FIX FOR ESP32 ---
    # This line prevents the "FullyConnected per-channel quantization not yet supported" error
    converter._experimental_disable_per_channel_quantization_for_dense_layers = True
    # ------------------------------

    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    tflite_model = converter.convert()

    # Save TFLite file
    with open("model.tflite", "wb") as f:
        f.write(tflite_model)
    print("Saved 'model.tflite'")

    # 6. Generate C++ File (model_data.cc)
    print("Generating C header file...")
    hex_array = [f"0x{b:02x}" for b in tflite_model]
    
    c_code = f"""
#include "model_data.h"

// Align to 16 bytes for ESP32-S3 DMA
const unsigned char g_model_data[] __attribute__((aligned(16))) = {{
  {', '.join(hex_array)}
}};

const unsigned int g_model_data_len = {len(tflite_model)};
"""

    with open("model_data.cc", "w") as f:
        f.write(c_code)

    print("\nSUCCESS! New 'model_data.cc' generated.")
    print("Replace the file in your ESP32 /main folder with this new one.")

if __name__ == '__main__':
    train()