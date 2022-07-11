#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include "Camera.h"
#include "SimpleShader.h"
#include <DirectXMath.h>
#include <memory>


struct Particle {
	float spawnTime;
	DirectX::XMFLOAT3 startPos;

	DirectX::XMFLOAT3 startVelocity;
	float startRotation;
	float endRotation;
	DirectX::XMFLOAT3 padding;

};

class Emitter
{
private:
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	Particle* particles;
	int maxParticles;
	int firstDeadIndex;
	int firstAliveIndex;

	int particlesPerSecond;
	float secondsPerParticle;
	float timeSinceEmit;

	int numAliveParticles;
	float lifetime;

	DirectX::XMFLOAT3 emitterAcceleration;
	DirectX::XMFLOAT3 emitterPosition;
	DirectX::XMFLOAT3 startVelocity;

	DirectX::XMFLOAT3 positionRandomRange;
	DirectX::XMFLOAT3 velocityRandomRange;

	DirectX::XMFLOAT2 rotationStart;
	DirectX::XMFLOAT2 rotationEnd;

	DirectX::XMFLOAT4 startColor;
	DirectX::XMFLOAT4 endColor;
	float startSize;
	float endSize;

	// Rendering
	std::shared_ptr<SimpleVertexShader> vs;
	std::shared_ptr<SimplePixelShader> ps;
	Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> particleDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleDataSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleTexture;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;

	// Update Methods
	void UpdateSingleParticle(float currentTime, int index);
	void SpawnParticle(float currentTime);
	void createBuffers(Microsoft::WRL::ComPtr<ID3D11Device> device);

public:
	Emitter(
		DirectX::XMFLOAT3 emitterPosition,
		std::shared_ptr<SimpleVertexShader> vs,
		std::shared_ptr<SimplePixelShader> ps,
		int maxParticles,
		int particlesPerSecond,
		float lifetime,
		float startSize,
		float endSize,
		DirectX::XMFLOAT4 startColor,
		DirectX::XMFLOAT4 endColor,
		DirectX::XMFLOAT3 startVelocity,
		DirectX::XMFLOAT3 positionRandomRange,
		DirectX::XMFLOAT3 velocityRandomRange,
		DirectX::XMFLOAT3 emitterAcceleration,
		DirectX::XMFLOAT2 rotationStart,
		DirectX::XMFLOAT2 rotationEnd,
		Microsoft::WRL::ComPtr<ID3D11Device> device,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleTexture,
		Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState,
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context
	);

	~Emitter();
	void Update(float dt, float currentTime);
	void Draw(std::shared_ptr<Camera> camera, float currentTime);
	void SetPosition(DirectX::XMFLOAT3 newPos);
};

