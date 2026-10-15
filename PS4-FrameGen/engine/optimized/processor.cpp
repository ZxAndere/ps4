#include "engine/framegen.hpp"
#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#if defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#define FG_SSE2 1
#endif

namespace fg {
namespace {
// One bounded pool per Processor. No thread creation in the steady-state frame path.
class Rows {
    std::vector<std::thread> workers_;
    std::mutex mutex_;
    std::condition_variable start_,done_;
    std::function<void(int,int)> job_;
    std::exception_ptr error_;
    int count_=1,rows_=0,remaining_=0;
    uint64_t generation_=0;
    bool stop_=false;
    void invoke(int id) {
        try {job_(rows_*id/count_,rows_*(id+1)/count_);}
        catch(...) {std::lock_guard<std::mutex> lock(mutex_);if(!error_)error_=std::current_exception();}
    }
public:
    explicit Rows(int count):count_(count) {
        if(count<1||count>8)throw std::invalid_argument("Worker count must be 1..8");
        try {
            for(int id=1;id<count;++id)workers_.emplace_back([this,id]{
                uint64_t observed=0;
                std::unique_lock<std::mutex> lock(mutex_);
                for(;;) {
                    start_.wait(lock,[&]{return stop_||generation_!=observed;});
                    if(stop_)return;
                    observed=generation_;lock.unlock();invoke(id);lock.lock();
                    if(--remaining_==0)done_.notify_one();
                }
            });
        } catch(...) {
            {std::lock_guard<std::mutex> lock(mutex_);stop_=true;}start_.notify_all();
            for(auto& t:workers_)t.join();
            throw;
        }
    }
    ~Rows() {
        {std::lock_guard<std::mutex> lock(mutex_);stop_=true;}start_.notify_all();
        for(auto& t:workers_)t.join();
    }
    void run(int rows,const std::function<void(int,int)>& job) {
        if(count_==1){job(0,rows);return;}
        {
            std::lock_guard<std::mutex> lock(mutex_);
            rows_=rows;job_=job;error_=nullptr;remaining_=count_-1;++generation_;
        }
        start_.notify_all();invoke(0);
        std::unique_lock<std::mutex> lock(mutex_);
        done_.wait(lock,[&]{return remaining_==0;});
        auto error=error_;job_={};lock.unlock();if(error)std::rethrow_exception(error);
    }
};
struct Sampling {const Pixel* p[4];float w[4];};
inline Sampling sampling(const Image& im,float x,float y) {
    Size s=im.size();x=std::max(0.f,std::min(x,float(s.width-1)));y=std::max(0.f,std::min(y,float(s.height-1)));
    int x0=int(x),y0=int(y),x1=std::min(x0+1,s.width-1),y1=std::min(y0+1,s.height-1);
    float fx=x-x0,fy=y-y0;const Pixel* d=im.data();
    return {{d+std::size_t(y0)*s.width+x0,d+std::size_t(y0)*s.width+x1,
             d+std::size_t(y1)*s.width+x0,d+std::size_t(y1)*s.width+x1},
            {(1-fx)*(1-fy),fx*(1-fy),(1-fx)*fy,fx*fy}};
}
inline uint8_t rounded(float value) {return uint8_t(std::max(0.f,std::min(255.f,value+.5f)));}
inline Pixel sample_scalar(const Sampling& s) {
    float r=0,g=0,b=0;
    for(int i=0;i<4;++i){r+=s.w[i]*s.p[i]->r;g+=s.w[i]*s.p[i]->g;b+=s.w[i]*s.p[i]->b;}
    return {rounded(r),rounded(g),rounded(b),255};
}
#ifdef FG_SSE2
inline __m128 unpack(const Pixel* p) {
    uint32_t value;std::memcpy(&value,p,sizeof(value));
    auto bytes=_mm_cvtsi32_si128(int(value));
    auto words=_mm_unpacklo_epi8(bytes,_mm_setzero_si128());
    return _mm_cvtepi32_ps(_mm_unpacklo_epi16(words,_mm_setzero_si128()));
}
inline Pixel pack(__m128 values) {
    auto integers=_mm_cvttps_epi32(_mm_add_ps(values,_mm_set1_ps(.5f)));
    auto words=_mm_packs_epi32(integers,_mm_setzero_si128());
    auto bytes=_mm_packus_epi16(words,_mm_setzero_si128());
    uint32_t value=uint32_t(_mm_cvtsi128_si32(bytes));
    return {uint8_t(value),uint8_t(value>>8),uint8_t(value>>16),255};
}
inline Pixel sample_simd(const Sampling& s) {
    auto value=_mm_setzero_ps();
    for(int i=0;i<4;++i)value=_mm_add_ps(value,_mm_mul_ps(unpack(s.p[i]),_mm_set1_ps(s.w[i])));
    return pack(value);
}
#endif
template<bool simd> inline Pixel sampled(const Image& im,float x,float y) {
    auto s=sampling(im,x,y);
#ifdef FG_SSE2
    if(simd)return sample_simd(s);
#endif
    return sample_scalar(s);
}
struct V2 {float x=0,y=0;};
inline V2 field(const Flow& f,float x,float y) {
    // Preserve the reference coordinate arithmetic, omitting its unused cost interpolation.
    float gx=std::max(0.f,std::min(float(f.columns-1),x*f.columns/f.image_size.width-.5f));
    float gy=std::max(0.f,std::min(float(f.rows-1),y*f.rows/f.image_size.height-.5f));
    int x0=int(gx),y0=int(gy),x1=std::min(x0+1,f.columns-1),y1=std::min(y0+1,f.rows-1);
    float fx=gx-x0,fy=gy-y0,w[4]={(1-fx)*(1-fy),fx*(1-fy),(1-fx)*fy,fx*fy};
    const Vector* p[4]={&f.vectors[std::size_t(y0)*f.columns+x0],&f.vectors[std::size_t(y0)*f.columns+x1],
        &f.vectors[std::size_t(y1)*f.columns+x0],&f.vectors[std::size_t(y1)*f.columns+x1]};
    V2 result;for(int i=0;i<4;++i){result.x+=w[i]*p[i]->x;result.y+=w[i]*p[i]->y;}return result;
}
// SIMD on the two flow components, preserving the scalar multiply/divide/add order.
// No reciprocal approximation, AVX, or FMA; the scalar path remains available for ablation.
inline V2 vector_field(const Flow& f,float x,float y) {
#ifdef FG_SSE2
    __m128 xy=_mm_setr_ps(x,y,0,0);
    xy=_mm_sub_ps(_mm_div_ps(_mm_mul_ps(xy,_mm_setr_ps(float(f.columns),float(f.rows),0,0)),
        _mm_setr_ps(float(f.image_size.width),float(f.image_size.height),1,1)),_mm_set1_ps(.5f));
    xy=_mm_max_ps(_mm_setzero_ps(),_mm_min_ps(xy,_mm_setr_ps(float(f.columns-1),float(f.rows-1),0,0)));
    alignas(16) float coordinates[4];_mm_store_ps(coordinates,xy);
    int x0=int(coordinates[0]),y0=int(coordinates[1]);
    int x1=std::min(x0+1,f.columns-1),y1=std::min(y0+1,f.rows-1);
    float fx=coordinates[0]-x0,fy=coordinates[1]-y0;
    float weights[4]={(1-fx)*(1-fy),fx*(1-fy),(1-fx)*fy,fx*fy};
    const Vector* values[4]={&f.vectors[std::size_t(y0)*f.columns+x0],&f.vectors[std::size_t(y0)*f.columns+x1],
        &f.vectors[std::size_t(y1)*f.columns+x0],&f.vectors[std::size_t(y1)*f.columns+x1]};
    __m128 sum=_mm_setzero_ps();
    for(int i=0;i<4;++i) {
        auto value=_mm_castsi128_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(values[i])));
        sum=_mm_add_ps(sum,_mm_mul_ps(value,_mm_set1_ps(weights[i])));
    }
    _mm_store_ps(coordinates,sum);return {coordinates[0],coordinates[1]};
#else
    return field(f,x,y);
#endif
}
template<bool fast> inline V2 lookup(const Flow& f,float x,float y) {
    if(fast)return vector_field(f,x,y);
    return field(f,x,y);
}
template<bool simd,bool vector_flow> void warp(const Image& a,const Image& b,const Motion& motion,float t,Image& out,Rows& rows,int iterations) {
    auto size=a.size();out.reset(size);
    rows.run(size.height,[&](int begin,int end){
        float threshold=std::max(3.f,size.width/80.f),threshold2=threshold*threshold;
        for(int y=begin;y<end;++y)for(int x=0;x<size.width;++x) {
            auto f=lookup<vector_flow>(motion.forward,float(x),float(y)),r=lookup<vector_flow>(motion.backward,float(x),float(y));
            for(int k=0;k<iterations;++k){f=lookup<vector_flow>(motion.forward,x-t*f.x,y-t*f.y);r=lookup<vector_flow>(motion.backward,x-(1-t)*r.x,y-(1-t)*r.y);}
            float ax=x-t*f.x,ay=y-t*f.y,bx=x-(1-t)*r.x,by=y-(1-t)*r.y;
            auto opposite=lookup<vector_flow>(motion.backward,ax+f.x,ay+f.y);
            float dx=f.x+opposite.x,dy=f.y+opposite.y,wa=1-t,wb=t;
            float squared=dx*dx+dy*dy;
            // Around the discontinuity preserve the reference hypot rounding exactly.
            bool inconsistent=std::fabs(squared-threshold2)<=threshold2*1e-5f?
                std::hypot(dx,dy)>threshold:squared>threshold2;
            if(inconsistent){wa=t<.5f?1.f:0.f;wb=1-wa;}
            auto p=sampled<simd>(a,ax,ay),q=sampled<simd>(b,bx,by);
            Pixel result;
#ifdef FG_SSE2
            if(simd)result=pack(_mm_add_ps(_mm_mul_ps(unpack(&p),_mm_set1_ps(wa)),_mm_mul_ps(unpack(&q),_mm_set1_ps(wb))));
            else
#endif
            result={rounded(p.r*wa+q.r*wb),rounded(p.g*wa+q.g*wb),rounded(p.b*wa+q.b*wb),255};
            out.data()[std::size_t(y)*size.width+x]=result;
        }
    });
}
struct Contribution {int index;double weight;};
struct AxisPoint {int lo=0,hi=0;float fraction=0,coordinate=0;};
struct ScalePlan {
    Size source,target;int width=0,height=0,ox=0,oy=0;
    std::vector<std::vector<Contribution>> xs,ys;
    std::vector<AxisPoint> xp,yp;
    void prepare(Size s,Size d) {
        if(source==s&&target==d)return;
        source=s;target=d;double ratio=std::min(double(d.width)/s.width,double(d.height)/s.height);
        width=std::max(1,int(std::lround(s.width*ratio)));height=std::max(1,int(std::lround(s.height*ratio)));
        ox=(d.width-width)/2;oy=(d.height-height)/2;
        auto axis=[](int src,int dst,std::vector<std::vector<Contribution>>& axis){
            axis.resize(dst);
            for(int i=0;i<dst;++i){auto& entries=axis[i];entries.clear();
                double lo=double(i)*src/dst,hi=double(i+1)*src/dst;
                for(int j=int(lo);j<std::min(src,int(std::ceil(hi)));++j)
                    entries.push_back({j,std::min(hi,double(j+1))-std::max(lo,double(j))});
            }
        };
        axis(s.width,width,xs);axis(s.height,height,ys);
        auto points=[](int source,int target,std::vector<AxisPoint>& out){
            out.resize(target);
            for(int i=0;i<target;++i) {
                float coordinate=float((i+.5)*source/target-.5);
                coordinate=std::max(0.f,std::min(coordinate,float(source-1)));
                int lo=int(coordinate);out[i]={lo,std::min(lo+1,source-1),coordinate-lo,coordinate};
            }
        };
        points(s.width,width,xp);points(s.height,height,yp);
    }
};
template<bool simd> void resize_image(const Image& source,Size size,Filter filter,Image& out,ScalePlan& plan,Rows& rows,bool integer_scale,bool cached_scale) {
    out.reset(size);auto input=source.size();
    if(input==size){std::memcpy(out.data(),source.data(),out.bytes());record_image_copy(out.bytes());return;}
    plan.prepare(input,size);
    if(plan.width!=size.width||plan.height!=size.height)std::fill(out.data(),out.data()+checked_pixels(size),Pixel{});
    // Integer area footprints have unit weights: accumulate exactly in integers.
    // This is common for 1080->540/360 and avoids nested contribution containers.
    int sx=input.width/plan.width,sy=input.height/plan.height;
    if(integer_scale&&filter==Filter::Area&&sx>=1&&sy>=1&&sx<=8&&sy<=8&&
       input.width==plan.width*sx&&input.height==plan.height*sy) {
        rows.run(plan.height,[&](int begin,int end){
            for(int y=begin;y<end;++y)for(int x=0;x<plan.width;++x) {
                int r=0,g=0,b=0;
#ifdef FG_SSE2
                if(simd) {
                    auto sum=_mm_setzero_si128();
                    for(int j=0;j<sy;++j)for(int i=0;i<sx;++i) {
                        uint32_t value;std::memcpy(&value,source.data()+std::size_t(y*sy+j)*input.width+x*sx+i,4);
                        sum=_mm_add_epi16(sum,_mm_unpacklo_epi8(_mm_cvtsi32_si128(int(value)),_mm_setzero_si128()));
                    }
                    alignas(16) uint16_t channels[8];_mm_store_si128(reinterpret_cast<__m128i*>(channels),sum);
                    r=channels[0];g=channels[1];b=channels[2];
                } else
#endif
                for(int j=0;j<sy;++j)for(int i=0;i<sx;++i) {
                    auto p=source.data()[std::size_t(y*sy+j)*input.width+x*sx+i];r+=p.r;g+=p.g;b+=p.b;
                }
                int n=sx*sy;
                out.data()[std::size_t(y+plan.oy)*size.width+x+plan.ox]={uint8_t((r+n/2)/n),uint8_t((g+n/2)/n),uint8_t((b+n/2)/n),255};
            }
        });return;
    }
    bool bilinear=filter==Filter::Bilinear||std::min(double(size.width)/input.width,double(size.height)/input.height)>=1;
    rows.run(plan.height,[&](int begin,int end){
        for(int y=begin;y<end;++y)for(int x=0;x<plan.width;++x) {
            Pixel p;
            if(filter==Filter::Nearest||filter==Filter::Bicubic)
                p=reconstruct(source,plan.xp[x].coordinate,plan.yp[y].coordinate,filter);
            else if(bilinear&&!cached_scale)p=sampled<simd>(source,float((x+.5)*input.width/plan.width-.5),float((y+.5)*input.height/plan.height-.5));
            else if(bilinear) {
                auto xx=plan.xp[x],yy=plan.yp[y];float fx=xx.fraction,fy=yy.fraction;
                const auto* top=source.data()+std::size_t(yy.lo)*input.width;
                const auto* bottom=source.data()+std::size_t(yy.hi)*input.width;
                Sampling samples{{top+xx.lo,top+xx.hi,bottom+xx.lo,bottom+xx.hi},
                    {(1-fx)*(1-fy),fx*(1-fy),(1-fx)*fy,fx*fy}};
#ifdef FG_SSE2
                if(simd)p=sample_simd(samples);else
#endif
                p=sample_scalar(samples);
            }
            else {
                double r=0,g=0,b=0,weight=0;
                for(auto yy:plan.ys[y])for(auto xx:plan.xs[x]) {
                    double w=xx.weight*yy.weight;auto q=source.data()[std::size_t(yy.index)*input.width+xx.index];
                    r+=w*q.r;g+=w*q.g;b+=w*q.b;weight+=w;
                }
                // Values are nonnegative and bounded; trunc(v + .5) equals lround here.
                p={uint8_t(r/weight+.5),uint8_t(g/weight+.5),uint8_t(b/weight+.5),255};
            }
            out.data()[std::size_t(y+plan.oy)*size.width+x+plan.ox]=p;
        }
    });
}
}
struct Processor::Impl {
    Execution execution;Rows rows;
    Image pa{{1,1}},pb{{1,1}},temporary{{1,1}};
    ScalePlan prepare_plan,output_plan;
    Batch batch;
    explicit Impl(Execution e):execution(e),rows(e.threads){}
    void scale_into(const Image& image,Size s,Filter f,Image& out,ScalePlan& plan) {
        if(execution.simd)resize_image<true>(image,s,f,out,plan,rows,execution.integer_scale,execution.cached_scale);
        else resize_image<false>(image,s,f,out,plan,rows,execution.integer_scale,execution.cached_scale);
    }
};
Processor::Processor(const Execution& execution):impl_(new Impl(execution)){}
Processor::~Processor()=default;
Batch Processor::take_result(){return std::move(impl_->batch);}
Batch& Processor::run(const Image& a,const Image& b,const Config& c) {
    auto& w=*impl_;validate(c,a.size());
    if(a.size()!=b.size())throw std::invalid_argument("Input frame dimensions changed; reset capture");
    if(c.execution.threads!=w.execution.threads)throw std::invalid_argument("Recreate Processor to change its worker count");
    w.execution=c.execution;
    if(!c.execution.kernels){w.batch=process_reference(a,b,c);return w.batch;}
    for(const auto& frame:w.batch.frames)
        if(a.data()==frame.data()||b.data()==frame.data())throw std::invalid_argument("Input aliases the borrowed previous output");
    auto begin=Clock::now();auto activity=image_activity();
    if(!c.execution.reuse) {
        w.pa=Image({1,1});w.pb=Image({1,1});w.temporary=Image({1,1});
        w.batch.frames.clear();w.prepare_plan=ScalePlan{};w.output_plan=ScalePlan{};
    }
    w.batch.metrics=Metrics{};auto& m=w.batch.metrics;m.input=a.size();
    m.processing=c.enabled?resolve(c.processing,a.size()):a.size();m.output=c.enabled?resolve(c.output,a.size()):a.size();
    auto start=Clock::now();
    const Image *pa=&a,*pb=&b;
    if(m.processing!=a.size()||!c.execution.direct) {
        w.scale_into(a,m.processing,c.filter,w.pa,w.prepare_plan);w.scale_into(b,m.processing,c.filter,w.pb,w.prepare_plan);pa=&w.pa;pb=&w.pb;
    }
    m.preparation_ms=elapsed_ms(start);Motion motion;int count=c.enabled?c.multiplier:1;
    if(c.enabled) {
        start=Clock::now();motion=c.execution.efficient_motion?estimate_efficient(*pa,*pb,c.profile):estimate(*pa,*pb,c.profile);
        m.motion_ms=elapsed_ms(start);m.scene_cut=motion.scene_cut;if(m.scene_cut){count=1;m.skipped_reason=1;}
        else if(c.scene_guard) {
            double cost=0,extreme=0;
            for(auto v:motion.forward.vectors){cost+=v.cost;extreme+=std::hypot(v.x,v.y)>m.processing.width*.20;}
            auto n=motion.forward.vectors.size();
            if(n&&cost/n>35&&motion.mean_difference>25){count=1;m.skipped_reason=2;}
            else if(n&&extreme/n>.75){count=1;m.skipped_reason=3;}
        }
    }
    while(w.batch.frames.size()>std::size_t(count))w.batch.frames.pop_back();
    while(w.batch.frames.size()<std::size_t(count))w.batch.frames.emplace_back(m.output);
    w.batch.times.resize(count);
    for(int i=0;i<count;++i) {
        auto& frame=w.batch.frames[i];float t=float(i)/count;w.batch.times[i]=t;
        if(i==0){start=Clock::now();w.scale_into(*pa,m.output,c.filter,frame,w.output_plan);m.scaling_ms+=elapsed_ms(start);continue;}
        Image& destination=c.execution.direct&&m.output==m.processing?frame:w.temporary;
        start=Clock::now();
        if(c.execution.simd) {
            if(c.execution.vector_flow)warp<true,true>(*pa,*pb,motion,t,destination,w.rows,c.warp_iterations);
            else warp<true,false>(*pa,*pb,motion,t,destination,w.rows,c.warp_iterations);
        } else warp<false,false>(*pa,*pb,motion,t,destination,w.rows,c.warp_iterations);
        m.interpolation_ms+=elapsed_ms(start);
        if(&destination!=&frame){start=Clock::now();w.scale_into(destination,m.output,c.filter,frame,w.output_plan);m.scaling_ms+=elapsed_ms(start);}
    }
    if(c.enabled&&c.sharpness) {
        start=Clock::now();
        for(auto& frame:w.batch.frames)frame=sharpen(frame,c.sharpness);
        m.scaling_ms+=elapsed_ms(start);
    }
    m.generated=count-1;m.total_ms=elapsed_ms(begin);m.lookahead_ms=c.enabled?c.source_interval_ms:0;
    m.latency_estimate_ms=m.lookahead_ms+m.total_ms;m.overloaded=c.enabled&&m.total_ms>c.source_interval_ms;
    m.image_bytes=a.capacity_bytes()+b.capacity_bytes()+w.pa.capacity_bytes()+w.pb.capacity_bytes()+w.temporary.capacity_bytes();
    for(const auto& im:w.batch.frames)m.image_bytes+=im.capacity_bytes();
    m.working_set_upper_bound=m.image_bytes+4*320u*2160u*sizeof(float)+(motion.forward.vectors.size()+motion.backward.vectors.size())*sizeof(Vector);
    auto after=image_activity();m.image_allocations=after.allocations-activity.allocations;m.copied_image_bytes=after.copied_bytes-activity.copied_bytes;
    return w.batch;
}
Batch process(const Image& a,const Image& b,const Config& c) {
    if(!c.execution.kernels)return process_reference(a,b,c);
    Processor processor(c.execution);processor.run(a,b,c);return processor.take_result();
}
}
