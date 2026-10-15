#pragma once
#include "engine/framegen.hpp"
namespace fg {
enum Button : uint32_t { L3=1u<<0,R3=1u<<1,Up=1u<<2,Down=1u<<3,Left=1u<<4,Right=1u<<5,Cross=1u<<6,Circle=1u<<7,L1=1u<<8,R1=1u<<9,Emergency=1u<<10 };
class Menu {
    uint32_t last_=0;
public:
    bool visible=false;
    int selected=0,section=0;
    bool comparison=false,restore_previous=false,profile_restore_available=true;
    std::string game_id="FGEN00003",compatibility="UNTESTED";
    uint64_t dropped=0;
    int pressure=0;
    int rows() const;

    Config config;
    void update(uint32_t buttons);
};
void atomic_replace(const std::string& temporary,const std::string& target);
void save_settings(const Config& config,const std::string& directory,const std::string& game);
Config load_settings(const std::string& directory,const std::string& game);
void draw_menu(Image& image,const Menu& menu,const Metrics& metrics,const PresentationMonitor& monitor,const std::string& status);
void draw_text(Image& image,int x,int y,const std::string& text,int scale=2);
}
