using Utilities.Entities;
using Utilities.Tools.UpgradeTool;

namespace MapUpgrader.Upgrades.BlueShift
{
    /// <summary>
    /// Removes the HEV suit from <c>ba_tram1</c> (now given by map config).
    /// </summary>
    internal sealed class BaTram1FixSuitUpgrade : MapSpecificUpgrade
    {
        public BaTram1FixSuitUpgrade()
            : base("ba_tram1")
        {
        }

        protected override void ApplyCore(MapUpgradeContext context)
        {
            context.Map.Entities.RemoveAllOfClass("item_suit");
        }
    }
}
