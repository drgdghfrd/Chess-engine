#include "../src/tablebase/Syzygy.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace uc;
int main(){const std::string d="v092_tb_fixture";std::filesystem::remove_all(d);std::filesystem::create_directories(d);std::ofstream(d+"/KQvK.rtbw").put('x');std::ofstream(d+"/KQvK.rtbz").put('x');Syzygy tb;tb.setPath(d);tb.setProbeLimit(7);Position p;assert(p.setFEN("7k/8/8/8/8/8/6Q1/7K w - - 0 1"));auto s=tb.status(p);assert(s.pieces==3&&s.eligible&&s.pathExists&&s.wdlFiles&&s.dtzFiles);auto r=tb.probe(p);assert(!r.wdlAvailable&&!r.dtzAvailable&&r.wdl==SyzygyWDL::Unknown&&r.dtz==SyzygyDTZ::Unknown);std::filesystem::remove_all(d);std::cout<<"v0.92 Syzygy probe API tests: PASS\nwdl_files="<<(s.wdlFiles?1:0)<<" dtz_files="<<(s.dtzFiles?1:0)<<" probe_backend="<<(s.probingAvailable?1:0)<<"\n";}
