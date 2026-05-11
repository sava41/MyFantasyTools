#pragma once

#include "mf_level.h"

#include <atomic>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <memory>
#include <unordered_map>

class MFManager : public godot::Object
{
    GDCLASS( MFManager, godot::Object )

  protected:
    static void _bind_methods();

  public:
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
    struct PendingView
    {
        // Images allocated on the main thread; worker writes into their buffers.
        godot::Ref<godot::Image> color_direct;
        godot::Ref<godot::Image> color_indirect;
        godot::Ref<godot::Image> depth;
        godot::Ref<godot::Image> env;
        godot::Ref<godot::Image> light_dir;

        // Raw pointers captured once before dispatching. Never touched on main thread
        // until ready fires.
        void* buf_color_direct   = nullptr;
        void* buf_color_indirect = nullptr;
        void* buf_depth          = nullptr;
        void* buf_env            = nullptr;
        void* buf_light_dir      = nullptr;

        // Written by worker with release; read by main thread with acquire.
        std::atomic<bool> ready{ false };
    };

    void begin_load_view( int view_index );

    static inline MFManager* m_static_inst = nullptr;

    mft::Level m_level;
    godot::String m_active_path;
    int m_current_view_id = -1;

    std::unordered_map<int, ViewCache> m_loaded;
    std::unordered_map<int, std::shared_ptr<PendingView>> m_pending;
};
