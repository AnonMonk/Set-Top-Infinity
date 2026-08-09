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

NAME = demo
LIB_DIR := ./
CXX = g++
CXXFLAGS_BASE = -O2 -std=c++17

# Default-Target: Hinweis, welches OS-Target man bauen soll
.PHONY: all mac linux win clean info

all:
	@echo "Bitte Plattform angeben:"
	@echo "  make mac"
	@echo "  make linux"
	@echo "  make win"

# =========================
# macOS
# =========================
mac:
	$(CXX) $(SRC) -o $(NAME) $(CXXFLAGS_BASE) -w -L$(LIB_DIR) \
		-framework OpenGL -lvipgfx -lglTTF

# =========================
# Linux
# =========================
linux:
	$(CXX) $(SRC) -o $(NAME) $(CXXFLAGS_BASE) -Wall -L$(LIB_DIR) \
		-lGL -lvipgfx -lglTTF

# =========================
# Windows (MinGW)
# =========================
win:
	$(CXX) $(SRC) -o $(NAME).exe $(CXXFLAGS_BASE) -w -L$(LIB_DIR) \
		-lopengl32 -lvipgfx -lglTTF -lwinmm

# =========================
# Hilfs-Targets
# =========================
clean:
	rm -f $(NAME) $(NAME).exe CreditsPreview GreetsPreview EndTitlesPreview

info:
	@echo "SRC     = $(SRC)"
	@echo "NAME    = $(NAME)"
	@echo "CXX     = $(CXX)"
	@echo "LIB_DIR = $(LIB_DIR)"
	@echo ""
	@echo "Targets: make mac | make linux | make win | make clean | make info"
