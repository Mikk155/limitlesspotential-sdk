using Utilities.Entities;
using Utilities.Tools.UpgradeTool;

namespace MapUpgrader.Upgrades.HalfLife
{
    /// <summary>
    /// Fixes "n_ending" to be triggered instead of touched and add auxiliary entities for cooperative setup
    /// </summary>
    internal sealed class C4a3FixTeleportending : MapSpecificUpgrade
    {
        public C4a3FixTeleportending()
            : base("c4a3")
        {
        }

        protected override void ApplyCore(MapUpgradeContext context)
        {
            foreach( Entity entity in context.Map.Entities.Where( e => e.GetTargetName() == "n_ending" ) )
            {
                entity.SetTargetName( "n_ending_teleport" );
                break;
            }

            Entity it = context.Map.Entities.CreateNewEntity( "trigger_entity_iterator" );
            it.SetTarget( "n_ending_teleport#7" );
            it.SetInteger( "status_filter", 1 );
            it.SetString( "classname_filter", "player" );
            it.SetInteger( "run_mode", 0 );
            it.SetTargetName( "n_ending" );
        }
    }
}
