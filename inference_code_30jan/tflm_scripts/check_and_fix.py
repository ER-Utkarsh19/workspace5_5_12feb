import os
from PIL import Image

DATASET_DIR = "dataset"
CLASSES = ["clean", "dirty"]

def fix_images():
    if not os.path.exists(DATASET_DIR):
        print(f"ERROR: '{DATASET_DIR}' folder is missing!")
        return

    total_pngs = 0
    
    print("--- Checking Dataset ---")
    for category in CLASSES:
        folder_path = os.path.join(DATASET_DIR, category)
        if not os.path.exists(folder_path):
            print(f"Creating missing folder: {folder_path}")
            os.makedirs(folder_path)
            continue
            
        files = os.listdir(folder_path)
        png_count = 0
        pgm_count = 0
        
        for f in files:
            full_path = os.path.join(folder_path, f)
            
            # Convert PGM to PNG
            if f.lower().endswith(".pgm"):
                try:
                    pgm_count += 1
                    with Image.open(full_path) as img:
                        new_name = os.path.splitext(f)[0] + ".png"
                        img.save(os.path.join(folder_path, new_name))
                    os.remove(full_path) # Delete old file
                except Exception as e:
                    print(f"Error converting {f}: {e}")

            # Count PNGs
            if f.lower().endswith(".png"):
                png_count += 1

        print(f"Folder '{category}': Found {png_count} PNGs (Converted {pgm_count} PGMs)")
        total_pngs += png_count

    print("-" * 30)
    if total_pngs == 0:
        print("❌ ERROR: No images found! Run extract_images.py first.")
    else:
        print(f"✅ READY: Found {total_pngs} valid images. You can run train_model.py now.")

if __name__ == "__main__":
    fix_images()