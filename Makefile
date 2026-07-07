# Makefile for DaveSaveEd (using NMAKE for MSVC)
#
# Copyright (c) 2025 FNGarvin (184324400+FNGarvin@users.noreply.github.com)
# All rights reserved.
#
# This software is provided 'as-is', without any express or implied
# warranty. In no event will the authors be held liable for any damages
# arising from the use of this software.
#
# Permission is granted to anyone to use this software for any purpose,
# including commercial applications, and to alter it and redistribute it
# freely, subject to the following restrictions:
#
# 1. The origin of this software must not be misrepresented; you must not
#    claim that you wrote the original software. If you use this software
#    in a product, an acknowledgment in the product documentation would be
#    appreciated but is not required.
# 2. Altered source versions must be plainly marked as such, and must not be
#    misrepresented as being the original software.
# 3. This notice may not be removed or altered from any source distribution.
#
# Disclaimer: This project and its creators are not affiliated with Mintrocket, Nexon,
# or any other entities associated with the game "Dave the Diver." This is an independent
# fan-made tool.
#
# This project uses third-party libraries under their respective licenses:
# - zlib (Zlib License)
# - nlohmann/json (MIT License)
# - SQLite (Public Domain)
# Full license texts can be found in the /dist/zlib, /dist/nlohmann_json, and /dist/sqlite3 directories.
#

# --- Configuration ---
# Compiler and Linker executables.
CC = cl.exe
LINK = link.exe

# Compiler flags:
# /EHsc: Enables C++ exception handling.
# /std:c++17: Specifies C++17 standard.
# /W4: Sets warning level to 4 (high level of warnings).
# /MD: Links with the multi-threaded DLL version of the C runtime library (MSVCRT.LIB).
# /D_CRT_SECURE_NO_WARNINGS: Suppresses warnings for older, less secure C runtime functions.
# /Zi: Generates complete debugging information (optional, remove for release builds).
CFLAGS = /EHsc /std:c++17 /W4 /MD /D_CRT_SECURE_NO_WARNINGS # /Zi
# Define CXXFLAGS as CFLAGS (since cl.exe handles both)
CXXFLAGS = $(CFLAGS)

# Linker flags:
# /DEBUG: Generates debugging information for the executable (optional).
LFLAGS = # /DEBUG

# Include paths for external libraries and custom headers.
# /I: Specifies directories to add to the include search path.
INCLUDE_PATHS = /I"dist\zlib\include" /I"dist\nlohmann_json\include" /I"dist\nlohmann_fifo\include" /I"dist\sqlite3\include" /I"." # Add current directory for custom headers

# Library paths for external static libraries.
# /LIBPATH: Specifies directories to add to the library search path.
LIB_PATHS = /LIBPATH:"dist\zlib\lib\x64"

# Libraries to link with the executable.
# zlib.lib: Static library for zlib.
# User32.lib, Gdi32.lib, Shell32.lib, Comdlg32.lib, Ole32.lib: Standard Windows API libraries.
LIBS = zlib.lib User32.lib Gdi32.lib Shell32.lib Comdlg32.lib Ole32.lib

# Output directory for compiled binaries and object files.
BIN_DIR = bin

# Source files for the project.
DAVESAVEED_SRC = DaveSaveEd.cpp
SQLITE_SRC = dist\sqlite3\src\sqlite3.c
LOGGER_SRC = Logger.cpp
SAVEMGR_SRC = SaveGameManager.cpp

# Object files derived from source files, placed in the BIN_DIR.
DAVESAVEED_OBJ = $(BIN_DIR)\DaveSaveEd.obj
SQLITE_OBJ = $(BIN_DIR)\sqlite3.obj
LOGGER_OBJ = $(BIN_DIR)\Logger.obj
SAVEMGR_OBJ = $(BIN_DIR)\SaveGameManager.obj

# All object files that need to be linked to form the executable.
ALL_OBJS = $(DAVESAVEED_OBJ) $(SQLITE_OBJ) $(LOGGER_OBJ) $(SAVEMGR_OBJ)

# Resource file variable
RES_FILE = $(BIN_DIR)\DaveSaveEd.res

# Name of the final executable.
TARGET = $(BIN_DIR)\DaveSaveEd.exe

# Pattern for log files, used by the clean and log targets.
LOG_FILE_PATTERN = DaveSaveEd_log_*.txt

# --- Targets ---

# Build target: Builds the executable. This is the new default target.
.PHONY: build
build: $(TARGET)

# Ensures the binary output directory exists. Creates it if it doesn't.
$(BIN_DIR):
    @if not exist $@ mkdir $@

# Rule to compile the resource file
$(RES_FILE): DaveSaveEd.rc resource.h appicon.ico
    @echo Compiling resource file DaveSaveEd.rc...
    rc.exe /r /fo$@ DaveSaveEd.rc

# Main executable target: Links all compiled object files, resource file, and necessary libraries.
# Dependencies: All object files, the binary directory, AND the resource file.
$(TARGET): $(ALL_OBJS) $(BIN_DIR) $(RES_FILE)
    @echo Linking $@...
    $(LINK) $(LFLAGS) $(ALL_OBJS) $(RES_FILE) $(LIB_PATHS) $(LIBS) /OUT:$@

# Rule to compile DaveSaveEd.cpp into an object file.
# Dependencies: The binary directory, Source file and relevant headers.
$(DAVESAVEED_OBJ): $(BIN_DIR) $(DAVESAVEED_SRC) DaveSaveEd.h Logger.h SaveGameManager.h resource.h # Add resource.h as a dependency
    @echo Compiling $(DAVESAVEED_SRC)...
    $(CC) $(CFLAGS) $(INCLUDE_PATHS) /c $(DAVESAVEED_SRC) /Fo$@

# Rule to compile sqlite3.c into an object file.
# Dependencies: The binary directory, SQLite source file.
$(SQLITE_OBJ): $(BIN_DIR) $(SQLITE_SRC)
    @echo Compiling $(SQLITE_SRC)...
    $(CC) $(CFLAGS) $(INCLUDE_PATHS) /c $(SQLITE_SRC) /Fo$@

# Rule to compile Logger.cpp into an object file.
# Dependencies: The binary directory, Logger source file and its headers.
$(LOGGER_OBJ): $(BIN_DIR) $(LOGGER_SRC) Logger.h DaveSaveEd.h
    @echo Compiling $(LOGGER_SRC)...
    $(CC) $(CFLAGS) $(INCLUDE_PATHS) /c $(LOGGER_SRC) /Fo$@

# Rule to compile SaveGameManager.cpp into an object file.
# Dependencies: The binary directory, SaveGameManager source file and its headers.
$(SAVEMGR_OBJ): $(BIN_DIR) $(SAVEMGR_SRC) SaveGameManager.h DaveSaveEd.h Logger.h
    @echo Compiling $(SAVEMGR_SRC)...
    $(CC) $(CFLAGS) $(INCLUDE_PATHS) /c $(SAVEMGR_SRC) /Fo$@

# Clean target: Removes intermediate object files and log files.
# The executable is kept by default for convenience during development.
clean:
    @echo Cleaning build artifacts...
    @if exist $(BIN_DIR)\*.obj del $(BIN_DIR)\*.obj
    @if exist $(BIN_DIR)\*.res del $(BIN_DIR)\*.res
    @echo Cleaning log files from $(BIN_DIR)...
    @if exist "$(BIN_DIR)\$(LOG_FILE_PATTERN)" del "$(BIN_DIR)\$(LOG_FILE_PATTERN)"
    @echo Note: The executable $(TARGET) is kept by the clean target.

# Run target: Executes the compiled program directly.
# Dependencies: The executable itself.
run: $(TARGET)
    @echo Running $(TARGET)...
    start "" "$(TARGET)"

# Log target: Runs the program with file logging enabled and then prints the latest log file content.
# Dependencies: The executable itself.
log: $(TARGET)
    @echo Running $(TARGET) with logging...
    start /wait "" "$(TARGET)" -log
    @echo Checking for log files in $(BIN_DIR)...
    @powershell.exe -NoProfile -Command "$$logDir = '$(BIN_DIR)'; $$logPattern = '$(LOG_FILE_PATTERN)'; $$logFile = Get-ChildItem -Path $$logDir -Filter $$logPattern | Sort-Object LastWriteTime -Descending | Select-Object -First 1; if ($$logFile) { Write-Host ''; Write-Host '--- Log Output (' + $$logFile.Name + ') ---'; Get-Content $$logFile.FullName; } else { Write-Host 'No log files found matching pattern: ' + $$logPattern + ' in ' + $$logDir; }"

# Release Target
# Directory for compiled release packages.
RELEASE_DIR = releases
# Name of the release package. Consider updating this version manually for each release.
# Example: DaveSaveEd_v1.0.0_Win64
RELEASE_NAME = DaveSaveEd_Release_v1.6b_Win64 # <<<--- IMPORTANT: UPDATE VERSION HERE FOR EACH RELEASE
RELEASE_ZIP = $(RELEASE_DIR)\$(RELEASE_NAME).zip

# Release target: Builds the project, then creates a distributable zip archive.
# Contains the executable, README.md, and LICENSE.
release: clean build $(BIN_DIR)
    @echo Creating release package...
    @if not exist $(RELEASE_DIR) mkdir $(RELEASE_DIR)
    @if exist "$(RELEASE_ZIP)" del "$(RELEASE_ZIP)"
    @echo Packaging $(TARGET), README.md, and LICENSE into $(RELEASE_ZIP)...
    @powershell.exe -NoProfile -Command "Compress-Archive -Path '$(BIN_DIR)\DaveSaveEd.exe', 'README.md', 'LICENSE' -DestinationPath '$(RELEASE_ZIP)' -Force"
    @echo Release package created: $(RELEASE_ZIP)
    @echo You can now upload "$(RELEASE_ZIP)" to your GitHub Releases page.

#END OF MAKEFILE


