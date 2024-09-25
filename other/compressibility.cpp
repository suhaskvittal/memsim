/*
 *  author: Suhas Vittal
 *  date:   23 September 2024
 * */

#include "io.h"

#include <algorithm>
#include <iostream>
#include <unordered_map>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static uint64_t page_count = 0;

struct Page {
    uint64_t page_id;
    uint64_t data[512];
    uint64_t line_accesses[2][64];
    
    bool    is_dirty;

    /*
     * Stores the size of the page when
     * compressed at a given granularity.
     *
     * index 0 = 64B (cacheline),
     *       1 = 128B (2 lines)
     *       ...
     *       63 = 4KB (full page)
     * */
    int32_t compressed_mem_size[64];

    Page(void)
        :page_id(page_count++),
        data(),
        line_accesses(),
        is_dirty(false)
    {
        memset(data, 0, 4096);
        memset(line_accesses, 0, 128*sizeof(uint64_t));
    }

    void
    reset_stats() {
        memset(line_accesses, 0, 128*sizeof(uint64_t));
        is_dirty = false;
    }

    std::vector<uint64_t>
    tot_accesses(int gran, bool for_writes=false) {
        std::vector<uint64_t> tot;
        for (int i = 0; i < 64; i += gran) {
            uint64_t s = 0;
            for (int j = i; j < i+gran; j++) {
                s += line_accesses[for_writes][j];
            }
            tot.push_back(s);
        }
        return tot;
    }
};

struct MemorySystem {
    std::unordered_map<uint64_t, Page> pages;
    uint64_t mem_access_count=0;

    void access_page(uint64_t addr, uint64_t* line, bool is_write) {
        uint64_t p = addr >> 12;
        uint64_t off = (addr & 0x0fff) >> 6;
        Page& pg = pages[p];
        pg.line_accesses[is_write][off]++;
        if (is_write) {
            memmove(pg.data + 8*off, line, 64);
            pg.is_dirty = true;
        }
    }
};

/*
 * Returns size of the compressed data in bytes.
 * */
uint64_t gz_compress_page(Page&, size_t gran);
void write_data(FILE*, MemorySystem&);

constexpr uint64_t BURST_SIZE = 100'000'000;

int main(int argc, char* argv[]) {
    char* trace_file = argv[1];
    char* init_file = argv[2];
    char* output_file = argv[3];
    uint64_t max_ins = atoll(argv[4]);

    MemorySystem memsys;

    gzFile fin_trace = gzopen(trace_file, "r");
    gzFile fin_init = gzopen(init_file, "r");
    FILE* fout = fopen(output_file, "wb");

    TraceData d;

    printf("Reading from initialization file..\n");
    while (!gzeof(fin_init)) {
        read_from_init(fin_init, d);
        memsys.access_page(d.wb_addr, d.wb_line, true);
    }
    gzclose(fin_init);
    printf("\tDone! Allocated %llu pages.\n", page_count);

    uint64_t next_burst = BURST_SIZE;

    uint64_t inscount = 0;
    while (inscount < max_ins && !gzeof(fin_trace)) {
        read_from_trace(fin_trace, d);
        memsys.access_page(d.ld_addr, nullptr, false);
        if (d.is_wb) {
            memsys.access_page(d.wb_addr, d.wb_line, true);
        }
        if (inscount < next_burst && d.instno >= next_burst) {
            write_data(fout, memsys);
            next_burst += BURST_SIZE;
        }
        inscount = d.instno;
    }
    write_data(fout, memsys);
    gzclose(fin_trace);
    fclose(fout);
    return 0;
}

///////////////////////////////////////////////////
//              gzip_compress_page               //
///////////////////////////////////////////////////
uint64_t
gz_compress_page(Page& pg, size_t gran) {
    char pgout[4096];

    z_stream zs;
    zs.zalloc = Z_NULL;
    zs.zfree = Z_NULL;
    zs.opaque = Z_NULL;
    zs.next_out = (Bytef*)pgout;
    zs.avail_out = 4096;
    // Init `zs`.
    deflateInit(&zs, Z_BEST_COMPRESSION);
    for (size_t off = 0; off < 64; off += gran) {
        zs.next_in = (Bytef*)pg.data+off;
        zs.avail_in = 64*gran;

        deflate(&zs, Z_NO_FLUSH);
    }
    pg.compressed_mem_size[gran-1] = zs.total_out;
    deflateEnd(&zs);

    return pg.compressed_mem_size[gran-1];
}

///////////////////////////////////////////////////
//              gzip_compress_page               //
///////////////////////////////////////////////////

void
write_data(FILE* fout, MemorySystem& memsys) {
    printf("Writing page data to output file\n");
    // Write page data to `fout`.
    // First write number of pages.
    uint64_t n_pages = memsys.pages.size();
    fwrite(&n_pages, sizeof(n_pages), 1, fout);
    // Now write each page.
    for (auto& [a, pg] : memsys.pages) {
        // Page id
        fwrite(&pg.page_id, sizeof(pg.page_id), 1, fout);
        // Reads and writes to each line.
        for (int i = 0; i < 2; i++) {
            fwrite(pg.line_accesses+i, sizeof(uint64_t), 64, fout);
        }
        // Write compressed size of page if:
        //  (1) compressed at page level (4KB) granularity 
        //  (2) compressed at cacheline level (64B) granularity
        //  (3) compressed at a 128B granularity.
        //  (4) compressed at a 256B granularity.
        for (size_t gran : {64, 1, 2, 4}) {
            uint64_t c_mem_size;
            if (pg.is_dirty) {
                c_mem_size = gz_compress_page(pg, gran);
            } else {
                c_mem_size = pg.compressed_mem_size[gran-1];
            }
            fwrite(&c_mem_size, sizeof(uint64_t), 1, fout);
        }
        pg.reset_stats();
    }
    printf("\tDone! Wrote %llu pages.\n", page_count);
}
