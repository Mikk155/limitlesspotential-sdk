using Utilities.Entities;
using Utilities.Tools.UpgradeTool;

namespace MapUpgrader.Upgrades.BlueShift
{
    /// <summary>
    /// Removes the <c>body</c> keyvalue from the <c>monster_generic</c> newspaper scientist in <c>ba_tram1</c>.
    /// </summary>
    internal sealed class BaTram1FixScientistHeadUpgrade : MapSpecificUpgrade
    {
        public BaTram1FixScientistHeadUpgrade()
            : base("ba_tram1")
        {
        }

        protected override void ApplyCore(MapUpgradeContext context)
        {
            context.Map.Entities.Find("sitter")?.Remove("body");
        }
    }
}
