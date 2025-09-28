#pragma once

#include "gd_viewdata.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/templates/vector.hpp>
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

    bool load( const godot::String& path );

    godot::String get_current_level();
    int get_num_views();
    int get_closest_view_Id( const godot::Vector3& point );
    const std::unique_ptr<MFViewData>& get_view_data( int viewId );
    godot::PackedVector3Array get_navmesh();

    bool set_current_view( int viewId );

  private:
    static inline MFManager* m_static_inst = nullptr;
    mft::ViewLevelManager<MFViewData> m_manager;
    int m_currentViewId;

    godot::String m_currentLevel;
};