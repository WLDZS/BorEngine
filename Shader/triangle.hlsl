float4 VSMain(uint vertexId : SV_VertexID) : SV_Position
{
    const float2 positions[3] =
    {
        float2( 0.0, -0.6),
        float2( 0.6,  0.6),
        float2(-0.6,  0.6)
    };

    return float4(positions[vertexId], 0.0, 1.0);
}

float4 PSMain() : SV_Target0
{
    return float4(0.2, 0.8, 0.4, 1.0);
}