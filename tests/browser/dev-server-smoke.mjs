import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import path from "node:path";
import process from "node:process";

const port = 5197;
const child = spawn(process.execPath, [path.join("tools", "dev-server", "src", "server.mjs"), "minimal"], {
  cwd: process.cwd(),
  env: {
    ...process.env,
    PORT: String(port)
  },
  stdio: ["ignore", "pipe", "pipe"]
});

const output = [];

function track(chunk) {
  output.push(String(chunk));
}

child.stdout.on("data", track);
child.stderr.on("data", track);

async function waitForServer() {
  const deadline = Date.now() + 60000;

  while (Date.now() < deadline) {
    try {
      const response = await fetch(`http://127.0.0.1:${port}/minimal/`);
      if (response.ok) {
        return response.text();
      }
    } catch {
      // espera hasta que el servidor este listo
    }

    await new Promise((resolve) => setTimeout(resolve, 500));
  }

  throw new Error(`El servidor de desarrollo no estuvo listo a tiempo.\n${output.join("")}`);
}

try {
  const html = await waitForServer();
  assert.ok(html.includes("__xanda_dev_client.js"), "El HTML servido debe inyectar el cliente de desarrollo.");
  assert.ok(html.includes("__xanda_dev_host.js"), "El HTML servido debe inyectar el host de desarrollo.");
  assert.ok(html.includes("__XANDA_DEV__"), "El HTML servido debe exponer el estado del cliente de desarrollo.");
  assert.ok(html.includes('"version":"1.0.0"'), "El HTML servido debe declarar la version estable del runtime dev.");
  assert.ok(html.includes('"protocolVersion":1'), "El HTML servido debe declarar la version del protocolo dev.");
  assert.ok(html.includes('<link rel="stylesheet" href="./app.css">'), "El HTML servido debe enlazar el CSS compilado del ejemplo.");
  assert.ok(!html.includes('<script src="minimal.js"></script>'), "El host debe reemplazar la carga directa del bundle en modo desarrollo.");
  console.log("OK");
} finally {
  child.kill();
}
