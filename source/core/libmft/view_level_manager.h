
#pragma once

#include "level.h"
#include "view_data.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace mft
{

    class ViewData;

    template <typename ViewDataImpl>
    class ViewLevelManager : Level
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

            ret = ret && Level::load_views( m_views );

            return ret;
        }

        int get_num_views() const
        {
            return m_views.size();
        }

        ViewDataImpl& get_view( int viewIndex )
        {
            if( 0 > viewIndex || viewIndex >= get_views_length() )
                return nullptr;

            return m_views[viewIndex];
        }

      private:
        std::vector<std::unique_ptr<ViewDataImpl>> m_views;
    };

} // namespace mft
