struct INPUT_VERTEX
{
	float4 coordinate : POSITION;
	float4 color : COLOR;
	float4 index : INDEX;
	float4 weight : WEIGHT;
};

struct OUTPUT_VERTEX
{
	float4 colorOut : COLOR;
	float4 projectedCoordinate : SV_POSITION;
};

cbuffer THIS_IS_VRAM : register( b0 )
{
	matrix camView;
	matrix camProj;
	matrix modelPos;
};

cbuffer THIS_IS_ANIMATION_VRAM : register(b1)
{
	matrix inverseBindPose[128];
	matrix realTimePose[128];
};

OUTPUT_VERTEX main( INPUT_VERTEX fromVertexBuffer )
{
	float4 pos = float4(fromVertexBuffer.coordinate.xyz,1.0f);

	pos += mul(pos, inverseBindPose[(int)fromVertexBuffer.index.x]) * fromVertexBuffer.weight.x;
	pos += mul(pos, transpose(realTimePose[(int)fromVertexBuffer.index.x]))* fromVertexBuffer.weight.x;

	pos += mul(pos, inverseBindPose[(int)fromVertexBuffer.index.y]) * fromVertexBuffer.weight.y;
	pos += mul(pos, transpose(realTimePose[(int)fromVertexBuffer.index.y]))* fromVertexBuffer.weight.y;

	pos += mul(pos, inverseBindPose[(int)fromVertexBuffer.index.z]) * fromVertexBuffer.weight.z;
	pos += mul(pos, transpose(realTimePose[(int)fromVertexBuffer.index.z]))* fromVertexBuffer.weight.z;

	pos += mul(pos, inverseBindPose[(int)fromVertexBuffer.index.w]) * fromVertexBuffer.weight.w;
	pos += mul(pos, transpose(realTimePose[(int)fromVertexBuffer.index.w]))* fromVertexBuffer.weight.w;

	//pos.w = 1.0f;
	pos = mul(pos, modelPos);
	pos = mul(pos, camView);
	pos = mul(pos, camProj);

	OUTPUT_VERTEX sendToRasterizer = (OUTPUT_VERTEX)0;
	sendToRasterizer.projectedCoordinate = pos;
	sendToRasterizer.colorOut = fromVertexBuffer.color;

	return sendToRasterizer;
}