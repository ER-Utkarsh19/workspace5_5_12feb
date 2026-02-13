import os
import shutil
from PIL import Image

# Define path
BASE_DIR = "dataset"

# Map your current folder names to the correct class names
# "Current Name" : "Target Name"
FOLDER_MAP = {
    "extracted_images": "clean",
    "extracted_images_dirty": "dirty"
}

def prepare():
    if not os.path.exists(BASE_DIR):
        print(f"Error: '{BASE_DIR}' folder not found.")
        return

    print("--- 1. Renaming Folders ---")
    for current_name, target_name in FOLDER_MAP.items():
        old_path = os.path.join(BASE_DIR, current_name)
        new_path = os.path.join(BASE_DIR, target_name)
        
        if os.path.exists(old_path):
            if not os.path.exists(new_path):
                os.rename(old_path, new_path)
                print(f"Renamed: {current_name} -> {target_name}")
            else:
                print(f"Merging {current_name} into existing {target_name}...")
                for file in os.listdir(old_path):
                    shutil.move(os.path.join(old_path, file), os.path.join(new_path, file))
                os.rmdir(old_path)
        elif os.path.exists(new_path):
            print(f"Folder '{target_name}' already exists. Good.")
        else:
            print(f"Warning: Source folder '{current_name}' not found. Did you already rename it?")

    print("\n--- 2. Converting PGM to PNG ---")
    total_converted = 0
    
    for class_name in FOLDER_MAP.values(): # Look in 'clean' and 'dirty'
        class_path = os.path.join(BASE_DIR, class_name)
        if not os.path.exists(class_path):
            continue
            
        files = os.listdir(class_path)
        for filename in files:
            if filename.lower().endswith(".pgm"):
                pgm_path = os.path.join(class_path, filename)
                png_path = os.path.join(class_path, os.path.splitext(filename)[0] + ".png")
                
                try:
                    with Image.open(pgm_path) as img:
                        img.save(png_path, "PNG")
                    os.remove(pgm_path) # Remove original
                    total_converted += 1
                except Exception as e:
                    print(f"Error converting {filename}: {e}")

    print("-" * 30)
    print(f"Conversion Complete! Total images ready: {total_converted}")

    # Final Check
    for class_name in ["clean", "dirty"]:
        path = os.path.join(BASE_DIR, class_name)
        if os.path.exists(path):
            count = len([f for f in os.listdir(path) if f.endswith(".png")])
            print(f"  Folder '{class_name}': {count} PNG files")
        else:
            print(f"  ⚠️ Folder '{class_name}' is MISSING.")

if __name__ == "__main__":
    prepare()