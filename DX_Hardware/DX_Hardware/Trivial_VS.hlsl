struct INPUT_VERTEX
{
	float4 coordinate : POSITION;
};

struct OUTPUT_VERTEX
{
	float4 colorOut : COLOR;
	float4 projectedCoordinate : SV_POSITION;
};

// TODO: PART 3 STEP 2a
cbuffer THIS_IS_VRAM : register( b0 )
{
	matrix camView;
	matrix camProj;
	matrix camPos;
	matrix modelpos;
};

OUTPUT_VERTEX main( INPUT_VERTEX fromVertexBuffer )
{
	OUTPUT_VERTEX sendToRasterizer = (OUTPUT_VERTEX)0;
	sendToRasterizer.projectedCoordinate.w = 1;
	sendToRasterizer.projectedCoordinate.xyz = fromVertexBuffer.coordinate.xyz;
	
	/*float4 pos = float4(0,0,0,1);
	pos.xyz = fromVertexBuffer.coordinate.xyz;
	pos = mul(pos, modelpos);

	pos = mul(pos, camView);
	pos = mul(pos, camProj);
	
	sendToRasterizer.projectedCoordinate = pos;

		
	//sendToRasterizer.projectedCoordinate.xy += constantOffset;
	
	//sendToRasterizer.colorOut = constantColor;*/

	return sendToRasterizer;
}