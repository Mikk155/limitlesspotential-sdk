using Utilities.Entities;
using Utilities.Tools.UpgradeTool;

namespace MapUpgrader.Upgrades.Common
{
    /// <summary>
    /// Changes all unnamed <c>game_player_equip</c> entities to be fired on player spawn.
    /// </summary>
    internal sealed class ReworkGamePlayerEquipUpgrade : MapUpgrade
    {
        private const int UseOnlyFlag = 1 << 0;

        protected override void ApplyCore(MapUpgradeContext context)
        {
            bool HasEventHandler = false;

            foreach (var entity in context.Map.Entities
                .OfClass("game_player_equip")
                .Where(e => string.IsNullOrEmpty(e.GetTargetName())))
            {
                int spawnFlags = entity.GetSpawnFlags();

                // If it's marked as use only already then it was previously unused, so skip it.
                if ((spawnFlags & UseOnlyFlag) != 0)
                {
                    continue;
                }

                entity.SetTargetName("GPE_PlayerSpawn");
                HasEventHandler = true;

                entity.SetSpawnFlags(spawnFlags | UseOnlyFlag);
            }

            if( HasEventHandler )
            {
                Entity EventHandler = context.Map.Entities.CreateNewEntity( "trigger_eventhandler" );
                EventHandler.SetInteger( "event_type", 6 );
                EventHandler.SetTarget( "GPE_PlayerSpawn" );
            }
        }
    }
}
