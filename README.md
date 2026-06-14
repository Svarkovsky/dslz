<sub>*DS-LZ is **public source** — the code is open for inspection, modification, and non-commercial use under CC BY-NC-SA 4.0.*</sub>

---

# DS-LZ

Delta-Stride LZ — domain-optimized LZ77 variant for tile-based graphics and retro emulator state compression.

Achieves **822× compression** on MSX VRAM by exploiting vertical tile-row correlation through delta-XOR stride preprocessing.

- **Single-header C library** — drop `ds-lz.h` into your project
- **Zero heap RAM decompression** — works on microcontrollers
- **Streaming I/O** — compress/decompress directly to storage

---

## Quick Start

```c
#define DS_LZ_IMPLEMENTATION
#include "ds-lz.h"

// Compress
dslz_compress(vram, 16384, fp_out, 1, 128);

// Decompress
dslz_decompress(fp_in, vram, 16384);
```

See [SPECIFICATION.md](SPECIFICATION.md) for the full bitstream format and algorithm details.

---

## License

**CC BY-NC-SA 4.0**

- ✅ Share — copy and redistribute
- ✅ Adapt — remix and build upon
- ❌ **Commercial use is prohibited without written permission**
- ⚠️ ShareAlike — derivatives must use the same license

Attribution required: name, link to repository, indication of changes.

Full license: [LICENSE.md](LICENSE.md) · [NOTICE.md](NOTICE.md)

Commercial licensing: ivansvarkovsky@gmail.com

---

## Author

Ivan Svarkovsky — ivansvarkovsky@gmail.com  
Repository: https://github.com/Svarkovsky/dslz
