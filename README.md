<sub>*DS-LZ is **public source** — the code is open for inspection, modification, and non-commercial use under CC BY-NC-SA 4.0.*</sub>

# DS-LZ — Delta-Stride LZ Compression

[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/License-CC_BY--NC--SA_4.0-lightgrey.svg)](https://creativecommons.org/licenses/by-nc-sa/4.0/)
[![Language: C99](https://img.shields.io/badge/language-C99-blue.svg)](https://en.wikipedia.org/wiki/C99)
[![Platform: ESP32-S3](https://img.shields.io/badge/platform-ESP32--S3-green.svg)](https://www.espressif.com/)
[![Single Header](https://img.shields.io/badge/single--header-yes-orange.svg)](#quick-start)
[![MSX VRAM: 822×](https://img.shields.io/badge/MSX_VRAM-822×-purple.svg)](#performance)

> *"DS-LZ is not a point on the chart. It's a new axis."*

A domain-optimized LZ77 compression library for tile-based graphics and retro emulator state.  
Achieves **822× compression** on MSX VRAM with **zero heap RAM** for decompression.

## Features

- **Domain-aware delta-XOR transform** with configurable stride (128/256 bytes)
- **Pre-emptive stride rep-match** — exploits vertical tile-row correlation
- **Cost-based lazy matching** — near-optimal parsing for short matches
- **Rep-match caching** — repeated offsets in 1 byte
- **Zero heap RAM decompression** — only 4 bytes on stack
- **Single-header C99** — just `#include "ds-lz.h"`
- **Streaming I/O** — compress/decompress directly to `FILE*`

## Quick Start

```c
#define DS_LZ_IMPLEMENTATION
#include "ds-lz.h"

// Compress with delta-stride for MSX Screen 0-5 (stride=128)
dslz_compress(vram, 16384, fp, 1, 128);

// Decompress (format auto-detected from stream header)
dslz_decompress(fp, vram, 16384);
```

### Block Types

| Type | Stride | Use case |
|------|--------|----------|
| `0` | — | Generic data (no delta) |
| `1` | `128` | MSX Screen 0-5, 13 (VRAM) |
| `2` | `256` | MSX Screen 6-12 (high-res VRAM) |

## API Reference

| Function | Description |
|----------|-------------|
| `dslz_compress(src, len, out, type, stride)` | Compress with domain transform |
| `dslz_decompress(in, dst, len)` | Decompress (0 heap RAM) |
| `dslz_delta_encode(data, size, stride)` | Apply 2D delta-XOR |
| `dslz_delta_decode(data, size, stride)` | Reverse 2D delta-XOR |

Full bitstream format: [SPECIFICATION.md](SPECIFICATION.md)

## Performance

> All states are from a 4.4 MB MSX emulator save (VRAM + RAM + CPU state).

### Real-World Results

| Game | Scene | Compressed | Ratio | Time |
|------|-------|------------|-------|------|
| Space Manbow | Title screen | 5.23 KB | **822×** | ~50 ms |
| Space Manbow | In-game | 39.87 KB | **108×** | ~150 ms |
| King's Valley II | Title screen | 28.21 KB | **156×** | ~50 ms |
| Psycho World | Title screen | 45.73 KB | **96×** | ~50 ms |

### vs Universal Algorithms (VRAM Title Screen)

| Algorithm | Compression | Encode | Decode | Decode RAM |
|-----------|-------------|--------|--------|------------|
| **DS-LZ** | **822×** | ~15 MB/s | ~200 MB/s | **0 bytes** |
| LZ4 | 2× | 450 MB/s | 3.5 GB/s | 0 bytes |
| Zstd level 1 | 3.2× | 250 MB/s | 900 MB/s | 128 KB |
| ZX0 | 2.9× | 2 MB/s | ~200 MB/s | 0 bytes |
| LZSA2 | 3× | 8 MB/s | 1.5 GB/s | 0 bytes |

## Use Cases

**Designed for:** Retro emulator save states, tile-based graphics, VRAM dumps, embedded systems with tight RAM.

**Not suitable for:** General-purpose text compression, natural images, audio/video.

## Requirements

- **C99 compiler** (GCC, Clang, xtensa-esp32-elf)
- Standard library: `<stdio.h>`, `<stdlib.h>`, `<string.h>`

## Acknowledgments

- **Abraham Lempel & Jacob Ziv** — LZ77 (1977), public domain
- **Marat Fayzullin** — [fMSX](https://fms.komkon.org/fMSX/) emulator
- **Einar Saukas** — ZX0, optimal LZ77 for 8-bit CPUs
- **Emmanuel Marty** — LZSA, fast retro compression

## License

**CC BY-NC-SA 4.0**

- ✅ Share — copy and redistribute
- ✅ Adapt — remix and build upon
- ❌ **Commercial use is prohibited without written permission**
- ⚠️ ShareAlike — derivatives must use the same license

Attribution required: name, link to repository, indication of changes.

Full license: [LICENSE.md](LICENSE.md) · [NOTICE.md](NOTICE.md)

Commercial licensing: ivansvarkovsky@gmail.com

## Author

**Ivan Svarkovsky** — [GitHub](https://github.com/Svarkovsky) — ivansvarkovsky@gmail.com
