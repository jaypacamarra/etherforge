cmake_minimum_required(VERSION 3.16)
project(etherforge VERSION 1.0.0 LANGUAGES C)

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

# Build SOEM from submodule
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SOEM/CMakeLists.txt")
    message(STATUS "Building SOEM from submodule")
    
    # Add SOEM subdirectory
    add_subdirectory(third_party/SOEM)
    
    set(HAVE_SOEM TRUE)
    add_compile_definitions(HAVE_SOEM)
    set(SOEM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SOEM/soem")
    set(SOEM_LIBRARY soem)
    
    message(STATUS "SOEM support: ENABLED (built from source)")
else()
    # Fallback: try to find system SOEM
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
    src/config.c
    src/logging.c
)

# Add appropriate EtherCAT implementation
if(HAVE_SOEM)
    list(APPEND SOURCES src/ethercat_soem.c)
else()
    list(APPEND SOURCES src/ethercat.c)
endif()

# Create executable
add_executable(etherforge ${SOURCES})

# Link libraries
target_link_libraries(etherforge
    Threads::Threads
    rt
    ${YAML_LIBRARIES}
)

if(HAVE_SOEM)
    target_link_libraries(etherforge ${SOEM_LIBRARY})
endif()

# Include yaml compile flags
target_compile_options(etherforge PRIVATE ${YAML_CFLAGS})

# Install targets
install(TARGETS etherforge
    RUNTIME DESTINATION bin
)

install(FILES config/etherforge.yaml
    DESTINATION /etc/etherforge
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