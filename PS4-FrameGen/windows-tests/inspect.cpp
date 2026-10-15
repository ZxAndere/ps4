#include "engine/runtime.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <cmath>
int main(int argc,char** argv) {
 try {
    if(argc!=5){std::cerr<<"framegen_inspect ORIGINAL.ppm GENERATED.ppm REFERENCE.ppm NEW_OUTPUT_DIRECTORY\n";return 2;}
    auto a=fg::read_ppm(argv[1]),b=fg::read_ppm(argv[2]),truth=fg::read_ppm(argv[3]);
    auto result=fg::compare(b,truth);if(!std::filesystem::create_directory(argv[4]))throw std::runtime_error("Use a new output directory");
    std::string dir=argv[4];fg::write_ppm(result.difference,dir+"/difference-x4.ppm");
    fg::write_ppm(fg::comparison_panel(a,b,&truth),dir+"/comparison.ppm");
    std::ofstream out(dir+"/quality.json");out<<std::setprecision(10)<<"{\"psnr_db\":";
    if(std::isfinite(result.psnr_db))out<<result.psnr_db;else out<<"null";
    out<<",\"mae\":"<<result.mae<<",\"horizontal_edge_mae\":"<<result.edge_mae<<",\"difference_gain\":4}\n";
    if(!out)throw std::runtime_error("Cannot write comparator report");return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
