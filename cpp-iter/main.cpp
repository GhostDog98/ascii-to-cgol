#include <signal.h>
#include <signal.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <bitset>
#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstring>
#include <climits>
#include "master.hpp"
#include <unistd.h>
using namespace std;

atomic<bool> running(true);
atomic<size_t> totalPatterns(0); // global state for total patterns output
mutex coutMutex;

string runLengthEncode(const string& str) {
    int n = str.size();
    string encoded;

    for (int i = 0; i < n; ++i) {
        int count = 1;
        while (i + 1 < n && str[i] == str[i + 1]) {
            ++count;
            ++i;
        }
        encoded += to_string(count) + str[i];
    }
    return encoded;
}

unordered_map<char, vector<string>> readBetweenMarkers(const string& filePath, const string& startMarker = "STARTCHAR",
                                                       const string& endMarker = "ENDCHAR") {
    unordered_map<char, vector<string>> contents;
    ifstream file(filePath);
    if (!file.is_open()) {
        throw runtime_error("Failed to open the file: " + filePath);
    }

    string line;
    char currentKey = '\0';
    vector<string> currentSection;

    while (getline(file, line)) {
        line.erase(0, line.find_first_not_of(" \t\n\r"));
        line.erase(line.find_last_not_of(" \t\n\r") + 1);

        if (line.rfind(startMarker, 0) == 0) {
            if (currentKey != '\0') {
                contents[currentKey] = currentSection;
            }

            string hexStr = line.substr(startMarker.length());
            hexStr.erase(0, hexStr.find_first_not_of(" \t\n\r"));
            hexStr.erase(0, hexStr.find_first_of("+") + 1);

            int hexValue = stoi(hexStr, nullptr, 16);
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

vector<string> form_translator(const string& file_to_read) {
    vector<string> translated_sections(95);
    unordered_map<char, vector<string>> sections = readBetweenMarkers(file_to_read);

    for (const auto& [key, lines] : sections) {
        string transformed_part;
        for (size_t i = 5; i < 5 + 16; ++i) {
            int line_value = stoi(lines[i], nullptr, 16);
            string binary = bitset<8>(line_value).to_string() + "\n";
            transformed_part.append(binary);
        }
        translated_sections[key - 32] = transformed_part;
    }

    return translated_sections;
}

string translateFontFile(const string& chars, const vector<string>& masterDictionary) {
    vector<string> combinedRows;

    for (char c : chars) {
        if (c >= 32 && c <= 126) {
            const string& charData = masterDictionary[c - 32];
            istringstream stream(charData);
            string line;
            size_t rowIndex = 0;

            while (getline(stream, line)) {
                if (rowIndex >= combinedRows.size()) {
                    combinedRows.emplace_back();
                }
                combinedRows[rowIndex] += line;
                ++rowIndex;
            }
        } else {
            size_t charHeight = 16;
            size_t charWidth = 8;
            for (size_t rowIndex = 0; rowIndex < charHeight; ++rowIndex) {
                if (rowIndex >= combinedRows.size()) {
                    combinedRows.emplace_back();
                }
                combinedRows[rowIndex] += string(charWidth, '0');
            }
        }
    }

    ostringstream result;
    for (const auto& row : combinedRows) {
        result << row << '\n';
    }
    return result.str();
}

string conway_encode(const string& chars, const vector<string>& masterDictionary) {  // 99% of time
    string result = translateFontFile(chars, masterDictionary);                      // 52% of time

    // Pre-allocate exactly 16 rows
    vector<string> lines(16);
    int x_max = 0;
    int y_max = 0;

    istringstream stream(result);
    string line;

    // Process each line from the translated font data
    while (getline(stream, line) && y_max < 16) {  // Ensure `lines` never exceeds 16 rows
        if (!line.empty() && line.find('1') != string::npos) {
            x_max = max(x_max, static_cast<int>(line.size()));

            // Replace '0' → 'b' and '1' → 'o'
            for (char& ch : line) {
                ch = (ch == '0') ? 'b' : 'o';
            }

            // Store the encoded line in pre-allocated vector
            lines[y_max++] = runLengthEncode(line);  // 22% of time
        }
    }

    // Create the final RLE-formatted output
    ostringstream final_stream;
    // final_stream << "#C Generated by ASCII-to-Conway program\n";
    // final_stream << "#r 23/3\n";
    // final_stream << format("x = {}, y = {}\n", x_max, y_max);

    // Output encoded lines, separating rows with '$'
    for (int i = 0; i < y_max; ++i) {
        final_stream << lines[i];
        if (i < y_max - 1) {
            final_stream << "$";
        }
    }
    final_stream << "!";

    return final_stream.str();
}

const string CHARACTERS =
    " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
random_device rd;
mt19937 generator(rd());
uniform_int_distribution<size_t> distribution(0, CHARACTERS.size() - 1);

string random_string(size_t length) {
    string result(length, '0');
    for (auto& ch : result) {
        ch = CHARACTERS[distribution(generator)];
    }
    return result;
}

void generate_combination(size_t number, size_t length, const string& characters, string& out) {
    size_t base = characters.size();
    for (size_t i = 0; i < length; ++i) {
        out[length - i - 1] = characters[number % base];
        number /= base;
    }
}


bool next_combination(string& current, const string& characters) {
    for (size_t i = current.size(); i-- > 0;) {
        if (current[i] != characters.back()) {
            current[i] = characters[characters.find(current[i]) + 1];
            return true;
        }
        current[i] = characters.front();
    }
    return false; // Reached the final combination
}

void handle_signal(int signal) {
    if (signal == SIGINT) {
        running = false;
    }
}

void print_speed(size_t iterationsDone, size_t totalCharsDone, double charsPerSecondDone) {
    const char* prefixes[] = {"c", "kc", "mc", "gc", "tc", "pc", "ec", "zc", "yc"};
    size_t prefixCount = sizeof(prefixes) / sizeof(prefixes[0]);

    size_t prefixIndex = 0;

    while (charsPerSecondDone >= 1000 && prefixIndex < prefixCount - 1) {
        charsPerSecondDone /= 1000;
        totalCharsDone /= 1000;
        ++prefixIndex;
    }

    cerr << "Iterations: " << iterationsDone << ", Total Chars: " << totalCharsDone << prefixes[prefixIndex]
         << ", Performance: " << charsPerSecondDone << " " << prefixes[prefixIndex] << "/sec" << endl
         << flush;
}

void dumpMasterDictionary(const vector<string>& masterDictionary, const string& outputFile) {
    // Dumps the dictionary to a hpp file so we can compile it
    ofstream outFile(outputFile);
    if (!outFile.is_open()) {
        throw runtime_error("Failed to open file for writing.");
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

mutex statsMutex;
size_t totalChars = 0;
size_t iterations = 0;

void workerFunction(size_t start, size_t end, const vector<string>& masterDictionary, size_t testLength, const string& characters) {
    string currentPattern(testLength, characters.front());

    for (size_t i = start; i < end && running; ++i) {
        generate_combination(i, testLength, characters, currentPattern);
        string encoded = conway_encode(currentPattern, masterDictionary);

        {
            lock_guard<mutex> lock(coutMutex);
            cout << "x = 0, y = 0, rule=B3/S23\n" << encoded << endl;
        }

        {
            lock_guard<mutex> lock(statsMutex);
            totalChars += currentPattern.size();
            ++iterations;
        }
    }
}




// ./recompile --symmetry letters_stdin --cuda
// then `./conway -l <n> | ./apgluxe -t 1 -n 999999
int main(int argc, char* argv[]) {
    size_t testLength = 2;  // Default value for test length
    int opt;

    // Option parsing
    while ((opt = getopt(argc, argv, "l:")) != -1) {
        switch (opt) {
            case 'l':
                testLength = stoi(optarg);
                break;
            default:
                cerr << "Usage: " << argv[0] << " [-l testLength]" << endl;
                return EXIT_FAILURE;
        }
    }


    size_t threadCount = thread::hardware_concurrency();
    size_t totalCombinations = std::pow(CHARACTERS.size(), testLength);

    //vector<string> masterDictionary = form_translator("your_font_file.txt");
    vector<thread> workers;
    size_t chunkSize = totalCombinations / threadCount;

    for (size_t i = 0; i < threadCount; ++i) {
        size_t start = i * chunkSize;
        size_t end = (i == threadCount - 1) ? totalCombinations : (i + 1) * chunkSize;
        workers.emplace_back(workerFunction, start, end, cref(masterDictionary), testLength, cref(CHARACTERS));
    }

    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    return 0;
}