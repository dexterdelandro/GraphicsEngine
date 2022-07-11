#include "Lighting.hlsli"

// How many lights could we handle?
#define MAX_LIGHTS 128

// Data that only changes once per frame
cbuffer perFrame : register(b0)
{
	// An array of light data
	Light Lights[MAX_LIGHTS];

	// The amount of lights THIS FRAME
	int LightCount;

	// Needed for specular (reflection) calculation
	float3 CameraPosition;

	// The number of mip levels in the specular IBL map
	int SpecIBLTotalMipLevels;

	float2 screenSize;

	float refractionScale;
};

// Defines the input to this pixel shader
// - Should match the output of our corresponding vertex shader
struct VertexToPixel
{
	float4 screenPosition	: SV_POSITION;
	float2 uv				: TEXCOORD;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
	float3 worldPos			: POSITION; // The world position of this PIXEL
};

Texture2D NormalTexture			: register(t0);
Texture2D ScreenPixels			: register(t1);
// Samplers
SamplerState BasicSampler		: register(s0);
SamplerState ClampSampler		: register(s1);

float4 main(VertexToPixel input) : SV_TARGET
{
	float2 screenUV = input.screenPosition.xy / screenSize;
	float2 offsetUV = NormalTexture.Sample(BasicSampler, input.uv).xy * 2 - 1;
	offsetUV.y *= -1;

	float2 refractedUV = screenUV + offsetUV * refractionScale;
	return ScreenPixels.Sample(ClampSampler, refractedUV);
}