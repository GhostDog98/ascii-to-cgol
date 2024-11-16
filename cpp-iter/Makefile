# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -pedantic -Ofast -g
CXXFLAGS += -fdiagnostics-color=always # Easier to read

# Check the operating system
ifeq ($(shell uname), Darwin)
    # macOS (Darwin)
    CXX := g++-14
else # Why is fsanitize broken on macos? because god hates me
	CXXFLAGS += -fsanitize=address,undefined 
endif

# Target executable name
TARGET = conway

# Source files
SRC = main.cpp

# Build target
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

# Clean target
clean:
	rm -f $(TARGET)

# Run target
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run

