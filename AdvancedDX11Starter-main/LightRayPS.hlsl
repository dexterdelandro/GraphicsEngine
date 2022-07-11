cbuffer externalData : register(b0)
{
	int numSamples;
	float2 lightPosScreenSpace;
	float density;
	float weight;
	float decay;
	float exposure;
};

struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};

Texture2D SkyAndOccluders		: register(t0);
Texture2D FinalScene			: register(t1);
SamplerState BasicSampler		: register(s0);

float4 main(VertexToPixel input) : SV_TARGET
{
	// Adjust light pos to UV space
	float2 lightPosUV = lightPosScreenSpace * 0.5f + 0.5f;
	lightPosUV.y = 1.0f - lightPosUV.y;

	// Start with initial color sample from sky/occlusion buffer
	float3 rayColor = SkyAndOccluders.Sample(BasicSampler, input.uv).rgb;

	// Set up the ray for the screen space ray march
	float2 rayPos = input.uv;
	float2 rayDir = rayPos - lightPosUV;
	rayDir *= 1.0f / numSamples * density;

	// Light decays as we get further from light source
	float illumDecay = 1.0f;

	// Loop across screen and accumulate light
	for (int i = 0; i < numSamples; i++)
	{
		// Step and grab new sample and applu attenuation
		rayPos -= rayDir;
		float3 stepColor = SkyAndOccluders.Sample(BasicSampler, rayPos).rgb;
		stepColor *= illumDecay * weight;

		// Accumulate color as we go
		rayColor += stepColor;

		// Exponential decay
		illumDecay *= decay;
	}

	// Combine (using overall exposure for ray color), gamma correct, return
	float3 finalSceneColor = FinalScene.Sample(BasicSampler, input.uv).rgb;
	return float4(pow(rayColor * exposure + finalSceneColor, 1.0f / 2.2f), 1);
}

//float4 main(float2 texCoord : TEXCOORD0) : COLOR0{   
//	// Calculate vector from pixel to light source in screen space.    
//	half2 deltaTexCoord = (texCoord - ScreenLightPos.xy);   // Divide by number of samples and scale by control factor.   
//	deltaTexCoord *= 1.0f / NUM_SAMPLES * Density;   // Store initial sample.    
//	half3 color = tex2D(frameSampler, texCoord);   // Set up illumination decay factor.    
//	half illuminationDecay = 1.0f;   // Evaluate summation from Equation 3 NUM_SAMPLES iterations.    
//	for (int i = 0; i < NUM_SAMPLES; i++)   {     // Step sample location along ray.     
//		texCoord -= deltaTexCoord;     // Retrieve sample at new location.   
//		half3 sample = tex2D(frameSampler, texCoord);     // Apply sample attenuation scale/decay factors.     
//		sample *= illuminationDecay * Weight;     // Accumulate combined color.    
//		color += sample;     // Update exponential decay factor.     
//		illuminationDecay *= Decay;   
//	}   // Output final color with a further scale control factor.    
//	return float4( color * Exposure, 1); } 
// https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-13-volumetric-light-scattering-post-process
