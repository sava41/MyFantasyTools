
#pragma once

#include "mf_level.h"
#include "mf_view_data.h"

#include <array>
#include <cassert>
#include <memory>
#include <string>
#include <vector>

namespace mft
{

    class ViewData;

    template <typename ViewDataImpl>
    class ViewLevelManager : public Level
    {
      public:
        static_assert( std::is_base_of_v<ViewData, ViewDataImpl>, "ViewDataImpl must inherit from ViewData" );

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
                m_views.push_back( std::make_unique<ViewDataImpl>() );
            }

            ret = ret && Level::load_views( reinterpret_cast<std::vector<std::unique_ptr<ViewData>>&>( m_views ) );

            return ret;
        }

        int get_num_views() const
        {
            return m_views.size();
        }

        std::unique_ptr<ViewDataImpl>& get_view( int viewIndex )
        {
            assert( viewIndex >= 0 && viewIndex < get_num_views() );

            return m_views[viewIndex];
        }

      private:
        std::vector<std::unique_ptr<ViewDataImpl>> m_views;
    };

} // namespace mft
