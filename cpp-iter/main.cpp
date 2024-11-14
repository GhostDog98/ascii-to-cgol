#include <atomic>
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

    while (getline(stream, line) && y_max < 16) {
        if (!line.empty() && line.find('1') != string::npos) {
            x_max = max(x_max, static_cast<int>(line.size()));
            for (char& ch : line) {
                ch = (ch == '0') ? 'b' : 'o';
            }
            lines[y_max++] = runLengthEncode(line);
        }
    }

    ostringstream final_stream;
    final_stream << "x=0,y=0,r=B3/S23\n";
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
    int pipefd[2];

    if (pipe(pipefd) < 0) {
        perror("pipe");
        cout << "Error creating the pipe..." << endl;
        return;
    }

    // Set the write end of the pipe to non-blocking
    int flags = fcntl(pipefd[1], F_GETFL, 0);
    fcntl(pipefd[1], F_SETFL, flags | O_NONBLOCK);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        cout << "Error forking a child..." << endl;
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }

    if (pid == 0) {
        // In the child process
        close(pipefd[1]);  // Close unused write end
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);

        execvp("cat", const_cast<char* const*>(apgluxeArgs.data()));
        cout << "error starting cat...";
        perror("execvp");
        exit(EXIT_FAILURE);

    } else {
        // In the parent process
        close(pipefd[0]);  // Close unused read end

        const int pipeSize = 1048576;  // Pipe buffer size
        fcntl(pipefd[1], F_SETPIPE_SZ, pipeSize);

        for (size_t i = start; i < end && running; ++i) {
            generate_combination(i, testLength, characters, currentPattern);
            string output = conway_encode(currentPattern, masterDictionary);

            size_t written = 0;
            while (written < output.size() && running) {
                ssize_t bytes = write(pipefd[1], output.c_str() + written, output.size() - written);
                if (bytes == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // Pipe is full, wait and retry the remaining bytes...
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));  // Short sleep
                        continue;
                    } else {
                        perror("write");
                        break;
                    }
                }
                written += bytes;
            }

            fsync(pipefd[1]);  // Ensure data is flushed after each pattern
        }

        fsync(pipefd[1]);
        fsync(pipefd[0]);
        close(pipefd[0]);
        close(pipefd[1]);  // Close write end once finished

        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            lock_guard<mutex> lock(coutMutex);
            cout << "Thread " << id << " finished. Exit code: " << WEXITSTATUS(status) << endl;
        } else if (WIFSIGNALED(status)) {
            lock_guard<mutex> lock(coutMutex);
            cerr << "Thread " << id << " terminated by signal: " << WTERMSIG(status) << endl;
        }
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

    vector<const char*> apgluxeArgs = {"cat"};
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
    ThreadPool threadPool(threadCount);

    size_t totalCombinations = pow(CHARACTERS.size(), testLength);
    cout << "Detected " << totalCombinations << " number of combinations for length " << testLength << endl;
    size_t chunkSize = totalCombinations / threadCount;
    size_t remainder = totalCombinations % threadCount;

    size_t start = 0;
    for (size_t i = 0; i < threadCount; ++i) {
        size_t end = start + chunkSize + (i < remainder ? 1 : 0);
        cout << "Starting thread for tasks " << start << "-" << end << endl;
        threadPool.enqueueTask([=, &apgluxeArgs]() {
            workerFunction(start, end, masterDictionary, testLength, CHARACTERS, apgluxeArgs, i);
        });

        start = end;
    }

    return 0;
}
