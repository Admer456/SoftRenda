
#include <iostream>
#include <chrono>
using namespace std::chrono;

#include "SDL.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

constexpr int CENTER = SDL_WINDOWPOS_CENTERED;

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;

float windowWidth = 1024.0f;
float windowHeight = 1024.0f;

glm::vec3 viewOrigin{ 0.0f, 0.0f, 0.0f };
glm::vec3 viewAngles{ 0.0f, 0.0f, 0.0f };
glm::vec3 viewForward{ 1.0f, 0.0f, 0.0f };
glm::vec3 viewRight{ 0.0f, -1.0f, 0.0f };
glm::vec3 viewUp{ 0.0f, 0.0f, 1.0f };

float viewSpeed = 10.0f;

glm::mat4 projMatrix;
glm::mat4 viewMatrix;

// Takes points in [-1, 1] coordinates, will convert them to screen coords properly
void DrawLine( const float& x1, const float& y1, const float& x2, const float& y2 )
{
	const auto ntoz = []( const float& n )
	{
		return (n * 0.5f) + 0.5f;
	};

	// Transformed ones
	const float tx1 = ntoz( x1 ) * windowWidth;
	const float tx2 = ntoz( x2 ) * windowWidth;
	const float ty1 = (1.0f - ntoz( y1 )) * windowHeight;
	const float ty2 = (1.0f - ntoz( y2 )) * windowHeight;

	SDL_RenderDrawLine( renderer, tx1, ty1, tx2, ty2 );
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
			projMatrix * viewMatrix * glm::identity<glm::mat4>() * GetVec4From( verts[0] ),
			projMatrix * viewMatrix * glm::identity<glm::mat4>() * GetVec4From( verts[1] ),
			projMatrix * viewMatrix * glm::identity<glm::mat4>() * GetVec4From( verts[2] )
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

	viewRight = cross( viewForward, viewUp );

	viewMatrix = lookAt( viewOrigin, viewOrigin + viewForward, viewUp );
}

inline float crandom()
{
	return rand() / 32768.0f;
}

inline glm::vec3 randVec()
{
	return glm::vec3( crandom(), crandom(), crandom() ) * crandom() * 15.0f;
}

void RunFrame( const float& deltaTime, const UserCommands& uc )
{
	viewAngles.x += uc.mouseY * deltaTime * 80.0f;
	viewAngles.y += uc.mouseX * deltaTime * 80.0f;

	// Clamp the pitch
	if ( viewAngles.x > 89.0f )
		viewAngles.x = 89.0f;
	if ( viewAngles.x < -89.0f )
		viewAngles.x = -89.0f;

	// Offset the view position
	viewOrigin += uc.forward * viewForward * deltaTime * viewSpeed;
	viewOrigin += uc.right * viewRight * deltaTime * viewSpeed;
	viewOrigin += uc.up * viewUp * deltaTime * viewSpeed;
	
	SetupMatrices();

	// Clear the view
	SDL_SetRenderDrawColor( renderer, 0, 0, 0, 255 );
	SDL_RenderClear( renderer );
	
	// Draw some triangles
	SDL_SetRenderDrawColor( renderer, 255, 255, 255, 255 );
	static const Triangle tris[]
	{
		{ { { -1.0f, -1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 1.0f } } },
		{ { randVec(), randVec(), randVec() } },
		{ { randVec(), randVec(), randVec() } },
		{ { randVec(), randVec(), randVec() } },
		{ { randVec(), randVec(), randVec() } },
		{ { randVec(), randVec(), randVec() } },
		{ { randVec(), randVec(), randVec() } },
		{ { randVec(), randVec(), randVec() } },
		{ { randVec(), randVec(), randVec() } }
	};
	for ( const Triangle& tri : tris )
	{
		tri.Draw();
	}

	{
		// Top view
		// Forward = red
		SDL_SetRenderDrawColor( renderer, 255, 100, 100, 255 );
		DrawLine( 0.0f, 0.0f, viewForward.x * 0.1f, viewForward.y * 0.1f );
		// Right = green
		SDL_SetRenderDrawColor( renderer, 100, 255, 100, 255 );
		DrawLine( 0.0f, 0.0f, viewRight.x * 0.1f, viewRight.y * 0.1f );
		// Up = blue
		SDL_SetRenderDrawColor( renderer, 100, 100, 255, 255 );
		DrawLine( 0.0f, 0.0f, viewUp.x * 0.1f, viewUp.y * 0.1f );

		// Side view
		// Forward = red
		SDL_SetRenderDrawColor( renderer, 255, 100, 100, 255 );
		DrawLine( 0.3f, 0.0f, 0.3f + viewForward.x * 0.1f, viewForward.z * 0.1f );
		// Right = green
		SDL_SetRenderDrawColor( renderer, 100, 255, 100, 255 );
		DrawLine( 0.3f, 0.0f, 0.3f + viewRight.x * 0.1f, viewRight.z * 0.1f );
		// Up = blue
		SDL_SetRenderDrawColor( renderer, 100, 100, 255, 255 );
		DrawLine( 0.3f, 0.0f, 0.3f + viewUp.x * 0.1f, viewUp.z * 0.1f );
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
	}

	SDL_Quit();

	return 0;
}
