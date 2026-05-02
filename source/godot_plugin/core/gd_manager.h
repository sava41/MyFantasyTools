#pragma once

#include "gd_view_resources.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/object.hpp>
#include <mf_view_level_manager.h>

class MFManager : public godot::Object
{
    GDCLASS( MFManager, godot::Object )

  protected:
    static void _bind_methods();

  public:
    virtual ~MFManager() override;
    MFManager();
    static inline MFManager* get()
    {
        return m_static_inst;
    }

    // Load (or swap to) the level at path. If path is already active, no-op.
    bool load( const godot::String& path );

    godot::String get_active_path() const;
    int get_num_views() const;
    int get_closest_view_id( const godot::Vector3& point ) const;
    const std::unique_ptr<GDViewResources>& get_view_data( int viewId );
    godot::PackedVector3Array get_navmesh();

    bool set_current_view( int viewId );

  private:
    static inline MFManager* m_static_inst = nullptr;
    mft::ViewLevelManager<GDViewResources> m_manager;
    godot::String m_activePath;
    int m_currentViewId = -1;
};
