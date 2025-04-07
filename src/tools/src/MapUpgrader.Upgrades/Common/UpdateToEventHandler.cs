using Utilities.Entities;
using Utilities.Tools.UpgradeTool;
using System.Collections.Immutable;

namespace MapUpgrader.Upgrades.Common
{
    /// <summary>
    /// Update the old's game auto triggering by events to our new trigger_eventhandler
    /// </summary>
    internal sealed class UpdateToEventHandler : MapUpgrade
    {
        private static Dictionary<string, int> Targetnames = new()
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
            foreach( Entity entity in context.Map.Entities.Where( ent => Targetnames.ContainsKey( ent.GetTargetName() ) ) )
            {
                Entity EventHandler = context.Map.Entities.CreateNewEntity( "trigger_eventhandler" );
                EventHandler.SetInteger( "event_type", Targetnames[ entity.GetTargetName() ] );
                EventHandler.SetTarget( entity.GetTargetName() );

                // Some things were MP only in the game code
                // So let's allow SP mods to use them now w/o breaking legacy maps.
                switch( entity.GetTargetName() )
                {
                    case "game_playerkill":
                    case "game_playerdie":
                    case "game_playerjoin":
                    case "game_playerleave":
                    {
                        // The game used to pass the attacker as both caller and activator
                        if( entity.GetTargetName() == "game_playerkill" )
                        {
                            EventHandler.SetString( "m_Caller", "!activator" );
                        }

                        EventHandler.SetInteger( "appearflag_multiplayer", -1 );
                        break;
                    }
                }

                Targetnames.Remove( entity.GetTargetName() );
            }
        }
    }
}
