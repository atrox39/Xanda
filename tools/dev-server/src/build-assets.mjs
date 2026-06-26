import fsp from "node:fs/promises";
import path from "node:path";
import process from "node:process";
import { fileURLToPath } from "node:url";

const currentFile = fileURLToPath(import.meta.url);
const currentDir = path.dirname(currentFile);
const devServerRoot = path.resolve(currentDir, "..");
const projectRoot = path.resolve(devServerRoot, "..", "..");

async function pathExists(targetPath) {
  try {
    await fsp.access(targetPath);
    return true;
  } catch {
    return false;
  }
}

async function compileStyles(exampleName, outputDir) {
  const sassEntry = path.join(projectRoot, "examples", exampleName, "styles", "app.scss");

  let sassModule;
  try {
    sassModule = await import("sass");
  } catch (error) {
    throw new Error(`No se encontro la dependencia 'sass'. Ejecuta 'make dev-setup'.\n${error.message}`);
  }

  if (!(await pathExists(sassEntry))) {
    throw new Error(`No se encontro el entrypoint Sass para '${exampleName}' en ${sassEntry}.`);
  }

  const compiled = await sassModule.compileAsync(sassEntry, {
    style: "expanded"
  });

  await fsp.writeFile(path.join(outputDir, "app.css"), compiled.css, "utf8");
}

async function copyExampleShell(exampleName, outputDir) {
  const exampleRoot = path.join(projectRoot, "examples", exampleName);
  const sourceHtml = path.join(exampleRoot, "index.html");
  const sourceAssets = path.join(exampleRoot, "assets");

  await fsp.mkdir(outputDir, { recursive: true });
  await fsp.copyFile(sourceHtml, path.join(outputDir, "index.html"));

  if (await pathExists(sourceAssets)) {
    await fsp.cp(sourceAssets, path.join(outputDir, "assets"), {
      recursive: true,
      force: true
    });
  }
}

async function buildExampleAssets(exampleName) {
  const outputDir = path.join(projectRoot, "build", exampleName);
  await copyExampleShell(exampleName, outputDir);
  await compileStyles(exampleName, outputDir);
}

async function buildExampleDist(exampleName) {
  const sourceDir = path.join(projectRoot, "build", exampleName);
  const outputDir = path.join(projectRoot, "dist", exampleName);

  if (!(await pathExists(sourceDir))) {
    throw new Error(`No existe el build de '${exampleName}'. Ejecuta primero la compilacion del ejemplo.`);
  }

  await fsp.rm(outputDir, { recursive: true, force: true });
  await fsp.mkdir(path.dirname(outputDir), { recursive: true });
  await fsp.cp(sourceDir, outputDir, {
    recursive: true,
    force: true
  });
}

async function main() {
  const mode = process.argv[2];
  const exampleName = process.argv[3];

  if (!mode || !exampleName) {
    throw new Error("Uso: node build-assets.mjs <build|dist> <example>.");
  }

  if (mode === "build") {
    await buildExampleAssets(exampleName);
    return;
  }

  if (mode === "dist") {
    await buildExampleDist(exampleName);
    return;
  }

  throw new Error(`Modo no soportado: ${mode}.`);
}

main().catch((error) => {
  console.error(error.message);
  process.exit(1);
});
