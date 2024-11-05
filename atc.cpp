#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <locale>
#include <codecvt>

std::map<wchar_t, std::vector<std::wstring>> read_between_markers(
    const std::string& file_path,
    const std::wstring& start_marker = L"STARTCHAR",
    const std::wstring& end_marker = L"ENDCHAR") {

    std::map<wchar_t, std::vector<std::wstring>> contents;
    wchar_t current_key = 0; // Initialize current_key
    std::vector<std::wstring> current_section;

    // Set locale to handle wide characters correctly
    std::locale loc("en_US.UTF-8");
    std::wifstream file(file_path);
    file.imbue(loc); // Use the appropriate locale for the file stream

    if (!file.is_open()) {
        std::wcerr << L"Could not open the file: " << file_path.c_str() << std::endl;
        return contents;
    } else {
        std::wcout << L"Opened file..." << std::endl;
    }

    std::wstring line;
    while (std::getline(file, line)) {

        // Check for the start marker
        if (line.find(start_marker) == 0) {
            // Extract key this entry is for as a character
            std::wstring key_str = line.substr(start_marker.length());
            unsigned int key_value;
            std::wstringstream ss;
            ss << std::hex << key_str.substr(2); // Ensure we're treating as hex, e.g. `space` is 0x20
            ss >> key_value;

            // Check if the value is within the valid Unicode range
            if (key_value > 0xFFFF) {
                std::wcerr << L"Key value out of range: " << key_value << std::endl;
                continue;
            }

            current_key = static_cast<wchar_t>(key_value); // Convert to wchar_t
            current_section.clear(); // We're at the start of a record, start a new section

            // Debugging output for the current key
            std::wcout << L"Reading key: " << current_key << L" (" << std::hex << key_value << L")" << std::endl;

        } 
        // Check for the end marker
        else if (line == end_marker && current_key) {
            contents[current_key] = current_section; // Store the current section
            current_key = 0; // Reset for next section

            // Debugging output to confirm storing of the key
            std::wcout << L"Stored key: " << current_key << std::endl;
        } 
        // Collect lines between markers
        else if (current_key) {
            current_section.push_back(line);
        }
    }

    // Handle the last section if the file ends without an ENDCHAR
    if (current_key) {
        std::wcout << L"File ended without endchar for key: " << current_key << L" (" << std::hex << static_cast<unsigned int>(current_key) << L")" << std::endl;
        contents[current_key] = current_section;
    }
    
    file.close();
    std::wcout << L"Closing file..." << std::endl;
    return contents;
}

int main() {
    std::string file_path = "font.bdf"; // Update with the actual file path
    auto translated_form = read_between_markers(file_path);

    // Output the results for demonstration
    for (const auto& pair : translated_form) {
        std::wcout << L"Key: " << std::hex << static_cast<unsigned int>(pair.first) << L" (" << pair.first << L")" << std::endl;
        for (const auto& line : pair.second) {
            std::wcout << L"  " << line << std::endl;
        }
    }

    return 0;
}
