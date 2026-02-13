import binascii

def convert_hex_to_pgm(hex_text_file, output_filename):
    with open(hex_text_file, 'r') as f:
        # Remove "IMAGE_START", "IMAGE_END", and newlines
        lines = f.readlines()
        clean_hex = "".join([l.strip() for l in lines if "IMAGE" not in l])

    # Convert hex string back to raw bytes
    raw_pixels = binascii.unhexlify(clean_hex)

    # Write as PGM
    with open(output_filename, 'wb') as f:
        f.write(f"P5\n96 96\n255\n".encode())
        f.write(raw_pixels)
    
    print(f"Success! Image saved as {output_filename}")

if __name__ == "__main__":
    # You will manually copy the serial output into 'dump.txt'
    convert_hex_to_pgm('dump2.txt', 'test_view2.pgm')