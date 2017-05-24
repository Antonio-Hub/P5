Texture2D Texture : register(t0);
SamplerState DefaultFilter : register (s0);
struct OUTPUT_VERTEX
{
	float4 colorOut : COLOR;
	float4 projectedCoordinate : SV_POSITION;
	float3 l_pos : COLOR1;
	float4 normal : NORMAL;
	float2 uv : UV;
};
cbuffer THIS_IS_VRAM : register(b0)
{
	matrix camView;
	matrix camProj;
	matrix modelPos;
	float4 spot_light_pos;
	float4 spot_light_dir;
};
float4 main(OUTPUT_VERTEX input) : SV_TARGET
{
	float4 Texture_Color = Texture.Sample(DefaultFilter, input.uv);
	//light color
	float4 Light_Color = float4(1.0f, 1.0f, 1.0f, 0.0f);
	//float4 Blended_Light_Color = float4(0.0f, 0.0f, 0.0f, 0.0f);

	//hard-coded point light position//
	 //float3 light_pos = float3(0.0f, 10.0f, 0.0f);

	//point light//
	/*float3 Light_Direction = normalize(light_pos - input.l_pos.xyz);
	float Light_Ratio = clamp(dot(Light_Direction, normalize(input.normal.xyz)), 0.0f, 1.0f);
	float p_lightAttenuation = 1.0f - clamp(length(light_pos.xyz - input.l_pos.xyz) / 15.0f, 0.0f, 1.0f);
	Light_Color = p_lightAttenuation * Light_Ratio * Light_Color;
*/
	//spot lighting//
	float3 sld = float3(0.0f, -1.0f, 0.0f);
	float3 slp = float3(1.0f, 10.0f, 1.0f);
	//float3 slp = spot_light_pos.xyz;
	//float3 sld = spot_light_dir.xyz;
	float3 s_light_Direction = normalize(slp - input.l_pos.xyz);
	float s_surfaceRatio = clamp(dot(-s_light_Direction, sld), 0.0f, 1.0f);
	float s_lightRatio = clamp(dot(s_light_Direction, input.normal.xyz), 0.0f, 1.0f);
	float s_lightDistanceAttenuation = 1.0f - clamp(length(spot_light_pos.xyz - input.l_pos.xyz) / 20.0f, 0.0f, 1.0f);
	float s_lightRadiusAttenuation = 1.0f - clamp((0.9f - s_surfaceRatio) / (0.9f - 0.5f), 0.0f, 1.0f);
		

	//float3 s_light_Direction = normalize(spot_light_pos - input.l_pos.xyz);
	//float s_surfaceRatio = clamp(dot(-s_light_Direction, spot_light_dir), 0.0f, 1.0f);
	//float s_lightRatio = clamp(dot(s_light_Direction, input.normal.xyz), 0.0f, 1.0f);
	//float s_lightDistanceAttenuation = 1.0f - clamp(length(spot_light_pos - input.l_pos.xyz) / 20.0f, 0.0f, 1.0f);
	//float s_lightRadiusAttenuation = 1.0f - clamp((0.9f - s_surfaceRatio) / (0.9f - 0.65f), 0.0f, 1.0f);
	//
	Light_Color = s_lightRadiusAttenuation * s_lightDistanceAttenuation * s_lightRatio * Light_Color;
	
	Light_Color = saturate(Light_Color);
	return input.colorOut;//Texture_Color;// *Light_Color;
}