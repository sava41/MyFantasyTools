
#pragma once

#include "mf_level.h"
#include "mf_view_resources.h"

#include <cassert>
#include <memory>
#include <queue>
#include <string>
#include <unordered_set>
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

        // Load the level file and initialise all view metadata.
        // Image data is NOT loaded here; call set_current_view() to trigger loading.
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

        // Call when the player transitions to a new view node.
        // Loads image data for all views within MAX_LOADED_HOPS hops of
        // view_index, and unloads data for views further away.
        void set_current_view( int view_index )
        {
            assert( view_index >= 0 && view_index < static_cast<int>( m_views.size() ) );

            const std::unordered_set<int> in_range = get_views_within_hops( view_index, MAX_LOADED_HOPS );

            for( int i = 0; i < static_cast<int>( m_views.size() ); ++i )
            {
                if( !in_range.count( i ) )
                {
                    m_views[i]->unload_image_data();
                }
                else if( !m_views[i]->is_data_loaded() )
                {
                    m_views[i]->load_image_data();
                }
            }

            m_current_view = view_index;
        }

        int get_current_view() const { return m_current_view; }

        int get_num_views() const { return static_cast<int>( m_views.size() ); }

        std::unique_ptr<ViewResourcesImpl>& get_view( int viewIndex )
        {
            assert( viewIndex >= 0 && viewIndex < get_num_views() );
            return m_views[viewIndex];
        }

      private:
        // BFS from start collecting all view indices reachable within max_hops.
        std::unordered_set<int> get_views_within_hops( int start, int max_hops ) const
        {
            std::unordered_set<int> visited;
            std::queue<std::pair<int, int>> queue; // {view_index, depth}
            queue.push( { start, 0 } );
            visited.insert( start );

            while( !queue.empty() )
            {
                auto [idx, depth] = queue.front();
                queue.pop();

                if( depth >= max_hops )
                    continue;

                for( int adj : Level::get_adjacent_views( idx ) )
                {
                    if( !visited.count( adj ) )
                    {
                        visited.insert( adj );
                        queue.push( { adj, depth + 1 } );
                    }
                }
            }

            return visited;
        }

        static constexpr int MAX_LOADED_HOPS = 2;

        std::vector<std::unique_ptr<ViewResourcesImpl>> m_views;
        int                                             m_current_view = -1;
    };

} // namespace mft
