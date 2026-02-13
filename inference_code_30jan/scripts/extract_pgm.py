import os

def extract_pgms(bin_file):
    # The header pattern we are looking for in hex
    # "P5\n96 96\n255\n"
    HEADER = b'P5\n96 96\n255\n'
    IMAGE_SIZE = 96 * 96
    TOTAL_SIZE = len(HEADER) + IMAGE_SIZE

    if not os.path.exists(bin_file):
        print(f"Error: {bin_file} not found!")
        return

    with open(bin_file, 'rb') as f:
        data = f.read()

    print(f"Searching {bin_file} ({len(data)} bytes)...")

    start_pos = 0
    count = 0
    
    while True:
        # Find the next occurrence of the header
        start_pos = data.find(HEADER, start_pos)
        
        if start_pos == -1:
            break  # No more headers found
        
        # Extract the header + the raw pixel data
        pgm_data = data[start_pos : start_pos + TOTAL_SIZE]
        
        # Save to a new file
        filename = f"extracted_frame_{count}.pgm"
        with open(filename, 'wb') as out:
            out.write(pgm_data)
        
        print(f"Found image at offset {start_pos}, saved as {filename}")
        
        # Move past this image to find the next one
        start_pos += TOTAL_SIZE
        count += 1

    print(f"Done! Extracted {count} images.")

if __name__ == "__main__":
    extract_pgms('spiffs_dump.bin')