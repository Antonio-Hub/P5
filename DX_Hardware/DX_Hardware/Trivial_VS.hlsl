struct INPUT_VERTEX
{
	float4 coordinate : POSITION;
	float4 color : COLOR;
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
	matrix modelpos;
};

OUTPUT_VERTEX main( INPUT_VERTEX fromVertexBuffer )
{
	float4 pos = float4(fromVertexBuffer.coordinate.xyz,1.0f);
	pos = mul(pos, modelpos);
	pos = mul(pos, camView);
	pos = mul(pos, camProj);

	OUTPUT_VERTEX sendToRasterizer = (OUTPUT_VERTEX)0;
	sendToRasterizer.projectedCoordinate = pos;
	sendToRasterizer.colorOut = fromVertexBuffer.color;

	return sendToRasterizer;
}