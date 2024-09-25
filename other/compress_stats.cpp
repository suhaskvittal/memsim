/*
 *  author: Suhas Vittal
 *  date:   24 September 2024
 * */

#include <iostream>
#include <string>
#include <unordered_map>

#include <stdint.h>
#include <zlib.h>

#include "io.h"

struct AccessData {
    uint64_t reads  =0;
    uint64_t writes =0;
};

struct AddressData {
    uint64_t line;
    uint64_t dbl_line;
    uint64_t page;

    AddressData(uint64_t a)
        :line(a>>6),
        dbl_line(a>>7),
        page(a>>12)
    {}
};

std::unordered_map<uint64_t, AccessData> line_access_map;
std::unordered_map<uint64_t, AccessData> dbl_line_access_map;
std::unordered_map<uint64_t, AccessData> page_access_map;

int main(int argc, char* argv[]) {
    char* trace_file = argv[1];
    uint64_t inscount = atoll(argv[2]);
    gzFile fin_trace = gzopen(trace_file, "r");

    TraceData dat;
    uint64_t ii = 0;
    while (ii < inscount && !gzeof(fin_trace)) {
        read_from_trace(fin_trace, dat);
        
        AddressData r(dat.ld_addr);
        line_access_map[r.line].reads++;
        dbl_line_access_map[r.dbl_line].reads++;
        page_access_map[r.page].reads++;

        if (dat.is_wb) {
            AddressData w(dat.wb_addr);
            line_access_map[w.line].writes++;
            dbl_line_access_map[w.dbl_line].writes++;
            page_access_map[w.page].writes++;
        }
        ii = dat.instno;
    }

    std::string prefixes[] = { "Line", "Dbl. line", "Page" };
    int i = 0;
    for (auto m : { line_access_map, dbl_line_access_map, page_access_map }) {
        std::string prfx = prefixes[i];
        uint64_t rmin = std::numeric_limits<uint64_t>::max(),
                 wmin = std::numeric_limits<uint64_t>::max(),
                 rmax = 0,
                 wmax = 0;
        double rmean = 0.0, wmean = 0.0;
        for (const auto& [_a, x] : m) {
            rmin = std::min( rmin, x.reads );
            wmin = std::min( wmin, x.writes );
            rmax = std::max( rmax, x.reads );
            wmax = std::max( wmax, x.writes );
            rmean += (double)x.reads;
            wmean += (double)x.writes;
        }
        rmean /= static_cast<double>( m.size() );
        wmean /= static_cast<double>( m.size() );

        std::cout << prfx << " =============================\n"
            << "Reads = " << rmin << ", " << rmean << ", " << rmax << "\n"
            << "Writes = " << wmin << ", " << wmean << ", " << wmax << "\n"
            << "Total " << prfx << " = " << m.size() << "\n";
        i++;
    }

    gzclose(fin_trace);
    return 0;
}
