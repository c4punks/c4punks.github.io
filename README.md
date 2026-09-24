# c4punks.github.io

The organisation site for **C 4 Punk Developers**: who we are, what we
maintain, and how to work with us.

Live at <https://c4punks.github.io>.

## How the site is built

The site is a [CWIST](https://github.com/c4punks/CWIST) application compiled to
`wasm32-wasip2`. The same binary does two jobs:

| Command | What it does |
| --- | --- |
| `make run` | Serves the site over `wasi:sockets` under wasmtime. |
| `make export` | Renders every route to static HTML in the repository root, which is what GitHub Pages publishes. |

There is no JavaScript on any page. Markup is written as templates and rendered
by CWIST's template engine. The stylesheet and the three self-hosted woff2 web
fonts are minified, content-hashed and served from CWIST's in-memory asset
registry. Syntax highlighting is done in C at render time by
`site/highlight.h`, so code samples arrive already coloured.

## Layout

```
site/
  main.c          routing, rendering and the static exporter
  content.h       organisation copy: principles, about, contributing
  projects.h      project entries (CWIST, libttak)
  guides.h        long-form guides
  highlight.h     the C and shell syntax highlighter
  templates/      HTML templates and the stylesheet
  fonts/          self-hosted woff2 web fonts (Inter, Space Grotesk, JetBrains Mono)
  tools/          embeds templates and fonts into the binary at build time
```

`site/templates.h`, `site/fonts.h` and `site/site.wasm` are generated and
not committed.

Everything published at the repository root (`index.html`, `projects/`,
`guides/`, `about/`, `contribute/`, `assets/` and `sitemap.xml`) is output from
`make export`. Edit the sources in `site/`, not the generated HTML.

## Building

```sh
cd site
make            # requires the WASI SDK and a CWIST checkout
make run        # http://localhost:8080
make export     # regenerate the static site in the repository root
```

Override the toolchain paths if yours differ:

```sh
make CWIST_ROOT=/path/to/CWIST WASI_SDK=/path/to/wasi-sdk WASMTIME=/path/to/wasmtime
```

## Continuous integration

`.github/workflows/build-site.yml` rebuilds the site on every push that touches
`site/`. It builds the CWIST WASI archive, links the application against it,
runs the exporter under wasmtime, and commits the result. The published HTML is
therefore always the output of a C program, and never something edited by hand.

## Editing content

All copy lives in the three header files under `site/`. Adding a guide means
appending one entry to `g_guides` in `guides.h`; the index page, the project
page listing and the sitemap all pick it up automatically.

## Licence

The content and code in this repository are MIT, matching CWIST. See
[`LICENSE`](LICENSE).

The projects this site describes are licensed separately: CWIST is MIT,
libttak is BSD 3-Clause, and CWIST's vendored dependencies keep their own
terms. Each repository's own `LICENSE` file is the authoritative text.

The bundled web fonts are under the SIL Open Font License 1.1; see
[`site/fonts/README.md`](site/fonts/README.md).
