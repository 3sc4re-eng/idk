#include "collision.h"
#include "asset_loader_retry.h"
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#include <windows.h>
#include <compressapi.h>
#include <fstream>
#include <functional>
#include <limits>
#include <condition_variable>
#include <thread>
#include <chrono>
namespace collision{
static float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static bool finite(Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&p.len()<1e8f;}
static Vec3 lower(Vec3 a,Vec3 b){return {std::min(a.x,b.x),std::min(a.y,b.y),std::min(a.z,b.z)};}
static Vec3 upper(Vec3 a,Vec3 b){return {std::max(a.x,b.x),std::max(a.y,b.y),std::max(a.z,b.z)};}
static Vec3 lo(const Shape& s){return s.kind?lower(s.a,s.b)-Vec3{s.radius,s.radius,s.radius}:lower(lower(s.a,s.b),s.c);}
static Vec3 hi(const Shape& s){return s.kind?upper(s.a,s.b)+Vec3{s.radius,s.radius,s.radius}:upper(upper(s.a,s.b),s.c);}
static float component(Vec3 v,int axis){return axis==0?v.x:(axis==1?v.y:v.z);}
void Scene::build(std::vector<Shape> shapes,std::string map){
    shapes_=std::move(shapes);map_=std::move(map);nodes_.clear();
    shapes_.erase(std::remove_if(shapes_.begin(),shapes_.end(),[](const Shape&s){return !s.mask||!finite(s.a)||!finite(s.b)||!finite(s.c)||s.kind>1||!std::isfinite(s.radius)||s.radius<0;}),shapes_.end());
    std::function<u32(u32,u32)> split=[&](u32 first,u32 count){
        u32 index=(u32)nodes_.size();Node node;node.low={1e30f,1e30f,1e30f};node.high={-1e30f,-1e30f,-1e30f};
        for(u32 i=first;i<first+count;i++){node.low=lower(node.low,lo(shapes_[i]));node.high=upper(node.high,hi(shapes_[i]));}
        nodes_.push_back(node);if(count<=8){nodes_[index].first=first;nodes_[index].count=count;return index;}
        Vec3 extent=node.high-node.low;int axis=extent.y>extent.x?1:0;if(extent.z>component(extent,axis))axis=2;
        u32 middle=first+count/2;std::nth_element(shapes_.begin()+first,shapes_.begin()+middle,shapes_.begin()+first+count,[&](const Shape&a,const Shape&b){return component(lo(a)+hi(a),axis)<component(lo(b)+hi(b),axis);});
        auto left=split(first,middle-first),right=split(middle,first+count-middle);nodes_[index].left=left;nodes_[index].right=right;return index;
    };if(!shapes_.empty())split(0,(u32)shapes_.size());
}
static bool box(Vec3 start,Vec3 delta,Vec3 low,Vec3 high,float limit){
    float enter=0,exit=limit;for(int i=0;i<3;i++){float p=component(start,i),v=component(delta,i),l=component(low,i),h=component(high,i);
        if(std::fabs(v)<1e-9f){if(p<l||p>h)return false;continue;}float a=(l-p)/v,b=(h-p)/v;if(a>b)std::swap(a,b);enter=std::max(enter,a);exit=std::min(exit,b);if(enter>exit)return false;
    }return true;
}
static float sphere(Vec3 start,Vec3 delta,Vec3 center,float radius){
    Vec3 v=start-center;float a=dot(delta,delta),b=dot(v,delta),c=dot(v,v)-radius*radius,d=b*b-a*c;if(c<=0)return 0;if(a<1e-12f||d<0)return 2;float t=(-b-std::sqrt(d))/a;return t>=0?t:2;
}
static float intersect(const Shape& s,Vec3 start,Vec3 delta){
    if(s.kind){
        float best=std::min(sphere(start,delta,s.a,s.radius),sphere(start,delta,s.b,s.radius));Vec3 axis=s.b-s.a;float length=axis.len();if(length<1e-5f)return best;
        axis=axis*(1.f/length);Vec3 v=start-s.a,dp=delta-axis*dot(delta,axis),vp=v-axis*dot(v,axis);float a=dot(dp,dp),b=dot(vp,dp),c=dot(vp,vp)-s.radius*s.radius,discriminant=b*b-a*c;
        float height=dot(v,axis);if(c<=0&&height>=0&&height<=length)return 0;
        if(a>1e-10f&&discriminant>=0){float t=(-b-std::sqrt(discriminant))/a;float h=height+t*dot(delta,axis);if(t>=0&&h>=0&&h<=length)best=std::min(best,t);}return best;
    }
    Vec3 ab=s.b-s.a,ac=s.c-s.a,p=cross(delta,ac);float determinant=dot(ab,p);if(std::fabs(determinant)<1e-8f)return 2;
    float inv=1/determinant;Vec3 t=start-s.a;float u=dot(t,p)*inv;if(u<0||u>1)return 2;Vec3 q=cross(t,ab);float v=dot(delta,q)*inv;if(v<0||u+v>1)return 2;float distance=dot(ac,q)*inv;return distance>=0?distance:2;
}
Result Scene::trace(Vec3 from,Vec3 to,unsigned channel)const{
    Result result;if(nodes_.empty()||!finite(from)||!finite(to)||!channel)return result;result.known=true;result.clear=true;Vec3 delta=to-from;if(delta.len()<.01f)return result;
    std::array<u32,128> stack{};size_t top=1;stack[0]=0;
    while(top){auto index=stack[--top];const auto& node=nodes_[index];if(!box(from,delta,node.low,node.high,result.fraction))continue;
        if(node.count){for(u32 i=node.first;i<node.first+node.count;i++){const auto& shape=shapes_[i];if(!(shape.mask&channel))continue;float t=intersect(shape,from,delta);if(t>=0&&t<result.fraction&&t<1-.01f/delta.len()){result.fraction=t;result.clear=false;}}}
        else{if(top+2>stack.size()){result.known=false;result.clear=false;return result;}stack[top++]=node.left;stack[top++]=node.right;}
    }result.hit=from+delta*result.fraction;return result;
}
Result Scene::trace_any(Vec3 from,Vec3 to,unsigned channel)const{
    Result result;if(nodes_.empty()||!finite(from)||!finite(to)||!channel)return result;
    result.known=true;result.clear=true;result.hit=to;Vec3 delta=to-from;const float length=delta.len();if(length<.01f)return result;
    const float endpoint=1-.01f/length;std::array<u32,128> stack{};size_t top=1;stack[0]=0;
    while(top){
        const auto& node=nodes_[stack[--top]];if(!box(from,delta,node.low,node.high,1))continue;
        if(node.count){
            for(u32 i=node.first;i<node.first+node.count;i++){
                const auto& shape=shapes_[i];if(!(shape.mask&channel))continue;const float t=intersect(shape,from,delta);
                if(t>=0&&t<1&&t<endpoint){result.clear=false;result.fraction=t;result.hit=from+delta*t;return result;}
            }
        }else{
            if(top+2>stack.size()){result.known=false;result.clear=false;return result;}
            stack[top++]=node.left;stack[top++]=node.right;
        }
    }return result;
}
struct Header{char magic[8];u32 version,revision,raw_size,packed_size,shapes,nodes;u64 hash;char map[64];};
static u64 hash_bytes(const unsigned char* data,size_t size){u64 value=14695981039346656037ull;for(size_t i=0;i<size;i++){value^=data[i];value*=1099511628211ull;}return value;}
bool Scene::save(const std::string& file,u32 revision,std::string& error)const{
    if(nodes_.empty()||map_.size()>63){error="Empty collision scene";return false;}size_t raw_size=shapes_.size()*sizeof(Shape)+nodes_.size()*sizeof(Node);if(raw_size>512*1024*1024){error="Collision scene too large";return false;}
    std::vector<unsigned char> raw(raw_size);std::memcpy(raw.data(),shapes_.data(),shapes_.size()*sizeof(Shape));std::memcpy(raw.data()+shapes_.size()*sizeof(Shape),nodes_.data(),nodes_.size()*sizeof(Node));
    COMPRESSOR_HANDLE handle=nullptr;if(!CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF,nullptr,&handle)){error="Compressor unavailable";return false;}SIZE_T needed=0;Compress(handle,raw.data(),raw.size(),nullptr,0,&needed);std::vector<unsigned char> packed(needed);
    bool ok=Compress(handle,raw.data(),raw.size(),packed.data(),packed.size(),&needed);CloseCompressor(handle);if(!ok){error="Collision compression failed";return false;}packed.resize(needed);
    Header h{};std::memcpy(h.magic,"ACELOS2",7);h.version=2;h.revision=revision;h.raw_size=(u32)raw.size();h.packed_size=(u32)packed.size();h.shapes=(u32)shapes_.size();h.nodes=(u32)nodes_.size();h.hash=hash_bytes(raw.data(),raw.size());std::memcpy(h.map,map_.data(),map_.size());
    std::ofstream out(file,std::ios::binary);out.write((char*)&h,sizeof(h));out.write((char*)packed.data(),packed.size());if(!out){error="Collision file write failed";return false;}return true;
}
bool Scene::load(const std::string& file,const std::string& map,std::string& error){return load(file,map,11193905,error);}
bool Scene::load(const std::string& file,const std::string& map,u32 expected_revision,std::string& error){
    shapes_.clear();nodes_.clear();map_.clear();std::ifstream in(file,std::ios::binary|std::ios::ate);auto file_size=in.tellg();in.seekg(0);Header h{};in.read((char*)&h,sizeof(h));
    if(!in||std::memcmp(h.magic,"ACELOS2",8)||h.version!=2||h.revision!=expected_revision||!std::memchr(h.map,0,64)||map!=h.map||h.raw_size>512*1024*1024||h.packed_size>512*1024*1024||h.shapes==0||h.nodes==0||h.nodes>2ull*h.shapes||sizeof(h)+(u64)h.packed_size!=(u64)file_size||(u64)h.shapes*sizeof(Shape)+(u64)h.nodes*sizeof(Node)!=h.raw_size){error="Missing or incompatible map collision file";return false;}
    std::vector<unsigned char> packed(h.packed_size),raw(h.raw_size);in.read((char*)packed.data(),packed.size());DECOMPRESSOR_HANDLE handle=nullptr;SIZE_T written=0;
    if(!in||!CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF,nullptr,&handle)){error="Collision decompressor unavailable";return false;}bool ok=Decompress(handle,packed.data(),packed.size(),raw.data(),raw.size(),&written);CloseDecompressor(handle);
    if(!ok||written!=raw.size()||hash_bytes(raw.data(),raw.size())!=h.hash){error="Corrupt map collision data";return false;}
    std::vector<Shape> shapes(h.shapes);std::vector<Node> nodes(h.nodes);std::memcpy(shapes.data(),raw.data(),shapes.size()*sizeof(Shape));std::memcpy(nodes.data(),raw.data()+shapes.size()*sizeof(Shape),nodes.size()*sizeof(Node));
    for(const auto&s:shapes)if(!finite(s.a)||!finite(s.b)||!finite(s.c)||s.kind>1||!std::isfinite(s.radius)||s.radius<0){error="Invalid collision primitive";return false;}
    std::vector<bool> visited(nodes.size());std::function<bool(u32,unsigned)> validate=[&](u32 index,unsigned depth){if(index>=nodes.size()||depth>100||visited[index])return false;visited[index]=true;auto& n=nodes[index];if(!finite(n.low)||!finite(n.high)||n.low.x>n.high.x||n.low.y>n.high.y||n.low.z>n.high.z)return false;if(n.count)return (u64)n.first+n.count<=shapes.size();return validate(n.left,depth+1)&&validate(n.right,depth+1);};
    if(!validate(0,0)||std::find(visited.begin(),visited.end(),false)!=visited.end()){error="Invalid collision BVH";return false;}shapes_=std::move(shapes);nodes_=std::move(nodes);map_=map;return true;
}
bool camera_origin(const float* m,Vec3& position){Vec3 a{m[0],m[1],m[2]},b{m[4],m[5],m[6]},c{m[12],m[13],m[14]};float determinant=dot(a,cross(b,c));if(!std::isfinite(determinant)||std::fabs(determinant)<1e-7f)return false;position=(cross(b,c)*(-m[3])+cross(c,a)*(-m[7])+cross(a,b)*(-m[15]))*(1/determinant);return finite(position);}
static std::array<u32,13> obstacle_key(const Obstacle& value){
    auto bits=[](float number){if(number==0)number=0;u32 key=0;std::memcpy(&key,&number,sizeof(key));return key;};
    return {bits(value.origin.x),bits(value.origin.y),bits(value.origin.z),bits(value.low.x),bits(value.low.y),bits(value.low.z),bits(value.high.x),bits(value.high.y),bits(value.high.z),bits(value.angles.x),bits(value.angles.y),bits(value.angles.z),bits(value.scale)};
}

// Geometry is built/decompressed only by this worker. A reader holds an
// immutable scene/obstacle snapshot, never the loader mutex during traversal.
class NativeSceneLoader {
    using ScenePtr=std::shared_ptr<const Scene>;
    using ObstaclesPtr=std::shared_ptr<const std::vector<Obstacle>>;
    mutable std::mutex mutex_;
    std::mutex shutdown_mutex_;
    std::condition_variable wake_;
    std::thread worker_;
    bool stopping_=false,requested_once_=false,pending_=false;
    asset_loader_retry::State retry_;
    u64 generation_=0,published_generation_=0;
    GameMapKey requested_;
    std::string message_="Map collision not loaded";
    Revision revision_{};
    ScenePtr scene_;
    ObstaclesPtr obstacles_=std::make_shared<const std::vector<Obstacle>>();
    // Keep retired ownership until existing traces release their references.
    // The worker then reclaims the large BVH outside every state/query lock.
    std::vector<ScenePtr> retired_;
    static void advance(u64& value){if(++value==0)++value;}
    void run(){
        for(;;){
            GameMapKey key;u64 generation=0;bool have_job=false;
            std::vector<ScenePtr> reclaim;
            {
                std::unique_lock lock(mutex_);
                if(!pending_&&retired_.empty())wake_.wait(lock,[&]{return stopping_||pending_||!retired_.empty();});
                else if(!pending_)wake_.wait_for(lock,std::chrono::milliseconds(25),[&]{return stopping_||pending_;});
                if(stopping_)return;
                for(auto it=retired_.begin();it!=retired_.end();){
                    if(it->use_count()==1){reclaim.push_back(std::move(*it));it=retired_.erase(it);}else ++it;
                }
                if(pending_){key=requested_;generation=generation_;pending_=false;have_job=true;}
            }
            reclaim.clear();
            if(!have_job)continue;
            const auto started=now_ms();const auto* asset=find_map_asset(key);const std::string label=key.game_id+"/"+key.map_id+" @"+std::to_string(key.build_revision);std::shared_ptr<Scene> next;std::string error;bool loaded=false;
            try{next=std::make_shared<Scene>();if(asset)loaded=next->load(asset->path,key.map_id,asset->asset_revision,error);else error="Map asset is not registered";}
            catch(const std::exception&){error="Collision load raised an exception";}
            catch(...){error="Collision load raised an unknown exception";}
            std::string report;bool obsolete=false;
            {
                std::lock_guard lock(mutex_);
                if(stopping_)return;
                obsolete=generation!=generation_||key!=requested_;
                if(!obsolete){
                    // A stale completion must not alter the current request's retry state.
                    retry_.finish(loaded,now_ms());
                    if(loaded){scene_=std::move(next);published_generation_=generation;advance(revision_.scene);message_="Ready: "+label+": "+std::to_string(scene_->size())+" collision primitives";}
                    else{published_generation_=0;message_="Unavailable: "+label+": "+(error.empty()?"Collision load failed":error);}
                    report=message_;
                }
            }
            if(obsolete)plat_log("Collision load discarded: %s (request changed), elapsed=%llu ms",label.c_str(),(unsigned long long)(now_ms()-started));
            else plat_log("Collision: %s, load elapsed=%llu ms",report.c_str(),(unsigned long long)(now_ms()-started));
        }
    }
public:
    struct Context{ScenePtr scene;ObstaclesPtr obstacles;Revision revision;u64 generation=0;};
    NativeSceneLoader()=default;
    ~NativeSceneLoader(){shutdown();}
    NativeSceneLoader(const NativeSceneLoader&)=delete;
    NativeSceneLoader& operator=(const NativeSceneLoader&)=delete;
    void request(const GameMapKey& key){
        std::string report;
        {
            std::lock_guard lock(mutex_);
            if(stopping_||!retry_.accepts(requested_once_&&requested_==key,now_ms()))return;
            // Invalidate before scheduling I/O. Retired geometry cannot answer
            // queries for a new, empty, invalid or unsuccessfully loaded map.
            requested_once_=true;requested_=key;advance(generation_);advance(revision_.scene);published_generation_=0;
            if(scene_)retired_.push_back(std::move(scene_));
            // Dynamic geometry from another map/game is not retained either.
            if(!obstacles_->empty()){obstacles_=std::make_shared<const std::vector<Obstacle>>();advance(revision_.obstacles);}
            const bool valid_key=!key.game_id.empty()&&key.game_id.size()<=64&&!key.map_id.empty()&&key.map_id.size()<=128;
            const auto* asset=valid_key?find_map_asset(key):nullptr;pending_=asset!=nullptr;
            // Existing per-frame selection retries only failed registered assets,
            // after the backoff; it never restarts a ready or in-flight load.
            retry_.begin(pending_);
            const std::string label=key.game_id+"/"+key.map_id+" @"+std::to_string(key.build_revision);
            message_=key.map_id.empty()?"Unavailable: map not identified":(!asset?"Unavailable: no map geometry registered for "+label:"Loading: "+label);
            if(asset&&!worker_.joinable()){
                try{worker_=std::thread([this]{run();});}
                catch(...){pending_=false;retry_.finish(false,now_ms());message_="Unavailable: map loader worker could not start";}
            }
            report=message_;
        }
        wake_.notify_one();plat_log("Collision: %s",report.c_str());
    }
    Context acquire()const{
        std::lock_guard lock(mutex_);
        if(stopping_||!scene_||published_generation_!=generation_||scene_->map()!=requested_.map_id)return {};
        return {scene_,obstacles_,revision_,generation_};
    }
    Result checked(const Context& context,Result value)const{
        std::lock_guard lock(mutex_);
        if(stopping_||context.generation!=generation_||published_generation_!=generation_||context.scene!=scene_||context.revision!=revision_)return {};
        return value;
    }
    Revision revision()const{std::lock_guard lock(mutex_);return revision_;}
    std::string status()const{std::lock_guard lock(mutex_);return message_;}
    void obstacles(std::vector<Obstacle> next){
        std::sort(next.begin(),next.end(),[](const auto& a,const auto& b){return obstacle_key(a)<obstacle_key(b);});
        next.erase(std::unique(next.begin(),next.end(),[](const auto& a,const auto& b){return obstacle_key(a)==obstacle_key(b);}),next.end());
        auto replacement=std::make_shared<const std::vector<Obstacle>>(std::move(next));ObstaclesPtr previous;
        {
            std::lock_guard lock(mutex_);bool equal=obstacles_->size()==replacement->size();
            if(equal)for(size_t i=0;i<replacement->size();i++)if(obstacle_key((*obstacles_)[i])!=obstacle_key((*replacement)[i])){equal=false;break;}
            if(equal)return;
            previous=std::move(obstacles_);obstacles_=std::move(replacement);advance(revision_.obstacles);
        }
    }
    // Terminal operation: the host stops query/reader workers first. Joining may
    // wait for the current native decompression; it is never forcibly cancelled.
    void shutdown(){
        std::lock_guard shutdown_lock(shutdown_mutex_);ScenePtr release;
        {
            std::lock_guard lock(mutex_);
            if(stopping_)return;
            stopping_=true;pending_=false;retry_.stop();advance(generation_);advance(revision_.scene);published_generation_=0;
            release=std::move(scene_);message_="Map collision stopped";
        }
        wake_.notify_all();if(worker_.joinable())worker_.join();
        // Host has ended queries. Release final BVH ownership outside the lock.
        release.reset();retired_.clear();
    }
};
// Constructed on first host-side use, after ordinary namespace globals, so its
// destructor joins the worker before those globals are destroyed. Explicit
// shutdown() is the normal application-exit path and this is only a backstop.
static NativeSceneLoader& loader(){static NativeSceneLoader value;return value;}
void select_map(const GameMapKey& key){loader().request(key);}
void select_map(const std::string& map){select_map(GameMapKey{"deadlock",map,11193905});}
void set_obstacles(std::vector<Obstacle> obstacles){loader().obstacles(std::move(obstacles));}
Revision revision(){return loader().revision();}
std::string status(){return loader().status();}
void shutdown(){loader().shutdown();}
Result trace(Vec3 from,Vec3 to,unsigned channel){
    auto& owner=loader();auto context=owner.acquire();if(!context.scene)return {};auto result=context.scene->trace(from,to,channel);if(!result.known)return owner.checked(context,result);
    for(const auto& obstacle:*context.obstacles){
        float pitch=deg2rad(obstacle.angles.x),yaw=deg2rad(obstacle.angles.y),roll=deg2rad(obstacle.angles.z);float cp=std::cos(pitch),sp=std::sin(pitch),cy=std::cos(yaw),sy=std::sin(yaw),cr=std::cos(roll),sr=std::sin(roll);
        Vec3 x{cp*cy,cp*sy,-sp},y{cy*sp*sr-sy*cr,sy*sp*sr+cy*cr,cp*sr},z{cy*sp*cr+sy*sr,sy*sp*cr-cy*sr,cp*cr};
        auto local=[&](Vec3 point){Vec3 relative=point-obstacle.origin;return Vec3{dot(relative,x),dot(relative,y),dot(relative,z)}*(1/obstacle.scale);};Vec3 a=local(from),delta=local(to)-a;float enter=0,exit=result.fraction;bool hit=true;
        for(int axis=0;axis<3;axis++){float p=component(a,axis),v=component(delta,axis),low=component(obstacle.low,axis),high=component(obstacle.high,axis);if(std::fabs(v)<1e-8f){if(p<low||p>high){hit=false;break;}continue;}float first=(low-p)/v,last=(high-p)/v;if(first>last)std::swap(first,last);enter=std::max(enter,first);exit=std::min(exit,last);if(enter>exit){hit=false;break;}}
        if(hit&&enter<result.fraction){result.clear=false;result.fraction=enter;result.hit=from+(to-from)*enter;}
    }return owner.checked(context,result);
}
Result trace_any(Vec3 from,Vec3 to,unsigned channel){
    auto& owner=loader();auto context=owner.acquire();if(!context.scene)return {};auto result=context.scene->trace_any(from,to,channel);if(!result.known||!result.clear)return owner.checked(context,result);
    for(const auto& obstacle:*context.obstacles){
        // Missing/invalid dynamic geometry cannot establish a clear ray.
        if(!finite(obstacle.origin)||!finite(obstacle.low)||!finite(obstacle.high)||!finite(obstacle.angles)||!std::isfinite(obstacle.scale)||obstacle.scale<=0||obstacle.low.x>obstacle.high.x||obstacle.low.y>obstacle.high.y||obstacle.low.z>obstacle.high.z){result.known=false;result.clear=false;return owner.checked(context,result);}
        float pitch=deg2rad(obstacle.angles.x),yaw=deg2rad(obstacle.angles.y),roll=deg2rad(obstacle.angles.z);float cp=std::cos(pitch),sp=std::sin(pitch),cy=std::cos(yaw),sy=std::sin(yaw),cr=std::cos(roll),sr=std::sin(roll);
        Vec3 x{cp*cy,cp*sy,-sp},y{cy*sp*sr-sy*cr,sy*sp*sr+cy*cr,cp*sr},z{cy*sp*cr+sy*sr,sy*sp*cr-cy*sr,cp*cr};
        auto local=[&](Vec3 point){Vec3 relative=point-obstacle.origin;return Vec3{dot(relative,x),dot(relative,y),dot(relative,z)}*(1/obstacle.scale);};Vec3 a=local(from),delta=local(to)-a;float enter=0,exit=1;bool hit=true;
        for(int axis=0;axis<3;axis++){float p=component(a,axis),v=component(delta,axis),low=component(obstacle.low,axis),high=component(obstacle.high,axis);if(std::fabs(v)<1e-8f){if(p<low||p>high){hit=false;break;}continue;}float first=(low-p)/v,last=(high-p)/v;if(first>last)std::swap(first,last);enter=std::max(enter,first);exit=std::min(exit,last);if(enter>exit){hit=false;break;}}
        if(hit&&enter<1){result.clear=false;result.fraction=enter;result.hit=from+(to-from)*enter;return owner.checked(context,result);}
    }return owner.checked(context,result);
}
}
#ifdef ACE_COLLISION_BAKER
int main(int argc,char**argv){
    if(argc<5)return 1;std::ifstream triangles(argv[1],std::ios::binary),primitives(argv[2],std::ios::binary);std::vector<collision::Shape> shapes;struct Triangle{Vec3 a,b,c;u32 attribute;};struct Capsule{Vec3 a,b;float radius;u32 attribute;};
    std::vector<u32> masks;for(int i=5;i<argc;i++)masks.push_back((u32)std::stoul(argv[i]));Triangle t;while(triangles.read((char*)&t,sizeof(t))){if(t.attribute>=masks.size())return 2;shapes.push_back({t.a,t.b,t.c,0,0,masks[t.attribute]});}Capsule c;while(primitives.read((char*)&c,sizeof(c))){if(c.attribute>=masks.size())return 3;shapes.push_back({c.a,c.b,{},c.radius,1,masks[c.attribute]});}
    collision::Scene scene;scene.build(std::move(shapes),argv[3]);std::string error;if(!scene.save(argv[4],11193905,error)){std::fprintf(stderr,"%s\n",error.c_str());return 4;}std::printf("Baked %s: %zu collision primitives\n",argv[3],scene.size());return 0;
}
#endif
