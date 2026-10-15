#include "engine/framegen.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fg {
Image interpolate(const Image& a,const Image& b,const Motion& m,float t) {
    if(a.size()!=b.size()||m.forward.image_size!=a.size()||m.backward.image_size!=a.size())
        throw std::invalid_argument("Interpolation size mismatch");
    for(const Flow* flow:{&m.forward,&m.backward})
        if(flow->columns<=0||flow->rows<=0||std::size_t(flow->columns)*flow->rows!=flow->vectors.size())
            throw std::invalid_argument("Invalid motion grid");
    for(const Flow* flow:{&m.forward,&m.backward})for(const Vector& v:flow->vectors)
        if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.cost))
            throw std::invalid_argument("Non-finite motion vector");
    if(!std::isfinite(t)||t<0||t>1)throw std::invalid_argument("Interpolation time must be [0,1]");
    if(t==0)return a;
    if(t==1)return b;
    if(m.scene_cut)throw std::runtime_error("Scene cut: interpolation suppressed");
    Image out(a.size());
    for(int y=0;y<a.size().height;++y)for(int x=0;x<a.size().width;++x) {
        // Two fixed-point iterations approximate inverse mapping into each source.
        auto f=m.forward.at(float(x),float(y));auto r=m.backward.at(float(x),float(y));
        for(int it=0;it<2;++it) {
            f=m.forward.at(x-t*f.x,y-t*f.y);
            r=m.backward.at(x-(1-t)*r.x,y-(1-t)*r.y);
        }
        float ax=x-t*f.x,ay=y-t*f.y,bx=x-(1-t)*r.x,by=y-(1-t)*r.y;
        auto p=sample(a,ax,ay),q=sample(b,bx,by);
        auto opposite=m.backward.at(ax+f.x,ay+f.y);
        float consistency=std::hypot(f.x+opposite.x,f.y+opposite.y);
        // Inconsistent motion is marked low-confidence; temporal-nearest warp limits ghosting.
        float wa=1-t,wb=t;
        if(consistency>std::max(3.f,a.size().width/80.f)) {wa=t<.5f?1.f:0.f;wb=1-wa;}
        auto mix=[&](int v,int w){return uint8_t(std::max(0.f,std::min(255.f,v*wa+w*wb+.5f)));};
        out.at(x,y)={mix(p.r,q.r),mix(p.g,q.g),mix(p.b,q.b),255};
    }
    return out;
}
}
