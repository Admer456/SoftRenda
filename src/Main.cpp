
#include <iostream>
#include <thread>
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

glm::vec3 viewOrigin{ -30.0f, -10.0f, 10.0f };
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

// The Jolt headers don't include Jolt.h. Always include Jolt.h before including any other Jolt header.
// You can use Jolt.h in your precompiled header to speed up compilation.
#include <Jolt/Jolt.h>

// Jolt includes
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

namespace phys
{
	JPH_SUPPRESS_WARNINGS;

	using namespace JPH;
	
	// Layer that objects can be in, determines which other objects it can collide with
	// Typically you at least want to have 1 layer for moving bodies and 1 layer for static bodies, but you can have more
	// layers if you want. E.g. you could have a layer for high detail collision (which is not used by the physics simulation
	// but only if you do collision testing).
	namespace Layers
	{
		static constexpr uint8_t NON_MOVING = 0;
		static constexpr uint8_t MOVING = 1;
		static constexpr uint8_t NUM_LAYERS = 2;
	};

	// Function that determines if two object layers can collide
	static bool MyObjectCanCollide( ObjectLayer inObject1, ObjectLayer inObject2 )
	{
		switch ( inObject1 )
		{
		case Layers::NON_MOVING:
			return inObject2 == Layers::MOVING; // Non moving only collides with moving
		case Layers::MOVING:
			return true; // Moving collides with everything
		default:
			JPH_ASSERT( false );
			return false;
		}
	};

	// Each broadphase layer results in a separate bounding volume tree in the broad phase. You at least want to have
	// a layer for non-moving and moving objects to avoid having to update a tree full of static objects every frame.
	// You can have a 1-on-1 mapping between object layers and broadphase layers (like in this case) but if you have
	// many object layers you'll be creating many broad phase trees, which is not efficient. If you want to fine tune
	// your broadphase layers define JPH_TRACK_BROADPHASE_STATS and look at the stats reported on the TTY.
	namespace BroadPhaseLayers
	{
		static constexpr BroadPhaseLayer NON_MOVING( 0 );
		static constexpr BroadPhaseLayer MOVING( 1 );
		static constexpr uint NUM_LAYERS( 2 );
	};

	// BroadPhaseLayerInterface implementation
	// This defines a mapping between object and broadphase layers.
	class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface
	{
	public:
		BPLayerInterfaceImpl()
		{
			// Create a mapping table from object to broad phase layer
			mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
			mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
		}

		virtual uint					GetNumBroadPhaseLayers() const override
		{
			return BroadPhaseLayers::NUM_LAYERS;
		}

		virtual BroadPhaseLayer			GetBroadPhaseLayer( ObjectLayer inLayer ) const override
		{
			JPH_ASSERT( inLayer < Layers::NUM_LAYERS );
			return mObjectToBroadPhase[inLayer];
		}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		virtual const char* GetBroadPhaseLayerName( BroadPhaseLayer inLayer ) const override
		{
			switch ( (BroadPhaseLayer::Type)inLayer )
			{
			case (BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:	return "NON_MOVING";
			case (BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:		return "MOVING";
			default:													JPH_ASSERT( false ); return "INVALID";
			}
		}
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

	private:
		BroadPhaseLayer					mObjectToBroadPhase[Layers::NUM_LAYERS];
	};

	// Function that determines if two broadphase layers can collide
	static bool MyBroadPhaseCanCollide( ObjectLayer inLayer1, BroadPhaseLayer inLayer2 )
	{
		switch ( inLayer1 )
		{
		case Layers::NON_MOVING:
			return inLayer2 == BroadPhaseLayers::MOVING;
		case Layers::MOVING:
			return true;
		default:
			JPH_ASSERT( false );
			return false;
		}
	}

	adm::Vec3 PhysToAdm( const JPH::Float3& v )
	{
		return &v.x;
	}

	adm::Vec3 PhysToAdm( const JPH::Vec3& v )
	{
		return { v.GetX(), v.GetY(), v.GetZ() };
	}

	class PhysicsDebugMesh final : public RefTargetVirtual, RefTarget<PhysicsDebugMesh>
	{
	public:
		PhysicsDebugMesh( const DebugRenderer::Triangle* triangles, int triangleCount )
		{
			for ( int i = 0; i < triangleCount; i++ )
			{
				verts.push_back( triangles[i].mV[0] );
				verts.push_back( triangles[i].mV[1] );
				verts.push_back( triangles[i].mV[2] );
			}
		}

		PhysicsDebugMesh( const DebugRenderer::Vertex* vertices, int vertexCount, const uint32* indices, int indexCount )
		{
			for ( int i = 0; i < indexCount; i++ )
			{
				verts.push_back( vertices[indices[i]] );
			}
		}

		void AddRef() override
		{
			RefTarget<PhysicsDebugMesh>::AddRef();
		}
		
		void Release() override
		{
			if ( --mRefCount == 0 )
			{
				delete this;
			}
		}

		std::vector<DebugRenderer::Vertex> verts;
	};

	// Debug rendera
	class PhysicsDebugRenderer final : public DebugRenderer
	{
	public:
		PhysicsDebugRenderer()
		{
			DebugRenderer::Initialize();
		}

		void DrawLine( const Float3& inFrom, const Float3& inTo, ColorArg inColor ) override
		{
			SDL_SetRenderDrawColor( renderer, inColor.r, inColor.g, inColor.b, inColor.a );
			DrawLine3D( &inFrom.x, &inTo.x );
		}

		void DrawLine( const Vec4& inFrom, const Vec4& inTo, ColorArg color )
		{
			Float3 from{ inFrom.GetX(), inFrom.GetY(), inFrom.GetZ() };
			Float3 to{ inTo.GetX(), inTo.GetY(), inTo.GetZ() };

			return DrawLine( from, to, color );
		}

		void DrawTriangle( Vec3Arg inV1, Vec3Arg inV2, Vec3Arg inV3, ColorArg inColor ) override
		{
			SDL_SetRenderDrawColor( renderer, inColor.r, inColor.g, inColor.b, inColor.a );
			DrawLine3D( PhysToAdm( inV1 ), PhysToAdm( inV2 ) );
			DrawLine3D( PhysToAdm( inV2 ), PhysToAdm( inV3 ) );
			DrawLine3D( PhysToAdm( inV3 ), PhysToAdm( inV1 ) );
		}

		Batch CreateTriangleBatch( const Triangle* inTriangles, int inTriangleCount ) override
		{
			return new PhysicsDebugMesh( inTriangles, inTriangleCount );
		}

		Batch CreateTriangleBatch( const Vertex* inVertices, int inVertexCount, const uint32* inIndices, int inIndexCount ) override
		{
			return new PhysicsDebugMesh( inVertices, inVertexCount, inIndices, inIndexCount );
		}

		void DrawGeometry( 
			Mat44Arg inModelMatrix, 
			const AABox& inWorldSpaceBounds, 
			float inLODScaleSq, 
			ColorArg inModelColor, 
			const GeometryRef& inGeometry, 
			ECullMode inCullMode = ECullMode::CullBackFace, 
			ECastShadow inCastShadow = ECastShadow::On, 
			EDrawMode inDrawMode = EDrawMode::Solid ) override
		{
			SDL_SetRenderDrawColor( renderer, inModelColor.r, inModelColor.g, inModelColor.b, inModelColor.a );

			// Because yes
			PhysicsDebugMesh* m = static_cast<PhysicsDebugMesh*>( inGeometry.GetPtr()->mLODs[0].mTriangleBatch.GetPtr() );
			
			for ( int i = 0; i < m->verts.size(); i += 3 )
			{
				const auto& v = m->verts;
				JPH::Vec4 t[3];

				t[0] = Vec4( v[i+0].mPosition.x, v[i+0].mPosition.y, v[i+0].mPosition.z, 1.0f );
				t[1] = Vec4( v[i+1].mPosition.x, v[i+1].mPosition.y, v[i+1].mPosition.z, 1.0f );
				t[2] = Vec4( v[i+2].mPosition.x, v[i+2].mPosition.y, v[i+2].mPosition.z, 1.0f );

				t[0] = inModelMatrix * t[0];
				t[1] = inModelMatrix * t[1];
				t[2] = inModelMatrix * t[2];

				DrawLine( t[0], t[1], inModelColor );
				DrawLine( t[1], t[2], inModelColor );
				DrawLine( t[2], t[0], inModelColor );
			}
		}
	
		void DrawText3D( Vec3Arg inPosition, const string& inString, ColorArg inColor = Color::sWhite, float inHeight = 0.5f ) override
		{
			// No.
			return;
		}
	};

	PhysicsDebugRenderer PhysDebugRenderer;

	// We need a temp allocator for temporary allocations during the physics update. We're
	// pre-allocating 10 MB to avoid having to do allocations during the physics update. 
	// B.t.w. 10 MB is way too much for this example but it is a typical value you can use.
	// If you don't want to pre-allocate you can also use TempAllocatorMalloc to fall back to
	// malloc / free.
	TempAllocatorImpl TempAllocator( 10 * 1024 * 1024 );

	// We need a job system that will execute physics jobs on multiple threads. Typically
	// you would implement the JobSystem interface yourself and let Jolt Physics run on top
	// of your own job scheduler. JobSystemThreadPool is an example implementation.
	JobSystemThreadPool* JobSystem = nullptr;

	// This is the max amount of rigid bodies that you can add to the physics system. If you try to add more you'll get an error.
	// Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
	const JPH::uint cMaxBodies = 1024;

	// This determines how many mutexes to allocate to protect rigid bodies from concurrent access. Set it to 0 for the default settings.
	const JPH::uint cNumBodyMutexes = 0;

	// This is the max amount of body pairs that can be queued at any time (the broad phase will detect overlapping
	// body pairs based on their bounding boxes and will insert them into a queue for the narrowphase). If you make this buffer
	// too small the queue will fill up and the broad phase jobs will start to do narrow phase work. This is slightly less efficient.
	// Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
	const JPH::uint cMaxBodyPairs = 1024;

	// This is the maximum size of the contact constraint buffer. If more contacts (collisions between bodies) are detected than this
	// number then these contacts will be ignored and bodies will start interpenetrating / fall through the world.
	// Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
	const JPH::uint cMaxContactConstraints = 1024;

	// Create mapping table from object layer to broadphase layer
	// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
	phys::BPLayerInterfaceImpl BPLayerInterface;

	JPH::PhysicsSystem PhysSystem;

	BodyID FloorId, BoxId;
}

void PhysInit()
{
	using namespace phys;

	JPH::Trace = []( const char* str, ... )
	{
		char buffer[2048U];
		va_list arguments;

		va_start( arguments, str );
		vsprintf( buffer, str, arguments );
		va_end( arguments );

		std::cout << buffer << std::endl;
	};

	// Register all Jolt physics types
	JPH::RegisterTypes();

	// Now we can create the actual physics system.
	PhysSystem.Init( cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, 
		BPLayerInterface, MyBroadPhaseCanCollide, MyObjectCanCollide );

	PhysSystem.SetGravity( Vec3( 0.0f, 0.0f, -9.81f ) );

	// The main way to interact with the bodies in the physics system is through the body interface. There is a locking and a non-locking
	// variant of this. We're going to use the locking version (even though we're not planning to access bodies from multiple threads)
	BodyInterface& bodyInterface = PhysSystem.GetBodyInterface();

	// Next we can create a rigid body to serve as the floor, we make a large box
	// Create the settings for the collision volume (the shape). 
	// Note that for simple shapes (like boxes) you can also directly construct a BoxShape.
	BoxShapeSettings floorShapeSettings( Vec3( 20.0f, 20.0f, 0.5f ) );

	// Create the shape
	ShapeSettings::ShapeResult floorShapeResult = floorShapeSettings.Create();
	ShapeRefC floorShape = floorShapeResult.Get(); // We don't expect an error here, but you can check floor_shape_result for HasError() / GetError()

	// Create the settings for the body itself. Note that here you can also set other properties like the restitution / friction.
	BodyCreationSettings floorSettings( floorShape, Vec3( 0.0f, 0.0f, -1.0f ), Quat::sIdentity(), EMotionType::Static, Layers::NON_MOVING );

	// Create the actual rigid body
	Body* floor = bodyInterface.CreateBody( floorSettings ); // Note that if we run out of bodies this can return nullptr
	FloorId = floor->GetID();

	// Add it to the world
	bodyInterface.AddBody( floor->GetID(), EActivation::DontActivate );

	// Now create a dynamic body to bounce on the floor
	// Note that this uses the shorthand version of creating and adding a body to the world
	BoxShapeSettings boxShapeSettings( Vec3( 6.0f, 7.0f, 4.0f ) );
	ShapeSettings::ShapeResult boxShapeResult = boxShapeSettings.Create();
	ShapeRefC boxShape = boxShapeResult.Get();

	BodyCreationSettings boxSettings( boxShape, Vec3( 0.0f, 0.0f, 60.0f ), Quat::sIdentity() * Quat::sEulerAngles( Vec3( 0.333f, 0.2f, 0.05f ) ), EMotionType::Dynamic, Layers::MOVING);
	BoxId = bodyInterface.CreateAndAddBody( boxSettings, EActivation::Activate );

	// Now you can interact with the dynamic body, in this case we're going to give it a velocity.
	// (note that if we had used CreateBody then we could have set the velocity straight on the body before adding it to the physics system)
	bodyInterface.SetLinearVelocity( BoxId, Vec3( 0.0f, 0.0f, 5.0f ) );

	// We simulate the physics world in discrete time steps. 60 Hz is a good rate to update the physics system.
	const float cDeltaTime = 1.0f / 60.0f;

	// Optional step: Before starting the physics simulation you can optimize the broad phase. This improves collision detection performance (it's pointless here because we only have 2 bodies).
	// You should definitely not call this every frame or when e.g. streaming in a new level section as it is an expensive operation.
	// Instead insert all new objects in batches instead of 1 at a time to keep the broad phase efficient.
	PhysSystem.OptimizeBroadPhase();

	// Look at PhysShutdown for why we're doing this
	phys::JobSystem = new JobSystemThreadPool( JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1 );
}

void PhysUpdate( const float& deltaTime )
{
	phys::PhysSystem.Update( deltaTime, 2, 4, &phys::TempAllocator, phys::JobSystem );

	static JPH::BodyManager::DrawSettings ds;
	ds.mDrawShapeWireframe = true;
	ds.mDrawShape = true;
	phys::PhysSystem.DrawBodies( ds, &phys::PhysDebugRenderer );
}

void PhysShutdown()
{
	auto& bodyInterface = phys::PhysSystem.GetBodyInterface();

	bodyInterface.RemoveBody( phys::BoxId );
	bodyInterface.RemoveBody( phys::FloorId );

	bodyInterface.DestroyBody( phys::BoxId );
	bodyInterface.DestroyBody( phys::FloorId );

	// If we kept the job system as a non-pointer, it'd get destroyed at the end of the program
	// which, for some reason, would crash because some threads are nullptr :)
	delete phys::JobSystem;
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

	// Grid
	{
		using adm::Vec3;

		SDL_SetRenderDrawColor( renderer, 128, 128, 128, 255 );
		for ( int x = -8; x <= 8; x++ )
		{
			DrawLine3D( Vec3::Forward * 20.0f + (Vec3::Right * 20.0f / 8.0f) * x, Vec3::Forward * -20.0f + (Vec3::Right * 20.0f / 8.0f) * x );
		}
		for ( int y = -8; y <= 8; y++ )
		{
			DrawLine3D( Vec3::Right * 20.0f + (Vec3::Forward * 20.0f / 8.0f) * y, Vec3::Right * -20.0f + (Vec3::Forward * 20.0f / 8.0f) * y );
		}
	}

	PhysUpdate( deltaTime );

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

	PhysInit();

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

	PhysShutdown();
	SDL_Quit();

	return 0;
}
