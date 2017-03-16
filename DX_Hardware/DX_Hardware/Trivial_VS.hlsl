struct INPUT_VERTEX
{
	float4 coordinate : POSITION;
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
	OUTPUT_VERTEX sendToRasterizer = (OUTPUT_VERTEX)0;
	
	float4 pos = float4(fromVertexBuffer.coordinate.xyz,1.0f);
	pos = mul(pos, modelpos);
	pos = mul(pos, camView);
	pos = mul(pos, camProj);
	sendToRasterizer.projectedCoordinate = pos;
	sendToRasterizer.colorOut = float4(1.0f,0.0f,0.0f,0.0f);

	//sendToRasterizer.projectedCoordinate.w = 1;
	//sendToRasterizer.projectedCoordinate.xyz = fromVertexBuffer.coordinate.xyz;
	//sendToRasterizer.colorOut = constantColor;

	return sendToRasterizer;
}