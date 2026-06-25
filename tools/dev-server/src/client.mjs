(() => {
  const devState = window.__XANDA_DEV__ || { example: "counter" };
  const overlayId = "__xanda_dev_overlay";
  const statusId = "__xanda_dev_status";
  let socket = null;
  let reconnectTimer = null;
  let sawSuccessfulBuild = false;

  function ensureOverlay() {
    let overlay = document.getElementById(overlayId);
    if (overlay) {
      return overlay;
    }

    overlay = document.createElement("div");
    overlay.id = overlayId;
    overlay.style.position = "fixed";
    overlay.style.inset = "0";
    overlay.style.zIndex = "2147483647";
    overlay.style.display = "none";
    overlay.style.background = "rgba(17, 24, 39, 0.94)";
    overlay.style.color = "#f9fafb";
    overlay.style.fontFamily = "ui-monospace, SFMono-Regular, Menlo, Consolas, monospace";
    overlay.style.padding = "24px";
    overlay.style.overflow = "auto";
    document.body.appendChild(overlay);
    return overlay;
  }

  function ensureStatus() {
    let status = document.getElementById(statusId);
    if (status) {
      return status;
    }

    status = document.createElement("div");
    status.id = statusId;
    status.style.position = "fixed";
    status.style.right = "12px";
    status.style.bottom = "12px";
    status.style.zIndex = "2147483646";
    status.style.background = "rgba(17, 24, 39, 0.85)";
    status.style.color = "#e5e7eb";
    status.style.padding = "8px 12px";
    status.style.borderRadius = "10px";
    status.style.fontFamily = "system-ui, sans-serif";
    status.style.fontSize = "12px";
    status.style.boxShadow = "0 8px 30px rgba(0, 0, 0, 0.25)";
    status.style.display = "none";
    document.body.appendChild(status);
    return status;
  }

  function showStatus(message) {
    const status = ensureStatus();
    status.textContent = message;
    status.style.display = "block";
  }

  function hideStatus() {
    ensureStatus().style.display = "none";
  }

  function hideOverlay() {
    const overlay = ensureOverlay();
    overlay.style.display = "none";
    overlay.innerHTML = "";
  }

  function showErrorOverlay(message) {
    const overlay = ensureOverlay();
    const safeMessage = typeof message === "string" ? message : "Error desconocido del servidor de desarrollo.";
    overlay.innerHTML = `
      <div style="max-width: 1100px; margin: 0 auto;">
        <h1 style="font-size: 20px; margin: 0 0 16px;">Xanda Dev Server</h1>
        <p style="margin: 0 0 16px; color: #fca5a5;">La ultima compilacion fallo. Corrige el error y el navegador se recargara automaticamente.</p>
        <pre style="white-space: pre-wrap; line-height: 1.5; background: rgba(0, 0, 0, 0.35); padding: 16px; border-radius: 12px;">${safeMessage.replace(/</g, "&lt;").replace(/>/g, "&gt;")}</pre>
      </div>
    `;
    overlay.style.display = "block";
  }

  function handlePayload(payload) {
    switch (payload.type) {
      case "connected":
        if (payload.status === "success") {
          sawSuccessfulBuild = true;
          hideOverlay();
          hideStatus();
        } else if (payload.status === "error") {
          showErrorOverlay(payload.error || payload.message);
        } else if (payload.status === "building") {
          showStatus("Compilando...");
        }
        break;
      case "build-start":
        showStatus(`Compilando ${payload.example || devState.example}...`);
        break;
      case "build-error":
        showErrorOverlay(payload.error || payload.message);
        break;
      case "build-ok":
        hideOverlay();
        showStatus("Compilacion lista. Reemplazando el modulo...");
        if (sawSuccessfulBuild) {
          void reloadModuleWithoutPageRefresh();
          return;
        }
        sawSuccessfulBuild = true;
        window.setTimeout(hideStatus, 800);
        break;
      default:
        break;
    }
  }

  function frame(message) {
    return typeof message === "string" ? message : JSON.stringify(message);
  }

  async function reloadModuleWithoutPageRefresh() {
    try {
      if (window.__XANDA_HMR_HOST__ && typeof window.__XANDA_HMR_HOST__.swapModule === "function") {
        await window.__XANDA_HMR_HOST__.swapModule();
        showStatus("Modulo actualizado sin recargar la pagina.");
        window.setTimeout(hideStatus, 900);
        return;
      }
    } catch (error) {
      console.warn("Xanda dev client: el swap del modulo fallo; se usara recarga completa", error);
    }

    window.location.reload();
  }

  function connect() {
    const protocol = window.location.protocol === "https:" ? "wss" : "ws";
    socket = new WebSocket(`${protocol}://${window.location.host}/__xanda_ws?example=${encodeURIComponent(devState.example)}`);
    socket.addEventListener("message", (event) => {
      try {
        handlePayload(JSON.parse(frame(event.data)));
      } catch (error) {
        console.error("Xanda dev client: mensaje invalido", error);
      }
    });
    socket.addEventListener("close", () => {
      showStatus("Reconectando al servidor de desarrollo...");
      if (!reconnectTimer) {
        reconnectTimer = window.setTimeout(() => {
          reconnectTimer = null;
          connect();
        }, 1000);
      }
    });
    socket.addEventListener("error", () => {
      socket.close();
    });
  }

  connect();
})();
