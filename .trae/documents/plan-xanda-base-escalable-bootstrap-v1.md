# Plan de implementacion: base escalable con bootstrap modular

## Resumen

Objetivo: refactorizar el proyecto completo para establecer una base inicial minima, legible y escalable, alineada con un flujo de arranque tipo Vite + React, pero adaptada al stack real de Xanda en C + WebAssembly + Emscripten.

Resultado esperado:

- una estructura base obligatoria para ejemplos y nuevos proyectos con `index.html`, `main.c` y `bootstrap.c`;
- una capa de estilos modular con Sass real, variables centralizadas y overrides locales;
- una salida formal tipo `dist` lista para despliegue, incluyendo `index.html`, `.js`, `.wasm`, `.css` y assets estaticos;
- ejemplos y tooling alineados con la nueva arquitectura;
- documentacion y validaciones que demuestren funcionamiento en desarrollo y produccion.

Decisiones ya cerradas:

- alcance: todo el repo;
- estilos: Sass real;
- build: incluir desde ahora un flujo formal tipo `dist`;
- organizacion de recursos: capa compartida + capa local por ejemplo/proyecto.

## Analisis del estado actual

### Estado del runtime y ejemplos

- `examples/minimal/minimal.c` mezcla render y arranque en un solo archivo; hoy no existe `bootstrap.c`.
- `examples/counter/main.c` ya separa el entry point de la logica del componente, pero no existe una capa comun de bootstrap reutilizable.
- `examples/counter/counter.c` y `examples/minimal/minimal.c` usan estilos inline mediante atributos `style`, lo que vuelve la UI rigida, poco reusable y dificil de tematizar.
- `examples/counter/index.html` y `examples/minimal/index.html` solo cargan el script JS generado; no hay importacion de CSS ni carpeta de assets.

### Estado del build

- `Makefile` solo compila fuentes C y copia `index.html` a `build/<ejemplo>/`.
- No existe pipeline para Sass, CSS compartido, assets locales ni salida formal tipo `dist`.
- El servidor de desarrollo en `tools/dev-server/src/server.mjs` ya sabe servir `.css`, pero hoy el repo no produce ni copia CSS para los ejemplos.
- `tools/dev-server/package.json` existe, pero solo cubre el dev server; no hay pipeline frontend para Sass ni empaquetado estatico.

### Estado de documentacion

- `README.md` documenta build, dev server y despliegue del bundle generado, pero no define todavia una arquitectura base obligatoria con `bootstrap.c`.
- El README ya reconoce `build/<ejemplo>/` como artefacto publicable, asi que la nueva salida `dist` debe integrarse sin romper esa narrativa.

### Hallazgos relevantes para el plan

- No hay Bootstrap CSS real en el repo; el problema a resolver es la rigidez del arranque y de los estilos inline.
- Existe un helper interno llamado `js_bootstrap_runtime` en `src/xanda_js_bridge.c`, pero es parte del runtime JS interno y no sustituye un `bootstrap.c` a nivel de proyecto.
- La mejor base actual para la nueva plantilla es combinar la simplicidad de `examples/minimal/` con la separacion de responsabilidades ya lograda en `examples/counter/main.c`.

## Propuesta de cambios

### 1. Definir una arquitectura base obligatoria para ejemplos y nuevos proyectos

#### Archivos a introducir o reorganizar

- `examples/minimal/main.c`
  - Que: mover el punto de entrada desde `minimal.c` a `main.c`.
  - Por que: `minimal` debe alinearse con la arquitectura canonica del proyecto.
  - Como: `main.c` inicializa el runtime, delega a `bootstrap.c` y lanza el montaje inicial.

- `examples/minimal/bootstrap.c`
- `examples/minimal/bootstrap.h`
  - Que: crear la capa de arranque e inicializacion para `minimal`.
  - Por que: convertir el ejemplo minimo en la plantilla oficial de nuevos proyectos.
  - Como: centralizar setup de app, handler de errores, restauracion dev y configuracion base en funciones reutilizables.

- `examples/minimal/minimal.c`
- `examples/minimal/minimal.h`
  - Que: reducir `minimal.c` a logica del componente y exponer una interfaz limpia.
  - Por que: separar bootstrap, entry point y componente, mejorando legibilidad y escalabilidad.
  - Como: mover cualquier inicializacion ajena al componente a `bootstrap.c`, conservar solo render, bind y helpers del feature.

- `examples/counter/bootstrap.c`
- `examples/counter/bootstrap.h`
  - Que: introducir la misma capa de bootstrap en `counter`.
  - Por que: la arquitectura base debe ser transversal a todos los proyectos nuevos y ejemplos existentes.
  - Como: encapsular inicializacion de `XandaApp`, configuracion del entorno y arranque del feature `counter`.

- `examples/counter/main.c`
  - Que: simplificar `main.c` para que solo orqueste bootstrap + run.
  - Por que: que el entry point quede ligero y consistente con `minimal`.
  - Como: delegar setup y restauracion dev al bootstrap.

- `examples/counter/counter.c`
- `examples/counter/counter.h`
  - Que: limpiar nombres, separar bloques funcionales y eliminar estilos inline.
  - Por que: mejorar legibilidad y alinear el componente con la nueva capa de estilos.
  - Como: reemplazar `style` hardcodeado por clases CSS y helpers mas expresivos.

#### Estructura objetivo por ejemplo

Cada ejemplo debe converger al siguiente patron:

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

### 2. Crear una capa de estilos Sass compartida + local

#### Archivos nuevos compartidos

- `examples/shared/styles/_variables.scss`
  - Que: variables globales de tema y tokens base.
  - Por que: evitar estilos hardcodeados y permitir personalizacion centralizada.
  - Como: definir colores, tipografia, spacing, radius, shadows, layout y breakpoints.

- `examples/shared/styles/_reset.scss`
- `examples/shared/styles/_base.scss`
- `examples/shared/styles/_utilities.scss`
- `examples/shared/styles/_components.scss`
  - Que: reset, estilos base, utilidades y componentes reutilizables.
  - Por que: habilitar una base comun escalable para cualquier proyecto nuevo.
  - Como: exponer clases modulares tipo contenedor, stack, card, button, heading, section y utilidades de spacing/layout.

- `examples/shared/styles/xanda.scss`
  - Que: entrypoint Sass compartido.
  - Por que: compilar una base CSS reutilizable y versionable para todos los ejemplos.
  - Como: importar parciales compartidos y exponer el contrato de estilos comun.

#### Archivos nuevos locales por ejemplo

- `examples/minimal/styles/app.scss`
- `examples/minimal/styles/_tokens.scss`
- `examples/counter/styles/app.scss`
- `examples/counter/styles/_tokens.scss`
  - Que: capa local por app para overrides y estilos propios.
  - Por que: mantener el equilibrio entre base compartida y personalizacion local.
  - Como: importar la base compartida, redefinir variables locales y declarar estilos especificos del feature.

#### Cambios en HTML y componentes

- `examples/minimal/index.html`
- `examples/counter/index.html`
  - Que: referenciar el CSS compilado y limpiar la shell HTML.
  - Por que: el HTML base debe importar recursos centralizados y dejar listo el bundle de produccion.
  - Como: agregar `<link rel="stylesheet" href="./app.css">`, metadatos coherentes y una estructura base ligera.

- `examples/minimal/minimal.c`
- `examples/counter/counter.c`
  - Que: sustituir atributos `style` por clases semanticas y reutilizables.
  - Por que: separar completamente estructura y presentacion.
  - Como: usar `class="app-shell"`, `class="card"`, `class="button button--primary"` o equivalentes, manteniendo nombres consistentes.

### 3. Endurecer legibilidad y convenciones del codigo C

#### Archivos afectados

- `examples/minimal/minimal.c`
- `examples/minimal/bootstrap.c`
- `examples/minimal/main.c`
- `examples/counter/counter.c`
- `examples/counter/bootstrap.c`
- `examples/counter/main.c`
- headers asociados

#### Lineamientos a aplicar

- nombres de funciones orientados a responsabilidad, por ejemplo:
  - `minimal_render_view`
  - `minimal_bootstrap_init`
  - `minimal_bootstrap_run`
  - `counter_feature_init`
  - `counter_feature_mount`
- comentarios cortos solo en secciones complejas:
  - arranque del runtime;
  - restauracion dev;
  - armado del arbol;
  - enlace de eventos.
- bloques funcionales claros:
  - tipos y estado;
  - helpers privados;
  - definicion del componente;
  - API del modulo;
  - entry point.
- eliminacion de redundancias:
  - setup repetido entre `counter` y `minimal`;
  - estilos repetidos hoy embebidos;
  - inicializacion dispersa entre `main.c` y el feature.

### 4. Implementar pipeline real de Sass y salida tipo dist

#### Cambios en tooling

- `tools/dev-server/package.json`
  - Que: ampliar scripts y dependencias para compilar Sass.
  - Por que: el proyecto ahora requiere Sass real, no solo CSS estatico.
  - Como: agregar dependencia `sass` y scripts de compilacion para estilos.

- `tools/dev-server/` o nueva carpeta `tools/build/`
  - Que: incorporar un script de build estatico para ejemplos.
  - Por que: hoy el `Makefile` solo compila C y copia HTML.
  - Como: crear un script Node que:
    - compile Sass compartido + local;
    - copie `index.html`;
    - copie assets del ejemplo;
    - opcionalmente copie solo los CSS necesarios al bundle final.

#### Cambios en `Makefile`

- `Makefile`
  - Que: agregar targets de assets y dist, manteniendo los actuales.
  - Por que: la salida del proyecto debe incluir CSS y assets de manera formal.
  - Como:
    - agregar `check-style-tools` o equivalente;
    - agregar build de Sass antes de cerrar `build/<ejemplo>/`;
    - agregar targets tipo:
      - `make dist EXAMPLE=counter`
      - `make dist-counter`
      - `make dist-minimal`
    - hacer que `build/<ejemplo>/` y/o `dist/<ejemplo>/` contengan:
      - `index.html`
      - `<ejemplo>.js`
      - `<ejemplo>.wasm`
      - `app.css`
      - assets locales requeridos.

#### Decision de salida

Para no romper compatibilidad con lo ya documentado, el plan propone:

- mantener `build/<ejemplo>/` como salida tecnica del proceso;
- introducir `dist/<ejemplo>/` como salida formal de publicacion;
- documentar que `dist/<ejemplo>/` es el bundle final recomendado para produccion.

### 5. Adaptar el servidor de desarrollo al nuevo modelo

#### Archivos afectados

- `tools/dev-server/src/server.mjs`
  - Que: incluir observacion de `.scss`, `.css` y assets.
  - Por que: los estilos deben recompilar y reflejarse en desarrollo.
  - Como: ampliar el watcher y el flujo de build para cambios de `styles/` y `assets/`.

- `tools/dev-server/src/client.mjs`
  - Que: revisar el overlay para mantener coherencia visual minima con la nueva base.
  - Por que: hoy contiene estilos inline; no es bloqueante, pero debe entrar al plan de legibilidad.
  - Como: evaluar si queda con inline styles de bajo impacto o si pasa a un snippet mas mantenible; si no se toca en esta iteracion, documentarlo como deuda secundaria.

### 6. Actualizar la documentacion y la guia de nuevos proyectos

#### Archivos afectados

- `README.md`
  - Que: reescribir la seccion de estructura base y creacion de proyectos.
  - Por que: la arquitectura canonica cambia y debe quedar como contrato del repo.
  - Como: documentar:
    - estructura base obligatoria;
    - rol de `index.html`, `main.c`, `bootstrap.c`;
    - capa compartida + local de estilos;
    - flujo de build y dist;
    - desarrollo y produccion con Nginx;
    - como crear un proyecto nuevo a partir de `minimal`.

- `CHANGELOG.md`
  - Que: agregar entrada describiendo la nueva arquitectura base y pipeline Sass/dist.
  - Por que: la refactorizacion afecta la experiencia de uso del proyecto.

### 7. Verificacion y criterios de aceptacion

#### Verificacion tecnica

- `make test`
  - Debe seguir pasando sin regresiones del runtime base.

- `make test-browser`
  - Debe seguir validando el dev server.

- `make -n counter DEV=1`
- `make -n minimal DEV=1`
  - Deben mostrar los nuevos pasos de build incluyendo CSS/assets.

- nuevos checks sugeridos:
  - `make dist-counter`
  - `make dist-minimal`
  - validacion de que `dist/<ejemplo>/app.css` existe;
  - validacion de que `index.html` referencia correctamente `app.css`.

#### Verificacion funcional

- `minimal` debe arrancar con `main.c + bootstrap.c + minimal.c`.
- `counter` debe arrancar con `main.c + bootstrap.c + counter.c`.
- ambos ejemplos deben renderizar sin estilos inline en el feature principal.
- cambios en archivos `.scss` deben reflejarse en desarrollo.
- `dist/<ejemplo>/` debe poder servirse como sitio estatico sin pasos extra.

#### Verificacion de produccion

- probar `dist/counter/` y `dist/minimal/` con servidor estatico local;
- validar carga de `index.html`, `.js`, `.wasm` y `app.css`;
- confirmar que el bundle puede publicarse detras de Nginx con `application/wasm`.

## Supuestos y decisiones

- no se incorporara Bootstrap CSS como framework externo; “bootstrap” se interpreta como capa de arranque del proyecto.
- el repositorio adoptara una arquitectura canonica de ejemplo base centrada en `main.c`, `bootstrap.c` y un modulo de feature.
- se usara Sass real, presumiblemente mediante una dependencia Node en el tooling existente.
- la base compartida de estilos vivira bajo `examples/shared/styles/` para servir como plantilla transversal.
- el ejemplo `minimal` pasara a ser la plantilla mas cercana a “crear app nueva”.
- `counter` servira como ejemplo de feature con estado y eventos dentro de la misma arquitectura.
- el runtime C principal no requiere cambios conceptuales profundos para esta tarea; el foco esta en estructura de proyecto, estilos, build y legibilidad.

## Riesgos y mitigaciones

- Riesgo: introducir demasiada complejidad en el ejemplo minimo.
  - Mitigacion: mantener `main.c` y `bootstrap.c` ligeros, y mover la complejidad a modulos y estilos compartidos.

- Riesgo: acoplar demasiado el build a herramientas Node.
  - Mitigacion: limitar Node al pipeline de estilos/assets y conservar la compilacion C en `Makefile`.

- Riesgo: duplicar CSS entre capa compartida y local.
  - Mitigacion: definir con claridad tokens compartidos vs overrides por proyecto.

- Riesgo: romper el dev server actual.
  - Mitigacion: adaptar watchers y smoke tests despues del refactor.

## Secuencia de implementacion recomendada

1. Reestructurar `examples/minimal/` para convertirlo en la plantilla base con `main.c`, `bootstrap.c` y modulo de feature.
2. Reestructurar `examples/counter/` con el mismo patron.
3. Introducir la capa de estilos compartida + local y eliminar estilos inline de ambos ejemplos.
4. Implementar compilacion Sass y copia de assets en el tooling Node y en `Makefile`.
5. Introducir salida `dist/<ejemplo>/` y mantener `build/<ejemplo>/` como salida intermedia o tecnica.
6. Adaptar el dev server para observar `.scss` y assets.
7. Actualizar README, changelog y guias de despliegue.
8. Ejecutar validaciones nativas, browser y de bundle final.

## Archivos previstos por area

### Ejemplos

- `examples/minimal/index.html`
- `examples/minimal/main.c`
- `examples/minimal/bootstrap.c`
- `examples/minimal/bootstrap.h`
- `examples/minimal/minimal.c`
- `examples/minimal/minimal.h`
- `examples/minimal/styles/app.scss`
- `examples/minimal/styles/_tokens.scss`
- `examples/counter/index.html`
- `examples/counter/main.c`
- `examples/counter/bootstrap.c`
- `examples/counter/bootstrap.h`
- `examples/counter/counter.c`
- `examples/counter/counter.h`
- `examples/counter/styles/app.scss`
- `examples/counter/styles/_tokens.scss`

### Estilos compartidos

- `examples/shared/styles/xanda.scss`
- `examples/shared/styles/_variables.scss`
- `examples/shared/styles/_reset.scss`
- `examples/shared/styles/_base.scss`
- `examples/shared/styles/_utilities.scss`
- `examples/shared/styles/_components.scss`

### Build y tooling

- `Makefile`
- `tools/dev-server/package.json`
- `tools/dev-server/src/server.mjs`
- posible script nuevo bajo `tools/dev-server/src/` o `tools/build/`

### Documentacion

- `README.md`
- `CHANGELOG.md`

## Pasos de verificacion

1. Ejecutar `make test`.
2. Ejecutar `make test-browser`.
3. Ejecutar `make -n counter DEV=1`.
4. Ejecutar `make -n minimal DEV=1`.
5. Ejecutar el nuevo flujo de distribucion para `counter`.
6. Ejecutar el nuevo flujo de distribucion para `minimal`.
7. Servir `dist/counter/` y `dist/minimal/` con un servidor estatico local.
8. Verificar carga correcta bajo un escenario equivalente a Nginx.
