#!/usr/bin/env bash

set -ex

SRC_PATH=$1
COVERAGE_THRESHOLD=$2

if [ -z "$SRC_PATH" ]; then
    echo "SRC_PATH not defined!"
    exit 1
fi

if [ -z "$COVERAGE_THRESHOLD" ]; then
    echo "COVERAGE_THRESHOLD not defined!"
    exit 1
fi

pushd "$SRC_PATH"

rm -fR build
mkdir build

# Run the tests in Release mode and check code coverage
# and code style (the coverage target runs all unit tests)
cmake -DBUILD_TESTS=ON -DCOVERAGE=ON -DDOCUMENTATION=ON -DBUILD_SAMPLES=ON -DCMAKE_BUILD_TYPE=Debug -B build/
cmake --build build/ -- -j6
cmake --build build/ --target coverage
cmake --build build/ --target check_style

# Make sure unit test coverage doesn't fall below the predefined threshold
function_coverage=$(lcov --summary build/coverage/coverage.info | grep functions | cut -c 16-17)
line_coverage=$(lcov --summary build/coverage/coverage.info | grep lines | cut -c 16-17)
echo "The current function coverage is $(lcov --summary build/coverage/coverage.info | grep functions | cut -c 16-19)%"
if [ "$function_coverage" -lt "$COVERAGE_THRESHOLD" ]; then
    echo "The current function coverage falls below the $COVERAGE_THRESHOLD% threshold."
    exit 1
fi
echo "The current line coverage is $(lcov --summary build/coverage/coverage.info | grep lines | cut -c 16-19)%"
if [ "$line_coverage" -lt "$COVERAGE_THRESHOLD" ]; then
    echo "The current line coverage falls below the $COVERAGE_THRESHOLD% threshold."
    exit 1
fi

# Copy the coverage directory so we don't loose it
rm -rf ./coverage
cp -r build/coverage .

# Running the various code sanitizers
# -----------------------------------
clean_and_build() {
    rm -fR build
    mkdir build
    cmake -DBUILD_TESTS=ON -DDOCUMENTATION=OFF -DBUILD_SAMPLES=OFF -DCMAKE_BUILD_TYPE="$1" -B build/
    cmake --build build/ -- -j6
}

run_tests() {
    cmake --build build/ --target unit_test
}

echo -e "\nRunning the UndefinedBehaviorSanitizer...\n"
clean_and_build ubsan
run_tests

echo -e "\nRunning the LeakSanitizer...\n"
clean_and_build lsan
run_tests

echo -e "\nRunning the AddressSanitizer...\n"
clean_and_build asan
run_tests

echo -e "\nRunning the ThreadSanitizer...\n"
clean_and_build tsan
run_tests

popd
