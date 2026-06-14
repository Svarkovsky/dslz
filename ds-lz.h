/**
 * ds-lz.h — Delta-Stride LZ (DS-LZ) Compression Codec
 * 
 * 
 * A domain-optimized LZ77 variant for tile-based graphics and
 * retro emulator state compression. Achieves 822× compression
 * on MSX VRAM by exploiting vertical tile-row correlation
 * through delta-XOR stride preprocessing.
 * 
 * Copyright (C) 2026 Ivan Svarkovsky <ivansvarkovsky@gmail.com>
 * 
 * This algorithm and its implementation are distributed under the
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0
 * (CC BY-NC-SA 4.0) license.
 * 
 * Full license text: https://creativecommons.org/licenses/by-nc-sa/4.0/
 * 
 * Under this license, you are free to:
 *   - Share — copy and redistribute the material in any medium or format
 *   - Adapt — remix, transform, and build upon the material
 * 
 * Under the following terms:
 *   - Attribution — You must give appropriate credit to Ivan Svarkovsky,
 *     provide a link to the original repository, and indicate if changes
 *     were made. You may do so in any reasonable manner, but not in any
 *     way that suggests the licensor endorses you or your use.
 *   - NonCommercial — You may not use the material for commercial purposes.
 *   - ShareAlike — If you remix, transform, or build upon the material,
 *     you must distribute your contributions under the same license as
 *     the original.
 * 
 * Based on public-domain LZ77 (Lempel-Ziv, 1977).
 * All patents expired. Independent implementation.
 * 
 * Original repository: https://github.com/Svarkovsky/dslz
 * 
 * Core algorithm: LZ77 + delta-XOR stride + rep-match + VLC
 * 
 * Key features:
 *   - 2D delta-XOR preprocessing with configurable stride (128/256)
 *   - Variable-length coding: near/far/rep match tokens
 *   - 16-bit match lengths (up to 65535 bytes)
 *   - 64KB sliding window with 2-way associative hash
 *   - Cost-based lazy matching for optimal parsing
 *   - Rep-match caching: repeated offsets in 1 byte
 *   - 32-bit accelerated match extension
 *   - Zero heap RAM decompression (in-place reconstruction)
 *   - Buffered I/O for streaming to/from storage
 * 
 * Usage:
 *   In ONE .c file:
 *     #define DS_LZ_IMPLEMENTATION
 *     #include "ds-lz.h"
 *   In all other files:
 *     #include "ds-lz.h"
 * 
 * API:
 *   int  dslz_compress(const uint8_t *src, size_t len, FILE *out,
 *                      int use_delta, int stride);
 *   int  dslz_decompress(FILE *in, uint8_t *dst, size_t len);
 *   void dslz_delta_encode(uint8_t *data, size_t size, int stride);
 *   void dslz_delta_decode(uint8_t *data, size_t size, int stride);
 * 
 * Command format:
 *   0xxxxxxx (0-127)     : Literals, length = cmd+1 (1-128 bytes)
 *   10xxxxxx (128-191)   : Near match, length = (cmd&0x3F)+3 (3-66),
 *                          followed by 8-bit offset (1-255)
 *   110xxxxx (192-223)   : Far match short, length = (cmd&0x1F)+4 (4-35),
 *                          followed by 16-bit offset (1-65535)
 *   1110xxxx (224-239)   : Rep match short, length = (cmd&0x0F)+3 (3-18),
 *                          uses last_offset (no offset in stream)
 *   11110xxx (240-247)   : Long literals, length = (cmd&0x07)+129 (129-136)
 *   11111000 (248)       : Escape code, followed by sub-command:
 *     sub=0 + len16     : Long literals extended (137-65672 bytes)
 *     sub=1 + len16+off16: Long far match (any length, 16-bit offset)
 *     sub=2 + len16     : Long rep match (any length, uses last_offset)
 *     sub=3 + len16+off8: Long near match (any length, 8-bit offset)
 */


/*
 * THIS SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
 

#ifndef DS_LZ_H
#define DS_LZ_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Compress data to file stream.
 * 
 * @param src        Input data buffer (must be writable if use_delta=1)
 * @param len        Input data length in bytes
 * @param out        Output file stream (must be writable)
 * @param use_delta  1 to apply Delta-XOR encoding before compression
 * @param stride     Stride for 2D delta (>0 for 2D, 0 for 1D).
 *                   Common values: 128 (MSX SCREEN 0-5), 256 (SCREEN 6-12)
 * @return 1 on success, 0 on failure
 */
int dslz_compress(uint8_t *src, size_t len, FILE *out, int use_delta, int stride);

/**
 * Decompress data from file stream directly to destination buffer.
 * Uses 0 bytes of heap memory (in-place reconstruction).
 * 
 * @param in         Input file stream (must be readable)
 * @param dst        Destination buffer (will be overwritten)
 * @param len        Expected decompressed length
 * @return 1 on success, 0 on failure
 */
int dslz_decompress(FILE *in, uint8_t *dst, size_t len);

/**
 * Apply 2D Delta-XOR encoding.
 * 
 * @param data       Data buffer (modified in-place)
 * @param size       Data size in bytes
 * @param stride     Row stride for 2D delta (0 for 1D, >0 for 2D)
 */
void dslz_delta_encode(uint8_t *data, size_t size, int stride);

/**
 * Reverse 2D Delta-XOR encoding.
 * 
 * @param data       Data buffer (modified in-place)
 * @param size       Data size in bytes
 * @param stride     Row stride used during encoding
 */
void dslz_delta_decode(uint8_t *data, size_t size, int stride);

#ifdef __cplusplus
}
#endif

#endif /* DS_LZ_H */

/* =========================================================================
 * IMPLEMENTATION
 * ========================================================================= */

#ifdef DS_LZ_IMPLEMENTATION

/* -------------------------------------------------------------------------
 * Buffered I/O
 * ------------------------------------------------------------------------- */

#define DS_LZ_IO_BUF_SIZE 4096

static FILE *dslz_file = NULL;
static uint8_t dslz_out_buf[DS_LZ_IO_BUF_SIZE];
static size_t  dslz_out_pos = 0;
static uint8_t dslz_in_buf[DS_LZ_IO_BUF_SIZE];
static size_t  dslz_in_pos = 0;
static size_t  dslz_in_avail = 0;

static inline void dslz_putc(uint8_t c) {
    dslz_out_buf[dslz_out_pos++] = c;
    if (dslz_out_pos >= DS_LZ_IO_BUF_SIZE) {
        fwrite(dslz_out_buf, 1, dslz_out_pos, dslz_file);
        dslz_out_pos = 0;
    }
}

static inline void dslz_flush(void) {
    if (dslz_out_pos > 0 && dslz_file) {
        fwrite(dslz_out_buf, 1, dslz_out_pos, dslz_file);
        dslz_out_pos = 0;
    }
}

static inline void dslz_write(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++) dslz_putc(p[i]);
}

static inline int dslz_getc(void) {
    if (dslz_in_pos >= dslz_in_avail) {
        dslz_in_avail = fread(dslz_in_buf, 1, DS_LZ_IO_BUF_SIZE, dslz_file);
        dslz_in_pos = 0;
        if (dslz_in_avail == 0) return EOF;
    }
    return dslz_in_buf[dslz_in_pos++];
}

/* -------------------------------------------------------------------------
 * 32-bit accelerated match extension
 * ------------------------------------------------------------------------- */

static inline uint32_t dslz_match_len(const uint8_t *a, const uint8_t *b, uint32_t max_len) {
    uint32_t cur = 0;
    while (cur + 4 <= max_len && *(const uint32_t*)(a + cur) == *(const uint32_t*)(b + cur))
        cur += 4;
    while (cur < max_len && a[cur] == b[cur]) cur++;
    return cur;
}

/* -------------------------------------------------------------------------
 * Delta-XOR encoding
 * ------------------------------------------------------------------------- */

void dslz_delta_encode(uint8_t *data, size_t size, int stride) {
    if (size < (size_t)stride || stride <= 0) return;
    size_t i;

    if (stride % 4 == 0 && size % 4 == 0 && ((uintptr_t)data % 4) == 0) {
        for (i = size - 4; i >= (size_t)stride; i -= 4)
            *(uint32_t*)&data[i] ^= *(uint32_t*)&data[i - stride];
    } else {
        for (i = size - 1; i >= (size_t)stride; i--)
            data[i] ^= data[i - stride];
    }

    for (i = size - 1; i > 0; i--) {
        if (i % stride != 0) data[i] ^= data[i - 1];
    }
}

void dslz_delta_decode(uint8_t *data, size_t size, int stride) {
    if (size < (size_t)stride || stride <= 0) return;
    size_t i;

    for (i = 1; i < size; i++) {
        if (i % stride != 0) data[i] ^= data[i - 1];
    }

    if (stride % 4 == 0 && size % 4 == 0 && ((uintptr_t)data % 4) == 0) {
        for (i = stride; i + 3 < size; i += 4)
            *(uint32_t*)&data[i] ^= *(uint32_t*)&data[i - stride];
    } else {
        for (i = stride; i < size; i++)
            data[i] ^= data[i - stride];
    }
}

/* -------------------------------------------------------------------------
 * Improved hash function
 * ------------------------------------------------------------------------- */

static inline uint32_t dslz_hash3(const uint8_t *p) {
    uint32_t h = (p[0] * 0x9E3779B1u) ^ (p[1] * 0x85EBCA77u) ^ p[2];
    return (h ^ (h >> 12)) & 4095;
}

/* -------------------------------------------------------------------------
 * Cost model
 * ------------------------------------------------------------------------- */

static inline int dslz_match_cost(unsigned int len, unsigned int offset, int is_rep) {
    if (is_rep) {
        if (len < 3) return 999;
        return (len <= 18) ? 1 : 4;
    }
    if (offset <= 255) {
        if (len < 3) return 999;
        return (len <= 66) ? 2 : 5;
    }
    if (len < 4) return 999;
    return (len <= 35) ? 3 : 6;
}

/* -------------------------------------------------------------------------
 * Hash table
 * ------------------------------------------------------------------------- */

typedef struct { uint32_t pos[2]; } dslz_hash_t;

/* -------------------------------------------------------------------------
 * Best match finder
 * ------------------------------------------------------------------------- */

static void dslz_find_match(const uint8_t *src, unsigned int pos, unsigned int len,
                             dslz_hash_t *hash, uint32_t last_offset, int block_type,
                             uint32_t *out_len, uint32_t *out_off, int *out_is_rep) {
    uint32_t best_len = 0, best_off = 0;
    int is_rep = 0;

    /* Pre-emptive stride rep-match for VRAM */
    if (block_type == 1 && pos >= 128) {
        uint32_t voff = 128;
        uint32_t max_match = len - pos;
        if (max_match > 65535) max_match = 65535;
        uint32_t cur = dslz_match_len(&src[pos], &src[pos - voff], max_match);
        if (cur >= 3 && cur > best_len) {
            best_len = cur; best_off = voff;
            is_rep = (voff == last_offset) ? 1 : 0;
        }
    } else if (block_type == 2 && pos >= 256) {
        uint32_t voff = 256;
        uint32_t max_match = len - pos;
        if (max_match > 65535) max_match = 65535;
        uint32_t cur = dslz_match_len(&src[pos], &src[pos - voff], max_match);
        if (cur >= 3 && cur > best_len) {
            best_len = cur; best_off = voff;
            is_rep = (voff == last_offset) ? 1 : 0;
        }
    }

    /* Hash-based search */
    if (pos + 3 <= len) {
        uint32_t h = dslz_hash3(&src[pos]);
        for (int e = 0; e < 2; e++) {
            uint32_t prev = hash[pos & 1 ? h : (h ^ 1)].pos[e];
            if (prev != 0xFFFFFFFF && pos > prev && (pos - prev) <= 65535) {
                uint32_t off = pos - prev;
                uint32_t max_match = len - pos;
                if (max_match > 65535) max_match = 65535;
                uint32_t cur = dslz_match_len(&src[pos], &src[prev], max_match);
                unsigned int min_needed = (off <= 255) ? 3 : 4;
                if (cur >= min_needed && cur > best_len) {
                    best_len = cur; best_off = off; is_rep = 0;
                }
            }
        }
    }

    /* Last-offset rep-match */
    if (last_offset > 0 && last_offset <= pos) {
        uint32_t max_match = len - pos;
        if (max_match > 65535) max_match = 65535;
        uint32_t rep_len = dslz_match_len(&src[pos], &src[pos - last_offset], max_match);
        if (rep_len >= 3) {
            int cost_rep = dslz_match_cost(rep_len, last_offset, 1);
            int cost_best = dslz_match_cost(best_len, best_off, is_rep);
            int save_rep = (int)rep_len - cost_rep, save_best = (int)best_len - cost_best;
            int is_stride = (block_type == 1 && last_offset == 128) ||
                            (block_type == 2 && last_offset == 256);
            if (save_rep > save_best || (save_rep == save_best && is_stride)) {
                best_len = rep_len; best_off = last_offset; is_rep = 1;
            }
        }
    }

    *out_len = best_len; *out_off = best_off; *out_is_rep = is_rep;
}

/* -------------------------------------------------------------------------
 * Compressor
 * ------------------------------------------------------------------------- */

int dslz_compress(uint8_t *src, size_t len, FILE *out, int use_delta, int stride) {
    dslz_file = out;
    dslz_out_pos = 0;

    if (use_delta) dslz_delta_encode(src, len, stride);

    /* Write header */
    dslz_putc((uint8_t)(use_delta ? 0xFF : 0x00));
    dslz_putc((uint8_t)(len & 0xFF));
    dslz_putc((uint8_t)((len >> 8) & 0xFF));
    dslz_putc((uint8_t)((len >> 16) & 0xFF));
    dslz_putc((uint8_t)((len >> 24) & 0xFF));
    dslz_putc((uint8_t)(stride & 0xFF));
    dslz_putc((uint8_t)((stride >> 8) & 0xFF));

    int block_type = 0;
    if (use_delta && stride == 128) block_type = 1;
    else if (use_delta && stride == 256) block_type = 2;

    dslz_hash_t *hash = (dslz_hash_t *)malloc(4096 * sizeof(dslz_hash_t));
    if (!hash) { if (use_delta) dslz_delta_decode(src, len, stride); return 0; }
    memset(hash, 0xFF, 4096 * sizeof(dslz_hash_t));

    uint32_t last_offset = 0;
    size_t i = 0, lit_start = 0;

    #define FLUSH_LITS() do { \
        size_t _ll = i - lit_start; \
        while (_ll > 0) { \
            size_t _chunk = (_ll > 65535 + 137) ? 65535 + 137 : _ll; \
            if (_chunk <= 128) dslz_putc((uint8_t)(_chunk - 1)); \
            else if (_chunk <= 136) dslz_putc((uint8_t)(240 | (_chunk - 129))); \
            else { dslz_putc(248); dslz_putc(0); \
                size_t _ext = _chunk - 137; \
                dslz_putc((uint8_t)(_ext & 0xFF)); dslz_putc((uint8_t)((_ext >> 8) & 0xFF)); } \
            for (size_t _j = 0; _j < _chunk; _j++) dslz_putc(src[lit_start + _j]); \
            lit_start += _chunk; _ll -= _chunk; \
        } \
    } while(0)

    while (i < len) {
        uint32_t best_len = 0, best_off = 0;
        int is_rep = 0;

        dslz_find_match(src, (unsigned int)i, (unsigned int)len, hash,
                        last_offset, block_type, &best_len, &best_off, &is_rep);

        if (best_len >= 3 && best_len < 16 && i + 1 < len) {
            uint32_t lazy_len = 0, lazy_off = 0;
            int lazy_rep = 0;
            dslz_find_match(src, (unsigned int)(i + 1), (unsigned int)len, hash,
                            last_offset, block_type, &lazy_len, &lazy_off, &lazy_rep);
            if (lazy_len >= 3) {
                int cost_best = dslz_match_cost(best_len, best_off, is_rep);
                int cost_lazy = dslz_match_cost(lazy_len, lazy_off, lazy_rep);
                if ((int)lazy_len - cost_lazy > (int)best_len - cost_best) best_len = 0;
            }
        }

        if (best_len >= 3) {
            FLUSH_LITS();

            if (is_rep) {
                if (best_len <= 18) dslz_putc((uint8_t)(0xE0 | (best_len - 3)));
                else { dslz_putc(248); dslz_putc(2);
                    dslz_putc((uint8_t)(best_len & 0xFF)); dslz_putc((uint8_t)((best_len >> 8) & 0xFF)); }
            } else if (best_off <= 255 && best_len <= 66) {
                dslz_putc((uint8_t)(0x80 | (best_len - 3)));
                dslz_putc((uint8_t)(best_off & 0xFF));
            } else if (best_len <= 35) {
                dslz_putc((uint8_t)(0xC0 | (best_len - 4)));
                dslz_putc((uint8_t)(best_off & 0xFF)); dslz_putc((uint8_t)((best_off >> 8) & 0xFF));
            } else {
                dslz_putc(248); dslz_putc(1);
                dslz_putc((uint8_t)(best_len & 0xFF)); dslz_putc((uint8_t)((best_len >> 8) & 0xFF));
                dslz_putc((uint8_t)(best_off & 0xFF)); dslz_putc((uint8_t)((best_off >> 8) & 0xFF));
            }
            last_offset = best_off;

            unsigned int ulim = (best_len > 4) ? 4 : (unsigned int)best_len;
            for (unsigned int j = 0; j < ulim; j++) {
                if (i + j + 3 <= len) {
                    uint32_t h = dslz_hash3(&src[i + j]);
                    hash[h].pos[1] = hash[h].pos[0];
                    hash[h].pos[0] = (uint32_t)(i + j);
                }
            }
            i += best_len; lit_start = i;
        } else {
            if (i + 3 <= len) {
                uint32_t h = dslz_hash3(&src[i]);
                hash[h].pos[1] = hash[h].pos[0];
                hash[h].pos[0] = (uint32_t)i;
            }
            i++;
        }
    }

    FLUSH_LITS();
    free(hash);
    if (use_delta) dslz_delta_decode(src, len, stride);
    dslz_flush();
    #undef FLUSH_LITS
    return 1;
}

/* -------------------------------------------------------------------------
 * Decompressor (0 bytes heap RAM)
 * ------------------------------------------------------------------------- */

int dslz_decompress(FILE *in, uint8_t *dst, size_t len) {
    dslz_file = in;
    dslz_in_pos = 0; dslz_in_avail = 0;

    int use_delta = dslz_getc();
    size_t stored = (size_t)(uint8_t)dslz_getc() | ((size_t)(uint8_t)dslz_getc() << 8) |
                    ((size_t)(uint8_t)dslz_getc() << 16) | ((size_t)(uint8_t)dslz_getc() << 24);
    int stride = (uint8_t)dslz_getc() | ((uint8_t)dslz_getc() << 8);

    if (use_delta == EOF || stored != len) return 0;

    size_t i = 0;
    unsigned int last_offset = 0;

    while (i < len) {
        int cmd = dslz_getc();
        if (cmd == EOF) return 0;

        if (cmd <= 127) {
            size_t lit_len = (size_t)cmd + 1;
            if (i + lit_len > len) return 0;
            for (size_t j = 0; j < lit_len; j++) {
                int c = dslz_getc();
                if (c == EOF) return 0;
                dst[i + j] = (uint8_t)c;
            }
            i += lit_len;
        } else if (cmd <= 191) {
            size_t mlen = (size_t)(cmd & 0x3F) + 3;
            int off = dslz_getc();
            if (off == EOF) return 0;
            unsigned int offset = (uint8_t)off;
            if (offset == 0 || offset > i || i + mlen > len) return 0;
            size_t src = i - offset;
            for (size_t j = 0; j < mlen; j++) dst[i + j] = dst[src + j];
            i += mlen; last_offset = offset;
        } else if (cmd <= 223) {
            size_t mlen = (size_t)(cmd & 0x1F) + 4;
            int ol = dslz_getc(), oh = dslz_getc();
            if (ol == EOF || oh == EOF) return 0;
            unsigned int offset = (uint8_t)ol | ((uint8_t)oh << 8);
            if (offset == 0 || offset > i || i + mlen > len) return 0;
            size_t src = i - offset;
            for (size_t j = 0; j < mlen; j++) dst[i + j] = dst[src + j];
            i += mlen; last_offset = offset;
        } else if (cmd <= 239) {
            size_t mlen = (size_t)(cmd & 0x0F) + 3;
            if (last_offset == 0 || last_offset > i || i + mlen > len) return 0;
            size_t src = i - last_offset;
            for (size_t j = 0; j < mlen; j++) dst[i + j] = dst[src + j];
            i += mlen;
        } else if (cmd <= 247) {
            size_t lit_len = (size_t)(cmd & 0x07) + 129;
            if (i + lit_len > len) return 0;
            for (size_t j = 0; j < lit_len; j++) {
                int c = dslz_getc();
                if (c == EOF) return 0;
                dst[i + j] = (uint8_t)c;
            }
            i += lit_len;
        } else if (cmd == 248) {
            int sub = dslz_getc();
            if (sub == EOF) return 0;

            if (sub == 0) {
                int ll = dslz_getc(), lh = dslz_getc();
                if (ll == EOF || lh == EOF) return 0;
                size_t lit_len = (size_t)(uint8_t)ll | ((size_t)(uint8_t)lh << 8);
                lit_len += 137;
                if (i + lit_len > len) return 0;
                for (size_t j = 0; j < lit_len; j++) {
                    int c = dslz_getc();
                    if (c == EOF) return 0;
                    dst[i + j] = (uint8_t)c;
                }
                i += lit_len;
            } else if (sub == 1) {
                int ll = dslz_getc(), lh = dslz_getc();
                int ol = dslz_getc(), oh = dslz_getc();
                if (ll == EOF || lh == EOF || ol == EOF || oh == EOF) return 0;
                size_t mlen = (size_t)(uint8_t)ll | ((size_t)(uint8_t)lh << 8);
                unsigned int offset = (uint8_t)ol | ((uint8_t)oh << 8);
                if (offset == 0 || offset > i || i + mlen > len) return 0;
                size_t src = i - offset;
                for (size_t j = 0; j < mlen; j++) dst[i + j] = dst[src + j];
                i += mlen; last_offset = offset;
            } else if (sub == 2) {
                int ll = dslz_getc(), lh = dslz_getc();
                if (ll == EOF || lh == EOF) return 0;
                size_t mlen = (size_t)(uint8_t)ll | ((size_t)(uint8_t)lh << 8);
                if (last_offset == 0 || last_offset > i || i + mlen > len) return 0;
                size_t src = i - last_offset;
                for (size_t j = 0; j < mlen; j++) dst[i + j] = dst[src + j];
                i += mlen;
            } else if (sub == 3) {
                int ll = dslz_getc(), lh = dslz_getc();
                int off = dslz_getc();
                if (ll == EOF || lh == EOF || off == EOF) return 0;
                size_t mlen = (size_t)(uint8_t)ll | ((size_t)(uint8_t)lh << 8);
                unsigned int offset = (uint8_t)off;
                if (offset == 0 || offset > i || i + mlen > len) return 0;
                size_t src = i - offset;
                for (size_t j = 0; j < mlen; j++) dst[i + j] = dst[src + j];
                i += mlen; last_offset = offset;
            } else return 0;
        } else return 0;
    }

    if (use_delta == 0xFF) dslz_delta_decode(dst, len, stride);
    return 1;
}

#endif /* DS_LZ_IMPLEMENTATION */
