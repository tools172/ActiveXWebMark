# Diagram renderer

Vendored runtime files for Mermaid and PlantUML rendering.

- `sidecar.cjs` — Node script bundled with puppeteer-core. Mermaid runs in
  headless Edge/Chrome; PlantUML runs locally through the official headless
  native executable, or is POSTed to a Kroki-compatible server.
- `mermaid.min.js` — official mermaid 11.12.1 browser bundle.

The application copies this directory to `<exe>\mermaid\` and launches it with
the user's Node.js (18+). No `npm install` is required on user machines.
The PlantUML native runtime is downloaded and staged separately by
`tools/plantuml-native/fetch.ps1`; in remote mode PlantUML source text is sent
to the configured server.

Regenerate from the source package:

```powershell
cd tools/mermaid-sidecar
npm install
npm run vendor
```

Licenses: `LICENSE.mermaid` (MIT), `LICENSE.puppeteer-core` (Apache-2.0).
The bundled JavaScript also contains transitive dependencies under their
respective licenses.
