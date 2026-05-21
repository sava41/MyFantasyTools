#pragma once

#include "mf_level.h"

#include <atomic>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <memory>
#include <unordered_set>
#include <vector>

class MFManager : public godot::Object
{
    GDCLASS( MFManager, godot::Object )

  protected:
    static void _bind_methods();

  public:
    enum class ViewStatus : uint8_t
    {
        unloaded = 0,
        loading  = 1 << 0,
        ready    = 1 << 1,
        loaded   = 1 << 2,
    };

    struct ViewCache
    {
        godot::Ref<godot::Image> color_direct;
        godot::Ref<godot::Image> color_indirect;
        godot::Ref<godot::Image> depth;
        godot::Ref<godot::Image> env;
        godot::Ref<godot::Image> light_dir;
    };

    virtual ~MFManager() override;
    MFManager();

    static inline MFManager* get()
    {
        return m_static_inst;
    }

    // Load (or swap to) the level at path. Clears all cached view data.
    bool load( const godot::String& path );
    godot::String get_active_path() const;

    const mft::Level& get_level() const
    {
        return m_level;
    }
    int get_num_views() const;
    int get_closest_view_id( const godot::Vector3& point ) const;
    godot::PackedVector3Array get_navmesh();

    // Triggers BFS-based eviction and async loading of nearby views.
    bool set_current_view( int view_id );

    // Call once per frame from MFLevel::_process to promote finished loads into the cache.
    // Emits "view_data_ready" for each view that becomes available.
    void poll_pending();

    bool is_view_loaded( int view_id ) const;
    const ViewCache* get_view_cache( int view_id ) const;

  private:
    void begin_load_view( int view_index );

    static inline MFManager* m_static_inst = nullptr;

    mft::Level m_level;
    godot::String m_active_path;
    int m_current_view_id = -1;
    std::unordered_set<int> m_in_range;

    // Both sized to num_views on load(). Indexed by view_id.
    std::vector<std::unique_ptr<ViewCache>> m_view_cache;
    std::vector<std::atomic<ViewStatus>> m_view_cache_status;
};
