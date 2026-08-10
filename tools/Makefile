# Standalone build for the external CPU/RAM logger.

TARGET := log_demo_stats
SOURCE := log_demo_stats.cpp

CXX ?= g++
CPPFLAGS ?=
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra
LDFLAGS ?=
LDLIBS ?=

ifeq ($(OS),Windows_NT)
SHELL := cmd.exe
.SHELLFLAGS := /C
endif

.PHONY: all clean help

all: $(TARGET)

$(TARGET): $(SOURCE) Makefile
ifeq ($(OS),Windows_NT)
	@echo $(TARGET) is only supported on macOS and Linux.
	@echo It uses ps, pgrep and POSIX process APIs.
	@exit /b 1
else
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -o $@ $(SOURCE) $(LDLIBS)
endif

clean:
ifeq ($(OS),Windows_NT)
	@if exist $(TARGET) del /Q $(TARGET)
	@if exist $(TARGET).exe del /Q $(TARGET).exe
else
	rm -f $(TARGET) $(TARGET).exe
endif

help:
	@echo Targets:
	@echo   make        Build $(TARGET)
	@echo   make clean  Remove the logger binary
	@echo   make help   Show this help
