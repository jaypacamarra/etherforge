#!/bin/bash

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if cmake and ninja are available
if ! command -v cmake &> /dev/null; then
    print_error "cmake not found. Please install cmake first."
    exit 1
fi

if ! command -v ninja &> /dev/null; then
    print_error "ninja not found. Please install ninja-build first."
    exit 1
fi

# Parse command line arguments
BUILD_TYPE="Release"
CLEAN=false
INSTALL=false
HELP=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -i|--install)
            INSTALL=true
            shift
            ;;
        -h|--help)
            HELP=true
            shift
            ;;
        *)
            print_error "Unknown option: $1"
            HELP=true
            shift
            ;;
    esac
done

if [ "$HELP" = true ]; then
    echo "Usage: $0 [OPTIONS]"
    echo "Options:"
    echo "  -d, --debug    Build in debug mode"
    echo "  -c, --clean    Clean build directory before building"
    echo "  -i, --install  Install after building"
    echo "  -h, --help     Show this help message"
    exit 0
fi

# Clean build directory if requested
if [ "$CLEAN" = true ]; then
    print_status "Cleaning build directory..."
    rm -rf build
fi

# Create build directory
mkdir -p build
cd build

# Configure with cmake
print_status "Configuring build with CMake (${BUILD_TYPE})..."
cmake -G Ninja -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ..

# Build with ninja
print_status "Building with Ninja..."
ninja

# Install if requested
if [ "$INSTALL" = true ]; then
    print_status "Installing..."
    sudo ninja install
    print_status "Installation complete."
fi

print_status "Build complete! Executable: build/etherforge"