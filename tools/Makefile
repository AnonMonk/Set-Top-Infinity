# Standalone build for the external CPU/RAM logger.

LOGGER_TARGET := log_demo_stats
LOGGER_SOURCE := log_demo_stats.cpp
WINDOWS_TARGET := run_atv_slow_win.exe
WINDOWS_SOURCE := run_atv_slow_win.cpp

CXX ?= g++
CPPFLAGS ?=
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra
LDFLAGS ?=
LDLIBS ?=

ifeq ($(OS),Windows_NT)
SHELL := cmd.exe
.SHELLFLAGS := /C
ACTIVE_TARGET := $(WINDOWS_TARGET)
ACTIVE_DESCRIPTION := Windows ATV CPU limiter
else
ACTIVE_TARGET := $(LOGGER_TARGET)
ACTIVE_DESCRIPTION := POSIX CPU/RAM logger
endif

.PHONY: all clean help

ifeq ($(OS),Windows_NT)
all: $(WINDOWS_TARGET)

$(WINDOWS_TARGET): $(WINDOWS_SOURCE) Makefile
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -o $@ $(WINDOWS_SOURCE) -ladvapi32 $(LDLIBS)
else
all: $(LOGGER_TARGET)

$(LOGGER_TARGET): $(LOGGER_SOURCE) Makefile
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -o $@ $(LOGGER_SOURCE) $(LDLIBS)
endif

clean:
ifeq ($(OS),Windows_NT)
	@if exist $(LOGGER_TARGET) del /Q $(LOGGER_TARGET)
	@if exist $(LOGGER_TARGET).exe del /Q $(LOGGER_TARGET).exe
	@if exist $(WINDOWS_TARGET) del /Q $(WINDOWS_TARGET)
else
	rm -f $(LOGGER_TARGET) $(LOGGER_TARGET).exe $(WINDOWS_TARGET)
endif

help:
	@echo Targets:
	@echo   make        Build $(ACTIVE_TARGET) - $(ACTIVE_DESCRIPTION)
	@echo   make clean  Remove tool binaries
	@echo   make help   Show this help
