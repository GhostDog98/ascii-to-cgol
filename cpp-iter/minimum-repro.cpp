#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <sstream>
#include <cstdio>  // For popen and pclose
#include <mutex>


const int NUM_THREADS = 4; // Number of threads
std::mutex io_mutex;       // Mutex for synchronized console output

void runCommandWithData(const std::string& command, const std::string& data) {
    // Open a process with write access to its stdin
    FILE* pipe = popen(command.c_str(), "w");
    if (!pipe) {
        std::cerr << "Failed to open pipe for command: " << command << "\n";
        return;
    }

    // Send data to the process's stdin
    fwrite(data.c_str(), sizeof(char), data.size(), pipe);
    pclose(pipe); // Close the pipe and wait for the process to finish
}

void workerFunction(int id) {
    // Generate data
    std::ostringstream oss;
    oss << "Thread " << id << " data\n";
    std::string data = oss.str();

    // Command to execute (e.g., "cat" for echoing data back in this example)
    std::string command = "cat";

    // Output data generation log (thread-safe)
    {
        std::lock_guard<std::mutex> lock(io_mutex);
        std::cout << "Thread " << id << " generated data: " << data;
    }

    // Send data to the command
    runCommandWithData(command, data);
}

int main() {
    // Create a vector to hold the threads
    std::vector<std::thread> threads;

    // Spawn threads
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(workerFunction, i);
    }

    // Join threads back to the main thread
    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "All threads completed.\n";
    return 0;
}
