# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -Ofast

# Target executable name
TARGET = conway

# Source files
SRC = main-test.cpp

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

