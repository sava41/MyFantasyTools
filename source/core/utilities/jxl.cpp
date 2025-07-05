#include "jxl.h"

#include "io.h"

#include <assert.h>
#include <jxl/decode_cxx.h>
#include <jxl/encode_cxx.h>
#include <jxl/resizable_parallel_runner_cxx.h>
#include <jxl/thread_parallel_runner_cxx.h>

namespace mft::jxl
{

    bool encode_oneshot( const uint32_t xsize, const uint32_t ysize, const uint32_t channels, const void* pixelData, std::vector<char>& compressed )
    {
        auto enc    = JxlEncoderMake( nullptr );
        auto runner = JxlThreadParallelRunnerMake( nullptr, JxlThreadParallelRunnerDefaultNumWorkerThreads() );

        if( JXL_ENC_SUCCESS != JxlEncoderSetParallelRunner( enc.get(), JxlThreadParallelRunner, runner.get() ) )
        {
            fprintf( stderr, "JxlEncoderSetParallelRunner failed\n" );
            return false;
        }

        assert( channels == 1 || channels == 3 );

        JxlPixelFormat pixelFormat = { channels, JXL_TYPE_FLOAT, JXL_NATIVE_ENDIAN, 0 };

        JxlBasicInfo basicInfo;
        JxlEncoderInitBasicInfo( &basicInfo );
        basicInfo.xsize                    = xsize;
        basicInfo.ysize                    = ysize;
        basicInfo.bits_per_sample          = 32;
        basicInfo.exponent_bits_per_sample = 8;
        basicInfo.uses_original_profile    = JXL_FALSE;
        basicInfo.num_color_channels       = channels;
        if( JXL_ENC_SUCCESS != JxlEncoderSetBasicInfo( enc.get(), &basicInfo ) )
        {
            fprintf( stderr, "JxlEncoderSetBasicInfo failed\n" );
            return false;
        }

        JxlColorEncoding colorEncoding = {};
        JxlColorEncodingSetToLinearSRGB( &colorEncoding, channels < 3 );
        if( JXL_ENC_SUCCESS != JxlEncoderSetColorEncoding( enc.get(), &colorEncoding ) )
        {
            fprintf( stderr, "JxlEncoderSetColorEncoding failed\n" );
            return false;
        }

        JxlEncoderFrameSettings* frameSettings = JxlEncoderFrameSettingsCreate( enc.get(), nullptr );

        // JxlEncoderSetFrameLossless(frameSettings, JXL_FALSE);

        if( JXL_ENC_SUCCESS != JxlEncoderAddImageFrame( frameSettings, &pixelFormat, pixelData, sizeof( float ) * xsize * ysize * channels ) )
        {
            fprintf( stderr, "JxlEncoderAddImageFrame failed\n" );
            return false;
        }
        JxlEncoderCloseInput( enc.get() );

        compressed.resize( 16384 );
        uint8_t* nextOut = reinterpret_cast<uint8_t*>( compressed.data() );
        size_t availOut  = static_cast<size_t>( compressed.size() );

        JxlEncoderStatus result = JXL_ENC_NEED_MORE_OUTPUT;
        while( result == JXL_ENC_NEED_MORE_OUTPUT )
        {
            result = JxlEncoderProcessOutput( enc.get(), &nextOut, &availOut );

            if( result == JXL_ENC_NEED_MORE_OUTPUT )
            {
                size_t offset = nextOut - reinterpret_cast<uint8_t*>( compressed.data() );
                compressed.resize( compressed.size() * 2 );
                nextOut  = reinterpret_cast<uint8_t*>( compressed.data() ) + offset;
                availOut = static_cast<size_t>( compressed.size() ) - offset;
            }
        }
        compressed.resize( nextOut - reinterpret_cast<uint8_t*>( compressed.data() ) );

        if( JXL_ENC_SUCCESS != result )
        {
            fprintf( stderr, "JxlEncoderProcessOutput failed\n" );
            return false;
        }

        return true;
    }

    bool decode_oneshot( std::vector<char>& compressed, const buffer_allocator& allocator )
    {
        // Multi-threaded parallel runner.
        auto runner = JxlResizableParallelRunnerMake( nullptr );

        auto dec = JxlDecoderMake( nullptr );
        if( JXL_DEC_SUCCESS != JxlDecoderSubscribeEvents( dec.get(), JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING | JXL_DEC_FULL_IMAGE ) )
        {
            fprintf( stderr, "JxlDecoderSubscribeEvents failed\n" );
            return false;
        }

        if( JXL_DEC_SUCCESS != JxlDecoderSetParallelRunner( dec.get(), JxlResizableParallelRunner, runner.get() ) )
        {
            fprintf( stderr, "JxlDecoderSetParallelRunner failed\n" );
            return false;
        }

        JxlBasicInfo info;

        JxlDecoderSetInput( dec.get(), reinterpret_cast<uint8_t*>( compressed.data() ), compressed.size() );
        JxlDecoderCloseInput( dec.get() );

        for( ;; )
        {
            JxlDecoderStatus status = JxlDecoderProcessInput( dec.get() );

            if( status == JXL_DEC_ERROR )
            {
                fprintf( stderr, "Decoder error\n" );
                return false;
            }
            else if( status == JXL_DEC_NEED_MORE_INPUT )
            {
                fprintf( stderr, "Error, already provided all input\n" );
                return false;
            }
            else if( status == JXL_DEC_BASIC_INFO )
            {
                if( JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo( dec.get(), &info ) )
                {
                    fprintf( stderr, "JxlDecoderGetBasicInfo failed\n" );
                    return false;
                }

                JxlResizableParallelRunnerSetThreads( runner.get(), JxlResizableParallelRunnerSuggestThreads( info.xsize, info.ysize ) );
            }
            else if( status == JXL_DEC_COLOR_ENCODING )
            {
                // Get the ICC color profile of the pixel data
                size_t icc_size;
                if( JXL_DEC_SUCCESS != JxlDecoderGetICCProfileSize( dec.get(), JXL_COLOR_PROFILE_TARGET_DATA, &icc_size ) )
                {
                    fprintf( stderr, "JxlDecoderGetICCProfileSize failed\n" );
                    return false;
                }
                std::vector<uint8_t> iccProfile( icc_size );
                if( JXL_DEC_SUCCESS != JxlDecoderGetColorAsICCProfile( dec.get(), JXL_COLOR_PROFILE_TARGET_DATA, iccProfile.data(), iccProfile.size() ) )
                {
                    fprintf( stderr, "JxlDecoderGetColorAsICCProfile failed\n" );
                    return false;
                }
            }
            else if( status == JXL_DEC_NEED_IMAGE_OUT_BUFFER )
            {
                size_t bufferSize;
                JxlPixelFormat format = { info.num_color_channels, JXL_TYPE_FLOAT, JXL_NATIVE_ENDIAN, 0 };
                if( JXL_DEC_SUCCESS != JxlDecoderImageOutBufferSize( dec.get(), &format, &bufferSize ) )
                {
                    fprintf( stderr, "JxlDecoderImageOutBufferSize failed\n" );
                    return false;
                }

                void* pixelsBuffer = allocator( info.xsize, info.ysize, info.num_color_channels, bufferSize );

                if( pixelsBuffer == nullptr )
                {
                    return false;
                }

                if( JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer( dec.get(), &format, pixelsBuffer, bufferSize ) )
                {
                    fprintf( stderr, "JxlDecoderSetImageOutBuffer failed\n" );
                    return false;
                }
            }
            else if( status == JXL_DEC_FULL_IMAGE )
            {
                // Nothing to do. Do not yet return. If the image is an animation, more
                // full frames may be decoded. This example only keeps the last one.
            }
            else if( status == JXL_DEC_SUCCESS )
            {
                // All decoding successfully finished.
                // It's not required to call JxlDecoderReleaseInput(dec.get()) here since
                // the decoder will be destroyed.
                return true;
            }
            else
            {
                fprintf( stderr, "Unknown decoder status\n" );
                return false;
            }
        }
    }
} // namespace mft::jxl