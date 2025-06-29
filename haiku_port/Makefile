# Basic Makefile for SysMonTask Haiku Port

# Application Name
APP_NAME = SysMonTaskHaiku

# Sources
SRCS = SysMonTaskApp.cpp
SRCS += CPUView.cpp
SRCS += MemView.cpp
SRCS += DiskView.cpp
SRCS += NetworkView.cpp
SRCS += ProcessView.cpp
SRCS += GPUView.cpp
SRCS += SysInfoView.cpp
# Add more .cpp files here as we create them:

# Objects: Automatically generated from SRCS
OBJS = $(SRCS:.cpp=.o)

# Compiler and Linker
CC = g++

# Target architecture (adjust if necessary, e.g., x86_gcc2 for older Haiku)
ARCH = x86

# Haiku build command
HAIKU_BUILD_COMMAND = setarch $(ARCH) $(CC)

# Compiler Flags
# -Wall: Enable all warnings
# -g: Include debugging symbols
# -I /boot/system/develop/headers/posix : Include POSIX headers explicitly if needed
#   (often included by default with Haiku's build tools)
CFLAGS = -Wall -g -std=c++11

# Linker Flags
# Common Haiku libraries:
# -lbe: Be API (Application Kit, Interface Kit, Support Kit, etc.)
# -ltracker: For Tracker-related functions (if any, not typical for this app)
# -lnetwork: For Network Kit
# -lroot: Standard C library functions
# Add more libraries as needed
LIBS = -lbe -lnetwork # Add -lstorage, etc. as features are implemented

# Default Target
all: $(APP_NAME)

# Linking the application
$(APP_NAME): $(OBJS)
	@echo "Linking $(APP_NAME)..."
	$(HAIKU_BUILD_COMMAND) $(OBJS) -o $(APP_NAME) $(LIBS)
	@echo "$(APP_NAME) built successfully."

# Compiling source files
%.o: %.cpp
	@echo "Compiling $<..."
	$(HAIKU_BUILD_COMMAND) $(CFLAGS) -c $< -o $@

# Clean target
clean:
	@echo "Cleaning up..."
	rm -f $(OBJS) $(APP_NAME)
	@echo "Cleanup complete."

# Phony targets
.PHONY: all clean
