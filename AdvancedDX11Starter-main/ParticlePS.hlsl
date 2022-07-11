struct ParticleVertexToPixel {
	float4 position		: SV_POSITION;
	float2 uv			: TEXCOORD0;
	float4 color		: COLOR;
};

Texture2D particleTexture	: register(t0);
SamplerState sampleState	: register(s0);

float4 main(ParticleVertexToPixel input) : SV_TARGET
{
	return particleTexture.Sample(sampleState, input.uv) * input.color;
}