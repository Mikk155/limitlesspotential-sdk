using Utilities.Entities;
using Utilities.Games;
using Utilities.Tools.UpgradeTool;

namespace MapUpgrader.Upgrades.OpposingForce
{
    /// <summary>
    /// Converts <c>item_suit</c>'s model to use <c>w_pcv.mdl</c> in Opposing Force maps.
    /// </summary>
    internal sealed class ConvertSuitToPCVUpgrade : GameSpecificMapUpgrade
    {
        public ConvertSuitToPCVUpgrade()
            : base(ValveGames.OpposingForce)
        {
        }

        protected override void ApplyCore(MapUpgradeContext context)
        {
            foreach (var entity in context.Map.Entities.OfClass("item_suit"))
            {
                entity.SetModel("models/w_pcv.mdl");
            }
        }
    }
}
