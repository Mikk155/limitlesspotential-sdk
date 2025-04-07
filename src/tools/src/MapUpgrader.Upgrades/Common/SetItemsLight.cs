using Utilities.Entities;
using Utilities.Tools.UpgradeTool;
using System.Collections.Immutable;
using System.Numerics;

namespace MapUpgrader.Upgrades.Common
{
    /// <summary>
    /// Sets items new renderfx mode for displaying dynamic lighting
    /// </summary>
    internal sealed class SetItemsLight : MapUpgrade
    {
        private static readonly ImmutableArray<string> ClassNames = ImmutableArray.Create(
            "item_battery",
            "ammo_spore"
        );

        protected override void ApplyCore( MapUpgradeContext context )
        {
            foreach( var entity in context.Map.Entities.Where( e => ClassNames.Contains( e.ClassName ) ) )
            {
                if( entity.GetRenderMode() == 0 && entity.GetInteger( "renderfx" ) == 0 )
                {
                    entity.SetInteger( "renderfx", 22 );

                    if( entity.ClassName == "ammo_spore" )
                    {
                        if( entity.GetInteger( "renderamt" ) == 0 )
                            entity.SetInteger( "renderamt", 10 );

                        if( entity.GetRenderColor() == System.Numerics.Vector3.Zero )
                            entity.SetVector3( "rendercolor", new Vector3( 0, 255, 200 ) );
                    }
                    else if( entity.ClassName == "item_battery" )
                    {
                        if( entity.GetInteger( "renderamt" ) == 0 )
                            entity.SetInteger( "renderamt", 10 );

                        if( entity.GetRenderColor() == System.Numerics.Vector3.Zero )
                            entity.SetVector3( "rendercolor", new Vector3( 0, 90, 70 ) );
                    }
                }
            }
        }
    }
}
