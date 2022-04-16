
#include <iostream>
#include <chrono>
using namespace std::chrono;

#include "SDL.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "extern/adm-utils/src/Precompiled.hpp"

constexpr int CENTER = SDL_WINDOWPOS_CENTERED;

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;

float windowWidth = 1024.0f;
float windowHeight = 1024.0f;

glm::vec3 viewOrigin{ -80.0f, -10.0f, 40.0f };
glm::vec3 viewAngles{ 0.0f, 0.0f, 0.0f };
glm::vec3 viewForward{ 1.0f, 0.0f, 0.0f };
glm::vec3 viewRight{ 0.0f, -1.0f, 0.0f };
glm::vec3 viewUp{ 0.0f, 0.0f, 1.0f };

float viewSpeed = 30.0f;

glm::mat4 projMatrix;
glm::mat4 viewMatrix;

// Takes points in [-1, 1] coordinates, will convert them to screen coords properly
void DrawLine( const float& x1, const float& y1, const float& x2, const float& y2 )
{
	using glm::vec2;

	const auto ntoz = []( const float& n )
	{
		return (n * 0.5f) + 0.5f;
	};

	// @returns true if it should be displayed, false if not, outputs to p1 and p2
	const auto clipViewport = []( glm::vec2& p1, glm::vec2& p2, const glm::vec2& normal, const float& dist )
	{
		constexpr float Epsilon = 0.0001f;
		// Essentially dot(p, normal) + dist
		float d1 = normal.x * p1.x + normal.y * p1.y - dist;
		float d2 = normal.x * p2.x + normal.y * p2.y - dist;

		// All above the plane, render but don't compute intersection
		if ( d1 >= 0.0f && d2 >= 0.0f )
		{
			return true;
		}
		// All below plane, don't render
		if ( d1 < 0.0f && d2 < 0.0f )
		{
			return false;
		}

		const float t = d1 / (d1 - d2);
		const glm::vec2 intersect = p1 * (1.0f - t) + p2 * t;

		if ( d1 < d2 )
			p1 = intersect;
		else
			p2 = intersect;

		return true;
	};

	// Transformed ones
	vec2 t1 = { ntoz( x1 ) * windowWidth, (1.0f - ntoz( y1 )) * windowHeight };
	vec2 t2 = { ntoz( x2 ) * windowWidth, (1.0f - ntoz( y2 )) * windowHeight };

	const vec2 Up = { 0.0f, 1.0f };
	const vec2 Down = { 0.0f, -1.0f };
	const vec2 Left = { -1.0f, 0.0f };
	const vec2 Right = { 1.0f, 0.0f };

	if ( !clipViewport( t1, t2, Up, windowHeight * 0.02f ) )
		return;
	
	if ( !clipViewport( t1, t2, Right, windowWidth * 0.02f ) )
		return;
	
	if ( !clipViewport( t1, t2, Left, -windowWidth * 0.98f ) )
		return;
	
	if ( !clipViewport( t1, t2, Down, -windowHeight * 0.98f ) )
		return;

	SDL_RenderDrawLineF( renderer, t1.x, t1.y, t2.x, t2.y );
}

inline glm::vec4 GetVec4From( const glm::vec3& v )
{
	return glm::vec4( v, 1.0f );
}

struct Triangle
{
	void Draw() const
	{
		const glm::vec4 transformed[3]
		{
			projMatrix * viewMatrix * GetVec4From( verts[0] ),
			projMatrix * viewMatrix * GetVec4From( verts[1] ),
			projMatrix * viewMatrix * GetVec4From( verts[2] )
		};
		
		for ( int i = 0; i < 3; i++ )
		{
			const int next = (i + 1) % 3;
			DrawLine( 
				transformed[i].x / transformed[i].w, 
				transformed[i].y / transformed[i].w,
				transformed[next].x / transformed[next].w, 
				transformed[next].y / transformed[next].w );
		}
	}

	glm::vec3 verts[3];
};

void DrawLine3D( const adm::Vec3& a, const adm::Vec3& b )
{
	glm::vec4 at = projMatrix * viewMatrix * glm::vec4{ a.x, a.y, a.z, 1.0f };
	glm::vec4 bt = projMatrix * viewMatrix * glm::vec4{ b.x, b.y, b.z, 1.0f };

	// Need better camera clipping, but this'll do

	if ( at.w <= 0.0f )
		at.w = 0.000000001f;

	if ( bt.w <= 0.0f )
		bt.w = 0.000000001f;

	DrawLine( at.x / at.w, at.y / at.w,
		bt.x / bt.w, bt.y / bt.w );
}

void DrawPolygon( const adm::Polygon& polygon, bool drawNormal = false )
{
	// To draw the normal
	const adm::Vec3 origin = polygon.GetOrigin();
	const adm::Plane plane = polygon.GetPlane();
	const adm::Vec3 originAndNormal = origin + plane.GetNormal().Normalized() * 1.5f;
	// To draw the lines
	const auto& verts = polygon.vertices;

	for ( size_t i = 0U; i < verts.size(); i++ )
	{
		// Loopback index
		const size_t j = (i + 1) % verts.size();
		DrawLine3D( verts[i], verts[j] );
	}
	
	if ( !drawNormal )
	{
		return;
	}

	// Light blue for the normals
	SDL_SetRenderDrawColor( renderer, 128, 192, 255, 255 );
	DrawLine3D( origin, originAndNormal );
}

struct UserCommands
{
	enum Flags
	{
		Quit = 1,
		SpeedModifier = 2,
		LeftMouseButton = 4,
		RightMouseButton = 8
	};

	int flags{ 0 };

	float forward{ 0.0f };
	float right{ 0.0f };
	float up{ 0.0f };

	float mouseX{ 0.0f };
	float mouseY{ 0.0f };
};

UserCommands GenerateUserCommands()
{
	// Before we do all that, let's also update the window info
	{
		int w, h;
		SDL_GetWindowSize( window, &w, &h );
		windowWidth = w;
		windowHeight = h;
	}

	UserCommands uc;

	SDL_Event e;
	while ( SDL_PollEvent( &e ) )
	{
		if ( e.type == SDL_QUIT )
		{
			uc.flags |= UserCommands::Quit;
		}

		else if ( e.type == SDL_MOUSEBUTTONDOWN )
		{
			if ( e.button.button == SDL_BUTTON_LEFT )
			{
				uc.flags |= UserCommands::LeftMouseButton;
			}
			if ( e.button.button == SDL_BUTTON_RIGHT )
			{
				uc.flags |= UserCommands::RightMouseButton;
			}
		}
	}

	const auto* states = SDL_GetKeyboardState( nullptr );
	if ( states[SDL_SCANCODE_W] )
	{
		uc.forward += 1.0f;
	}
	if ( states[SDL_SCANCODE_S] )
	{
		uc.forward -= 1.0f;
	}
	if ( states[SDL_SCANCODE_A] )
	{
		uc.right -= 1.0f;
	}
	if ( states[SDL_SCANCODE_D] )
	{
		uc.right += 1.0f;
	}
	if ( states[SDL_SCANCODE_LSHIFT] )
	{
		uc.flags |= UserCommands::SpeedModifier;
	}

	int x, y;
	SDL_GetRelativeMouseState( &x, &y );
	uc.mouseX = x;
	uc.mouseY = y;

	return uc;
}

void SetupMatrices()
{
	using namespace glm;

	projMatrix = perspective( 90.0f, windowWidth / windowHeight, 0.01f, 1000.0f );

	// Spherical coords
	const vec3 angles = radians( viewAngles );

	const float cosPitch = cos( angles.x );
	const float sinPitch = sin( angles.x );
	const float cosYaw = cos( angles.y );
	const float sinYaw = sin( angles.y );
	const float cosRoll = cos( angles.z );
	const float sinRoll = sin( angles.z );

	viewForward = 
	{
		cosYaw * cosPitch,
		-sinYaw * cosPitch,
		-sinPitch
	};

	viewUp =
	{
		(cosRoll * sinPitch * cosYaw) + (-sinRoll * -sinYaw),
		(cosRoll * -sinPitch * sinYaw) + (-sinRoll * cosYaw),
		cosPitch * cosRoll
	};
	
	viewRight = normalize( cross( viewForward, viewUp ) );

	// glm::lookAt does this but in a slightly more convoluted way
	// So let's just do it ourselves
	viewMatrix[0][0] = viewRight.x;
	viewMatrix[1][0] = viewRight.y;
	viewMatrix[2][0] = viewRight.z;

	viewMatrix[0][1] = viewUp.x;
	viewMatrix[1][1] = viewUp.y;
	viewMatrix[2][1] = viewUp.z;

	viewMatrix[0][2] = -viewForward.x;
	viewMatrix[1][2] = -viewForward.y;
	viewMatrix[2][2] = -viewForward.z;

	viewMatrix[3][0] = -glm::dot( viewRight, viewOrigin );
	viewMatrix[3][1] = -glm::dot( viewUp, viewOrigin );
	viewMatrix[3][2] = glm::dot( viewForward, viewOrigin );

	viewMatrix[0][3] = 1.0f;
	viewMatrix[1][3] = 1.0f;
	viewMatrix[2][3] = 1.0f;
	viewMatrix[3][3] = 1.0f;
}

inline float crandom()
{
	return rand() / 32768.0f;
}

inline glm::vec3 randVec()
{
	return glm::vec3( crandom(), crandom(), crandom() ) * crandom() * 15.0f;
}

void TestPolygonIntersection( const float& time )
{
	using namespace adm;

	static Plane planes[] =
	{
		Plane( Vec3( 20.0f, 40.0f, 100.0f ).Normalized(), 2.0f),
		Plane( Vec3::Right, 12.0f ),
		Plane( -Vec3::Right, 6.0f ),
		Plane( -Vec3::Up, 1.0f ),
		Plane( Vec3::Forward, 10.0f ),
		Plane( -Vec3::Forward, 9.0f )
	};

	planes[0].SetNormal( (Vec3::Up * 4.0f + Vec3( std::sin(time), std::cos(time * 0.4f), 0.0f) ).Normalized() );
	planes[1].SetNormal( (Vec3::Right * 12.0f + Vec3( std::sin(time * 2.0f), std::cos(time * 0.87f), 0.0f) ).Normalized() );

	// Blue-ish for the planes
	//SDL_SetRenderDrawColor( renderer, 100, 100, 255, 255 );
	Polygon polygons[std::size( planes )];
	for ( size_t i = 0U; i < std::size( planes ); i++ )
	{
		polygons[i] = Polygon(planes[i], 20.0f);
		DrawPolygon( polygons[i] );
	}

	// For every huge polygon generated from the planes
	for ( int i = 0; i < std::size( polygons ); i++ )
	{
		// Clip against every other plane and modify the polygon
		for ( int j = 0; j < std::size( planes ); j++ )
		{
			// No need to clip against the same plane
			if ( i == j )
			{
				continue;
			}

			auto result = polygons[i].Split( planes[j] );
			if ( result.didIntersect && result.back.has_value() )
			{
				// Modify the polygon we started off from
				polygons[i] = result.back.value();
				// Red for the intersecting polygons
				SDL_SetRenderDrawColor( renderer, 255, 0, 0, 255 );
				DrawPolygon( polygons[i] );
			}
		}

		// Orange for the final polygons
		SDL_SetRenderDrawColor( renderer, 255, 192, 64, 255 );
		DrawPolygon( polygons[i], true );
	}
}

void RunFrame( const float& deltaTime, const UserCommands& uc )
{
	static float time = 0.0f;
	time += deltaTime;

	viewAngles.x += uc.mouseY * deltaTime * 2.0f;
	viewAngles.y += uc.mouseX * deltaTime * 2.0f;
	//viewAngles.z = std::sinf( time ) * 15.0f;

	// Clamp the pitch
	if ( viewAngles.x > 89.9f )
		viewAngles.x = 89.9f;
	if ( viewAngles.x < -89.9f )
		viewAngles.x = -89.9f;

	// Offset the view position
	viewOrigin += uc.forward * viewForward * deltaTime * viewSpeed;
	viewOrigin += uc.right * viewRight * deltaTime * viewSpeed;
	viewOrigin += uc.up * viewUp * deltaTime * viewSpeed;
	
	SetupMatrices();

	// Clear the view
	SDL_SetRenderDrawColor( renderer, 0, 0, 0, 255 );
	SDL_RenderClear( renderer );
	
	// Draw some polygons
	SDL_SetRenderDrawColor( renderer, 255, 255, 255, 255 );

	TestPolygonIntersection( time );

	// Crosshair
	//if ( false )
	{
		using adm::Vec3;

		static glm::vec3 crosshairOrigin = viewOrigin;
		crosshairOrigin += ((viewOrigin + viewForward * 5.0f) - crosshairOrigin) * deltaTime * 20.0f;

		// This is kinda elegant I think :3c
		Vec3 crosshair = &crosshairOrigin.x;

		SDL_SetRenderDrawColor( renderer, 128, 255, 0, 255 );
		DrawLine3D( crosshair + Vec3::Up * 0.25f, crosshair - Vec3::Up * 0.25f );
		DrawLine3D( crosshair + Vec3::Right * 0.25f, crosshair - Vec3::Right * 0.25f );
		DrawLine3D( crosshair + Vec3::Forward * 0.25f, crosshair - Vec3::Forward * 0.25f );
	}

	// View gizmos
	//if ( false )
	{
		// Top view
		// Forward = red
		SDL_SetRenderDrawColor( renderer, 255, 100, 100, 255 );
		DrawLine( 0.2f, 0.0f, 0.2f + viewForward.x * 0.05f, viewForward.y * 0.05f );
		// Right = green
		SDL_SetRenderDrawColor( renderer, 100, 255, 100, 255 );
		DrawLine( 0.2f, 0.0f, 0.2f + viewRight.x * 0.05f, viewRight.y * 0.05f );
		// Up = blue
		SDL_SetRenderDrawColor( renderer, 100, 100, 255, 255 );
		DrawLine( 0.2f, 0.0f, 0.2f + viewUp.x * 0.05f, viewUp.y * 0.05f );

		// Side view
		// Forward = red
		SDL_SetRenderDrawColor( renderer, 255, 100, 100, 255 );
		DrawLine( 0.3f, 0.0f, 0.3f + viewForward.x * 0.05f, viewForward.z * 0.05f );
		// Right = green
		SDL_SetRenderDrawColor( renderer, 100, 255, 100, 255 );
		DrawLine( 0.3f, 0.0f, 0.3f + viewRight.x * 0.05f, viewRight.z * 0.05f );
		// Up = blue
		SDL_SetRenderDrawColor( renderer, 100, 100, 255, 255 );
		DrawLine( 0.3f, 0.0f, 0.3f + viewUp.x * 0.05f, viewUp.z * 0.05f );
	}

	SDL_RenderPresent( renderer );
}

int main( int argc, char** argv )
{
	SDL_Init( SDL_INIT_VIDEO | SDL_INIT_EVENTS );

	window = SDL_CreateWindow( "SoftRenda", CENTER, CENTER, windowWidth, windowHeight, SDL_WINDOW_RESIZABLE );
	renderer = SDL_CreateRenderer( window, 0, SDL_RENDERER_SOFTWARE );
	SDL_SetRelativeMouseMode( SDL_TRUE );

	float deltaTime = 0.016f;
	while ( true )
	{
		auto tpStart = system_clock::now();

		auto uc = GenerateUserCommands();
		if ( uc.flags & UserCommands::Quit )
		{
			break;
		}

		RunFrame( deltaTime, uc );

		auto tpEnd = system_clock::now();
		deltaTime = duration_cast<microseconds>(tpEnd - tpStart).count() * 0.001f * 0.001f;

		if ( deltaTime < 1.0f / 60.0f )
		{
			float remainingTime = (1.0f / 60.0f) - deltaTime;
			std::this_thread::sleep_for( microseconds( size_t( remainingTime * 1000.0f * 1000.0f ) ) );
			deltaTime += remainingTime;
		}
	}

	SDL_Quit();

	return 0;
}
