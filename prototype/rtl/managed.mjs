import { join } from "node:path";
import { patchApp, restoreApp } from "./bin/patch-app.js";
import { patchIde, restoreIde } from "./bin/patch-ide.js";

const [product, action, staging] = process.argv.slice(2);
if (!["app", "ide"].includes(product) || !["apply", "restore"].includes(action) || !staging) {
    process.exit(2);
}

const ide = product === "ide";
const target = join(staging, ide ? "resources/app" : "resources/app.asar");
const operation = ide
    ? (action === "apply" ? patchIde : restoreIde)
    : (action === "apply" ? patchApp : restoreApp);
process.exitCode = await operation(target, { exitOnError: false }) ? 0 : 1;
