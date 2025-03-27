using Utilities.Entities;
using Utilities.Tools.UpgradeTool;
using System.Collections.Immutable;

namespace MapUpgrader.Upgrades.OpposingForce
{
    /// <summary>
    /// Disables item dropping for a couple NPCs in the Opposing Force intro map so players can't get weapons from them if they die.
    /// </summary>
    internal sealed class Of0a0DisableItemDroppingUpgrade : MapSpecificUpgrade
    {
        private static readonly ImmutableArray<string> NPCTargetNames = ImmutableArray.Create(
            "ichabod",
            "booth");

        public Of0a0DisableItemDroppingUpgrade()
            : base("of0a0")
        {
        }

        protected override void ApplyCore(MapUpgradeContext context)
        {
            foreach (var entity in context.Map.Entities.Where(e => NPCTargetNames.Contains(e.GetTargetName())))
            {
                entity.SetInteger("cfg_allow_npc_item_dropping", 0);
            }
        }
    }
}
