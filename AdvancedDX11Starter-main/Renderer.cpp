#include "Renderer.h"
#include <DirectXMath.h>
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui\imgui_impl_win32.h"
#include "SimpleShader.h"

using namespace DirectX;

Renderer::Renderer(Microsoft::WRL::ComPtr<ID3D11Device> device, 
				  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, 
				  Microsoft::WRL::ComPtr<IDXGISwapChain> swapchain, 
				  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV, 
				  Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV, 
				  unsigned int windowWidth, unsigned int windowHeight, 
				  std::shared_ptr<Sky> sky, std::vector<std::shared_ptr<GameEntity>>&entities,
				  std::vector<Light>&lights,
				  int lightCount, std::shared_ptr<SimplePixelShader> lightPS,
				  std::shared_ptr<SimpleVertexShader> lightVS, std::shared_ptr<Mesh> lightMesh,
				  std::shared_ptr<SimplePixelShader> refractionPS, std::shared_ptr<SimpleVertexShader> fullScreenVS,
				  std::shared_ptr<SimplePixelShader> simplePS,
				  std::shared_ptr<SimplePixelShader> ssaoPS,
				  std::shared_ptr<SimplePixelShader> ssaoBlurPS,
				  std::shared_ptr<SimplePixelShader> ssaoCombinePS,
				  std::shared_ptr<SimplePixelShader> skyPS,
				  std::shared_ptr<SimplePixelShader> lightRayPS,
				  std::vector<std::shared_ptr<Emitter>>emitters): entities(entities), lights(lights)
{
	this->device = device;
	this->context = context;
	this->swapchain = swapchain;
	this->backBufferRTV = backBufferRTV;
	this->depthBufferDSV = depthBufferDSV;
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;
	this->sky = sky;
	this->lightCount = lightCount;
	this->lightVS = lightVS;
	this->lightPS = lightPS;
	this->lightMesh = lightMesh;
	this->refractionPS = refractionPS;
	this->fullScreenVS = fullScreenVS;
	this->simplePS = simplePS;
	this->ssaoPS = ssaoPS;
	this->ssaoBlurPS = ssaoBlurPS;
	this->ssaoCombinePS = ssaoCombinePS;
	this->emitters = emitters;
	this->numLightRaySamples = 128;
	this->lightRayDensity = 1.0f;
	this->lightRaySampleWeight = 0.2f;
	this->lightDecay = 0.98f;
	this->lightRayExposure = 0.02f;
	this->SunDirection = XMFLOAT3(0,0,1);
	this->lightRaySunFalloffExponent = 128.0f;
	this->lightRayColor = XMFLOAT3(1,1,1);
	this->skyPS = skyPS;
	this->lightRayPS = lightRayPS;
	this->refractionScale = 0.1f;
	this->drawPointMeshes = true;


	PostResize(windowWidth, windowHeight, backBufferRTV, depthBufferDSV);

	//set up ssao offsets
	for (int i = 0; i < ARRAYSIZE(ssaoOffsets); i++) {
	
		ssaoOffsets[i] = XMFLOAT4(
			(float)rand()/RAND_MAX*2-1,
			(float)rand()/RAND_MAX*2-1,
			(float)rand()/RAND_MAX,
			0
		);
		XMVECTOR vector = XMVector3Normalize(XMLoadFloat4(&ssaoOffsets[i]));
		//scale up over array
		float scale = (float)i / ARRAYSIZE(ssaoOffsets);
		XMVECTOR scaleVector = XMVectorLerp(
			XMVectorSet(0.1f, 0.1f, 0.1f, 1),
			XMVectorSet(1,1,1,1),
			scale*scale);
		XMStoreFloat4(&ssaoOffsets[i], vector * scaleVector);
	}
	CreateRandomTexture();

	//create depth and blend states for particles
	D3D11_DEPTH_STENCIL_DESC dsDesc = {};
	dsDesc.DepthEnable = true;
	dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
	device->CreateDepthStencilState(&dsDesc, particleDepthState.GetAddressOf());

	//additive blend
	D3D11_BLEND_DESC blend = {};
	//blend.AlphaToCoverageEnable = false;
	//blend.IndependentBlendEnable = false;
	blend.RenderTarget[0].BlendEnable = true;
	blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blend.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blend.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	device->CreateBlendState(&blend, particleBlendState.GetAddressOf());
}

void Renderer::PreResize()
{
	backBufferRTV.Reset();
	depthBufferDSV.Reset();
}

void Renderer::PostResize(unsigned int windowWidth, unsigned int windowHeight, Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV, Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV)
{
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;
	this->backBufferRTV = backBufferRTV;
	this->depthBufferDSV = depthBufferDSV;

	//Reset Render targets
	for (auto& rt : renderTargetRTVs) rt.Reset();
	for (auto& rt : renderTargetSRVs) rt.Reset();

	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SCENE_COLORS_NO_AMBIENT], renderTargetSRVs[RenderTargetType::SCENE_COLORS_NO_AMBIENT], DXGI_FORMAT_R8G8B8A8_UNORM);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SCENE_COLORS], renderTargetSRVs[RenderTargetType::SCENE_COLORS], DXGI_FORMAT_R8G8B8A8_UNORM);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SCENE_NORMALS], renderTargetSRVs[RenderTargetType::SCENE_NORMALS], DXGI_FORMAT_R8G8B8A8_UNORM);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SCENE_DEPTHS], renderTargetSRVs[RenderTargetType::SCENE_DEPTHS], DXGI_FORMAT_R32_FLOAT);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SCENE_SKY_AND_OCCLUDERS], renderTargetSRVs[RenderTargetType::SCENE_SKY_AND_OCCLUDERS], DXGI_FORMAT_R8G8B8A8_UNORM);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::FINAL_COMPOSITE], renderTargetSRVs[RenderTargetType::FINAL_COMPOSITE], DXGI_FORMAT_R8G8B8A8_UNORM);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SSAO_BLUR], renderTargetSRVs[RenderTargetType::SSAO_BLUR], DXGI_FORMAT_R8G8B8A8_UNORM);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SSAO_RESULTS], renderTargetSRVs[RenderTargetType::SSAO_RESULTS], DXGI_FORMAT_R8G8B8A8_UNORM);


}

void Renderer::Render(std::shared_ptr<Camera> camera, float totalTime)
{
	// Background color for clearing
	const float color[4] = { 0, 0, 0, 1 };

	//clearing RTVs 
	context->ClearRenderTargetView(backBufferRTV.Get(), color);
	context->ClearDepthStencilView(depthBufferDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	for (auto& renderTarget : renderTargetRTVs) {
		context->ClearRenderTargetView(renderTarget.Get(), color);
	}
	//need to clear depth separately
	const float depth[4] = { 1,0,0,0 };
	context->ClearRenderTargetView(renderTargetRTVs[SCENE_DEPTHS].Get(), depth);

	//set up RT's
	const int numTargets = 5;
	ID3D11RenderTargetView* targets[numTargets] = {};
	targets[0] = renderTargetRTVs[RenderTargetType::SCENE_COLORS_NO_AMBIENT].Get();
	targets[1] = renderTargetRTVs[RenderTargetType::SCENE_COLORS].Get();
	targets[2] = renderTargetRTVs[RenderTargetType::SCENE_NORMALS].Get();
	targets[3] = renderTargetRTVs[RenderTargetType::SCENE_DEPTHS].Get();
	targets[4] = renderTargetRTVs[RenderTargetType::SCENE_SKY_AND_OCCLUDERS].Get();
	context->OMSetRenderTargets(numTargets, targets, depthBufferDSV.Get());


	// Clear the render target and depth buffer (erases what's on the screen)
	//  - Do this ONCE PER FRAME
	//  - At the beginning of Draw (before drawing *anything*)
	context->ClearRenderTargetView(backBufferRTV.Get(), color);
	context->ClearDepthStencilView(
		depthBufferDSV.Get(),
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
		1.0f,
		0);

	std::vector<std::shared_ptr<GameEntity>> refractiveEntities;

	// Draw all of the entities
	for (auto& ge : entities)
	{
		if (ge->GetMaterial()->GetRefractive()) {
			refractiveEntities.push_back(ge);
			continue;
		}
		// Set the "per frame" data
		// Note that this should literally be set once PER FRAME, before
		// the draw loop, but we're currently setting it per entity since 
		// we are just using whichever shader the current entity has.  
		// Inefficient!!!
		std::shared_ptr<SimplePixelShader> ps = ge->GetMaterial()->GetPixelShader();
		ps->SetData("lights", (void*)(&lights[0]), sizeof(Light) * lightCount);
		ps->SetInt("lightCount", lightCount);
		ps->SetFloat3("cameraPosition", camera->GetTransform()->GetPosition());
		ps->SetInt("SpecIBLTotalMipLevels", sky->GetSpecIBLMipLevels());
		ps->SetShaderResourceView("BrdfLookUpMap", sky->GetBRDFLookUpTexture());
		ps->SetShaderResourceView("IrradianceIBLMap", sky->GetIrradianceMap());
		ps->SetShaderResourceView("SpecularIBLMap", sky->GetSpecularMap());

		ps->SetShaderResourceView("colorNoAmbient", renderTargetSRVs[RenderTargetType::SCENE_COLORS_NO_AMBIENT]);
		ps->SetShaderResourceView("sceneColors", renderTargetSRVs[RenderTargetType::SCENE_COLORS]);
		ps->SetShaderResourceView("sceneNormals", renderTargetSRVs[RenderTargetType::SCENE_NORMALS]);
		ps->SetShaderResourceView("sceneDepths", renderTargetSRVs[RenderTargetType::SCENE_DEPTHS]);
		ps->SetShaderResourceView("skyAndOccluders", renderTargetSRVs[RenderTargetType::SCENE_SKY_AND_OCCLUDERS]);

		ps->CopyBufferData("perFrame");



		// Draw the entity
		ge->Draw(context, camera);
	}

	// Draw the light sources
	if(drawPointMeshes)DrawPointLights(camera);

	// Draw the sky
	skyPS->SetFloat3("sunDirection", SunDirection);
	skyPS->SetFloat("falloffExponent", lightRaySunFalloffExponent);
	skyPS->SetFloat3("sunColor", lightRayColor);
	skyPS->CopyAllBufferData();
	sky->Draw(camera);

	//draw ssao (NOTE: SSAO DOESNT WORK)
	fullScreenVS->SetShader();
	targets[0] = renderTargetRTVs[RenderTargetType::SSAO_RESULTS].Get();
	targets[1] = 0;
	targets[2] = 0;
	targets[3] = 0;
	context->OMSetRenderTargets(numTargets, targets, 0);
	ssaoPS->SetShader();

	XMFLOAT4X4 invView, invProj, view = camera->GetView(), proj = camera->GetProjection();
	XMStoreFloat4x4(&invView, XMMatrixInverse(0, XMLoadFloat4x4(&view)));
	XMStoreFloat4x4(&invProj, XMMatrixInverse(0, XMLoadFloat4x4(&proj)));
	ssaoPS->SetMatrix4x4("invViewMatrix", invView);
	ssaoPS->SetMatrix4x4("invProjMatrix", invProj);
	ssaoPS->SetMatrix4x4("viewMatrix", view);
	ssaoPS->SetMatrix4x4("projectionMatrix", proj);
	ssaoPS->SetData("offsets", ssaoOffsets, sizeof(XMFLOAT4) * ARRAYSIZE(ssaoOffsets));
	ssaoPS->SetFloat("ssaoRadius", ssaoRadius);
	ssaoPS->SetInt("ssaoSamples", ssaoSamples);
	ssaoPS->SetFloat2("randomTextureScreenScale", XMFLOAT2(windowWidth / 4.0f, windowHeight / 4.0f));
	ssaoPS->CopyAllBufferData();
	ssaoPS->SetShaderResourceView("Normals", renderTargetSRVs[RenderTargetType::SCENE_NORMALS]);
	ssaoPS->SetShaderResourceView("Depths", renderTargetSRVs[RenderTargetType::SCENE_DEPTHS]);
	ssaoPS->SetShaderResourceView("Random", randomTexture.Get());
	context->Draw(3, 0);

	//SSAO blur
	targets[0] = renderTargetRTVs[RenderTargetType::SSAO_BLUR].Get();
	context->OMSetRenderTargets(1, targets, 0);
	ssaoBlurPS->SetShader();

	ssaoBlurPS->SetShaderResourceView("SSAO", renderTargetSRVs[RenderTargetType::SSAO_RESULTS]);
	ssaoBlurPS->SetFloat2("pixelSize", XMFLOAT2(1.0f / windowWidth, 1.0f / windowHeight));
	ssaoBlurPS->CopyAllBufferData();
	context->Draw(3, 0);

	//SSAO Combine
	targets[0] = renderTargetRTVs[RenderTargetType::FINAL_COMPOSITE].Get();
	context->OMSetRenderTargets(1, targets, 0);
	ssaoCombinePS->SetShader();
	ssaoCombinePS->SetShaderResourceView("SceneColorsNoAmbient", renderTargetSRVs[RenderTargetType::SCENE_COLORS_NO_AMBIENT]);
	ssaoCombinePS->SetShaderResourceView("Ambient", renderTargetSRVs[RenderTargetType::SCENE_COLORS]);
	ssaoCombinePS->SetShaderResourceView("SSAOBlur", renderTargetSRVs[RenderTargetType::SSAO_BLUR]);
	ssaoCombinePS->SetFloat2("pixelSize", XMFLOAT2(1.0f / windowWidth, 1.0f / windowHeight));
	ssaoCombinePS->CopyAllBufferData();
	context->Draw(3, 0);



	//draw final results
	fullScreenVS->SetShader();
	targets[0] = backBufferRTV.Get();
	context->OMSetRenderTargets(1, targets, 0);

	simplePS->SetShader();
	simplePS->SetShaderResourceView("pixels", renderTargetSRVs[RenderTargetType::SCENE_COLORS_NO_AMBIENT].Get());

	context->Draw(3, 0);
	context->OMSetRenderTargets(1, targets, depthBufferDSV.Get());

	//draw light rays
	if (useLightRays) {
		fullScreenVS->SetShader();
		targets[0] = backBufferRTV.Get();
		context->OMSetRenderTargets(1, targets, 0);

		view = camera->GetView();
		proj = camera->GetProjection();
		XMVECTOR lightPosinWorld = XMLoadFloat3(&SunDirection);
		XMVECTOR lightPosScreenVec = XMVector4Transform(lightPosinWorld, XMLoadFloat4x4(&view) * XMLoadFloat4x4(&proj));
		lightPosScreenVec /= XMVectorGetW(lightPosScreenVec); // divide by the perspective

		XMFLOAT2 lightPosScreen;
		XMStoreFloat2(&lightPosScreen, lightPosScreenVec);

		lightRayPS->SetShader();
		lightRayPS->SetShaderResourceView("SkyAndOccluders", renderTargetSRVs[RenderTargetType::SCENE_SKY_AND_OCCLUDERS]);
		lightRayPS->SetShaderResourceView("FinalScene", renderTargetSRVs[RenderTargetType::SCENE_COLORS_NO_AMBIENT]);
		lightRayPS->SetInt("numSamples", numLightRaySamples);
		lightRayPS->SetFloat("density", lightRayDensity);
		lightRayPS->SetFloat("weight", lightRaySampleWeight);
		lightRayPS->SetFloat("decay", lightDecay);
		lightRayPS->SetFloat("exposure", lightRayExposure);
		lightRayPS->SetFloat2("lightPosScreenSpace", lightPosScreen);
		lightRayPS->CopyAllBufferData();
		context->Draw(3, 0);
	}
	

	//draw refraction
	if (useRefraction) {
		fullScreenVS->SetShader();
		targets[0] = backBufferRTV.Get();
		context->OMSetRenderTargets(1, targets, 0);

		for (auto refractiveGE : refractiveEntities) {
			std::shared_ptr<Material> material = refractiveGE->GetMaterial();
			std::shared_ptr<SimplePixelShader> prevPS = material->GetPixelShader(); //get materials PS so we can set it back later after refraction
			material->SetPixelShader(refractionPS);
			refractionPS->SetData("lights", (void*)(&lights[0]), sizeof(Light) * lightCount);
			refractionPS->SetInt("lightCount", lightCount);
			refractionPS->SetFloat3("cameraPosition", camera->GetTransform()->GetPosition());
			refractionPS->SetInt("SpecIBLTotalMipLevels", sky->GetSpecIBLMipLevels());
			refractionPS->SetFloat2("screenSize", XMFLOAT2((float)windowWidth, (float)windowHeight));
			refractionPS->SetFloat("refractionScale", refractionScale);
			refractionPS->CopyBufferData("perFrame");

			refractionPS->SetShaderResourceView("NormalTexture", material->GetTextureSRV("NormalMap"));
			refractionPS->SetShaderResourceView("ScreenPixels", renderTargetSRVs[RenderTargetType::SCENE_COLORS_NO_AMBIENT].Get());

			refractiveGE->Draw(context, camera);

			material->SetPixelShader(prevPS);
		}
	}

	//now draw particles (commented out)
	targets[0] = backBufferRTV.Get();
	context->OMSetRenderTargets(1, targets, depthBufferDSV.Get());

	context->OMSetBlendState(particleBlendState.Get(), 0, 0xFFFFFFFF);
	context->OMSetDepthStencilState(particleDepthState.Get(), 0);

	for (auto& emitter : emitters) {
		emitter->Draw(camera, totalTime);
	} 

	context->OMSetBlendState(0, 0, 0xFFFFFFFF);
	context->OMSetDepthStencilState(0, 0);


	//Draw ImGui
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());


	// Present the back buffer to the user
	//  - Puts the final frame we're drawing into the window so the user can see it
	//  - Do this exactly ONCE PER FRAME (always at the very end of the frame)
	swapchain->Present(0, 0);

	// Due to the usage of a more sophisticated swap chain,
	// the render target must be re-bound after every call to Present()
	context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), depthBufferDSV.Get());

	ID3D11ShaderResourceView* nullSRVs[16] = {};
	context->PSSetShaderResources(0, 16, nullSRVs);
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Renderer::GetRenderTargetSRV(RenderTargetType type)
{
	if (type < 0 || type >= RenderTargetType::RENDER_TARGET_TYPE_COUNT) return 0;
	return renderTargetSRVs[type];
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Renderer::GetRandomTexture()
{
	return randomTexture;
}

void Renderer::CreateRenderTarget(unsigned int width, unsigned int height, Microsoft::WRL::ComPtr<ID3D11RenderTargetView>& rtv, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv, DXGI_FORMAT colorFormat)
{
	Microsoft::WRL::ComPtr<ID3D11Texture2D> rtTexture;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.ArraySize = 1;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // Need both!
	texDesc.Format = colorFormat;
	texDesc.MipLevels = 1; // Usually no mip chain needed for render targets
	texDesc.MiscFlags = 0;
	texDesc.SampleDesc.Count = 1; // Can't be zero
	device->CreateTexture2D(&texDesc, 0, rtTexture.GetAddressOf());

	// Make the render target view
	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D; // This points to a Texture2D
	rtvDesc.Texture2D.MipSlice = 0;                             // Which mip are we rendering into?
	rtvDesc.Format = texDesc.Format;                // Same format as texture
	device->CreateRenderTargetView(rtTexture.Get(), &rtvDesc, rtv.GetAddressOf());

	// Create the shader resource view using default options 
	device->CreateShaderResourceView(
		rtTexture.Get(),     // Texture resource itself
		0,                   // Null description = default SRV options
		srv.GetAddressOf()); // ComPtr<ID3D11ShaderResourceView>
}


void Renderer::CreateRandomTexture()
{
	const int textureSize = 4;
	const int totalPixels = textureSize * textureSize;
	XMFLOAT4 randomPixels[totalPixels] = {};
	for (int i = 0; i < totalPixels; i++)
	{
		XMVECTOR randomVec = XMVectorSet((float)rand()/RAND_MAX * 2 -1, (float)rand() / RAND_MAX * 2 - 1, 0, 0);
		XMStoreFloat4(&randomPixels[i], XMVector3Normalize(randomVec));
	}

	D3D11_TEXTURE2D_DESC td = {};
	td.ArraySize = 1;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	td.MipLevels = 1;
	td.Height = 4;
	td.Width = 4;
	td.SampleDesc.Count = 1;

	// Initial data for the texture
	D3D11_SUBRESOURCE_DATA data = {};
	data.pSysMem = randomPixels;
	data.SysMemPitch = sizeof(float) * 4 * 4;

	// Actually create it
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	device->CreateTexture2D(&td, &data, texture.GetAddressOf());

	// Create the shader resource view for this texture
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = td.Format;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	device->CreateShaderResourceView(texture.Get(), &srvDesc, randomTexture.GetAddressOf());
}

void Renderer::DrawPointLights(std::shared_ptr<Camera> camera)
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


