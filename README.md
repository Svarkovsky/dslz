<sub>DS-LZ is public source — the code is open for inspection, modification, and non-commercial use under CC BY-NC-SA 4.0.</sub>

# DS-LZ — Delta-Stride LZ Compression

[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/License-CC_BY--NC--SA_4.0-lightgrey.svg)](https://creativecommons.org/licenses/by-nc-sa/4.0/)
[![Language: C99](https://img.shields.io/badge/language-C99-blue.svg)](https://en.wikipedia.org/wiki/C99)
[![Platform: ESP32-S3](https://img.shields.io/badge/platform-ESP32--S3-green.svg)](https://www.espressif.com/)
[![Single Header](https://img.shields.io/badge/single--header-yes-orange.svg)](#quick-start)
[![MSX State: 822×](https://img.shields.io/badge/MSX_State-822×-purple.svg)](#performance)

> *"DS-LZ is not a point on the chart. It's a new axis."*

A domain-optimized LZ77 compression library for tile-based graphics and retro emulator state.  
Achieves **822× compression** on MSX emulator state with **zero heap RAM** for decompression.

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

Block type is determined automatically from `use_delta` and `stride` parameters:

| use_delta | Stride | Block Type | Use case |
|:---------:|:------:|:----------:|----------|
| `0` | — | `0` | Generic data (no delta) |
| `1` | `128` | `1` | MSX Screen 0-5, 13 (VRAM) |
| `1` | `256` | `2` | MSX Screen 6-12 (high-res VRAM) |

## API Reference

| Function | Description |
|----------|-------------|
| `dslz_compress(src, len, out, use_delta, stride)` | Compress with domain transform |
| `dslz_decompress(in, dst, len)` | Decompress (0 heap RAM) |
| `dslz_delta_encode(data, size, stride)` | Apply 2D delta-XOR |
| `dslz_delta_decode(data, size, stride)` | Reverse 2D delta-XOR |

Full bitstream format: [SPECIFICATION.md](SPECIFICATION.md)

## Performance

### Real-World Results (ESP32-S3, 240 MHz)

All states from MSX2 emulator: 4.4 MB total (128 KB VRAM + 64 KB RAM + CPU state + mappers).  
Total save time: ~50 ms (including SD card I/O). Encode speed on ESP32-S3: ~4 MB/s.

| Game | Scene | Compressed | Ratio |
|------|-------|------------|-------|
| Space Manbow | Title screen | 5.23 KB | **822×** |
| Penguin Adventure | Title screen | 15.81 KB | **278×** |
| King's Valley II | Title screen | 27.42 KB | **160×** |
| Space Manbow | In-game | 44.93 KB | **98×** |
| Aleste | In-game | 53.47 KB | **82×** |
| Psycho World | In-game | 46.62 KB | **94×** |
| Gofer no Yabou II | In-game | 304.62 KB | **14×** |

> **Title screens:** 160–822× compression (high vertical correlation).  
> **In-game:** 14–98× (mixed content: graphics + game state).

### vs Universal Algorithms (x86-64 Desktop Benchmark)

| Algorithm | Compression (VRAM title) | Encode | Decode | Decode RAM |
|-----------|--------------------------|--------|--------|------------|
| **DS-LZ** | **822×** | ~15 MB/s | ~200 MB/s | **0 bytes** |
| LZ4 | 2× | 450 MB/s | 3.5 GB/s | 0 bytes |
| Zstd level 1 | 3.2× | 250 MB/s | 900 MB/s | 128 KB |
| ZX0 | 2.9× | 2 MB/s | ~200 MB/s | 0 bytes |
| LZSA2 | 3× | 8 MB/s | 1.5 GB/s | 0 bytes |

> Benchmarks measured on x86-64 desktop. On ESP32-S3, DS-LZ encodes at ~4 MB/s, decodes at ~30 MB/s.

## Use Cases

**Designed for:** Retro emulator save states, tile-based graphics, VRAM dumps, embedded systems with tight RAM.

**Not suitable for:** General-purpose text compression, natural images, audio/video.

## Requirements

- **C99 compiler** (GCC, Clang, xtensa-esp32-elf)
- Standard library: `<stdio.h>`, `<stdlib.h>`, `<string.h>`

## Acknowledgments

DS-LZ builds upon decades of compression research and the work of many talented engineers.

### Foundational Work
- **Abraham Lempel & Jacob Ziv** — LZ77 algorithm (1977), the foundation of all LZ-based compression. [Paper](https://ieeexplore.ieee.org/document/1055714)
- **Phil Katz** — PKZIP and the DEFLATE algorithm (1989), brought compression to the masses
- **Jean-loup Gailly & Mark Adler** — gzip and zlib, the most widely used compression libraries. [zlib.net](https://zlib.net/)

### Modern Compressors
- **Yann Collet** — LZ4 and Zstd, pushing the boundaries of speed vs ratio. [LZ4](https://github.com/lz4/lz4) · [Zstd](https://github.com/facebook/zstd)
- **Ilya Muravyov** — LZMA and 7-Zip, achieving extreme compression ratios. [7-zip.org](https://7-zip.org/)
- **Julian Seward** — bzip2, Burrows-Wheeler transform for high compression. [bzip.org](http://bzip.org/)
- **Rich Geldreich** — LZHAM, LZMA-class compression with faster decompression. [GitHub](https://github.com/richgel999/lzham_codec)

### Retro & 8-bit Compression
- **Einar Saukas** — ZX0 and ZX7, optimal LZ77 for Z80 and 8-bit CPUs. [GitHub](https://github.com/einar-saukas/ZX0)
- **Emmanuel Marty** — LZSA, efficient compression for retro systems. [GitHub](https://github.com/emmanuel-marty/lzsa)
- **Jørgen Ibsen** — aPLib and aPUltra, demoscene compression classics. [IbSEN Software](http://ibsensoftware.com/)
- **Magnus Lind** — Exomizer, the gold standard for Commodore 64 compression. [Bitbucket](https://bitbucket.org/magli143/exomizer/)

### MSX & Tile-Based Graphics
- **Marat Fayzullin** — fMSX, the portable MSX emulator that started it all. [fMSX](https://fms.komkon.org/fMSX/)
- **Team Bomba** — Pletter, MSX-specific LZ compressor. [GitHub](https://github.com/bombteam/pletter)
- **Grauw** — Bitbuster, optimal MSX compression. [Grauw.nl](https://www.grauw.nl/projects/bitbuster/)
- **Vortex** — MegaLZ, classic MSX demoscene compressor. [MegaLZ](http://vortex.untergrund.net/megalz.htm)

### Platform-Specific Formats
- **Nintendo** — LC_LZ77, RLUncomp, and Huffman compression for GBA/DS. [GBATEK](https://problemkaputt.de/gbatek.htm)
- **Sega** — Kosinski, Enigma, and Nemesis compression for Genesis/Mega Drive. [Sega Retro](https://segaretro.org/Compression)

### Standards & Specifications
- **W3C** — PNG specification with delta filters (Sub, Up, Paeth). [RFC 2083](https://www.w3.org/TR/PNG/)
- **ITU-T** — JPEG-LS (T.87) and LOCO-I predictive coding. [ITU](https://www.itu.int/rec/T-REC-T.87)

### Academic References
- **Storer, J. & Szymanski, T.** — "Data Compression via Textual Substitution" (1982). [JACM](https://dl.acm.org/doi/10.1145/322344.322356)
- **Welch, T.** — "A Technique for High-Performance Data Compression" (1984), LZW algorithm. [IEEE](https://ieeexplore.ieee.org/document/1659047)

*DS-LZ is an independent implementation. All referenced works are used for educational and inspirational purposes only.*

## License

**CC BY-NC-SA 4.0**

- ✅ Share — copy and redistribute
- ✅ Adapt — remix and build upon
- ❌ **Commercial use is prohibited without written permission**
- ⚠️ ShareAlike — derivatives must use the same license

**Attribution required:** name, link to repository, indication of changes.

Full license: [LICENSE.md](LICENSE.md) · [NOTICE.md](NOTICE.md)

Commercial licensing: ivansvarkovsky@gmail.com

Based on public-domain LZ77. All patents expired. Independent implementation.

## Author

**Ivan Svarkovsky** — [GitHub](https://github.com/Svarkovsky) — ivansvarkovsky@gmail.com
