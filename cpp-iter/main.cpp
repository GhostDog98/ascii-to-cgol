#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <bitset>
#include <format>
#include <random>
#include <atomic>
#include <signal.h>
#include "./lifelib/pattern2.h"
using namespace std;

atomic<bool> running(true);

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

unordered_map<char, vector<string>> readBetweenMarkers(const string& filePath, const string& startMarker = "STARTCHAR", const string& endMarker = "ENDCHAR") {
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

string conway_encode(const string& chars, const vector<string>& masterDictionary) { // 99% of time
    string result = translateFontFile(chars, masterDictionary); // 52% of time

    // Pre-allocate exactly 16 rows
    vector<string> lines(16);
    int x_max = 0;
    int y_max = 0;

    istringstream stream(result);
    string line;

    // Process each line from the translated font data
    while (getline(stream, line) && y_max < 16) { // Ensure `lines` never exceeds 16 rows
        if (!line.empty() && line.find('1') != string::npos) {
            x_max = max(x_max, static_cast<int>(line.size()));

            // Replace '0' → 'b' and '1' → 'o'
            for (char& ch : line) {
                ch = (ch == '0') ? 'b' : 'o';
            }

            // Store the encoded line in pre-allocated vector
            lines[y_max++] = runLengthEncode(line); // 22% of time
        }
    }

    // Create the final RLE-formatted output
    ostringstream final_stream;
    //final_stream << "#C Generated by ASCII-to-Conway program\n";
    //final_stream << "#r 23/3\n";
    //final_stream << format("x = {}, y = {}\n", x_max, y_max);

    // Output encoded lines, separating rows with '$'
    for (int i = 0; i < y_max; ++i) {
        final_stream << lines[i];
        if (i < y_max - 1) {
            final_stream << "$";
        }
    }
    final_stream << "$$$";

    return final_stream.str();
}



const string CHARACTERS = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
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

void handle_signal(int signal) {
    if (signal == SIGINT) {
        running = false;
    }
}

int main() {
    
    vector<string> masterDictionary = form_translator("font-trimmed.bdf");
/*
    // make a very large string for us to use
    std::string str = "test123;-!~.()@#";
    int repeatCount = 10000;

    // Efficient way using string concatenation
    std::ostringstream result;
    for (int i = 0; i < repeatCount; ++i) {
        result << str;
    }
    cerr << "generated string..." << endl;
    std::string repeated = result.str();
    cout << conway_encode(repeated, masterDictionary) << endl;

    try {
        cout.precision(3);
        signal(SIGINT, handle_signal);

        size_t testLength = 12;
        size_t totalChars = 0;
        size_t iterations = 0;

        auto start = chrono::high_resolution_clock::now();
        cout << "Press Ctrl+C to stop the test." << endl;

        while (running) {
            string randomStr = random_string(testLength);
            string encoded = conway_encode(randomStr, masterDictionary);
            totalChars += randomStr.size();
            ++iterations;

            if (iterations % 1000 == 0) {
                auto now = chrono::high_resolution_clock::now();
                chrono::duration<double> elapsed = now - start;
                double charsPerSecond = totalChars / elapsed.count() / 1000;
                cout << "Iterations: " << iterations
                     << ", Total KiloChars: " << totalChars / 1000
                     << ", Performance: " << charsPerSecond
                     << " kc/sec\r" << flush;
            }
        }
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}