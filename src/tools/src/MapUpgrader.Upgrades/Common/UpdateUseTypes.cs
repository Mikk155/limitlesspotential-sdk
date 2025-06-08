using Utilities.Entities;
using Utilities.Tools.UpgradeTool;
using System.Collections.Immutable;
using System.Linq;

namespace MapUpgrader.Upgrades.Common
{
    /// <summary>
    /// Update entity's old USE_TYPE systems with our new global one.
    /// </summary>
    internal sealed class UpdateUseTypes : MapUpgrade
    {
        private static readonly ImmutableArray<string> ClassNames = ImmutableArray.Create(
            "trigger_relay",
            "trigger_auto",
            "trigger_ctfgeneric",
            "game_team_master"
        );

        private int GetDefault( Entity entity )
        {
            switch( entity.ClassName )
            {
                case "trigger_auto":
                case "trigger_ctfgeneric":
                case "game_team_master":
                {
                    return 1; // They return USE_ON on default case
                }
                case "trigger_relay":
                default:
                {
                    return 0; // This one returns USE_OFF
                }
            }
        }

        protected override void ApplyCore( MapUpgradeContext context )
        {
            foreach( var entity in context.Map.Entities.Where( e => ClassNames.Contains( e.ClassName ) ) )
            {
                if( !entity.ContainsKey( "triggerstate" ) )
                {
                    entity.SetInteger( "m_UseType", 0 ); // Fall to USE_OFF for uninitialized types
                    continue;
                }

                int trigger_type = entity.GetInteger( "triggerstate" ) switch
                {
                    0 => 0, // Off
                    1 => 1, // On
                    2 => 3, // TOGGLE
                    _ => GetDefault(entity)
                };

                entity.Remove( "triggerstate" );
                entity.SetInteger( "m_UseType", trigger_type );
            }
        }
    }
}
