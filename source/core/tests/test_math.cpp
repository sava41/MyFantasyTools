#include <gtest/gtest.h>
#include "mf_math.h"

using namespace mft;

// Helper so tests read naturally
static data::Vec3 v3( float x, float y, float z )
{
    return data::Vec3( x, y, z );
}

// ---- length ----------------------------------------------------------------

TEST( Length, Zero )
{
    EXPECT_FLOAT_EQ( length( v3( 0, 0, 0 ) ), 0.0f );
}

TEST( Length, UnitX )
{
    EXPECT_FLOAT_EQ( length( v3( 1, 0, 0 ) ), 1.0f );
}

TEST( Length, ThreeFourFive )
{
    EXPECT_FLOAT_EQ( length( v3( 3, 4, 0 ) ), 5.0f );
}

TEST( Length, NegativeComponents )
{
    EXPECT_FLOAT_EQ( length( v3( -3, -4, 0 ) ), 5.0f );
}

// ---- add -------------------------------------------------------------------

TEST( Add, Basic )
{
    auto r = add( v3( 1, 2, 3 ), v3( 4, 5, 6 ) );
    EXPECT_FLOAT_EQ( r.x(), 5.0f );
    EXPECT_FLOAT_EQ( r.y(), 7.0f );
    EXPECT_FLOAT_EQ( r.z(), 9.0f );
}

TEST( Add, ZeroIdentity )
{
    auto a = v3( 3, -1, 2 );
    auto r = add( a, v3( 0, 0, 0 ) );
    EXPECT_FLOAT_EQ( r.x(), a.x() );
    EXPECT_FLOAT_EQ( r.y(), a.y() );
    EXPECT_FLOAT_EQ( r.z(), a.z() );
}

// ---- sub -------------------------------------------------------------------

TEST( Sub, Basic )
{
    auto r = sub( v3( 4, 5, 6 ), v3( 1, 2, 3 ) );
    EXPECT_FLOAT_EQ( r.x(), 3.0f );
    EXPECT_FLOAT_EQ( r.y(), 3.0f );
    EXPECT_FLOAT_EQ( r.z(), 3.0f );
}

TEST( Sub, SelfIsZero )
{
    auto a = v3( 3, -1, 2 );
    auto r = sub( a, a );
    EXPECT_FLOAT_EQ( r.x(), 0.0f );
    EXPECT_FLOAT_EQ( r.y(), 0.0f );
    EXPECT_FLOAT_EQ( r.z(), 0.0f );
}

// ---- mul -------------------------------------------------------------------

TEST( MulVecVec, ComponentWise )
{
    auto r = mul( v3( 2, 3, 4 ), v3( 5, 6, 7 ) );
    EXPECT_FLOAT_EQ( r.x(), 10.0f );
    EXPECT_FLOAT_EQ( r.y(), 18.0f );
    EXPECT_FLOAT_EQ( r.z(), 28.0f );
}

TEST( MulScalarVec, Double )
{
    auto r = mul( 2.0f, v3( 1, 2, 3 ) );
    EXPECT_FLOAT_EQ( r.x(), 2.0f );
    EXPECT_FLOAT_EQ( r.y(), 4.0f );
    EXPECT_FLOAT_EQ( r.z(), 6.0f );
}

TEST( MulScalarVec, Zero )
{
    auto r = mul( 0.0f, v3( 1, 2, 3 ) );
    EXPECT_FLOAT_EQ( r.x(), 0.0f );
    EXPECT_FLOAT_EQ( r.y(), 0.0f );
    EXPECT_FLOAT_EQ( r.z(), 0.0f );
}

// ---- dot -------------------------------------------------------------------

TEST( Dot, Orthogonal )
{
    EXPECT_FLOAT_EQ( dot( v3( 1, 0, 0 ), v3( 0, 1, 0 ) ), 0.0f );
}

TEST( Dot, Parallel )
{
    EXPECT_FLOAT_EQ( dot( v3( 1, 0, 0 ), v3( 3, 0, 0 ) ), 3.0f );
}

TEST( Dot, General )
{
    // 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32
    EXPECT_FLOAT_EQ( dot( v3( 1, 2, 3 ), v3( 4, 5, 6 ) ), 32.0f );
}

TEST( Dot, SelfIsSqLength )
{
    auto a = v3( 3, 4, 0 );
    EXPECT_FLOAT_EQ( dot( a, a ), 25.0f );
}

// ---- cross -----------------------------------------------------------------

TEST( Cross, StandardBasisXY )
{
    auto r = cross( v3( 1, 0, 0 ), v3( 0, 1, 0 ) );
    EXPECT_FLOAT_EQ( r.x(), 0.0f );
    EXPECT_FLOAT_EQ( r.y(), 0.0f );
    EXPECT_FLOAT_EQ( r.z(), 1.0f );
}

TEST( Cross, StandardBasisYZ )
{
    auto r = cross( v3( 0, 1, 0 ), v3( 0, 0, 1 ) );
    EXPECT_FLOAT_EQ( r.x(), 1.0f );
    EXPECT_FLOAT_EQ( r.y(), 0.0f );
    EXPECT_FLOAT_EQ( r.z(), 0.0f );
}

TEST( Cross, ParallelIsZero )
{
    auto r = cross( v3( 1, 0, 0 ), v3( 2, 0, 0 ) );
    EXPECT_FLOAT_EQ( r.x(), 0.0f );
    EXPECT_FLOAT_EQ( r.y(), 0.0f );
    EXPECT_FLOAT_EQ( r.z(), 0.0f );
}

TEST( Cross, Anticommutative )
{
    auto ab = cross( v3( 1, 2, 3 ), v3( 4, 5, 6 ) );
    auto ba = cross( v3( 4, 5, 6 ), v3( 1, 2, 3 ) );
    EXPECT_FLOAT_EQ( ab.x(), -ba.x() );
    EXPECT_FLOAT_EQ( ab.y(), -ba.y() );
    EXPECT_FLOAT_EQ( ab.z(), -ba.z() );
}

TEST( Cross, OrthogonalToBothInputs )
{
    auto a = v3( 1, 2, 3 );
    auto b = v3( 4, 5, 6 );
    auto c = cross( a, b );
    EXPECT_NEAR( dot( c, a ), 0.0f, 1e-5f );
    EXPECT_NEAR( dot( c, b ), 0.0f, 1e-5f );
}

// ---- closesPointOnTriangle -------------------------------------------------
// Triangle used in all cases: XY-plane, verts (0,0,0), (1,0,0), (0,1,0)

TEST( ClosestPoint, AtOriginVertex )
{
    // Query point == vert1 → should snap to that vertex
    auto r = closesPointOnTriangle( v3( 0, 0, 0 ), v3( 1, 0, 0 ), v3( 0, 1, 0 ), v3( 0, 0, 0 ) );
    EXPECT_FLOAT_EQ( r.x(), 0.0f );
    EXPECT_FLOAT_EQ( r.y(), 0.0f );
    EXPECT_FLOAT_EQ( r.z(), 0.0f );
}

TEST( ClosestPoint, FarAlongXAxisSnapsToVertex2 )
{
    // Far beyond the (1,0,0) vertex along X → nearest point is (1,0,0)
    auto r = closesPointOnTriangle( v3( 0, 0, 0 ), v3( 1, 0, 0 ), v3( 0, 1, 0 ), v3( 10, 0, 0 ) );
    EXPECT_FLOAT_EQ( r.x(), 1.0f );
    EXPECT_FLOAT_EQ( r.y(), 0.0f );
    EXPECT_FLOAT_EQ( r.z(), 0.0f );
}

TEST( ClosestPoint, BehindNegativeXSnapsToOrigin )
{
    auto r = closesPointOnTriangle( v3( 0, 0, 0 ), v3( 1, 0, 0 ), v3( 0, 1, 0 ), v3( -5, 0, 0 ) );
    EXPECT_FLOAT_EQ( r.x(), 0.0f );
    EXPECT_FLOAT_EQ( r.y(), 0.0f );
    EXPECT_FLOAT_EQ( r.z(), 0.0f );
}

TEST( ClosestPoint, AboveInteriorLiesInTrianglePlane )
{
    // Point above interior of triangle — result must lie in the XY plane (z == 0)
    // and within the triangle bounds (x >= 0, y >= 0, x+y <= 1)
    auto r = closesPointOnTriangle( v3( 0, 0, 0 ), v3( 1, 0, 0 ), v3( 0, 1, 0 ), v3( 0.1f, 0.1f, 5.0f ) );
    EXPECT_NEAR( r.z(), 0.0f, 1e-5f );
    EXPECT_GE( r.x(), 0.0f );
    EXPECT_GE( r.y(), 0.0f );
    EXPECT_LE( r.x() + r.y(), 1.0f + 1e-5f );
}

TEST( ClosestPoint, OnHypotenuseEdge )
{
    // (1,1,0) is equidistant from both non-origin vertices → midpoint of hypotenuse
    auto r = closesPointOnTriangle( v3( 0, 0, 0 ), v3( 1, 0, 0 ), v3( 0, 1, 0 ), v3( 1, 1, 0 ) );
    EXPECT_NEAR( r.x(), 0.5f, 1e-5f );
    EXPECT_NEAR( r.y(), 0.5f, 1e-5f );
    EXPECT_NEAR( r.z(), 0.0f, 1e-5f );
}
