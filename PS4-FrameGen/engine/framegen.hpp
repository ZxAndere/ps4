#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <chrono>
#include <memory>

namespace fg {
struct Size {
    int width = 0, height = 0;
    bool operator==(Size b) const { return width == b.width && height == b.height; }
    bool operator!=(Size b) const { return !(*this == b); }
};
struct Pixel { uint8_t r = 0, g = 0, b = 0, a = 255; };
static_assert(sizeof(Pixel) == 4, "RGBA8 required");
constexpr std::size_t max_pixels = 4096u * 2160u;
std::size_t checked_pixels(Size size);
class Image {
    Size size_;
    std::vector<Pixel> data_;
public:
    explicit Image(Size size);
    Image(const Image& other);
    Image& operator=(const Image& other);
    Image(Image&&) noexcept = default;
    Image& operator=(Image&&) noexcept = default;
    void reset(Size size);
    Size size() const { return size_; }
    Pixel* data() { return data_.data(); }
    const Pixel* data() const { return data_.data(); }
    std::size_t bytes() const { return data_.size() * sizeof(Pixel); }
    std::size_t capacity_bytes() const { return data_.capacity() * sizeof(Pixel); }
    Pixel& at(int x, int y);
    const Pixel& at(int x, int y) const;
};
Image read_ppm(const std::string& path);
void write_ppm(const Image& image, const std::string& path);
Pixel sample(const Image& image, float x, float y);
double mse(const Image& a, const Image& b);
double psnr(const Image& a, const Image& b);
Image test_scene(Size size, float offset); // Analytic texture, not captured game footage.

enum class Filter { Bilinear, Area, Nearest, Bicubic };
enum class Resolution { Native = 0, P1080 = 1, P720 = 2, P540 = 3, P360 = 4 };
enum class Profile { Performance = 0, Quality = 1, Balanced = 2 };
Size resolve(Resolution mode, Size native);
Image scale(const Image& source, Size target, Filter filter = Filter::Area);
Pixel reconstruct(const Image& image,float x,float y,Filter filter);
Image sharpen(const Image& image,int strength); // 0..100, bounded cross unsharp filter

struct Vector { float x = 0, y = 0, cost = 0; };
struct Flow {
    int columns = 0, rows = 0;
    Size image_size;
    std::vector<Vector> vectors;
    Vector at(float x, float y) const;
};
struct Motion { Flow forward, backward; double mean_difference = 0; bool scene_cut = false; };
Motion estimate(const Image& a, const Image& b, Profile profile);
Motion estimate_efficient(const Image& a, const Image& b, Profile profile);
Image interpolate(const Image& a, const Image& b, const Motion& motion, float t);

struct Execution {
    bool kernels = true, simd = true, reuse = true, direct = true, efficient_motion = true;
    bool vector_flow = false, integer_scale = true, cached_scale = true;
#ifdef __PS4__
    int threads = 2;
#else
    int threads = 4;
#endif
};
struct Config {
    bool enabled = false;
    int multiplier = 2;
    Profile profile = Profile::Performance;
    Resolution output = Resolution::Native, processing = Resolution::Native;
    Filter filter = Filter::Area;
    double source_interval_ms = 1000.0 / 30.0;
    Execution execution;
    bool automatic = false, low_latency = false, scene_guard = false, safe_mode = false;
    int sharpness = 0, fps_limit = 0, warp_iterations = 2; // limiter: off, 30 or 60, only for owned presentation
    bool diagnostics = false;
};
void validate(const Config& config, Size input);
using Clock = std::chrono::steady_clock;
double elapsed_ms(Clock::time_point begin);
struct Metrics {
    Size input, processing, output;
    double preparation_ms = 0, motion_ms = 0, interpolation_ms = 0, scaling_ms = 0, total_ms = 0;
    double lookahead_ms = 0; // Estimate from supplied source interval; not end-to-end latency.
    double latency_estimate_ms = 0; // Lookahead + measured CPU time, excludes scanout and input devices.
    std::size_t image_bytes = 0; // Retained RGB(A) vector capacity, not process RSS or GPU memory.
    std::size_t working_set_upper_bound = 0;
    bool overloaded = false, scene_cut = false;
    int generated = 0;
    uint64_t image_allocations = 0, copied_image_bytes = 0;
    int skipped_reason = 0; // 0 none, 1 cut, 2 unreliable match, 3 extreme motion, 4 late pair bypass
    uint64_t dropped = 0;
};
struct Batch { std::vector<Image> frames; std::vector<float> times; Metrics metrics; };
// Returns the interval [A, B), including A and M-1 new images. Caller owns final B.
Batch process(const Image& a, const Image& b, const Config& config);
Batch process_reference(const Image& a, const Image& b, const Config& config);
struct ImageActivity { uint64_t allocations=0, copied_bytes=0; };
ImageActivity image_activity();
void record_image_copy(std::size_t bytes);
// Owns reusable storage and a persistent bounded worker pool. Not concurrently reentrant.
// The borrowed result is valid until the next run; input images must not alias that result.
class Processor {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    explicit Processor(const Execution& execution = Execution{});
    ~Processor();
    Processor(const Processor&)=delete;
    Processor& operator=(const Processor&)=delete;
    Batch& run(const Image& a,const Image& b,const Config& config);
    Batch take_result();
};

class PresentationMonitor {
    uint64_t originals_ = 0, presented_ = 0;
    double first_original_ = 0, last_original_ = 0, first_presented_ = 0, last_presented_ = 0;
    std::vector<double> frame_ms_;
public:
    void original(double seconds);
    void confirmed_present(double seconds); // Call only after a confirmed display flip.
    double original_fps() const;
    double presented_fps() const;
    uint64_t presented_count() const { return presented_; }
    struct FrameTimes { double mean=0,maximum=0,p50=0,p95=0,p99=0; std::size_t samples=0; };
    FrameTimes frame_times() const; // last 240 confirmed flip intervals; empty means unavailable
};
}
