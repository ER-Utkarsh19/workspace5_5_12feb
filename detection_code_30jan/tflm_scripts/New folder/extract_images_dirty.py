import re
import os
import binascii

# Configuration
LOG_FILE = 'monitor_logs_dirty.txt'
OUTPUT_DIR = 'extracted_images_dirty'
WIDTH = 96
HEIGHT = 96
# 96x96 pixels = 9216 bytes. In Hex, that is 18432 characters.
EXPECTED_CHARS = 18432 

def force_extract():
    if not os.path.exists(OUTPUT_DIR):
        os.makedirs(OUTPUT_DIR)

    print(f"Reading {LOG_FILE}...")
    try:
        with open(LOG_FILE, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
    except FileNotFoundError:
        print("Error: monitor_logs.txt not found.")
        return

    # 1. Clean the content (Remove timestamps, debug info, newlines)
    # We only keep Hex characters (0-9, A-F)
    print("Cleaning log data...")
    clean_content = re.sub(r'[^0-9a-fA-F]', '', content)

    # 2. Scan for chunks of exactly 18432 characters
    print(f"Scanning for images (looking for blocks of {EXPECTED_CHARS} hex chars)...")
    
    # We will slide through the cleaned text looking for headers or just valid blocks
    # Since P5 headers are removed in cleaning, we just chunk the hex data
    
    # Find specific marker "IMAGESTART" hex equivalent isn't reliable.
    # Instead, we rely on the specific boost pattern visually or just chunking.
    
    # Simple chunking strategy:
    total_len = len(clean_content)
    count = 0
    
    # We scan for the "magic" pattern. 
    # Usually huge blocks of hex appear together.
    # Let's try to split by the known log markers if possible, 
    # but since we cleaned them, let's just grab chunks.
    
    # BETTER APPROACH: Use the original content to find blocks between "IMAGE_START" manually
    # If regex failed, let's try a looser regex
    
    raw_matches = re.findall(r'IMAGE_START(.*?)IMAGE_END', content, re.DOTALL)
    
    if len(raw_matches) < 5:
        print("Standard tags missing. Attempting raw hex recovery...")
        # If tags are missing, we assume the log is MOSTLY image data.
        # We search for long continuous strings of hex digits.
        long_hex_strings = re.findall(r'[0-9a-fA-F]{18000,}', content)
        raw_matches = long_hex_strings

    print(f"Found {len(raw_matches)} potential data blocks.")

    for i, hex_block in enumerate(raw_matches):
        # Clean JUST this block
        clean_hex = re.sub(r'[^0-9a-fA-F]', '', hex_block)
        
        # If it's larger than expected, truncate it. If smaller, skip.
        if len(clean_hex) >= EXPECTED_CHARS:
            valid_hex = clean_hex[:EXPECTED_CHARS]
            
            try:
                byte_data = binascii.unhexlify(valid_hex)
                filename = f"{OUTPUT_DIR}/image_{i}.pgm"
                with open(filename, 'wb') as f_out:
                    header = f"P5\n{WIDTH} {HEIGHT}\n255\n".encode('ascii')
                    f_out.write(header)
                    f_out.write(byte_data)
                print(f"Saved {filename}")
                count += 1
            except Exception as e:
                print(f"Error saving image {i}: {e}")
        else:
             print(f"Block {i} too small: {len(clean_hex)} chars (Expected {EXPECTED_CHARS})")

    print(f"\nExtraction complete! Saved {count} images.")

if __name__ == '__main__':
    force_extract()