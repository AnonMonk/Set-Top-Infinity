# =========================
# Projekt
# =========================

SRC = main.cpp \
      EffectEndTitles.cpp \
      EffectLogo.cpp \
      EffectMandelbrot.cpp \
      EffectRotozoom.cpp \
      EffectStarfield.cpp \
      EffectTunnel.cpp \
      EffectTwister.cpp

NAME = demo

COMMON_WARNINGS = -w

LIB_DIR := ./

CXX = g++
OUT = $(NAME)
CXXFLAGS = -O2 $(COMMON_WARNINGS) -std=c++17
LDFLAGS = -lopengl32 -lvipgfx -lglTTF


all:
	$(CXX) $(SRC) -o $(OUT) $(CXXFLAGS) -L$(LIB_DIR) $(LDFLAGS)

clean:
	rm -f $(NAME) $(NAME).exe CreditsPreview GreetsPreview EndTitlesPreview

info:
	@echo TARGET = $(TARGET)
	@echo CXX = $(CXX)
	@echo OUT = $(OUT)
	@echo CXXFLAGS = $(CXXFLAGS)
	@echo LDFLAGS = $(LDFLAGS)
