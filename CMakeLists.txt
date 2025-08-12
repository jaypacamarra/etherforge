cmake_minimum_required(VERSION 3.16)
project(etherforged VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
add_compile_options(-Wall -Wextra -D_GNU_SOURCE)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Find yaml library
pkg_check_modules(YAML REQUIRED yaml-0.1)

# Find SOEM library (optional)
find_path(SOEM_INCLUDE_DIR ethercat.h PATHS /usr/include/soem /usr/local/include/soem)
find_library(SOEM_LIBRARY NAMES soem libsoem PATHS /usr/lib /usr/local/lib)

if(SOEM_INCLUDE_DIR AND SOEM_LIBRARY)
    message(STATUS "SOEM found: ${SOEM_LIBRARY}")
    set(HAVE_SOEM TRUE)
    add_compile_definitions(HAVE_SOEM)
else()
    message(WARNING "SOEM not found - EtherCAT functionality will be stubbed")
    set(HAVE_SOEM FALSE)
endif()

# Include directories
include_directories(include)
if(HAVE_SOEM)
    include_directories(${SOEM_INCLUDE_DIR})
endif()

# Source files
set(SOURCES
    src/main.c
    src/daemon.c
    src/protocol.c
    src/network.c
    src/commands.c
    src/ethercat.c
    src/config.c
    src/logging.c
)

# Create executable
add_executable(etherforged ${SOURCES})

# Link libraries
target_link_libraries(etherforged
    Threads::Threads
    rt
    ${YAML_LIBRARIES}
)

if(HAVE_SOEM)
    target_link_libraries(etherforged ${SOEM_LIBRARY})
endif()

# Include yaml compile flags
target_compile_options(etherforged PRIVATE ${YAML_CFLAGS})

# Install targets
install(TARGETS etherforged
    RUNTIME DESTINATION bin
)

install(FILES config/etherforged.yaml
    DESTINATION /etc/etherforged
    OPTIONAL
)

# Custom target for dependencies
add_custom_target(deps
    COMMAND sudo apt-get update
    COMMAND sudo apt-get install -y build-essential cmake ninja-build libyaml-dev
    COMMENT "Installing build dependencies"
)

# Print configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "SOEM support: ${HAVE_SOEM}")
message(STATUS "YAML found: ${YAML_FOUND}")