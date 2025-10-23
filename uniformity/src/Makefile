.RECIPEPREFIX := >
CC ?= gcc
TARGET ?= main
SRCS := main.c
INCLUDES ?=
EXE :=
UNAME_S := $(shell uname -s)
ifeq ($(OS),Windows_NT)
  PLATFORM := windows
else ifneq (,$(findstring MINGW,$(UNAME_S)))
  PLATFORM := windows
else ifneq (,$(findstring MSYS,$(UNAME_S)))
  PLATFORM := windows
else ifneq (,$(findstring CYGWIN,$(UNAME_S)))
  PLATFORM := windows
else ifeq ($(UNAME_S),Darwin)
  PLATFORM := mac
else
  PLATFORM := linux
endif
ifeq ($(PLATFORM),windows)
  EXE := .exe
  WIN_DEFS := -DSDL_MAIN_HANDLED
endif
PKG_CONFIG ?= pkg-config
SDL2_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl2 2>/dev/null)
SDL2_LIBS := $(shell $(PKG_CONFIG) --libs sdl2 2>/dev/null)
MIXER_CFLAGS := $(shell $(PKG_CONFIG) --cflags SDL2_mixer 2>/dev/null)
MIXER_LIBS := $(shell $(PKG_CONFIG) --libs SDL2_mixer 2>/dev/null)
ifeq ($(strip $(SDL2_LIBS)),)
  $(error pkg-config failed for 'sdl2'. Ensure pkg-config and SDL2 dev files are installed.)
endif
ifeq ($(strip $(MIXER_LIBS)),)
  $(error pkg-config failed for 'SDL2_mixer'. Ensure pkg-config and SDL2_mixer dev files are installed.)
endif
ifeq ($(PLATFORM),windows)
  OGL_LIBS := -lopengl32
else ifeq ($(PLATFORM),mac)
  OGL_LIBS := -framework OpenGL
else
  OGL_LIBS := -lGL
endif
CFLAGS ?= -std=c11 -Werror -Wextra -Wall -Wpedantic -fno-strict-aliasing -MMD -MP
CFLAGS += $(INCLUDES) $(SDL2_CFLAGS) $(MIXER_CFLAGS) $(WIN_DEFS)
LDFLAGS ?=
LDLIBS = $(SDL2_LIBS) $(MIXER_LIBS) $(OGL_LIBS) -lz -lm
OBJS := $(SRCS:.c=.o)
DEPS := $(OBJS:.o=.d)
.PHONY: all clean run debug release
all: $(TARGET)$(EXE)
$(TARGET)$(EXE): $(OBJS)
> $(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)
%.o: %.c
> $(CC) $(CFLAGS) -c $< -o $@
run: $(TARGET)$(EXE)
> ./$(TARGET)$(EXE)
clean:
> rm -f $(OBJS) $(DEPS) $(TARGET) $(TARGET).exe
debug: CFLAGS += -O0 -g -DDEBUG
debug: clean all
release: CFLAGS += -O2 -DNDEBUG
release: clean all
-include $(DEPS)
