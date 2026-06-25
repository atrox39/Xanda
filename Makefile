EMCC_DEFAULT := $(or $(wildcard C:/emsdk/upstream/emscripten/emcc.bat),$(wildcard /c/emsdk/upstream/emscripten/emcc.bat),emcc)
ifeq ($(OS),Windows_NT)
NODE_DEFAULT := node.exe
TEST_BIN = $(TEST_BUILD_DIR)/test_xanda.exe
else
NODE_DEFAULT := node
TEST_BIN = $(TEST_BUILD_DIR)/test_xanda
endif

NULL_DEVICE := /dev/null

EMCC ?= $(EMCC_DEFAULT)
CC ?= cc
NODE ?= $(NODE_DEFAULT)
MAKE_BIN ?= make
DEV ?= 0
EXAMPLE ?= counter
XANDA_VERSION ?= 1.0.0
XANDA_DEV_PROTOCOL ?= 1
DEV_SERVER_DIR := tools/dev-server
DEV_SERVER_PORT ?= 5173

BUILD_DIR := build
TEST_BUILD_DIR := $(BUILD_DIR)/tests
CORE_SOURCES := src/xanda.c src/xanda_runtime.c src/xanda_state.c src/xanda_component_runtime.c src/xanda_dev_runtime.c src/xanda_js_bridge.c
PUBLIC_HEADERS := include/xanda/xanda.h src/xanda_internal.h
EXAMPLES := counter minimal

COMMON_INCLUDE_FLAGS := -I include
EMCC_FLAGS := $(COMMON_INCLUDE_FLAGS) -O2 \
	-s NO_EXIT_RUNTIME=1 \
	-s EXPORTED_RUNTIME_METHODS="['ccall','cwrap']"
DEV_EMCC_FLAGS := -gsource-map -g3 \
	-s ASSERTIONS=2 \
	-s MODULARIZE=1 \
	-s EXPORT_NAME=createXandaModule
ACTIVE_EMCC_FLAGS := $(EMCC_FLAGS) $(if $(filter 1 true TRUE yes YES,$(DEV)),$(DEV_EMCC_FLAGS),)
DEV_EXPORTS := ,'_xanda_dev_protocol_version','_xanda_dev_snapshot_root_component','_xanda_dev_describe_root_boundary','_xanda_dev_list_boundaries'
COUNTER_EXPORTS := -s EXPORTED_FUNCTIONS="['_increment_and_render','_main'$(if $(filter 1 true TRUE yes YES,$(DEV)),$(DEV_EXPORTS),)]"
MINIMAL_EXPORTS := -s EXPORTED_FUNCTIONS="['_main'$(if $(filter 1 true TRUE yes YES,$(DEV)),$(DEV_EXPORTS),)]"

TEST_SOURCES := $(CORE_SOURCES) tests/test_xanda.c
TEST_CFLAGS := -std=c11 -Wall -Wextra -Werror -pedantic -I include -I src

define MKDIR_P
$(if $(filter Windows_NT,$(OS)),powershell -NoProfile -Command "New-Item -ItemType Directory -Force '$(subst /,\,$1)' | Out-Null",mkdir -p '$1')
endef

define COPY_FILE
$(if $(filter Windows_NT,$(OS)),powershell -NoProfile -Command "Copy-Item '$(subst /,\,$1)' '$(subst /,\,$2)' -Force",cp '$1' '$2')
endef

define REMOVE_DIR
$(if $(filter Windows_NT,$(OS)),powershell -NoProfile -Command "if (Test-Path '$(subst /,\,$1)') { Remove-Item '$(subst /,\,$1)' -Recurse -Force }",rm -rf '$1')
endef

.PHONY: all examples counter minimal test test-browser clean rebuild help check-emcc check-node check-devtools dev-setup serve watch dev dev-counter dev-minimal version release-check

all: examples

examples: $(EXAMPLES)

$(BUILD_DIR):
	$(call MKDIR_P,$(BUILD_DIR))

$(TEST_BUILD_DIR): | $(BUILD_DIR)
	$(call MKDIR_P,$(TEST_BUILD_DIR))

define EXAMPLE_RULES
$(BUILD_DIR)/$(1): | $(BUILD_DIR)
	$(call MKDIR_P,$(BUILD_DIR)/$(1))

$(BUILD_DIR)/$(1)/$(1).js: check-emcc $(CORE_SOURCES) $(wildcard examples/$(1)/*.c) $(wildcard examples/$(1)/*.h) $(PUBLIC_HEADERS) | $(BUILD_DIR)/$(1)
	$(EMCC) $(ACTIVE_EMCC_FLAGS) $(2) $(CORE_SOURCES) $(wildcard examples/$(1)/*.c) -o $$@

$(BUILD_DIR)/$(1)/index.html: examples/$(1)/index.html | $(BUILD_DIR)/$(1)
	$(call COPY_FILE,examples/$(1)/index.html,$$@)

$(1): $(BUILD_DIR)/$(1)/$(1).js $(BUILD_DIR)/$(1)/index.html
endef

$(eval $(call EXAMPLE_RULES,counter,$(COUNTER_EXPORTS)))
$(eval $(call EXAMPLE_RULES,minimal,$(MINIMAL_EXPORTS)))

check-emcc:
	@"$(EMCC)" -v > $(NULL_DEVICE) 2>&1 || ( \
		echo "No se pudo ejecutar EMCC='$(EMCC)'."; \
		echo "Instala/activa Emscripten o ejecuta make EMCC=C:/emsdk/upstream/emscripten/emcc.bat"; \
		exit 127; \
	)

check-node:
	@"$(NODE)" -v > $(NULL_DEVICE) 2>&1 || ( \
		echo "No se pudo ejecutar NODE='$(NODE)'."; \
		echo "Instala Node.js para usar el servidor de desarrollo."; \
		exit 127; \
	)

check-devtools: check-node
	@[ -f "$(DEV_SERVER_DIR)/src/server.mjs" ] || ( \
		echo "No se encontro $(DEV_SERVER_DIR)/src/server.mjs."; \
		exit 127; \
	)

dev-setup: check-node
	@echo "No se requieren dependencias externas. El dev server usa solo Node.js."

$(TEST_BIN): $(TEST_SOURCES) $(PUBLIC_HEADERS) | $(TEST_BUILD_DIR)
	$(CC) $(TEST_CFLAGS) $(TEST_SOURCES) -o $(TEST_BIN)

test: $(TEST_BIN)
	$(TEST_BIN)

test-browser: check-devtools
	PORT=5197 MAKE=$(MAKE_BIN) $(NODE) tests/browser/dev-server-smoke.mjs

serve: check-devtools
	PORT=$(DEV_SERVER_PORT) MAKE=$(MAKE_BIN) $(NODE) $(DEV_SERVER_DIR)/src/server.mjs $(EXAMPLE)

watch: serve

dev: dev-counter

dev-counter:
	$(MAKE) serve EXAMPLE=counter DEV=1

dev-minimal:
	$(MAKE) serve EXAMPLE=minimal DEV=1

clean:
	$(call REMOVE_DIR,$(BUILD_DIR))

rebuild:
	$(MAKE) clean
	$(MAKE) -B all

version:
	@echo $(XANDA_VERSION)

release-check:
	$(MAKE) test
	$(MAKE) test-browser
	$(MAKE) -n counter DEV=1
	$(MAKE) -n minimal DEV=1

help:
	@echo Targets disponibles:
	@echo   make all       - Compila todos los ejemplos web
	@echo   make examples  - Compila counter y minimal
	@echo   make counter   - Compila el ejemplo interactivo counter
	@echo   make minimal   - Compila el ejemplo minimo con defaults
	@echo   make test      - Compila y ejecuta pruebas unitarias nativas
	@echo   make test-browser - Ejecuta el smoke test del servidor de desarrollo
	@echo   make dev-setup - Verifica que Node.js este disponible
	@echo   make dev       - Levanta el modo desarrollo para counter
	@echo   make dev-counter - Igual que dev
	@echo   make dev-minimal - Levanta el modo desarrollo para minimal
	@echo   make version   - Imprime la version actual de Xanda
	@echo   make release-check - Ejecuta pruebas y validaciones base de la release
	@echo   make serve EXAMPLE=counter DEV=1 - Inicia el dev server con recompilacion automatica
	@echo   make watch EXAMPLE=counter DEV=1 - Alias de serve
	@echo   make clean     - Elimina build/
	@echo   make rebuild   - Limpia y recompila los ejemplos
	@echo   Override EMCC  - make EMCC=C:/emsdk/upstream/emscripten/emcc.bat
