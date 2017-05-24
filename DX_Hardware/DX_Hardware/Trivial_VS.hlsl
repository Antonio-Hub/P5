struct INPUT_VERTEX
{
	float4 coordinate : POSITION;
	float4 normal : NORMAL;
	float4 color : COLOR;
	float2 uv : UV;
	float4 index : INDEX;
	float4 weight : WEIGHT;
};

struct OUTPUT_VERTEX
{
	float4 colorOut : COLOR;
	float4 projectedCoordinate : SV_POSITION;
	float3 l_pos : COLOR1;
	float4 normal : NORMAL;
	float2 uv : UV;
};

cbuffer THIS_IS_VRAM : register( b0 )
{
	matrix camView;
	matrix camProj;
	matrix modelPos;
	float3 spot_light_pos;
	float3 spot_light_dir;
};

cbuffer THIS_IS_ANIMATION_VRAM : register(b1)
{
	matrix inverseBindPose[128];
	matrix realTimePose[128];
};

OUTPUT_VERTEX main( INPUT_VERTEX fromVertexBuffer )
{
	float4 pos = float4(fromVertexBuffer.coordinate.xyz,1.0f);

	pos += mul(pos, realTimePose[(int)fromVertexBuffer.index.x])* fromVertexBuffer.weight.x;
	pos += mul(pos, realTimePose[(int)fromVertexBuffer.index.y])* fromVertexBuffer.weight.y;
	pos += mul(pos, realTimePose[(int)fromVertexBuffer.index.z])* fromVertexBuffer.weight.z;
	pos += mul(pos, realTimePose[(int)fromVertexBuffer.index.w])* fromVertexBuffer.weight.w;

	//pos.w = 1.0f;
	OUTPUT_VERTEX sendToRasterizer = (OUTPUT_VERTEX)0;
	pos = mul(pos, modelPos);
	sendToRasterizer.l_pos = pos.xyz;
	pos = mul(pos, camView);
	pos = mul(pos, camProj);

	float4 nor = float4(normalize(fromVertexBuffer.normal.xyz), 0.0f);
	nor = mul(nor, modelPos);

	sendToRasterizer.colorOut = fromVertexBuffer.color;
	sendToRasterizer.projectedCoordinate = pos;
	sendToRasterizer.normal = nor;
	sendToRasterizer.uv = fromVertexBuffer.uv;

	return sendToRasterizer;
}