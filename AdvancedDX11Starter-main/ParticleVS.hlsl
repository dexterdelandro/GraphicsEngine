cbuffer externalData : register(b0) {
	matrix view;
	matrix projection;
	float3 acceleration;
	float4 startColor;
	float4 endColor;
	float startSize;
	float endSize;
	float lifetime;
	float currentTime;
}

struct Particle {
	float spawnTime;
	float3 startPos;
	float3 startVelocity;
	float startRotation;
	float endRotation;
	float3 padding;


};

struct ParticleVertexToPixel {
	float4 position		: SV_POSITION;
	float2 uv			: TEXCOORD0;
	float4 color		: COLOR;

};

StructuredBuffer<Particle> ParticleData : register(t0);


ParticleVertexToPixel main( uint id : SV_VertexID )
{
	ParticleVertexToPixel output;

	uint particleID = id / 4;
	uint cornerID = id % 4;

	Particle particle = ParticleData.Load(particleID);

	float timeAlive = currentTime - particle.spawnTime;
	float agePercent = timeAlive / lifetime;

	float3 position = acceleration * timeAlive * timeAlive / 2.0f + particle.startVelocity * timeAlive + particle.startPos;
	//float3 position = particle.startPos;
	float4 color = lerp(startColor, endColor, agePercent);
	float size = lerp(startSize, endSize, agePercent);

	//handle smaller tri's
	float2 offset[4];
	offset[0] = float2(-1.0f, 1.0f);
	offset[1] = float2(1.0f, 1.0f);
	offset[2] = float2(1.0f, -1.0f);
	offset[3] = float2(-1.0f, -1.0f);

	float sin, cos, rotation = lerp(particle.startRotation, particle.endRotation, agePercent);
	sincos(rotation, sin, cos);

	float2x2 rotationMat = {
		cos, sin, -sin, cos
	};

	float2 rotatedOffset = mul(offset[cornerID], rotationMat);

	position += float3(view._11, view._12, view._13) * rotatedOffset.x * size;
	position += float3(view._21, view._22, view._23) * rotatedOffset.y * size;

	//calculate output position
	matrix viewProjection = mul(projection, view);
	output.position = mul(viewProjection, float4(position, 1.0f));
	output.uv = saturate(offset[cornerID]);
	output.uv.y = 1 - output.uv.y;
	output.color = color;

	return output;
}