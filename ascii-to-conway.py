import sys, unicodedata
import numpy as np

def read_between_markers(file_path, start_marker="STARTCHAR", end_marker="ENDCHAR", onlyascii = True):
    contents = {}
    current_key = None
    current_section = []
    line_count = 0
    with open(file_path, 'r', encoding='UTF-8') as file:
        for line in file:
            line_count += 1
            if onlyascii == True and line_count > 2975:
                break

            line = line.strip()  # Remove leading/trailing whitespace
            
            if line.startswith(start_marker):
                if current_key is not None:  # Store previous section if it exists
                    contents[current_key] = current_section
                
                current_key = chr(int(line[len(start_marker):].strip()[2:], 16))  # Extract key
                current_section = []  # Start a new section
            elif line == end_marker and current_key is not None:
                contents[current_key] = current_section
                current_key = None  # Reset for next section
            elif current_key is not None:  # Collect lines between markers
                current_section.append(line)

    # Handle the last section if the file ends without an ENDCHAR
    if current_key is not None:
        contents[current_key] = current_section

    return contents

def form_translator(file_to_read):
    sections = read_between_markers(file_to_read)
    translated_form = {}

    for key, value in sections.items():
        if unicodedata.category(key) == "Cc":
            continue  # Skip null character and non-printable keys

        # Get the bounding width from the third element
        bounding_width = int(value[3].split()[2])

        # Translate each code into binary
        translated_form[key] = [
            list(bin(int(code, 16))[2:].zfill(bounding_width - 8))  # Remove '0b' and pad to width
            for code in value[5:]  # Skip first five lines
        ]
    return translated_form

def translate_font_file(file_path, chars):
    master_dictionary = form_translator(file_path)

    # Collect binary arrays for all specified characters
    char_arrays = [master_dictionary.get(char, []) for char in chars]

    # Find the maximum length to ensure we combine properly
    max_length = max(len(arr) for arr in char_arrays)

    # Create combined 2D array
    combined_array = []
    for i in range(max_length):
        combined_row = []
        for arr in char_arrays:
            # Get the i-th row from the character array, filling with empty strings if out of bounds
            row = arr[i] if i < len(arr) else [''] * len(arr[0]) if arr else []  # Fill with empty if out of bounds
            combined_row.extend(row)  # Merge the rows into combined_row

        combined_array.append(combined_row)  # Append to the combined array


    # Convert combined_array to a NumPy array of type string
    combined_array_np = np.array(combined_array, dtype=str)

    # Return the resulting combined array without extra newlines
    return '\n'.join([''.join(row) for row in combined_array_np.tolist()])  # Ensure to convert to list


def rle_encode(data):
    if not data:
        return ''

    encoding = []
    prev_char = data[0]
    count = 1

    for char in data[1:]:
        if char != prev_char:
            encoding.append(f"{count}{prev_char}")
            count = 1
            prev_char = char
        else:
            count += 1
    encoding.append(f"{count}{prev_char}")  # Finish off the encoding
    return ''.join(encoding)

def conway_rle_translate(result):
    result_line_array = result.split()
    x_max = len(result_line_array[0])  # assume a uniform length
    y_max = len(result_line_array)
    
    first_lines = (
        "#C Generated by Lilly's ascii-to-conway program\n"
        "#r 23/3\n"
        f"x = {y_max}, y = {x_max}"
    )
    
    rest_of_lines = []
    for line in result_line_array:
        if "1" in line:
            line = line.rstrip('0')  # remove end 0's
        line = line.replace('0', 'b').replace('1', 'o')  # replace dead and live cells
        rest_of_lines.append(rle_encode(line))

    print(first_lines)
    for line in rest_of_lines:
        if line:
            print(line + "$", end='')
    print("\n")


if __name__ == "__main__":
    # Example usage
    file_path = 'font.bdf'  # Replace with your actual file path

    #chars_to_translate = 'testing'  # The characters you want to combine
    chars_to_translate = sys.argv[1]
    result = translate_font_file(file_path, chars_to_translate)
    conway_rle_translate(result)
