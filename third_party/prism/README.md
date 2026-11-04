# PrismJS bundle

`prism.bundle.min.js` is a pre-built PrismJS 1.29.0 bundle used by the preview.
It is embedded into `ActiveXWebMark.exe` as an `RCDATA` resource (see
`src/app/app.rc.in`) and injected into the generated preview document.

The application must keep working in IE11 document mode, so Prism is loaded with
`Prism.manual = true` and two small ES5 shims run before highlighting:

- `Prism.util.setLanguage` is replaced because IE11 throws a "regex syntax
  error" when a `RegExp` object is passed to the `RegExp(regex, flags)`
  constructor (Prism 1.29 does this internally).
- Language names are normalised before highlighting so fences such as `c++`,
  `c#`, `js`, `html`, `sh`, `py` and `yml` resolve to Prism grammars.

The shims live in `src/core/markdown/previewhtml.cpp`, not in this bundle.

## Components (concatenation order)

```
core clike markup css javascript typescript jsx tsx c cpp csharp java go
python json yaml ini bash powershell sql rust markdown diff docker
```

Ordering constraints from Prism's `components.json`:

- `c` requires `clike`; `cpp` requires `c`.
- `css` and `javascript` modify `markup`.
- `typescript` requires `javascript`; `jsx` requires `markup` + `javascript`;
  `tsx` requires `jsx` + `typescript`.
- `markdown` requires `markup`.

## Rebuilding

Download the matching `prism-*.min.js` files from
`https://cdnjs.cloudflare.com/ajax/libs/prism/1.29.0/components/` and concatenate
them in the order above, separating each file with a newline and `;`. Keep the
version and component list in the header comment up to date, then re-run the
normal build (the `.rc` resource is recompiled through `OBJECT_DEPENDS`).

PrismJS is MIT licensed; see `LICENSE` in this directory.
