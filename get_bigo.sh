#!/bin/bash

# Check if the 'multitime' command is available
if ! command -v multitime &> /dev/null; then
    echo "multitime could not be found. Please install it to proceed."
    exit 1
fi

# Function to run the program and calculate average time
run_foo() {
    local n=$1
    # Generate a random string of length n
    random_string=$( LC_ALL=C tr -dc A-Za-z0-9 </dev/urandom | head -c "$n")
    # Use multitime to run 'foo' with the random string as an argument, 20 times
    avg_time=$(multitime -n 20 python3 ascii-to-conway.py "$random_string" > /dev/null)
    echo "$n: $avg_time"
}

# Loop to run until Ctrl+C is pressed
n=1
while true; do
    run_foo "$n"
    n=$((n + 1))
done
