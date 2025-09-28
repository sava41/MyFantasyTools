// Copied from godot-cpp/test/src and modified.

#include "gd_level.h"
#include "gd_manager.h"

#include <gdextension_interface.h>
#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/classes/editor_plugin_registration.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace
{

    static inline MFManager* mf_manager = nullptr;

    void on_initialize( godot::ModuleInitializationLevel p_level )
    {
        switch( p_level )
        {
        case godot::MODULE_INITIALIZATION_LEVEL_CORE:
        {
        }
        break;
        case godot::MODULE_INITIALIZATION_LEVEL_SERVERS:
        {
        }
        break;
        case godot::MODULE_INITIALIZATION_LEVEL_SCENE:
        {
            godot::ClassDB::register_class<MFManager>();
            godot::ClassDB::register_class<MFLevel>();

            mf_manager = memnew( MFManager );

            godot::Engine::get_singleton()->register_singleton( "MFManager", MFManager::get() );
        }
        break;
        case godot::MODULE_INITIALIZATION_LEVEL_EDITOR:
        {
#ifdef MFT_CONFIG_EDITOR

#endif // MFT_CONFIG_EDITOR
        }
        break;
        case godot::MODULE_INITIALIZATION_LEVEL_MAX:
        {
        }
        break;
        }
    }

    void on_terminate( godot::ModuleInitializationLevel p_level )
    {
        switch( p_level )
        {
        case godot::MODULE_INITIALIZATION_LEVEL_CORE:
        {
        }
        break;
        case godot::MODULE_INITIALIZATION_LEVEL_SERVERS:
        {
        }
        break;
        case godot::MODULE_INITIALIZATION_LEVEL_SCENE:
        {
            godot::Engine::get_singleton()->unregister_singleton( "MFManager" );
            memdelete( mf_manager );
        }
        break;
        case godot::MODULE_INITIALIZATION_LEVEL_EDITOR:
        {
        }
        break;
        case godot::MODULE_INITIALIZATION_LEVEL_MAX:
        {
        }
        break;
        }
    }

} // namespace

extern "C"
{
    GDExtensionBool GDE_EXPORT mft_init( GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library,
                                         GDExtensionInitialization* r_initialization )
    {
        {
            godot::GDExtensionBinding::InitObject init_obj( p_get_proc_address, p_library, r_initialization );

            init_obj.register_initializer( on_initialize );
            init_obj.register_terminator( on_terminate );
            init_obj.set_minimum_library_initialization_level( godot::MODULE_INITIALIZATION_LEVEL_SCENE );

            return init_obj.init();
        }
    }
}
