
#pragma once

#include "mf_level.h"
#include "mf_view_resources.h"

#include <array>
#include <cassert>
#include <memory>
#include <string>
#include <vector>

namespace mft
{

    class ViewResources;

    template <typename ViewResourcesImpl>
    class ViewLevelManager : public Level
    {
      public:
        static_assert( std::is_base_of_v<ViewResources, ViewResourcesImpl>, "ViewResourcesImpl must inherit from ViewResources" );

        ViewLevelManager()  = default;
        ~ViewLevelManager() = default;

        bool load_data( const std::string& path )
        {
            bool ret = true;

            ret = ret && Level::load_level( path );

            m_views.clear();
            m_views.reserve( Level::get_num_views() );
            for( size_t i = 0; i < Level::get_num_views(); ++i )
            {
                m_views.push_back( std::make_unique<ViewResourcesImpl>() );
            }

            ret = ret && Level::load_views( reinterpret_cast<std::vector<std::unique_ptr<ViewResources>>&>( m_views ) );

            return ret;
        }

        int get_num_views() const
        {
            return m_views.size();
        }

        std::unique_ptr<ViewResourcesImpl>& get_view( int viewIndex )
        {
            assert( viewIndex >= 0 && viewIndex < get_num_views() );

            return m_views[viewIndex];
        }

      private:
        std::vector<std::unique_ptr<ViewResourcesImpl>> m_views;
    };

} // namespace mft
