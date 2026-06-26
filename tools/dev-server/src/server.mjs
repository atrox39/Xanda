import crypto from "node:crypto";
import fs from "node:fs";
import fsp from "node:fs/promises";
import http from "node:http";
import path from "node:path";
import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";

const currentFile = fileURLToPath(import.meta.url);
const currentDir = path.dirname(currentFile);
const devServerRoot = path.resolve(currentDir, "..");
const projectRoot = path.resolve(devServerRoot, "..", "..");
const buildRoot = path.join(projectRoot, "build");
const example = process.argv[2] || process.env.XANDA_EXAMPLE || "counter";
const port = Number(process.env.PORT || process.env.XANDA_DEV_PORT || "5173");
const host = process.env.HOST || "127.0.0.1";
const makeCommand = process.env.MAKE || "make";
const clientScriptPath = path.join(currentDir, "client.mjs");
const hostScriptPath = path.join(currentDir, "host.mjs");
const wsClients = new Set();

let buildStatus = "idle";
let lastBuildOutput = "";
let buildProcess = null;
let pendingBuild = false;
let debounceTimer = null;

function contentTypeFor(filePath) {
  if (filePath.endsWith(".html")) return "text/html; charset=utf-8";
  if (filePath.endsWith(".js")) return "application/javascript; charset=utf-8";
  if (filePath.endsWith(".mjs")) return "application/javascript; charset=utf-8";
  if (filePath.endsWith(".json")) return "application/json; charset=utf-8";
  if (filePath.endsWith(".wasm")) return "application/wasm";
  if (filePath.endsWith(".map")) return "application/json; charset=utf-8";
  if (filePath.endsWith(".css")) return "text/css; charset=utf-8";
  return "application/octet-stream";
}

function clientSnippet() {
  return [
    `<script>window.__XANDA_DEV__=${JSON.stringify({ example, factoryName: "createXandaModule", version: "1.0.0", protocolVersion: 1 })};</script>`,
    `<script src="/__xanda_dev_host.js"></script>`,
    `<script src="/__xanda_dev_client.js"></script>`
  ].join("");
}

function injectClient(html) {
  const entryPattern = new RegExp(`<script\\s+src=["']${example}\\.js["']\\s*><\\/script>`, "i");
  const sanitizedHtml = html.replace(entryPattern, "");

  if (html.includes("__xanda_dev_client.js")) {
    return sanitizedHtml;
  }

  if (sanitizedHtml.includes("</body>")) {
    return sanitizedHtml.replace("</body>", `${clientSnippet()}</body>`);
  }

  return `${sanitizedHtml}${clientSnippet()}`;
}

function encodeFrame(payload) {
  const data = Buffer.from(JSON.stringify(payload));
  if (data.length < 126) {
    return Buffer.concat([Buffer.from([0x81, data.length]), data]);
  }

  if (data.length < 65536) {
    const header = Buffer.from([0x81, 126, (data.length >> 8) & 0xff, data.length & 0xff]);
    return Buffer.concat([header, data]);
  }

  const header = Buffer.alloc(10);
  header[0] = 0x81;
  header[1] = 127;
  header.writeBigUInt64BE(BigInt(data.length), 2);
  return Buffer.concat([header, data]);
}

function broadcast(payload) {
  const frame = encodeFrame(payload);
  for (const socket of wsClients) {
    socket.write(frame);
  }
}

function scheduleBuild(reason) {
  if (debounceTimer) {
    clearTimeout(debounceTimer);
  }

  debounceTimer = setTimeout(() => {
    debounceTimer = null;
    void runBuild(reason);
  }, 180);
}

async function runBuild(reason) {
  if (buildProcess) {
    pendingBuild = true;
    return;
  }

  buildStatus = "building";
  lastBuildOutput = `Compilando ${example}...\nMotivo: ${reason}`;
  broadcast({
    type: "build-start",
    example,
    status: buildStatus,
    message: lastBuildOutput
  });

  buildProcess = spawn(makeCommand, [example, "DEV=1"], {
    cwd: projectRoot,
    env: {
      ...process.env,
      DEV: "1",
      XANDA_DEV: "1",
      XANDA_DEV_PORT: String(port)
    },
    shell: true
  });

  const outputChunks = [];
  buildProcess.stdout.on("data", (chunk) => outputChunks.push(String(chunk)));
  buildProcess.stderr.on("data", (chunk) => outputChunks.push(String(chunk)));

  const exitCode = await new Promise((resolve) => {
    buildProcess.on("close", (code) => resolve(code ?? 0));
  });

  buildProcess = null;
  lastBuildOutput = outputChunks.join("").trim();

  if (exitCode === 0) {
    buildStatus = "success";
    broadcast({
      type: "build-ok",
      example,
      status: buildStatus,
      message: lastBuildOutput || `Compilacion de ${example} completada correctamente.`
    });
  } else {
    buildStatus = "error";
    broadcast({
      type: "build-error",
      example,
      status: buildStatus,
      error: lastBuildOutput || `La compilacion de ${example} fallo con codigo ${exitCode}.`
    });
  }

  if (pendingBuild) {
    pendingBuild = false;
    await runBuild("Cambios acumulados mientras se compilaba");
  }
}

function watchPath(targetPath, recursive) {
  fs.watch(targetPath, { recursive }, (_event, fileName) => {
    const changed = fileName ? String(fileName) : targetPath;
    if (
      changed.endsWith(".c") ||
      changed.endsWith(".h") ||
      changed.endsWith(".html") ||
      changed.endsWith(".scss") ||
      changed.endsWith(".css") ||
      changed.endsWith(".png") ||
      changed.endsWith(".jpg") ||
      changed.endsWith(".jpeg") ||
      changed.endsWith(".svg") ||
      changed.endsWith(".webp") ||
      changed.endsWith(".ico") ||
      changed.endsWith(".md") ||
      changed.endsWith("Makefile") ||
      targetPath.endsWith("Makefile")
    ) {
      scheduleBuild(`Cambio detectado en ${changed}`);
    }
  });
}

async function resolveRequestPath(requestPath) {
  let normalized = decodeURIComponent(requestPath.split("?")[0]);

  if (normalized === "/") {
    normalized = `/${example}/`;
  }

  if (!normalized.startsWith("/")) {
    normalized = `/${normalized}`;
  }

  let candidate = path.join(buildRoot, normalized);
  if (!candidate.startsWith(buildRoot)) {
    return null;
  }

  try {
    const stats = await fsp.stat(candidate);
    if (stats.isDirectory()) {
      candidate = path.join(candidate, "index.html");
    }
  } catch {
    if (normalized.endsWith("/")) {
      candidate = path.join(candidate, "index.html");
    }
  }

  if (!candidate.startsWith(buildRoot)) {
    return null;
  }

  return candidate;
}

const server = http.createServer(async (request, response) => {
  const urlPath = request.url || "/";

  if (urlPath === "/__xanda_dev_client.js") {
    const clientSource = await fsp.readFile(clientScriptPath, "utf8");
    response.writeHead(200, { "Content-Type": "application/javascript; charset=utf-8" });
    response.end(clientSource);
    return;
  }

  if (urlPath === "/__xanda_dev_host.js") {
    const hostSource = await fsp.readFile(hostScriptPath, "utf8");
    response.writeHead(200, { "Content-Type": "application/javascript; charset=utf-8" });
    response.end(hostSource);
    return;
  }

  if (urlPath === "/__xanda_dev_status") {
    response.writeHead(200, { "Content-Type": "application/json; charset=utf-8" });
    response.end(JSON.stringify({ example, status: buildStatus, output: lastBuildOutput }));
    return;
  }

  const targetPath = await resolveRequestPath(urlPath);
  if (!targetPath) {
    response.writeHead(403, { "Content-Type": "text/plain; charset=utf-8" });
    response.end("Ruta no permitida.");
    return;
  }

  try {
    const fileBuffer = await fsp.readFile(targetPath);
    const fileType = contentTypeFor(targetPath);

    if (targetPath.endsWith(".html")) {
      response.writeHead(200, { "Content-Type": fileType });
      response.end(injectClient(fileBuffer.toString("utf8")));
      return;
    }

    response.writeHead(200, { "Content-Type": fileType });
    response.end(fileBuffer);
  } catch {
    response.writeHead(404, { "Content-Type": "text/plain; charset=utf-8" });
    response.end("Archivo no encontrado.");
  }
});

server.on("upgrade", (request, socket) => {
  if (!request.url || !request.url.startsWith("/__xanda_ws")) {
    socket.destroy();
    return;
  }

  const key = request.headers["sec-websocket-key"];
  if (!key) {
    socket.destroy();
    return;
  }

  const accept = crypto
    .createHash("sha1")
    .update(`${key}258EAFA5-E914-47DA-95CA-C5AB0DC85B11`)
    .digest("base64");

  socket.write(
    [
      "HTTP/1.1 101 Switching Protocols",
      "Upgrade: websocket",
      "Connection: Upgrade",
      `Sec-WebSocket-Accept: ${accept}`,
      "",
      ""
    ].join("\r\n")
  );

  wsClients.add(socket);
  socket.on("close", () => wsClients.delete(socket));
  socket.on("end", () => wsClients.delete(socket));
  socket.on("error", () => wsClients.delete(socket));
  socket.on("data", () => {
    // El cliente no necesita enviar mensajes por ahora.
  });

  socket.write(
    encodeFrame({
      type: "connected",
      example,
      status: buildStatus,
      message: lastBuildOutput,
      error: buildStatus === "error" ? lastBuildOutput : undefined
    })
  );
});

server.listen(port, host, async () => {
  console.log(`Xanda dev server listo en http://${host}:${port}/${example}/`);
  watchPath(path.join(projectRoot, "src"), true);
  watchPath(path.join(projectRoot, "include"), true);
  watchPath(path.join(projectRoot, "examples", example), true);
  watchPath(path.join(projectRoot, "Makefile"), false);
  await runBuild("Arranque inicial del servidor de desarrollo");
});
