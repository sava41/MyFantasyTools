

#include "io.h"
#include "jxl.h"

#include <ImfArray.h>
#include <ImfChannelList.h>
#include <ImfFrameBuffer.h>
#include <ImfInputFile.h>
#include <ImfRgbaFile.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <sstream>
#include <string>
#include <vector>

namespace mft
{
    namespace py = pybind11;

    bool save_jxl( const uint32_t xsize, const uint32_t ysize, const uint32_t channels,
                   const py::array_t<float, py::array::c_style | py::array::forcecast>& pixel_data, const std::string& path )
    {
        auto pixel_data_raw = pixel_data.unchecked<>();

        if( pixel_data_raw.size() != xsize * ysize * channels )
        {
            return false;
        }

        std::vector<char> compressed;

        if( !jxl::encode_oneshot( xsize, ysize, channels, static_cast<const void*>( pixel_data_raw.data( 0, 0, 0 ) ), compressed ) )
        {
            return false;
        }

        if( !write_binary( path, compressed ) )
        {
            return false;
        }

        return true;
    }

    bool exr_to_jxl( const std::string& input_file, const std::string& output_file )
    {
        Imf::InputFile file( input_file.c_str() );

        Imath::Box2i dw = file.header().dataWindow();
        int width       = dw.max.x - dw.min.x + 1;
        int height      = dw.max.y - dw.min.y + 1;

        // Check if this is a BW (single channel) EXR
        const Imf::ChannelList& channels = file.header().channels();
        int num_channels                 = 0;
        std::string channel_name;
        for( Imf::ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i )
        {
            channel_name = i.name();
            num_channels++;
        }

        std::vector<float> pixels;

        if( num_channels == 1 )
        {
            pixels.reserve( width * height );

            Imf::FrameBuffer frameBuffer;
            frameBuffer.insert( channel_name.c_str(),
                                Imf::Slice( Imf::FLOAT, (char*)pixels.data() - dw.min.x * sizeof( float ) - dw.min.y * width * sizeof( float ), sizeof( float ),
                                            width * sizeof( float ) ) );

            file.setFrameBuffer( frameBuffer );
            file.readPixels( dw.min.y, dw.max.y );
        }
        else if( num_channels == 3 )
        {
            pixels.reserve( width * height * 3 );

            Imf::FrameBuffer frameBuffer;
            frameBuffer.insert( "R", Imf::Slice( Imf::FLOAT, (char*)pixels.data() - dw.min.x * 3 * sizeof( float ) - dw.min.y * width * 3 * sizeof( float ),
                                                 3 * sizeof( float ), width * 3 * sizeof( float ) ) );
            frameBuffer.insert( "G",
                                Imf::Slice( Imf::FLOAT, (char*)( pixels.data() + 1 ) - dw.min.x * 3 * sizeof( float ) - dw.min.y * width * 3 * sizeof( float ),
                                            3 * sizeof( float ), width * 3 * sizeof( float ) ) );
            frameBuffer.insert( "B",
                                Imf::Slice( Imf::FLOAT, (char*)( pixels.data() + 2 ) - dw.min.x * 3 * sizeof( float ) - dw.min.y * width * 3 * sizeof( float ),
                                            3 * sizeof( float ), width * 3 * sizeof( float ) ) );

            file.setFrameBuffer( frameBuffer );
            file.readPixels( dw.min.y, dw.max.y );
        }
        else
        {
            return false;
        }

        std::vector<char> compressed;

        if( !jxl::encode_oneshot( width, height, num_channels, pixels.data(), compressed ) )
        {
            return false;
        }

        if( !write_binary( output_file, compressed ) )
        {
            return false;
        }

        return true;
    }

#define STRINGIFY( x ) #x
#define MACRO_STRINGIFY( x ) STRINGIFY( x )

    PYBIND11_MODULE( mftools, m )
    {
        m.doc() = R"pbdoc(
            Pybind11 example plugin
            -----------------------

            .. currentmodule:: mftools

            .. autosummary::
            :toctree: _generate

            save_jxl
        )pbdoc";

        m.def( "save_jxl", &save_jxl, pybind11::call_guard<pybind11::gil_scoped_release>(), R"pbdoc(
            Save np array to jxl

            saves a an np array as a jxl image
        )pbdoc" );

        m.def( "exr_to_jxl", &exr_to_jxl, pybind11::call_guard<pybind11::gil_scoped_release>(), R"pbdoc(
            Convert EXR to JXL

            Converts an OpenEXR file to JPEG XL format
        )pbdoc" );

#ifdef VERSION_INFO
        m.attr( "__version__" ) = MACRO_STRINGIFY( VERSION_INFO );
#else
        m.attr( "__version__" ) = "dev";
#endif
    }

} // namespace mft