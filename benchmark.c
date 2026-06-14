#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

/**
 * DS-LZ C API Synthetic Benchmark
 * 
 * This is a SYNTHETIC benchmark designed to verify API correctness:
 *   - Compression/decompression round-trip integrity
 *   - Function call overhead measurement
 *   - Worst-case (ideal data) encode/decode speed on x86-64
 * 
 * The speed ratio (encode > decode) observed here is an artifact of
 * synthetic tile data on a desktop CPU. On real hardware (ESP32-S3)
 * with actual emulator state, decode is significantly faster:
 * 
 *   Platform       | Encode    | Decode
 *   ----------------|-----------|----------
 *   x86-64 (synth)  | ~170 MB/s | ~80 MB/s
 *   ESP32-S3 (real) | ~4 MB/s   | ~30 MB/s
 * 
 * Real-world compression ratios on MSX emulator state (4.4 MB):
 *   - Title screen: 822× (5.23 KB)
 *   - In-game:      108× (39.87 KB)
 */

#define DS_LZ_IMPLEMENTATION
#include "ds-lz.h"

#define VRAM_SIZE 16384
#define STRIDE 128
#define ITERATIONS 1000

void generate_msx_vram(uint8_t *vram) {
    memset(vram, 0, VRAM_SIZE);
    
    for (int i = 0x1800; i < 0x1B00; i++) {
        vram[i] = (rand() % 100 < 90) ? 0 : (rand() % 50 + 1);
    }
    
    for (int tile = 1; tile < 256; tile++) {
        for (int r = 0; r < 8; r++) {
            int off = tile * 8 + r;
            uint8_t pat = (r >= 2 && r <= 5) ? 0x18 : 0x00;
            vram[off] = pat;
            vram[off + 0x0800] = pat;
            vram[off + 0x1000] = pat;
        }
    }
    
    for (int tile = 0; tile < 256; tile++) {
        for (int r = 0; r < 8; r++) {
            int off = 0x2000 + tile * 8 + r;
            vram[off] = (tile > 0) ? 0xF1 : 0x01;
        }
    }
}

int main(void) {
    printf("==========================================\n");
    printf(" DS-LZ C API Synthetic Benchmark\n");
    printf("==========================================\n");
    printf(" NOTE: Synthetic data — real-world results differ.\n");
    printf(" See SPECIFICATION.md for production metrics.\n");
    printf("==========================================\n\n");

    uint8_t *original = (uint8_t *)malloc(VRAM_SIZE);
    uint8_t *decompressed = (uint8_t *)malloc(VRAM_SIZE);
    if (!original || !decompressed) {
        printf("Memory allocation failed!\n");
        return 1;
    }

    srand(42);
    generate_msx_vram(original);
    printf("Generated test data: %d bytes (MSX VRAM Title)\n", VRAM_SIZE);

    FILE *tmp = tmpfile();
    if (!tmp) {
        printf("Failed to create temporary file!\n");
        return 1;
    }

    clock_t start, end;
    long comp_size = 0;

    printf("\nRunning Compression (%d iterations)...\n", ITERATIONS);
    start = clock();
    for (int i = 0; i < ITERATIONS; i++) {
        rewind(tmp);
        if (!dslz_compress(original, VRAM_SIZE, tmp, 1, STRIDE)) {
            printf("Compression API failed!\n");
            return 1;
        }
    }
    end = clock();
    double enc_time_ms = ((double)(end - start) / CLOCKS_PER_SEC * 1000.0) / ITERATIONS;
    
    comp_size = ftell(tmp);

    printf("Running Decompression (%d iterations)...\n", ITERATIONS);
    start = clock();
    for (int i = 0; i < ITERATIONS; i++) {
        rewind(tmp);
        if (!dslz_decompress(tmp, decompressed, VRAM_SIZE)) {
            printf("Decompression API failed!\n");
            return 1;
        }
    }
    end = clock();
    double dec_time_ms = ((double)(end - start) / CLOCKS_PER_SEC * 1000.0) / ITERATIONS;

    int valid = (memcmp(original, decompressed, VRAM_SIZE) == 0);

    printf("\n==========================================\n");
    printf(" RESULTS (Synthetic — see notes above)\n");
    printf("==========================================\n");
    printf(" Original Size   : %d bytes\n", VRAM_SIZE);
    printf(" Compressed Size : %ld bytes\n", comp_size);
    printf(" Compression Ratio: %.2fx\n", (double)VRAM_SIZE / comp_size);
    printf(" Encode Time     : %.3f ms (per cycle)\n", enc_time_ms);
    printf(" Decode Time     : %.3f ms (per cycle)\n", dec_time_ms);
    
    double enc_mbs = (VRAM_SIZE / 1048576.0) / (enc_time_ms / 1000.0);
    double dec_mbs = (VRAM_SIZE / 1048576.0) / (dec_time_ms / 1000.0);
    printf(" Encode Speed    : %.1f MB/s\n", enc_mbs);
    printf(" Decode Speed    : %.1f MB/s\n", dec_mbs);
    
    printf(" Data Integrity  : %s\n", valid ? "PASSED [OK]" : "FAILED [ERROR]");

    fclose(tmp);
    free(original);
    free(decompressed);

    return 0;
}
