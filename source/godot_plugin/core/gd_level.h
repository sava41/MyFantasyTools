#pragma once

#include "mf_level.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/panorama_sky_material.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/timer.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

class MFLevel : public godot::Node3D
{
    GDCLASS( MFLevel, godot::Node3D )

  protected:
    static void _bind_methods();

  public:
    virtual ~MFLevel() override;
    MFLevel( const godot::String& path = "" );
    void _ready() override;
    void _enter_tree() override;
    void _exit_tree() override;
    void _process( double delta ) override;
    bool set_view( int view_id );
    bool set_closest_view( const godot::Vector3& point );
    bool look_at( godot::Vector3 point, bool clamp_region = true, float smooth = 0.0 );

    void update_shadows();

    void set_min_view_duration( float timeMS );
    float get_min_view_duration() const;

    void set_level_file_path( const godot::String& path );
    godot::String get_level_file_path() const;

  private:
    void initialize_level_data();
    void setup_editor_cameras();
    void setup_navmesh();
    bool apply_view( int view_id );
    void _on_view_data_ready( godot::String path, int view_id );

    static godot::Transform3D setup_camera( const mft::Level& level, int view_index, godot::Camera3D* camera );

    bool m_editor_mode;
    godot::String m_level_file_path;

    godot::Vector<godot::Camera3D*> m_editor_cameras;
    godot::Camera3D* m_game_camera             = nullptr;
    godot::MeshInstance3D* m_background        = nullptr;
    godot::StaticBody3D* m_collision           = nullptr;
    godot::CollisionShape3D* m_collision_shape = nullptr;

    godot::Ref<godot::ShaderMaterial> m_background_material;
    godot::Ref<godot::PanoramaSkyMaterial> m_sky_material;

    float m_min_view_duration;

    int m_cur_view_id     = -1;
    int m_pending_view_id = -1;

    godot::Transform3D m_cur_view_transform;

    std::atomic<bool> m_level_ready = false;
};