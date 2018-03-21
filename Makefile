SLUG = Fundamental
VERSION = 0.6.0

SOURCES += $(wildcard src/*.cpp)

DISTRIBUTABLES += $(wildcard LICENSE*) res

FLAGS += -Idep/libsamplerate/src
LDFLAGS += -Ldep/libsamplerate/src/.libs -l:libsamplerate.a

libsamplerate = dep/libsamplerate/src/.libs/libsamplerate.a
$(libsamplerate):
	cd dep/libsamplerate && ./autogen.sh
	cd dep/libsamplerate && CFLAGS=-fPIC ./configure
	cd dep/libsamplerate && $(MAKE)

RESOURCES += $(libsamplerate)

RACK_DIR ?= ../..
include $(RACK_DIR)/plugin.mk
