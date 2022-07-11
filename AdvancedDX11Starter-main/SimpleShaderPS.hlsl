Texture2D pixels		: register(t0);
SamplerState BasicSampler	: register(s0);

struct VertexToPixel
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD0;
};

float4 main(VertexToPixel input) : SV_TARGET
{
	return pixels.Sample(BasicSampler, input.uv);
}