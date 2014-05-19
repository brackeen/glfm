# Makefile.local should define EMSCRIPTEN_PATH
-include Makefile.local

EMSDK = $(EMSCRIPTEN_PATH)/emsdk
EMSDK_PATH = $(shell $(EMSDK) active_path)
export PATH := $(EMSDK_PATH):$(PATH)
# Not sure why this line is required
export SHELL := $(shell echo $$SHELL)

OUT_NAME = index
ROOT = ../..
BIN = bin
INCLUDES = -I $(ROOT)/include
SOURCES = $(ROOT)/src/glfm_platform_emscripten.c $(ROOT)/example/main.c

build: init
	emcc -Wall -O2 -std=c99 --shell-file shell.html --preload-file $(ROOT)/example/assets@/ $(INCLUDES) -o $(BIN)/$(OUT_NAME).html $(SOURCES)

init:
	mkdir -p $(BIN)
	
clean:
	rm -f $(BIN)/$(OUT_NAME).html
	rm -f $(BIN)/$(OUT_NAME).js
