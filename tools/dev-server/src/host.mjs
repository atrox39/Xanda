(() => {
  const devState = window.__XANDA_DEV__ || { example: "counter" };
  const hostState = {
    example: devState.example || "counter",
    factoryName: devState.factoryName || "createXandaModule",
    expectedProtocolVersion: Number(devState.protocolVersion || 1),
    scriptElement: null,
    module: null,
    bootPromise: null,
    version: Date.now(),
    isSwapping: false
  };

  function scope() {
    return typeof globalThis !== "undefined" ? globalThis : window;
  }

  function entryScriptName() {
    return `${hostState.example}.js`;
  }

  function entryScriptUrl(version) {
    return `/${hostState.example}/${entryScriptName()}?v=${encodeURIComponent(String(version))}`;
  }

  function locateFile(path) {
    return `/${hostState.example}/${path}?v=${encodeURIComponent(String(hostState.version))}`;
  }

  function resetBridgeScope() {
    const globalScope = scope();
    globalScope.__xanda = { next_id: 0, nodes: {} };
  }

  function removeCurrentScript() {
    if (hostState.scriptElement && hostState.scriptElement.parentNode) {
      hostState.scriptElement.parentNode.removeChild(hostState.scriptElement);
    }
    hostState.scriptElement = null;
  }

  function unloadCurrentModule() {
    if (hostState.module && typeof hostState.module.destroy === "function") {
      try {
        hostState.module.destroy();
      } catch (_error) {
        // El runtime de desarrollo puede continuar aunque la instancia no exponga destroy.
      }
    }

    hostState.module = null;
    scope().Module = null;
    delete scope()[hostState.factoryName];
    removeCurrentScript();
    resetBridgeScope();
  }

  function loadFactory(version) {
    return new Promise((resolve, reject) => {
      const script = document.createElement("script");
      script.src = entryScriptUrl(version);
      script.async = true;
      script.dataset.xandaEntry = hostState.example;
      script.onload = () => {
        hostState.scriptElement = script;
        const factory = scope()[hostState.factoryName];
        if (typeof factory !== "function") {
          reject(new Error(`Xanda dev host: no se encontro ${hostState.factoryName} tras cargar ${script.src}`));
          return;
        }
        resolve(factory);
      };
      script.onerror = () => reject(new Error(`Xanda dev host: no se pudo cargar ${script.src}`));
      document.body.appendChild(script);
    });
  }

  function readProtocolVersion(moduleInstance) {
    if (!moduleInstance || typeof moduleInstance.ccall !== "function") {
      return null;
    }

    return Number(moduleInstance.ccall("xanda_dev_protocol_version", "number", [], []));
  }

  async function instantiate(version) {
    const factory = await loadFactory(version);
    hostState.version = version;
    const moduleInstance = await factory({
      locateFile,
      noInitialRun: false
    });
    const protocolVersion = readProtocolVersion(moduleInstance);
    if (protocolVersion !== hostState.expectedProtocolVersion) {
      if (typeof moduleInstance.destroy === "function") {
        moduleInstance.destroy();
      }
      throw new Error(
        `Xanda dev host: protocolo incompatible (host=${hostState.expectedProtocolVersion}, modulo=${protocolVersion})`
      );
    }
    hostState.module = moduleInstance;
    scope().Module = moduleInstance;
    return moduleInstance;
  }

  async function boot() {
    if (hostState.bootPromise) {
      return hostState.bootPromise;
    }

    hostState.bootPromise = (async () => {
      unloadCurrentModule();
      return instantiate(Date.now());
    })();

    try {
      return await hostState.bootPromise;
    } finally {
      hostState.bootPromise = null;
    }
  }

  function captureSnapshot() {
    if (!hostState.module || typeof hostState.module.ccall !== "function" || !window.sessionStorage) {
      return null;
    }

    const encoded = hostState.module.ccall("xanda_dev_snapshot_root_component", "string", [], []);
    if (encoded) {
      window.sessionStorage.setItem("__xanda_dev_snapshot__", encoded);
      return encoded;
    }

    window.sessionStorage.removeItem("__xanda_dev_snapshot__");
    return null;
  }

  function describeRootBoundary(moduleInstance) {
    if (!moduleInstance || typeof moduleInstance.ccall !== "function") {
      return null;
    }

    return moduleInstance.ccall("xanda_dev_describe_root_boundary", "string", [], []);
  }

  function parseBoundaryDescriptor(encoded) {
    if (!encoded || typeof encoded !== "string") {
      return null;
    }

    const parts = encoded.split("|");
    if (parts.length !== 6) {
      return null;
    }

    return {
      name: parts[0] || "",
      key: parts[1] || "",
      instanceId: Number(parts[2] || "0"),
      renderCount: Number(parts[3] || "0"),
      stateVersion: Number(parts[4] || "0"),
      stateSize: Number(parts[5] || "0")
    };
  }

  function boundariesAreCompatible(previousBoundary, nextBoundary) {
    if (!previousBoundary || !nextBoundary) {
      return true;
    }

    return (
      previousBoundary.name === nextBoundary.name &&
      previousBoundary.key === nextBoundary.key &&
      previousBoundary.stateVersion === nextBoundary.stateVersion &&
      previousBoundary.stateSize === nextBoundary.stateSize
    );
  }

  async function swapModule() {
    if (hostState.isSwapping) {
      return hostState.bootPromise || Promise.resolve(hostState.module);
    }

    hostState.isSwapping = true;
    try {
      const previousBoundary = parseBoundaryDescriptor(describeRootBoundary(hostState.module));
      captureSnapshot();
      unloadCurrentModule();
      const nextModule = await instantiate(Date.now());
      const nextBoundary = parseBoundaryDescriptor(describeRootBoundary(nextModule));

      if (!boundariesAreCompatible(previousBoundary, nextBoundary)) {
        throw new Error(
          `Xanda dev host: el boundary raiz cambio de forma incompatible (${JSON.stringify(previousBoundary)} -> ${JSON.stringify(nextBoundary)})`
        );
      }

      return nextModule;
    } finally {
      hostState.isSwapping = false;
    }
  }

  scope().__XANDA_HMR_HOST__ = {
    boot,
    swapModule,
    captureSnapshot,
    getModule() {
      return hostState.module;
    }
  };

  void boot().catch((error) => {
    console.error("Xanda dev host: fallo el arranque inicial", error);
  });
})();
