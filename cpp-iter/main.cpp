#include <iostream>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <bitset>
#include <format>
#include <random>
using namespace std;

atomic<bool> running(true);

string runLengthEncode(const string & str){
    int i = str.size();
    string letters;

    for (int j = 0; j < i; ++j){
        int count = 1;
        while (str[j] == str[j+1]){
            count++;
            j++;
        }
        letters += std::to_string(count);
        letters.push_back(str[j]);
    }
    return letters;
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
        

        // Trim leading and trailing whitespace
        line.erase(0, line.find_first_not_of(" \t\n\r"));
        line.erase(line.find_last_not_of(" \t\n\r") + 1);

        if (line.rfind(startMarker, 0) == 0) { // Line starts with startMarker
            if (currentKey != '\0') {
                contents[currentKey] = currentSection;
            }

            // Extract key from marker
            string hexStr = line.substr(startMarker.length());
            hexStr.erase(0, hexStr.find_first_not_of(" \t\n\r")); // Trim whitespace around marker
            hexStr.erase(0, hexStr.find_first_of("+")+1); // remove "U+"

            int hexValue = stoi(hexStr, nullptr, 16); // convert B16 string to b10 number
            
            currentKey = static_cast<char>(hexValue); // convert to char
            currentSection.clear();
        } else if (line == endMarker && currentKey != '\0') {
            contents[currentKey] = currentSection;
            currentKey = '\0';
        } else if (currentKey != '\0') {
            currentSection.push_back(line);
        }
    }

    // Handle the last section if the file ends without an ENDCHAR
    if (currentKey != '\0') {
        contents[currentKey] = currentSection;
    }

    file.close();
    return contents;
}

unordered_map<char, string> form_translator(string file_to_read){
    unordered_map<char, vector<string>> sections = readBetweenMarkers(file_to_read);
    unordered_map<char, string> translated_sections;

    for (const auto& [key, lines] : sections) { // for each key-value pair (for key, value in dict:)
        string transformed_part;
        for (size_t i = 5; i < 5+16; ++i) { // for each section of codes. Each of these are 16 long, and 5 lines in
            int line_value = stoi(lines[i], nullptr, 16); // convert from base16 string to base10 int
            string binary = bitset<8>(line_value).to_string() + "\n"; // then convert to binary, and add a newline
            transformed_part.append(binary); // append to our final string
        }
    
        translated_sections[key] = transformed_part; // assign that multiline string to the master dict
    }

    return translated_sections;
}



string translateFontFile(const string& chars, const unordered_map<char, string>& masterDictionary) {
    // Vector to store rows of the output
    vector<string> combinedRows;

    // Process each character in the input
    for (char c : chars) {
        if (masterDictionary.count(c)) {
            istringstream stream(masterDictionary.at(c));
            string line;
            size_t rowIndex = 0;

            // Read the binary string row by row
            while (getline(stream, line)) {
                if (rowIndex >= combinedRows.size()) {
                    combinedRows.emplace_back(); // Add a new row if needed
                }
                combinedRows[rowIndex] += line; // Append the line horizontally
                ++rowIndex;
            }
        } else {
            // Handle missing characters with blank rows
            size_t charHeight = 16; // Assuming each character's binary representation is 16 rows
            size_t charWidth = 8;  // Assuming each character's binary representation is 8 bits wide
            for (size_t rowIndex = 0; rowIndex < charHeight; ++rowIndex) {
                if (rowIndex >= combinedRows.size()) {
                    combinedRows.emplace_back(); // Add a new row if needed
                }
                combinedRows[rowIndex] += string(charWidth, '0'); // Fill with blanks
            }
        }
    }

    // Combine rows into a single string
    ostringstream result;
    for (const auto& row : combinedRows) {
        result << row << '\n';
    }
    return result.str();
}


string conway_encode(const string& chars, unordered_map<char, string> masterDictionary){
    string result = translateFontFile(chars, masterDictionary);

    // Vector to store the lines
    vector<string> lines;

    // Use a stringstream to split by lines
    istringstream stream(result);
    string line;
    int x_max = 0, y_max = 0;

    while (getline(stream, line)) {
        y_max++;
        x_max = max(x_max, static_cast<int>(line.size()));
        if(line.find("1") == string::npos){
            line.erase(0, line.find_first_not_of('0')); // erase leading zeros
        }
        replace(line.begin(), line.end(), '0', 'b');
        replace(line.begin(), line.end(), '1', 'o');
        lines.push_back(runLengthEncode(line)); // Add each line to the vector
    }

    
    // Combine all rows into the final transfer array
    ostringstream final_stream;

    final_stream << (
        format(
            "#C Generated by Lilly's ascii-to-conway\
            program\n#r 23/3\nx = {}, y = {}\n", x_max, y_max
        )
    );
    for (const auto& line : lines) {
        final_stream << line << '$';
    }
    final_stream << endl;
    return final_stream.str();
}

const std::string CHARACTERS = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
static std::random_device rd;
static std::mt19937 generator(rd());
static std::uniform_int_distribution<size_t> distribution(0, CHARACTERS.size() - 1);

std::string random_string(std::size_t length) {
    std::string result(length, '0');
    for (auto& ch : result) {
        ch = CHARACTERS[distribution(generator)];
    }
    return result;
}


// Signal handler to stop the loop
void handle_signal(int signal) {
    if (signal == SIGINT) {
        running = false;
    }
}

int main() {

    // cache this value so we only need to read the file and parse it once...
    unordered_map <char, string> masterDict = form_translator("font-trimmed.bdf");    
    try {

        cout.precision(3);
        signal(SIGINT, handle_signal);

        size_t testLength = 12; // Length of random strings
        size_t totalChars = 0;
        size_t iterations = 0;

        // Start timer
        auto start = chrono::high_resolution_clock::now();

        cout << "Press Ctrl+C to stop the test." << endl;

        // Run continuously until Ctrl+C is pressed
        while (running) {
            string randomStr = random_string(testLength);
            string encoded = conway_encode(randomStr, masterDict);
            totalChars += randomStr.size();
            ++iterations;

            // Update performance every 1000 iterations
            if (iterations % 1000 == 0) {
                auto now = chrono::high_resolution_clock::now();
                chrono::duration<double> elapsed = now - start;
                double charsPerSecond = totalChars / elapsed.count();
                charsPerSecond = charsPerSecond / 1000;                
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
}
