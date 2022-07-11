#include "Emitter.h"
using namespace DirectX;

Emitter::Emitter(DirectX::XMFLOAT3 emitterPosition, std::shared_ptr<SimpleVertexShader> vs, std::shared_ptr<SimplePixelShader> ps, int maxParticles, int particlesPerSecond, float lifetime, float startSize, float endSize,
	DirectX::XMFLOAT4 startColor, DirectX::XMFLOAT4 endColor, DirectX::XMFLOAT3 startVelocity, DirectX::XMFLOAT3 positionRandomRange, DirectX::XMFLOAT3 velocityRandomRange,
	DirectX::XMFLOAT3 emitterAcceleration, DirectX::XMFLOAT2 rotationStart, DirectX::XMFLOAT2 rotationEnd,
	Microsoft::WRL::ComPtr<ID3D11Device> device, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleTexture, Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState, Microsoft::WRL::ComPtr<ID3D11DeviceContext> context
)
{
	//set variables
	this->context = context;
	this->emitterPosition = emitterPosition;
	this->vs = vs;
	this->ps = ps;
	this->maxParticles = maxParticles;
	this->particlesPerSecond = particlesPerSecond;
	this->lifetime = lifetime;
	this->startSize = startSize;
	this->endSize = endSize;
	this->startColor = startColor;
	this->endColor = endColor;
	this->startVelocity = startVelocity;
	this->positionRandomRange = positionRandomRange;
	this->velocityRandomRange = velocityRandomRange;
	this->emitterAcceleration = emitterAcceleration;
	this->particleTexture = particleTexture;
	this->rotationEnd = rotationEnd;
	this->rotationStart = rotationStart;

	this->secondsPerParticle = 1.0 / particlesPerSecond;
	timeSinceEmit = 0;
	numAliveParticles = 0;
	firstAliveIndex = 0;
	firstDeadIndex = 0;
	particles = new Particle[maxParticles];
	ZeroMemory(particles, sizeof(Particle) * maxParticles);

	this->sampler = samplerState;

	createBuffers(device);

}

void Emitter::createBuffers(Microsoft::WRL::ComPtr<ID3D11Device> device)
{
	if (particles!=NULL) delete[] particles;
	indexBuffer.Reset();
	particleDataBuffer.Reset();
	particleDataSRV.Reset();
	particles = new Particle[maxParticles];
	ZeroMemory(particles, sizeof(Particle) * maxParticles);

	unsigned int* indices = new unsigned int[(unsigned int)maxParticles * 6];
	int indexCount = 0;

	//index represents 4 corners of image
	for (int i = 0; i < maxParticles * 4; i += 4)
	{
		indices[indexCount++] = i;
		indices[indexCount++] = i + 1;
		indices[indexCount++] = i + 2;
		indices[indexCount++] = i;
		indices[indexCount++] = i + 2;
		indices[indexCount++] = i + 3;
	}
	D3D11_SUBRESOURCE_DATA indexData = {};
	indexData.pSysMem = indices;

	//create and set index buffer
	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.CPUAccessFlags = 0;
	ibDesc.Usage = D3D11_USAGE_DEFAULT;
	ibDesc.ByteWidth = sizeof(unsigned int) * maxParticles * 6;
	device->CreateBuffer(&ibDesc, &indexData, indexBuffer.GetAddressOf());
	delete[] indices;

	//create and set particle buffer
	D3D11_BUFFER_DESC allParticleBufferDesc = {};
	allParticleBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	allParticleBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	allParticleBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	allParticleBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	allParticleBufferDesc.StructureByteStride = sizeof(Particle);
	allParticleBufferDesc.ByteWidth = sizeof(Particle) * maxParticles;
	device->CreateBuffer(&allParticleBufferDesc, 0, particleDataBuffer.GetAddressOf());

	//create and set particleSRV
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = maxParticles;
	device->CreateShaderResourceView(particleDataBuffer.Get(), &srvDesc, particleDataSRV.GetAddressOf());


}

Emitter::~Emitter()
{
	//only thing we created on the heap
	delete[] particles;
}

void Emitter::Update(float dt, float currentTime)
{
	//first check to see that there are alive particles
	if (numAliveParticles > 0) {

		//when we don't need to wrap around array
		if (firstAliveIndex < firstDeadIndex) {
			for (int i = firstAliveIndex; i < firstDeadIndex; i++) {
				UpdateSingleParticle(currentTime, i);
			}
		}//when we do need to wrap around the array
		else if (firstAliveIndex > firstDeadIndex) {
			for (int i = firstAliveIndex; i < maxParticles; i++) {
				UpdateSingleParticle(currentTime, i);
			}
			for (int i = 0; i < firstDeadIndex; i++) {
				UpdateSingleParticle(currentTime, i);

			}
		}//when we need to update all particles
		else {
			for (int i = 0; i < maxParticles; i++) {
				UpdateSingleParticle(currentTime, i);
			}
		}
	}

	//do this no matter what particles we need to update
	timeSinceEmit += dt;

	//spawn more particles if it's time
	while (timeSinceEmit > secondsPerParticle) {
		SpawnParticle(currentTime);
		timeSinceEmit -= secondsPerParticle;
	}

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	context->Map(particleDataBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

	if (firstAliveIndex < firstDeadIndex) {
		memcpy(mapped.pData, particles + firstAliveIndex, sizeof(Particle) * numAliveParticles);
	}
	else {
		memcpy(mapped.pData, particles, sizeof(Particle) * firstDeadIndex);
		memcpy(
			(void*)((Particle*)mapped.pData + firstDeadIndex),
			particles + firstAliveIndex,
			sizeof(Particle) * (maxParticles - firstAliveIndex));

	}
	context->Unmap(particleDataBuffer.Get(), 0);
}

void Emitter::UpdateSingleParticle(float currentTime, int index)
{
	//calculate how long this particle has been alive
	float timeAlive = currentTime - particles[index].spawnTime;

	//check to see if it's time to die, kill it if it is
	if (timeAlive >= lifetime) {
		firstAliveIndex = (firstAliveIndex + 1) % maxParticles; //take mod so we can wrap around when at last index
		numAliveParticles--;
	}
}

void Emitter::SpawnParticle(float currentTime)
{
	//only spawn if we aren't at max particles
	if (numAliveParticles < maxParticles) {
		particles[firstDeadIndex].spawnTime = currentTime;

		//get random position based around emitterPosition
		DirectX::XMFLOAT3 tempPos = emitterPosition;
		tempPos.x += (((float)rand() / RAND_MAX) * 2 - 1) * positionRandomRange.x;
		tempPos.y += (((float)rand() / RAND_MAX) * 2 - 1) * positionRandomRange.y;
		tempPos.z += (((float)rand() / RAND_MAX) * 2 - 1) * positionRandomRange.z;
		particles[firstDeadIndex].startPos = tempPos;

		DirectX::XMFLOAT3 tempVelocity = startVelocity;
		tempVelocity.x += (rand()) / (float)(RAND_MAX / (velocityRandomRange.x * 2));
		tempVelocity.y += (rand()) / (float)(RAND_MAX / (velocityRandomRange.y * 2));
		tempVelocity.z += (rand()) / (float)(RAND_MAX / (velocityRandomRange.z * 2));
		particles[firstDeadIndex].startVelocity = tempVelocity;

		particles[firstDeadIndex].startRotation = ((float)(rand()) / (float)RAND_MAX) * (rotationStart.y - rotationStart.x) + rotationStart.x;
		particles[firstDeadIndex].endRotation = ((float)(rand()) / (float)RAND_MAX) * (rotationEnd.y - rotationEnd.x) + rotationEnd.x;


		//update parameters
		firstDeadIndex = (firstDeadIndex + 1) % maxParticles; //take mod so we can wrap around when at last index
		numAliveParticles++;
	}
}

void Emitter::Draw(std::shared_ptr<Camera> camera, float currentTime)
{
	//set up buffers for drawing
	//D3D11_MAPPED_SUBRESOURCE mapped = {};
	//context->Map(particleDataBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	//memcpy(mapped.pData, particles, sizeof(Particle) * maxParticles);
	//context->Unmap(particleDataBuffer.Get(), 0);

	UINT stride = 0;
	UINT offset = 0;
	ID3D11Buffer* nullBuffer = 0;
	context->IASetVertexBuffers(0, 1, &nullBuffer, &stride, &offset);
	context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

	vs->SetShader();
	ps->SetShader();

	//set info for vertex Shader 
	vs->SetMatrix4x4("view", camera->GetView());
	vs->SetMatrix4x4("projection", camera->GetProjection());
	vs->SetFloat3("acceleration", emitterAcceleration);
	vs->SetFloat4("startColor", startColor);
	vs->SetFloat4("endColor", endColor);
	vs->SetFloat("startSize", startSize);
	vs->SetFloat("endSize", endSize);
	vs->SetFloat("lifetime", lifetime);
	vs->SetFloat("currentTime", currentTime);
	vs->CopyAllBufferData();

	vs->SetShaderResourceView("ParticleData", particleDataSRV);

	//set info for pixel shader
	ps->SetShaderResourceView("particleTexture", particleTexture.Get());
	ps->SetSamplerState("sampleState", sampler.Get());
	ps->SetShader();

	context->DrawIndexed(numAliveParticles * 6, 0, 0);

	////check to see if we need to wrap
	//if (firstAliveIndex < firstDeadIndex)
	//{
	//	//no wrap
	//	vs->SetInt("startIndex", firstAliveIndex);
	//	context->DrawIndexed(numAliveParticles * 6, 0, 0);
	//}
	//else
	//{
	//	//first part of wrap
	//	vs->SetInt("startIndex", 0);
	//	context->DrawIndexed(firstDeadIndex * 6, 0, 0);

	//	//second part of wrap
	//	vs->SetInt("startIndex", firstAliveIndex);
	//	context->DrawIndexed((maxParticles - firstAliveIndex) * 6, 0, 0);
	//}
}

void Emitter::SetPosition(DirectX::XMFLOAT3 newPos)
{
	this->emitterPosition = newPos;
}
