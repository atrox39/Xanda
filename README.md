# Xanda

Xanda es un experimento pequeno de UI en C para WebAssembly con Emscripten. El proyecto construye un arbol de nodos en memoria, lo renderiza al DOM desde JavaScript embebido con `EM_JS` y permite asociar eventos del navegador a funciones C exportadas.

## Que incluye

- Un VDOM minimo en C con nodos de elemento y texto.
- Un bridge C/JavaScript para crear nodos DOM, asignar atributos, montar el arbol y conectar eventos.
- Un ejemplo funcional `counter` que compila a `counter.js` y `counter.wasm`.

## Estructura

```text
xanda/
|-- include/xanda/xanda.h
|-- src/xanda.c
|-- src/xanda_js_bridge.c
|-- examples/counter/counter.c
|-- examples/counter/index.html
|-- Makefile
`-- build/                    # salida generada
```

## Requisitos

- [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) instalado y activado.
- `make` o `mingw32-make` disponible en el sistema.
- Un navegador para abrir el ejemplo compilado.

## Compilacion

Desde la raiz del proyecto:

```bash
make
```

En Windows, si tu instalacion expone `mingw32-make` en lugar de `make`:

```bash
mingw32-make
```

Esto genera:

- `build/counter.js`
- `build/counter.wasm`
- `build/index.html`

El `Makefile` replica el build manual actual, pero desde la raiz del repo:

```bash
emcc -I include src/xanda.c src/xanda_js_bridge.c examples/counter/counter.c \
  -o build/counter.js \
  -s NO_EXIT_RUNTIME=1 \
  -s EXPORTED_RUNTIME_METHODS="['ccall','cwrap']" \
  -s EXPORTED_FUNCTIONS="['_increment_and_render','_main']" \
  -O2
```

## Targets del Makefile

```bash
make all
make counter
make clean
make rebuild
make help
```

## Ejecutar el ejemplo

Despues de compilar, sirve la carpeta `build/` con un servidor estatico simple:

```bash
cd build
python -m http.server 8000
```

Luego abre `http://localhost:8000`.

## API principal

Definida en `include/xanda/xanda.h`:

- `xanda_create(const char *tag)`: crea un nodo de elemento.
- `xanda_text(const char *text)`: crea un nodo de texto.
- `xanda_attr(...)`: agrega atributos a un elemento.
- `xanda_append(...)`: agrega un hijo a un nodo padre.
- `xanda_render_to_dom(...)`: materializa el arbol en nodos DOM del navegador.
- `xanda_mount(...)`: monta el resultado en un elemento destino del HTML.
- `xanda_event(...)`: conecta un evento del DOM con una funcion C exportada.
- `xanda_free(...)`: libera el arbol en memoria.

## Flujo del ejemplo `counter`

1. `counter_render()` construye un arbol `XandaNode`.
2. `xanda_render_to_dom()` crea los nodos DOM reales desde ese arbol.
3. `xanda_event()` enlaza el click del boton con `increment_and_render`.
4. `xanda_mount()` monta el resultado en el elemento `#xanda`.
5. `xanda_free()` libera el VDOM temporal ya renderizado.

Nota: `xanda_event()` debe llamarse despues de `xanda_render_to_dom()`, porque el `dom_id` del nodo se asigna durante el renderizado.
