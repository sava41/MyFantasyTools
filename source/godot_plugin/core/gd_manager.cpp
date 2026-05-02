#include "gd_manager.h"

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector3.hpp>

MFManager::MFManager()
    : godot::Object()
{
    m_static_inst = this;
}

MFManager::~MFManager()
{
    m_static_inst = nullptr;
}

bool MFManager::load( const godot::String& path )
{
    if( m_activePath == path )
        return true;

    const godot::String fullPath = godot::ProjectSettings::get_singleton()->globalize_path( path );

    // load_data clears old view data and streams in the new level
    if( !m_manager.load_data( fullPath.utf8().get_data() ) )
    {
        godot::UtilityFunctions::print( "MF manager failed to load " + fullPath );
        return false;
    }

    m_activePath    = path;
    m_currentViewId = -1;

    return true;
}

godot::String MFManager::get_active_path() const
{
    return m_activePath;
}

int MFManager::get_num_views() const
{
    return m_manager.get_num_views();
}

const std::unique_ptr<GDViewResources>& MFManager::get_view_data( int viewId )
{
    return m_manager.get_view( viewId );
}

int MFManager::get_closest_view_id( const godot::Vector3& point ) const
{
    // Input godot point is y-up; convert to z-up for mft
    const godot::Vector3 rotated = point.rotated( godot::Vector3( 1, 0, 0 ), Math_PI * 0.5 );

    return m_manager.get_view_id_from_position( rotated.x, rotated.y, rotated.z );
}

godot::PackedVector3Array MFManager::get_navmesh()
{
    godot::PackedVector3Array tris;
    tris.resize( m_manager.get_navmesh_num_tris() * 3 );

    for( int i = 0; i < tris.size(); i += 3 )
    {
        auto verts = m_manager.get_navmesh_tri_verts( i / 3 );

        // Godot uses CW winding order while mft uses CCW, so insert verts in reverse order.
        // Input transform is z-up; convert to y-up for godot.
        tris.set( i + 2, godot::Vector3( verts[0], verts[1], verts[2] ).rotated( godot::Vector3( 1, 0, 0 ), -Math_PI * 0.5 ) );
        tris.set( i + 1, godot::Vector3( verts[3], verts[4], verts[5] ).rotated( godot::Vector3( 1, 0, 0 ), -Math_PI * 0.5 ) );
        tris.set( i, godot::Vector3( verts[6], verts[7], verts[8] ).rotated( godot::Vector3( 1, 0, 0 ), -Math_PI * 0.5 ) );
    }

    return tris;
}

bool MFManager::set_current_view( int viewId )
{
    if( m_activePath.is_empty() )
        return false;

    if( viewId < 0 || viewId >= m_manager.get_num_views() || viewId == m_currentViewId )
        return false;

    m_currentViewId = viewId;
    m_manager.set_current_view( viewId );

    emit_signal( "current_view_changed", m_activePath, viewId );

    return true;
}

void MFManager::_bind_methods()
{
    ADD_SIGNAL( godot::MethodInfo( "current_view_changed",
        godot::PropertyInfo( godot::Variant::STRING, "path" ),
        godot::PropertyInfo( godot::Variant::INT, "view_id" ) ) );

    godot::ClassDB::bind_method( godot::D_METHOD( "load", "path" ), &MFManager::load );
    godot::ClassDB::bind_method( godot::D_METHOD( "get_active_path" ), &MFManager::get_active_path );
    godot::ClassDB::bind_method( godot::D_METHOD( "get_num_views" ), &MFManager::get_num_views );
    godot::ClassDB::bind_method( godot::D_METHOD( "get_closest_view_id", "point" ), &MFManager::get_closest_view_id );
    godot::ClassDB::bind_method( godot::D_METHOD( "set_current_view", "view_id" ), &MFManager::set_current_view );
    godot::ClassDB::bind_method( godot::D_METHOD( "get_navmesh" ), &MFManager::get_navmesh );
}
