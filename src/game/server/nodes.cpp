/***
 *
 *    Copyright (c) 1996-2001, Valve LLC. All rights reserved.
 *
 *    This product contains software technology licensed from Id
 *    Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *    All Rights Reserved.
 *
 *   This source code contains proprietary and confidential information of
 *   Valve LLC and its suppliers.  Access to this code is restricted to
 *   persons who have executed a written SDK license with Valve.  Any access,
 *   use or distribution of this code by or to any unlicensed person is illegal.
 *
 ****/
//=========================================================
// nodes.cpp - AI node tree stuff.
//=========================================================

#include <limits>
#include <string>

#include "cbase.h"
#include "CCorpse.h"
#include "nodes.h"
#include "doors.h"
#include "filesystem_utils.h"

#define HULL_STEP_SIZE 16 // how far the test hull moves on each step
#define NODE_HEIGHT 8      // how high to lift nodes off the ground after we drop them all (make stair/ramp mapping easier)

// to help eliminate node clutter by level designers, this is used to cap how many other nodes
// any given node is allowed to 'see' in the first stage of graph creation "LinkVisibleNodes()".
#define MAX_NODE_INITIAL_LINKS 128
#define MAX_NODES 1024

CGraph WorldGraph;

LINK_ENTITY_TO_CLASS( info_node, CNodeEnt );
LINK_ENTITY_TO_CLASS( info_node_air, CNodeEnt );

//=========================================================
// CGraph - InitGraph - prepares the graph for use. Frees any
// memory currently in use by the world graph, NULLs
// all pointers, and zeros the node count.
//=========================================================
void CGraph::InitGraph()
{

    // Make the graph unavailable
    //
    m_fGraphPresent = 0;
    m_fGraphPointersSet = 0;
    m_fRoutingComplete = 0;

    // Free the link pool
    //
    if( m_pLinkPool )
    {
        free( m_pLinkPool );
        m_pLinkPool = nullptr;
    }

    // Free the node info
    //
    if( m_pNodes )
    {
        free( m_pNodes );
        m_pNodes = nullptr;
    }

    if( m_di )
    {
        free( m_di );
        m_di = nullptr;
    }

    // Free the routing info.
    //
    if( m_pRouteInfo )
    {
        free( m_pRouteInfo );
        m_pRouteInfo = nullptr;
    }

    if( m_pHashLinks )
    {
        free( m_pHashLinks );
        m_pHashLinks = nullptr;
    }

    // Zero node and link counts
    //
    m_cNodes = 0;
    m_cLinks = 0;
    m_nRouteInfo = 0;

    m_iLastActiveIdleSearch = 0;
    m_iLastCoverSearch = 0;
}

//=========================================================
// CGraph - AllocNodes - temporary function that mallocs a
// reasonable number of nodes so we can build the path which
// will be saved to disk.
//=========================================================
bool CGraph::AllocNodes()
{
    //  malloc all of the nodes
    m_pNodes = ( CNode* )calloc( sizeof( CNode ), MAX_NODES );

    // could not malloc space for all the nodes!
    if( !m_pNodes )
    {
        Logger->error( "Couldn't malloc {} nodes!", m_cNodes );
        return false;
    }

    return true;
}

//=========================================================
// CGraph - LinkEntForLink - sometimes the ent that blocks
// a path is a usable door, in which case the monster just
// needs to face the door and fire it. In other cases, the
// monster needs to operate a button or lever to get the
// door to open. This function will return a pointer to the
// button if the monster needs to hit a button to open the
// door, or returns a pointer to the door if the monster
// need only use the door.
//
// pNode is the node the monster will be standing on when it
// will need to stop and trigger the ent.
//=========================================================
entvars_t* CGraph::LinkEntForLink( CLink* pLink, CNode* pNode )
{
    auto pevLinkEnt = pLink->m_pLinkEnt;
    if( !pevLinkEnt )
        return nullptr;

    CBaseEntity* entity = CBaseEntity::Instance( pevLinkEnt );

    if( entity->ClassnameIs( "func_door" ) || entity->ClassnameIs( "func_door_rotating" ) )
    {

        ///!!!UNDONE - check for TOGGLE or STAY open doors here. If a door is in the way, and is
        // TOGGLE or STAY OPEN, even monsters that can't open doors can go that way.

        if( ( entity->pev->spawnflags & SF_DOOR_USE_ONLY ) != 0 )
        { // door is use only, so the door is all the monster has to worry about
            return pevLinkEnt;
        }

        TraceResult tr;

        // find the button or trigger
        for( auto trigger : UTIL_FindEntitiesByTarget( entity->GetTargetname() ) )
        {
            if( trigger->ClassnameIs( "func_button" ) || trigger->ClassnameIs( "func_button_rotating" ) )
            { // only buttons are handled right now.

                // trace from the node to the trigger, make sure it's one we can see from the node.
                // !!!HACKHACK Use bodyqueue here cause there are no ents we really wish to ignore!
                UTIL_TraceLine( pNode->m_vecOrigin, VecBModelOrigin( trigger ), ignore_monsters, g_pBodyQueueHead->edict(), &tr );

                if( tr.pHit == trigger->edict() )
                { // good to go!
                    return VARS( tr.pHit );
                }
            }
        }

        // no trigger found

        // right now this is a problem among auto-open doors, or any door that opens through the use
        // of a trigger brush. Trigger brushes have no models, and don't show up in searches. Just allow
        // monsters to open these sorts of doors for now.
        return pevLinkEnt;
    }
    else
    {
        Logger->trace( "Unsupported PathEnt: '{}'", entity->GetClassname() );
        return nullptr;
    }
}

//=========================================================
// CGraph - HandleLinkEnt - a brush ent is between two
// nodes that would otherwise be able to see each other.
// Given the monster's capability, determine whether
// or not the monster can go this way.
//=========================================================
bool CGraph::HandleLinkEnt( int iNode, entvars_t* pevLinkEnt, int afCapMask, NODEQUERY queryType )
{
    if( 0 == m_fGraphPresent || 0 == m_fGraphPointersSet )
    { // protect us in the case that the node graph isn't available
        Logger->error( "Graph not ready!" );
        return false;
    }

    if( FNullEnt( pevLinkEnt ) )
    {
        Logger->debug( "dead path ent!" );
        return true;
    }

    CBaseEntity* entity = CBaseEntity::Instance( pevLinkEnt );

    // func_door
    if( entity->ClassnameIs( "func_door" ) || entity->ClassnameIs( "func_door_rotating" ) )
    { // ent is a door.
        auto door = static_cast<CBaseToggle*>( entity );

        if( ( door->pev->spawnflags & SF_DOOR_USE_ONLY ) != 0 )
        { // door is use only.

            if( ( afCapMask & bits_CAP_OPEN_DOORS ) != 0 )
            { // let monster right through if he can open doors
                return true;
            }
            else
            {
                // monster should try for it if the door is open and looks as if it will stay that way
                if( door->m_toggle_state == TS_AT_TOP && ( door->pev->spawnflags & SF_DOOR_NO_AUTO_RETURN ) != 0 )
                {
                    return true;
                }

                return false;
            }
        }
        else
        { // door must be opened with a button or trigger field.

            // monster should try for it if the door is open and looks as if it will stay that way
            if( door->m_toggle_state == TS_AT_TOP && ( door->pev->spawnflags & SF_DOOR_NO_AUTO_RETURN ) != 0 )
            {
                return true;
            }
            if( ( afCapMask & bits_CAP_OPEN_DOORS ) != 0 )
            {
                if( ( door->pev->spawnflags & SF_DOOR_NOMONSTERS ) == 0 || queryType == NODEGRAPH_STATIC )
                    return true;
            }

            return false;
        }
    }
    // func_breakable
    else if( entity->ClassnameIs( "func_breakable" ) && queryType == NODEGRAPH_STATIC )
    {
        return true;
    }
    else
    {
        Logger->trace( "Unhandled Ent in Path {}", entity->GetClassname() );
        return false;
    }
}

#if 0
//=========================================================
// FindNearestLink - finds the connection (line) nearest
// the given point. Returns false if fails, or true if it
// has stuffed the index into the nearest link pool connection
// into the passed int pointer, and a bool telling whether or 
// not the point is along the line into the passed bool pointer.
//=========================================================
int    CGraph::FindNearestLink( const Vector& vecTestPoint, int* piNearestLink, bool* pfAlongLine )
{
    int            i, j;// loops

    int            iNearestLink;// index into the link pool, this is the nearest node at any time. 
    float        flMinDist;// the distance of of the nearest case so far
    float        flDistToLine;// the distance of the current test case

    bool        fCurrentAlongLine;
    bool        fSuccess;

    //float        flConstant;// line constant
    Vector        vecSpot1, vecSpot2;
    Vector2D    vec2Spot1, vec2Spot2, vec2TestPoint;
    Vector2D    vec2Normal;// line normal
    Vector2D    vec2Line;

    TraceResult    tr;

    iNearestLink = -1;// prepare for failure
    fSuccess = false;

    flMinDist = 9999;// anything will be closer than this

// go through all of the nodes, and each node's connections    
    int    cSkip = 0;// how many links proper pairing allowed us to skip
    int cChecked = 0;// how many links were checked

    for( i = 0; i < m_cNodes; i++ )
    {
        vecSpot1 = m_pNodes[i].m_vecOrigin;

        if( m_pNodes[i].m_cNumLinks <= 0 )
        {// this shouldn't happen!
            Logger->error( "Node {} has no links", i );
            continue;
        }

        for( j = 0; j < m_pNodes[i].m_cNumLinks; j++ )
        {
            /*
            !!!This optimization only works when the node graph consists of properly linked pairs.
            if ( INodeLink ( i, j ) <= i )
            {
                // since we're going through the nodes in order, don't check
                // any connections whose second node is lower in the list
                // than the node we're currently working with. This eliminates
                // redundant checks.
                cSkip++;
                continue;
            }
            */

            vecSpot2 = PNodeLink( i, j )->m_vecOrigin;

            // these values need a little attention now and then, or sometimes ramps cause trouble.
            if( fabs( vecSpot1.z - vecTestPoint.z ) > 48 && fabs( vecSpot2.z - vecTestPoint.z ) > 48 )
            {
                // if both endpoints of the line are 32 units or more above or below the monster, 
                // the monster won't be able to get to them, so we do a bit of trivial rejection here.
                // this may change if monsters are allowed to jump down. 
                // 
                // !!!LATER: some kind of clever X/Y hashing should be used here, too
                continue;
            }

            // now we have two endpoints for a line segment that we've not already checked. 
            // since all lines that make it this far are within -/+ 32 units of the test point's
            // Z Plane, we can get away with doing the point->line check in 2d.

            cChecked++;

            vec2Spot1 = vecSpot1.Make2D();
            vec2Spot2 = vecSpot2.Make2D();
            vec2TestPoint = vecTestPoint.Make2D();

            // get the line normal.
            vec2Line = ( vec2Spot1 - vec2Spot2 ).Normalize();
            vec2Normal.x = -vec2Line.y;
            vec2Normal.y = vec2Line.x;

            if( DotProduct( vec2Line, ( vec2TestPoint - vec2Spot1 ) ) > 0 )
            {// point outside of line
                flDistToLine = ( vec2TestPoint - vec2Spot1 ).Length();
                fCurrentAlongLine = false;
            }
            else if( DotProduct( vec2Line, ( vec2TestPoint - vec2Spot2 ) ) < 0 )
            {// point outside of line
                flDistToLine = ( vec2TestPoint - vec2Spot2 ).Length();
                fCurrentAlongLine = false;
            }
            else
            {// point inside line
                flDistToLine = fabs( DotProduct( vec2TestPoint - vec2Spot2, vec2Normal ) );
                fCurrentAlongLine = true;
            }

            if( flDistToLine < flMinDist )
            {// just found a line nearer than any other so far

                UTIL_TraceLine( vecTestPoint, SourceNode( i, j ).m_vecOrigin, ignore_monsters, g_pBodyQueueHead, &tr );

                if( tr.flFraction != 1.0 )
                {// crap. can't see the first node of this link, try to see the other

                    UTIL_TraceLine( vecTestPoint, DestNode( i, j ).m_vecOrigin, ignore_monsters, g_pBodyQueueHead, &tr );
                    if( tr.flFraction != 1.0 )
                    {// can't use this link, cause can't see either node!
                        continue;
                    }

                }

                fSuccess = true;// we know there will be something to return.
                flMinDist = flDistToLine;
                iNearestLink = m_pNodes[i].m_iFirstLink + j;
                *piNearestLink = m_pNodes[i].m_iFirstLink + j;
                *pfAlongLine = fCurrentAlongLine;
            }
        }
    }

    /*
        if ( fSuccess )
        {
            WRITE_BYTE(MSG_BROADCAST, SVC_TEMPENTITY);
            WRITE_BYTE(MSG_BROADCAST, TE_SHOWLINE);

            WRITE_COORD(MSG_BROADCAST, m_pNodes[ m_pLinkPool[ iNearestLink ].m_iSrcNode ].m_vecOrigin.x );
            WRITE_COORD(MSG_BROADCAST, m_pNodes[ m_pLinkPool[ iNearestLink ].m_iSrcNode ].m_vecOrigin.y );
            WRITE_COORD(MSG_BROADCAST, m_pNodes[ m_pLinkPool[ iNearestLink ].m_iSrcNode ].m_vecOrigin.z + NODE_HEIGHT);

            WRITE_COORD(MSG_BROADCAST, m_pNodes[ m_pLinkPool[ iNearestLink ].m_iDestNode ].m_vecOrigin.x );
            WRITE_COORD(MSG_BROADCAST, m_pNodes[ m_pLinkPool[ iNearestLink ].m_iDestNode ].m_vecOrigin.y );
            WRITE_COORD(MSG_BROADCAST, m_pNodes[ m_pLinkPool[ iNearestLink ].m_iDestNode ].m_vecOrigin.z + NODE_HEIGHT);
        }
    */

    Logger->trace( "{} Checked", cChecked );
    return fSuccess;
}

#endif

int CGraph::HullIndex( const CBaseEntity* pEntity )
{
    if( pEntity->pev->movetype == MOVETYPE_FLY )
        return NODE_FLY_HULL;

    if( pEntity->pev->mins == Vector( -12, -12, 0 ) )
        return NODE_SMALL_HULL;
    else if( pEntity->pev->mins == VEC_HUMAN_HULL_MIN )
        return NODE_HUMAN_HULL;
    else if( pEntity->pev->mins == Vector( -32, -32, 0 ) )
        return NODE_LARGE_HULL;

    //    Logger->error("Unknown Hull Mins!");
    return NODE_HUMAN_HULL;
}


int CGraph::NodeType( const CBaseEntity* pEntity )
{
    if( pEntity->pev->movetype == MOVETYPE_FLY )
    {
        if( pEntity->pev->waterlevel != WaterLevel::Dry )
        {
            return bits_NODE_WATER;
        }
        else
        {
            return bits_NODE_AIR;
        }
    }
    return bits_NODE_LAND;
}


// Sum up graph weights on the path from iStart to iDest to determine path length
float CGraph::PathLength( int iStart, int iDest, int iHull, int afCapMask )
{
    float distance = 0;
    int iNext;

    int iMaxLoop = m_cNodes;

    int iCurrentNode = iStart;
    int iCap = CapIndex( afCapMask );

    while( iCurrentNode != iDest )
    {
        if( iMaxLoop-- <= 0 )
        {
            Logger->info( "Route Failure" );
            return 0;
        }

        iNext = NextNodeInRoute( iCurrentNode, iDest, iHull, iCap );
        if( iCurrentNode == iNext )
        {
            // Logger->debug("SVD: Can't get there from here..");
            return 0;
        }

        int iLink;
        HashSearch( iCurrentNode, iNext, iLink );
        if( iLink < 0 )
        {
            Logger->info( "HashLinks is broken from {} to {}.", iCurrentNode, iDest );
            return 0;
        }
        CLink& link = Link( iLink );
        distance += link.m_flWeight;

        iCurrentNode = iNext;
    }

    return distance;
}


// Parse the routing table at iCurrentNode for the next node on the shortest path to iDest
int CGraph::NextNodeInRoute( int iCurrentNode, int iDest, int iHull, int iCap )
{
    int iNext = iCurrentNode;
    int nCount = iDest + 1;
    char* pRoute = m_pRouteInfo + m_pNodes[iCurrentNode].m_pNextBestNode[iHull][iCap];

    // Until we decode the next best node
    //
    while( nCount > 0 )
    {
        char ch = *pRoute++;
        // Logger->trace("C({})", ch);
        if( ch < 0 )
        {
            // Sequence phrase
            //
            ch = -ch;
            if( nCount <= ch )
            {
                iNext = iDest;
                nCount = 0;
                // Logger->trace("SEQ: iNext/iDest={}", iNext);
            }
            else
            {
                // Logger->trace("SEQ: nCount + ch ({} + {})", nCount, ch);
                nCount = nCount - ch;
            }
        }
        else
        {
            // Logger->trace("C({})", *pRoute);

            // Repeat phrase
            //
            if( nCount <= ch + 1 )
            {
                iNext = iCurrentNode + *pRoute;
                if( iNext >= m_cNodes )
                    iNext -= m_cNodes;
                else if( iNext < 0 )
                    iNext += m_cNodes;
                nCount = 0;
                // Logger->trace("REP: iNext={}", iNext);
            }
            else
            {
                // Logger->trace("REP: nCount - ch+1 ({} - {}+1)", nCount, ch);
                nCount = nCount - ch - 1;
            }
            pRoute++;
        }
    }

    return iNext;
}


//=========================================================
// CGraph - FindShortestPath
//
// accepts a capability mask (afCapMask), and will only
// find a path usable by a monster with those capabilities
// returns the number of nodes copied into supplied array
//=========================================================
int CGraph::FindShortestPath( int* piPath, int iStart, int iDest, int iHull, int afCapMask )
{
    int iVisitNode;
    int iCurrentNode;
    int iNumPathNodes;
    int iHullMask;

    if( 0 == m_fGraphPresent || 0 == m_fGraphPointersSet )
    { // protect us in the case that the node graph isn't available or built
        Logger->error( "Graph not ready!" );
        return 0;
    }

    if( iStart < 0 || iStart > m_cNodes )
    { // The start node is bad?
        Logger->error( "Can't build a path, iStart is {}!", iStart );
        return 0;
    }

    if( iStart == iDest )
    {
        piPath[0] = iStart;
        piPath[1] = iDest;
        return 2;
    }

    // Is routing information present.
    //
    if( 0 != m_fRoutingComplete )
    {
        int iCap = CapIndex( afCapMask );

        iNumPathNodes = 0;
        piPath[iNumPathNodes++] = iStart;
        iCurrentNode = iStart;
        int iNext;

        // Logger->trace("GOAL: {} to {}", iStart, iDest);

        // Until we arrive at the destination
        //
        while( iCurrentNode != iDest )
        {
            iNext = NextNodeInRoute( iCurrentNode, iDest, iHull, iCap );
            if( iCurrentNode == iNext )
            {
                // Logger->trace("SVD: Can't get there from here..");
                return 0;
                break;
            }
            if( iNumPathNodes >= MAX_PATH_SIZE )
            {
                // Logger->trace("SVD: Don't return the entire path.");
                break;
            }
            piPath[iNumPathNodes++] = iNext;
            iCurrentNode = iNext;
        }
        // Logger->trace("SVD: Path with {} nodes.", iNumPathNodes);
    }
    else
    {
        CQueuePriority queue;

        switch ( iHull )
        {
        default:
        case NODE_SMALL_HULL:
            iHullMask = bits_LINK_SMALL_HULL;
            break;
        case NODE_HUMAN_HULL:
            iHullMask = bits_LINK_HUMAN_HULL;
            break;
        case NODE_LARGE_HULL:
            iHullMask = bits_LINK_LARGE_HULL;
            break;
        case NODE_FLY_HULL:
            iHullMask = bits_LINK_FLY_HULL;
            break;
        }

        // Mark all the nodes as unvisited.
        //
        int i;
        for( i = 0; i < m_cNodes; i++ )
        {
            m_pNodes[i].m_flClosestSoFar = -1.0;
        }

        m_pNodes[iStart].m_flClosestSoFar = 0.0;
        m_pNodes[iStart].m_iPreviousNode = iStart; // tag this as the origin node
        queue.Insert( iStart, 0.0 );                   // insert start node

        while( !queue.Empty() )
        {
            // now pull a node out of the queue
            float flCurrentDistance;
            iCurrentNode = queue.Remove( flCurrentDistance );

            // For straight-line weights, the following Shortcut works. For arbitrary weights,
            // it doesn't.
            //
            if( iCurrentNode == iDest )
                break;

            CNode* pCurrentNode = &m_pNodes[iCurrentNode];

            for( i = 0; i < pCurrentNode->m_cNumLinks; i++ )
            { // run through all of this node's neighbors

                iVisitNode = INodeLink( iCurrentNode, i );
                if( ( m_pLinkPool[m_pNodes[iCurrentNode].m_iFirstLink + i].m_afLinkInfo & iHullMask ) != iHullMask )
                { // monster is too large to walk this connection
                    // Logger->trace("fat ass {}/{}",m_pLinkPool[ m_pNodes[ iCurrentNode ].m_iFirstLink + i ].m_afLinkInfo, iMonsterHull );
                    continue;
                }
                // check the connection from the current node to the node we're about to mark visited and push into the queue
                if( m_pLinkPool[m_pNodes[iCurrentNode].m_iFirstLink + i].m_pLinkEnt != nullptr )
                { // there's a brush ent in the way! Don't mark this node or put it into the queue unless the monster can negotiate it

                    if( !HandleLinkEnt( iCurrentNode, m_pLinkPool[m_pNodes[iCurrentNode].m_iFirstLink + i].m_pLinkEnt, afCapMask, NODEGRAPH_STATIC ) )
                    { // monster should not try to go this way.
                        continue;
                    }
                }
                float flOurDistance = flCurrentDistance + m_pLinkPool[m_pNodes[iCurrentNode].m_iFirstLink + i].m_flWeight;
                if( m_pNodes[iVisitNode].m_flClosestSoFar < -0.5 || flOurDistance < m_pNodes[iVisitNode].m_flClosestSoFar - 0.001 )
                {
                    m_pNodes[iVisitNode].m_flClosestSoFar = flOurDistance;
                    m_pNodes[iVisitNode].m_iPreviousNode = iCurrentNode;

                    queue.Insert( iVisitNode, flOurDistance );
                }
            }
        }
        if( m_pNodes[iDest].m_flClosestSoFar < -0.5 )
        { // Destination is unreachable, no path found.
            return 0;
        }

        // the queue is not empty

        // now we must walk backwards through the m_iPreviousNode field, and count how many connections there are in the path
        iCurrentNode = iDest;
        iNumPathNodes = 1; // count the dest

        while( iCurrentNode != iStart )
        {
            iNumPathNodes++;
            iCurrentNode = m_pNodes[iCurrentNode].m_iPreviousNode;
        }

        iCurrentNode = iDest;
        for( i = iNumPathNodes - 1; i >= 0; i-- )
        {
            piPath[i] = iCurrentNode;
            iCurrentNode = m_pNodes[iCurrentNode].m_iPreviousNode;
        }
    }

#if 0

    if( m_fRoutingComplete )
    {
        // This will draw the entire path that was generated for the monster.

        for( int i = 0; i < iNumPathNodes - 1; i++ )
        {
            MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
            WRITE_BYTE( TE_SHOWLINE );

            WRITE_COORD( m_pNodes[piPath[i]].m_vecOrigin.x );
            WRITE_COORD( m_pNodes[piPath[i]].m_vecOrigin.y );
            WRITE_COORD( m_pNodes[piPath[i]].m_vecOrigin.z + NODE_HEIGHT );

            WRITE_COORD( m_pNodes[piPath[i + 1]].m_vecOrigin.x );
            WRITE_COORD( m_pNodes[piPath[i + 1]].m_vecOrigin.y );
            WRITE_COORD( m_pNodes[piPath[i + 1]].m_vecOrigin.z + NODE_HEIGHT );
            MESSAGE_END();
        }
    }

#endif
#if 0 // MAZE map
    MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
    WRITE_BYTE( TE_SHOWLINE );

    WRITE_COORD( m_pNodes[4].m_vecOrigin.x );
    WRITE_COORD( m_pNodes[4].m_vecOrigin.y );
    WRITE_COORD( m_pNodes[4].m_vecOrigin.z + NODE_HEIGHT );

    WRITE_COORD( m_pNodes[9].m_vecOrigin.x );
    WRITE_COORD( m_pNodes[9].m_vecOrigin.y );
    WRITE_COORD( m_pNodes[9].m_vecOrigin.z + NODE_HEIGHT );
    MESSAGE_END();
#endif

    return iNumPathNodes;
}

inline unsigned int Hash( const void* p, int len )
{
    CRC32_t ulCrc;
    CRC32_INIT( &ulCrc );
    CRC32_PROCESS_BUFFER( &ulCrc, p, len );
    return CRC32_FINAL( ulCrc );
}

void inline CalcBounds( int& Lower, int& Upper, int Goal, int Best )
{
    int Temp = 2 * Goal - Best;
    if( Best > Goal )
    {
        Lower = std::max( 0, Temp );
        Upper = Best;
    }
    else
    {
        Upper = std::min( 255, Temp );
        Lower = Best;
    }
}

// Convert from [-8192,8192] to [0, 255]
//
inline int CALC_RANGE( int x, int lower, int upper )
{
    return NUM_RANGES * ( x - lower ) / ( ( upper - lower + 1 ) );
}


void inline UpdateRange( int& minValue, int& maxValue, int Goal, int Best )
{
    int Lower, Upper;
    CalcBounds( Lower, Upper, Goal, Best );
    if( Upper < maxValue )
        maxValue = Upper;
    if( minValue < Lower )
        minValue = Lower;
}

void CGraph::CheckNode( Vector vecOrigin, int iNode )
{
    // Have we already seen this point before?.
    //
    if( m_di[iNode].m_CheckedEvent == m_CheckedCounter )
        return;
    m_di[iNode].m_CheckedEvent = m_CheckedCounter;

    float flDist = ( vecOrigin - m_pNodes[iNode].m_vecOriginPeek ).Length();

    if( flDist < m_flShortest )
    {
        TraceResult tr;

        // make sure that vecOrigin can trace to this node!
        UTIL_TraceLine( vecOrigin, m_pNodes[iNode].m_vecOriginPeek, ignore_monsters, nullptr, &tr );

        if( tr.flFraction == 1.0 )
        {
            m_iNearest = iNode;
            m_flShortest = flDist;

            UpdateRange( m_minX, m_maxX, CALC_RANGE( vecOrigin.x, m_RegionMin[0], m_RegionMax[0] ), m_pNodes[iNode].m_Region[0] );
            UpdateRange( m_minY, m_maxY, CALC_RANGE( vecOrigin.y, m_RegionMin[1], m_RegionMax[1] ), m_pNodes[iNode].m_Region[1] );
            UpdateRange( m_minZ, m_maxZ, CALC_RANGE( vecOrigin.z, m_RegionMin[2], m_RegionMax[2] ), m_pNodes[iNode].m_Region[2] );

            // From maxCircle, calculate maximum bounds box. All points must be
            // simultaneously inside all bounds of the box.
            //
            m_minBoxX = CALC_RANGE( vecOrigin.x - flDist, m_RegionMin[0], m_RegionMax[0] );
            m_maxBoxX = CALC_RANGE( vecOrigin.x + flDist, m_RegionMin[0], m_RegionMax[0] );
            m_minBoxY = CALC_RANGE( vecOrigin.y - flDist, m_RegionMin[1], m_RegionMax[1] );
            m_maxBoxY = CALC_RANGE( vecOrigin.y + flDist, m_RegionMin[1], m_RegionMax[1] );
            m_minBoxZ = CALC_RANGE( vecOrigin.z - flDist, m_RegionMin[2], m_RegionMax[2] );
            m_maxBoxZ = CALC_RANGE( vecOrigin.z + flDist, m_RegionMin[2], m_RegionMax[2] );
        }
    }
}

//=========================================================
// CGraph - FindNearestNode - returns the index of the node nearest
// the given vector -1 is failure (couldn't find a valid
// near node )
//=========================================================
int CGraph::FindNearestNode( const Vector& vecOrigin, CBaseEntity* pEntity )
{
    return FindNearestNode( vecOrigin, NodeType( pEntity ) );
}

int CGraph::FindNearestNode( const Vector& vecOrigin, int afNodeTypes )
{
    if( 0 == m_fGraphPresent || 0 == m_fGraphPointersSet )
    { // protect us in the case that the node graph isn't available
        Logger->error( "Graph not ready!" );
        return -1;
    }

    // Check with the cache
    //
    unsigned int iHash = ( CACHE_SIZE - 1 ) & Hash( &vecOrigin, sizeof( vecOrigin ) );
    if( m_Cache[iHash].v == vecOrigin )
    {
        // Logger->trace("Cache Hit.");
        return m_Cache[iHash].n;
    }
    else
    {
        // Logger->trace("Cache Miss.");
    }

    // Mark all points as unchecked.
    //
    m_CheckedCounter++;
    if( m_CheckedCounter == 0 )
    {
        for( int i = 0; i < m_cNodes; i++ )
        {
            m_di[i].m_CheckedEvent = 0;
        }
        m_CheckedCounter++;
    }

    m_iNearest = -1;
    m_flShortest = 999999.0; // just a big number.

    // If we can find a visible point, then let CalcBounds set the limits, but if
    // we have no visible point at all to start with, then don't restrict the limits.
    //
#if 1
    m_minX = 0;
    m_maxX = 255;
    m_minY = 0;
    m_maxY = 255;
    m_minZ = 0;
    m_maxZ = 255;
    m_minBoxX = 0;
    m_maxBoxX = 255;
    m_minBoxY = 0;
    m_maxBoxY = 255;
    m_minBoxZ = 0;
    m_maxBoxZ = 255;
#else
    m_minBoxX = CALC_RANGE( vecOrigin.x - flDist, m_RegionMin[0], m_RegionMax[0] );
    m_maxBoxX = CALC_RANGE( vecOrigin.x + flDist, m_RegionMin[0], m_RegionMax[0] );
    m_minBoxY = CALC_RANGE( vecOrigin.y - flDist, m_RegionMin[1], m_RegionMax[1] );
    m_maxBoxY = CALC_RANGE( vecOrigin.y + flDist, m_RegionMin[1], m_RegionMax[1] );
    m_minBoxZ = CALC_RANGE( vecOrigin.z - flDist, m_RegionMin[2], m_RegionMax[2] );
    m_maxBoxZ = CALC_RANGE( vecOrigin.z + flDist, m_RegionMin[2], m_RegionMax[2] )
        CalcBounds( m_minX, m_maxX, CALC_RANGE( vecOrigin.x, m_RegionMin[0], m_RegionMax[0] ), m_pNodes[m_iNearest].m_Region[0] );
    CalcBounds( m_minY, m_maxY, CALC_RANGE( vecOrigin.y, m_RegionMin[1], m_RegionMax[1] ), m_pNodes[m_iNearest].m_Region[1] );
    CalcBounds( m_minZ, m_maxZ, CALC_RANGE( vecOrigin.z, m_RegionMin[2], m_RegionMax[2] ), m_pNodes[m_iNearest].m_Region[2] );
#endif

    int halfX = ( m_minX + m_maxX ) / 2;
    int halfY = ( m_minY + m_maxY ) / 2;
    int halfZ = ( m_minZ + m_maxZ ) / 2;

    int j;

    for( int i = halfX; i >= m_minX; i-- )
    {
        for( j = m_RangeStart[0][i]; j <= m_RangeEnd[0][i]; j++ )
        {
            if( ( m_pNodes[m_di[j].m_SortedBy[0]].m_afNodeInfo & afNodeTypes ) == 0 )
                continue;

            int rgY = m_pNodes[m_di[j].m_SortedBy[0]].m_Region[1];
            if( rgY > m_maxBoxY )
                break;
            if( rgY < m_minBoxY )
                continue;

            int rgZ = m_pNodes[m_di[j].m_SortedBy[0]].m_Region[2];
            if( rgZ < m_minBoxZ )
                continue;
            if( rgZ > m_maxBoxZ )
                continue;
            CheckNode( vecOrigin, m_di[j].m_SortedBy[0] );
        }
    }

    for( int i = std::max( m_minY, halfY + 1 ); i <= m_maxY; i++ )
    {
        for( j = m_RangeStart[1][i]; j <= m_RangeEnd[1][i]; j++ )
        {
            if( ( m_pNodes[m_di[j].m_SortedBy[1]].m_afNodeInfo & afNodeTypes ) == 0 )
                continue;

            int rgZ = m_pNodes[m_di[j].m_SortedBy[1]].m_Region[2];
            if( rgZ > m_maxBoxZ )
                break;
            if( rgZ < m_minBoxZ )
                continue;
            int rgX = m_pNodes[m_di[j].m_SortedBy[1]].m_Region[0];
            if( rgX < m_minBoxX )
                continue;
            if( rgX > m_maxBoxX )
                continue;
            CheckNode( vecOrigin, m_di[j].m_SortedBy[1] );
        }
    }

    for( int i = std::min( m_maxZ, halfZ ); i >= m_minZ; i-- )
    {
        for( j = m_RangeStart[2][i]; j <= m_RangeEnd[2][i]; j++ )
        {
            if( ( m_pNodes[m_di[j].m_SortedBy[2]].m_afNodeInfo & afNodeTypes ) == 0 )
                continue;

            int rgX = m_pNodes[m_di[j].m_SortedBy[2]].m_Region[0];
            if( rgX > m_maxBoxX )
                break;
            if( rgX < m_minBoxX )
                continue;
            int rgY = m_pNodes[m_di[j].m_SortedBy[2]].m_Region[1];
            if( rgY < m_minBoxY )
                continue;
            if( rgY > m_maxBoxY )
                continue;
            CheckNode( vecOrigin, m_di[j].m_SortedBy[2] );
        }
    }

    for( int i = std::max( m_minX, halfX + 1 ); i <= m_maxX; i++ )
    {
        for( j = m_RangeStart[0][i]; j <= m_RangeEnd[0][i]; j++ )
        {
            if( ( m_pNodes[m_di[j].m_SortedBy[0]].m_afNodeInfo & afNodeTypes ) == 0 )
                continue;

            int rgY = m_pNodes[m_di[j].m_SortedBy[0]].m_Region[1];
            if( rgY > m_maxBoxY )
                break;
            if( rgY < m_minBoxY )
                continue;

            int rgZ = m_pNodes[m_di[j].m_SortedBy[0]].m_Region[2];
            if( rgZ < m_minBoxZ )
                continue;
            if( rgZ > m_maxBoxZ )
                continue;
            CheckNode( vecOrigin, m_di[j].m_SortedBy[0] );
        }
    }

    for( int i = std::min( m_maxY, halfY ); i >= m_minY; i-- )
    {
        for( j = m_RangeStart[1][i]; j <= m_RangeEnd[1][i]; j++ )
        {
            if( ( m_pNodes[m_di[j].m_SortedBy[1]].m_afNodeInfo & afNodeTypes ) == 0 )
                continue;

            int rgZ = m_pNodes[m_di[j].m_SortedBy[1]].m_Region[2];
            if( rgZ > m_maxBoxZ )
                break;
            if( rgZ < m_minBoxZ )
                continue;
            int rgX = m_pNodes[m_di[j].m_SortedBy[1]].m_Region[0];
            if( rgX < m_minBoxX )
                continue;
            if( rgX > m_maxBoxX )
                continue;
            CheckNode( vecOrigin, m_di[j].m_SortedBy[1] );
        }
    }

    for( int i = std::max( m_minZ, halfZ + 1 ); i <= m_maxZ; i++ )
    {
        for( j = m_RangeStart[2][i]; j <= m_RangeEnd[2][i]; j++ )
        {
            if( ( m_pNodes[m_di[j].m_SortedBy[2]].m_afNodeInfo & afNodeTypes ) == 0 )
                continue;

            int rgX = m_pNodes[m_di[j].m_SortedBy[2]].m_Region[0];
            if( rgX > m_maxBoxX )
                break;
            if( rgX < m_minBoxX )
                continue;
            int rgY = m_pNodes[m_di[j].m_SortedBy[2]].m_Region[1];
            if( rgY < m_minBoxY )
                continue;
            if( rgY > m_maxBoxY )
                continue;
            CheckNode( vecOrigin, m_di[j].m_SortedBy[2] );
        }
    }

#if 0
    // Verify our answers.
    //
    int iNearestCheck = -1;
    m_flShortest = 8192;// find nodes within this radius

    for( int i = 0; i < m_cNodes; i++ )
    {
        float flDist = ( vecOrigin - m_pNodes[i].m_vecOriginPeek ).Length();

        if( flDist < m_flShortest )
        {
            // make sure that vecOrigin can trace to this node!
            UTIL_TraceLine( vecOrigin, m_pNodes[i].m_vecOriginPeek, ignore_monsters, 0, &tr );

            if( tr.flFraction == 1.0 )
            {
                iNearestCheck = i;
                m_flShortest = flDist;
            }
        }
    }

    if( iNearestCheck != m_iNearest )
    {
        Logger->trace( "NOT closest {}({},{},{}) {}({},{},{}).",
            iNearestCheck,
            m_pNodes[iNearestCheck].m_vecOriginPeek.x,
            m_pNodes[iNearestCheck].m_vecOriginPeek.y,
            m_pNodes[iNearestCheck].m_vecOriginPeek.z,
            m_iNearest,
            ( m_iNearest == -1 ? 0.0 : m_pNodes[m_iNearest].m_vecOriginPeek.x ),
            ( m_iNearest == -1 ? 0.0 : m_pNodes[m_iNearest].m_vecOriginPeek.y ),
            ( m_iNearest == -1 ? 0.0 : m_pNodes[m_iNearest].m_vecOriginPeek.z ) );
    }
    if( m_iNearest == -1 )
    {
        Logger->trace( "All that work for nothing." );
    }
#endif
    m_Cache[iHash].v = vecOrigin;
    m_Cache[iHash].n = m_iNearest;
    return m_iNearest;
}

//=========================================================
// CGraph - ShowNodeConnections - draws a line from the given node
// to all connected nodes
//=========================================================
void CGraph::ShowNodeConnections( int iNode )
{
    Vector vecSpot;
    CNode* pNode;
    CNode* pLinkNode;
    int i;

    if( 0 == m_fGraphPresent || 0 == m_fGraphPointersSet )
    { // protect us in the case that the node graph isn't available or built
        Logger->error( "Graph not ready!" );
        return;
    }

    if( iNode < 0 )
    {
        Logger->error( "Can't show connections for node {}", iNode );
        return;
    }

    pNode = &m_pNodes[iNode];

    UTIL_ParticleEffect( pNode->m_vecOrigin, g_vecZero, 255, 20 ); // show node position

    if( pNode->m_cNumLinks <= 0 )
    { // no connections!
        Logger->trace( "No Connections!" );
    }

    for( i = 0; i < pNode->m_cNumLinks; i++ )
    {

        pLinkNode = &Node( NodeLink( iNode, i ).m_iDestNode );
        vecSpot = pLinkNode->m_vecOrigin;

        MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
        WRITE_BYTE( TE_SHOWLINE );

        WRITE_COORD( m_pNodes[iNode].m_vecOrigin.x );
        WRITE_COORD( m_pNodes[iNode].m_vecOrigin.y );
        WRITE_COORD( m_pNodes[iNode].m_vecOrigin.z + NODE_HEIGHT );

        WRITE_COORD( vecSpot.x );
        WRITE_COORD( vecSpot.y );
        WRITE_COORD( vecSpot.z + NODE_HEIGHT );
        MESSAGE_END();
    }
}

//=========================================================
// CGraph - LinkVisibleNodes - the first, most basic
// function of node graph creation, this connects every
// node to every other node that it can see. Expects a
// pointer to an empty connection pool and a file pointer
// to write progress to. Returns the total number of initial
// links.
//
// If there's a problem with this process, the index
// of the offending node will be written to piBadNode
//=========================================================
int CGraph::LinkVisibleNodes( CLink* pLinkPool, FSFile& file, int* piBadNode )
{
    int i, j, z;
    edict_t* pTraceEnt;
    int cTotalLinks, cLinksThisNode, cMaxInitialLinks;
    TraceResult tr;

    // !!!BUGBUG - this function returns 0 if there is a problem in the middle of connecting the graph
    // it also returns 0 if none of the nodes in a level can see each other. piBadNode is ALWAYS read
    // by BuildNodeGraph() if this function returns a 0, so make sure that it doesn't get some random
    // number back.
    *piBadNode = 0;


    if( m_cNodes <= 0 )
    {
        Logger->error( "No Nodes!" );
        return 0;
    }

    // if the file pointer is bad, don't blow up, just don't write the
    // file.
    if( !file )
    {
        Logger->error( "LinkVisibleNodes: can't write to file." );
    }
    else
    {
        file.Printf( "----------------------------------------------------------------------------\n" );
        file.Printf( "LinkVisibleNodes - Initial Connections\n" );
        file.Printf( "----------------------------------------------------------------------------\n" );
    }

    cTotalLinks = 0; // start with no connections

    // to keep track of the maximum number of initial links any node had so far.
    // this lets us keep an eye on MAX_NODE_INITIAL_LINKS to ensure that we are
    // being generous enough.
    cMaxInitialLinks = 0;

    for( i = 0; i < m_cNodes; i++ )
    {
        cLinksThisNode = 0; // reset this count for each node.

        if( file )
        {
            file.Printf( "Node #%4d:\n\n", i );
        }

        for( z = 0; z < MAX_NODE_INITIAL_LINKS; z++ )
        {                                               // clear out the important fields in the link pool for this node
            pLinkPool[cTotalLinks + z].m_iSrcNode = i; // so each link knows which node it originates from
            pLinkPool[cTotalLinks + z].m_iDestNode = 0;
            pLinkPool[cTotalLinks + z].m_pLinkEnt = nullptr;
        }

        m_pNodes[i].m_iFirstLink = cTotalLinks;

        // now build a list of every other node that this node can see
        for( j = 0; j < m_cNodes; j++ )
        {
            if( j == i )
            { // don't connect to self!
                continue;
            }

#if 0

            if( ( m_pNodes[i].m_afNodeInfo & bits_NODE_WATER ) != ( m_pNodes[j].m_afNodeInfo & bits_NODE_WATER ) )
            {
                // don't connect water nodes to air nodes or land nodes. It just wouldn't be prudent at this juncture.
                continue;
            }
#else
            if( ( m_pNodes[i].m_afNodeInfo & bits_NODE_GROUP_REALM ) != ( m_pNodes[j].m_afNodeInfo & bits_NODE_GROUP_REALM ) )
            {
                // don't connect air nodes to water nodes to land nodes. It just wouldn't be prudent at this juncture.
                continue;
            }
#endif

            tr.pHit = nullptr; // clear every time so we don't get stuck with last trace's hit ent
            pTraceEnt = nullptr;

            UTIL_TraceLine( m_pNodes[i].m_vecOrigin,
                m_pNodes[j].m_vecOrigin,
                ignore_monsters,
                g_pBodyQueueHead->edict(), //!!!HACKHACK no real ent to supply here, using a global we don't care about
                &tr );


            if( 0 != tr.fStartSolid )
                continue;

            if( tr.flFraction != 1.0 )
            { // trace hit a brush ent, trace backwards to make sure that this ent is the only thing in the way.

                pTraceEnt = tr.pHit; // store the ent that the trace hit, for comparison

                UTIL_TraceLine( m_pNodes[j].m_vecOrigin,
                    m_pNodes[i].m_vecOrigin,
                    ignore_monsters,
                    g_pBodyQueueHead->edict(), //!!!HACKHACK no real ent to supply here, using a global we don't care about
                    &tr );

                auto hitEnt = CBaseEntity::Instance( pTraceEnt );

                // there is a solid_bsp ent in the way of these two nodes, so we must record several things about in order to keep
                // track of it in the pathfinding code, as well as through save and restore of the node graph. ANY data that is manipulated
                // as part of the process of adding a LINKENT to a connection here must also be done in CGraph::SetGraphPointers, where reloaded
                // graphs are prepared for use.
                if( tr.pHit == pTraceEnt && !hitEnt->ClassnameIs( "worldspawn" ) )
                {
                    // get a pointer
                    pLinkPool[cTotalLinks].m_pLinkEnt = VARS( tr.pHit );

                    // record the modelname, so that we can save/load node trees
                    memcpy( pLinkPool[cTotalLinks].m_szLinkEntModelname, STRING( VARS( tr.pHit )->model ), 4 );

                    // set the flag for this ent that indicates that it is attached to the world graph
                    // if this ent is removed from the world, it must also be removed from the connections
                    // that it formerly blocked.
                    if( !FBitSet( VARS( tr.pHit )->flags, FL_GRAPHED ) )
                    {
                        VARS( tr.pHit )->flags += FL_GRAPHED;
                    }
                }
                else
                { // even if the ent wasn't there, these nodes couldn't be connected. Skip.
                    continue;
                }
            }

            if( file )
            {
                file.Printf( "%4d", j );

                if( !FNullEnt( pLinkPool[cTotalLinks].m_pLinkEnt ) )
                { // record info about the ent in the way, if any.
                    file.Printf( "  Entity on connection: %s, name: %s  Model: %s", STRING( VARS( pTraceEnt )->classname ), STRING( VARS( pTraceEnt )->targetname ), STRING( VARS( tr.pHit )->model ) );
                }

                file.Printf( "\n" );
            }

            pLinkPool[cTotalLinks].m_iDestNode = j;
            cLinksThisNode++;
            cTotalLinks++;

            // If we hit this, either a level designer is placing too many nodes in the same area, or
            // we need to allow for a larger initial link pool.
            if( cLinksThisNode == MAX_NODE_INITIAL_LINKS )
            {
                Logger->warn( "LinkVisibleNodes: Node {} has NodeLinks > MAX_NODE_INITIAL_LINKS", i );
                file.Printf( "** NODE %d HAS NodeLinks > MAX_NODE_INITIAL_LINKS **\n", i );
                *piBadNode = i;
                return 0;
            }
            else if( cTotalLinks > MAX_NODE_INITIAL_LINKS * m_cNodes )
            { // this is paranoia
                Logger->warn( "LinkVisibleNodes: TotalLinks > MAX_NODE_INITIAL_LINKS * NUMNODES" );
                *piBadNode = i;
                return 0;
            }

            if( cLinksThisNode == 0 )
            {
                file.Printf( "**NO INITIAL LINKS**\n" );
            }

            // record the connection info in the link pool
            m_pNodes[i].m_cNumLinks = cLinksThisNode;

            // keep track of the most initial links ANY node had, so we can figure out
            // if we have a large enough default link pool
            if( cLinksThisNode > cMaxInitialLinks )
            {
                cMaxInitialLinks = cLinksThisNode;
            }
        }


        if( file )
        {
            file.Printf( "----------------------------------------------------------------------------\n" );
        }
    }

    file.Printf( "\n%4d Total Initial Connections - %4d Maximum connections for a single node.\n", cTotalLinks, cMaxInitialLinks );
    file.Printf( "----------------------------------------------------------------------------\n\n\n" );

    return cTotalLinks;
}

//=========================================================
// CGraph - RejectInlineLinks - expects a pointer to a link
// pool, and a pointer to and already-open file ( if you
// want status reports written to disk ). RETURNS the number
// of connections that were rejected
//=========================================================
int CGraph::RejectInlineLinks( CLink* pLinkPool, FSFile& file )
{
    int i, j, k;

    int cRejectedLinks;

    bool fRestartLoop; // have to restart the J loop if we eliminate a link.

    CNode* pSrcNode;
    CNode* pCheckNode; // the node we are testing for(one of pSrcNode's connections)
    CNode* pTestNode;  // the node we are checking against ( also one of pSrcNode's connections)

    float flDistToTestNode, flDistToCheckNode;

    Vector2D vec2DirToTestNode, vec2DirToCheckNode;

    if( file )
    {
        file.Printf( "----------------------------------------------------------------------------\n" );
        file.Printf( "InLine Rejection:\n" );
        file.Printf( "----------------------------------------------------------------------------\n" );
    }

    cRejectedLinks = 0;

    for( i = 0; i < m_cNodes; i++ )
    {
        pSrcNode = &m_pNodes[i];

        if( file )
        {
            file.Printf( "Node %3d:\n", i );
        }

        for( j = 0; j < pSrcNode->m_cNumLinks; j++ )
        {
            pCheckNode = &m_pNodes[pLinkPool[pSrcNode->m_iFirstLink + j].m_iDestNode];

            vec2DirToCheckNode = ( pCheckNode->m_vecOrigin - pSrcNode->m_vecOrigin ).Make2D();
            flDistToCheckNode = vec2DirToCheckNode.Length();
            vec2DirToCheckNode = vec2DirToCheckNode.Normalize();

            pLinkPool[pSrcNode->m_iFirstLink + j].m_flWeight = flDistToCheckNode;

            fRestartLoop = false;
            for( k = 0; k < pSrcNode->m_cNumLinks && !fRestartLoop; k++ )
            {
                if( k == j )
                { // don't check against same node
                    continue;
                }

                pTestNode = &m_pNodes[pLinkPool[pSrcNode->m_iFirstLink + k].m_iDestNode];

                vec2DirToTestNode = ( pTestNode->m_vecOrigin - pSrcNode->m_vecOrigin ).Make2D();

                flDistToTestNode = vec2DirToTestNode.Length();
                vec2DirToTestNode = vec2DirToTestNode.Normalize();

                if( DotProduct( vec2DirToCheckNode, vec2DirToTestNode ) >= 0.998 )
                {
                    // there's a chance that TestNode intersects the line to CheckNode. If so, we should disconnect the link to CheckNode.
                    if( flDistToTestNode < flDistToCheckNode )
                    {
                        if( file )
                        {
                            file.Printf( "REJECTED NODE %3d through Node %3d, Dot = %8f\n", pLinkPool[pSrcNode->m_iFirstLink + j].m_iDestNode, pLinkPool[pSrcNode->m_iFirstLink + k].m_iDestNode, DotProduct( vec2DirToCheckNode, vec2DirToTestNode ) );
                        }

                        pLinkPool[pSrcNode->m_iFirstLink + j] = pLinkPool[pSrcNode->m_iFirstLink + ( pSrcNode->m_cNumLinks - 1 )];
                        pSrcNode->m_cNumLinks--;
                        j--;

                        cRejectedLinks++; // keeping track of how many links are cut, so that we can return that value.

                        fRestartLoop = true;
                    }
                }
            }
        }

        if( file )
        {
            file.Printf( "----------------------------------------------------------------------------\n\n" );
        }
    }

    return cRejectedLinks;
}

//=========================================================
// TestHull is a modelless clip hull that verifies reachable
// nodes by walking from every node to each of it's connections
//=========================================================
class CTestHull : public CBaseMonster
{
    DECLARE_CLASS( CTestHull, CBaseMonster );
    DECLARE_DATAMAP();

public:
    void Spawn( CBaseEntity* masterNode );
    int ObjectCaps() override { return CBaseMonster::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
    bool IsMonster() override { return false; }
    void CallBuildNodeGraph();
    void BuildNodeGraph();
    void ShowBadNode();
    void DropDelay();
    void PathFind();

    Vector vecBadNodeOrigin;
};

BEGIN_DATAMAP( CTestHull )
    DEFINE_FUNCTION( CallBuildNodeGraph ),
    DEFINE_FUNCTION( ShowBadNode ),
    DEFINE_FUNCTION( DropDelay ),
    DEFINE_FUNCTION( PathFind ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( testhull, CTestHull );

//=========================================================
// CTestHull::Spawn
//=========================================================
void CTestHull::Spawn( CBaseEntity* masterNode )
{
    SetModel( "models/player.mdl" );
    SetSize( VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );

    pev->solid = SOLID_SLIDEBOX;
    pev->movetype = MOVETYPE_STEP;
    pev->effects = 0;
    pev->health = 50;
    pev->yaw_speed = 8;

    if( 0 != WorldGraph.m_fGraphPresent )
    { // graph loaded from disk, so we don't need the test hull
        SetThink( &CTestHull::SUB_Remove );
        pev->nextthink = gpGlobals->time;
    }
    else
    {
        SetThink( &CTestHull::DropDelay );
        pev->nextthink = gpGlobals->time + 1;
    }

    // Make this invisible
    // UNDONE: Shouldn't we just use EF_NODRAW?  This doesn't need to go to the client.
    pev->rendermode = kRenderTransTexture;
    pev->renderamt = 0;
}

//=========================================================
// TestHull::DropDelay - spawns TestHull on top of
// the 0th node and drops it to the ground.
//=========================================================
void CTestHull::DropDelay()
{
    //    UTIL_CenterPrintAll( "Node Graph out of Date. Rebuilding..." );

    SetOrigin( WorldGraph.m_pNodes[0].m_vecOrigin );

    SetThink( &CTestHull::CallBuildNodeGraph );

    pev->nextthink = gpGlobals->time + 1;
}

//=========================================================
// nodes start out as ents in the world. As they are spawned,
// the node info is recorded then the ents are discarded.
//=========================================================
bool CNodeEnt::KeyValue( KeyValueData* pkvd )
{
    if( FStrEq( pkvd->szKeyName, "hinttype" ) )
    {
        m_sHintType = (short)atoi( pkvd->szValue );
        return true;
    }

    if( FStrEq( pkvd->szKeyName, "activity" ) )
    {
        m_sHintActivity = (short)atoi( pkvd->szValue );
        return true;
    }

    return CBaseEntity::KeyValue( pkvd );
}

//=========================================================
//=========================================================
SpawnAction CNodeEnt::Spawn()
{
    pev->movetype = MOVETYPE_NONE;
    pev->solid = SOLID_NOT; // always solid_not

    // graph loaded from disk, so discard all these node ents as soon as they spawn
    if( 0 != WorldGraph.m_fGraphPresent )
    {
        return SpawnAction::RemoveNow;
    }

    if( WorldGraph.m_cNodes == 0 )
    { // this is the first node to spawn, spawn the test hull entity that builds and walks the node tree
        CTestHull* pHull = g_EntityDictionary->Create<CTestHull>( "testhull" );
        pHull->Spawn( this );
    }

    if( WorldGraph.m_cNodes >= MAX_NODES )
    {
        CGraph::Logger->error( "cNodes >= MAX_NODES" );
        return SpawnAction::RemoveNow;
    }

    WorldGraph.m_pNodes[WorldGraph.m_cNodes].m_vecOriginPeek =
    WorldGraph.m_pNodes[WorldGraph.m_cNodes].m_vecOrigin = pev->origin;
    WorldGraph.m_pNodes[WorldGraph.m_cNodes].m_flHintYaw = pev->angles.y;
    WorldGraph.m_pNodes[WorldGraph.m_cNodes].m_sHintType = m_sHintType;
    WorldGraph.m_pNodes[WorldGraph.m_cNodes].m_sHintActivity = m_sHintActivity;
    WorldGraph.m_pNodes[WorldGraph.m_cNodes].m_afNodeInfo = ( ClassnameIs( "info_node_air" ) ? bits_NODE_AIR : 0 );
    WorldGraph.m_cNodes++;

    return SpawnAction::RemoveNow; // Should we really be aware if this failed?
}

//=========================================================
// CTestHull - ShowBadNode - makes a bad node fizzle. When
// there's a problem with node graph generation, the test
// hull will be placed up the bad node's location and will generate
// particles
//=========================================================
void CTestHull::ShowBadNode()
{
    pev->movetype = MOVETYPE_FLY;
    pev->angles.y = pev->angles.y + 4;

    UTIL_MakeVectors( pev->angles );

    UTIL_ParticleEffect( pev->origin, g_vecZero, 255, 25 );
    UTIL_ParticleEffect( pev->origin + gpGlobals->v_forward * 64, g_vecZero, 255, 25 );
    UTIL_ParticleEffect( pev->origin - gpGlobals->v_forward * 64, g_vecZero, 255, 25 );
    UTIL_ParticleEffect( pev->origin + gpGlobals->v_right * 64, g_vecZero, 255, 25 );
    UTIL_ParticleEffect( pev->origin - gpGlobals->v_right * 64, g_vecZero, 255, 25 );

    pev->nextthink = gpGlobals->time + 0.1;
}

void CTestHull::CallBuildNodeGraph()
{
    // TOUCH HACK -- Don't allow this entity to call anyone's "touch" function
    gTouchDisabled = true;
    BuildNodeGraph();
    gTouchDisabled = false;
    // Undo TOUCH HACK
}

//=========================================================
// BuildNodeGraph - think function called by the empty walk
// hull that is spawned by the first node to spawn. This
// function links all nodes that can see each other, then
// eliminates all inline links, then uses a monster-sized
// hull that walks between each node and each of its links
// to ensure that a monster can actually fit through the space
//=========================================================
void CTestHull::BuildNodeGraph()
{
    CLink* pTempPool; // temporary link pool

    CNode* pSrcNode;  // node we're currently working with
    CNode* pDestNode; // the other node in comparison operations

    bool fSkipRemainingHulls; // if smallest hull can't fit, don't check any others
    bool fPairsValid;          // are all links in the graph evenly paired?

    int i, j, hull;

    int iBadNode; // this is the node that caused graph generation to fail

    int cPoolLinks; // number of links in the pool.

    Vector vecSpot;

    float flYaw; // use this stuff to walk the hull between nodes
    float flDist;
    int step;

    SetThink( &CTestHull::SUB_Remove ); // no matter what happens, the hull gets rid of itself.
    pev->nextthink = gpGlobals->time;

    //     malloc a swollen temporary connection pool that we trim down after we know exactly how many connections there are.
    pTempPool = ( CLink* )calloc( sizeof( CLink ), ( WorldGraph.m_cNodes * MAX_NODE_INITIAL_LINKS ) );
    if( !pTempPool )
    {
        CGraph::Logger->error( "Could not malloc TempPool!" );
        return;
    }

    // make sure directories have been made
    g_pFileSystem->CreateDirHierarchy( "maps/graphs", "GAMECONFIG" );

    const std::string nrpFileName{std::string{"maps/graphs/"} + STRING( gpGlobals->mapname ) + ".nrp"};

    FSFile file{nrpFileName.c_str(), "w+", "GAMECONFIG"};

    if( !file )
    { // file error
        CGraph::Logger->error( "Couldn't create {}!", nrpFileName.c_str() );

        if( pTempPool )
        {
            free( pTempPool );
        }

        return;
    }

    file.Printf( "Node Graph Report for map:  %s.bsp\n", STRING( gpGlobals->mapname ) );
    file.Printf( "%d Total Nodes\n\n", WorldGraph.m_cNodes );

    for( i = 0; i < WorldGraph.m_cNodes; i++ )
    { // print all node numbers and their locations to the file.
        WorldGraph.m_pNodes[i].m_cNumLinks = 0;
        WorldGraph.m_pNodes[i].m_iFirstLink = 0;
        memset( WorldGraph.m_pNodes[i].m_pNextBestNode, 0, sizeof( WorldGraph.m_pNodes[i].m_pNextBestNode ) );

        file.Printf( "Node#         %4d\n", i );
        file.Printf( "Location      %4d,%4d,%4d\n", (int)WorldGraph.m_pNodes[i].m_vecOrigin.x, (int)WorldGraph.m_pNodes[i].m_vecOrigin.y, (int)WorldGraph.m_pNodes[i].m_vecOrigin.z );
        file.Printf( "HintType:     %4d\n", WorldGraph.m_pNodes[i].m_sHintType );
        file.Printf( "HintActivity: %4d\n", WorldGraph.m_pNodes[i].m_sHintActivity );
        file.Printf( "HintYaw:      %4f\n", WorldGraph.m_pNodes[i].m_flHintYaw );
        file.Printf( "-------------------------------------------------------------------------------\n" );
    }
    file.Printf( "\n\n" );


    // Automatically recognize WATER nodes and drop the LAND nodes to the floor.
    //
    for( i = 0; i < WorldGraph.m_cNodes; i++ )
    {
        if( ( WorldGraph.m_pNodes[i].m_afNodeInfo & bits_NODE_AIR ) != 0 )
        {
            // do nothing
        }
        else if( UTIL_PointContents( WorldGraph.m_pNodes[i].m_vecOrigin ) == CONTENTS_WATER )
        {
            WorldGraph.m_pNodes[i].m_afNodeInfo |= bits_NODE_WATER;
        }
        else
        {
            WorldGraph.m_pNodes[i].m_afNodeInfo |= bits_NODE_LAND;

            // trace to the ground, then pop up 8 units and place node there to make it
            // easier for them to connect (think stairs, chairs, and bumps in the floor).
            // After the routing is done, push them back down.
            //
            TraceResult tr;

            UTIL_TraceLine( WorldGraph.m_pNodes[i].m_vecOrigin,
                WorldGraph.m_pNodes[i].m_vecOrigin - Vector( 0, 0, 384 ),
                ignore_monsters,
                g_pBodyQueueHead->edict(), //!!!HACKHACK no real ent to supply here, using a global we don't care about
                &tr );

            // This trace is ONLY used if we hit an entity flagged with FL_WORLDBRUSH
            TraceResult trEnt;
            UTIL_TraceLine( WorldGraph.m_pNodes[i].m_vecOrigin,
                WorldGraph.m_pNodes[i].m_vecOrigin - Vector( 0, 0, 384 ),
                dont_ignore_monsters,
                g_pBodyQueueHead->edict(), //!!!HACKHACK no real ent to supply here, using a global we don't care about
                &trEnt );


            // Did we hit something closer than the floor?
            if( trEnt.flFraction < tr.flFraction )
            {
                // If it was a world brush entity, copy the node location
                if( trEnt.pHit && ( trEnt.pHit->v.flags & FL_WORLDBRUSH ) != 0 )
                    tr.vecEndPos = trEnt.vecEndPos;
            }

            WorldGraph.m_pNodes[i].m_vecOriginPeek.z =
                WorldGraph.m_pNodes[i].m_vecOrigin.z = tr.vecEndPos.z + NODE_HEIGHT;
        }
    }

    cPoolLinks = WorldGraph.LinkVisibleNodes( pTempPool, file, &iBadNode );

    if( 0 == cPoolLinks )
    {
        CGraph::Logger->error( "ConnectVisibleNodes FAILED!" );

        SetThink( &CTestHull::ShowBadNode ); // send the hull off to show the offending node.
        // pev->solid = SOLID_NOT;
        pev->origin = WorldGraph.m_pNodes[iBadNode].m_vecOrigin;

        if( pTempPool )
        {
            free( pTempPool );
        }

        return;
    }

    // send the walkhull to all of this node's connections now. We'll do this here since
    // so much of it relies on being able to control the test hull.
    file.Printf( "----------------------------------------------------------------------------\n" );
    file.Printf( "Walk Rejection:\n" );

    for( i = 0; i < WorldGraph.m_cNodes; i++ )
    {
        pSrcNode = &WorldGraph.m_pNodes[i];

        file.Printf( "-------------------------------------------------------------------------------\n" );
        file.Printf( "Node %4d:\n\n", i );

        for( j = 0; j < pSrcNode->m_cNumLinks; j++ )
        {
            // assume that all hulls can walk this link, then eliminate the ones that can't.
            pTempPool[pSrcNode->m_iFirstLink + j].m_afLinkInfo = bits_LINK_SMALL_HULL | bits_LINK_HUMAN_HULL | bits_LINK_LARGE_HULL | bits_LINK_FLY_HULL;


            // do a check for each hull size.

            // if we can't fit a tiny hull through a connection, no other hulls with fit either, so we
            // should just fall out of the loop. Do so by setting the SkipRemainingHulls flag.
            fSkipRemainingHulls = false;
            for( hull = 0; hull < MAX_NODE_HULLS; hull++ )
            {
                if( fSkipRemainingHulls && ( hull == NODE_HUMAN_HULL || hull == NODE_LARGE_HULL ) ) // skip the remaining walk hulls
                    continue;

                switch ( hull )
                {
                case NODE_SMALL_HULL:
                    SetSize( Vector( -12, -12, 0 ), Vector( 12, 12, 24 ) );
                    break;
                case NODE_HUMAN_HULL:
                    SetSize( VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );
                    break;
                case NODE_LARGE_HULL:
                    SetSize( Vector( -32, -32, 0 ), Vector( 32, 32, 64 ) );
                    break;
                case NODE_FLY_HULL:
                    SetSize( Vector( -32, -32, 0 ), Vector( 32, 32, 64 ) );
                    // SetSize(Vector(0, 0, 0), Vector(0, 0, 0));
                    break;
                }

                SetOrigin( pSrcNode->m_vecOrigin ); // place the hull on the node

                if( !FBitSet( pev->flags, FL_ONGROUND ) )
                {
                    CGraph::Logger->trace( "OFFGROUND!" );
                }

                // now build a yaw that points to the dest node, and get the distance.
                if( j < 0 )
                {
                    CGraph::Logger->trace( "**** j = {} ****", j );
                    if( pTempPool )
                    {
                        free( pTempPool );
                    }

                    return;
                }

                pDestNode = &WorldGraph.m_pNodes[pTempPool[pSrcNode->m_iFirstLink + j].m_iDestNode];

                vecSpot = pDestNode->m_vecOrigin;
                // vecSpot.z = pev->origin.z;

                if( hull < NODE_FLY_HULL )
                {
                    int SaveFlags = pev->flags;
                    int MoveMode = WALKMOVE_WORLDONLY;
                    if( ( pSrcNode->m_afNodeInfo & bits_NODE_WATER ) != 0 )
                    {
                        pev->flags |= FL_SWIM;
                        MoveMode = WALKMOVE_NORMAL;
                    }

                    flYaw = VectorToYaw( pDestNode->m_vecOrigin - pev->origin );

                    flDist = ( vecSpot - pev->origin ).Length2D();

                    bool fWalkFailed = false;

                    // in this loop we take tiny steps from the current node to the nodes that it links to, one at a time.
                    // pev->angles.y = flYaw;
                    for( step = 0; step < flDist && !fWalkFailed; step += HULL_STEP_SIZE )
                    {
                        float stepSize = HULL_STEP_SIZE;

                        if( ( step + stepSize ) >= ( flDist - 1 ) )
                            stepSize = ( flDist - step ) - 1;

                        if( !WALK_MOVE( edict(), flYaw, stepSize, MoveMode ) )
                        { // can't take the next step

                            fWalkFailed = true;
                            break;
                        }
                    }

                    if( !fWalkFailed && ( pev->origin - vecSpot ).Length() > 64 )
                    {
                        // Logger->info("bogus walk");
                        // we thought we
                        fWalkFailed = true;
                    }

                    if( fWalkFailed )
                    {

                        // pTempPool[ pSrcNode->m_iFirstLink + j ] = pTempPool [ pSrcNode->m_iFirstLink + ( pSrcNode->m_cNumLinks - 1 ) ];

                        // now me must eliminate the hull that couldn't walk this connection
                        switch ( hull )
                        {
                        case NODE_SMALL_HULL: // if this hull can't fit, nothing can, so drop the connection
                            file.Printf( "NODE_SMALL_HULL step %d\n", step );
                            pTempPool[pSrcNode->m_iFirstLink + j].m_afLinkInfo &= ~( bits_LINK_SMALL_HULL | bits_LINK_HUMAN_HULL | bits_LINK_LARGE_HULL );
                            fSkipRemainingHulls = true; // don't bother checking larger hulls
                            break;
                        case NODE_HUMAN_HULL:
                            file.Printf( "NODE_HUMAN_HULL step %d\n", step );
                            pTempPool[pSrcNode->m_iFirstLink + j].m_afLinkInfo &= ~( bits_LINK_HUMAN_HULL | bits_LINK_LARGE_HULL );
                            fSkipRemainingHulls = true; // don't bother checking larger hulls
                            break;
                        case NODE_LARGE_HULL:
                            file.Printf( "NODE_LARGE_HULL step %d\n", step );
                            pTempPool[pSrcNode->m_iFirstLink + j].m_afLinkInfo &= ~bits_LINK_LARGE_HULL;
                            break;
                        }
                    }
                    pev->flags = SaveFlags;
                }
                else
                {
                    TraceResult tr;

                    UTIL_TraceHull( pSrcNode->m_vecOrigin + Vector( 0, 0, 32 ), pDestNode->m_vecOriginPeek + Vector( 0, 0, 32 ), ignore_monsters, large_hull, edict(), &tr );
                    if( 0 != tr.fStartSolid || tr.flFraction < 1.0 )
                    {
                        pTempPool[pSrcNode->m_iFirstLink + j].m_afLinkInfo &= ~bits_LINK_FLY_HULL;
                    }
                }
            }

            if( pTempPool[pSrcNode->m_iFirstLink + j].m_afLinkInfo == 0 )
            {
                file.Printf( "Rejected Node %3d - Unreachable by ", pTempPool[pSrcNode->m_iFirstLink + j].m_iDestNode );
                pTempPool[pSrcNode->m_iFirstLink + j] = pTempPool[pSrcNode->m_iFirstLink + ( pSrcNode->m_cNumLinks - 1 )];
                file.Printf( "Any Hull\n" );

                pSrcNode->m_cNumLinks--;
                cPoolLinks--; // we just removed a link, so decrement the total number of links in the pool.
                j--;
            }
        }
    }
    file.Printf( "-------------------------------------------------------------------------------\n\n\n" );

    cPoolLinks -= WorldGraph.RejectInlineLinks( pTempPool, file );

    // now malloc a pool just large enough to hold the links that are actually used
    WorldGraph.m_pLinkPool = ( CLink* )calloc( sizeof( CLink ), cPoolLinks );

    if( !WorldGraph.m_pLinkPool )
    { // couldn't make the link pool!
        CGraph::Logger->error( "Couldn't malloc LinkPool!" );
        if( pTempPool )
        {
            free( pTempPool );
        }

        return;
    }
    WorldGraph.m_cLinks = cPoolLinks;

    // copy only the used portions of the TempPool into the graph's link pool
    int iFinalPoolIndex = 0;
    int iOldFirstLink;

    for( i = 0; i < WorldGraph.m_cNodes; i++ )
    {
        iOldFirstLink = WorldGraph.m_pNodes[i].m_iFirstLink; // store this, because we have to re-assign it before entering the copy loop

        WorldGraph.m_pNodes[i].m_iFirstLink = iFinalPoolIndex;

        for( j = 0; j < WorldGraph.m_pNodes[i].m_cNumLinks; j++ )
        {
            WorldGraph.m_pLinkPool[iFinalPoolIndex++] = pTempPool[iOldFirstLink + j];
        }
    }


    // Node sorting numbers linked nodes close to each other
    //
    WorldGraph.SortNodes();

    // This is used for HashSearch
    //
    WorldGraph.BuildLinkLookups();

    fPairsValid = true; // assume that the connection pairs are all valid to start

    file.Printf( "\n\n-------------------------------------------------------------------------------\n" );
    file.Printf( "Link Pairings:\n" );

    // link integrity check. The idea here is that if Node A links to Node B, node B should
    // link to node A. If not, we have a situation that prevents us from using a basic
    // optimization in the FindNearestLink function.
    for( i = 0; i < WorldGraph.m_cNodes; i++ )
    {
        for( j = 0; j < WorldGraph.m_pNodes[i].m_cNumLinks; j++ )
        {
            int iLink;
            WorldGraph.HashSearch( WorldGraph.INodeLink( i, j ), i, iLink );
            if( iLink < 0 )
            {
                fPairsValid = false; // unmatched link pair.
                file.Printf( "WARNING: Node %3d does not connect back to Node %3d\n", WorldGraph.INodeLink( i, j ), i );
            }
        }
    }

    // !!!LATER - if all connections are properly paired, when can enable an optimization in the pathfinding code
    // (in the find nearest line function)
    if( fPairsValid )
    {
        file.Printf( "\nAll Connections are Paired!\n" );
    }

    file.Printf( "-------------------------------------------------------------------------------\n" );
    file.Printf( "\n\n-------------------------------------------------------------------------------\n" );
    file.Printf( "Total Number of Connections in Pool: %d\n", cPoolLinks );
    file.Printf( "-------------------------------------------------------------------------------\n" );
    file.Printf( "Connection Pool: %d bytes\n", sizeof( CLink ) * cPoolLinks );
    file.Printf( "-------------------------------------------------------------------------------\n" );


    CGraph::Logger->trace( "{} Nodes, {} Connections", WorldGraph.m_cNodes, cPoolLinks );

    // This is used for FindNearestNode
    //
    WorldGraph.BuildRegionTables();


    // Push all of the LAND nodes down to the ground now. Leave the water and air nodes alone.
    //
    for( i = 0; i < WorldGraph.m_cNodes; i++ )
    {
        if( ( WorldGraph.m_pNodes[i].m_afNodeInfo & bits_NODE_LAND ) != 0 )
        {
            WorldGraph.m_pNodes[i].m_vecOrigin.z -= NODE_HEIGHT;
        }
    }


    if( pTempPool )
    { // free the temp pool
        free( pTempPool );
    }

    file.Close();

    // We now have some graphing capabilities.
    //
    WorldGraph.m_fGraphPresent = 1;        // graph is in memory.
    WorldGraph.m_fGraphPointersSet = 1; // since the graph was generated, the pointers are ready
    WorldGraph.m_fRoutingComplete = 0;    // Optimal routes aren't computed, yet.

    // Compute and compress the routing information.
    //
    WorldGraph.ComputeStaticRoutingTables();

    // save the node graph for this level
    WorldGraph.FSaveGraph( STRING( gpGlobals->mapname ) );
    CGraph::Logger->info( "Done." );
}


//=========================================================
// returns a hardcoded path.
//=========================================================
void CTestHull::PathFind()
{
    int iPath[50];
    int iPathSize;
    int i;
    CNode *pNode, *pNextNode;

    if( 0 == WorldGraph.m_fGraphPresent || 0 == WorldGraph.m_fGraphPointersSet )
    { // protect us in the case that the node graph isn't available
        CGraph::Logger->error( "Graph not ready!" );
        return;
    }

    iPathSize = WorldGraph.FindShortestPath( iPath, 0, 19, 0, 0 ); // UNDONE use hull constant

    if( 0 == iPathSize )
    {
        CGraph::Logger->error( "No Path!" );
        return;
    }

    CGraph::Logger->trace( "{}", iPathSize );

    pNode = &WorldGraph.m_pNodes[iPath[0]];

    for( i = 0; i < iPathSize - 1; i++ )
    {

        pNextNode = &WorldGraph.m_pNodes[iPath[i + 1]];

        MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
        WRITE_BYTE( TE_SHOWLINE );

        WRITE_COORD( pNode->m_vecOrigin.x );
        WRITE_COORD( pNode->m_vecOrigin.y );
        WRITE_COORD( pNode->m_vecOrigin.z + NODE_HEIGHT );

        WRITE_COORD( pNextNode->m_vecOrigin.x );
        WRITE_COORD( pNextNode->m_vecOrigin.y );
        WRITE_COORD( pNextNode->m_vecOrigin.z + NODE_HEIGHT );
        MESSAGE_END();

        pNode = pNextNode;
    }
}


//=========================================================
// CStack Constructor
//=========================================================
CStack::CStack()
{
    m_level = 0;
}

//=========================================================
// pushes a value onto the stack
//=========================================================
void CStack::Push( int value )
{
    if( m_level >= MAX_STACK_NODES )
    {
        printf( "Error!\n" );
        return;
    }
    m_stack[m_level] = value;
    m_level++;
}

//=========================================================
// pops a value off of the stack
//=========================================================
int CStack::Pop()
{
    if( m_level <= 0 )
        return -1;

    m_level--;
    return m_stack[m_level];
}

//=========================================================
// returns the value on the top of the stack
//=========================================================
int CStack::Top()
{
    return m_stack[m_level - 1];
}

//=========================================================
// copies every element on the stack into an array LIFO
//=========================================================
void CStack::CopyToArray( int* piArray )
{
    int i;

    for( i = 0; i < m_level; i++ )
    {
        piArray[i] = m_stack[i];
    }
}

//=========================================================
// CQueue constructor
//=========================================================
CQueue::CQueue()
{
    m_cSize = 0;
    m_head = 0;
    m_tail = -1;
}

//=========================================================
// inserts a value into the queue
//=========================================================
void CQueue::Insert( int iValue, float fPriority )
{

    if( Full() )
    {
        printf( "Queue is full!\n" );
        return;
    }

    m_tail++;

    if( m_tail == MAX_STACK_NODES )
    { // wrap around
        m_tail = 0;
    }

    m_queue[m_tail].Id = iValue;
    m_queue[m_tail].Priority = fPriority;
    m_cSize++;
}

//=========================================================
// removes a value from the queue (FIFO)
//=========================================================
int CQueue::Remove( float& fPriority )
{
    if( m_head == MAX_STACK_NODES )
    { // wrap
        m_head = 0;
    }

    m_cSize--;
    fPriority = m_queue[m_head].Priority;
    return m_queue[m_head++].Id;
}

//=========================================================
// CQueue constructor
//=========================================================
CQueuePriority::CQueuePriority()
{
    m_cSize = 0;
}

//=========================================================
// inserts a value into the priority queue
//=========================================================
void CQueuePriority::Insert( int iValue, float fPriority )
{

    if( Full() )
    {
        printf( "Queue is full!\n" );
        return;
    }

    m_heap[m_cSize].Priority = fPriority;
    m_heap[m_cSize].Id = iValue;
    m_cSize++;
    Heap_SiftUp();
}

//=========================================================
// removes the smallest item from the priority queue
//
//=========================================================
int CQueuePriority::Remove( float& fPriority )
{
    int iReturn = m_heap[0].Id;
    fPriority = m_heap[0].Priority;

    m_cSize--;

    m_heap[0] = m_heap[m_cSize];

    Heap_SiftDown( 0 );
    return iReturn;
}

#define HEAP_LEFT_CHILD(x) (2 * (x) + 1)
#define HEAP_RIGHT_CHILD(x) (2 * (x) + 2)
#define HEAP_PARENT(x) (((x)-1) / 2)

void CQueuePriority::Heap_SiftDown( int iSubRoot )
{
    int parent = iSubRoot;
    int child = HEAP_LEFT_CHILD( parent );

    tag_HEAP_NODE Ref = m_heap[parent];

    while( child < m_cSize )
    {
        int rightchild = HEAP_RIGHT_CHILD( parent );
        if( rightchild < m_cSize )
        {
            if( m_heap[rightchild].Priority < m_heap[child].Priority )
            {
                child = rightchild;
            }
        }
        if( Ref.Priority <= m_heap[child].Priority )
            break;

        m_heap[parent] = m_heap[child];
        parent = child;
        child = HEAP_LEFT_CHILD( parent );
    }
    m_heap[parent] = Ref;
}

void CQueuePriority::Heap_SiftUp()
{
    int child = m_cSize - 1;
    while( 0 != child )
    {
        int parent = HEAP_PARENT( child );
        if( m_heap[parent].Priority <= m_heap[child].Priority )
            break;

        tag_HEAP_NODE Tmp;
        Tmp = m_heap[child];
        m_heap[child] = m_heap[parent];
        m_heap[parent] = Tmp;

        child = parent;
    }
}

//=========================================================
// CGraph - FLoadGraph - attempts to load a node graph from disk.
// if the current level is maps/snar.bsp, maps/graphs/snar.nod
// will be loaded. If file cannot be loaded, the node tree
// will be created and saved to disk.
//=========================================================
bool CGraph::FLoadGraph( const char* szMapName )
{
    // make sure the directories have been made
    g_pFileSystem->CreateDirHierarchy( "maps/graphs", "GAMECONFIG" );

    const std::string fileName{std::string{"maps/graphs/"} + szMapName + ".nod"};

    // Note: Allow loading graphs only from the mod directory itself.
    // Do not allow loading from other games since they may have a different graph format.
    const auto buffer = FileSystem_LoadFileIntoBuffer( fileName.c_str(), FileContentFormat::Binary, "GAMECONFIG" );

    if( buffer.empty() )
    {
        return false;
    }

    auto pMemFile = reinterpret_cast<const byte*>( buffer.data() );

    assert( buffer.size() <= static_cast<std::size_t>( std::numeric_limits<int>::max() ) );
    int length = static_cast<int>( buffer.size() );

    // Read the graph version number
    //
    length -= sizeof(int);
    if( length < 0 )
        return false;

    int iVersion;
    memcpy( &iVersion, pMemFile, sizeof(int) );
    pMemFile += sizeof(int);

    if( iVersion != GRAPH_VERSION )
    {
        // This file was written by a different build of the dll!
        //
        Logger->error( "Graph version is {}, expected {}", iVersion, GRAPH_VERSION );
        return false;
    }

    // Read the graph class
    //
    length -= sizeof( CGraph );
    if( length < 0 )
        return false;
    memcpy( this, pMemFile, sizeof( CGraph ) );
    pMemFile += sizeof( CGraph );

    // Set the pointers to zero, just in case we run out of memory.
    //
    m_pNodes = nullptr;
    m_pLinkPool = nullptr;
    m_di = nullptr;
    m_pRouteInfo = nullptr;
    m_pHashLinks = nullptr;


    // Malloc for the nodes
    //
    m_pNodes = ( CNode* )calloc( sizeof( CNode ), m_cNodes );

    if( !m_pNodes )
    {
        Logger->error( "Couldn't malloc {} nodes!", m_cNodes );
        return false;
    }

    // Read in all the nodes
    //
    length -= sizeof( CNode ) * m_cNodes;
    if( length < 0 )
        return false;
    memcpy( m_pNodes, pMemFile, sizeof( CNode ) * m_cNodes );
    pMemFile += sizeof( CNode ) * m_cNodes;


    // Malloc for the link pool
    //
    m_pLinkPool = ( CLink* )calloc( sizeof( CLink ), m_cLinks );

    if( !m_pLinkPool )
    {
        Logger->trace( "Couldn't malloc {} link!", m_cLinks );
        return false;
    }

    // Read in all the links
    //
    length -= sizeof( CLink ) * m_cLinks;
    if( length < 0 )
        return false;
    memcpy( m_pLinkPool, pMemFile, sizeof( CLink ) * m_cLinks );
    pMemFile += sizeof( CLink ) * m_cLinks;

    // Malloc for the sorting info.
    //
    m_di = ( DIST_INFO* )calloc( sizeof( DIST_INFO ), m_cNodes );
    if( !m_di )
    {
        Logger->trace( "Couldn't malloc {} entries sorting nodes!", m_cNodes );
        return false;
    }

    // Read it in.
    //
    length -= sizeof( DIST_INFO ) * m_cNodes;
    if( length < 0 )
        return false;
    memcpy( m_di, pMemFile, sizeof( DIST_INFO ) * m_cNodes );
    pMemFile += sizeof( DIST_INFO ) * m_cNodes;

    // Malloc for the routing info.
    //
    m_fRoutingComplete = 0;
    m_pRouteInfo = (char*)calloc( sizeof(char), m_nRouteInfo );
    if( !m_pRouteInfo )
    {
        Logger->trace( "Couldn't malloc {} route bytes!", m_nRouteInfo );
        return false;
    }
    m_CheckedCounter = 0;
    for( int i = 0; i < m_cNodes; i++ )
    {
        m_di[i].m_CheckedEvent = 0;
    }

    // Read in the route information.
    //
    length -= sizeof(char) * m_nRouteInfo;
    if( length < 0 )
        return false;
    memcpy( m_pRouteInfo, pMemFile, sizeof(char) * m_nRouteInfo );
    pMemFile += sizeof(char) * m_nRouteInfo;
    m_fRoutingComplete = 1;

    // malloc for the hash links
    //
    m_pHashLinks = (short*)calloc( sizeof(short), m_nHashLinks );
    if( !m_pHashLinks )
    {
        Logger->trace( "Couldn't malloc {} hash link bytes!", m_nHashLinks );
        return false;
    }

    // Read in the hash link information
    //
    length -= sizeof(short) * m_nHashLinks;
    if( length < 0 )
        return false;
    memcpy( m_pHashLinks, pMemFile, sizeof(short) * m_nHashLinks );
    pMemFile += sizeof(short) * m_nHashLinks;

    // Set the graph present flag, clear the pointers set flag
    //
    m_fGraphPresent = 1;
    m_fGraphPointersSet = 0;

    if( length != 0 )
    {
        Logger->warn( "Node graph was longer than expected by {} bytes!", length );
    }

    return true;
}

//=========================================================
// CGraph - FSaveGraph - It's not rocket science.
// this WILL overwrite existing files.
//=========================================================
bool CGraph::FSaveGraph( const char* szMapName )
{
    if( 0 == m_fGraphPresent || 0 == m_fGraphPointersSet )
    { // protect us in the case that the node graph isn't available or built
        Logger->error( "Graph not ready!" );
        return false;
    }

    // make sure directories have been made
    g_pFileSystem->CreateDirHierarchy( "maps/graphs", "GAMECONFIG" );

    const std::string fileName{std::string{"maps/graphs/"} + szMapName + ".nod"};

    FSFile file{fileName.c_str(), "wb", "GAMECONFIG"};

    Logger->debug( "Created: {}", fileName );

    if( !file )
    { // couldn't create
        Logger->trace( "Couldn't Create: {}", fileName );
        return false;
    }

    // write the version
    const int iVersion = GRAPH_VERSION;
    file.Write( &iVersion, sizeof(int) );

    // write the CGraph class
    file.Write( this, sizeof( CGraph ) );

    // write the nodes
    file.Write( m_pNodes, sizeof( CNode ) * m_cNodes );

    // write the links
    file.Write( m_pLinkPool, sizeof( CLink ) * m_cLinks );

    file.Write( m_di, sizeof( DIST_INFO ) * m_cNodes );

    // Write the route info.
    //
    if( m_pRouteInfo && 0 != m_nRouteInfo )
    {
        file.Write( m_pRouteInfo, sizeof(char) * m_nRouteInfo );
    }

    if( m_pHashLinks && 0 != m_nHashLinks )
    {
        file.Write( m_pHashLinks, sizeof(short) * m_nHashLinks );
    }
    return true;
}

//=========================================================
// CGraph - FSetGraphPointers - Takes the modelnames of
// all of the brush ents that block connections in the node
// graph and resolves them into pointers to those entities.
// this is done after loading the graph from disk, whereupon
// the pointers are not valid.
//=========================================================
bool CGraph::FSetGraphPointers()
{
    CBaseEntity* linkEnt;

    for( int i = 0; i < m_cLinks; i++ )
    { // go through all of the links

        if( m_pLinkPool[i].m_pLinkEnt != nullptr )
        {
            char name[5];
            // when graphs are saved, any valid pointers are will be non-zero, signifying that we should
            // reset those pointers upon reloading. Any pointers that were nullptr when the graph was saved
            // will be nullptr when reloaded, and will ignored by this function.

            // m_szLinkEntModelname is not necessarily nullptr terminated (so we can store it in a more alignment-friendly 4 bytes)
            memcpy( name, m_pLinkPool[i].m_szLinkEntModelname, 4 );
            name[4] = 0;
            linkEnt = UTIL_FindEntityByString( nullptr, "model", name );

            if( FNullEnt( linkEnt ) )
            {
                // the ent isn't around anymore? Either there is a major problem, or it was removed from the world
                // ( like a func_breakable that's been destroyed or something ). Make sure that LinkEnt is null.
                Logger->debug( "Could not find model {}", name );
                m_pLinkPool[i].m_pLinkEnt = nullptr;
            }
            else
            {
                m_pLinkPool[i].m_pLinkEnt = linkEnt->pev;

                if( !FBitSet( m_pLinkPool[i].m_pLinkEnt->flags, FL_GRAPHED ) )
                {
                    m_pLinkPool[i].m_pLinkEnt->flags += FL_GRAPHED;
                }
            }
        }
    }

    // the pointers are now set.
    m_fGraphPointersSet = 1;
    return true;
}

//=========================================================
// CGraph - CheckNODFile - this function checks the date of
// the BSP file that was just loaded and the date of the a
// ssociated .NOD file. If the NOD file is not present, or
// is older than the BSP file, we rebuild it.
//
// returns false if the .NOD file doesn't qualify and needs
// to be rebuilt.
//
// !!!BUGBUG - the file times we get back are 20 hours ahead!
// since this happens consistantly, we can still correctly
// determine which of the 2 files is newer. This needs fixed,
// though. ( I now suspect that we are getting GMT back from
// these functions and must compensate for local time ) (sjb)
//=========================================================
bool CGraph::CheckNODFile( const char* szMapName )
{
    const std::string bspFileName{std::string{"maps/"} + szMapName + ".bsp"};
    const std::string graphFileName{std::string{"maps/graphs/"} + szMapName + ".nod"};

    bool retValue = true;

    int iCompare;
    if( FileSystem_CompareFileTime( bspFileName.c_str(), graphFileName.c_str(), &iCompare ) )
    {
        if( iCompare > 0 )
        { // BSP file is newer.
            Logger->debug( ".NOD File will be updated" );
            retValue = false;
        }
    }
    else
    {
        retValue = false;
    }

    return retValue;
}

#define ENTRY_STATE_EMPTY -1

struct tagNodePair
{
    short iSrc;
    short iDest;
};

void CGraph::HashInsert( int iSrcNode, int iDestNode, int iKey )
{
    tagNodePair np;

    np.iSrc = iSrcNode;
    np.iDest = iDestNode;
    CRC32_t dwHash;
    CRC32_INIT( &dwHash );
    CRC32_PROCESS_BUFFER( &dwHash, &np, sizeof( np ) );
    dwHash = CRC32_FINAL( dwHash );

    int di = m_HashPrimes[dwHash & 15];
    int i = ( dwHash >> 4 ) % m_nHashLinks;
    while( m_pHashLinks[i] != ENTRY_STATE_EMPTY )
    {
        i += di;
        if( i >= m_nHashLinks )
            i -= m_nHashLinks;
    }
    m_pHashLinks[i] = iKey;
}

void CGraph::HashSearch( int iSrcNode, int iDestNode, int& iKey )
{
    tagNodePair np;

    np.iSrc = iSrcNode;
    np.iDest = iDestNode;
    CRC32_t dwHash;
    CRC32_INIT( &dwHash );
    CRC32_PROCESS_BUFFER( &dwHash, &np, sizeof( np ) );
    dwHash = CRC32_FINAL( dwHash );

    int di = m_HashPrimes[dwHash & 15];
    int i = ( dwHash >> 4 ) % m_nHashLinks;
    while( m_pHashLinks[i] != ENTRY_STATE_EMPTY )
    {
        CLink& link = Link( m_pHashLinks[i] );
        if( iSrcNode == link.m_iSrcNode && iDestNode == link.m_iDestNode )
        {
            break;
        }
        else
        {
            i += di;
            if( i >= m_nHashLinks )
                i -= m_nHashLinks;
        }
    }
    iKey = m_pHashLinks[i];
}

#define NUMBER_OF_PRIMES 177

int Primes[NUMBER_OF_PRIMES] =
    {1, 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67,
        71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151,
        157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227, 229, 233, 239,
        241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311, 313, 317, 331, 337,
        347, 349, 353, 359, 367, 373, 379, 383, 389, 397, 401, 409, 419, 421, 431, 433,
        439, 443, 449, 457, 461, 463, 467, 479, 487, 491, 499, 503, 509, 521, 523, 541,
        547, 557, 563, 569, 571, 577, 587, 593, 599, 601, 607, 613, 617, 619, 631, 641,
        643, 647, 653, 659, 661, 673, 677, 683, 691, 701, 709, 719, 727, 733, 739, 743,
        751, 757, 761, 769, 773, 787, 797, 809, 811, 821, 823, 827, 829, 839, 853, 857,
        859, 863, 877, 881, 883, 887, 907, 911, 919, 929, 937, 941, 947, 953, 967, 971,
        977, 983, 991, 997, 1009, 1013, 1019, 1021, 1031, 1033, 1039, 0};

void CGraph::HashChoosePrimes( int TableSize )
{
    int LargestPrime = TableSize / 2;
    if( LargestPrime > Primes[NUMBER_OF_PRIMES - 2] )
    {
        LargestPrime = Primes[NUMBER_OF_PRIMES - 2];
    }
    int Spacing = LargestPrime / 16;

    // Pick a set primes that are evenly spaced from (0 to LargestPrime)
    // We divide this interval into 16 equal sized zones. We want to find
    // one prime number that best represents that zone.
    //
    int iPrime, iZone;
    for( iZone = 1, iPrime = 0; iPrime < 16; iZone += Spacing )
    {
        // Search for a prime number that is less than the target zone
        // number given by iZone.
        //
        int Lower = Primes[0];
        for( int jPrime = 0; Primes[jPrime] != 0; jPrime++ )
        {
            if( jPrime != 0 && TableSize % Primes[jPrime] == 0 )
                continue;
            int Upper = Primes[jPrime];
            if( Lower <= iZone && iZone <= Upper )
            {
                // Choose the closest lower prime number.
                //
                if( iZone - Lower <= Upper - iZone )
                {
                    m_HashPrimes[iPrime++] = Lower;
                }
                else
                {
                    m_HashPrimes[iPrime++] = Upper;
                }
                break;
            }
            Lower = Upper;
        }
    }

    // Alternate negative and positive numbers
    //
    for( iPrime = 0; iPrime < 16; iPrime += 2 )
    {
        m_HashPrimes[iPrime] = TableSize - m_HashPrimes[iPrime];
    }

    // Shuffle the set of primes to reduce correlation with bits in
    // hash key.
    //
    for( iPrime = 0; iPrime < 16 - 1; iPrime++ )
    {
        int Pick = RANDOM_LONG( 0, 15 - iPrime );
        int Temp = m_HashPrimes[Pick];
        m_HashPrimes[Pick] = m_HashPrimes[15 - iPrime];
        m_HashPrimes[15 - iPrime] = Temp;
    }
}

// Renumber nodes so that nodes that link together are together.
//
#define UNNUMBERED_NODE -1
void CGraph::SortNodes()
{
    // We are using m_iPreviousNode to be the new node number.
    // After assigning new node numbers to everything, we move
    // things and patchup the links.
    //
    int iNodeCnt = 0;
    int i;
    m_pNodes[0].m_iPreviousNode = iNodeCnt++;

    for( i = 1; i < m_cNodes; i++ )
    {
        m_pNodes[i].m_iPreviousNode = UNNUMBERED_NODE;
    }

    for( i = 0; i < m_cNodes; i++ )
    {
        // Run through all of this node's neighbors
        //
        for( int j = 0; j < m_pNodes[i].m_cNumLinks; j++ )
        {
            int iDestNode = INodeLink( i, j );
            if( m_pNodes[iDestNode].m_iPreviousNode == UNNUMBERED_NODE )
            {
                m_pNodes[iDestNode].m_iPreviousNode = iNodeCnt++;
            }
        }
    }

    // Assign remaining node numbers to unlinked nodes.
    //
    for( i = 0; i < m_cNodes; i++ )
    {
        if( m_pNodes[i].m_iPreviousNode == UNNUMBERED_NODE )
        {
            m_pNodes[i].m_iPreviousNode = iNodeCnt++;
        }
    }

    // Alter links to reflect new node numbers.
    //
    for( i = 0; i < m_cLinks; i++ )
    {
        m_pLinkPool[i].m_iSrcNode = m_pNodes[m_pLinkPool[i].m_iSrcNode].m_iPreviousNode;
        m_pLinkPool[i].m_iDestNode = m_pNodes[m_pLinkPool[i].m_iDestNode].m_iPreviousNode;
    }

    // Rearrange nodes to reflect new node numbering.
    //
    for( i = 0; i < m_cNodes; i++ )
    {
        while( m_pNodes[i].m_iPreviousNode != i )
        {
            // Move current node off to where it should be, and bring
            // that other node back into the current slot.
            //
            int iDestNode = m_pNodes[i].m_iPreviousNode;
            CNode TempNode = m_pNodes[iDestNode];
            m_pNodes[iDestNode] = m_pNodes[i];
            m_pNodes[i] = TempNode;
        }
    }
}

void CGraph::BuildLinkLookups()
{
    m_nHashLinks = 3 * m_cLinks / 2 + 3;

    HashChoosePrimes( m_nHashLinks );
    m_pHashLinks = (short*)calloc( sizeof(short), m_nHashLinks );
    if( !m_pHashLinks )
    {
        Logger->error( "Couldn't allocate Link Lookup Table." );
        return;
    }
    int i;
    for( i = 0; i < m_nHashLinks; i++ )
    {
        m_pHashLinks[i] = ENTRY_STATE_EMPTY;
    }

    for( i = 0; i < m_cLinks; i++ )
    {
        CLink& link = Link( i );
        HashInsert( link.m_iSrcNode, link.m_iDestNode, i );
    }
#if 0
    for( i = 0; i < m_cLinks; i++ )
    {
        CLink& link = Link( i );
        int iKey;
        HashSearch( link.m_iSrcNode, link.m_iDestNode, iKey );
        if( iKey != i )
        {
            Logger->error( "HashLinks don't match ({} versus {})", i, iKey );
        }
    }
#endif
}

void CGraph::BuildRegionTables()
{
    if( m_di )
        free( m_di );

    // Go ahead and setup for range searching the nodes for FindNearestNodes
    //
    m_di = ( DIST_INFO* )calloc( sizeof( DIST_INFO ), m_cNodes );
    if( !m_di )
    {
        Logger->error( "Couldn't allocate node ordering array." );
        return;
    }

    // Calculate regions for all the nodes.
    //
    //
    int i;
    for( i = 0; i < 3; i++ )
    {
        m_RegionMin[i] = 999999999.0;  // just a big number out there;
        m_RegionMax[i] = -999999999.0; // just a big number out there;
    }
    for( i = 0; i < m_cNodes; i++ )
    {
        if( m_pNodes[i].m_vecOrigin.x < m_RegionMin[0] )
            m_RegionMin[0] = m_pNodes[i].m_vecOrigin.x;
        if( m_pNodes[i].m_vecOrigin.y < m_RegionMin[1] )
            m_RegionMin[1] = m_pNodes[i].m_vecOrigin.y;
        if( m_pNodes[i].m_vecOrigin.z < m_RegionMin[2] )
            m_RegionMin[2] = m_pNodes[i].m_vecOrigin.z;

        if( m_pNodes[i].m_vecOrigin.x > m_RegionMax[0] )
            m_RegionMax[0] = m_pNodes[i].m_vecOrigin.x;
        if( m_pNodes[i].m_vecOrigin.y > m_RegionMax[1] )
            m_RegionMax[1] = m_pNodes[i].m_vecOrigin.y;
        if( m_pNodes[i].m_vecOrigin.z > m_RegionMax[2] )
            m_RegionMax[2] = m_pNodes[i].m_vecOrigin.z;
    }
    for( i = 0; i < m_cNodes; i++ )
    {
        m_pNodes[i].m_Region[0] = CALC_RANGE( m_pNodes[i].m_vecOrigin.x, m_RegionMin[0], m_RegionMax[0] );
        m_pNodes[i].m_Region[1] = CALC_RANGE( m_pNodes[i].m_vecOrigin.y, m_RegionMin[1], m_RegionMax[1] );
        m_pNodes[i].m_Region[2] = CALC_RANGE( m_pNodes[i].m_vecOrigin.z, m_RegionMin[2], m_RegionMax[2] );
    }

    for( i = 0; i < 3; i++ )
    {
        int j;
        for( j = 0; j < NUM_RANGES; j++ )
        {
            m_RangeStart[i][j] = 255;
            m_RangeEnd[i][j] = 0;
        }
        for( j = 0; j < m_cNodes; j++ )
        {
            m_di[j].m_SortedBy[i] = j;
        }

        for( j = 0; j < m_cNodes - 1; j++ )
        {
            int jNode = m_di[j].m_SortedBy[i];
            int jCodeX = m_pNodes[jNode].m_Region[0];
            int jCodeY = m_pNodes[jNode].m_Region[1];
            int jCodeZ = m_pNodes[jNode].m_Region[2];
            int jCode;
            switch ( i )
            {
            default:
                jCode = ( jCodeX << 16 ) + ( jCodeY << 8 ) + jCodeZ;
                break;
            case 1:
                jCode = ( jCodeY << 16 ) + ( jCodeZ << 8 ) + jCodeX;
                break;
            case 2:
                jCode = ( jCodeZ << 16 ) + ( jCodeX << 8 ) + jCodeY;
                break;
            }

            for( int k = j + 1; k < m_cNodes; k++ )
            {
                int kNode = m_di[k].m_SortedBy[i];
                int kCodeX = m_pNodes[kNode].m_Region[0];
                int kCodeY = m_pNodes[kNode].m_Region[1];
                int kCodeZ = m_pNodes[kNode].m_Region[2];
                int kCode;
                switch ( i )
                {
                default:
                    kCode = ( kCodeX << 16 ) + ( kCodeY << 8 ) + kCodeZ;
                    break;
                case 1:
                    kCode = ( kCodeY << 16 ) + ( kCodeZ << 8 ) + kCodeX;
                    break;
                case 2:
                    kCode = ( kCodeZ << 16 ) + ( kCodeX << 8 ) + kCodeY;
                    break;
                }

                if( kCode < jCode )
                {
                    // Swap j and k entries.
                    //
                    int Tmp = m_di[j].m_SortedBy[i];
                    m_di[j].m_SortedBy[i] = m_di[k].m_SortedBy[i];
                    m_di[k].m_SortedBy[i] = Tmp;
                }
            }
        }
    }

    // Generate lookup tables.
    //
    for( i = 0; i < m_cNodes; i++ )
    {
        int CodeX = m_pNodes[m_di[i].m_SortedBy[0]].m_Region[0];
        int CodeY = m_pNodes[m_di[i].m_SortedBy[1]].m_Region[1];
        int CodeZ = m_pNodes[m_di[i].m_SortedBy[2]].m_Region[2];

        if( i < m_RangeStart[0][CodeX] )
        {
            m_RangeStart[0][CodeX] = i;
        }
        if( i < m_RangeStart[1][CodeY] )
        {
            m_RangeStart[1][CodeY] = i;
        }
        if( i < m_RangeStart[2][CodeZ] )
        {
            m_RangeStart[2][CodeZ] = i;
        }
        if( m_RangeEnd[0][CodeX] < i )
        {
            m_RangeEnd[0][CodeX] = i;
        }
        if( m_RangeEnd[1][CodeY] < i )
        {
            m_RangeEnd[1][CodeY] = i;
        }
        if( m_RangeEnd[2][CodeZ] < i )
        {
            m_RangeEnd[2][CodeZ] = i;
        }
    }

    // Initialize the cache.
    //
    memset( m_Cache, 0, sizeof( m_Cache ) );
}

void CGraph::ComputeStaticRoutingTables()
{
    int nRoutes = m_cNodes * m_cNodes;
#define FROM_TO(x, y) ((x)*m_cNodes + (y))
    short* Routes = new short[nRoutes];

    int* pMyPath = new int[m_cNodes];
    unsigned short* BestNextNodes = new unsigned short[m_cNodes];
    char* pRoute = new char[m_cNodes * 2];


    if( Routes && pMyPath && BestNextNodes && pRoute )
    {
        int nTotalCompressedSize = 0;
        for( int iHull = 0; iHull < MAX_NODE_HULLS; iHull++ )
        {
            for( int iCap = 0; iCap < 2; iCap++ )
            {
                int iCapMask;
                switch ( iCap )
                {
                default:
                    iCapMask = 0;
                    break;

                case 1:
                    iCapMask = bits_CAP_OPEN_DOORS | bits_CAP_AUTO_DOORS | bits_CAP_USE;
                    break;
                }


                // Initialize Routing table to uncalculated.
                //
                int iFrom;
                for( iFrom = 0; iFrom < m_cNodes; iFrom++ )
                {
                    for( int iTo = 0; iTo < m_cNodes; iTo++ )
                    {
                        Routes[FROM_TO( iFrom, iTo )] = -1;
                    }
                }

                for( iFrom = 0; iFrom < m_cNodes; iFrom++ )
                {
                    for( int iTo = m_cNodes - 1; iTo >= 0; iTo-- )
                    {
                        if( Routes[FROM_TO( iFrom, iTo )] != -1 )
                            continue;

                        int cPathSize = FindShortestPath( pMyPath, iFrom, iTo, iHull, iCapMask );

                        // Use the computed path to update the routing table.
                        //
                        if( cPathSize > 1 )
                        {
                            for( int iNode = 0; iNode < cPathSize - 1; iNode++ )
                            {
                                int iStart = pMyPath[iNode];
                                int iNext = pMyPath[iNode + 1];
                                for( int iNode1 = iNode + 1; iNode1 < cPathSize; iNode1++ )
                                {
                                    int iEnd = pMyPath[iNode1];
                                    Routes[FROM_TO( iStart, iEnd )] = iNext;
                                }
                            }
#if 0
                            // Well, at first glance, this should work, but actually it's safer
                            // to be told explictly that you can take a series of node in a
                            // particular direction. Some links don't appear to have links in
                            // the opposite direction.
                            //
                            for( iNode = cPathSize - 1; iNode >= 1; iNode-- )
                            {
                                int iStart = pMyPath[iNode];
                                int iNext = pMyPath[iNode - 1];
                                for( int iNode1 = iNode - 1; iNode1 >= 0; iNode1-- )
                                {
                                    int iEnd = pMyPath[iNode1];
                                    Routes[FROM_TO( iStart, iEnd )] = iNext;
                                }
                            }
#endif
                        }
                        else
                        {
                            Routes[FROM_TO( iFrom, iTo )] = iFrom;
                            Routes[FROM_TO( iTo, iFrom )] = iTo;
                        }
                    }
                }

                for( iFrom = 0; iFrom < m_cNodes; iFrom++ )
                {
                    for( int iTo = 0; iTo < m_cNodes; iTo++ )
                    {
                        BestNextNodes[iTo] = Routes[FROM_TO( iFrom, iTo )];
                    }

                    // Compress this node's routing table.
                    //
                    int iLastNode = 9999999; // just really big.
                    int cSequence = 0;
                    int cRepeats = 0;
                    int CompressedSize = 0;
                    char* p = pRoute;
                    for( int i = 0; i < m_cNodes; i++ )
                    {
                        bool CanRepeat = ( ( BestNextNodes[i] == iLastNode ) && cRepeats < 127 );
                        bool CanSequence = ( BestNextNodes[i] == i && cSequence < 128 );

                        if( 0 != cRepeats )
                        {
                            if( CanRepeat )
                            {
                                cRepeats++;
                            }
                            else
                            {
                                // Emit the repeat phrase.
                                //
                                CompressedSize += 2; // (count-1, iLastNode-i)
                                *p++ = cRepeats - 1;
                                int a = iLastNode - iFrom;
                                int b = iLastNode - iFrom + m_cNodes;
                                int c = iLastNode - iFrom - m_cNodes;
                                if( -128 <= a && a <= 127 )
                                {
                                    *p++ = a;
                                }
                                else if( -128 <= b && b <= 127 )
                                {
                                    *p++ = b;
                                }
                                else if( -128 <= c && c <= 127 )
                                {
                                    *p++ = c;
                                }
                                else
                                {
                                    Logger->debug( "Nodes need sorting ({},{})!", iLastNode, iFrom );
                                }
                                cRepeats = 0;

                                if( CanSequence )
                                {
                                    // Start a sequence.
                                    //
                                    cSequence++;
                                }
                                else
                                {
                                    // Start another repeat.
                                    //
                                    cRepeats++;
                                }
                            }
                        }
                        else if( 0 != cSequence )
                        {
                            if( CanSequence )
                            {
                                cSequence++;
                            }
                            else
                            {
                                // It may be advantageous to combine
                                // a single-entry sequence phrase with the
                                // next repeat phrase.
                                //
                                if( cSequence == 1 && CanRepeat )
                                {
                                    // Combine with repeat phrase.
                                    //
                                    cRepeats = 2;
                                    cSequence = 0;
                                }
                                else
                                {
                                    // Emit the sequence phrase.
                                    //
                                    CompressedSize += 1; // (-count)
                                    *p++ = -cSequence;
                                    cSequence = 0;

                                    // Start a repeat sequence.
                                    //
                                    cRepeats++;
                                }
                            }
                        }
                        else
                        {
                            if( CanSequence )
                            {
                                // Start a sequence phrase.
                                //
                                cSequence++;
                            }
                            else
                            {
                                // Start a repeat sequence.
                                //
                                cRepeats++;
                            }
                        }
                        iLastNode = BestNextNodes[i];
                    }
                    if( 0 != cRepeats )
                    {
                        // Emit the repeat phrase.
                        //
                        CompressedSize += 2;
                        *p++ = cRepeats - 1;
#if 0
                        iLastNode = iFrom + *pRoute;
                        if( iLastNode >= m_cNodes ) iLastNode -= m_cNodes;
                        else if( iLastNode < 0 ) iLastNode += m_cNodes;
#endif
                        int a = iLastNode - iFrom;
                        int b = iLastNode - iFrom + m_cNodes;
                        int c = iLastNode - iFrom - m_cNodes;
                        if( -128 <= a && a <= 127 )
                        {
                            *p++ = a;
                        }
                        else if( -128 <= b && b <= 127 )
                        {
                            *p++ = b;
                        }
                        else if( -128 <= c && c <= 127 )
                        {
                            *p++ = c;
                        }
                        else
                        {
                            Logger->debug( "Nodes need sorting ({},{})!", iLastNode, iFrom );
                        }
                    }
                    if( 0 != cSequence )
                    {
                        // Emit the Sequence phrase.
                        //
                        CompressedSize += 1;
                        *p++ = -cSequence;
                    }

                    // Go find a place to store this thing and point to it.
                    //
                    int nRoute = p - pRoute;
                    if( m_pRouteInfo )
                    {
                        int i;
                        for( i = 0; i < m_nRouteInfo - nRoute; i++ )
                        {
                            if( memcmp( m_pRouteInfo + i, pRoute, nRoute ) == 0 )
                            {
                                break;
                            }
                        }
                        if( i < m_nRouteInfo - nRoute )
                        {
                            m_pNodes[iFrom].m_pNextBestNode[iHull][iCap] = i;
                        }
                        else
                        {
                            char* Tmp = (char*)calloc( sizeof(char), ( m_nRouteInfo + nRoute ) );
                            memcpy( Tmp, m_pRouteInfo, m_nRouteInfo );
                            free( m_pRouteInfo );
                            m_pRouteInfo = Tmp;
                            memcpy( m_pRouteInfo + m_nRouteInfo, pRoute, nRoute );
                            m_pNodes[iFrom].m_pNextBestNode[iHull][iCap] = m_nRouteInfo;
                            m_nRouteInfo += nRoute;
                            nTotalCompressedSize += CompressedSize;
                        }
                    }
                    else
                    {
                        m_nRouteInfo = nRoute;
                        m_pRouteInfo = (char*)calloc( sizeof(char), nRoute );
                        memcpy( m_pRouteInfo, pRoute, nRoute );
                        m_pNodes[iFrom].m_pNextBestNode[iHull][iCap] = 0;
                        nTotalCompressedSize += CompressedSize;
                    }
                }
            }
        }
        Logger->debug( "Size of Routes = {}", nTotalCompressedSize );
    }

    delete[] Routes;
    delete[] BestNextNodes;
    delete[] pRoute;
    delete[] pMyPath;

    Routes = nullptr;
    BestNextNodes = nullptr;
    pRoute = nullptr;
    pMyPath = nullptr;

#if 0
    TestRoutingTables();
#endif
    m_fRoutingComplete = 1;
}

// Test those routing tables. Doesn't really work, yet.
//
void CGraph::TestRoutingTables()
{
    int* pMyPath = new int[m_cNodes];
    int* pMyPath2 = new int[m_cNodes];
    if( pMyPath && pMyPath2 )
    {
        for( int iHull = 0; iHull < MAX_NODE_HULLS; iHull++ )
        {
            for( int iCap = 0; iCap < 2; iCap++ )
            {
                int iCapMask;
                switch ( iCap )
                {
                default:
                    iCapMask = 0;
                    break;

                case 1:
                    iCapMask = bits_CAP_OPEN_DOORS | bits_CAP_AUTO_DOORS | bits_CAP_USE;
                    break;
                }

                for( int iFrom = 0; iFrom < m_cNodes; iFrom++ )
                {
                    for( int iTo = 0; iTo < m_cNodes; iTo++ )
                    {
                        m_fRoutingComplete = 0;
                        int cPathSize1 = FindShortestPath( pMyPath, iFrom, iTo, iHull, iCapMask );
                        m_fRoutingComplete = 1;
                        int cPathSize2 = FindShortestPath( pMyPath2, iFrom, iTo, iHull, iCapMask );

                        // Unless we can look at the entire path, we can verify that it's correct.
                        //
                        if( cPathSize2 == MAX_PATH_SIZE )
                            continue;

                            // Compare distances.
                            //
#if 1
                        float flDistance1 = 0.0;

                        for( int i = 0; i < cPathSize1 - 1; i++ )
                        {
                            // Find the link from pMyPath[i] to pMyPath[i+1]
                            //
                            if( pMyPath[i] == pMyPath[i + 1] )
                                continue;
                            int iVisitNode;
                            bool bFound = false;
                            for( int iLink = 0; iLink < m_pNodes[pMyPath[i]].m_cNumLinks; iLink++ )
                            {
                                iVisitNode = INodeLink( pMyPath[i], iLink );
                                if( iVisitNode == pMyPath[i + 1] )
                                {
                                    flDistance1 += m_pLinkPool[m_pNodes[pMyPath[i]].m_iFirstLink + iLink].m_flWeight;
                                    bFound = true;
                                    break;
                                }
                            }
                            if( !bFound )
                            {
                                Logger->trace( "No link." );
                            }
                        }

                        float flDistance2 = 0.0;
                        for( int i = 0; i < cPathSize2 - 1; i++ )
                        {
                            // Find the link from pMyPath2[i] to pMyPath2[i+1]
                            //
                            if( pMyPath2[i] == pMyPath2[i + 1] )
                                continue;
                            int iVisitNode;
                            bool bFound = false;
                            for( int iLink = 0; iLink < m_pNodes[pMyPath2[i]].m_cNumLinks; iLink++ )
                            {
                                iVisitNode = INodeLink( pMyPath2[i], iLink );
                                if( iVisitNode == pMyPath2[i + 1] )
                                {
                                    flDistance2 += m_pLinkPool[m_pNodes[pMyPath2[i]].m_iFirstLink + iLink].m_flWeight;
                                    bFound = true;
                                    break;
                                }
                            }
                            if( !bFound )
                            {
                                Logger->trace( "No link." );
                            }
                        }
                        if( fabs( flDistance1 - flDistance2 ) > 0.10 )
                        {
#else
                        if( cPathSize1 != cPathSize2 || memcmp( pMyPath, pMyPath2, sizeof(int) * cPathSize1 ) != 0 )
                        {
#endif
                            Logger->error( "Routing is inconsistent!!!" );
                            Logger->error( "({} to {} |{}/{})1:", iFrom, iTo, iHull, iCap );
                            for( int i = 0; i < cPathSize1; i++ )
                            {
                                Logger->error( "{}", pMyPath[i] );
                            }
                            Logger->trace( "({} to {} |{}/{})2:", iFrom, iTo, iHull, iCap );
                            for( int i = 0; i < cPathSize2; i++ )
                            {
                                Logger->trace( "{}", pMyPath2[i] );
                            }
                            m_fRoutingComplete = 0;
                            cPathSize1 = FindShortestPath( pMyPath, iFrom, iTo, iHull, iCapMask );
                            m_fRoutingComplete = 1;
                            cPathSize2 = FindShortestPath( pMyPath2, iFrom, iTo, iHull, iCapMask );
                            goto EnoughSaid;
                        }
                    }
                }
            }
        }
    }

EnoughSaid:

    delete[] pMyPath;
    delete[] pMyPath2;
    pMyPath = nullptr;
    pMyPath2 = nullptr;
}









//=========================================================
// CNodeViewer - Draws a graph of the shorted path from all nodes
// to current location (typically the player).  It then draws
// as many connects as it can per frame, trying not to overflow the buffer
//=========================================================
class CNodeViewer : public CBaseEntity
{
    DECLARE_CLASS( CNodeViewer, CBaseEntity );
    DECLARE_DATAMAP();

public:
    SpawnAction Spawn() override;

    int m_iBaseNode;
    int m_iDraw;
    int m_nVisited;
    int m_aFrom[128];
    int m_aTo[128];
    int m_iHull;
    int m_afNodeType;
    Vector m_vecColor;

    void FindNodeConnections( int iNode );
    void AddNode( int iFrom, int iTo );
    void DrawThink();
};

BEGIN_DATAMAP( CNodeViewer )
    DEFINE_FUNCTION( DrawThink ),
END_DATAMAP();

LINK_ENTITY_TO_CLASS( node_viewer, CNodeViewer );
LINK_ENTITY_TO_CLASS( node_viewer_human, CNodeViewer );
LINK_ENTITY_TO_CLASS( node_viewer_fly, CNodeViewer );
LINK_ENTITY_TO_CLASS( node_viewer_large, CNodeViewer );

SpawnAction CNodeViewer::Spawn()
{
    if( 0 == WorldGraph.m_fGraphPresent || 0 == WorldGraph.m_fGraphPointersSet )
    { // protect us in the case that the node graph isn't available or built
        CGraph::Logger->error( "Graph not ready!" );
        return SpawnAction::RemoveNow;
    }

    if( ClassnameIs( "node_viewer_fly" ) )
    {
        m_iHull = NODE_FLY_HULL;
        m_afNodeType = bits_NODE_AIR;
        m_vecColor = Vector( 160, 100, 255 );
    }
    else if( ClassnameIs( "node_viewer_large" ) )
    {
        m_iHull = NODE_LARGE_HULL;
        m_afNodeType = bits_NODE_LAND | bits_NODE_WATER;
        m_vecColor = Vector( 100, 255, 160 );
    }
    else
    {
        m_iHull = NODE_HUMAN_HULL;
        m_afNodeType = bits_NODE_LAND | bits_NODE_WATER;
        m_vecColor = Vector( 255, 160, 100 );
    }


    m_iBaseNode = WorldGraph.FindNearestNode( pev->origin, m_afNodeType );

    if( m_iBaseNode < 0 )
    {
        CGraph::Logger->debug( "No nearby node" );
        return SpawnAction::DelayRemove;
    }

    m_nVisited = 0;

    CGraph::Logger->debug( "basenode {}", m_iBaseNode );

    if( WorldGraph.m_cNodes < 128 )
    {
        for( int i = 0; i < WorldGraph.m_cNodes; i++ )
        {
            AddNode( i, WorldGraph.NextNodeInRoute( i, m_iBaseNode, m_iHull, 0 ) );
        }
    }
    else
    {
        // do a depth traversal
        FindNodeConnections( m_iBaseNode );

        int start = 0;
        int end;
        do
        {
            end = m_nVisited;
            // Logger->info("{} :", m_nVisited);
            for( end = m_nVisited; start < end; start++ )
            {
                FindNodeConnections( m_aFrom[start] );
                FindNodeConnections( m_aTo[start] );
            }
        } while( end != m_nVisited );
    }

    CGraph::Logger->debug( "{} nodes", m_nVisited );

    m_iDraw = 0;
    SetThink( &CNodeViewer::DrawThink );
    pev->nextthink = gpGlobals->time;

    return SpawnAction::HandledSpawn;
}


void CNodeViewer::FindNodeConnections( int iNode )
{
    AddNode( iNode, WorldGraph.NextNodeInRoute( iNode, m_iBaseNode, m_iHull, 0 ) );
    for( int i = 0; i < WorldGraph.m_pNodes[iNode].m_cNumLinks; i++ )
    {
        CLink* pToLink = &WorldGraph.NodeLink( iNode, i );
        AddNode( pToLink->m_iDestNode, WorldGraph.NextNodeInRoute( pToLink->m_iDestNode, m_iBaseNode, m_iHull, 0 ) );
    }
}

void CNodeViewer::AddNode( int iFrom, int iTo )
{
    if( m_nVisited >= 128 )
    {
        return;
    }
    else
    {
        if( iFrom == iTo )
            return;

        for( int i = 0; i < m_nVisited; i++ )
        {
            if( m_aFrom[i] == iFrom && m_aTo[i] == iTo )
                return;
            if( m_aFrom[i] == iTo && m_aTo[i] == iFrom )
                return;
        }
        m_aFrom[m_nVisited] = iFrom;
        m_aTo[m_nVisited] = iTo;
        m_nVisited++;
    }
}


void CNodeViewer::DrawThink()
{
    pev->nextthink = gpGlobals->time;

    for( int i = 0; i < 10; i++ )
    {
        if( m_iDraw == m_nVisited )
        {
            UTIL_Remove( this );
            return;
        }

        extern short g_sModelIndexLaser;
        MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
        WRITE_BYTE( TE_BEAMPOINTS );
        WRITE_COORD( WorldGraph.m_pNodes[m_aFrom[m_iDraw]].m_vecOrigin.x );
        WRITE_COORD( WorldGraph.m_pNodes[m_aFrom[m_iDraw]].m_vecOrigin.y );
        WRITE_COORD( WorldGraph.m_pNodes[m_aFrom[m_iDraw]].m_vecOrigin.z + NODE_HEIGHT );

        WRITE_COORD( WorldGraph.m_pNodes[m_aTo[m_iDraw]].m_vecOrigin.x );
        WRITE_COORD( WorldGraph.m_pNodes[m_aTo[m_iDraw]].m_vecOrigin.y );
        WRITE_COORD( WorldGraph.m_pNodes[m_aTo[m_iDraw]].m_vecOrigin.z + NODE_HEIGHT );
        WRITE_SHORT( g_sModelIndexLaser );
        WRITE_BYTE( 0 );              // framerate
        WRITE_BYTE( 0 );              // framerate
        WRITE_BYTE( 250 );          // life
        WRITE_BYTE( 40 );              // width
        WRITE_BYTE( 0 );              // noise
        WRITE_BYTE( m_vecColor.x ); // r, g, b
        WRITE_BYTE( m_vecColor.y ); // r, g, b
        WRITE_BYTE( m_vecColor.z ); // r, g, b
        WRITE_BYTE( 128 );          // brightness
        WRITE_BYTE( 0 );              // speed
        MESSAGE_END();

        m_iDraw++;
    }
}
