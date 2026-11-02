#pragma once
#include "utils.h"
#include <memory>
#include "map_asset_registry.h"
namespace collision{
enum Channel:unsigned {Vision=1,Bullet=2,Projectile=4,Ability=8};
struct Shape{Vec3 a,b,c;float radius=0;u32 kind=0,mask=15;};
struct Node{Vec3 low,high;u32 first=0,count=0,left=0,right=0;};
struct Result{bool known=false,clear=false;float fraction=1;Vec3 hit{};};
struct Revision{u64 scene=0,obstacles=0;};
inline bool operator==(Revision a,Revision b){return a.scene==b.scene&&a.obstacles==b.obstacles;}
inline bool operator!=(Revision a,Revision b){return !(a==b);}
class Scene{
    std::vector<Shape> shapes_;std::vector<Node> nodes_;std::string map_;
public:
    void build(std::vector<Shape> shapes,std::string map);
    // Explicit format revision supplied by the game-qualified asset registry.
    bool load(const std::string& file,const std::string& map,u32 expected_revision,std::string& error);
    // Deadlock-only compatibility for existing baker/standalone fixtures.
    bool load(const std::string& file,const std::string& map,std::string& error);
    bool save(const std::string& file,u32 revision,std::string& error)const;
    Result trace(Vec3 from,Vec3 to,unsigned channel)const;
    // Boolean visibility path: hit/fraction identify the first encountered blocker, not the nearest.
    Result trace_any(Vec3 from,Vec3 to,unsigned channel)const;
    size_t size()const{return shapes_.size();}const std::string& map()const{return map_;}
};
struct Obstacle{Vec3 origin,low,high,angles;float scale=1;};
void set_obstacles(std::vector<Obstacle> obstacles);
// Queues native .acecol work; no collision-asset I/O or decompression on the caller.
void select_map(const GameMapKey& key);
// Deadlock-only legacy entry point. Live dispatch must use the qualified key.
void select_map(const std::string& map);std::string status();
// Call once on application exit, after reader/query workers have stopped.
void shutdown();
Result trace(Vec3 from,Vec3 to,unsigned channel);
Result trace_any(Vec3 from,Vec3 to,unsigned channel);
Revision revision();
bool camera_origin(const float* matrix,Vec3& position);
}
