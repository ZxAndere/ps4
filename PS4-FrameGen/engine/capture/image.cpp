#include "engine/framegen.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <atomic>

namespace fg {
std::size_t checked_pixels(Size s) {
    if (s.width <= 0 || s.height <= 0 || s.width > 4096 || s.height > 2160 ||
        std::size_t(s.width) * std::size_t(s.height) > max_pixels)
        throw std::invalid_argument("Invalid image size: limit 4096x2160 RGBA8");
    return std::size_t(s.width) * std::size_t(s.height);
}
static std::atomic<uint64_t> allocations{0},copied_bytes{0};
ImageActivity image_activity() {return {allocations.load(),copied_bytes.load()};}
void record_image_copy(std::size_t bytes) {copied_bytes+=bytes;}
Image::Image(Size s) : size_(s), data_(checked_pixels(s)) {++allocations;}
Image::Image(const Image& other):size_(other.size_),data_(other.data_){++allocations;copied_bytes+=bytes();}
Image& Image::operator=(const Image& other) {
    if(this!=&other){if(data_.capacity()<other.data_.size())++allocations;data_=other.data_;size_=other.size_;copied_bytes+=bytes();}
    return *this;
}
void Image::reset(Size size) {
    auto n=checked_pixels(size);
    if(data_.capacity()<n)++allocations;
    data_.resize(n);size_=size;
}
Pixel& Image::at(int x, int y) {
    if (x < 0 || y < 0 || x >= size_.width || y >= size_.height) throw std::out_of_range("Pixel index");
    return data_[std::size_t(y) * size_.width + x];
}
const Pixel& Image::at(int x, int y) const {
    if (x < 0 || y < 0 || x >= size_.width || y >= size_.height) throw std::out_of_range("Pixel index");
    return data_[std::size_t(y) * size_.width + x];
}
static std::string token(std::istream& f) {
    std::string s;
    for (;;) {
        f >> std::ws;
        if (f.peek() != '#') break;
        f.ignore(4096, '\n');
        if (!f) throw std::runtime_error("Invalid PPM comment");
    }
    if (!(f >> s) || s.size() > 32) throw std::runtime_error("Invalid PPM header");
    return s;
}
static int positive_int(const std::string& s) {
    std::size_t end = 0;
    int n = std::stoi(s, &end);
    if (end != s.size() || n <= 0) throw std::runtime_error("Invalid PPM integer");
    return n;
}
Image read_ppm(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f || token(f) != "P6") throw std::runtime_error("Expected binary RGB PPM: " + path);
    int w = positive_int(token(f)), h = positive_int(token(f));
    if (positive_int(token(f)) != 255) throw std::runtime_error("PPM supports 8-bit RGB only");
    char separator = 0;
    f.get(separator);
    if (separator != ' ' && separator != '\t' && separator != '\n' && separator != '\r')
        throw std::runtime_error("Missing PPM raster separator");
    if (separator == '\r' && f.peek() == '\n') f.get();
    Image out({w, h});
    std::vector<char> row(std::size_t(w) * 3);
    for (int y = 0; y < h; ++y) {
        if (!f.read(row.data(), std::streamsize(row.size()))) throw std::runtime_error("Truncated PPM raster");
        for (int x = 0; x < w; ++x)
            out.at(x, y) = {uint8_t(row[3*x]), uint8_t(row[3*x+1]), uint8_t(row[3*x+2]), 255};
    }
    return out;
}
void write_ppm(const Image& image, const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot create " + path);
    auto s = image.size();
    f << "P6\n" << s.width << ' ' << s.height << "\n255\n";
    std::vector<char> row(std::size_t(s.width) * 3);
    for (int y = 0; y < s.height; ++y) {
        for (int x = 0; x < s.width; ++x) {
            auto p = image.at(x, y);
            row[3*x] = char(p.r); row[3*x+1] = char(p.g); row[3*x+2] = char(p.b);
        }
        f.write(row.data(), std::streamsize(row.size()));
    }
    f.close();
    if (!f) throw std::runtime_error("Failed writing " + path);
}
Pixel sample(const Image& image, float x, float y) {
    auto s = image.size();
    x = std::max(0.0f, std::min(x, float(s.width - 1)));
    y = std::max(0.0f, std::min(y, float(s.height - 1)));
    int x0 = int(x), y0 = int(y);
    int x1 = std::min(x0 + 1, s.width - 1), y1 = std::min(y0 + 1, s.height - 1);
    Pixel p[4] = {image.at(x0,y0), image.at(x1,y0), image.at(x0,y1), image.at(x1,y1)};
    float fx = x-x0, fy = y-y0;
    float w[4] = {(1-fx)*(1-fy), fx*(1-fy), (1-fx)*fy, fx*fy};
    auto channel = [&](int c) {
        float result = 0;
        for (int i=0;i<4;++i) {
            auto v = c == 0 ? p[i].r : c == 1 ? p[i].g : p[i].b;
            result += w[i]*v;
        }
        return uint8_t(std::max(0.0f, std::min(255.0f, result + 0.5f)));
    };
    return {channel(0),channel(1),channel(2),255};
}
double mse(const Image& a, const Image& b) {
    if (a.size() != b.size()) throw std::invalid_argument("MSE size mismatch");
    double total = 0;
    for (std::size_t i=0;i<checked_pixels(a.size());++i) {
        double dr = int(a.data()[i].r)-int(b.data()[i].r);
        double dg = int(a.data()[i].g)-int(b.data()[i].g);
        double db = int(a.data()[i].b)-int(b.data()[i].b);
        total += dr*dr+dg*dg+db*db;
    }
    return total / (checked_pixels(a.size()) * 3.0);
}
double psnr(const Image& a, const Image& b) {
    double error = mse(a,b);
    return error == 0 ? std::numeric_limits<double>::infinity() : 10*std::log10(255.0*255.0/error);
}
Image test_scene(Size size, float offset) {
    Image out(size);
    // The controlled source is separable: evaluate trigonometry once per axis, not per pixel.
    // Keep the SDK's actual sin(float) result type to preserve the v0.1 raster on each toolchain.
    using Trig=decltype(std::sin(0.f));
    const float ax[3]={.055f,.087f,.031f},ay[3]={.071f,.041f,.099f},phase[3]={0,2,4};
    std::vector<Trig> xs(std::size_t(size.width)*3),ys(std::size_t(size.height)*3);
    for(int x=0;x<size.width;++x)for(int k=0;k<3;++k)xs[std::size_t(x)*3+k]=std::sin((float(x)-offset)*ax[k]+phase[k]);
    for(int y=0;y<size.height;++y)for(int k=0;k<3;++k)ys[std::size_t(y)*3+k]=std::cos((float(y)-offset*.375f)*ay[k]+phase[k]);
    for (int y=0;y<size.height;++y) for (int x=0;x<size.width;++x) {
        auto wave = [&](int channel) {
            float value=float(127.0f+65*xs[std::size_t(x)*3+channel]+55*ys[std::size_t(y)*3+channel]);
            return uint8_t(std::max(0.0f,std::min(255.0f,value)));
        };
        out.data()[std::size_t(y)*size.width+x] = {wave(0),wave(1),wave(2),255};
    }
    return out;
}
}
