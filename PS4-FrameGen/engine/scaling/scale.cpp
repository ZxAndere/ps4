#include "engine/framegen.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fg {
Size resolve(Resolution mode, Size native) {
    checked_pixels(native);
    switch (mode) {
    case Resolution::Native: return native;
    case Resolution::P1080: return {1920,1080};
    case Resolution::P720: return {1280,720};
    case Resolution::P540: return {960,540};
    case Resolution::P360: return {640,360};
    }
    throw std::invalid_argument("Unknown resolution");
}
static float cubic(float v) {
    v=std::fabs(v);
    if(v<=1)return (1.5f*v-2.5f)*v*v+1;
    if(v<2)return ((-.5f*v+2.5f)*v-4)*v+2;
    return 0;
}
Pixel reconstruct(const Image& im,float x,float y,Filter filter) {
    auto size=im.size();
    x=std::max(0.f,std::min(float(size.width-1),x));y=std::max(0.f,std::min(float(size.height-1),y));
    if(filter==Filter::Nearest)return im.data()[std::size_t(int(y+.5f))*size.width+int(x+.5f)];
    if(filter!=Filter::Bicubic)return sample(im,x,y);
    int ix=int(x),iy=int(y);float r=0,g=0,b=0,w=0;
    for(int j=-1;j<=2;++j)for(int i=-1;i<=2;++i) {
        float weight=cubic(x-ix-i)*cubic(y-iy-j);
        int px=std::max(0,std::min(size.width-1,ix+i)),py=std::max(0,std::min(size.height-1,iy+j));
        auto p=im.data()[std::size_t(py)*size.width+px];r+=weight*p.r;g+=weight*p.g;b+=weight*p.b;w+=weight;
    }
    auto bound=[](float v){return uint8_t(std::max(0.f,std::min(255.f,v+.5f)));};
    return {bound(r/w),bound(g/w),bound(b/w),255};
}
Image sharpen(const Image& im,int strength) {
    if(strength<0||strength>100)throw std::invalid_argument("Sharpness must be 0..100");
    Image out(im.size());auto s=im.size();
    for(int y=0;y<s.height;++y)for(int x=0;x<s.width;++x) {
        auto center=im.data()[std::size_t(y)*s.width+x];
        Pixel neighbors[4]={im.data()[std::size_t(y)*s.width+std::max(0,x-1)],im.data()[std::size_t(y)*s.width+std::min(s.width-1,x+1)],
            im.data()[std::size_t(std::max(0,y-1))*s.width+x],im.data()[std::size_t(std::min(s.height-1,y+1))*s.width+x]};
        auto channel=[&](int c,int n){return uint8_t(std::max(0,std::min(255,c+(4*c-n)*strength/400)));};
        out.data()[std::size_t(y)*s.width+x]={channel(center.r,neighbors[0].r+neighbors[1].r+neighbors[2].r+neighbors[3].r),
            channel(center.g,neighbors[0].g+neighbors[1].g+neighbors[2].g+neighbors[3].g),
            channel(center.b,neighbors[0].b+neighbors[1].b+neighbors[2].b+neighbors[3].b),255};
    }
    return out;
}
Image scale(const Image& src, Size dst, Filter filter) {
    checked_pixels(dst);
    Size s = src.size();
    if (dst == s) return src;
    // Fixed-size output canvas, fit source within it and letterbox without stretching.
    double ratio = std::min(double(dst.width)/s.width, double(dst.height)/s.height);
    int w = std::max(1,int(std::lround(s.width*ratio)));
    int h = std::max(1,int(std::lround(s.height*ratio)));
    int ox = (dst.width-w)/2, oy = (dst.height-h)/2;
    Image out(dst);
    for (int y=0;y<h;++y) for (int x=0;x<w;++x) {
        Pixel p;
        if(filter==Filter::Nearest||filter==Filter::Bicubic) {
            p=reconstruct(src,float((x+.5)*s.width/w-.5),float((y+.5)*s.height/h-.5),filter);
        } else if (filter == Filter::Bilinear || ratio >= 1) {
            p = sample(src,float((x+.5)*s.width/w-.5),float((y+.5)*s.height/h-.5));
        } else {
            double x0=double(x)*s.width/w, x1=double(x+1)*s.width/w;
            double y0=double(y)*s.height/h, y1=double(y+1)*s.height/h;
            double r=0,g=0,b=0,weight=0;
            for (int sy=int(y0);sy<std::min(s.height,int(std::ceil(y1)));++sy)
                for (int sx=int(x0);sx<std::min(s.width,int(std::ceil(x1)));++sx) {
                    double t=(std::min(x1,double(sx+1))-std::max(x0,double(sx)))*
                             (std::min(y1,double(sy+1))-std::max(y0,double(sy)));
                    auto q=src.at(sx,sy); r+=t*q.r; g+=t*q.g; b+=t*q.b; weight+=t;
                }
            p = {uint8_t(std::lround(r/weight)),uint8_t(std::lround(g/weight)),uint8_t(std::lround(b/weight)),255};
        }
        out.at(x+ox,y+oy)=p;
    }
    return out;
}
}
