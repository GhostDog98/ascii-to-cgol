#include <atomic>
#include <cerrno>
#include <cmath>
#include <csignal>
#include <cstring>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <fcntl.h>
#include <chrono>
#include <iostream>
#include <string>
#include <unordered_map>
//#include "./libs/hypersonic-rle-kit/src/rle.h"
#include "rle.h"
#include <condition_variable>

#include "master.hpp"

using namespace std;

atomic<bool> running(true);
atomic<size_t> totalPatterns(0); // global state for total patterns output
mutex coutMutex;
const string CHARACTERS = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

class ScopedSignalHandler {
    struct sigaction oldAction;
    int signalNumber;

public:
    ScopedSignalHandler(int signum, void (*handler)(int)) : signalNumber(signum) {
        struct sigaction newAction{};
        newAction.sa_handler = handler;
        sigemptyset(&newAction.sa_mask);
        newAction.sa_flags = 0;
        sigaction(signum, &newAction, &oldAction);
    }
    ~ScopedSignalHandler() {
        sigaction(signalNumber, &oldAction, nullptr);
    }
};

class ThreadPool {
    vector<thread> threads;
    queue<function<void()>> taskQueue;
    mutex queueMutex;
    condition_variable cv;
    atomic<bool> stop;

public:
    ThreadPool(size_t threadCount) : stop(false) {
        for (size_t i = 0; i < threadCount; ++i) {
            threads.emplace_back([this] {
                while (true) {
                    function<void()> task;
                    {
                        unique_lock<mutex> lock(queueMutex);
                        cv.wait(lock, [this] { return stop || !taskQueue.empty(); });
                        if (stop && taskQueue.empty()) return;
                        task = std::move(taskQueue.front());
                        taskQueue.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            unique_lock<mutex> lock(queueMutex);
            stop = true;
        }
        cv.notify_all();
        for (thread& t : threads) {
            t.join();
        }
    }

    void enqueueTask(function<void()> task) {
        {
            unique_lock<mutex> lock(queueMutex);
            taskQueue.push(move(task));
        }
        cv.notify_one();
    }
};

void handle_signal(int signal) {
    if (signal == SIGINT) {
        running = false;
    }
}
/*
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
}*/

// Define function types for compression
using CompressFunc = std::function<uint32_t(const uint8_t*, const uint32_t, uint8_t*, const uint32_t)>;
using BoundsFunc = std::function<uint32_t(const uint32_t)>;

// Create a helper function to initialize the map
std::unordered_map<std::string, std::pair<CompressFunc, BoundsFunc>> initializeCompressionMethods() {
    std::unordered_map<std::string, std::pair<CompressFunc, BoundsFunc>> methods;

    methods["rle8_low_entropy"] = {rle8_low_entropy_compress, rle8_low_entropy_compress_bounds};
    methods["rle8_low_entropy_short"] = {rle8_low_entropy_short_compress, rle8_low_entropy_short_compress_bounds};
    methods["rle8_multi"] = {rle8_multi_compress, rle_compress_bounds};
    methods["rle8_single"] = {rle8_single_compress, rle_compress_bounds};
    // Add more compression methods here as needed

    return methods;
}

std::string run_length_encode(const std::string& text_to_encode, const std::string& compression_type) {
    static auto compression_methods = initializeCompressionMethods();

    // Find the correct compression method
    const auto it = compression_methods.find(compression_type);
    if (it == compression_methods.end()) {
        throw std::invalid_argument("Unsupported compression type");
    }

    const CompressFunc& compress_func = it->second.first;
    const BoundsFunc& bounds_func = it->second.second;

    uint8_t* pIn = (uint8_t*)text_to_encode.data();
    uint32_t inSize = static_cast<uint32_t>(text_to_encode.size());

    uint32_t compressedBufferSize = bounds_func(inSize);
    std::string compressedData(compressedBufferSize, '\0');
    uint32_t compressedSize = compress_func(pIn, inSize, (uint8_t*)compressedData.data(), compressedBufferSize);

    if (compressedSize == 0) {
        throw std::runtime_error("Compression failed");
    }

    compressedData.resize(compressedSize);
    return compressedData;
}

/*
string runLengthEncode(const string& input){
    // Convert to byte array and calc size
    const uint8_t* pIn = reinterpret_cast<const uint8_t*>(input.data());
    const size_t inSize = input.size();

    // prep output buffer
    uint8_t* pOut = new uint8_t[inSize];
    size_t pOutIndex = 0;

    uint8_t maxFreqSymbol = "o";
    int64_t compressedSize = CONCAT3(rle8_, CODEC, compress_single_sse2)(pIn, inSize, pOut, &pOutIndex, maxFreqSymbol, ...moreArgs);

}*/



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

string conway_encode(const string& chars, const vector<string>& masterDictionary) {
    string result = translateFontFile(chars, masterDictionary);
    vector<string> lines(16);
    int x_max = 0;
    int y_max = 0;

    istringstream stream(result);
    string line;
    // 6500 per second when rle'd, 6000 when not
    while (getline(stream, line) && y_max < 16) {
        if (!line.empty() && line.find('1') != string::npos) {
            x_max = max(x_max, static_cast<int>(line.size()));
            for (char& ch : line) {
                ch = (ch == '0') ? 'b' : 'o';
            }
            lines[y_max++] = run_length_encode(line, "rle8_single");//runLengthEncode(line);
        }
    }

    ostringstream final_stream;
    //final_stream << "x=0,y=0,r=B3/S23\n";
    final_stream << "x\n"; // why does this work lmfao
    for (int i = 0; i < y_max; ++i) {
        final_stream << lines[i];
        if (i < y_max - 1) {
            final_stream << "$";
        }
    }
    final_stream << "!\n";

    return final_stream.str();
}

void generate_combination(size_t number, size_t length, const string& characters, string& out) {
    size_t base = characters.size();
    for (size_t i = 0; i < length; ++i) {
        out[length - i - 1] = characters[number % base];
        number /= base;
    }
}


void workerFunction(size_t start, size_t end, const vector<string>& masterDictionary,
                    size_t testLength, const string& characters,
                    const vector<const char*>& apgluxeArgs, size_t id) {
    string currentPattern(testLength, characters.front());

    // Construct the command string for popen
    //string command = "./apgluxe --rule=life"; 
    string command = "cat";
    for (const char* arg : apgluxeArgs) {
        if (arg != nullptr) {
            command += " ";
            command += arg;
        }
    }

    FILE* pipe = popen(command.c_str(), "w");
    if (!pipe) {
        perror("popen");
        cout << "Error opening the pipe..." << endl;
        return;
    }

    // Get file descriptor from pipe for next step
    int pipefd = fileno(pipe);
    if(pipefd == -1){
        perror("fileno");
        return;
    }

    // Set to non-blocking writes
    int flags = fcntl(pipefd, F_GETFL, 0);
    if( flags == -1 || fcntl(pipefd, F_SETFL, flags | O_NONBLOCK) == 1){
        perror("fcntl");
        return;
    }

    for (size_t i = start; i < end && running; ++i) {
        generate_combination(i, testLength, characters, currentPattern);
        string output = conway_encode(currentPattern, masterDictionary);
        size_t written = 0;

        // Non blocking write allows us to sleep for a bit if our write would block the child
        while (written < output.size() && running) {
            ssize_t result = write(pipefd, output.c_str() + written, output.size() -written);
            if (result == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                this_thread::sleep_for(chrono::milliseconds(10)); //sleep then try again
            } else if (result == -1){
                perror("fwrite");
                break;
            }else {
                written += result;
            }
        }
        fflush(pipe); // Ensure data is flushed after each pattern
    }

    if (pclose(pipe) != 0) {
        lock_guard<mutex> lock(coutMutex);
        cerr << "Thread " << id << " failed. Error closing the pipe." << endl;
    } else {
        lock_guard<mutex> lock(coutMutex);
        cout << "Thread " << id << " finished successfully." << endl;
    }
}





int main(int argc, char* argv[]) {
    
    size_t testLength = 2;
    int opt;

    if (argc <= 1) {
        cerr << "Usage: " << argv[0] << " -l <testlength> -- <apgluxe arguments>" << endl;
        cerr << "Example: \n" << "./conway -l 5 -- -t 1 -n 99" << endl;
        return EXIT_FAILURE;
    }

    ScopedSignalHandler sigintHandler(SIGINT, handle_signal);

    vector<const char*> apgluxeArgs = {""};
    while ((opt = getopt(argc, argv, "l:")) != -1) {
        if (opt == 'l') {
            testLength = stoi(optarg);
        } else {
            cerr << "Usage: " << argv[0] << " [-l testLength]" << endl;
            return EXIT_FAILURE;
        }
    }
    for (int i = optind; i < argc; ++i) {
        apgluxeArgs.push_back(argv[i]);
    }
    apgluxeArgs.push_back(nullptr);

    if (masterDictionary.empty()) {
        cerr << "Master dictionary is not initialized properly." << endl;
        return EXIT_FAILURE;
    }

    if (access("./apgluxe", X_OK) == -1) {
        cerr << "apgluxe is not executable." << endl;
        return EXIT_FAILURE;
    }

    size_t threadCount = max(2U, thread::hardware_concurrency());
    cout << "Starting " << threadCount << " threads..." << endl;

    size_t totalCombinations = pow(CHARACTERS.size(), testLength);
    cout << "Detected " << totalCombinations << " number of combinations for length " << testLength << endl;
    size_t chunkSize = totalCombinations / threadCount;
    size_t remainder = totalCombinations % threadCount;

    std::vector<std::thread> threads;
    size_t start = 0;

    for (size_t i = 0; i < threadCount; ++i) {
        size_t end = start + chunkSize + (i < remainder ? 1 : 0);
        cout << "Starting thread " << (i+1) << " for tasks " << (start+1) << "-" << (end) << endl;

        threads.emplace_back([start, end, testLength, &characters = CHARACTERS, &apgluxeArgs, i] {
            workerFunction(start, end, masterDictionary, testLength, characters, apgluxeArgs, i);
        });

        start = end;
    }

    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "All threads completed.\n";
    
    return 0;
}

