cmake_minimum_required(VERSION 3.16)

project(SonusFlow VERSION 0.1 LANGUAGES CXX C)

# --- 1. Standard Qt & CMake Setup ---
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools)

# --- 2. Define Source Files ---
# Group project's source files.
set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    # resources.qrc
    SonusFlow_en_CA.ts
)

# --- 3. Create the Targets ---
# First, create the main application executable from sources.
qt_add_executable(SonusFlow MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

# Second, create a static library for the miniaudio implementation.
add_library(miniaudio_lib STATIC
    miniaudio_impl.cpp
)

# --- 4. Configure Include Paths ---
# Tell the SonusFlow executable where to find header files.
target_include_directories(SonusFlow PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party # For miniaudio.h
)

# Tell the miniaudio_lib where to find its own header file.
target_include_directories(miniaudio_lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party # For miniaudio.h
)

# --- 5. Link Libraries ---
# Link the SonusFlow executable against Qt and other libraries.
target_link_libraries(SonusFlow PRIVATE
    Qt6::Widgets
    miniaudio_lib
    asound      # For ALSA on Linux
    pthread     # For threading
    m           # For the math library
)

# --- 6. Custom Build Step: Copy Assets to Build Directory ---
# This ensures sounds are always available next to the executable.

# Define the source directory for assets
set(ASSET_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sounds)

# Define the destination directory (where the executable will be)
set(ASSET_DEST_DIR ${CMAKE_BINARY_DIR}/sounds)

# Find all files within the source directory and subdirectories
file(GLOB_RECURSE ASSET_FILES "${ASSET_SOURCE_DIR}/*")

# For each file, create a command to copy it to the destination
foreach(ASSET_FILE ${ASSET_FILES})
    # Determine the relative path to preserve the subfolder structure
    file(RELATIVE_PATH RELATIVE_ASSET_FILE ${ASSET_SOURCE_DIR} ${ASSET_FILE})
    set(DEST_FILE "${ASSET_DEST_DIR}/${RELATIVE_ASSET_FILE}")

    # Add the command to copy the file if it's new or has changed
    add_custom_command(
        TARGET SonusFlow POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ASSET_FILE}"
        "${DEST_FILE}"
        COMMENT "Copying asset: ${RELATIVE_ASSET_FILE}"
    )
endforeach()

# --- 7. Packaging & Installation ---
set_target_properties(SonusFlow PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.SonusFlow
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS SonusFlow
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Finalize the Qt executable.
qt_finalize_executable(SonusFlow)
