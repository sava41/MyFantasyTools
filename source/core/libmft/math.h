#pragma once

#include "level_generated.h"

namespace mft
{
    inline float length( const data::Vec3& a )
    {
        return sqrt( a.x() * a.x() + a.y() * a.y() + a.z() * a.z() );
    };

    inline data::Vec3 add( const data::Vec3& a, const data::Vec3& b )
    {
        return { a.x() + b.x(), a.y() + b.y(), a.z() + b.z() };
    };

    inline data::Vec3 sub( const data::Vec3& a, const data::Vec3& b )
    {
        return { a.x() - b.x(), a.y() - b.y(), a.z() - b.z() };
    };

    inline data::Vec3 mul( const data::Vec3& a, const data::Vec3& b )
    {
        return { a.x() * b.x(), a.y() * b.y(), a.z() * b.z() };
    };

    inline data::Vec3 mul( const float& a, const data::Vec3& b )
    {
        return { a * b.x(), a * b.y(), a * b.z() };
    };

    inline float dot( const data::Vec3& a, const data::Vec3& b )
    {
        return a.x() * b.x() + a.y() * b.y() + a.z() * b.z();
    };

    inline data::Vec3 cross( const data::Vec3& a, const data::Vec3& b )
    {
        return { a.y() * b.z() - a.z() * b.y(), a.z() * b.x() - a.x() * b.z(), a.x() * b.y() - a.y() * b.x() };
    };

    // https://github.com/pmjoniak/GeometricTools/blob/master/GTEngine/Include/Mathematics/GteDistPointTriangleExact.h
    // https://www.geometrictools.com/Documentation/DistancePoint3Triangle3.pdf
    data::Vec3 closesPointOnTriangle( const data::Vec3& vert1, const data::Vec3& vert2, const data::Vec3& vert3, const data::Vec3& point )
    {
        data::Vec3 diff  = sub( point, vert1 );
        data::Vec3 edge0 = sub( vert2, vert1 );
        data::Vec3 edge1 = sub( vert3, vert1 );

        const float a00 = dot( edge0, edge0 );
        const float a01 = dot( edge0, edge1 );
        const float a11 = dot( edge1, edge1 );
        const float b0  = -dot( diff, edge0 );
        const float b1  = -dot( diff, edge1 );

        const float det = a00 + a11 - a01 * a01;
        float t0        = a01 * b1 - a11 * b0;
        float t1        = a01 * b0 - a00 * b1;

        if( t0 + t1 <= det )
        {
            if( t0 < 0.0 )
            {
                if( t1 < 0.0 ) // region 4
                {
                    if( b0 < 0.0 )
                    {
                        t1 = 0.0;
                        if( -b0 >= a00 ) // V1
                        {
                            t0 = 1.0;
                        }
                        else // E01
                        {
                            t0 = -b0 / a00;
                        }
                    }
                    else
                    {
                        t0 = 0.0;
                        if( b1 >= 0.0 ) // V0
                        {
                            t1 = 0.0;
                        }
                        else if( -b1 >= a11 ) // V2
                        {
                            t1 = 1.0;
                        }
                        else // E20
                        {
                            t1 = -b1 / a11;
                        }
                    }
                }
                else // region 3
                {
                    t0 = 0.0;
                    if( b1 >= 0.0 ) // V0
                    {
                        t1 = 0.0;
                    }
                    else if( -b1 >= a11 ) // V2
                    {
                        t1 = 1.0;
                    }
                    else // E20
                    {
                        t1 = -b1 / a11;
                    }
                }
            }
            else if( t1 < 0.0 ) // region 5
            {
                t1 = 0.0;
                if( b0 >= 0.0 ) // V0
                {
                    t0 = 0.0;
                }
                else if( -b0 >= a00 ) // V1
                {
                    t0 = 1.0;
                }
                else // E01
                {
                    t0 = -b0 / a00;
                }
            }
            else // region 0, interior
            {
                float invDet = 1.0 / det;
                t0 *= invDet;
                t1 *= invDet;
            }
        }
        else
        {
            float tmp0, tmp1, numer, denom;

            if( t0 < 0.0 ) // region 2
            {
                tmp0 = a01 + b0;
                tmp1 = a11 + b1;
                if( tmp1 > tmp0 )
                {
                    numer = tmp1 - tmp0;
                    denom = a00 - 2.0 * a01 + a11;
                    if( numer >= denom ) // V1
                    {
                        t0 = 1.0;
                        t1 = 0.0;
                    }
                    else // E12
                    {
                        t0 = numer / denom;
                        t1 = 1.0 - t0;
                    }
                }
                else
                {
                    t0 = 0.0;
                    if( tmp1 <= 0.0 ) // V2
                    {
                        t1 = 1.0;
                    }
                    else if( b1 >= 0.0 ) // V0
                    {
                        t1 = 0.0;
                    }
                    else // E20
                    {
                        t1 = -b1 / a11;
                    }
                }
            }
            else if( t1 < 0.0 ) // region 6
            {
                tmp0 = a01 + b1;
                tmp1 = a00 + b0;
                if( tmp1 > tmp0 )
                {
                    numer = tmp1 - tmp0;
                    denom = a00 - 2.0 * a01 + a11;
                    if( numer >= denom ) // V2
                    {
                        t1 = 1.0;
                        t0 = 0.0;
                    }
                    else // E12
                    {
                        t1 = numer / denom;
                        t0 = 1.0 - t1;
                    }
                }
                else
                {
                    t1 = 0.0;
                    if( tmp1 <= 0.0 ) // V1
                    {
                        t0 = 1.0;
                    }
                    else if( b0 >= 0.0 ) // V0
                    {
                        t0 = 0.0;
                    }
                    else // E01
                    {
                        t0 = -b0 / a00;
                    }
                }
            }
            else // region 1
            {
                numer = a11 + b1 - a01 - b0;
                if( numer <= 0.0 ) // V2
                {
                    t0 = 0.0;
                    t1 = 1.0;
                }
                else
                {
                    denom = a00 - 2.0 * a01 + a11;
                    if( numer >= denom ) // V1
                    {
                        t0 = 1.0;
                        t1 = 0.0;
                    }
                    else // 12
                    {
                        t0 = numer / denom;
                        t1 = 1.0 - t0;
                    }
                }
            }
        }

        return add( add( vert1, mul( t0, edge0 ) ), mul( t1, edge1 ) );
    };
} // namespace mft
