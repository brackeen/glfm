# Makefile.local should define EMSCRIPTEN_PATH
-include Makefile.local

EMSDK = $(EMSCRIPTEN_PATH)/emsdk
$(shell $(EMSDK) construct_env)
include ./emsdk_set_env.sh
# Not sure why this line is required
export SHELL := $(shell echo $$SHELL)

# Modified by new_project.py
GLFM_ROOT := ../..
APP_ROOT := ../../example

OUT_NAME = index
BIN = bin
INCLUDES = -I $(GLFM_ROOT)/include
SOURCES := $(GLFM_ROOT)/src/glfm_platform_emscripten.c \
           $(wildcard $(APP_ROOT)/*.c)

build: init
	emcc -Wall -O2 -std=c99 --memory-init-file 0 --shell-file shell.html \
	--preload-file $(APP_ROOT)/assets@/ $(INCLUDES) -o $(BIN)/$(OUT_NAME).html \
	$(SOURCES)

init:
	mkdir -p $(BIN)
	
clean:
	rm -f $(BIN)/$(OUT_NAME).html
	rm -f $(BIN)/$(OUT_NAME).data
	rm -f $(BIN)/$(OUT_NAME).js
