import http from "node:http";
import { createReadStream, existsSync, statSync } from "node:fs";
import { extname, join, normalize } from "node:path";
import { fileURLToPath } from "node:url";

const root = normalize(join(fileURLToPath(import.meta.url), "..", "..", "web"));
const port = Number(process.env.WEB_PORT || 8081);

const types = {
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".png": "image/png",
  ".jpg": "image/jpeg",
  ".jpeg": "image/jpeg",
  ".svg": "image/svg+xml",
};

function send404(response) {
  response.writeHead(404, { "content-type": "text/plain; charset=utf-8" });
  response.end("Not found");
}

const server = http.createServer((request, response) => {
  const url = new URL(request.url || "/", "http://localhost");
  const pathname = decodeURIComponent(url.pathname === "/" ? "/led-control.html" : url.pathname);
  const fullPath = normalize(join(root, pathname));

  if (!fullPath.startsWith(root) || !existsSync(fullPath) || !statSync(fullPath).isFile()) {
    send404(response);
    return;
  }

  response.writeHead(200, {
    "content-type": types[extname(fullPath).toLowerCase()] || "application/octet-stream",
    "cache-control": "no-store",
  });
  createReadStream(fullPath).pipe(response);
});

server.listen(port, "0.0.0.0", () => {
  console.log(`Web server running: http://0.0.0.0:${port}/led-control.html`);
});
