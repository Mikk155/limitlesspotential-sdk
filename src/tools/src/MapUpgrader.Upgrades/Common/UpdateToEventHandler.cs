using Utilities.Entities;
using Utilities.Tools.UpgradeTool;
using System.Collections.Immutable;
using System.Collections.Generic;

namespace MapUpgrader.Upgrades.Common
{
    /// <summary>
    /// Update the old's game auto triggering by events to our new trigger_eventhandler
    /// </summary>
    internal sealed class UpdateToEventHandler : MapUpgrade
    {
        private static readonly Dictionary<string, int> Targetnames = new()
        {
            [ "game_playerdie" ] = 1,
            [ "game_playerleave" ] = 2,
            [ "game_playerkill" ] = 3,
            [ "game_playeractivate" ] = 4,
            [ "game_playerjoin" ] = 5,
            [ "game_playerspawn" ] = 6
        };

        protected override void ApplyCore( MapUpgradeContext context )
        {
            foreach( KeyValuePair<string, int> pair_name in Targetnames )
            {
                Entity? entity = context.Map.Entities.Find( pair_name.Key );

                if( entity is not null )
                {
                    Entity EventHandler = context.Map.Entities.CreateNewEntity( "trigger_eventhandler" );
                    EventHandler.SetInteger( "event_type", pair_name.Value );
                    EventHandler.SetTarget( pair_name.Key );

                    // Some things were MP only in the game code
                    // So let's allow SP mods to use them now w/o breaking legacy maps.
                    switch( pair_name.Key )
                    {
                        case "game_playerkill":
                        case "game_playerdie":
                        case "game_playerjoin":
                        case "game_playerleave":
                        {
                            // They don't work in SP
                            EventHandler.SetInteger( "appearflag_multiplayer", -1 );
                            break;
                        }
                    }

                    // The game used to pass the attacker as both caller and activator
                    if( pair_name.Key == "game_playerkill" )
                    {
                        EventHandler.SetString( "m_Caller", "!activator" );
                    }
                }
            }
        }
    }
}
