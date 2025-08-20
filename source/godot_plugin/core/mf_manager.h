#pragma once

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <level_manager.h>

struct MFViewData
{
    int viewID;
    godot::String name;
    godot::Transform3D transform;
    float fov;
    int sizeX;
    int sizeY;
    float *colorBuffer;
    float *depthBuffer;
    float *envBuffer;
};

class MFManager : public godot::Object
{
    GDCLASS( MFManager, godot::Object )

protected:
    static void _bind_methods();

public:
    virtual ~MFManager() override;
    MFManager();
    static inline MFManager *get()
    {
        return m_static_inst;
    }

    bool load( const godot::String &path );

    godot::String get_current_level();
    int get_views_length();
    int get_closest_view_Id( const godot::Vector3 &point );
    MFViewData get_view_data( int viewId );
    godot::PackedVector3Array get_navmesh();

    bool set_current_view( int viewId );

private:
    static inline MFManager *m_static_inst = nullptr;
    mft::LevelManager m_manager;
    MFViewData m_currentView;

    godot::String m_currentLevel;
};