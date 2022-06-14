
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

constexpr float viewSpeed = 100.0f;

glm::mat4 projMatrix;
glm::mat4 viewMatrix;

// Converts x from [-1,1] into [0,1]
template<typename T, typename OperationType = float>
constexpr T nto( const T& x )
{
	return (OperationType( 0.5 ) * x) + T( OperationType( 0.5 ) );
}

// Converts x from [0,1] into [-1,1]
template<typename T, typename OperationType = float>
constexpr T otn( const T& x )
{
	return (OperationType( 2.0 ) * x) - T( OperationType( 1.0 ) );
}

// Takes points in [-1, 1] coordinates, will convert them to screen coords properly
void DrawLine( const float& x1, const float& y1, const float& x2, const float& y2 )
{
	using glm::vec2;

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
	vec2 t1 = { nto( x1 ) * windowWidth, (1.0f - nto( y1 )) * windowHeight };
	vec2 t2 = { nto( x2 ) * windowWidth, (1.0f - nto( y2 )) * windowHeight };

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
		RightMouseButton = 8,
		Reload = 16
	};

	int flags{ 0 };

	float forward{ 0.0f };
	float right{ 0.0f };
	float up{ 0.0f };

	float mouseX{ 0.0f };
	float mouseY{ 0.0f };

	float windowMouseX{ 0.0f };
	float windowMouseY{ 0.0f };
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
	if ( states[SDL_SCANCODE_R] )
	{
		uc.flags |= UserCommands::Reload;
	}

	int x, y;
	SDL_GetRelativeMouseState( &x, &y );
	uc.mouseX = x;
	uc.mouseY = y;

	int mouseState = SDL_GetMouseState( &x, &y );
	uc.windowMouseX = x;
	uc.windowMouseY = y;

	if ( mouseState & SDL_BUTTON_LMASK )
	{
		uc.flags |= UserCommands::LeftMouseButton;
	}
	if ( mouseState & SDL_BUTTON_RMASK )
	{
		uc.flags |= UserCommands::RightMouseButton;
	}

	return uc;
}

glm::vec3 ScreenToWorldDirection( const glm::mat4& proj, const glm::mat4& view, const glm::vec2& screen, const glm::vec2& viewport )
{
	// Treat the position as 0,0,0 as we're only interested in the direction
	glm::mat4 viewModified = view;
	viewModified[3][0] = 0.0f;
	viewModified[3][1] = 0.0f;
	viewModified[3][2] = 0.0f;

	const glm::vec2 screenNdc = otn( glm::vec2( screen.x / viewport.x, screen.y / viewport.y ) );
	const glm::vec4 direction = glm::inverse( proj * viewModified ) * glm::vec4( screenNdc.x, -screenNdc.y, -1.0f, 1.0f );

	return glm::vec3( direction );
}

void DrawCross( const glm::vec3& pos )
{
	using adm::Vec3;

	// This is kinda elegant I think :3c
	Vec3 position = &pos.x;

	SDL_SetRenderDrawColor( renderer, 128, 255, 0, 255 );
	DrawLine3D( position + Vec3::Up * 0.25f, position - Vec3::Up * 0.25f );
	DrawLine3D( position + Vec3::Right * 0.25f, position - Vec3::Right * 0.25f );
	DrawLine3D( position + Vec3::Forward * 0.25f, position - Vec3::Forward * 0.25f );
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

namespace Map
{
	struct MapFace
	{
		adm::Plane plane;
		adm::Vec3 planeVerts[3];
		bool noDraw{ false };

		adm::Vec3 GetOrigin() const
		{
			return (planeVerts[0] + planeVerts[1] + planeVerts[2]) / 3.0f;
		}
	};

	std::vector<std::vector<MapFace>> Brushes;
	std::vector<adm::Polygon> Polygons;
	bool valveMapFormat = false;

	namespace Parsing
	{
		std::optional<std::array<float, 4U>> ParseBrushSideTexCoord( adm::Lexer& lex, adm::String& token )
		{
			std::array<float, 4U> texCoords;

			if ( !lex.Expect( "[" ) )
			{
				std::cout << "Expected a [, got a: " << lex.Next() << std::endl;
				token = "}";
				return {};
			}
			token = lex.Next();

			token = lex.Next();
			texCoords[0] = std::stof( token );
			token = lex.Next();
			texCoords[1] = std::stof( token );
			token = lex.Next();
			texCoords[2] = std::stof( token );
			token = lex.Next();
			texCoords[3] = std::stof( token );

			if ( !lex.Expect( "]" ) )
			{
				std::cout << "Expected a ], got a: " << lex.Next() << std::endl;
				token = "}";
				return {};
			}
			token = lex.Next();

			return texCoords;
		}

		// ( x y z )
		std::optional<adm::Vec3> ParseBrushSidePoint( adm::Lexer& lex, adm::String& token )
		{
			adm::Vec3 point;

			if ( !lex.Expect( "(" ) )
			{
				std::cout << "Expected a (, got a: " << lex.Next() << std::endl;
				token = "}";
				return {};
			}
			token = lex.Next();

			token = lex.Next();
			point.x = std::stof( token );
			token = lex.Next();
			point.y = std::stof( token );
			token = lex.Next();
			point.z = std::stof( token );

			if ( !lex.Expect( ")" ) )
			{
				std::cout << "Expected a ), got a: " << lex.Next() << std::endl;
				token = "}";
				return {};
			}
			token = lex.Next();

			return point;
		}

		// ( -68 76 -44 ) ( -68 0 52 ) ( -68 -64 52 ) __TB_empty [ 0 -1 0 0 ] [ 0 0 -1 0 ] 0 1 1
		// ( x1 y1 z1 ) ( x2 y2 z2 ) ( x3 y3 z3 ) texture_name [ ux uy uz offsetX ] [ vx vy vz offsetY ] rotation scaleX scaleY
		// Right now it only parses the plane (x1...z3) and generates polygones from dat
		void ParseBrushSide( adm::Lexer& lex, adm::String& token, std::vector<MapFace>& brush )
		{
			// We're done, the whole brush is parsed
			if ( lex.Expect( "}" ) )
			{
				token = lex.Next();
				return;
			}

			std::optional<adm::Vec3> points[3]
			{
				ParseBrushSidePoint( lex, token ),
				ParseBrushSidePoint( lex, token ),
				ParseBrushSidePoint( lex, token )
			};

			for ( const auto& p : points )
			{
				if ( !p.has_value() )
				{
					std::cout << "Failed to parse at least one of the verts" << std::endl;
					return;
				}
			}

			// texture_name
			token = lex.Next();
			if ( token.empty() )
			{
				std::cout << "Could not parse texture name" << std::endl;
				return;
			}

			adm::String textureName = token;

			// Texture coordinates
			if ( !ParseBrushSideTexCoord( lex, token ).has_value() )
			{
				std::cout << "Failed to parse the U texcoord" << std::endl;
				return;
			}
			if ( !ParseBrushSideTexCoord( lex, token ).has_value() )
			{
				std::cout << "Failed to parse the V texcoord" << std::endl;
				return;
			}

			// rotation
			token = lex.Next();
			if ( token.empty() )
			{
				std::cout << "Could not parse rotation" << std::endl;
				return;
			}

			// scaleX
			token = lex.Next();
			if ( token.empty() )
			{
				std::cout << "Could not parse scaleX" << std::endl;
				return;
			}

			// scaleY
			token = lex.Next();
			if ( token.empty() )
			{
				std::cout << "Could not parse scaleY" << std::endl;
				return;
			}

			brush.push_back(
				{
					adm::Plane( points[0].value(), points[1].value(), points[2].value() ),
					{ points[0].value(), points[1].value(), points[2].value() },
					textureName == "SKIP" || textureName == "SKY1" || textureName == "*04MWATS"
				} );
		}

		// We have just entered a { block for brushes
		void ParseBrush( adm::Lexer& lex, adm::String& token )
		{
			// Bail out
			if ( token == "}" )
			{
				return;
			}

			std::vector<MapFace> brush;
			brush.reserve( 6U );

			while ( token != "}" )
			{
				ParseBrushSide( lex, token, brush );
			}

			// There cannot be a brush with fewer than 4 faces (tetrahedron)
			if ( brush.size() >= 4 )
			{
				Brushes.push_back( std::move( brush ) );
			}
			else
			{
				std::cout << "Invalid brush" << std::endl;
			}

			// Expect a }
			if ( token != "}" )
			{
				std::cout << "Brush does not have an ending }" << std::endl;
				token = "}";
				return;
			}
		}

		void ParseEntity( adm::Lexer& lex, adm::String& token )
		{
			adm::Dictionary entityProperties;

			while ( !lex.IsEndOfFile() )
			{
				token = lex.Next();

				// The entity is done, bail out
				if ( token == "}" )
				{
					break;
				}

				// Keyvalues are stored first
				if ( token != "{" )
				{
					entityProperties.SetString( token.c_str(), lex.Next() );
				}
				// Then brushes
				else
				{
					if ( entityProperties["classname"] == "worldspawn" )
					{
						const int mapVersion = entityProperties.GetInteger( "mapversion" );
						if ( mapVersion == 220 )
						{
							valveMapFormat = true;
						}
						else if ( mapVersion == 0 )
						{
							valveMapFormat = false;
							std::cout << "Only Valve220 map format is supported" << std::endl;
							token = "}";
							return;
						}
						else
						{
							std::cout << "Unsupported map format: " << mapVersion << std::endl;
							token = "}";
							return;
						}
					}

					ParseBrush( lex, token );
				}
			}
		}
	}

	void ProcessBrushes()
	{
		for ( const auto& brush : Brushes )
		{
			// Sides of a brush
			for ( const auto& brushSide : brush )
			{
				// The polygon which will get intersected
				adm::Polygon polygon = adm::Polygon( brushSide.plane, 2048.0f );
				if ( brushSide.noDraw )
				{
					continue;
				}

				// Polygons that are very large will have pretty imprecise coordinates
				// after splitting, so here we're basically moving a smaller polygon into place,
				// to retain that precision
				const adm::Vec3 planeOrigin = brushSide.GetOrigin();
				const adm::Vec3 polygonOrigin = polygon.GetOrigin();
				const adm::Vec3 difference = planeOrigin - polygonOrigin;

				// Todo: Polygon::Shift method
				for ( auto& v : polygon.vertices )
				{
					v += difference;
				}

				// The planes that intersect the polygon
				for ( const auto& intersector : brush )
				{
					// Hackety hack til we get a == operator for adm::Plane
					if ( &brushSide == &intersector )
					{
						continue;
					}

					auto result = polygon.Split( intersector.plane );
					if ( result.didIntersect && result.back.has_value() )
					{
						// Modify the polygon we started off from
						polygon = result.back.value();
					}
				}

				Polygons.push_back( std::move( polygon ) );
			}
		}
	}

	void Load()
	{
		Polygons.clear();

		std::ifstream mapFile( "test.map" );
		if ( !mapFile ) 
		{
			std::cout << "WARNING: cannot find test.map" << std::endl;
			return;
		}

		adm::Lexer lex( mapFile );
		lex.SetDelimiters( adm::Lexer::DelimitersSimple );

		// Reserving 128 characters so we don't reallocate stuff often
		adm::String token;
		token.reserve( 128U );

		// Idea: Lexer to bool operator, so we can just do while ( lex )
		do
		{
			token = lex.Next();
			if ( token == "{" )
			{
				Parsing::ParseEntity( lex, token );
			}
			else if ( token.empty() )
			{
				break;
			}
			else
			{
				std::cout << "Unknown token: " << token << std::endl;
			}
		} while ( !lex.IsEndOfFile() );

		ProcessBrushes();
	}
}

void RunFrame( const float& deltaTime, const UserCommands& uc )
{
	static float time = 0.0f;
	time += deltaTime;

	if ( uc.flags & UserCommands::Flags::RightMouseButton )
	{
		viewAngles.x += uc.mouseY * deltaTime * 4.0f;
		viewAngles.y += uc.mouseX * deltaTime * 4.0f;
	}
	
	//viewAngles.z = std::sinf( time ) * 15.0f;

	// Clamp the pitch
	if ( viewAngles.x > 89.9f )
		viewAngles.x = 89.9f;
	if ( viewAngles.x < -89.9f )
		viewAngles.x = -89.9f;

	// Offset the view position
	const float adjustedViewSpeed = viewSpeed * ((uc.flags & UserCommands::SpeedModifier) ? 1.5f : 1.0f);

	viewOrigin += uc.forward * viewForward * deltaTime * adjustedViewSpeed;
	viewOrigin += uc.right * viewRight * deltaTime * adjustedViewSpeed;
	viewOrigin += uc.up * viewUp * deltaTime * adjustedViewSpeed;
	
	SetupMatrices();

	// Clear the view
	SDL_SetRenderDrawColor( renderer, 0, 0, 0, 255 );
	SDL_RenderClear( renderer );
	SDL_SetRenderDrawColor( renderer, 255, 255, 255, 255 );
	
	// Draw some polygons
	for ( const auto& p : Map::Polygons )
	{
		DrawPolygon( p );
	}

	// Crosshair
	//if ( false )
	{
		using adm::Vec3;

		static glm::vec3 crosshairOrigin = viewOrigin;

		// Draw a crosshair here
		if ( uc.flags & UserCommands::LeftMouseButton )
		{
			const glm::vec2 screen{ uc.windowMouseX, uc.windowMouseY };
			const glm::vec2 viewport{ windowWidth, windowHeight };
			const glm::vec3 screenToWorld = ScreenToWorldDirection( projMatrix, viewMatrix, screen, viewport );
			const glm::vec3 pos = viewOrigin + screenToWorld * 10.0f;

			DrawCross( pos );

			// Also attract the main crosshair
			crosshairOrigin += (pos - crosshairOrigin) * deltaTime * 20.0f;
		}
		else
		{
			crosshairOrigin += ((viewOrigin + viewForward * 5.0f) - crosshairOrigin) * deltaTime * 20.0f;
		}

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
	SDL_SetRelativeMouseMode( SDL_FALSE );

	Map::Load();

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
