# Changelog

## 1.0.0 - 2026-06-25

- Declara estable la API centrada en `XandaApp`, `XandaComponent` y `XandaStateSnapshot`.
- Consolida el runtime de desarrollo con snapshot textual, boundaries formales y validacion del boundary raiz.
- Fija el contrato publico de version mediante `XANDA_VERSION_STRING` y `xanda_version_string()`.
- Fija la version del protocolo dev mediante `XANDA_DEV_PROTOCOL_VERSION` y `xanda_dev_protocol_version()`.
- Agrega el target `make release-check` para validar la release localmente.
- Endurece el `Makefile` para funcionar mejor en Windows y Unix.
