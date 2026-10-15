#pragma once
#include "engine/framegen.hpp"
#include <deque>
#include <fstream>
namespace fg {
// Synchronous, bounded work: never queues pairs. The caller owns the one outstanding batch.
class AdaptiveController {
    int pressure_=0,stable_=0;
    double ewma_=0;
public:
    Config select(const Config& requested,Size input) const;
    void observe(double cpu_ms,double budget_ms,bool automatic);
    int pressure() const{return pressure_;}
    double smoothed_ms() const{return ewma_;}
    void reset(){pressure_=stable_=0;ewma_=0;}
};
class Runtime {
    Processor processor_;
    AdaptiveController adaptive_;
    Config effective_;
public:
    explicit Runtime(Execution e={}):processor_(e){}
    Batch& run(const Image& a,const Image& b,const Config& requested);
    const Config& effective()const{return effective_;}
    const AdaptiveController& adaptive()const{return adaptive_;}
};
// A pair captured at B is played over [B, B+interval). No queue, no synthetic flip counts.
// Normal mode shifts the interval to completion; low-latency mode retains capture deadlines.
class PresentationSchedule {
    double anchor_=0,interval_=0;
    int fps_=0;
public:
    PresentationSchedule(double captured_b,double completed,double interval_ms,bool low_latency,int fps_limit,double display_hz=60);
    double due(float temporal_position) const;
    bool expired(bool generated,float temporal_position,int multiplier,double now) const;
    static double limited_due(double due,double last_confirmed,int fps_limit);
};
enum class Compatibility { Untested, Compatible, Limited, Incompatible, Failed };
const char* compatibility_name(Compatibility state);
struct GameProfile {
    std::string game_id;
    Config config;
    Compatibility state=Compatibility::Untested;
    std::string evidence;
    bool allow_automatic_enable=false;
};
void save_profile(const GameProfile&,const std::string& directory);
GameProfile recover_profile(const std::string& directory,const std::string& game_id);
GameProfile previous_profile(const std::string& directory,const std::string& game_id);
GameProfile load_profile(const std::string& directory,const std::string& game_id);
// Marker is written before experimental execution. An unclean previous run blocks experiments.
class RecoverySession {
    std::string directory_;
    bool started_=false;
public:
    explicit RecoverySession(std::string directory):directory_(std::move(directory)){}
    bool begin(Config&); // false = previous run did not finish cleanly, safe defaults selected
    void fault(const std::string& message);
    void finish(); // leaves history and writes a clean marker; never deletes user files
};
class Diagnostics {
    std::ofstream output_;
public:
    explicit Diagnostics(const std::string& path);
    void record(const Metrics&,const PresentationMonitor&,uint64_t dropped,const Config&,int pressure);
};
struct Comparison { double psnr_db=0,mae=0,edge_mae=0; Image difference; };
Comparison compare(const Image& candidate,const Image& reference);
Image comparison_panel(const Image& original,const Image& generated,const Image* reference=nullptr);
}
