#include "mf_manager.h"

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector3.hpp>

// Used to mark unused parameters to indicate intent and suppress warnings.
#define UNUSED( expr ) (void)( expr )

MFManager::MFManager()
    : godot::Object()
{
    m_currentView.viewID = -1;
    m_static_inst        = this;
}

MFManager::~MFManager()
{
    m_static_inst = nullptr;
}

bool MFManager::load( const godot::String& path )
{
    if( m_currentLevel == path )
    {
        return true;
    }

    const godot::String fullPath = godot::ProjectSettings::get_singleton()->globalize_path( path );

    if( !m_manager.load_level( fullPath.utf8().get_data() ) )
    {
        godot::UtilityFunctions::print( "MF manager failed to load " + fullPath );
        return false;
    }

    m_currentLevel = path;

    return true;
}

godot::String MFManager::get_current_level()
{
    return m_currentLevel;
}

int MFManager::get_views_length()
{
    return m_manager.get_views_length();
}

MFViewData MFManager::get_view_data( int viewId )
{
    if( viewId < 0 || viewId >= m_manager.get_views_length() || viewId == m_currentView.viewID )
    {
        return m_currentView;
    }

    MFViewData view;

    view.viewID = viewId;

    view.name = godot::String( m_manager.get_view_name( viewId ).c_str() );

    view.fov = m_manager.get_view_fov( viewId );

    view.sizeX = m_manager.get_view_width( viewId );
    view.sizeY = m_manager.get_view_height( viewId );

    view.colorBuffer = m_manager.get_view_color_buffer( viewId );
    view.depthBuffer = m_manager.get_view_depth_buffer( viewId );
    view.envBuffer   = m_manager.get_view_env_buffer( viewId );

    // Transform comes in as 1x16 vector representing 4x4 matrix
    // Godot transform constructor takes basis vectors first and offset vector last
    const auto mat4 = m_manager.get_view_tranform( viewId );
    view.transform  = godot::Transform3D( mat4[0], mat4[1], mat4[2], mat4[4], mat4[5], mat4[6], mat4[8], mat4[9], mat4[10], mat4[3], mat4[7], mat4[11] );

    // Input tranform is z-up we need to convert to y-up for godot
    view.transform.rotate( godot::Vector3( 1, 0, 0 ), -Math_PI * 0.5 );

    return view;
}

int MFManager::get_closest_view_Id( const godot::Vector3& point )
{
    // Input godot point is y-up we need to convert to z-up for mft
    const godot::Vector3 rotated = point.rotated( godot::Vector3( 1, 0, 0 ), Math_PI * 0.5 );

    return m_manager.get_view_id_from_position( rotated.x, rotated.y, rotated.z );
}

godot::PackedVector3Array MFManager::get_navmesh()
{
    godot::PackedVector3Array tris;
    tris.resize( m_manager.get_navmesh_tris_length() * 3 );

    for( int i = 0; i < tris.size(); i += 3 )
    {
        auto verts = m_manager.get_navmesh_tri_verts( i / 3 );

        // Godot uses CW winding order for triangles while mft uses CCW so we insert verts in
        // reverse order
        // Input tranform is z-up we need to convert to y-up for godot
        tris.set( i + 2, godot::Vector3( verts[0], verts[1], verts[2] ).rotated( godot::Vector3( 1, 0, 0 ), -Math_PI * 0.5 ) );
        tris.set( i + 1, godot::Vector3( verts[3], verts[4], verts[5] ).rotated( godot::Vector3( 1, 0, 0 ), -Math_PI * 0.5 ) );
        tris.set( i, godot::Vector3( verts[6], verts[7], verts[8] ).rotated( godot::Vector3( 1, 0, 0 ), -Math_PI * 0.5 ) );
    }

    return tris;
}

bool MFManager::set_current_view( int viewId )
{
    if( viewId < 0 || viewId >= m_manager.get_views_length() || viewId == m_currentView.viewID )
    {
        return false;
    }

    m_currentView = get_view_data( viewId );

    emit_signal( "current_view_changed", m_currentView.viewID );

    return true;
}

void MFManager::_bind_methods()
{
    ADD_SIGNAL( godot::MethodInfo( "current_view_changed", godot::PropertyInfo( godot::Variant::INT, "view_id" ) ) );
    // godot::ClassDB::bind_method( godot::D_METHOD( "load", "path", "node" ), &MFManager::load );
    // godot::ClassDB::bind_method( godot::D_METHOD( "set_closest_view", "point" ),
    //                              &MFManager::set_closest_view );
}