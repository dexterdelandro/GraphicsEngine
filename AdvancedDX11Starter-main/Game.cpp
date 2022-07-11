#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui\imgui_impl_win32.h"
#include <stdlib.h>     // For seeding random and rand()
#include <time.h>       // For grabbing time (to seed random)

#include "Game.h"
#include "Vertex.h"
#include "Input.h"

#include "WICTextureLoader.h"


// Needed for a helper function to read compiled shader files from the hard drive
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

// For the DirectX Math library
using namespace DirectX;

// Helper macro for getting a float between min and max
#define RandomRange(min, max) (float)rand() / RAND_MAX * (max - min) + min

// Helper macros for making texture and shader loading code more succinct
#define LoadTexture(file, srv) CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(file).c_str(), 0, srv.GetAddressOf())
#define LoadShader(type, file) std::make_shared<type>(device.Get(), context.Get(), GetFullPathTo_Wide(file).c_str())


// --------------------------------------------------------
// Constructor
//
// DXCore (base class) constructor will set up underlying fields.
// DirectX itself, and our window, are not ready yet!
//
// hInstance - the application's OS-level handle (unique ID)
// --------------------------------------------------------
Game::Game(HINSTANCE hInstance)
	: DXCore(
		hInstance,		   // The application's handle
		"DirectX Game",	   // Text for the window's title bar
		1280,			   // Width of the window's client area
		720,			   // Height of the window's client area
		true),			   // Show extra stats (fps) in title bar?
	camera(0),
	sky(0),
	spriteBatch(0),
	lightCount(0),
	arial(0)
{
	// Seed random
	srand((unsigned int)time(0));

#if defined(DEBUG) || defined(_DEBUG)
	// Do we want a console window?  Probably only in debug mode
	CreateConsoleWindow(500, 120, 32, 120);
	printf("Console window created successfully.  Feel free to printf() here.\n");
#endif

}

// --------------------------------------------------------
// Destructor - Clean up anything our game has created:
//  - Release all DirectX objects created here
//  - Delete any objects to prevent memory leaks
// --------------------------------------------------------
Game::~Game()
{
	// Note: Since we're using smart pointers (ComPtr),
	// we don't need to explicitly clean up those DirectX objects
	// - If we weren't using smart pointers, we'd need
	//   to call Release() on each DirectX object

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	delete renderer;
}

// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{
	// Asset loading and entity creation
	LoadAssetsAndCreateEntities();
	
	// Tell the input assembler stage of the pipeline what kind of
	// geometric primitives (points, lines or triangles) we want to draw.  
	// Essentially: "What kind of shape should the GPU draw with our data?"
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set up lights initially
	lightCount = 10;
	GenerateLights();

	renderer = new Renderer(
		device,
		context,
		swapChain,
		backBufferRTV,
		depthStencilView,
		width,
		height,
		sky,
		entities,
		lights,
		lightCount,
		lightPS,
		lightVS,
		lightMesh,
		refractionPS,
		fullscreenVS,
		simplePS,
		ssaoPS,
		ssaoBlurPS,
		ssaoCombinePS,
		skyPS,
		lightRayPS,
		emitters
	);

	// Make our camera
	camera = std::make_shared<Camera>(
		0.0f, 0.0f, -10.0f,	// Position
		3.0f,		// Move speed
		1.0f,		// Mouse look
		this->width / (float)this->height); // Aspect ratio

	//Initialize Imgui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	//Style
	ImGui::StyleColorsDark();

	//Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(device.Get(), context.Get());

	guiActive = true;
}


// --------------------------------------------------------
// Load all assets and create materials, entities, etc.
// --------------------------------------------------------
void Game::LoadAssetsAndCreateEntities()
{
	// Load shaders using our succinct LoadShader() macro
	std::shared_ptr<SimpleVertexShader> vertexShader	= LoadShader(SimpleVertexShader, L"VertexShader.cso");
	std::shared_ptr<SimplePixelShader> pixelShader		= LoadShader(SimplePixelShader, L"PixelShader.cso");
	pixelShaderPBR	= LoadShader(SimplePixelShader, L"PixelShaderPBR.cso");
	std::shared_ptr<SimplePixelShader> solidColorPS		= LoadShader(SimplePixelShader, L"SolidColorPS.cso");
	
	std::shared_ptr<SimpleVertexShader> skyVS = LoadShader(SimpleVertexShader, L"SkyVS.cso");
	skyPS  = LoadShader(SimplePixelShader, L"SkyPS.cso");

	//IBL SHADERS
	fullscreenVS = LoadShader(SimpleVertexShader, L"FullscreenVS.cso");
	std::shared_ptr<SimplePixelShader> irradiancePS = LoadShader(SimplePixelShader, L"IBLIrradianceMapPS.cso");
	std::shared_ptr<SimplePixelShader> specConvPS = LoadShader(SimplePixelShader, L"IBLSpecularConvolutionPS.cso");
	std::shared_ptr<SimplePixelShader> envBrdfPS = LoadShader(SimplePixelShader, L"IBLBrdfLookUpTablePS.cso");

	//Refraction Shaders
	refractionPS = LoadShader(SimplePixelShader, L"RefractionPS.cso");
	simplePS = LoadShader(SimplePixelShader, L"SimpleShaderPS.cso");

	//Particle Shaders
	particlePS = LoadShader(SimplePixelShader, L"ParticlePS.cso");
	particleVS = LoadShader(SimpleVertexShader, L"ParticleVS.cso");

	//SSAO Shaders
	ssaoPS = LoadShader(SimplePixelShader, L"SsaoPS.cso");
	ssaoBlurPS = LoadShader(SimplePixelShader, L"SsaoPS.cso");
	ssaoCombinePS = LoadShader(SimplePixelShader, L"SsaoPS.cso");

	lightRayPS = LoadShader(SimplePixelShader, L"LightRayPS.cso");


	// Set up the sprite batch and load the sprite font
	spriteBatch = std::make_shared<SpriteBatch>(context.Get());
	arial = std::make_shared<SpriteFont>(device.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/arial.spritefont").c_str());

	// Make the meshes
	std::shared_ptr<Mesh> sphereMesh = std::make_shared<Mesh>(GetFullPathTo("../../Assets/Models/sphere.obj").c_str(), device);
	std::shared_ptr<Mesh> helixMesh = std::make_shared<Mesh>(GetFullPathTo("../../Assets/Models/helix.obj").c_str(), device);
	std::shared_ptr<Mesh> cubeMesh = std::make_shared<Mesh>(GetFullPathTo("../../Assets/Models/cube.obj").c_str(), device);
	std::shared_ptr<Mesh> coneMesh = std::make_shared<Mesh>(GetFullPathTo("../../Assets/Models/cone.obj").c_str(), device);
	
	// Declare the textures we'll need
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cobbleA,  cobbleN,  cobbleR,  cobbleM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> floorA,  floorN,  floorR,  floorM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> paintA,  paintN,  paintR,  paintM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> scratchedA,  scratchedN,  scratchedR,  scratchedM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bronzeA,  bronzeN,  bronzeR,  bronzeM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> roughA,  roughN,  roughR,  roughM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> woodA,  woodN,  woodR,  woodM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> white, black, light_gray, dark_gray, flatNormalMap;
	

	// Load the textures using our succinct LoadTexture() macro
	LoadTexture(L"../../Assets/Textures/cobblestone_albedo.png", cobbleA);
	LoadTexture(L"../../Assets/Textures/cobblestone_normals.png", cobbleN);
	LoadTexture(L"../../Assets/Textures/cobblestone_roughness.png", cobbleR);
	LoadTexture(L"../../Assets/Textures/cobblestone_metal.png", cobbleM);

	LoadTexture(L"../../Assets/Textures/floor_albedo.png", floorA);
	LoadTexture(L"../../Assets/Textures/floor_normals.png", floorN);
	LoadTexture(L"../../Assets/Textures/floor_roughness.png", floorR);
	LoadTexture(L"../../Assets/Textures/floor_metal.png", floorM);
	
	LoadTexture(L"../../Assets/Textures/paint_albedo.png", paintA);
	LoadTexture(L"../../Assets/Textures/paint_normals.png", paintN);
	LoadTexture(L"../../Assets/Textures/paint_roughness.png", paintR);
	LoadTexture(L"../../Assets/Textures/paint_metal.png", paintM);
	
	LoadTexture(L"../../Assets/Textures/scratched_albedo.png", scratchedA);
	LoadTexture(L"../../Assets/Textures/scratched_normals.png", scratchedN);
	LoadTexture(L"../../Assets/Textures/scratched_roughness.png", scratchedR);
	LoadTexture(L"../../Assets/Textures/scratched_metal.png", scratchedM);
	
	LoadTexture(L"../../Assets/Textures/bronze_albedo.png", bronzeA);
	LoadTexture(L"../../Assets/Textures/bronze_normals.png", bronzeN);
	LoadTexture(L"../../Assets/Textures/bronze_roughness.png", bronzeR);
	LoadTexture(L"../../Assets/Textures/bronze_metal.png", bronzeM);
	
	LoadTexture(L"../../Assets/Textures/rough_albedo.png", roughA);
	LoadTexture(L"../../Assets/Textures/rough_normals.png", roughN);
	LoadTexture(L"../../Assets/Textures/rough_roughness.png", roughR);
	LoadTexture(L"../../Assets/Textures/rough_metal.png", roughM);
	
	LoadTexture(L"../../Assets/Textures/wood_albedo.png", woodA);
	LoadTexture(L"../../Assets/Textures/wood_normals.png", woodN);
	LoadTexture(L"../../Assets/Textures/wood_roughness.png", woodR);
	LoadTexture(L"../../Assets/Textures/wood_metal.png", woodM);

	LoadTexture(L"../../Assets/Textures/white.png", white);
	LoadTexture(L"../../Assets/Textures/black.png", black);
	LoadTexture(L"../../Assets/Textures/light_gray.png", light_gray);
	LoadTexture(L"../../Assets/Textures/dark_gray.png", dark_gray);

	LoadTexture(L"../../Assets/Particles/snowflake.png", particleTextureSnow);
	LoadTexture(L"../../Assets/Particles/smoke_01.png", particleTextureSmoke);
	LoadTexture(L"../../Assets/Particles/trace_03.png", particleTextureTrace);
	//LoadTexture(L"../../Assets/Textures/FlatNormalMap.png", flatNormalMap);
	CreateWICTextureFromFileEx(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/FlatNormalMap.png").c_str(), 1024, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, WIC_LOADER_IGNORE_SRGB, 0, flatNormalMap.GetAddressOf());

	FlatNormalMapTest = flatNormalMap;

	// Describe and create our sampler state
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	sampDesc.MaxAnisotropy = 16;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	device->CreateSamplerState(&sampDesc, samplerOptions.GetAddressOf());

	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	device->CreateSamplerState(&sampDesc, clampSampler.GetAddressOf());


	// Create the sky using 6 images
	sky = std::make_shared<Sky>(
		GetFullPathTo_Wide(L"..\\..\\Assets\\Skies\\Clouds Blue\\right.png").c_str(),
		GetFullPathTo_Wide(L"..\\..\\Assets\\Skies\\Clouds Blue\\left.png").c_str(),
		GetFullPathTo_Wide(L"..\\..\\Assets\\Skies\\Clouds Blue\\up.png").c_str(),
		GetFullPathTo_Wide(L"..\\..\\Assets\\Skies\\Clouds Blue\\down.png").c_str(),
		GetFullPathTo_Wide(L"..\\..\\Assets\\Skies\\Clouds Blue\\front.png").c_str(),
		GetFullPathTo_Wide(L"..\\..\\Assets\\Skies\\Clouds Blue\\back.png").c_str(),
		cubeMesh,
		skyVS,
		skyPS,
		samplerOptions,
		device,
		context,
		fullscreenVS,
		irradiancePS,
		specConvPS,
		envBrdfPS);


	// Create PBR materials
	std::shared_ptr<Material> cobbleMat2xPBR = std::make_shared<Material>(pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	cobbleMat2xPBR->AddSampler("BasicSampler", samplerOptions);
	cobbleMat2xPBR->AddSampler("ClampSampler", clampSampler);
	cobbleMat2xPBR->AddTextureSRV("Albedo", cobbleA);
	cobbleMat2xPBR->AddTextureSRV("NormalMap", cobbleN);
	cobbleMat2xPBR->AddTextureSRV("RoughnessMap", cobbleR);
	cobbleMat2xPBR->AddTextureSRV("MetalMap", cobbleM);
	cobbleMat2xPBR->SetRefractive(true);


	std::shared_ptr<Material> cobbleMat4xPBR = std::make_shared<Material>(pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 4));
	cobbleMat4xPBR->AddSampler("BasicSampler", samplerOptions);
	cobbleMat4xPBR->AddSampler("ClampSampler", clampSampler);
	cobbleMat4xPBR->AddTextureSRV("Albedo", cobbleA);
	cobbleMat4xPBR->AddTextureSRV("NormalMap", cobbleN);
	cobbleMat4xPBR->AddTextureSRV("RoughnessMap", cobbleR);
	cobbleMat4xPBR->AddTextureSRV("MetalMap", cobbleM);

	std::shared_ptr<Material> floorMatPBR = std::make_shared<Material>(pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	floorMatPBR->AddSampler("BasicSampler", samplerOptions);
	floorMatPBR->AddSampler("ClampSampler", clampSampler);
	floorMatPBR->AddTextureSRV("Albedo", floorA);
	floorMatPBR->AddTextureSRV("NormalMap", floorN);
	floorMatPBR->AddTextureSRV("RoughnessMap", floorR);
	floorMatPBR->AddTextureSRV("MetalMap", floorM);

	std::shared_ptr<Material> paintMatPBR = std::make_shared<Material>(pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	paintMatPBR->AddSampler("BasicSampler", samplerOptions);
	paintMatPBR->AddSampler("ClampSampler", clampSampler);
	paintMatPBR->AddTextureSRV("Albedo", paintA);
	paintMatPBR->AddTextureSRV("NormalMap", paintN);
	paintMatPBR->AddTextureSRV("RoughnessMap", paintR);
	paintMatPBR->AddTextureSRV("MetalMap", paintM);

	std::shared_ptr<Material> scratchedMatPBR = std::make_shared<Material>(pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	scratchedMatPBR->AddSampler("BasicSampler", samplerOptions);
	scratchedMatPBR->AddSampler("ClampSampler", clampSampler);
	scratchedMatPBR->AddTextureSRV("Albedo", scratchedA);
	scratchedMatPBR->AddTextureSRV("NormalMap", scratchedN);
	scratchedMatPBR->AddTextureSRV("RoughnessMap", scratchedR);
	scratchedMatPBR->AddTextureSRV("MetalMap", scratchedM);

	std::shared_ptr<Material> bronzeMatPBR = std::make_shared<Material>(pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	bronzeMatPBR->AddSampler("BasicSampler", samplerOptions);
	bronzeMatPBR->AddSampler("ClampSampler", clampSampler);
	bronzeMatPBR->AddTextureSRV("Albedo", bronzeA);
	bronzeMatPBR->AddTextureSRV("NormalMap", bronzeN);
	bronzeMatPBR->AddTextureSRV("RoughnessMap", bronzeR);
	bronzeMatPBR->AddTextureSRV("MetalMap", bronzeM);

	std::shared_ptr<Material> roughMatPBR = std::make_shared<Material>(pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	roughMatPBR->AddSampler("BasicSampler", samplerOptions);
	roughMatPBR->AddSampler("ClampSampler", clampSampler);
	roughMatPBR->AddTextureSRV("Albedo", roughA);
	roughMatPBR->AddTextureSRV("NormalMap", roughN);
	roughMatPBR->AddTextureSRV("RoughnessMap", roughR);
	roughMatPBR->AddTextureSRV("MetalMap", roughM);
	roughMatPBR->SetRefractive(true);

	std::shared_ptr<Material> woodMatPBR = std::make_shared<Material>(pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	woodMatPBR->AddSampler("BasicSampler", samplerOptions);
	woodMatPBR->AddSampler("ClampSampler", clampSampler);
	woodMatPBR->AddTextureSRV("Albedo", woodA);
	woodMatPBR->AddTextureSRV("NormalMap", woodN);
	woodMatPBR->AddTextureSRV("RoughnessMap", woodR);
	woodMatPBR->AddTextureSRV("MetalMap", woodM);

	//create testing materials
	std::shared_ptr<Material> shinyMetal = std::make_shared<Material>(pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	shinyMetal->AddSampler("BasicSampler", samplerOptions);
	shinyMetal->AddSampler("ClampSampler", clampSampler);
	shinyMetal->AddTextureSRV("Albedo", white);
	shinyMetal->AddTextureSRV("NormalMap", flatNormalMap);
	shinyMetal->AddTextureSRV("RoughnessMap", black);
	shinyMetal->AddTextureSRV("MetalMap", white);

	std::shared_ptr<Material> quarterRoughMetal = std::make_shared<Material>(pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	quarterRoughMetal->AddSampler("BasicSampler", samplerOptions);
	quarterRoughMetal->AddSampler("ClampSampler", clampSampler);
	quarterRoughMetal->AddTextureSRV("Albedo", white);
	quarterRoughMetal->AddTextureSRV("NormalMap", flatNormalMap);
	quarterRoughMetal->AddTextureSRV("RoughnessMap", dark_gray);
	quarterRoughMetal->AddTextureSRV("MetalMap", white);

	std::shared_ptr<Material> halfRoughMetal = std::make_shared<Material>(pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	halfRoughMetal->AddSampler("BasicSampler", samplerOptions);
	halfRoughMetal->AddSampler("ClampSampler", clampSampler);
	halfRoughMetal->AddTextureSRV("Albedo", white);
	halfRoughMetal->AddTextureSRV("NormalMap", flatNormalMap);
	halfRoughMetal->AddTextureSRV("RoughnessMap", light_gray);
	halfRoughMetal->AddTextureSRV("MetalMap", white);

	std::shared_ptr<Material> shinyPlastic = std::make_shared<Material>(pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	shinyPlastic->AddSampler("BasicSampler", samplerOptions);
	shinyPlastic->AddSampler("ClampSampler", clampSampler);
	shinyPlastic->AddTextureSRV("Albedo", white);
	shinyPlastic->AddTextureSRV("NormalMap", flatNormalMap);
	shinyPlastic->AddTextureSRV("RoughnessMap", black);
	shinyPlastic->AddTextureSRV("MetalMap", black);

	std::shared_ptr<Material> quarterRoughPlastic = std::make_shared<Material>(pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	quarterRoughPlastic->AddSampler("BasicSampler", samplerOptions);
	quarterRoughPlastic->AddSampler("ClampSampler", clampSampler);
	quarterRoughPlastic->AddTextureSRV("Albedo", white);
	quarterRoughPlastic->AddTextureSRV("NormalMap", flatNormalMap);
	quarterRoughPlastic->AddTextureSRV("RoughnessMap", dark_gray);
	quarterRoughPlastic->AddTextureSRV("MetalMap", black);

	std::shared_ptr<Material> halfRoughPlastic = std::make_shared<Material>(pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	halfRoughPlastic->AddSampler("BasicSampler", samplerOptions);
	halfRoughPlastic->AddSampler("ClampSampler", clampSampler);
	halfRoughPlastic->AddTextureSRV("Albedo", white);
	halfRoughPlastic->AddTextureSRV("NormalMap", flatNormalMap);
	halfRoughPlastic->AddTextureSRV("RoughnessMap", light_gray);
	halfRoughPlastic->AddTextureSRV("MetalMap", black);



	// === Create the PBR entities =====================================
	std::shared_ptr<GameEntity> cobSpherePBR = std::make_shared<GameEntity>(sphereMesh, cobbleMat2xPBR);
	cobSpherePBR->GetTransform()->SetPosition(-6, 2, 0);

	std::shared_ptr<GameEntity> floorSpherePBR = std::make_shared<GameEntity>(sphereMesh, floorMatPBR);
	floorSpherePBR->GetTransform()->SetPosition(-4, 2, 0);

	std::shared_ptr<GameEntity> paintSpherePBR = std::make_shared<GameEntity>(sphereMesh, paintMatPBR);
	paintSpherePBR->GetTransform()->SetPosition(-2, 2, 0);

	std::shared_ptr<GameEntity> scratchSpherePBR = std::make_shared<GameEntity>(sphereMesh, scratchedMatPBR);
	scratchSpherePBR->GetTransform()->SetPosition(0, 2, 0);

	std::shared_ptr<GameEntity> bronzeSpherePBR = std::make_shared<GameEntity>(sphereMesh, bronzeMatPBR);
	bronzeSpherePBR->GetTransform()->SetPosition(2, 2, 0);

	std::shared_ptr<GameEntity> roughSpherePBR = std::make_shared<GameEntity>(sphereMesh, roughMatPBR);
	roughSpherePBR->GetTransform()->SetPosition(4, 2, 0);

	std::shared_ptr<GameEntity> woodSpherePBR = std::make_shared<GameEntity>(sphereMesh, woodMatPBR);
	woodSpherePBR->GetTransform()->SetPosition(6, 2, 0);

	std::shared_ptr<GameEntity> roughPlanePBR = std::make_shared<GameEntity>(cubeMesh, roughMatPBR);
	roughPlanePBR->GetTransform()->SetScale(5, 5, 1);
	roughPlanePBR->GetTransform()->SetPosition(0, 0, -2);

	entities.push_back(cobSpherePBR);
	entities.push_back(floorSpherePBR);
	entities.push_back(paintSpherePBR);
	entities.push_back(scratchSpherePBR);
	entities.push_back(bronzeSpherePBR);
	entities.push_back(roughSpherePBR);
	entities.push_back(woodSpherePBR);
	entities.push_back(roughPlanePBR); //for refraction

	//IBL Testing Entities
	std::shared_ptr<GameEntity> shinyMetalSphere = std::make_shared<GameEntity>(sphereMesh, shinyMetal);
	shinyMetalSphere->GetTransform()->SetPosition(-6, 7, 0);

	std::shared_ptr<GameEntity> quarterRoughMetalSphere = std::make_shared<GameEntity>(sphereMesh, quarterRoughMetal);
	quarterRoughMetalSphere->GetTransform()->SetPosition(-4, 7, 0);

	std::shared_ptr<GameEntity> halfRoughMetalSphere = std::make_shared<GameEntity>(sphereMesh, halfRoughMetal);
	halfRoughMetalSphere->GetTransform()->SetPosition(-2, 7, 0);

	std::shared_ptr<GameEntity> shinyPlasticSphere = std::make_shared<GameEntity>(sphereMesh, shinyPlastic);
	shinyPlasticSphere->GetTransform()->SetPosition(-6, 5, 0);

	std::shared_ptr<GameEntity> quarterRoughPlasticSphere = std::make_shared<GameEntity>(sphereMesh, quarterRoughPlastic);
	quarterRoughPlasticSphere->GetTransform()->SetPosition(-4, 5, 0);

	std::shared_ptr<GameEntity> halfRoughPlasticSphere = std::make_shared<GameEntity>(sphereMesh, halfRoughPlastic);
	halfRoughPlasticSphere->GetTransform()->SetPosition(-2, 5, 0);

	entities.push_back(shinyMetalSphere);
	entities.push_back(quarterRoughMetalSphere);
	entities.push_back(halfRoughMetalSphere);
	entities.push_back(shinyPlasticSphere);
	entities.push_back(quarterRoughPlasticSphere);
	entities.push_back(halfRoughPlasticSphere);

	snowEmitter = std::make_shared<Emitter>(
		XMFLOAT3(0, 5, 0),			// Emitter position
		particleVS,
		particlePS,
		200,							// Max particles
		50,								// Particles per second
		5,								// Particle lifetime
		0.2f,							// Start size
		0.0f,							// End size
		XMFLOAT4(.9f, .9f, 1.0f, 0.0f),// Start color
		XMFLOAT4(1.0f, 1.0f, 1.0f, 0.4f),// End color
		XMFLOAT3(0, 0, 0),				// Start velocity
		XMFLOAT3(10.0f, 0.01f, 10.0f),				// Position randomness range
		XMFLOAT3(0.1f, 0.1, 0.1f),		// Velocity randomness range
		XMFLOAT3(0, -1, 0),				// Constant acceleration
		XMFLOAT2(-XM_PI,XM_PI),					//start rotation
		XMFLOAT2(-XM_PI,XM_PI),						//end rotation
		device,
		particleTextureSnow,
		samplerOptions,
		context
	);


	flameEmitter = std::make_shared<Emitter>(
		XMFLOAT3(0.0f, -2.0f, 0.0f),			// Emitter position
		particleVS,
		particlePS,
		120,									// Max particles
		20,										// Particles per second
		3,										// Particle lifetime
		0.2f,									// Start size
		1.0f,									// End size
		XMFLOAT4(1.0f, 0.27f, 0.0f, 0.0f),		// Start color
		XMFLOAT4(1.0, 0.0f, 0.0f, 0.1f),		// End color
		XMFLOAT3(1.0f, 0.65f, 0.25f),				// Start velocity
		XMFLOAT3(0.05f, 0.01f, 0.05f),			// Position randomness range
		XMFLOAT3(0.1f, 0.1, 0.1f),				// Velocity randomness range
		XMFLOAT3(0.8f, -1.0f, 0.7f),				// Constant acceleration
		XMFLOAT2(0, 0),					//start rotation
		XMFLOAT2(0, 0),						//end rotation
		device,
		particleTextureSmoke,
		samplerOptions,
		context
	);

	traceEmitter = std::make_shared<Emitter>(
		XMFLOAT3(3.0f, -2.0f, 0.0f),			// Emitter position
		particleVS,
		particlePS,
		15,									// Max particles
		2,										// Particles per second
		5,										// Particle lifetime
		0.5f,									// Start size
		2.5f,									// End size
		XMFLOAT4(0.2f, 1.0f, 0.2f, 0.0f),		// Start color
		XMFLOAT4(0.5f, 1.0f, 0.5f, 0.1f),		// End color
		XMFLOAT3(2.0f, 1.0f, 0),				// Start velocity
		XMFLOAT3(2.5f, 0.01f, 0.05f),			// Position randomness range
		XMFLOAT3(0.1f, 0.1, 0.1f),				// Velocity randomness range
		XMFLOAT3(-2.0f, 0.0f, 0.0f),				// Constant acceleration
		XMFLOAT2(-XM_PI, 0),					//start rotation
		XMFLOAT2(XM_PI, 0),						//end rotation
		device,
		particleTextureTrace,
		samplerOptions,
		context
		);

	emitters.push_back(snowEmitter);
	emitters.push_back(flameEmitter);
	//emitters.push_back(traceEmitter);




	// Save assets needed for drawing point lights
	lightMesh = sphereMesh;
	lightVS = vertexShader;
	lightPS = solidColorPS;
}




// --------------------------------------------------------
// Generates the lights in the scene: 3 directional lights
// and many random point lights.
// --------------------------------------------------------
void Game::GenerateLights()
{
	// Reset
	lights.clear();

	// Setup directional lights
	Light dir1 = {};
	dir1.Type = LIGHT_TYPE_DIRECTIONAL;
	dir1.Direction = XMFLOAT3(1, -1, 1);
	dir1.Color = XMFLOAT3(0.8f, 0.8f, 0.8f);
	dir1.Intensity = 1.0f;

	Light dir2 = {};
	dir2.Type = LIGHT_TYPE_DIRECTIONAL;
	dir2.Direction = XMFLOAT3(-1, -0.25f, 0);
	dir2.Color = XMFLOAT3(0.2f, 0.2f, 0.2f);
	dir2.Intensity = 1.0f;

	Light dir3 = {};
	dir3.Type = LIGHT_TYPE_DIRECTIONAL;
	dir3.Direction = XMFLOAT3(0, -1, 1);
	dir3.Color = XMFLOAT3(0.2f, 0.2f, 0.2f);
	dir3.Intensity = 1.0f;

	// Add light to the list
	lights.push_back(dir1);
	lights.push_back(dir2);
	lights.push_back(dir3);

	// Create the rest of the lights
	while (lights.size() < lightCount)
	{
		Light point = {};
		point.Type = LIGHT_TYPE_POINT;
		point.Position = XMFLOAT3(RandomRange(-10.0f, 10.0f), RandomRange(-5.0f, 5.0f), RandomRange(-10.0f, 10.0f));
		point.Color = XMFLOAT3(RandomRange(0, 1), RandomRange(0, 1), RandomRange(0, 1));
		point.Range = RandomRange(5.0f, 10.0f);
		point.Intensity = RandomRange(0.1f, 3.0f);

		// Add to the list
		lights.push_back(point);
	}

}




// --------------------------------------------------------
// Handle resizing DirectX "stuff" to match the new window size.
// For instance, updating our projection matrix's aspect ratio.
// --------------------------------------------------------
void Game::OnResize()
{
	renderer->PreResize();

	// Handle base-level DX resize stuff
	DXCore::OnResize();

	renderer->PostResize(width,height, backBufferRTV, depthStencilView);

	// Update our projection matrix to match the new aspect ratio
	if (camera)
		camera->UpdateProjectionMatrix(this->width / (float)this->height);
}

// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	//Get input once
	Input& input = Input::GetInstance();



	// IMGUI
	// Reset input manager's gui state
	// so we don't taint our own input
	input.SetGuiKeyboardCapture(false);
	input.SetGuiMouseCapture(false);



	// Set io info
	ImGuiIO& io = ImGui::GetIO();
	io.DeltaTime = deltaTime;
	io.DisplaySize.x = (float)this->width;
	io.DisplaySize.y = (float)this->height;
	io.KeyCtrl = input.KeyDown(VK_CONTROL);
	io.KeyShift = input.KeyDown(VK_SHIFT);
	io.KeyAlt = input.KeyDown(VK_MENU);
	io.MousePos.x = (float)input.GetMouseX();
	io.MousePos.y = (float)input.GetMouseY();
	io.MouseDown[0] = input.MouseLeftDown();
	io.MouseDown[1] = input.MouseRightDown();
	io.MouseDown[2] = input.MouseMiddleDown();
	io.MouseWheel = input.GetMouseWheel();
	input.GetKeyArray(io.KeysDown, 256);

	// Reset the frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// Determine new input capture
	input.SetGuiKeyboardCapture(io.WantCaptureKeyboard);
	input.SetGuiMouseCapture(io.WantCaptureMouse);

	// Show the demo window
	//ImGui::ShowDemoWindow();
	
	if (guiActive) {
	
		//ImGui::Begin("Engine Stats");

		//ImGui::BulletText("FPS: %.0f", ImGui::GetIO().Framerate);
		//ImGui::BulletText("Width: %d | Height: %d", width, height);
		//ImGui::BulletText("# of Lights: %d", lights.size());
		//ImGui::BulletText("# of Entities: %d", entities.size());


		//ImGui::End();

		ImGui::Begin("Engine Elements");

		if (ImGui::CollapsingHeader("Engine Stats")){
			ImGui::BulletText("FPS: %.0f", ImGui::GetIO().Framerate);
			ImGui::BulletText("Width: %d | Height: %d", width, height);
			ImGui::BulletText("# of Lights: %d", lights.size());
			ImGui::BulletText("# of Entities: %d", entities.size());
		}
		if (ImGui::CollapsingHeader("Camera")) {
			XMFLOAT3 cameraPos = camera->GetTransform()->GetPosition();
			XMFLOAT3 cameraRot = camera->GetTransform()->GetPitchYawRoll();


			if (ImGui::DragFloat3("Camera Position", &cameraPos.x, 0.25f)) {
				camera->GetTransform()->SetPosition(cameraPos.x, cameraPos.y, cameraPos.z);
			}

			if (ImGui::DragFloat3("Camera Rotation", &cameraRot.x, 0.1f)) {
				camera->GetTransform()->SetRotation(cameraRot.x, cameraRot.y, cameraRot.z);
			}

		}


		if (ImGui::CollapsingHeader("Entities")) {
			for (int i = 0; i < entities.size(); i++) {
				XMFLOAT3 position = entities[i].get()->GetTransform()->GetPosition();
				XMFLOAT3 rotation = entities[i].get()->GetTransform()->GetPitchYawRoll();
				XMFLOAT3 scale = entities[i].get()->GetTransform()->GetScale();

				std::string name = "Entity " + std::to_string(i);
				if (ImGui::TreeNode(name.c_str())) {
					if (ImGui::DragFloat3("Position", &position.x, 0.25f)) {
						entities[i]->GetTransform()->SetPosition(position.x, position.y, position.z);
					}
					if (ImGui::DragFloat3("Rotation", &rotation.x, 0.25f)) {
						entities[i]->GetTransform()->SetRotation(rotation.x, rotation.y, rotation.z);
					}
					if (ImGui::DragFloat3("Scale", &scale.x, 0.25f)) {
						entities[i]->GetTransform()->SetScale(scale.x, scale.y, scale.z);
					}
					ImGui::TreePop();
				}
			}
		}

		if (ImGui::CollapsingHeader("Lights")) {
			ImGui::Checkbox("Draw Point Light Meshes", &lightsOn); // show or hide the colored spheres indicating light position and color
			if(ImGui::Button("Randomize Lights")) for(int i=0; i<lightCount; i++)lights[i].Position = XMFLOAT3(RandomRange(-10.0f, 10.0f), RandomRange(-5.0f, 5.0f), RandomRange(-10.0f, 10.0f));
		}

		if (ImGui::CollapsingHeader("IBL Textures")) {
			ImGui::Image(sky->GetBRDFLookUpTexture().Get(), ImVec2(200,200));
			ImGui::Image(FlatNormalMapTest.Get(), ImVec2(200, 200));
			ImVec2 size = ImGui::GetItemRectSize();
			float rtHeight = size.x * ((float)height / width);
			ImGui::Image(renderer->GetRandomTexture().Get(), ImVec2(size.x, rtHeight));

		}

		if (ImGui::CollapsingHeader("Render Targets")) {
			ImVec2 size = ImGui::GetItemRectSize();
			float rtHeight = size.x * ((float)height / width);
			for (int i = 0; i < RenderTargetType::RENDER_TARGET_TYPE_COUNT; i++) {
				ImGui::Image(renderer->GetRenderTargetSRV((RenderTargetType)i).Get(), ImVec2(size.x, rtHeight));
			}
		}

		if (ImGui::CollapsingHeader("Volumetric Lighting")) {
			ImGui::Checkbox("Render Light Rays", &renderer->useLightRays);
			ImGui::SliderFloat("Falloff Exponent", &renderer->lightRaySunFalloffExponent, 1.0f, 512.0f);
			ImGui::SliderInt("Number of Samples", &renderer->numLightRaySamples, 2, 256);
			ImGui::SliderFloat("Light Decay", &renderer->lightDecay, 0.0f, 1.0f);
			ImGui::SliderFloat("Exposure", &renderer->lightRayExposure, 0.0f, 1.0f);
		}

		if (ImGui::CollapsingHeader("Refractive Objects")) {
			ImGui::Checkbox("Render Refractive Objects", &renderer->useRefraction);
			ImGui::SliderFloat("Refraction Scale", &renderer->refractionScale, 0.05f, 0.2f);
		}

		ImGui::End();
	}
	

	// Update the camera
	camera->Update(deltaTime);

	for (auto& emitter : emitters) {
		emitter->Update(deltaTime, totalTime);
	}

	if (input.KeyDown(VK_ESCAPE)) Quit();
	if (input.KeyPress(VK_TAB)) GenerateLights();
	if (input.KeyPress('I')) {
		guiActive = !guiActive;
		//entities.clear();
	}
}

// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------

void Game::Draw(float deltaTime, float totalTime)
{
	renderer->Render(camera, totalTime);
	DrawUI();
}


// --------------------------------------------------------
// Draws the point lights as solid color spheres
// --------------------------------------------------------
void Game::DrawPointLights()
{
	// Turn on these shaders
	lightVS->SetShader();
	lightPS->SetShader();

	// Set up vertex shader
	lightVS->SetMatrix4x4("view", camera->GetView());
	lightVS->SetMatrix4x4("projection", camera->GetProjection());

	for (int i = 0; i < lightCount; i++)
	{
		Light light = lights[i];

		// Only drawing points, so skip others
		if (light.Type != LIGHT_TYPE_POINT)
			continue;

		// Calc quick scale based on range
		float scale = light.Range / 20.0f;

		// Make the transform for this light
		XMMATRIX rotMat = XMMatrixIdentity();
		XMMATRIX scaleMat = XMMatrixScaling(scale, scale, scale);
		XMMATRIX transMat = XMMatrixTranslation(light.Position.x, light.Position.y, light.Position.z);
		XMMATRIX worldMat = scaleMat * rotMat * transMat;

		XMFLOAT4X4 world;
		XMFLOAT4X4 worldInvTrans;
		XMStoreFloat4x4(&world, worldMat);
		XMStoreFloat4x4(&worldInvTrans, XMMatrixInverse(0, XMMatrixTranspose(worldMat)));

		// Set up the world matrix for this light
		lightVS->SetMatrix4x4("world", world);
		lightVS->SetMatrix4x4("worldInverseTranspose", worldInvTrans);

		// Set up the pixel shader data
		XMFLOAT3 finalColor = light.Color;
		finalColor.x *= light.Intensity;
		finalColor.y *= light.Intensity;
		finalColor.z *= light.Intensity;
		lightPS->SetFloat3("Color", finalColor);

		// Copy data
		lightVS->CopyAllBufferData();
		lightPS->CopyAllBufferData();

		// Draw
		lightMesh->SetBuffersAndDraw(context);
	}

}


// --------------------------------------------------------
// Draws a simple informational "UI" using sprite batch
// --------------------------------------------------------
void Game::DrawUI()
{
	spriteBatch->Begin();

	// Basic controls
	float h = 10.0f;
	arial->DrawString(spriteBatch.get(), L"Controls:", XMVectorSet(10, h, 0, 0));
	arial->DrawString(spriteBatch.get(), L" (WASD, X, Space) Move camera", XMVectorSet(10, h + 20, 0, 0));
	arial->DrawString(spriteBatch.get(), L" (Left Click & Drag) Rotate camera", XMVectorSet(10, h + 40, 0, 0));
	arial->DrawString(spriteBatch.get(), L" (Left Shift) Hold to speed up camera", XMVectorSet(10, h + 60, 0, 0));
	arial->DrawString(spriteBatch.get(), L" (Left Ctrl) Hold to slow down camera", XMVectorSet(10, h + 80, 0, 0));
	arial->DrawString(spriteBatch.get(), L" (TAB) Randomize lights", XMVectorSet(10, h + 100, 0, 0));
	arial->DrawString(spriteBatch.get(), L" (I) Turn ImGui On/Off", XMVectorSet(10, h + 120, 0, 0));

	// Current "scene" info
	h = 150;
	arial->DrawString(spriteBatch.get(), L"Scene Details:", XMVectorSet(10, h, 0, 0));
	arial->DrawString(spriteBatch.get(), L" Top: PBR materials", XMVectorSet(10, h + 20, 0, 0));
	arial->DrawString(spriteBatch.get(), L" Bottom: Non-PBR materials", XMVectorSet(10, h + 40, 0, 0));

	spriteBatch->End();

	// Reset render states, since sprite batch changes these!
	context->OMSetBlendState(0, 0, 0xFFFFFFFF);
	context->OMSetDepthStencilState(0, 0);

}
