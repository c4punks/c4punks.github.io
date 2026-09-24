# Web fonts

Three latin-subset variable woff2 files, embedded into `site.wasm` at build
time and served from CWIST's asset registry.

| File | Family | Upstream | Licence |
| --- | --- | --- | --- |
| `inter.woff2` | Inter | <https://github.com/rsms/inter> | SIL Open Font License 1.1 |
| `grotesk.woff2` | Space Grotesk | <https://github.com/floriankarsten/space-grotesk> | SIL Open Font License 1.1 |
| `jbmono.woff2` | JetBrains Mono | <https://github.com/JetBrains/JetBrainsMono> | SIL Open Font License 1.1 |

Each file is the latin subset (`U+0000-00FF` and friends) of the upstream
variable font, as served by the Google Fonts CSS API. The OFL permits
redistribution of the font files, including embedded in a binary, provided the
licence notice travels with them. That notice is this file.

The fonts are not modified beyond subsetting, and no reserved font name is
used for a derivative.
