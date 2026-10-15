#include "overlay/overlay.hpp"
#include <cstdio>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace fg {
static std::string path(const std::string& directory,const std::string& game) {
    if(game.empty()||game.size()>32||game.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")!=std::string::npos)
        throw std::invalid_argument("Unsafe game ID");
    return directory+"/"+game+".cfg";
}
void atomic_replace(const std::string& tmp,const std::string& target) {
#ifdef _WIN32
    if(!MoveFileExA(tmp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
#else
    if(std::rename(tmp.c_str(),target.c_str())!=0)
#endif
        throw std::runtime_error("Atomic preference replacement failed; previous config retained");
}
void save_settings(const Config& c,const std::string& directory,const std::string& game) {
    validate(c,{1920,1080});
    auto target=path(directory,game);auto tmp=target+".tmp";
    std::ofstream f(tmp,std::ios::trunc);
    if(!f)throw std::runtime_error("Cannot write preferences");
    // Never persist automatic enablement of this experimental engine.
    f<<"FRAMEGEN1 "<<c.multiplier<<' '<<int(c.profile)<<' '<<int(c.output)<<' '<<int(c.processing)<<' '<<int(c.filter)<<'\n';
    f.close();if(!f)throw std::runtime_error("Preference write failed");
    atomic_replace(tmp,target);
}
Config load_settings(const std::string& directory,const std::string& game) {
    std::ifstream f(path(directory,game));Config c;
    if(!f)return c;
    std::string magic;int profile=-1,out=-1,proc=-1,filter=-1;
    if(!(f>>magic>>c.multiplier>>profile>>out>>proc>>filter)||magic!="FRAMEGEN1"||profile<0||profile>2||out<0||out>2||proc<0||proc>4||filter<0||filter>3)
        throw std::runtime_error("Invalid preferences; use defaults");
    std::string trailing;if(f>>trailing)throw std::runtime_error("Trailing preference data");
    c.profile=Profile(profile);c.output=Resolution(out);c.processing=Resolution(proc);c.filter=Filter(filter);
    validate(c,{1920,1080});c.enabled=false;return c;
}
}
