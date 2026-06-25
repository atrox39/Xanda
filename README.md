# Xanda

Xanda es una libreria pequena de UI en C para WebAssembly con Emscripten. Construye un arbol de nodos en memoria, lo traduce al DOM mediante un backend modular y ofrece una capa de alto nivel para renderizar componentes con configuracion por defecto, menos boilerplate y mensajes de error consistentes.

Version estable actual: `1.0.0`

## Objetivos de esta version

- Reducir pasos de integracion con una API de alto nivel basada en `XandaApp` y un nuevo modelo formal de `XandaComponent`.
- Mantener las primitivas del VDOM para quien quiera control fino.
- Centralizar errores para evitar fallos silenciosos.
- Separar el backend del navegador del flujo de render para facilitar pruebas y escalabilidad.

## Alcance de v1.0.0

Esta release declara estable:

- la API publica centrada en `XandaApp`, `XandaComponent` y `XandaStateSnapshot`;
- el flujo de render por componente con identidad estable;
- el runtime dev con snapshot textual, boundaries formales y hot swap del modulo completo con validacion del boundary raiz;
- los ejemplos `counter` y `minimal` como referencia de integracion.

Fuera del alcance estable de `v1.0.0`:

- HMR selectivo por subarbol o por componente anidado;
- reconciliacion parcial del DOM;
- migracion automatica avanzada de estado entre shapes incompatibles.

## Estructura

```text
xanda/
|-- .github/workflows/ci.yml
|-- include/xanda/xanda.h
|-- src/xanda.c
|-- src/xanda_runtime.c
|-- src/xanda_component_runtime.c
|-- src/xanda_state.c
|-- src/xanda_dev_runtime.c
|-- src/xanda_js_bridge.c
|-- examples/counter/
|-- examples/minimal/
|-- tests/browser/
|-- tests/test_xanda.c
|-- tools/dev-server/
|-- CHANGELOG.md
|-- LICENSE
|-- Makefile
|-- VERSION
`-- build/                         # salida generada
```

## Requisitos

- [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) instalado y activado para compilar ejemplos web.
- `make` o `mingw32-make`.
- Un compilador C nativo para `make test` como `cc` o `gcc`.
- Node.js para usar el servidor de desarrollo de Fase 1.

## Flujo recomendado

La nueva capa de alto nivel usa tres piezas:

- `XandaConfig`: configuracion con valores por defecto razonables.
- `XandaApp`: contexto de ejecucion de la app.
- `XandaComponent`: instancia con identidad estable, render y binding opcional de eventos.

Configuracion por defecto:

- `mount_target_id = "xanda"`
- `replace_target_children = 1`

Eso permite inicializar sin configurar nada:

```c
XandaApp app;
XandaComponent component;
XandaComponentDefinition definition = {
  "home",
  0,
  0,
  NULL,
  render_home,
  NULL,
  NULL,
  NULL,
  NULL
};

xanda_app_init(&app, NULL);
xanda_component_init(&component, &app, &definition, NULL, NULL, NULL);
xanda_component_mount(&component);
```

## API principal

### Capa de alto nivel

- `xanda_version_string()`: devuelve la version semantica estable del runtime.
- `xanda_config_default()`: devuelve la configuracion recomendada.
- `xanda_app_init(...)`: inicializa una app y completa defaults faltantes.
- `xanda_app_render(...)`: renderiza y monta un arbol ya construido.
- `xanda_component_init(...)`: registra una instancia de componente sobre una app ya inicializada.
- `xanda_component_mount(...)`: ejecuta el flujo render -> mount -> bind -> free manteniendo identidad estable.
- `xanda_component_id(...)`: expone el identificador estable de la instancia.
- `xanda_component_render_count(...)`: permite inspeccionar cuantos renders hizo la instancia.
- `xanda_component_snapshot(...)`: captura un snapshot del estado actual del componente.
- `xanda_component_restore(...)`: restaura el estado desde un snapshot compatible.
- `xanda_component_reload(...)`: reaplica una definicion de componente compatible preservando estado.
- `XANDA_VERSION_STRING`: macro publica con la version estable del runtime.
- `xanda_dev_restore_pending_component(...)`: recupera estado pendiente del entorno dev antes del primer mount.
- `xanda_dev_protocol_version(...)`: devuelve la version publica del protocolo dev.
- `xanda_dev_snapshot_root_component(...)`: expone el snapshot textual del componente raiz para tooling.
- `xanda_dev_describe_root_boundary(...)`: describe el boundary raiz activo con metadata estable para el host dev.
- `xanda_dev_list_boundaries(...)`: enumera los boundaries registrados por el runtime para inspeccion y tooling.

### Helpers del VDOM

- `xanda_create(...)`: crea un elemento.
- `xanda_create_with_text(...)`: crea un elemento y agrega un texto hijo.
- `xanda_text(...)`: crea un nodo de texto.
- `xanda_attrs(...)`: aplica varios atributos en una sola llamada.
- `xanda_append_text(...)`: agrega un texto hijo sin crear el nodo manualmente.
- `xanda_child(...)`: obtiene un hijo por indice con validacion.
- `xanda_set_key(...)`: asigna una clave estable a un nodo para prepararlo para reconciliacion futura.

### Estado serializable

Los componentes ahora pueden declarar estado versionado directamente en `XandaComponentDefinition`:

```c
typedef struct
{
  const char *name;
  unsigned int state_version;
  size_t state_size;
  XandaComponentSetupCallback setup;
  XandaComponentRenderCallback render;
  XandaComponentBindCallback bind;
  XandaComponentSnapshotCallback snapshot;
  XandaComponentRestoreCallback restore;
  XandaComponentDisposeCallback dispose;
} XandaComponentDefinition;
```

Si un componente define `state_size`, Xanda puede:

- copiar el estado por defecto a un `XandaStateSnapshot`;
- restaurarlo automaticamente cuando `state_version` y `state_size` coinciden;
- recargar la definicion del componente con `xanda_component_reload(...)` preservando el estado actual.

Para estados no triviales, puedes sobrescribir el comportamiento con `snapshot` y `restore`.

### Boundaries para HMR

Cada `XandaComponent` registrado participa ahora como boundary formal del runtime de desarrollo:

- el componente conserva una identidad estable por `instance_id`;
- el nodo raiz del render define la `key` estable del boundary;
- el runtime registra nombre, `key`, version de estado y tamano de estado;
- el host dev usa esa metadata para decidir si el swap del modulo puede preservar estado o debe abortarse.

El descriptor expuesto al tooling usa este formato textual:

```text
nombre|boundary_key|instance_id|render_count|state_version|state_size
```

Reglas actuales de compatibilidad del boundary raiz:

- `name` debe coincidir;
- `key` debe coincidir;
- `state_version` debe coincidir;
- `state_size` debe coincidir.

Si cualquiera de esos campos cambia entre la instancia anterior y la nueva, el host considera que el boundary ya no es compatible para hot swap con preservacion de estado.

El protocolo dev estable de `v1.0.0` expone ademas:

- `XANDA_DEV_PROTOCOL_VERSION`
- `xanda_dev_protocol_version()`

El host valida esa version antes de aceptar una nueva instancia del modulo Wasm.

### Render, backend y eventos

- `xanda_render_to_dom(...)`: transforma el arbol en nodos del backend activo.
- `xanda_backend_set(...)`: permite inyectar un backend custom, util para pruebas.
- `xanda_bind_event(...)`: registra eventos con validaciones descriptivas.
- `xanda_bind_click(...)`: helper para el caso mas comun.

### Errores

- `xanda_last_status()`: devuelve el ultimo estado.
- `xanda_last_error()`: devuelve el ultimo mensaje detallado.
- `xanda_set_error_handler(...)`: registra un handler centralizado.
- `xanda_status_message(...)`: documenta el significado general de cada estado.

## Mensajes y estados de error

Estados principales:

- `XANDA_STATUS_INVALID_ARGUMENT`: argumentos nulos o fuera de rango.
- `XANDA_STATUS_OUT_OF_MEMORY`: fallo de reserva o copia.
- `XANDA_STATUS_BACKEND_UNAVAILABLE`: no hay backend configurado para render o eventos.
- `XANDA_STATUS_RENDER_FAILED`: fallo al crear o ensamblar nodos en el backend.
- `XANDA_STATUS_TARGET_NOT_FOUND`: no existe el elemento destino para montar la vista.
- `XANDA_STATUS_EVENT_BIND_FAILED`: el evento no pudo registrarse o se intento registrar antes del render.

Ejemplos de mensajes concretos:

- `Xanda: no se puede registrar 'onclick' antes de renderizar el nodo.`
- `Xanda: no se encontro el contenedor '#xanda'.`
- `Xanda: la aplicacion debe inicializarse antes de renderizar.`

## Ejemplos incluidos

### Minimal

Caso base sin eventos y usando defaults:

```c
static XandaApp g_app;
static XandaComponent g_component;

static XandaNode *minimal_render(XandaComponent *component)
{
  XandaNode *root = xanda_create("main");
  XandaNode *title = xanda_create_with_text("h1", "Hola desde Xanda");

  (void)component;

  xanda_append(root, title);
  xanda_append_text(root, "Render con configuracion por defecto.");
  xanda_set_key(root, "minimal-root");
  return root;
}
```

### Counter

Caso interactivo organizado en dos archivos y usando el nuevo modelo de componente:

- `examples/counter/main.c`: punto de entrada, inicializacion y primer montaje.
- `examples/counter/counter.c`: definicion del componente, estado del contador y rerender.

El componente queda definido con identidad estable:

```c
static const XandaComponentDefinition COUNTER_COMPONENT = {
  "counter",
  1,
  sizeof(CounterState),
  NULL,
  counter_render,
  counter_bind,
  NULL,
  NULL,
  NULL
};

XandaStatus counter_init(XandaApp *app)
{
  return xanda_component_init(
      &g_counter.component,
      app,
      &COUNTER_COMPONENT,
      &g_counter.state,
      NULL,
      NULL);
}
```

## Crear un proyecto desde un ejemplo

La forma mas simple de empezar hoy con Xanda es usar uno de los ejemplos incluidos como base:

- `examples/minimal/`: punto de partida mas pequeno para una app sin eventos complejos.
- `examples/counter/`: punto de partida recomendado si quieres estado, eventos y una estructura modular con `main.c`.

### Opcion 1: reutilizar un ejemplo existente

Si quieres iterar rapidamente, puedes trabajar directamente sobre `counter` o `minimal`:

1. Copia la carpeta del ejemplo que mas se acerque a tu caso.
2. Renombra el contenido visual, estado y callbacks de ese ejemplo.
3. Mantiene el `index.html` como shell de la app.
4. Compila con el target correspondiente del `Makefile`.

Ejemplo de trabajo sobre `counter`:

```text
examples/
`-- counter/
    |-- index.html
    |-- main.c
    |-- counter.c
    `-- counter.h
```

En ese flujo, `main.c` actua como entry point de tu app y `counter.c` concentra la logica del componente principal.

### Opcion 2: crear un ejemplo nuevo a partir de uno existente

Si quieres una app con otro nombre, hoy el flujo recomendado es:

1. Copiar `examples/minimal/` o `examples/counter/` a `examples/<tu-app>/`.
2. Ajustar los nombres de archivos `.c` y `.h` segun tu proyecto.
3. Actualizar `examples/<tu-app>/index.html` para que apunte al script generado de tu app.
4. Registrar el nuevo ejemplo en `Makefile`.

Notas importantes sobre el `Makefile` actual:

- `EXAMPLES := counter minimal` define los ejemplos oficiales compilables por defecto.
- Cada ejemplo genera una salida en `build/<ejemplo>/`.
- Si agregas una app nueva, debes extender el `Makefile` con un target/export apropiado siguiendo el patron existente para `counter` y `minimal`.

Si no quieres tocar el `Makefile`, la via mas directa es reutilizar `counter` o `minimal` como base de tu proyecto real.

## Compilacion

Compila ambos ejemplos web:

```bash
make
```

Compila un ejemplo puntual:

```bash
make counter
make minimal
```

Ejecuta pruebas unitarias nativas:

```bash
make test
```

En Windows, si usas `mingw32-make`:

```bash
mingw32-make
mingw32-make test
```

## Bundle de produccion

Cuando ejecutas:

```bash
make counter
```

o:

```bash
make minimal
```

Xanda genera un artefacto estatico listo para despliegue en:

- `build/counter/`
- `build/minimal/`

Ese directorio funciona como el equivalente practico a un `dist/` o `bundle/` final, parecido al resultado de una compilacion de Vite: contiene el `index.html` y los artefactos web necesarios para publicar la app.

Ejemplo de salida para `counter`:

```text
build/
`-- counter/
    |-- index.html
    |-- counter.js
    `-- counter.wasm
```

Que contiene cada archivo:

- `index.html`: documento listo para servirse en produccion.
- `counter.js`: loader y runtime generado por Emscripten.
- `counter.wasm`: modulo WebAssembly de la app.

En otras palabras, para produccion no necesitas recompilar en el servidor ni montar tooling adicional: basta con publicar el contenido de `build/<ejemplo>/`.

### Publicar como `dist`

Si en tu pipeline prefieres trabajar con una carpeta llamada `dist/`, puedes copiar la salida final:

```bash
mkdir -p dist
cp -r build/counter/* dist/
```

En Windows PowerShell:

```powershell
New-Item -ItemType Directory -Force dist | Out-Null
Copy-Item build/counter/* dist/ -Recurse -Force
```

El contenido de `dist/` sera el bundle final desplegable.

## Desarrollo

El servidor de desarrollo en `tools/dev-server/` ya opera sobre un host separado del bundle compilado y actualmente ofrece:

- rebuild automatico al cambiar `src/`, `include/`, `examples/<ejemplo>/` o `Makefile`;
- hot swap visual del modulo por WebSocket, sin `window.location.reload()` cuando el host dev puede reemplazar la instancia;
- overlay simple cuando la compilacion falla;
- snapshot textual del componente raiz antes del reload;
- restauracion automatica del estado al arrancar la nueva instancia del modulo si la version del snapshot sigue siendo compatible;
- registro formal de boundaries del runtime para que el host pueda inspeccionar el componente raiz activo;
- comparacion explicita del boundary raiz antes de aceptar el swap del modulo;
- inyeccion automatica del cliente de desarrollo en los HTML servidos, sin modificar los `index.html` de los ejemplos;
- separacion entre host dev y bundle compilado: en modo `DEV=1` el HTML servido ya no ejecuta `counter.js` o `minimal.js` directamente;
- cero dependencias externas del lado del servidor: basta con Node.js.

Preparacion inicial:

```bash
make dev-setup
```

Flujo recomendado para `counter`:

```bash
make dev
```

Flujo para `minimal`:

```bash
make dev-minimal
```

Esto levanta un servidor en `http://127.0.0.1:5173/<ejemplo>/`.

Tambien puedes elegir ejemplo y puerto:

```bash
make serve EXAMPLE=counter DEV=1 DEV_SERVER_PORT=5173
make serve EXAMPLE=minimal DEV=1 DEV_SERVER_PORT=5180
```

Smoke test del servidor de desarrollo:

```bash
make test-browser
```

Chequeo completo de release local:

```bash
make release-check
```

Troubleshooting rapido:

- Si `make dev` falla por dependencias faltantes, ejecuta `make dev-setup`.
- `make dev-setup` solo verifica que Node.js este disponible; no instala paquetes.
- Si `emcc` no esta en `PATH`, el `Makefile` intentara usar `C:/emsdk/upstream/emscripten/emcc.bat`.
- Si el host no logra hacer swap en caliente, el cliente hara fallback a recarga completa de la pagina.
- Si el navegador no actualiza, revisa la consola y confirma que `__xanda_dev_host.js` y `__xanda_dev_client.js` se sirven desde el dev server.
- Si el estado no se restaura tras recompilar, revisa que `state_version` y `state_size` del componente sigan siendo compatibles con el snapshot previo.
- Si el swap falla aunque exista snapshot, revisa tambien que el `name` y la `key` estable del boundary raiz no hayan cambiado entre compilaciones.

Artefactos generados:

- `build/counter/counter.js`
- `build/counter/counter.wasm`
- `build/counter/index.html`
- `build/minimal/minimal.js`
- `build/minimal/minimal.wasm`
- `build/minimal/index.html`
- `build/tests/test_xanda.exe`

## Servir el bundle en produccion

Para produccion, piensa en `build/<ejemplo>/` como tu carpeta publicable.

Flujo tipico:

1. Compila el ejemplo o app base.
2. Toma `build/<ejemplo>/` como artefacto final.
3. Copia ese contenido al directorio servido por tu web server o CDN.

Ejemplo con `counter`:

```bash
make counter
```

La carpeta final a desplegar es:

```text
build/counter/
```

Si quieres probarla localmente como sitio estatico:

```bash
cd build/counter
python -m http.server 8000
```

Luego abre:

```text
http://localhost:8000/
```

## Despliegue con Nginx

Un despliegue de produccion con Nginx puede servir directamente el bundle generado.

Supongamos que copiaste `build/counter/` a:

```text
/var/www/xanda-counter
```

Una configuracion minima de Nginx seria:

```nginx
server {
    listen 80;
    server_name ejemplo.com;

    root /var/www/xanda-counter;
    index index.html;

    location / {
        try_files $uri $uri/ /index.html;
    }

    location ~* \.wasm$ {
        default_type application/wasm;
        add_header Cache-Control "public, max-age=31536000, immutable";
    }

    location ~* \.(js|css|png|jpg|jpeg|gif|svg|ico)$ {
        add_header Cache-Control "public, max-age=31536000, immutable";
    }
}
```

Puntos importantes para Nginx:

- `root` debe apuntar al directorio final del bundle, por ejemplo `build/counter/` ya copiado a tu servidor.
- `index index.html;` permite servir el shell de la app.
- `application/wasm` asegura que el navegador cargue correctamente el modulo WebAssembly.
- `try_files` ayuda si luego evolucionas a rutas que necesiten volver a `index.html`.

## Ejecutar los ejemplos en local

Despues de compilar, sirve `build/` con un servidor estatico:

```bash
cd build
python -m http.server 8000
```

Luego abre:

- `http://localhost:8000/counter/`
- `http://localhost:8000/minimal/`

## Pruebas incluidas

`tests/test_xanda.c` valida:

- configuracion por defecto;
- version publica del runtime y version del protocolo dev;
- helpers del VDOM;
- renderizado con backend falso;
- identidad estable, snapshot, restore y reload de componentes;
- protocolo dev de snapshot textual;
- registro de boundaries y enumeracion de metadata del boundary raiz;
- binding de eventos;
- mensajes de error cuando se intenta registrar un evento antes de renderizar.

## Makefile

Targets disponibles:

```bash
make all
make examples
make counter
make minimal
make test
make test-browser
make dev-setup
make dev
make dev-counter
make dev-minimal
make version
make release-check
make clean
make rebuild
make help
```

Artefactos de release:

- `VERSION`: version semantica del proyecto;
- `CHANGELOG.md`: historial de cambios publicado;
- `LICENSE`: licencia de distribucion;
- `.github/workflows/ci.yml`: verificacion automatica de pruebas nativas y dry-runs base.

## Compatibilidad

La superficie publica actual de Xanda esta centrada en componentes. El flujo recomendado y soportado es:

- `xanda_app_init()`
- `xanda_component_init()`
- `xanda_component_mount()`
- `xanda_component_snapshot()/restore()/reload()` para la evolucion hacia HMR real
- `xanda_dev_snapshot_root_component()/xanda_dev_describe_root_boundary()/xanda_dev_list_boundaries()` como protocolo dev para el host de hot swap
