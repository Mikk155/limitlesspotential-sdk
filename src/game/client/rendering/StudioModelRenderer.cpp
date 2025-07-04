// studio_model.cpp
// routines for setting up to draw 3DStudio models

#include "hud.h"
#include "const.h"
#include "com_model.h"
#include "studio.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "dlight.h"
#include "triangleapi.h"

#include "r_studioint.h"

#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"

#define VectorCopy(a, b) \
    {                    \
        ( b )[0] = ( a )[0]; \
        ( b )[1] = ( a )[1]; \
        ( b )[2] = ( a )[2]; \
    }

void VectorInverse( float* v )
{
    v[0] = -v[0];
    v[1] = -v[1];
    v[2] = -v[2];
}

void VectorScale( const float* in, float scale, float* out )
{
    out[0] = in[0] * scale;
    out[1] = in[1] * scale;
    out[2] = in[2] * scale;
}

// team colors for old TFC models
#define TEAM1_COLOR 150
#define TEAM2_COLOR 250
#define TEAM3_COLOR 45
#define TEAM4_COLOR 100

int m_nPlayerGaitSequences[MAX_PLAYERS];

// Global engine <-> studio model rendering code interface
engine_studio_api_t IEngineStudio;

/////////////////////
// Implementation of CStudioModelRenderer.h

/*
====================
Init

====================
*/
void CStudioModelRenderer::Init()
{
    // Set up some variables shared with engine
    m_pCvarHiModels = IEngineStudio.GetCvar( "cl_himodels" );
    m_pCvarDeveloper = IEngineStudio.GetCvar( "developer" );
    m_pCvarDrawEntities = IEngineStudio.GetCvar( "r_drawentities" );

    m_pChromeSprite = IEngineStudio.GetChromeSprite();

    IEngineStudio.GetModelCounters( &m_pStudioModelCount, &m_pModelsDrawn );

    // Get pointers to engine data structures
    m_pbonetransform = ( float(*)[MAXSTUDIOBONES][3][4] )IEngineStudio.StudioGetBoneTransform();
    m_plighttransform = ( float(*)[MAXSTUDIOBONES][3][4] )IEngineStudio.StudioGetLightTransform();
    m_paliastransform = ( float(*)[3][4] )IEngineStudio.StudioGetAliasTransform();
    m_protationmatrix = ( float(*)[3][4] )IEngineStudio.StudioGetRotationMatrix();
}

/*
====================
CStudioModelRenderer

====================
*/
CStudioModelRenderer::CStudioModelRenderer()
{
    m_fDoInterp = true;
    m_fGaitEstimation = true;
    m_pCurrentEntity = nullptr;
    m_pCvarHiModels = nullptr;
    m_pCvarDeveloper = nullptr;
    m_pCvarDrawEntities = nullptr;
    m_pChromeSprite = nullptr;
    m_pStudioModelCount = nullptr;
    m_pModelsDrawn = nullptr;
    m_protationmatrix = nullptr;
    m_paliastransform = nullptr;
    m_pbonetransform = nullptr;
    m_plighttransform = nullptr;
    m_pStudioHeader = nullptr;
    m_pBodyPart = nullptr;
    m_pSubModel = nullptr;
    m_pPlayerInfo = nullptr;
    m_pRenderModel = nullptr;
}

/*
====================
~CStudioModelRenderer

====================
*/
CStudioModelRenderer::~CStudioModelRenderer()
{
}

/*
====================
StudioCalcBoneAdj

====================
*/
void CStudioModelRenderer::StudioCalcBoneAdj( float dadt, float* adj, const byte* pcontroller1, const byte* pcontroller2, byte mouthopen )
{
    int i, j;
    float value;
    mstudiobonecontroller_t* pbonecontroller;

    pbonecontroller = ( mstudiobonecontroller_t* )( ( byte* )m_pStudioHeader + m_pStudioHeader->bonecontrollerindex );

    for( j = 0; j < m_pStudioHeader->numbonecontrollers; j++ )
    {
        i = pbonecontroller[j].index;
        if( i <= 3 )
        {
            // check for 360% wrapping
            if( ( pbonecontroller[j].type & STUDIO_RLOOP ) != 0 )
            {
                if( abs( pcontroller1[i] - pcontroller2[i] ) > 128 )
                {
                    int a, b;
                    a = ( pcontroller1[j] + 128 ) % 256;
                    b = ( pcontroller2[j] + 128 ) % 256;
                    value = ( ( a * dadt ) + ( b * ( 1 - dadt ) ) - 128 ) * ( 360.0 / 256.0 ) + pbonecontroller[j].start;
                }
                else
                {
                    value = ( ( pcontroller1[i] * dadt + ( pcontroller2[i] ) * ( 1.0 - dadt ) ) ) * ( 360.0 / 256.0 ) + pbonecontroller[j].start;
                }
            }
            else
            {
                value = ( pcontroller1[i] * dadt + pcontroller2[i] * ( 1.0 - dadt ) ) / 255.0;
                if( value < 0 )
                    value = 0;
                if( value > 1.0 )
                    value = 1.0;
                value = ( 1.0 - value ) * pbonecontroller[j].start + value * pbonecontroller[j].end;
            }
            // Con_DPrintf( "%d %d %f : %f\n", m_pCurrentEntity->curstate.controller[j], m_pCurrentEntity->latched.prevcontroller[j], value, dadt );
        }
        else
        {
            value = mouthopen / 64.0;
            if( value > 1.0 )
                value = 1.0;
            value = ( 1.0 - value ) * pbonecontroller[j].start + value * pbonecontroller[j].end;
            // Con_DPrintf("%d %f\n", mouthopen, value );
        }
        switch ( pbonecontroller[j].type & STUDIO_TYPES )
        {
        case STUDIO_XR:
        case STUDIO_YR:
        case STUDIO_ZR:
            adj[j] = value * ( PI / 180.0 );
            break;
        case STUDIO_X:
        case STUDIO_Y:
        case STUDIO_Z:
            adj[j] = value;
            break;
        }
    }
}


/*
====================
StudioCalcBoneQuaterion

====================
*/
void CStudioModelRenderer::StudioCalcBoneQuaterion( int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* q )
{
    int j, k;
    vec4_t q1, q2;
    Vector angle1, angle2;
    mstudioanimvalue_t* panimvalue;

    for( j = 0; j < 3; j++ )
    {
        if( panim->offset[j + 3] == 0 )
        {
            angle2[j] = angle1[j] = pbone->value[j + 3]; // default;
        }
        else
        {
            panimvalue = ( mstudioanimvalue_t* )( ( byte* )panim + panim->offset[j + 3] );
            k = frame;
            // DEBUG
            if( panimvalue->num.total < panimvalue->num.valid )
                k = 0;
            while( panimvalue->num.total <= k )
            {
                k -= panimvalue->num.total;
                panimvalue += panimvalue->num.valid + 1;
                // DEBUG
                if( panimvalue->num.total < panimvalue->num.valid )
                    k = 0;
            }
            // Bah, missing blend!
            if( panimvalue->num.valid > k )
            {
                angle1[j] = panimvalue[k + 1].value;

                if( panimvalue->num.valid > k + 1 )
                {
                    angle2[j] = panimvalue[k + 2].value;
                }
                else
                {
                    if( panimvalue->num.total > k + 1 )
                        angle2[j] = angle1[j];
                    else
                        angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
                }
            }
            else
            {
                angle1[j] = panimvalue[panimvalue->num.valid].value;
                if( panimvalue->num.total > k + 1 )
                {
                    angle2[j] = angle1[j];
                }
                else
                {
                    angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
                }
            }
            angle1[j] = pbone->value[j + 3] + angle1[j] * pbone->scale[j + 3];
            angle2[j] = pbone->value[j + 3] + angle2[j] * pbone->scale[j + 3];
        }

        if( pbone->bonecontroller[j + 3] != -1 )
        {
            angle1[j] += adj[pbone->bonecontroller[j + 3]];
            angle2[j] += adj[pbone->bonecontroller[j + 3]];
        }
    }

    if( angle1 != angle2 )
    {
        AngleQuaternion( angle1, q1 );
        AngleQuaternion( angle2, q2 );
        QuaternionSlerp( q1, q2, s, q );
    }
    else
    {
        AngleQuaternion( angle1, q );
    }
}

/*
====================
StudioCalcBonePosition

====================
*/
void CStudioModelRenderer::StudioCalcBonePosition( int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* pos )
{
    int j, k;
    mstudioanimvalue_t* panimvalue;

    for( j = 0; j < 3; j++ )
    {
        pos[j] = pbone->value[j]; // default;
        if( panim->offset[j] != 0 )
        {
            panimvalue = ( mstudioanimvalue_t* )( ( byte* )panim + panim->offset[j] );
            /*
            if (i == 0 && j == 0)
                Con_DPrintf("%d  %d:%d  %f\n", frame, panimvalue->num.valid, panimvalue->num.total, s );
            */

            k = frame;
            // DEBUG
            if( panimvalue->num.total < panimvalue->num.valid )
                k = 0;
            // find span of values that includes the frame we want
            while( panimvalue->num.total <= k )
            {
                k -= panimvalue->num.total;
                panimvalue += panimvalue->num.valid + 1;
                // DEBUG
                if( panimvalue->num.total < panimvalue->num.valid )
                    k = 0;
            }
            // if we're inside the span
            if( panimvalue->num.valid > k )
            {
                // and there's more data in the span
                if( panimvalue->num.valid > k + 1 )
                {
                    pos[j] += ( panimvalue[k + 1].value * ( 1.0 - s ) + s * panimvalue[k + 2].value ) * pbone->scale[j];
                }
                else
                {
                    pos[j] += panimvalue[k + 1].value * pbone->scale[j];
                }
            }
            else
            {
                // are we at the end of the repeating values section and there's another section with data?
                if( panimvalue->num.total <= k + 1 )
                {
                    pos[j] += ( panimvalue[panimvalue->num.valid].value * ( 1.0 - s ) + s * panimvalue[panimvalue->num.valid + 2].value ) * pbone->scale[j];
                }
                else
                {
                    pos[j] += panimvalue[panimvalue->num.valid].value * pbone->scale[j];
                }
            }
        }
        if( pbone->bonecontroller[j] != -1 && adj )
        {
            pos[j] += adj[pbone->bonecontroller[j]];
        }
    }
}

/*
====================
StudioSlerpBones

====================
*/
void CStudioModelRenderer::StudioSlerpBones( vec4_t q1[], float pos1[][3], vec4_t q2[], float pos2[][3], float s )
{
    int i;
    vec4_t q3;
    float s1;

    if( s < 0 )
        s = 0;
    else if( s > 1.0 )
        s = 1.0;

    s1 = 1.0 - s;

    for( i = 0; i < m_pStudioHeader->numbones; i++ )
    {
        QuaternionSlerp( q1[i], q2[i], s, q3 );
        q1[i][0] = q3[0];
        q1[i][1] = q3[1];
        q1[i][2] = q3[2];
        q1[i][3] = q3[3];
        pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * s;
        pos1[i][1] = pos1[i][1] * s1 + pos2[i][1] * s;
        pos1[i][2] = pos1[i][2] * s1 + pos2[i][2] * s;
    }
}

/*
====================
StudioGetAnim

====================
*/
mstudioanim_t* CStudioModelRenderer::StudioGetAnim( model_t* subModel, mstudioseqdesc_t* pseqdesc )
{
    mstudioseqgroup_t* pseqgroup;
    cache_user_t* paSequences;

    pseqgroup = ( mstudioseqgroup_t* )( ( byte* )m_pStudioHeader + m_pStudioHeader->seqgroupindex ) + pseqdesc->seqgroup;

    if( pseqdesc->seqgroup == 0 )
    {
        return ( mstudioanim_t* )( ( byte* )m_pStudioHeader + pseqdesc->animindex );
    }

    paSequences = ( cache_user_t* )subModel->submodels;

    if( paSequences == nullptr )
    {
        paSequences = ( cache_user_t* )IEngineStudio.Mem_Calloc( 16, sizeof( cache_user_t ) ); // UNDONE: leak!
        subModel->submodels = ( dmodel_t* )paSequences;
    }

    if( !IEngineStudio.Cache_Check( ( cache_user_t* )&( paSequences[pseqdesc->seqgroup] ) ) )
    {
        gEngfuncs.Con_DPrintf( "loading %s\n", pseqgroup->name );
        IEngineStudio.LoadCacheFile( pseqgroup->name, ( cache_user_t* )&paSequences[pseqdesc->seqgroup] );
    }
    return ( mstudioanim_t* )( ( byte* )paSequences[pseqdesc->seqgroup].data + pseqdesc->animindex );
}

/*
====================
StudioPlayerBlend

====================
*/
void CStudioModelRenderer::StudioPlayerBlend( mstudioseqdesc_t* pseqdesc, int* pBlend, float* pPitch )
{
    // calc up/down pointing
    *pBlend = ( *pPitch * 3 );
    if( *pBlend < pseqdesc->blendstart[0] )
    {
        *pPitch -= pseqdesc->blendstart[0] / 3.0;
        *pBlend = 0;
    }
    else if( *pBlend > pseqdesc->blendend[0] )
    {
        *pPitch -= pseqdesc->blendend[0] / 3.0;
        *pBlend = 255;
    }
    else
    {
        if( pseqdesc->blendend[0] - pseqdesc->blendstart[0] < 0.1 ) // catch qc error
            *pBlend = 127;
        else
            *pBlend = 255 * ( *pBlend - pseqdesc->blendstart[0] ) / ( pseqdesc->blendend[0] - pseqdesc->blendstart[0] );
        *pPitch = 0;
    }
}

/*
====================
StudioSetUpTransform

====================
*/
void CStudioModelRenderer::StudioSetUpTransform( bool trivial_accept )
{
    int i;
    Vector angles;
    Vector modelpos;

    // tweek model origin
    // for (i = 0; i < 3; i++)
    //    modelpos[i] = m_pCurrentEntity->origin[i];

    modelpos = m_pCurrentEntity->origin;

    // TODO: should really be stored with the entity instead of being reconstructed
    // TODO: should use a look-up table
    // TODO: could cache lazily, stored in the entity
    angles[ROLL] = m_pCurrentEntity->curstate.angles[ROLL];
    angles[PITCH] = m_pCurrentEntity->curstate.angles[PITCH];
    angles[YAW] = m_pCurrentEntity->curstate.angles[YAW];

    // Con_DPrintf("Angles %4.2f prev %4.2f for %i\n", angles[PITCH], m_pCurrentEntity->index);
    // Con_DPrintf("movetype %d %d\n", m_pCurrentEntity->movetype, m_pCurrentEntity->aiment );
    if( m_pCurrentEntity->curstate.movetype == MOVETYPE_STEP )
    {
        float f = 0;
        float d;

        // don't do it if the goalstarttime hasn't updated in a while.

        // NOTE:  Because we need to interpolate multiplayer characters, the interpolation time limit
        //  was increased to 1.0 s., which is 2x the max lag we are accounting for.

        if( ( m_clTime < m_pCurrentEntity->curstate.animtime + 1.0f ) &&
            ( m_pCurrentEntity->curstate.animtime != m_pCurrentEntity->latched.prevanimtime ) )
        {
            f = ( m_clTime - m_pCurrentEntity->curstate.animtime ) / ( m_pCurrentEntity->curstate.animtime - m_pCurrentEntity->latched.prevanimtime );
            // Con_DPrintf("%4.2f %.2f %.2f\n", f, m_pCurrentEntity->curstate.animtime, m_clTime);
        }

        if( m_fDoInterp )
        {
            // ugly hack to interpolate angle, position. current is reached 0.1 seconds after being set
            f = f - 1.0;
        }
        else
        {
            f = 0;
        }

        const auto pseqdesc = ( mstudioseqdesc_t* )( ( byte* )m_pStudioHeader + m_pStudioHeader->seqindex ) + m_pCurrentEntity->curstate.sequence;

        if( ( pseqdesc->motiontype & STUDIO_LX ) != 0 || ( m_pCurrentEntity->curstate.eflags & EFLAG_SLERP ) != 0 )
        {
            for( i = 0; i < 3; i++ )
            {
                modelpos[i] += ( m_pCurrentEntity->origin[i] - m_pCurrentEntity->latched.prevorigin[i] ) * f;
            }
        }

        // NOTE:  Because multiplayer lag can be relatively large, we don't want to cap
        //  f at 1.5 anymore.
        // if (f > -1.0 && f < 1.5) {}

        //            Con_DPrintf("%.0f %.0f\n",m_pCurrentEntity->msg_angles[0][YAW], m_pCurrentEntity->msg_angles[1][YAW] );
        for( i = 0; i < 3; i++ )
        {
            float ang1, ang2;

            ang1 = m_pCurrentEntity->angles[i];
            ang2 = m_pCurrentEntity->latched.prevangles[i];

            d = ang1 - ang2;
            if( d > 180 )
            {
                d -= 360;
            }
            else if( d < -180 )
            {
                d += 360;
            }

            angles[i] += d * f;
        }
        // Con_DPrintf("%.3f \n", f );
    }
    else if( m_pCurrentEntity->curstate.movetype != MOVETYPE_NONE )
    {
        angles = m_pCurrentEntity->angles;
    }

    // Con_DPrintf("%.0f %0.f %0.f\n", modelpos[0], modelpos[1], modelpos[2] );
    // Con_DPrintf("%.0f %0.f %0.f\n", angles[0], angles[1], angles[2] );

    angles[PITCH] = -angles[PITCH];
    AngleMatrix( angles, ( *m_protationmatrix ) );

    if( 0 == IEngineStudio.IsHardware() )
    {
        static float viewmatrix[3][4];

        VectorCopy( m_vRight, viewmatrix[0] );
        VectorCopy( m_vUp, viewmatrix[1] );
        VectorInverse( viewmatrix[1] );
        VectorCopy( m_vNormal, viewmatrix[2] );

        ( *m_protationmatrix )[0][3] = modelpos[0] - m_vRenderOrigin[0];
        ( *m_protationmatrix )[1][3] = modelpos[1] - m_vRenderOrigin[1];
        ( *m_protationmatrix )[2][3] = modelpos[2] - m_vRenderOrigin[2];

        ConcatTransforms( viewmatrix, ( *m_protationmatrix ), ( *m_paliastransform ) );

        // do the scaling up of x and y to screen coordinates as part of the transform
        // for the unclipped case (it would mess up clipping in the clipped case).
        // Also scale down z, so 1/z is scaled 31 bits for free, and scale down x and y
        // correspondingly so the projected x and y come out right
        // FIXME: make this work for clipped case too?
        if( trivial_accept )
        {
            for( i = 0; i < 4; i++ )
            {
                ( *m_paliastransform )[0][i] *= m_fSoftwareXScale *
                                              ( 1.0 / ( ZISCALE * 0x10000 ) );
                ( *m_paliastransform )[1][i] *= m_fSoftwareYScale *
                                              ( 1.0 / ( ZISCALE * 0x10000 ) );
                ( *m_paliastransform )[2][i] *= 1.0 / ( ZISCALE * 0x10000 );
            }
        }
    }

    ( *m_protationmatrix )[0][3] = modelpos[0];
    ( *m_protationmatrix )[1][3] = modelpos[1];
    ( *m_protationmatrix )[2][3] = modelpos[2];
}


/*
====================
StudioEstimateInterpolant

====================
*/
float CStudioModelRenderer::StudioEstimateInterpolant()
{
    float dadt = 1.0;

    if( m_fDoInterp && ( m_pCurrentEntity->curstate.animtime >= m_pCurrentEntity->latched.prevanimtime + 0.01 ) )
    {
        dadt = ( m_clTime - m_pCurrentEntity->curstate.animtime ) / 0.1;
        if( dadt > 2.0 )
        {
            dadt = 2.0;
        }
    }
    return dadt;
}

/*
====================
StudioCalcRotations

====================
*/
void CStudioModelRenderer::StudioCalcRotations( float pos[][3], vec4_t* q, mstudioseqdesc_t* pseqdesc, mstudioanim_t* panim, float f )
{
    int i;
    int frame;
    mstudiobone_t* pbone;

    float s;
    float adj[MAXSTUDIOCONTROLLERS];
    float dadt;

    if( f > pseqdesc->numframes - 1 )
    {
        f = 0; // bah, fix this bug with changing sequences too fast
    }
    // BUG ( somewhere else ) but this code should validate this data.
    // This could cause a crash if the frame # is negative, so we'll go ahead
    //  and clamp it here
    else if( f < -0.01 )
    {
        f = -0.01;
    }

    frame = (int)f;

    // Con_DPrintf("%d %.4f %.4f %.4f %.4f %d\n", m_pCurrentEntity->curstate.sequence, m_clTime, m_pCurrentEntity->animtime, m_pCurrentEntity->frame, f, frame );

    // Con_DPrintf( "%f %f %f\n", m_pCurrentEntity->angles[ROLL], m_pCurrentEntity->angles[PITCH], m_pCurrentEntity->angles[YAW] );

    // Con_DPrintf("frame %d %d\n", frame1, frame2 );


    dadt = StudioEstimateInterpolant();
    s = ( f - frame );

    // add in programtic controllers
    pbone = ( mstudiobone_t* )( ( byte* )m_pStudioHeader + m_pStudioHeader->boneindex );

    StudioCalcBoneAdj( dadt, adj, m_pCurrentEntity->curstate.controller, m_pCurrentEntity->latched.prevcontroller, m_pCurrentEntity->mouth.mouthopen );

    for( i = 0; i < m_pStudioHeader->numbones; i++, pbone++, panim++ )
    {
        StudioCalcBoneQuaterion( frame, s, pbone, panim, adj, q[i] );

        StudioCalcBonePosition( frame, s, pbone, panim, adj, pos[i] );
        // if (0 && i == 0)
        //    Con_DPrintf("%d %d %d %d\n", m_pCurrentEntity->curstate.sequence, frame, j, k );
    }

    if( ( pseqdesc->motiontype & STUDIO_X ) != 0 )
    {
        pos[pseqdesc->motionbone][0] = 0.0;
    }
    if( ( pseqdesc->motiontype & STUDIO_Y ) != 0 )
    {
        pos[pseqdesc->motionbone][1] = 0.0;
    }
    if( ( pseqdesc->motiontype & STUDIO_Z ) != 0 )
    {
        pos[pseqdesc->motionbone][2] = 0.0;
    }

    s = 0 * ( ( 1.0 - ( f - (int)( f ) ) ) / ( pseqdesc->numframes ) ) * m_pCurrentEntity->curstate.framerate;

    if( ( pseqdesc->motiontype & STUDIO_LX ) != 0 )
    {
        pos[pseqdesc->motionbone][0] += s * pseqdesc->linearmovement[0];
    }
    if( ( pseqdesc->motiontype & STUDIO_LY ) != 0 )
    {
        pos[pseqdesc->motionbone][1] += s * pseqdesc->linearmovement[1];
    }
    if( ( pseqdesc->motiontype & STUDIO_LZ ) != 0 )
    {
        pos[pseqdesc->motionbone][2] += s * pseqdesc->linearmovement[2];
    }
}

/*
====================
Studio_FxTransform

====================
*/
void CStudioModelRenderer::StudioFxTransform( cl_entity_t* ent, float transform[3][4] )
{
    if( ent->curstate.renderfx != kRenderFxExplode && ent->curstate.scale > 0 )
    {
        transform[0][0] *= ent->curstate.scale;
        transform[1][0] *= ent->curstate.scale;
        transform[2][0] *= ent->curstate.scale;

        transform[0][1] *= ent->curstate.scale;
        transform[1][1] *= ent->curstate.scale;
        transform[2][1] *= ent->curstate.scale;

        transform[0][2] *= ent->curstate.scale;
        transform[1][2] *= ent->curstate.scale;
        transform[2][2] *= ent->curstate.scale;
    }

    switch ( ent->curstate.renderfx )
    {
    case kRenderFxDistort:
    case kRenderFxHologram:
        if( gEngfuncs.pfnRandomLong( 0, 49 ) == 0 )
        {
            int axis = gEngfuncs.pfnRandomLong( 0, 1 );
            if( axis == 1 ) // Choose between x & z
                axis = 2;
            VectorScale( transform[axis], gEngfuncs.pfnRandomFloat( 1, 1.484 ), transform[axis] );
        }
        else if( gEngfuncs.pfnRandomLong( 0, 49 ) == 0 )
        {
            float offset;
            int axis = gEngfuncs.pfnRandomLong( 0, 1 );
            if( axis == 1 ) // Choose between x & z
                axis = 2;
            offset = gEngfuncs.pfnRandomFloat( -10, 10 );
            transform[gEngfuncs.pfnRandomLong( 0, 2 )][3] += offset;
        }
        break;
    case kRenderFxExplode:
    {
        float scale;

        scale = 1.0 + ( m_clTime - ent->curstate.animtime ) * 10.0;
        if( scale > 2 ) // Don't blow up more than 200%
            scale = 2;
        transform[0][1] *= scale;
        transform[1][1] *= scale;
        transform[2][1] *= scale;
    }
    break;
    }
}

/*
====================
StudioEstimateFrame

====================
*/
float CStudioModelRenderer::StudioEstimateFrame( mstudioseqdesc_t* pseqdesc )
{
    double dfdt, f;

    if( m_fDoInterp )
    {
        if( m_clTime < m_pCurrentEntity->curstate.animtime )
        {
            dfdt = 0;
        }
        else
        {
            dfdt = ( m_clTime - m_pCurrentEntity->curstate.animtime ) * m_pCurrentEntity->curstate.framerate * pseqdesc->fps;
        }
    }
    else
    {
        dfdt = 0;
    }

    if( pseqdesc->numframes <= 1 )
    {
        f = 0;
    }
    else
    {
        f = ( m_pCurrentEntity->curstate.frame * ( pseqdesc->numframes - 1 ) ) / 256.0;
    }

    f += dfdt;

    if( ( pseqdesc->flags & STUDIO_LOOPING ) != 0 )
    {
        if( pseqdesc->numframes > 1 )
        {
            f -= (int)( f / ( pseqdesc->numframes - 1 ) ) * ( pseqdesc->numframes - 1 );
        }
        if( f < 0 )
        {
            f += ( pseqdesc->numframes - 1 );
        }
    }
    else
    {
        if( f >= pseqdesc->numframes - 1.001 )
        {
            f = pseqdesc->numframes - 1.001;
        }
        if( f < 0.0 )
        {
            f = 0.0;
        }
    }
    return f;
}

/*
====================
StudioSetupBones

====================
*/
void CStudioModelRenderer::StudioSetupBones()
{
    int i;
    double f;

    mstudiobone_t* pbones;
    mstudioseqdesc_t* pseqdesc;
    mstudioanim_t* panim;

    static float pos[MAXSTUDIOBONES][3];
    static vec4_t q[MAXSTUDIOBONES];
    float bonematrix[3][4];

    static float pos2[MAXSTUDIOBONES][3];
    static vec4_t q2[MAXSTUDIOBONES];
    static float pos3[MAXSTUDIOBONES][3];
    static vec4_t q3[MAXSTUDIOBONES];
    static float pos4[MAXSTUDIOBONES][3];
    static vec4_t q4[MAXSTUDIOBONES];

    if( m_pCurrentEntity->curstate.sequence >= m_pStudioHeader->numseq )
    {
        m_pCurrentEntity->curstate.sequence = 0;
    }

    pseqdesc = ( mstudioseqdesc_t* )( ( byte* )m_pStudioHeader + m_pStudioHeader->seqindex ) + m_pCurrentEntity->curstate.sequence;

    // always want new gait sequences to start on frame zero
    /*    if ( m_pPlayerInfo )
    {
        int playerNum = m_pCurrentEntity->index - 1;

        // new jump gaitsequence?  start from frame zero
        if ( m_nPlayerGaitSequences[ playerNum ] != m_pPlayerInfo->gaitsequence )
        {
    //        m_pPlayerInfo->gaitframe = 0.0;
            gEngfuncs.Con_Printf( "Setting gaitframe to 0\n" );
        }

        m_nPlayerGaitSequences[ playerNum ] = m_pPlayerInfo->gaitsequence;
//        gEngfuncs.Con_Printf( "index: %d     gaitsequence: %d\n",playerNum, m_pPlayerInfo->gaitsequence);
    }
*/
    f = StudioEstimateFrame( pseqdesc );

    if( m_pCurrentEntity->latched.prevframe > f )
    {
        // Con_DPrintf("%f %f\n", m_pCurrentEntity->prevframe, f );
    }

    panim = StudioGetAnim( m_pRenderModel, pseqdesc );
    StudioCalcRotations( pos, q, pseqdesc, panim, f );

    if( pseqdesc->numblends > 1 )
    {
        float s;
        float dadt;

        panim += m_pStudioHeader->numbones;
        StudioCalcRotations( pos2, q2, pseqdesc, panim, f );

        dadt = StudioEstimateInterpolant();
        s = ( m_pCurrentEntity->curstate.blending[0] * dadt + m_pCurrentEntity->latched.prevblending[0] * ( 1.0 - dadt ) ) / 255.0;

        StudioSlerpBones( q, pos, q2, pos2, s );

        if( pseqdesc->numblends == 4 )
        {
            panim += m_pStudioHeader->numbones;
            StudioCalcRotations( pos3, q3, pseqdesc, panim, f );

            panim += m_pStudioHeader->numbones;
            StudioCalcRotations( pos4, q4, pseqdesc, panim, f );

            s = ( m_pCurrentEntity->curstate.blending[0] * dadt + m_pCurrentEntity->latched.prevblending[0] * ( 1.0 - dadt ) ) / 255.0;
            StudioSlerpBones( q3, pos3, q4, pos4, s );

            s = ( m_pCurrentEntity->curstate.blending[1] * dadt + m_pCurrentEntity->latched.prevblending[1] * ( 1.0 - dadt ) ) / 255.0;
            StudioSlerpBones( q, pos, q3, pos3, s );
        }
    }

    if( m_fDoInterp &&
        0 != m_pCurrentEntity->latched.sequencetime &&
        ( m_pCurrentEntity->latched.sequencetime + 0.2 > m_clTime ) &&
        ( m_pCurrentEntity->latched.prevsequence < m_pStudioHeader->numseq ) )
    {
        // blend from last sequence
        static float pos1b[MAXSTUDIOBONES][3];
        static vec4_t q1b[MAXSTUDIOBONES];
        float s;

        if( m_pCurrentEntity->latched.prevsequence >= m_pStudioHeader->numseq )
        {
            m_pCurrentEntity->latched.prevsequence = 0;
        }

        pseqdesc = ( mstudioseqdesc_t* )( ( byte* )m_pStudioHeader + m_pStudioHeader->seqindex ) + m_pCurrentEntity->latched.prevsequence;
        panim = StudioGetAnim( m_pRenderModel, pseqdesc );
        // clip prevframe
        StudioCalcRotations( pos1b, q1b, pseqdesc, panim, m_pCurrentEntity->latched.prevframe );

        if( pseqdesc->numblends > 1 )
        {
            panim += m_pStudioHeader->numbones;
            StudioCalcRotations( pos2, q2, pseqdesc, panim, m_pCurrentEntity->latched.prevframe );

            s = ( m_pCurrentEntity->latched.prevseqblending[0] ) / 255.0;
            StudioSlerpBones( q1b, pos1b, q2, pos2, s );

            if( pseqdesc->numblends == 4 )
            {
                panim += m_pStudioHeader->numbones;
                StudioCalcRotations( pos3, q3, pseqdesc, panim, m_pCurrentEntity->latched.prevframe );

                panim += m_pStudioHeader->numbones;
                StudioCalcRotations( pos4, q4, pseqdesc, panim, m_pCurrentEntity->latched.prevframe );

                s = ( m_pCurrentEntity->latched.prevseqblending[0] ) / 255.0;
                StudioSlerpBones( q3, pos3, q4, pos4, s );

                s = ( m_pCurrentEntity->latched.prevseqblending[1] ) / 255.0;
                StudioSlerpBones( q1b, pos1b, q3, pos3, s );
            }
        }

        s = 1.0 - ( m_clTime - m_pCurrentEntity->latched.sequencetime ) / 0.2;
        StudioSlerpBones( q, pos, q1b, pos1b, s );
    }
    else
    {
        // Con_DPrintf("prevframe = %4.2f\n", f);
        m_pCurrentEntity->latched.prevframe = f;
    }

    pbones = ( mstudiobone_t* )( ( byte* )m_pStudioHeader + m_pStudioHeader->boneindex );

    // bounds checking
    if( m_pPlayerInfo )
    {
        if( m_pPlayerInfo->gaitsequence >= m_pStudioHeader->numseq )
        {
            m_pPlayerInfo->gaitsequence = 0;
        }
    }

    // calc gait animation
    if( m_pPlayerInfo && m_pPlayerInfo->gaitsequence != 0 )
    {
        if( m_pPlayerInfo->gaitsequence >= m_pStudioHeader->numseq )
        {
            m_pPlayerInfo->gaitsequence = 0;
        }

        bool copy = true;

        pseqdesc = ( mstudioseqdesc_t* )( ( byte* )m_pStudioHeader + m_pStudioHeader->seqindex ) + m_pPlayerInfo->gaitsequence;

        panim = StudioGetAnim( m_pRenderModel, pseqdesc );
        StudioCalcRotations( pos2, q2, pseqdesc, panim, m_pPlayerInfo->gaitframe );

        for( i = 0; i < m_pStudioHeader->numbones; i++ )
        {
            auto bone = &pbones[i];

            if( 0 == strcmp( bone->name, "Bip01 Spine" ) )
            {
                copy = false;
            }
            else if( bone->parent >= 0 &&
                     bone->parent < m_pStudioHeader->numbones &&
                     0 == strcmp( pbones[bone->parent].name, "Bip01 Pelvis" ) )
            {
                copy = true;
            }

            if( copy )
            {
                memcpy( pos[i], pos2[i], sizeof( pos[i] ) );
                memcpy( q[i], q2[i], sizeof( q[i] ) );
            }
        }
    }

    for( i = 0; i < m_pStudioHeader->numbones; i++ )
    {
        const int parent = pbones[i].parent;

        QuaternionMatrix( q[i], bonematrix );

        bonematrix[0][3] = pos[i][0];
        bonematrix[1][3] = pos[i][1];
        bonematrix[2][3] = pos[i][2];

        if( parent == -1 )
        {
            if( 0 != IEngineStudio.IsHardware() )
            {
                ConcatTransforms( ( *m_protationmatrix ), bonematrix, ( *m_pbonetransform )[i] );

                // MatrixCopy should be faster...
                // ConcatTransforms ((*m_protationmatrix), bonematrix, (*m_plighttransform)[i]);
                MatrixCopy( ( *m_pbonetransform )[i], ( *m_plighttransform )[i] );
            }
            else
            {
                ConcatTransforms( ( *m_paliastransform ), bonematrix, ( *m_pbonetransform )[i] );
                ConcatTransforms( ( *m_protationmatrix ), bonematrix, ( *m_plighttransform )[i] );
            }

            // Apply client-side effects to the transformation matrix
            StudioFxTransform( m_pCurrentEntity, ( *m_pbonetransform )[i] );
        }
        else if( parent >= 0 && parent < m_pStudioHeader->numbones )
        {
            ConcatTransforms( ( *m_pbonetransform )[parent], bonematrix, ( *m_pbonetransform )[i] );
            ConcatTransforms( ( *m_plighttransform )[parent], bonematrix, ( *m_plighttransform )[i] );
        }
    }
}


/*
====================
StudioSaveBones

====================
*/
void CStudioModelRenderer::StudioSaveBones()
{
    int i;

    mstudiobone_t* pbones;
    pbones = ( mstudiobone_t* )( ( byte* )m_pStudioHeader + m_pStudioHeader->boneindex );

    m_nCachedBones = m_pStudioHeader->numbones;

    for( i = 0; i < m_pStudioHeader->numbones; i++ )
    {
        strcpy( m_nCachedBoneNames[i], pbones[i].name );
        MatrixCopy( ( *m_pbonetransform )[i], m_rgCachedBoneTransform[i] );
        MatrixCopy( ( *m_plighttransform )[i], m_rgCachedLightTransform[i] );
    }
}


/*
====================
StudioMergeBones

====================
*/
void CStudioModelRenderer::StudioMergeBones( model_t* subModel )
{
    int i, j;
    double f;

    mstudiobone_t* pbones;
    mstudioseqdesc_t* pseqdesc;
    mstudioanim_t* panim;

    static float pos[MAXSTUDIOBONES][3];
    float bonematrix[3][4];
    static vec4_t q[MAXSTUDIOBONES];

    if( m_pCurrentEntity->curstate.sequence >= m_pStudioHeader->numseq )
    {
        m_pCurrentEntity->curstate.sequence = 0;
    }

    pseqdesc = ( mstudioseqdesc_t* )( ( byte* )m_pStudioHeader + m_pStudioHeader->seqindex ) + m_pCurrentEntity->curstate.sequence;

    f = StudioEstimateFrame( pseqdesc );

    if( m_pCurrentEntity->latched.prevframe > f )
    {
        // Con_DPrintf("%f %f\n", m_pCurrentEntity->prevframe, f );
    }

    panim = StudioGetAnim( subModel, pseqdesc );
    StudioCalcRotations( pos, q, pseqdesc, panim, f );

    pbones = ( mstudiobone_t* )( ( byte* )m_pStudioHeader + m_pStudioHeader->boneindex );


    for( i = 0; i < m_pStudioHeader->numbones; i++ )
    {
        for( j = 0; j < m_nCachedBones; j++ )
        {
            if( stricmp( pbones[i].name, m_nCachedBoneNames[j] ) == 0 )
            {
                MatrixCopy( m_rgCachedBoneTransform[j], ( *m_pbonetransform )[i] );
                MatrixCopy( m_rgCachedLightTransform[j], ( *m_plighttransform )[i] );
                break;
            }
        }
        if( j >= m_nCachedBones )
        {
            QuaternionMatrix( q[i], bonematrix );

            bonematrix[0][3] = pos[i][0];
            bonematrix[1][3] = pos[i][1];
            bonematrix[2][3] = pos[i][2];

            if( pbones[i].parent == -1 )
            {
                if( 0 != IEngineStudio.IsHardware() )
                {
                    ConcatTransforms( ( *m_protationmatrix ), bonematrix, ( *m_pbonetransform )[i] );

                    // MatrixCopy should be faster...
                    // ConcatTransforms ((*m_protationmatrix), bonematrix, (*m_plighttransform)[i]);
                    MatrixCopy( ( *m_pbonetransform )[i], ( *m_plighttransform )[i] );
                }
                else
                {
                    ConcatTransforms( ( *m_paliastransform ), bonematrix, ( *m_pbonetransform )[i] );
                    ConcatTransforms( ( *m_protationmatrix ), bonematrix, ( *m_plighttransform )[i] );
                }

                // Apply client-side effects to the transformation matrix
                StudioFxTransform( m_pCurrentEntity, ( *m_pbonetransform )[i] );
            }
            else
            {
                ConcatTransforms( ( *m_pbonetransform )[pbones[i].parent], bonematrix, ( *m_pbonetransform )[i] );
                ConcatTransforms( ( *m_plighttransform )[pbones[i].parent], bonematrix, ( *m_plighttransform )[i] );
            }
        }
    }
}


/*
====================
StudioDrawModel

====================
*/
bool CStudioModelRenderer::StudioDrawModel( int flags )
{
    alight_t lighting;
    Vector dir;

    m_pCurrentEntity = IEngineStudio.GetCurrentEntity();
    IEngineStudio.GetTimes( &m_nFrameCount, &m_clTime, &m_clOldTime );
    IEngineStudio.GetViewInfo( m_vRenderOrigin, m_vUp, m_vRight, m_vNormal );
    IEngineStudio.GetAliasScale( &m_fSoftwareXScale, &m_fSoftwareYScale );

    if( m_pCurrentEntity->curstate.renderfx == kRenderFxDeadPlayer )
    {
        entity_state_t deadplayer;

        bool result;
        bool save_interp;

        if( m_pCurrentEntity->curstate.renderamt <= 0 || m_pCurrentEntity->curstate.renderamt > gEngfuncs.GetMaxClients() )
            return false;

        // get copy of player
        deadplayer = *( IEngineStudio.GetPlayerState( m_pCurrentEntity->curstate.renderamt - 1 ) ); // cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[m_pCurrentEntity->curstate.renderamt-1];

        // clear weapon, movement state
        deadplayer.number = m_pCurrentEntity->curstate.renderamt;
        deadplayer.weaponmodel = 0;
        deadplayer.gaitsequence = 0;

        deadplayer.movetype = MOVETYPE_NONE;
        deadplayer.angles = m_pCurrentEntity->curstate.angles;
        deadplayer.origin = m_pCurrentEntity->curstate.origin;

        save_interp = m_fDoInterp;
        m_fDoInterp = false;

        // draw as though it were a player
        result = StudioDrawPlayer( flags, &deadplayer );

        m_fDoInterp = save_interp;
        return result;
    }

    m_pRenderModel = m_pCurrentEntity->model;
    m_pStudioHeader = ( studiohdr_t* )IEngineStudio.Mod_Extradata( m_pRenderModel );
    IEngineStudio.StudioSetHeader( m_pStudioHeader );
    IEngineStudio.SetRenderModel( m_pRenderModel );

    StudioSetUpTransform( false );

    if( ( flags & STUDIO_RENDER ) != 0 )
    {
        // see if the bounding box lets us trivially reject, also sets
        if( 0 == IEngineStudio.StudioCheckBBox() )
            return false;

        ( *m_pModelsDrawn )++;
        ( *m_pStudioModelCount )++; // render data cache cookie

        if( m_pStudioHeader->numbodyparts == 0 )
            return true;
    }

    if( m_pCurrentEntity->curstate.movetype == MOVETYPE_FOLLOW )
    {
        StudioMergeBones( m_pRenderModel );
    }
    else
    {
        StudioSetupBones();
    }
    StudioSaveBones();

    if( ( flags & STUDIO_EVENTS ) != 0 )
    {
        StudioCalcAttachments();
        IEngineStudio.StudioClientEvents();
        // copy attachments into global entity array
        if( m_pCurrentEntity->index > 0 )
        {
            cl_entity_t* ent = gEngfuncs.GetEntityByIndex( m_pCurrentEntity->index );

            memcpy( ent->attachment, m_pCurrentEntity->attachment, sizeof( Vector ) * 4 );
        }
    }

    if( ( flags & STUDIO_RENDER ) != 0 )
    {
        lighting.plightvec = dir;
        IEngineStudio.StudioDynamicLight( m_pCurrentEntity, &lighting );

        IEngineStudio.StudioEntityLight( &lighting );

        // model and frame independant
        IEngineStudio.StudioSetupLighting( &lighting );

        // get remap colors

        m_nTopColor = m_pCurrentEntity->curstate.colormap & 0xFF;
        m_nBottomColor = ( m_pCurrentEntity->curstate.colormap & 0xFF00 ) >> 8;

        IEngineStudio.StudioSetRemapColors( m_nTopColor, m_nBottomColor );

        StudioRenderModel();

        if( m_pCurrentEntity->curstate.renderfx == kRenderFxDLightColor
        && m_pCurrentEntity->curstate.renderamt > 0 )
        {
            if( dlight_t* light = gHUD.m_Dlight.AllocDLight( StudioModelRendering ); light != nullptr )
            {
                light->origin = m_pCurrentEntity->curstate.origin;
                light->radius = m_pCurrentEntity->curstate.renderamt;
                light->color = m_pCurrentEntity->curstate.rendercolor;
                light->die += 0.1f;
            }
        }
    }

    return true;
}

/*
====================
StudioEstimateGait

====================
*/
void CStudioModelRenderer::StudioEstimateGait( entity_state_t* pplayer )
{
    float dt;
    Vector est_velocity;

    dt = ( m_clTime - m_clOldTime );
    if( dt < 0 )
        dt = 0;
    else if( dt > 1.0 )
        dt = 1;

    if( dt == 0 || m_pPlayerInfo->renderframe == m_nFrameCount )
    {
        m_flGaitMovement = 0;
        return;
    }

    // est_velocity = pplayer->velocity + pplayer->prediction_error;
    if( m_fGaitEstimation )
    {
        est_velocity = m_pCurrentEntity->origin - m_pPlayerInfo->prevgaitorigin;
        m_pPlayerInfo->prevgaitorigin = m_pCurrentEntity->origin;
        m_flGaitMovement = est_velocity.Length();
        if( dt <= 0 || m_flGaitMovement / dt < 5 )
        {
            m_flGaitMovement = 0;
            est_velocity[0] = 0;
            est_velocity[1] = 0;
        }
    }
    else
    {
        est_velocity = pplayer->velocity;
        m_flGaitMovement = est_velocity.Length() * dt;
    }

    if( est_velocity[1] == 0 && est_velocity[0] == 0 )
    {
        float flYawDiff = m_pCurrentEntity->angles[YAW] - m_pPlayerInfo->gaityaw;
        flYawDiff = flYawDiff - (int)( flYawDiff / 360 ) * 360;
        if( flYawDiff > 180 )
            flYawDiff -= 360;
        if( flYawDiff < -180 )
            flYawDiff += 360;

        if( dt < 0.25 )
            flYawDiff *= dt * 4;
        else
            flYawDiff *= dt;

        m_pPlayerInfo->gaityaw += flYawDiff;
        m_pPlayerInfo->gaityaw = m_pPlayerInfo->gaityaw - (int)( m_pPlayerInfo->gaityaw / 360 ) * 360;

        m_flGaitMovement = 0;
    }
    else
    {
        m_pPlayerInfo->gaityaw = ( atan2( est_velocity[1], est_velocity[0] ) * 180 / PI );
        if( m_pPlayerInfo->gaityaw > 180 )
            m_pPlayerInfo->gaityaw = 180;
        if( m_pPlayerInfo->gaityaw < -180 )
            m_pPlayerInfo->gaityaw = -180;
    }
}

/*
====================
StudioProcessGait

====================
*/
void CStudioModelRenderer::StudioProcessGait( entity_state_t* pplayer )
{
    mstudioseqdesc_t* pseqdesc;
    float dt;
    int iBlend;
    float flYaw; // view direction relative to movement

    if( m_pCurrentEntity->curstate.sequence >= m_pStudioHeader->numseq )
    {
        m_pCurrentEntity->curstate.sequence = 0;
    }

    pseqdesc = ( mstudioseqdesc_t* )( ( byte* )m_pStudioHeader + m_pStudioHeader->seqindex ) + m_pCurrentEntity->curstate.sequence;

    StudioPlayerBlend( pseqdesc, &iBlend, &m_pCurrentEntity->angles[PITCH] );

    m_pCurrentEntity->latched.prevangles[PITCH] = m_pCurrentEntity->angles[PITCH];
    m_pCurrentEntity->curstate.blending[0] = iBlend;
    m_pCurrentEntity->latched.prevblending[0] = m_pCurrentEntity->curstate.blending[0];
    m_pCurrentEntity->latched.prevseqblending[0] = m_pCurrentEntity->curstate.blending[0];

    // Con_DPrintf("%f %d\n", m_pCurrentEntity->angles[PITCH], m_pCurrentEntity->blending[0] );

    dt = ( m_clTime - m_clOldTime );
    if( dt < 0 )
        dt = 0;
    else if( dt > 1.0 )
        dt = 1;

    StudioEstimateGait( pplayer );

    // Con_DPrintf("%f %f\n", m_pCurrentEntity->angles[YAW], m_pPlayerInfo->gaityaw );

    // calc side to side turning
    flYaw = m_pCurrentEntity->angles[YAW] - m_pPlayerInfo->gaityaw;
    flYaw = flYaw - (int)( flYaw / 360 ) * 360;
    if( flYaw < -180 )
        flYaw = flYaw + 360;
    if( flYaw > 180 )
        flYaw = flYaw - 360;

    if( flYaw > 120 )
    {
        m_pPlayerInfo->gaityaw = m_pPlayerInfo->gaityaw - 180;
        m_flGaitMovement = -m_flGaitMovement;
        flYaw = flYaw - 180;
    }
    else if( flYaw < -120 )
    {
        m_pPlayerInfo->gaityaw = m_pPlayerInfo->gaityaw + 180;
        m_flGaitMovement = -m_flGaitMovement;
        flYaw = flYaw + 180;
    }

    // adjust torso
    m_pCurrentEntity->curstate.controller[0] = ( ( flYaw / 4.0 ) + 30 ) / ( 60.0 / 255.0 );
    m_pCurrentEntity->curstate.controller[1] = ( ( flYaw / 4.0 ) + 30 ) / ( 60.0 / 255.0 );
    m_pCurrentEntity->curstate.controller[2] = ( ( flYaw / 4.0 ) + 30 ) / ( 60.0 / 255.0 );
    m_pCurrentEntity->curstate.controller[3] = ( ( flYaw / 4.0 ) + 30 ) / ( 60.0 / 255.0 );
    m_pCurrentEntity->latched.prevcontroller[0] = m_pCurrentEntity->curstate.controller[0];
    m_pCurrentEntity->latched.prevcontroller[1] = m_pCurrentEntity->curstate.controller[1];
    m_pCurrentEntity->latched.prevcontroller[2] = m_pCurrentEntity->curstate.controller[2];
    m_pCurrentEntity->latched.prevcontroller[3] = m_pCurrentEntity->curstate.controller[3];

    m_pCurrentEntity->angles[YAW] = m_pPlayerInfo->gaityaw;
    if( m_pCurrentEntity->angles[YAW] < -0 )
        m_pCurrentEntity->angles[YAW] += 360;
    m_pCurrentEntity->latched.prevangles[YAW] = m_pCurrentEntity->angles[YAW];

    if( pplayer->gaitsequence >= m_pStudioHeader->numseq )
    {
        pplayer->gaitsequence = 0;
    }

    pseqdesc = ( mstudioseqdesc_t* )( ( byte* )m_pStudioHeader + m_pStudioHeader->seqindex ) + pplayer->gaitsequence;

    // calc gait frame
    if( pseqdesc->linearmovement[0] > 0 )
    {
        m_pPlayerInfo->gaitframe += ( m_flGaitMovement / pseqdesc->linearmovement[0] ) * pseqdesc->numframes;
    }
    else
    {
        m_pPlayerInfo->gaitframe += pseqdesc->fps * dt;
    }

    // do modulo
    m_pPlayerInfo->gaitframe = m_pPlayerInfo->gaitframe - (int)( m_pPlayerInfo->gaitframe / pseqdesc->numframes ) * pseqdesc->numframes;
    if( m_pPlayerInfo->gaitframe < 0 )
        m_pPlayerInfo->gaitframe += pseqdesc->numframes;
}

/*
====================
StudioDrawPlayer

====================
*/
bool CStudioModelRenderer::StudioDrawPlayer( int flags, entity_state_t* pplayer )
{
    alight_t lighting;
    Vector dir;

    m_pCurrentEntity = IEngineStudio.GetCurrentEntity();
    IEngineStudio.GetTimes( &m_nFrameCount, &m_clTime, &m_clOldTime );
    IEngineStudio.GetViewInfo( m_vRenderOrigin, m_vUp, m_vRight, m_vNormal );
    IEngineStudio.GetAliasScale( &m_fSoftwareXScale, &m_fSoftwareYScale );

    m_nPlayerIndex = pplayer->number - 1;

    if( m_nPlayerIndex < 0 || m_nPlayerIndex >= gEngfuncs.GetMaxClients() )
        return false;


    m_pRenderModel = IEngineStudio.SetupPlayerModel( m_nPlayerIndex );


    if( m_pRenderModel == nullptr )
        return false;

    m_pStudioHeader = ( studiohdr_t* )IEngineStudio.Mod_Extradata( m_pRenderModel );
    IEngineStudio.StudioSetHeader( m_pStudioHeader );
    IEngineStudio.SetRenderModel( m_pRenderModel );

    if( 0 != pplayer->gaitsequence )
    {
        Vector orig_angles;
        m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );

        orig_angles = m_pCurrentEntity->angles;

        StudioProcessGait( pplayer );

        m_pPlayerInfo->gaitsequence = pplayer->gaitsequence;
        m_pPlayerInfo = nullptr;

        StudioSetUpTransform( false );
        m_pCurrentEntity->angles = orig_angles;
    }
    else
    {
        m_pCurrentEntity->curstate.controller[0] = 127;
        m_pCurrentEntity->curstate.controller[1] = 127;
        m_pCurrentEntity->curstate.controller[2] = 127;
        m_pCurrentEntity->curstate.controller[3] = 127;
        m_pCurrentEntity->latched.prevcontroller[0] = m_pCurrentEntity->curstate.controller[0];
        m_pCurrentEntity->latched.prevcontroller[1] = m_pCurrentEntity->curstate.controller[1];
        m_pCurrentEntity->latched.prevcontroller[2] = m_pCurrentEntity->curstate.controller[2];
        m_pCurrentEntity->latched.prevcontroller[3] = m_pCurrentEntity->curstate.controller[3];

        m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );
        m_pPlayerInfo->gaitsequence = 0;

        StudioSetUpTransform( false );
    }

    if( ( flags & STUDIO_RENDER ) != 0 )
    {
        // see if the bounding box lets us trivially reject, also sets
        if( 0 == IEngineStudio.StudioCheckBBox() )
            return false;

        ( *m_pModelsDrawn )++;
        ( *m_pStudioModelCount )++; // render data cache cookie

        if( m_pStudioHeader->numbodyparts == 0 )
            return true;
    }

    m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );
    StudioSetupBones();
    StudioSaveBones();
    m_pPlayerInfo->renderframe = m_nFrameCount;

    m_pPlayerInfo = nullptr;

    if( ( flags & STUDIO_EVENTS ) != 0 )
    {
        StudioCalcAttachments();
        IEngineStudio.StudioClientEvents();
        // copy attachments into global entity array
        if( m_pCurrentEntity->index > 0 )
        {
            cl_entity_t* ent = gEngfuncs.GetEntityByIndex( m_pCurrentEntity->index );

            memcpy( ent->attachment, m_pCurrentEntity->attachment, sizeof( Vector ) * 4 );
        }
    }

    if( ( flags & STUDIO_RENDER ) != 0 )
    {
        if( 0 != m_pCvarHiModels->value && m_pRenderModel != m_pCurrentEntity->model )
        {
            // show highest resolution multiplayer model
            m_pCurrentEntity->curstate.body = 255;
        }

        if( !( m_pCvarDeveloper->value == 0 && gEngfuncs.GetMaxClients() == 1 ) && ( m_pRenderModel == m_pCurrentEntity->model ) )
        {
            m_pCurrentEntity->curstate.body = 1; // force helmet
        }

        lighting.plightvec = dir;
        IEngineStudio.StudioDynamicLight( m_pCurrentEntity, &lighting );

        IEngineStudio.StudioEntityLight( &lighting );

        // model and frame independant
        IEngineStudio.StudioSetupLighting( &lighting );

        m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );

        // get remap colors
        m_nTopColor = m_pPlayerInfo->topcolor;
        m_nBottomColor = m_pPlayerInfo->bottomcolor;


        // bounds check
        if( m_nTopColor < 0 )
            m_nTopColor = 0;
        if( m_nTopColor > 360 )
            m_nTopColor = 360;
        if( m_nBottomColor < 0 )
            m_nBottomColor = 0;
        if( m_nBottomColor > 360 )
            m_nBottomColor = 360;

        IEngineStudio.StudioSetRemapColors( m_nTopColor, m_nBottomColor );

        StudioRenderModel();
        m_pPlayerInfo = nullptr;

        if( 0 != pplayer->weaponmodel )
        {
            cl_entity_t saveent = *m_pCurrentEntity;

            model_t* pweaponmodel = IEngineStudio.GetModelByIndex( pplayer->weaponmodel );

            m_pStudioHeader = ( studiohdr_t* )IEngineStudio.Mod_Extradata( pweaponmodel );
            IEngineStudio.StudioSetHeader( m_pStudioHeader );


            StudioMergeBones( pweaponmodel );

            IEngineStudio.StudioSetupLighting( &lighting );

            StudioRenderModel();

            StudioCalcAttachments();

            *m_pCurrentEntity = saveent;
        }
    }

    return true;
}

/*
====================
StudioCalcAttachments

====================
*/
void CStudioModelRenderer::StudioCalcAttachments()
{
    int i;
    mstudioattachment_t* pattachment;

    if( m_pStudioHeader->numattachments > 4 )
    {
        gEngfuncs.Con_DPrintf( "Too many attachments on %s\n", m_pCurrentEntity->model->name );
        exit( -1 );
    }

    // calculate attachment points
    pattachment = ( mstudioattachment_t* )( ( byte* )m_pStudioHeader + m_pStudioHeader->attachmentindex );
    for( i = 0; i < m_pStudioHeader->numattachments; i++ )
    {
        VectorTransform( pattachment[i].org, ( *m_plighttransform )[pattachment[i].bone], m_pCurrentEntity->attachment[i] );
    }
}

/*
====================
StudioRenderModel

====================
*/
void CStudioModelRenderer::StudioRenderModel()
{
    IEngineStudio.SetChromeOrigin();
    IEngineStudio.SetForceFaceFlags( 0 );

    if( m_pCurrentEntity->curstate.renderfx == kRenderFxGlowShell )
    {
        m_pCurrentEntity->curstate.renderfx = kRenderFxNone;
        StudioRenderFinal();

        if( 0 == IEngineStudio.IsHardware() )
        {
            gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
        }

        IEngineStudio.SetForceFaceFlags( STUDIO_NF_CHROME );

        gEngfuncs.pTriAPI->SpriteTexture( m_pChromeSprite, 0 );
        m_pCurrentEntity->curstate.renderfx = kRenderFxGlowShell;

        StudioRenderFinal();
        if( 0 == IEngineStudio.IsHardware() )
        {
            gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
        }
    }
    else
    {
        StudioRenderFinal();
    }
}

/*
====================
StudioRenderFinal_Software

====================
*/
void CStudioModelRenderer::StudioRenderFinal_Software()
{
    int i;

    // Note, rendermode set here has effect in SW
    IEngineStudio.SetupRenderer( 0 );

    if( m_pCvarDrawEntities->value == 2 )
    {
        IEngineStudio.StudioDrawBones();
    }
    else if( m_pCvarDrawEntities->value == 3 )
    {
        IEngineStudio.StudioDrawHulls();
    }
    else
    {
        for( i = 0; i < m_pStudioHeader->numbodyparts; i++ )
        {
            IEngineStudio.StudioSetupModel( i, (void**)&m_pBodyPart, (void**)&m_pSubModel );
            IEngineStudio.StudioDrawPoints();
        }
    }

    if( m_pCvarDrawEntities->value == 4 )
    {
        gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
        IEngineStudio.StudioDrawHulls();
        gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
    }

    if( m_pCvarDrawEntities->value == 5 )
    {
        IEngineStudio.StudioDrawAbsBBox();
    }

    IEngineStudio.RestoreRenderer();
}

/*
====================
StudioRenderFinal_Hardware

====================
*/
void CStudioModelRenderer::StudioRenderFinal_Hardware()
{
    int i;
    int rendermode;

    rendermode = 0 != IEngineStudio.GetForceFaceFlags() ? kRenderTransAdd : m_pCurrentEntity->curstate.rendermode;
    IEngineStudio.SetupRenderer( rendermode );

    if( m_pCvarDrawEntities->value == 2 )
    {
        IEngineStudio.StudioDrawBones();
    }
    else if( m_pCvarDrawEntities->value == 3 )
    {
        IEngineStudio.StudioDrawHulls();
    }
    else
    {
        for( i = 0; i < m_pStudioHeader->numbodyparts; i++ )
        {
            IEngineStudio.StudioSetupModel( i, (void**)&m_pBodyPart, (void**)&m_pSubModel );

            if( m_fDoInterp )
            {
                // interpolation messes up bounding boxes.
                m_pCurrentEntity->trivial_accept = 0;
            }

            IEngineStudio.GL_SetRenderMode( rendermode );
            IEngineStudio.StudioDrawPoints();
            IEngineStudio.GL_StudioDrawShadow();
        }
    }

    if( m_pCvarDrawEntities->value == 4 )
    {
        gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
        IEngineStudio.StudioDrawHulls();
        gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
    }

    IEngineStudio.RestoreRenderer();
}

/*
====================
StudioRenderFinal

====================
*/
void CStudioModelRenderer::StudioRenderFinal()
{
    if( 0 != IEngineStudio.IsHardware() )
    {
        StudioRenderFinal_Hardware();
    }
    else
    {
        StudioRenderFinal_Software();
    }
}
