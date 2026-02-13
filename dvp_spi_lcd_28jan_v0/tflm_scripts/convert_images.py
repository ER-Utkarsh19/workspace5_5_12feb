import os
from PIL import Image

# Configuration
DATASET_DIR = "dataset"
CLASSES = ["clean", "dirty"]

def convert():
    if not os.path.exists(DATASET_DIR):
        print(f"Error: '{DATASET_DIR}' directory not found.")
        return

    total_converted = 0

    for class_name in CLASSES:
        class_path = os.path.join(DATASET_DIR, class_name)
        if not os.path.exists(class_path):
            print(f"Skipping missing folder: {class_path}")
            continue

        print(f"Scanning '{class_name}'...")
        files = os.listdir(class_path)
        
        for filename in files:
            if filename.lower().endswith(".pgm"):
                pgm_path = os.path.join(class_path, filename)
                png_path = os.path.join(class_path, os.path.splitext(filename)[0] + ".png")

                try:
                    # Convert
                    with Image.open(pgm_path) as img:
                        img.save(png_path)
                    
                    # Delete the old PGM to avoid confusion
                    os.remove(pgm_path)
                    total_converted += 1
                except Exception as e:
                    print(f"Failed to convert {filename}: {e}")

    print("-" * 30)
    print(f"Conversion Complete! Total images ready: {total_converted}")
    
    # Verification check
    for class_name in CLASSES:
        path = os.path.join(DATASET_DIR, class_name)
        if os.path.exists(path):
            count = len([f for f in os.listdir(path) if f.endswith(".png")])
            print(f"Folder '{class_name}': {count} PNG files")

if __name__ == "__main__":
    convert()