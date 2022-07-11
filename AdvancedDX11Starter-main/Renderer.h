#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include "Camera.h"
#include "GameEntity.h"
#include "Lights.h"
#include "Sky.h"
#include "DXCore.h"
#include "Emitter.h"

enum RenderTargetType {
	SCENE_COLORS_NO_AMBIENT,
	SCENE_COLORS,
	SCENE_NORMALS,
	SCENE_DEPTHS,
	SCENE_SKY_AND_OCCLUDERS,
	SSAO_RESULTS,
	SSAO_BLUR,
	FINAL_COMPOSITE,
	RENDER_TARGET_TYPE_COUNT
};

class Renderer
{

public:
	Renderer(
		Microsoft::WRL::ComPtr<ID3D11Device> device,
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
		Microsoft::WRL::ComPtr<IDXGISwapChain> swapchain,
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV,
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV,
		unsigned int windowWidth,
		unsigned int windowHeight,
		std::shared_ptr<Sky> sky,
		std::vector<std::shared_ptr<GameEntity>>&entities,
		std::vector<Light>&lights,
		int lightCount,
		std::shared_ptr<SimplePixelShader> lightPS,
		std::shared_ptr<SimpleVertexShader> lightVS,
		std::shared_ptr<Mesh> lightMesh,
		std::shared_ptr<SimplePixelShader> refractionPS,
		std::shared_ptr<SimpleVertexShader> fullScreenVS,
		std::shared_ptr<SimplePixelShader> simplePS,
		std::shared_ptr<SimplePixelShader> ssaoPS,
		std::shared_ptr<SimplePixelShader> ssaoBlurPS,
		std::shared_ptr<SimplePixelShader> ssaoCombinePS,
		std::shared_ptr<SimplePixelShader> skyPS,
		std::shared_ptr<SimplePixelShader> lightRayPS,
		std::vector<std::shared_ptr<Emitter>>emitters
	);

	void PreResize();
	void PostResize(
		unsigned int windowWidth,
		unsigned int windowHeight,
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV,
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV
	);

	void Render(std::shared_ptr<Camera> camera, float totalTime);
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetRenderTargetSRV(RenderTargetType type);
	void CreateRenderTarget(unsigned int width, unsigned int height,
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView>& rtv,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv,
		DXGI_FORMAT colorFormat);

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetRandomTexture();

	bool drawPointMeshes;
	int lightCount;

	//lightRays
	int numLightRaySamples;
	float lightRayDensity;
	float lightRaySampleWeight;
	float lightDecay;
	float lightRayExposure;
	DirectX::XMFLOAT3 SunDirection;
	float lightRaySunFalloffExponent;
	DirectX::XMFLOAT3 lightRayColor;
	bool useLightRays;

	//refraction
	bool useRefraction;
	float refractionScale;

private:
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	Microsoft::WRL::ComPtr<IDXGISwapChain> swapchain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV;
	unsigned int windowWidth;
	unsigned int windowHeight;
	std::shared_ptr<Sky> sky;
	std::vector<std::shared_ptr<GameEntity>>& entities;
	std::vector<Light>& lights;
	std::shared_ptr<SimplePixelShader> lightPS;
	std::shared_ptr<SimpleVertexShader> lightVS;
	std::shared_ptr<Mesh> lightMesh;

	//MRT's
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetRTVs[RenderTargetType::RENDER_TARGET_TYPE_COUNT];
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> renderTargetSRVs[RenderTargetType::RENDER_TARGET_TYPE_COUNT];
	std::shared_ptr<SimplePixelShader> simplePS;

	//Refraction
	std::shared_ptr<SimplePixelShader> refractionPS;
	std::shared_ptr<SimpleVertexShader> fullScreenVS;

	//SSAO
	std::shared_ptr<SimplePixelShader> ssaoPS;
	std::shared_ptr<SimplePixelShader> ssaoBlurPS;
	std::shared_ptr<SimplePixelShader> ssaoCombinePS;
	DirectX::XMFLOAT4 ssaoOffsets[64];
	int ssaoSamples;
	float ssaoRadius;
	void CreateRandomTexture();
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> randomTexture;


	//for particles
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> particleDepthState;
	Microsoft::WRL::ComPtr<ID3D11BlendState> particleBlendState;
	std::vector<std::shared_ptr<Emitter>>emitters;

	//light rays
	std::shared_ptr<SimplePixelShader> skyPS;
	std::shared_ptr<SimplePixelShader> lightRayPS;




	void DrawPointLights(std::shared_ptr<Camera> camera);
};

