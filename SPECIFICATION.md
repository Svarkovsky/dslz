

# DS-LZ Compression Format Specification

**Author:** Ivan Svarkovsky  
**Date:** June 2026  
**License:** CC BY-NC-SA 4.0  
**Repository:** [github.com/Svarkovsky/dslz](https://github.com/Svarkovsky/dslz)

## 1. Introduction

DS-LZ (Delta-Stride LZ) is a domain-optimized LZ77 variant designed for tile-based graphics compression, particularly retro emulator save states. It achieves 822× compression on MSX emulator state by exploiting vertical tile-row correlation through a delta-XOR stride preprocessing step.

This specification describes the DS-LZ bitstream format, the delta preprocessing algorithm, and the compressor/decompressor architecture. Implementations may target any platform; compliance with this specification ensures interoperability.

### 1.1 Terminology

| Term | Definition |
|------|------------|
| Literal | A raw byte copied directly to the output |
| Match | A back-reference to previously decompressed data |
| Offset | Distance in bytes from the current position to the match source |
| Length | Number of bytes to copy from the match source |
| Stride | Row width in bytes for 2D delta preprocessing (128 or 256 for MSX) |
| Rep-match | A match that reuses the most recently used offset |
| Last offset | The offset value from the most recently emitted match |

## 2. Bitstream Format

DS-LZ uses a byte-oriented variable-length encoding. All multi-byte values are little-endian.

### 2.1 Header

The stream begins with a 7-byte header:

| Byte | Content |
|------|---------|
| 0 | Delta flag: `0xFF` = delta encoded, `0x00` = raw |
| 1–4 | Uncompressed data length (32-bit LE) |
| 5–6 | Stride value (16-bit LE, 0 if no delta) |

### 2.2 Command Tokens

After the header, the stream consists of a sequence of variable-length commands:

**Literals (0–127)**  
`0xxxxxxx`  
- Range: `0x00`–`0x7F` (0–127)  
- Length: `cmd + 1` (1–128 bytes)  
- Payload: `length` raw bytes following the command byte  

**Near Match (128–191)**  
`10xxxxxx [offset8]`  
- Range: `0x80`–`0xBF` (128–191)  
- Length: `(cmd & 0x3F) + 3` (3–66 bytes)  
- Offset: 1 byte (1–255), follows the command byte  
- Total size: 2 bytes  

**Far Match Short (192–223)**  
`110xxxxx [offset16]`  
- Range: `0xC0`–`0xDF` (192–223)  
- Length: `(cmd & 0x1F) + 4` (4–35 bytes)  
- Offset: 2 bytes (1–65535), little-endian, follow the command byte  
- Total size: 3 bytes  

**Rep Match Short (224–239)**  
`1110xxxx`  
- Range: `0xE0`–`0xEF` (224–239)  
- Length: `(cmd & 0x0F) + 3` (3–18 bytes)  
- Offset: Uses `last_offset` (no offset bytes in stream)  
- Total size: 1 byte  

**Long Literals (240–247)**  
`11110xxx [data...]`  
- Range: `0xF0`–`0xF7` (240–247)  
- Length: `(cmd & 0x07) + 129` (129–136 bytes)  
- Payload: `length` raw bytes following the command byte  
- Total size: 1 + `length` bytes  

**Escape (248)**  
`11111000 [sub] [payload...]`  
- Value: `0xF8` (248)  
- Sub-command: 1 byte following the escape byte  

| Sub | Name | Payload | Length Formula | Total Size |
|-----|------|---------|----------------|------------|
| 0 | Long literals extended | `len16` + data | `len16` + 137 (137–65672) | 3 + length |
| 1 | Long far match | `len16` + `off16` | `len16` (1–65535) | 5 bytes |
| 2 | Long rep match | `len16` | `len16` (1–65535) | 3 bytes |
| 3 | Long near match | `len16` + `off8` | `len16` (1–65535) | 4 bytes |

*Notes:*  
- `len16` is a 16-bit little-endian value  
- `off16` is a 16-bit little-endian value  
- `off8` is an 8-bit value (1–255)  
- Reserved sub-commands (4–255) must cause the decoder to abort  

### 2.3 End of Stream

The stream ends when the decompressor has produced exactly `uncompressed_length` bytes as specified in the header. No explicit end marker is used.

## 3. Delta-XOR Preprocessing

### 3.1 Algorithm

Delta encoding transforms the input data to improve compression ratio. It is applied before LZ77 compression and reversed after decompression. The stride parameter must be greater than 0; a stride of 0 or less disables delta encoding entirely.

**2D Delta Encoding (stride > 0):**  
- Pass 1 (vertical): For i = size-1 down to stride: `data[i] ^= data[i - stride]`  
- Pass 2 (horizontal): For i = size-1 down to 1: If `i % stride != 0`: `data[i] ^= data[i - 1]`  

**2D Delta Decoding (stride > 0):**  
- Pass 1 (horizontal): For i = 1 to size-1: If `i % stride != 0`: `data[i] ^= data[i - 1]`  
- Pass 2 (vertical): For i = stride to size-1: `data[i] ^= data[i - stride]`  

*Note: If stride ≤ 0, the delta functions are no-ops and the data passes through uncompressed by the delta stage.*

### 3.2 Stride Values

| Platform | Screen Mode | Stride |
|----------|-------------|--------|
| MSX | Screen 0–5, 13 | 128 |
| MSX | Screen 6–12 | 256 |
| NES | Name Table | 32 |
| Game Boy | Tile Map | 32 |
| Sega Genesis | Plane A/B | 64 or 128 |

### 3.3 Effect

After delta encoding with the correct stride, identical tile rows become sequences of zeros. This transforms the problem from "find repeated byte patterns" to "find runs of zeros" — which LZ77 handles with near-perfect efficiency.

## 4. Compressor Architecture

### 4.1 Hash Table

The compressor uses a 2-way associative hash table with 4096 buckets, each containing two 32-bit position values. Total size: 32 KB.  

Hash function:  
```c
h = (p[0] * 0x9E3779B1u) ^ (p[1] * 0x85EBCA77u) ^ p[2];
return (h ^ (h >> 12)) & 0xFFF;  // 12 bits → 4096 buckets
```

### 4.2 Match Finding

The compressor searches for matches in this priority order:  
1. **Pre-emptive stride match:** Check offset = stride (for VRAM data). If found, use immediately.  
2. **Hash-based search:** Query the 2-way associative hash table. Compare up to 2 previous positions.  
3. **Last-offset rep-match:** Check if the previous match offset produces a valid match at the current position.  

### 4.3 Cost-Based Lazy Matching

For matches shorter than 16 bytes, the compressor evaluates position `i+1`. If the match at `i+1` provides a better cost-to-benefit ratio, the compressor emits a single literal byte at position `i` and uses the match at `i+1`.

**Cost model:**  
- Rep match ≤ 18 bytes: 1 byte  
- Rep match > 18 bytes: 4 bytes  
- Near match ≤ 66 bytes: 2 bytes  
- Near match > 66 bytes: 5 bytes  
- Far match ≤ 35 bytes: 3 bytes  
- Far match > 35 bytes: 6 bytes  

### 4.4 Internal Hash Update

When a match of length N is emitted, the hash table is updated for the first `min(N, 4)` positions inside the match. This prevents the loss of nested patterns.

## 5. Decompressor Architecture

The decompressor requires 0 bytes of heap memory. It reconstructs data in-place by copying from already-decompressed portions of the output buffer.

### 5.1 Algorithm

```python
while output_pos < output_len:
    cmd = read_byte()
    
    if cmd <= 127:
        copy cmd+1 literal bytes from input to output
    elif cmd <= 191:
        offset = read_byte()
        copy (cmd & 0x3F) + 3 bytes from output[output_pos - offset]
        last_offset = offset
    elif cmd <= 223:
        offset = read_u16_le()
        copy (cmd & 0x1F) + 4 bytes from output[output_pos - offset]
        last_offset = offset
    elif cmd <= 239:
        copy (cmd & 0x0F) + 3 bytes from output[output_pos - last_offset]
    elif cmd <= 247:
        copy (cmd & 0x07) + 129 literal bytes from input to output
    elif cmd == 248:
        sub = read_byte()
        # Handle extended commands per Table in Section 2.2
```

### 5.2 Memory Model

- **Input buffer:** Streamed from storage (file, flash, network)  
- **Output buffer:** Pre-allocated destination array  
- **Working memory:** 3 stack variables (`output_pos`, `last_offset`, `cmd`)  
- **No heap allocation**  

## 6. Performance Characteristics

| Metric | Value |
|--------|-------|
| Maximum match length | 65,535 bytes |
| Sliding window | 64 KB |
| Minimum match | 3 bytes (4 for far matches with offset > 255) |
| Compression ratio (MSX state, title screen) | 822× |
| Compression ratio (MSX state, in-game) | 14–108× |
| Encoder RAM | 32 KB (hash table) |
| Decoder RAM | 0 bytes (in-place) |
| Encoder speed (x86-64 desktop) | ~15 MB/s |
| Decoder speed (x86-64 desktop) | ~200 MB/s |
| Encoder speed (ESP32-S3, 240 MHz) | ~4 MB/s |
| Decoder speed (ESP32-S3, 240 MHz) | ~30 MB/s |

## 7. Reference Implementation

A reference implementation is available as a single-header C library:  
- **Repository:** [github.com/Svarkovsky/dslz](https://github.com/Svarkovsky/dslz)  
- **File:** `ds-lz.h`  
- **API:** `dslz_compress()`, `dslz_decompress()`, `dslz_delta_encode()`, `dslz_delta_decode()`  

## 8. Security Considerations

The decompressor trusts the input stream. Malformed data may cause the decoder to read past the input buffer or write past the output buffer. Implementations should validate:  
- That offsets do not exceed the current output position  
- That match lengths do not exceed the remaining output space  
- That literal lengths do not exceed the remaining output space  
- That the total decompressed size matches the header  

## References

- Lempel, A., Ziv, J. (1977). "A Universal Algorithm for Sequential Data Compression." *IEEE Transactions on Information Theory*  
- Storer, J., Szymanski, T. (1982). "Data Compression via Textual Substitution." *Journal of the ACM*  
- Fayzullin, M. (1994-2021). fMSX: Portable MSX Emulator  
- Collet, Y. (2011). LZ4: Extremely Fast Compression  
- Marty, E. (2019). LZSA: Efficient LZ77 Compression for 8-bit CPUs  
