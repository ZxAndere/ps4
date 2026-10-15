#include "engine/framegen.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace fg {
Vector Flow::at(float x, float y) const {
    float gx = std::max(0.f,std::min(float(columns-1),x*columns/image_size.width-.5f));
    float gy = std::max(0.f,std::min(float(rows-1),y*rows/image_size.height-.5f));
    int x0=int(gx),y0=int(gy),x1=std::min(x0+1,columns-1),y1=std::min(y0+1,rows-1);
    Vector out;
    const int ix[4]={x0,x1,x0,x1}, iy[4]={y0,y0,y1,y1};
    float fx=gx-x0,fy=gy-y0,w[4]={(1-fx)*(1-fy),fx*(1-fy),(1-fx)*fy,fx*fy};
    for(int i=0;i<4;++i) { auto v=vectors[std::size_t(iy[i])*columns+ix[i]];
        out.x+=w[i]*v.x;out.y+=w[i]*v.y;out.cost+=w[i]*v.cost;
    }
    return out;
}
static std::vector<float> gray(const Image& im, Size small) {
    std::vector<float> result(checked_pixels(small));
    for(int y=0;y<small.height;++y)for(int x=0;x<small.width;++x) {
        auto p=sample(im,(x+.5f)*im.size().width/small.width-.5f,(y+.5f)*im.size().height/small.height-.5f);
        result[std::size_t(y)*small.width+x]=(77*p.r+150*p.g+29*p.b)/256.f;
    }
    return result;
}
static Vector global_shift(const std::vector<float>& a,const std::vector<float>& b,Size s) {
    // Sparse coarse translation seeds local search for camera pans larger than a block radius.
    Vector best;float best_score=std::numeric_limits<float>::infinity();
    int step=std::max(2,s.width/32);
    for(int dy=-s.height/4;dy<=s.height/4;++dy)for(int dx=-s.width/4;dx<=s.width/4;++dx) {
        float error=0;int count=0;
        for(int y=step;y<s.height-step;y+=step)for(int x=step;x<s.width-step;x+=step) {
            if(x+dx<0||x+dx>=s.width||y+dy<0||y+dy>=s.height)continue;
            error+=float(std::fabs(a[std::size_t(y)*s.width+x]-b[std::size_t(y+dy)*s.width+x+dx]));++count;
        }
        if(count==0)continue;
        float score=error/count+.02f*(std::abs(dx)+std::abs(dy));
        if(score<best_score){best_score=score;best.x=float(dx);best.y=float(dy);}
    }
    return best;
}
static Flow match(const std::vector<float>& a,const std::vector<float>& b,Size s,Size full,int radius,Vector seed,bool efficient) {
    constexpr int block=8;
    Flow flow; flow.image_size=full;flow.columns=(s.width+block-1)/block;flow.rows=(s.height+block-1)/block;
    flow.vectors.resize(std::size_t(flow.columns)*flow.rows);
    for(int by=0;by<flow.rows;++by)for(int bx=0;bx<flow.columns;++bx) {
        int x0=bx*block,y0=by*block,xend=std::min(x0+block,s.width),yend=std::min(y0+block,s.height);
        float best=std::numeric_limits<float>::infinity();int bestx=0,besty=0;
        // Include zero as a fallback when the global seed goes out of bounds at an image edge.
        auto evaluate=[&](int dx,int dy) {
            if(x0+dx<0||y0+dy<0||xend+dx>s.width||yend+dy>s.height)return;
            float sad=0;int count=0;
            int samples=((xend-x0+1)/2)*((yend-y0+1)/2);
            float penalty=.015f*(std::abs(dx)+std::abs(dy));
            // A conservative margin exceeds SAD accumulation roundoff for <=16 samples.
            float limit=(best-penalty)*samples+1.f/64;
            for(int y=y0;y<yend;y+=2) {
                for(int x=x0;x<xend;x+=2) {
                    sad+=float(std::fabs(a[std::size_t(y)*s.width+x]-b[std::size_t(y+dy)*s.width+x+dx]));++count;
                }
                // Remaining SAD terms are nonnegative. One branch per row; no per-sample division.
                if(efficient&&sad>limit)return;
            }
            // Weak motion penalty breaks ties in flat areas without manufacturing a vector.
            float score=sad/count+.015f*(std::abs(dx)+std::abs(dy));
            if(score<best) {best=score;bestx=dx;besty=dy;}
        };
        evaluate(0,0);
        for(int dy=int(seed.y)-radius;dy<=int(seed.y)+radius;++dy)
            for(int dx=int(seed.x)-radius;dx<=int(seed.x)+radius;++dx)evaluate(dx,dy);
        flow.vectors[std::size_t(by)*flow.columns+bx]={float(bestx)*full.width/s.width,float(besty)*full.height/s.height,best};
    }
    return flow;
}
static Motion estimate_impl(const Image& a,const Image& b,Profile profile,bool efficient) {
    if(a.size()!=b.size())throw std::invalid_argument("Motion image size mismatch");
    if(profile!=Profile::Performance&&profile!=Profile::Quality&&profile!=Profile::Balanced)throw std::invalid_argument("Unknown profile");
    Size full=a.size();int max_width=profile==Profile::Quality?320:profile==Profile::Balanced?240:160;
    Size small={std::min(full.width,max_width),std::max(1,int(std::lround(double(full.height)*std::min(full.width,max_width)/full.width)))};
    auto ga=gray(a,small),gb=gray(b,small);
    Motion result;
    for(std::size_t i=0;i<ga.size();++i)result.mean_difference+=std::fabs(ga[i]-gb[i]);
    result.mean_difference/=ga.size();result.scene_cut=result.mean_difference>80;
    int radius=profile==Profile::Quality?12:profile==Profile::Balanced?8:6;
    auto seed=global_shift(ga,gb,small);
    result.forward=match(ga,gb,small,full,radius,seed,efficient);
    seed.x=-seed.x;seed.y=-seed.y;
    result.backward=match(gb,ga,small,full,radius,seed,efficient);
    return result;
}
Motion estimate(const Image& a,const Image& b,Profile profile){return estimate_impl(a,b,profile,false);}
Motion estimate_efficient(const Image& a,const Image& b,Profile profile){return estimate_impl(a,b,profile,true);}
}
