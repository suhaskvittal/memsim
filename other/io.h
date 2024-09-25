/*
 *  author: Suhas Vittal
 *  date:   24 September 2024
 * */

#ifndef IO_h
#define IO_h

#include <stdint.h>
#include <zlib.h>

struct TraceData {
    uint64_t    instno;
    uint64_t    ld_addr;
    
    uint64_t    wb_addr;
    uint64_t    wb_line[8];

    uint8_t     ld_type;
    uint8_t     is_wb;
};

inline void
read_from_init(gzFile& fin, TraceData& d) {
    gzread(fin, &d.wb_addr, 8);
    gzread(fin, d.wb_line, 64);
}

inline void
read_from_trace(gzFile& fin, TraceData& d) {
    gzread(fin, &d.instno, 8);
    gzread(fin, &d.ld_type, 1);
    gzread(fin, &d.ld_addr, 8);
    gzread(fin, &d.is_wb, 1);
    if (d.is_wb) {
        gzread(fin, &d.wb_addr, 8);
        gzread(fin, d.wb_line, 64);
    }
}

#endif  // IO_h
