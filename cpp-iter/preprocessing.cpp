// preprocessing.cpp

#include "preprocessing.hpp"
#include <fstream>
#include <sstream>
#include <bitset>

std::unordered_map<char, std::vector<std::string>> readBetweenMarkers(const std::string& filePath, const std::string& startMarker, const std::string& endMarker) {
    std::unordered_map<char, std::vector<std::string>> contents;
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open the file: " + filePath);
    }

    std::string line;
    char currentKey = '\0';
    std::vector<std::string> currentSection;

    while (getline(file, line)) {
        line.erase(0, line.find_first_not_of(" \t\n\r"));
        line.erase(line.find_last_not_of(" \t\n\r") + 1);

        if (line.rfind(startMarker, 0) == 0) {
            if (currentKey != '\0') {
                contents[currentKey] = currentSection;
            }

            std::string hexStr = line.substr(startMarker.length());
            hexStr.erase(0, hexStr.find_first_not_of(" \t\n\r"));
            hexStr.erase(0, hexStr.find_first_of("+") + 1);

            int hexValue = std::stoi(hexStr, nullptr, 16);
            currentKey = static_cast<char>(hexValue);
            currentSection.clear();
        } else if (line == endMarker && currentKey != '\0') {
            contents[currentKey] = currentSection;
            currentKey = '\0';
        } else if (currentKey != '\0') {
            currentSection.push_back(line);
        }
    }

    if (currentKey != '\0') {
        contents[currentKey] = currentSection;
    }

    file.close();
    return contents;
}

std::vector<std::string> form_translator(const std::string& file_to_read) {
    std::vector<std::string> translated_sections(95);
    std::unordered_map<char, std::vector<std::string>> sections = readBetweenMarkers(file_to_read);

    for (const auto& [key, lines] : sections) {
        std::string transformed_part;
        for (size_t i = 5; i < 5 + 16; ++i) {
            int line_value = std::stoi(lines[i], nullptr, 16);
            std::string binary = std::bitset<8>(line_value).to_string() + "\n";
            transformed_part.append(binary);
        }
        translated_sections[key - 32] = transformed_part;
    }

    return translated_sections;
}

void dumpMasterDictionary(const std::vector<std::string>& masterDictionary, const std::string& outputFile) {
    std::ofstream outFile(outputFile);
    if (!outFile.is_open()) {
        throw std::runtime_error("Failed to open file for writing.");
    }

    outFile << "#pragma once\n\n";
    outFile << "#include <string>\n";
    outFile << "#include <vector>\n\n";
    outFile << "static const std::vector<std::string> preloadedMasterDictionary = {\n";

    for (const auto& entry : masterDictionary) {
        outFile << "    R\"(" << entry << ")\",\n";  // Use raw string literals
    }

    outFile << "};\n";
    outFile.close();
}
