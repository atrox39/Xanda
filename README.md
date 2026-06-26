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

`examples/minimal/` es la plantilla base recomendada para iniciar un proyecto nuevo.

Su estructura objetivo es:

```text
examples/minimal/
|-- index.html
|-- main.c
|-- bootstrap.c
|-- bootstrap.h
|-- minimal.c
|-- minimal.h
`-- styles/
    |-- app.scss
    `-- _tokens.scss
```

Responsabilidades:

- `index.html`: shell HTML y carga de `app.css` + `minimal.js`.
- `main.c`: entry point minimo de la app.
- `bootstrap.c`: inicializacion de `XandaApp`, handler de errores y arranque.
- `minimal.c`: componente o feature principal.
- `styles/app.scss`: entrypoint local de estilos del proyecto.
- `styles/_tokens.scss`: overrides locales de tema.

### Counter

`examples/counter/` usa la misma arquitectura base, pero muestra un caso con estado, eventos y rerender.

Responsabilidades:

- `main.c`: orquesta `counter_bootstrap_init()` y `counter_bootstrap_run()`.
- `bootstrap.c`: prepara el runtime y la feature.
- `counter.c`: encapsula estado, render y binding de eventos.
- `styles/`: personalizacion visual local sobre la base compartida.

## Arquitectura base para nuevos proyectos

La base obligatoria para nuevos proyectos en Xanda queda definida asi:

```text
examples/<app>/
|-- index.html
|-- main.c
|-- bootstrap.c
|-- bootstrap.h
|-- <feature>.c
|-- <feature>.h
|-- styles/
|   |-- app.scss
|   `-- _tokens.scss
`-- assets/
```

Y la capa compartida de estilos vive en:

```text
examples/shared/styles/
|-- xanda.scss
|-- _variables.scss
|-- _reset.scss
|-- _base.scss
|-- _utilities.scss
`-- _components.scss
```

Reglas de diseño:

- `main.c` debe mantenerse ligero y actuar solo como entry point.
- `bootstrap.c` centraliza setup, configuracion y arranque.
- el modulo del feature concentra render, estado y eventos.
- `styles/app.scss` importa la base compartida y aplica overrides locales.
- nuevos modulos o recursos deben agregarse sin modificar la estructura base.

## Crear un proyecto desde un ejemplo

La forma mas simple de empezar hoy con Xanda es partir desde `examples/minimal/`.

### Flujo recomendado

1. Copia `examples/minimal/` a `examples/<tu-app>/`.
2. Renombra `minimal.c` y `minimal.h` al nombre de tu feature o modulo principal.
3. Ajusta `bootstrap.c` para inicializar tu modulo.
4. Actualiza `index.html` con el titulo y `data-app` de tu proyecto.
5. Personaliza `styles/_tokens.scss` y `styles/app.scss`.
6. Registra tu nuevo ejemplo en `Makefile`.

Ejemplo de proyecto nuevo:

```text
examples/dashboard/
|-- index.html
|-- main.c
|-- bootstrap.c
|-- bootstrap.h
|-- dashboard.c
|-- dashboard.h
|-- styles/
|   |-- app.scss
|   `-- _tokens.scss
`-- assets/
```

Si necesitas un ejemplo con estado y eventos desde el inicio, puedes partir de `examples/counter/`.

## Estilos y personalizacion

Xanda usa ahora una base compartida + local:

- `examples/shared/styles/`: tokens y componentes reutilizables del starter.
- `examples/<app>/styles/_tokens.scss`: overrides por proyecto.
- `examples/<app>/styles/app.scss`: entrada local de estilos.

La personalizacion debe hacerse mediante variables y clases reutilizables, no con atributos `style` hardcodeados en C.

Ejemplos de clases base:

- `app-shell`
- `app-container`
- `stack`
- `surface-card`
- `button`
- `button--primary`

## Compilacion

Instala primero las dependencias del tooling:

```bash
make dev-setup
```

Eso instala Sass en `tools/dev-server/` usando `pnpm`.

Compila ambos ejemplos web:

```bash
make
```

Compila un ejemplo puntual:

```bash
make counter
make minimal
```

Genera el bundle final de produccion:

```bash
make dist-counter
make dist-minimal
```

O de forma parametrica:

```bash
make dist EXAMPLE=counter
make dist EXAMPLE=minimal
```

Ejecuta pruebas unitarias nativas:

```bash
make test
```

En Windows, si usas `mingw32-make`:

```bash
mingw32-make dev-setup
mingw32-make
mingw32-make dist EXAMPLE=counter
mingw32-make test
```

## Build y dist

Xanda ahora maneja dos niveles de salida:

- `build/<ejemplo>/`: salida tecnica del proceso de compilacion.
- `dist/<ejemplo>/`: bundle final listo para despliegue.

### Contenido de `build/<ejemplo>/`

Ejemplo para `counter`:

```text
build/counter/
|-- app.css
|-- counter.js
|-- counter.wasm
|-- index.html
`-- assets/
```

### Contenido de `dist/<ejemplo>/`

`dist/<ejemplo>/` copia el artefacto final publicable y es el equivalente real al resultado tipo Vite para este stack.

Ejemplo:

```text
dist/counter/
|-- app.css
|-- counter.js
|-- counter.wasm
|-- index.html
`-- assets/
```

Que contiene cada archivo:

- `index.html`: shell final de la app.
- `app.css`: estilos compilados desde Sass.
- `<ejemplo>.js`: loader y runtime generado por Emscripten.
- `<ejemplo>.wasm`: modulo WebAssembly de la app.
- `assets/`: recursos locales del proyecto, si existen.

## Desarrollo

El servidor de desarrollo en `tools/dev-server/` ya opera sobre un host separado del bundle compilado y actualmente ofrece:

- rebuild automatico al cambiar `src/`, `include/`, `examples/<ejemplo>/`, `styles/` o `Makefile`;
- hot swap visual del modulo por WebSocket, sin `window.location.reload()` cuando el host dev puede reemplazar la instancia;
- overlay simple cuando la compilacion falla;
- snapshot textual del componente raiz antes del reload;
- restauracion automatica del estado al arrancar la nueva instancia del modulo si la version del snapshot sigue siendo compatible;
- registro formal de boundaries del runtime para que el host pueda inspeccionar el componente raiz activo;
- comparacion explicita del boundary raiz antes de aceptar el swap del modulo;
- soporte para recompilar Sass y volver a servir `app.css`;
- inyeccion automatica del cliente de desarrollo en los HTML servidos, sin modificar los `index.html` de los ejemplos.

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
- `make dev-setup` instala Sass en `tools/dev-server/`.
- Si `emcc` no esta en `PATH`, el `Makefile` intentara usar `C:/emsdk/upstream/emscripten/emcc.bat`.
- Si el host no logra hacer swap en caliente, el cliente hara fallback a recarga completa de la pagina.
- Si el navegador no actualiza, revisa la consola y confirma que `__xanda_dev_host.js`, `__xanda_dev_client.js` y `app.css` se sirven desde el dev server.
- Si el estado no se restaura tras recompilar, revisa que `state_version` y `state_size` del componente sigan siendo compatibles con el snapshot previo.
- Si el swap falla aunque exista snapshot, revisa tambien que el `name` y la `key` estable del boundary raiz no hayan cambiado entre compilaciones.

## Servir el bundle en produccion

Para produccion, toma `dist/<ejemplo>/` como el bundle final publicable.

Ejemplo:

```bash
make dist-counter
```

La carpeta final a desplegar sera:

```text
dist/counter/
```

Si quieres probarla localmente como sitio estatico:

```bash
cd dist/counter
python -m http.server 8000
```

Luego abre:

```text
http://localhost:8000/
```

## Despliegue con Nginx

Un despliegue de produccion con Nginx puede servir directamente `dist/<ejemplo>/`.

Supongamos que copiaste `dist/counter/` a:

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

    location ~* \.(js|css|png|jpg|jpeg|gif|svg|ico|webp)$ {
        add_header Cache-Control "public, max-age=31536000, immutable";
    }
}
```

Puntos importantes para Nginx:

- `root` debe apuntar al directorio final del bundle, por ejemplo `dist/counter/`.
- `index index.html;` permite servir el shell de la app.
- `application/wasm` asegura que el navegador cargue correctamente el modulo WebAssembly.
- `app.css` ya forma parte del bundle final y no requiere pipeline adicional en el servidor.
- `try_files` ayuda si luego evolucionas a rutas que necesiten volver a `index.html`.

## Ejecutar los ejemplos en local

Despues de compilar, puedes servir `build/` como salida tecnica:

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
make dist EXAMPLE=counter
make dist-counter
make dist-minimal
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
