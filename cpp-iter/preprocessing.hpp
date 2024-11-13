// preprocessing.hpp

#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept> // For runtime_error

std::unordered_map<char, std::vector<std::string>> readBetweenMarkers(const std::string& filePath, const std::string& startMarker = "STARTCHAR", const std::string& endMarker = "ENDCHAR");

std::vector<std::string> form_translator(const std::string& file_to_read);

void dumpMasterDictionary(const std::vector<std::string>& masterDictionary, const std::string& outputFile);
