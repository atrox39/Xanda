# Plan de implementacion hacia Hot Module Reload real en Xanda

## Resumen

Objetivo: evolucionar Xanda desde su base estable `v1.0.0`, que ya ofrece runtime de componentes, snapshots, boundaries formales y hot swap seguro del modulo raiz, hasta un sistema con Hot Module Reload real por componente, con preservacion o migracion de estado cuando el cambio sea compatible.

Meta final del roadmap:

- HMR real por componente.
- Host/runtime de desarrollo en JS/TS permitido.
- Nueva API formal de componentes como camino principal.
- Sin modo legacy: la base estable posterior a `v1.0.0` sigue centrada solo en componentes.

El plan se divide en fases incrementales porque intentar HMR real directamente sobre la arquitectura actual no es rentable ni seguro.

## Analisis del estado actual

### Hechos observados en el repo

- La API publica actual esta en `include/xanda/xanda.h`.
- El runtime central esta en `src/xanda_runtime.c`.
- El backend DOM del navegador esta en `src/xanda_js_bridge.c`.
- La construccion de UI actual se basa en `XandaNode`, `XandaApp`, `XandaView` y `XandaBackendOps`.
- El flujo de render recomendado hoy es `xanda_app_init()` + `xanda_app_render_view()`, documentado en `README.md`.
- El ejemplo `examples/counter/` usa estado global en C y rerender completo del arbol en cada evento.
- El build web es monolitico por ejemplo desde `Makefile`, generando un solo `counter.js` y `counter.wasm` por entrada.
- Las pruebas actuales (`tests/test_xanda.c`) cubren helpers de VDOM, backend falso, errores y bind de eventos, pero no cubren reconciliacion, boundaries de componente, serializacion de estado ni flujo de desarrollo.

### Limitaciones actuales que bloquean HMR real

1. Build monolitico:
   - `Makefile` compila un unico modulo WASM por ejemplo.
   - No existe nocion de chunk, modulo o boundary recargable por componente.

2. Modelo de UI insuficiente para HMR:
   - `XandaView` es un callback de render + bind, no un componente con identidad estable.
   - `XandaNode` modela estructura de salida, pero no define instancias de componente, props, lifecycle ni boundaries actualizables.

3. Estado acoplado a la memoria C/WASM:
   - El ejemplo `counter.c` mantiene estado en variables globales C.
   - No existe contrato de serializacion/restauracion/migracion de estado.

4. Eventos acoplados a simbolos exportados:
   - El bridge usa nombres de funciones C exportadas (`Module.ccall(...)`).
   - No existe tabla de handlers estable por instancia ni capa de indirecta para rebind tras hot swap.

5. Render completo en lugar de reconciliacion:
   - El runtime hace render -> mount -> bind -> free.
   - No existe scheduler, diff por componente, preservacion de instancia ni actualizacion parcial segura.

6. Tooling de desarrollo inexistente:
   - No hay dev server, overlay, watcher, websocket, cliente HMR ni protocolo de actualizacion.

7. Cobertura de pruebas insuficiente para el objetivo:
   - No hay pruebas de compatibilidad ABI, recarga de modulo, persistencia de estado, actualizacion incremental ni degradacion a full reload.

## Decisiones y supuestos cerrados

- Objetivo final: HMR real por componente, no solo live reload.
- Se permite introducir un host/runtime de desarrollo en JS/TS.
- Se acepta una nueva API formal de componentes como camino recomendado.
- La base estable `v1.0.0` ya esta cerrada sobre `XandaComponent`, snapshots, boundaries y host dev.
- El roadmap post-`v1.0.0` debe priorizar HMR por componente real, no mas capas de compatibilidad.

## Riesgos principales

### Riesgos tecnicos

- Riesgo de ABI inestable entre versiones de componentes WASM.
- Riesgo de perder estado o corromperlo cuando cambie su shape.
- Riesgo de fugas o listeners stale si se reemplaza un componente sin `dispose`.
- Riesgo de que el bridge JS y el runtime C diverjan y rompan compatibilidad.
- Riesgo de acoplar demasiado la API publica a detalles del host JS/TS.
- Riesgo de tiempos de compilacion elevados si el chunking/modularizacion no se diseña bien.

### Riesgos de producto

- Riesgo de romper la simplicidad actual si el nuevo modelo de componentes se introduce de golpe.
- Riesgo de mantener dos modelos demasiado tiempo y duplicar complejidad.
- Riesgo de prometer “HMR real” antes de tener migracion segura de estado y fallback a full reload.

### Riesgos operativos

- Riesgo de debugging complejo entre C, WASM, JS host y bridge DOM.
- Riesgo de tests lentos o poco confiables si no se separan correctamente pruebas nativas, pruebas browser y pruebas HMR.

## Estrategia general de implementacion

La implementacion debe avanzar en cinco fases acumulativas:

1. Base de desarrollo profesional.
2. Nuevo modelo formal de componentes.
3. Estado serializable y pseudo-HMR confiable.
4. Runtime host + protocolo HMR por componente.
5. HMR real con boundaries, compatibilidad y fallback seguro.

No se debe intentar la fase 4 o 5 antes de completar criterios minimos de la fase 2 y 3.

## Cambios propuestos por fase

### Fase 1: Base de desarrollo profesional

#### Objetivo

Construir una experiencia de desarrollo moderna y observable antes de tocar HMR real.

#### Archivos existentes a modificar

- `Makefile`
  - Que: agregar targets de desarrollo (`dev`, `serve`, `watch`, `dev-counter` o equivalentes).
  - Por que: hoy solo compila ejemplos o corre tests nativos.
  - Como: separar build de produccion y build de desarrollo, habilitar flags de debug, sourcemaps y un flujo de watcher.

- `README.md`
  - Que: documentar modo desarrollo, prerequisites extra y flujo de trabajo.
  - Por que: la experiencia profesional empieza por onboarding claro.
  - Como: agregar seccion “Desarrollo” con pasos concretos y troubleshooting.

#### Archivos/directorios nuevos a introducir

- `tools/dev-server/` o `devtools/`
  - `package.json`
  - `src/server.ts`
  - `src/client.ts`
  - `tsconfig.json`
  - Que: dev server con WebSocket y cliente de live reload.
  - Por que: crear base reutilizable para pseudo-HMR y HMR real.
  - Como: servir `build/`, detectar recompilaciones, inyectar cliente y mostrar overlay de errores.

- `tests/browser/`
  - pruebas de humo del flujo `counter` y `minimal`.
  - Por que: validar que el build web sigue funcionando de forma automatizada.

#### Entregables

- Watch + rebuild.
- Live reload automatico.
- Overlay simple de errores.
- Pruebas browser basicas.

#### Riesgos mitigados

- reduce friccion de DX;
- establece el host JS/TS que luego soportara HMR real.

### Fase 2: Nuevo modelo formal de componentes

#### Objetivo

Introducir boundaries de componente estables, separados del VDOM de salida.

#### Archivos existentes a modificar

- `include/xanda/xanda.h`
  - Que: agregar nueva API de componentes.
  - Por que: `XandaView` no es suficiente para HMR real.
  - Como: introducir tipos nuevos, por ejemplo:
    - `XandaComponentType`
    - `XandaComponentInstance`
    - `XandaProps`
    - hooks/funciones de lifecycle (`init`, `render`, `bind`, `dispose`, `serialize_state`, `restore_state`).

- `src/xanda_runtime.c`
  - Que: soportar registro de componentes, instanciacion y boundaries.
  - Por que: hoy solo existe render de arbol y mount.
  - Como: agregar un registro de tipos, tabla de instancias vivas, ids estables y API de montaje por componente raiz.

- `src/xanda.c`
  - Que: extender o adaptar `XandaNode` para referenciar boundaries o claves estables.
  - Por que: el reconciliador necesitara identidad semantica, no solo estructura de nodos.
  - Como: introducir `key`, `component_owner_id` o metadatos equivalentes.

- `src/xanda_js_bridge.c`
  - Que: desacoplar eventos de nombres de simbolos C directos.
  - Por que: el rebind tras hot swap no puede depender de strings estaticos solamente.
  - Como: introducir una tabla de dispatch con ids estables y capa de indirecta en JS host.

- `examples/counter/*`
  - Que: migrar `counter` a la nueva API de componentes.
  - Por que: debe convertirse en el primer vertical end-to-end.
  - Como: `counter.c` pasa a definir un componente formal y `main.c` monta una raiz basada en la nueva API.

#### Archivos/directorios nuevos a introducir

- `include/xanda/component.h` o integrarlo en `xanda.h`
- `src/xanda_component_runtime.c`
  - Que: implementar registro, lifecycle, boundaries e instancias.
  - Por que: evitar seguir cargando `xanda_runtime.c` con responsabilidades divergentes.

- `tests/components/`
  - pruebas de identidad, lifecycle y disposal.

#### Entregables

- componente con identidad estable;
- boundary explicitamente registrable;
- API legacy mantenida pero marcada como modo compatibilidad.

#### Riesgos mitigados

- resuelve la carencia principal de boundaries para HMR;
- reduce el acoplamiento del render completo.

### Fase 3: Estado serializable y pseudo-HMR confiable

#### Objetivo

Poder recompilar o reinstanciar el modulo sin perder estado cuando el componente sea compatible.

#### Archivos existentes a modificar

- `include/xanda/xanda.h`
  - Que: exponer contrato de serializacion/restauracion/migracion.
  - Como: callbacks del tipo:
    - `serialize_state(instance) -> bytes/json`
    - `restore_state(instance, payload)`
    - `can_accept_hot_update(old_meta, new_meta)`

- `src/xanda_runtime.c` y/o `src/xanda_component_runtime.c`
  - Que: administrar snapshots de estado por componente.
  - Por que: el estado hoy vive en globals C sin contrato de persistencia.
  - Como: registrar stores por componente y soporte para restauracion tras reload.

- `examples/counter/counter.c`
  - Que: mover `g_state` a un estado de componente serializable.
  - Por que: el ejemplo debe validar preservacion real de estado.

#### Archivos/directorios nuevos a introducir

- `src/xanda_state.c`
  - Que: helpers de serializacion/migracion/versionado de estado.

- `tests/state/`
  - Que: pruebas de snapshot, restore, migracion compatible e incompatible.

- `tests/browser/hot-state/`
  - Que: pruebas browser que verifiquen preservacion de estado durante recarga de modulo.

#### Entregables

- pseudo-HMR: recarga total del modulo WASM con restauracion de estado;
- criterio objetivo de compatibilidad o fallback a full reload.

#### Riesgos mitigados

- evita perder contexto del usuario;
- crea la base imprescindible para HMR real.

### Fase 4: Runtime host y protocolo HMR por componente

#### Objetivo

Mover la inteligencia de desarrollo a un host JS/TS que pueda coordinar updates selectivos.

#### Archivos existentes a modificar

- `Makefile`
  - Que: integrar build de metadata HMR y modo dev del host.
  - Como: generar manifiestos por componente, exports y hashes.

- `src/xanda_js_bridge.c`
  - Que: integrar al bridge con el host HMR y la tabla de dispatch.
  - Como: reemplazar llamadas directas por mensajes/ids controlados por el host.

#### Archivos/directorios nuevos a introducir

- `runtime-host/` o `packages/xanda-dev-runtime/`
  - `src/hmr-runtime.ts`
  - `src/component-registry.ts`
  - `src/module-loader.ts`
  - `src/state-store.ts`
  - `src/error-overlay.ts`
  - `src/protocol.ts`
  - Que: runtime JS/TS estable que:
    - mantiene el registry de componentes;
    - recibe eventos del dev server;
    - descarga nuevos artefactos;
    - decide si aplicar hot update, pseudo-HMR o full reload.

- `build-manifests/` o salida generada equivalente
  - manifiestos por componente:
    - id
    - version/hash
    - exports
    - compatibilidad de estado
    - chunk/artefacto asociado

- `tests/protocol/`
  - pruebas del protocolo de update y degradacion segura.

#### Entregables

- protocolo HMR explícito;
- runtime host capaz de cargar versiones nuevas;
- decision engine para update selectivo vs full reload.

#### Riesgos mitigados

- evita meter toda la logica de HMR dentro del bridge C/WASM;
- profesionaliza tooling y observabilidad.

### Fase 5: HMR real por componente

#### Objetivo

Aplicar actualizaciones selectivas a componentes compatibles sin recargar toda la pagina.

#### Archivos existentes a modificar

- `src/xanda_runtime.c`
  - Que: incorporar reconciliacion por boundaries e invalidacion selectiva.
  - Como: detectar subarbol afectado y rerenderizar solo ese componente o subtree propietario.

- `src/xanda_component_runtime.c`
  - Que: soporte a reemplazo de implementacion en caliente.
  - Como: rebind de lifecycle y re-evaluacion de compatibilidad de estado.

- `src/xanda_js_bridge.c`
  - Que: mantener los nodos DOM y listeners cuando el update sea parcial y compatible.
  - Como: no depender del remount global, sino del boundary update.

- `examples/counter/*`
  - Que: caso de demostracion HMR real.
  - Como: modificar UI/estado y verificar preservacion durante hot update.

#### Archivos/directorios nuevos a introducir

- `tests/hmr-real/`
  - Que: pruebas end-to-end de:
    - cambio de render sin perder estado;
    - cambio compatible de props/estado;
    - cambio incompatible con fallback a full reload;
    - recuperacion tras errores de compilacion;
    - rebind de eventos sin listeners zombie.

#### Entregables

- HMR real por componente;
- fallback automatico a pseudo-HMR o full reload;
- overlay y logs claros cuando el cambio no pueda aplicarse en caliente.

## Propuesta de cambios concretos por archivo

### `include/xanda/xanda.h`

- Mantener API actual como legacy.
- Agregar nueva API formal de componentes:
  - definicion de tipo de componente;
  - instancias;
  - lifecycle;
  - serializacion y restauracion de estado;
  - metadata de compatibilidad para hot update.

### `src/xanda_runtime.c`

- Reducir su rol a coordinador de app + runtime base.
- Extraer responsabilidades de componentes/estado/HMR a archivos dedicados.
- Conservar `XandaApp` como superficie de alto nivel, pero adaptarlo para montar roots basados en componentes.

### `src/xanda_js_bridge.c`

- Mantener responsabilidades DOM puras.
- Sacar de aqui cualquier logica que sea propia del protocolo HMR.
- Introducir integracion con ids de handlers y boundaries estables en lugar de depender solo de nombres exportados.

### `src/xanda.c`

- Extender `XandaNode` con identidad estable (`key`, owner o metadata equivalente).
- Preparar el terreno para reconciliacion por subarbol.

### `Makefile`

- Separar claramente build:
  - prod;
  - dev;
  - pseudo-HMR;
  - tests browser;
  - tests HMR.
- Integrar host JS/TS y generacion de metadata HMR.

### `examples/counter/main.c`

- Convertirlo en ejemplo canonico de root/mount con la nueva API de componente.

### `examples/counter/counter.c`

- Migrarlo a componente formal con estado serializable.
- Hacerlo primer caso de validacion de hot update compatible.

### `examples/minimal/minimal.c`

- Mantenerlo como ejemplo onboarding.
- Eventualmente agregar variante usando la nueva API para no dejar la documentacion partida.

### `tests/test_xanda.c`

- Mantenerlo para pruebas nativas del runtime base.
- No sobrecargarlo con HMR; crear suites nuevas separadas por concern.

### `README.md`

- Documentar:
  - roadmap de migracion;
  - nueva API de componentes;
  - modo legacy;
  - dev runtime;
  - fallback de compatibilidad;
  - criterios de HMR y limites.

## Plan de migracion y compatibilidad

### Etapa posterior a v1.0.0

- Mantener estable la API basada en componentes ya publicada.
- Considerar experimental cualquier trabajo de HMR por subarbol hasta que tenga pruebas E2E dedicadas.
- Evitar reintroducir compatibilidad legacy o adaptadores que mezclen modelos.

### Regla de degradacion

- Si un componente no implementa serializacion/restauracion:
  - fallback a full reload.
- Si cambia el contrato de estado y no hay migrador:
  - fallback a full reload.
- Si el runtime detecta incompatibilidad ABI:
  - rechazar HMR parcial y hacer pseudo-HMR o full reload.

## Criterios de aceptacion por fase

### Fase 1

- `make dev` o flujo equivalente levanta servidor y recarga automatica.
- Errores de compilacion se reflejan en overlay y logs claros.

### Fase 2

- Existe nueva API de componentes con identidad estable.
- `counter` usa la nueva API.
- Las vistas legacy siguen funcionando.

### Fase 3

- `counter` preserva estado tras reinstanciar el modulo.
- Hay pruebas de estado compatible/incompatible.

### Fase 4

- El host JS/TS recibe eventos de cambio y carga metadata por componente.
- El runtime decide entre hot update, pseudo-HMR y full reload.

### Fase 5

- Cambios compatibles en `counter` se aplican sin recargar toda la pagina.
- El estado se conserva cuando la version lo permite.
- Cambios incompatibles degradan de forma segura.

## Verificacion

### Verificacion continua durante implementacion

- Pruebas nativas:
  - mantener `make test`.

- Pruebas browser:
  - abrir `counter` y `minimal` en modo dev;
  - validar render inicial, eventos y logs.

- Pruebas HMR:
  - cambiar texto/markup de componente y verificar update parcial;
  - cambiar handlers y verificar rebind;
  - cambiar shape del estado y verificar migracion o fallback.

### Verificacion final del roadmap

- Demostracion de HMR real sobre `examples/counter/`.
- Demostracion de fallback cuando el cambio no es compatible.
- Documentacion actualizada con flujo dev y API nueva.
- Suites automatizadas separadas para runtime base, browser, estado y HMR.

## Orden recomendado de ejecucion

1. Fase 1 completa.
2. Fase 2 sobre `counter` como vertical piloto.
3. Fase 3 con snapshots/migracion de estado.
4. Fase 4 con runtime host y protocolo.
5. Fase 5 con HMR real por componente.
6. Una vez estabilizado `counter`, migrar `minimal` y luego promover la nueva API como default.

## No hacer todavia

- No intentar patching arbitrario de funciones C dentro de una instancia WASM viva.
- No prometer HMR real sin antes tener boundaries de componente y estado serializable.
- No esconder la complejidad dentro de `xanda_js_bridge.c`; el host JS/TS debe absorber el protocolo de desarrollo.
