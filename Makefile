EMCC ?= emcc

BUILD_DIR := build
EXAMPLE_DIR := examples/counter
OUTPUT_BASENAME := counter

SOURCES := src/xanda.c src/xanda_js_bridge.c $(EXAMPLE_DIR)/counter.c
HEADERS := include/xanda/xanda.h
OUTPUT_JS := $(BUILD_DIR)/$(OUTPUT_BASENAME).js
OUTPUT_HTML := $(BUILD_DIR)/index.html

EMCC_FLAGS := -I include -O2 \
	-s NO_EXIT_RUNTIME=1 \
	-s EXPORTED_RUNTIME_METHODS="['ccall','cwrap']" \
	-s EXPORTED_FUNCTIONS="['_increment_and_render','_main']"

.PHONY: all counter clean rebuild help

all: counter

counter: $(OUTPUT_JS) $(OUTPUT_HTML)

$(BUILD_DIR):
	powershell -NoProfile -Command "New-Item -ItemType Directory -Force '$(BUILD_DIR)' | Out-Null"

$(OUTPUT_JS): $(SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(EMCC) $(EMCC_FLAGS) $(SOURCES) -o $(OUTPUT_JS)

$(OUTPUT_HTML): $(EXAMPLE_DIR)/index.html | $(BUILD_DIR)
	powershell -NoProfile -Command "Copy-Item '$(EXAMPLE_DIR)/index.html' '$(OUTPUT_HTML)' -Force"

clean:
	powershell -NoProfile -Command "if (Test-Path '$(BUILD_DIR)') { Remove-Item '$(BUILD_DIR)' -Recurse -Force }"

rebuild:
	$(MAKE) clean
	$(MAKE) -B all

help:
	@echo Targets disponibles:
	@echo   make all       - Compila el ejemplo counter en build/
	@echo   make counter   - Igual que all
	@echo   make clean     - Elimina build/
	@echo   make rebuild   - Limpia y recompila
