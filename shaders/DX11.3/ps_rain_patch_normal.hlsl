#include "common.h"
#include "lmodel.h"
#include "shadow.h"
#include "rain.h"
#ifdef USE_SUNMASK
#include "shadow.h"
#else
float3x4 m_sunmask; // ortho-projection
#endif

Texture3D s_water;
Texture2D s_waterFall;
float4 RainDensity; //	float
float4 WorldX;		//	Float3
float4 WorldZ;		//	Float3



#ifndef ISAMPLE
#define ISAMPLE 0
#endif

#ifdef MSAA_OPTIMIZATION
float4 main(float2 tc: TEXCOORD0, float2 tcJ: TEXCOORD1, float4 Color: COLOR, float4 pos2d: SV_Position, uint iSample: SV_SAMPLEINDEX) : SV_Target
#else
float4 main(float2 tc: TEXCOORD0, float2 tcJ: TEXCOORD1, float4 Color: COLOR, float4 pos2d: SV_Position) : SV_Target
#endif
{
#ifdef MSAA_OPTIMIZATION
	gbuffer_data gbd = gbuffer_load_data(tc, pos2d, iSample);
#else
	gbuffer_data gbd = gbuffer_load_data(tc, pos2d, ISAMPLE);
#endif

	float4 _P = float4(gbd.P, 1.0);
	float3 _N = gbd.N;
	float3 D = gbd.C; // rgb	//.gloss

	_N.xyz = normalize(_N.xyz);

	float4 PS = mul(m_shadow, _P);

	float3 WorldP = mul(m_sunmask, _P);
	float3 WorldN = mul(m_sunmask, _N.xyz);

	// Read rain projection with some jetter. Also adding pixel normal
	// factor to jitter to make rain strips more realistic.
	float s = shadow_rain(PS, WorldP.xz - WorldN.xz);

	//	Apply distance falloff
	//float	fAtten = 1 - smoothstep( 10, 30, length( _P.xyz ));
	// Using fixed fallof factors according to float16 depth coordinate precision.
	float fAtten = 1 - smoothstep(5, 25, _P.z);
	s *= fAtten * fAtten;

	//	Apply rain density
	s *= saturate(RainDensity.x);

	float fIsUp = -dot(Ldynamic_dir.xyz, _N.xyz);
	s *= saturate(fIsUp * 10 + (10 * 0.5) + 0.5);
	fIsUp = max(0, fIsUp);

	float fIsX = WorldN.x;
	float fIsZ = WorldN.z;

	float3 waterSplash = GetNVNMap(s_water, WorldP.xz, timers.x);
	//	float3 waterFallX = GetWaterNMap( s_waterFall, float2(WorldP.x, WorldP.y-timers.x), 0.5 );
	//	float3 waterFallZ = GetWaterNMap( s_waterFall, float2(WorldP.z, WorldP.y-timers.x), 0.5 );

	float3 tc1 = WorldP / 2;

	float fAngleFactor = 1 - fIsUp;

	fAngleFactor = 0.1 * ceil(10 * fAngleFactor);

	//	Just slow down effect.
	//	fAngleFactor *= 0.25;
	fAngleFactor *= 0.35f;
	//	fAngleFactor *= 1.3;

	float3 waterFallX = GetWaterNMap(s_waterFall, float2(tc1.z, tc1.y + timers.x * fAngleFactor));
	float3 waterFallZ = GetWaterNMap(s_waterFall, float2(tc1.x, tc1.y + timers.x * fAngleFactor));

	float2 IsDir = (float2(fIsZ, fIsX));

	IsDir = normalize(IsDir);

	float3 waterFall = GetWaterNMap(s_waterFall, float2(dot(tc1.xz, IsDir), tc1.y + timers.x));

	float distance = length(_P.xyz);
	float WeaponAttenuation = smoothstep(0.8, 0.9, distance);

	if (alt_wet_hud && distance < 1.f)
	{
		waterSplash = 0.0f;
		WeaponAttenuation = 0.35f;
	}

	float ApplyNormalCoeff = s * WeaponAttenuation;
	float3 water = waterSplash * (fIsUp * ApplyNormalCoeff);

	water += waterFallX.yxz * (abs(fIsX) * ApplyNormalCoeff);
	water += waterFallZ.zxy * (abs(fIsZ) * ApplyNormalCoeff);

	//	Translate NM to view space
	water.xyz = mul(m_V, water.xyz);

	//return float4(water.xyz,1); // holger

	_N += water.xyz;

	_N = normalize(_N);

	s *= dot(D.xyz, float3(0.33, 0.33, 0.33));

	return float4(_N, s);
}