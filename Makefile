# =========================
# Projekt
# =========================

SRC = main.cpp \
      EffectEndTitles.cpp \
      EffectLogo.cpp \
      EffectJulia.cpp \
      EffectRotozoom.cpp \
      EffectStarfield.cpp \
      EffectTunnel.cpp \
      EffectTwister.cpp

MAC_SRC = MacSound.mm

NAME = demo
LIB_DIR := ./
CXX = g++
TARGET_TRIPLE ?= $(shell $(CXX) -dumpmachine)
X86_MARKERS := i386 i486 i586 i686 x86_64 amd64
X86_TARGET := $(strip $(foreach arch,$(X86_MARKERS),$(findstring $(arch),$(TARGET_TRIPLE))))
SSE2_FLAGS := $(if $(X86_TARGET),-msse2,)
CXXFLAGS_BASE = -O3 $(SSE2_FLAGS)

# Default-Target: Hinweis, welches OS-Target man bauen soll
.PHONY: all mac linux win clean info tools

all:
	@echo "Bitte Plattform angeben:"
	@echo "  make mac"
	@echo "  make linux"
	@echo "  make win"
	@echo "  make tools   # external CPU logger (tools/log_demo_stats)"

# =========================
# macOS
# =========================
mac:
	$(CXX) $(SRC) $(MAC_SRC) -o $(NAME) $(CXXFLAGS_BASE) -w -L$(LIB_DIR) \
		-framework OpenGL -framework Cocoa -lvipgfx -lglTTF

# =========================
# Linux
# =========================
linux:
	$(CXX) $(SRC) -o $(NAME) $(CXXFLAGS_BASE) -Wall -L$(LIB_DIR) \
		-Wl,-rpath,'$$ORIGIN' -lGL -lvipgfx -lglTTF

# =========================
# Windows (MinGW)
# =========================
win:
	$(CXX) $(SRC) -o $(NAME).exe $(CXXFLAGS_BASE) -w -L$(LIB_DIR) \
		-lopengl32 -lvipgfx -lglTTF -lwinmm

# =========================
# Tools (external, not linked into demo)
# =========================
tools:
	$(MAKE) -C tools all

# =========================
# Hilfs-Targets
# =========================
clean:
	rm -f $(NAME) $(NAME).exe CreditsPreview GreetsPreview EndTitlesPreview \
		tools/log_demo_stats tools/log_demo_stats.exe

info:
	@echo "SRC     = $(SRC)"
	@echo "NAME    = $(NAME)"
	@echo "CXX     = $(CXX)"
	@echo "TARGET  = $(TARGET_TRIPLE)"
	@echo "SSE2    = $(SSE2_FLAGS)"
	@echo "LIB_DIR = $(LIB_DIR)"
	@echo ""
	@echo "Targets: make mac | make linux | make win | make tools | make clean | make info"
