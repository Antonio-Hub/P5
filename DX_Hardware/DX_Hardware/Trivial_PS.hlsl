Texture2D Texture : register(t0);

float4 main( float4 colorFromRasterizer : COLOR ) : SV_TARGET
{
	return colorFromRasterizer;
}