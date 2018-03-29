RACK_DIR ?= ../..
SLUG = PulsumQuadratum-scope
VERSION = 0.6.0dev

SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += $(wildcard LICENSE*) res

include $(RACK_DIR)/plugin.mk
