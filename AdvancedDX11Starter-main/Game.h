#pragma once

#include "DXCore.h"
#include "Mesh.h"
#include "GameEntity.h"
#include "Camera.h"
#include "SimpleShader.h"
#include "SpriteFont.h"
#include "SpriteBatch.h"
#include "Lights.h"
#include "Sky.h"
#include "Renderer.h"
#include "Emitter.h"


#include <DirectXMath.h>
#include <wrl/client.h> // Used for ComPtr - a smart pointer for COM objects
#include <vector>

class Game 
	: public DXCore
{

public:
	Game(HINSTANCE hInstance);
	~Game();

	// Overridden setup and game loop methods, which
	// will be called automatically
	void Init();
	void OnResize();
	void Update(float deltaTime, float totalTime);
	void Draw(float deltaTime, float totalTime);
	bool lightsOn = true;

private:

	// Our scene
	std::vector<std::shared_ptr<GameEntity>> entities;
	std::shared_ptr<Camera> camera;

	// Lights
	std::vector<Light> lights;
	int lightCount;

	bool guiActive;

	// These will be loaded along with other assets and
	// saved to these variables for ease of access
	std::shared_ptr<Mesh> lightMesh;
	std::shared_ptr<SimpleVertexShader> lightVS;
	std::shared_ptr<SimplePixelShader> lightPS;

	// Text & ui
	std::shared_ptr<DirectX::SpriteFont> arial;
	std::shared_ptr<DirectX::SpriteBatch> spriteBatch;

	// Texture related resources
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> clampSampler;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> FlatNormalMapTest;

	std::shared_ptr<SimplePixelShader> pixelShaderPBR;
	std::shared_ptr<SimplePixelShader> refractionPS;
	std::shared_ptr<SimpleVertexShader> fullscreenVS;
	std::shared_ptr<SimplePixelShader> simplePS;

	//refraction ps
	std::shared_ptr<SimplePixelShader> ssaoPS;
	std::shared_ptr<SimplePixelShader> ssaoBlurPS;
	std::shared_ptr<SimplePixelShader> ssaoCombinePS;

	std::shared_ptr<SimplePixelShader> lightRayPS;
	std::shared_ptr<SimplePixelShader> skyPS;



	// Skybox
	std::shared_ptr<Sky> sky;

	// General helpers for setup and drawing
	void GenerateLights();
	void DrawPointLights();
	void DrawUI();

	// Initialization helper method
	void LoadAssetsAndCreateEntities();

	Renderer* renderer;

	std::vector<std::shared_ptr<Emitter>>emitters;
	std::shared_ptr<SimpleVertexShader> particleVS;
	std::shared_ptr<SimplePixelShader> particlePS;

	std::shared_ptr<Emitter>snowEmitter;
	std::shared_ptr<Emitter>flameEmitter;
	std::shared_ptr<Emitter>traceEmitter;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleTextureSnow, particleTextureSmoke, particleTextureTrace;
};

