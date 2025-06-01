using Utilities.Entities;
using Utilities.Tools.UpgradeTool;
using System.Collections.Immutable;
using System.Linq;
using Serilog.Core;
using System;
using System.CommandLine.IO;

namespace MapUpgrader.Upgrades.Common
{
    /// <summary>
    /// Update ambient_generic spawnflags to a more flexible keyvalue
    /// </summary>
    internal sealed class AmbientGenericAttenuation : MapUpgrade
    {
        private static readonly double ATTN_NONE = 0;
        private static readonly double ATTN_NORM = 0.8;
        private static readonly double ATTN_IDLE = 2;
        private static readonly double ATTN_STATIC = 1.25;

//        private static readonly int AMBIENT_SOUND_STATIC = 0;
        private static readonly int AMBIENT_SOUND_EVERYWHERE = 1;
        private static readonly int AMBIENT_SOUND_SMALLRADIUS = 2;
        private static readonly int AMBIENT_SOUND_MEDIUMRADIUS = 4;
        private static readonly int AMBIENT_SOUND_LARGERADIUS = 8;

        protected override void ApplyCore( MapUpgradeContext context )
        {
            foreach( Entity entity in context.Map.Entities.Where( e => e.ClassName == "ambient_generic" ) )
            {
                int flags = entity.GetSpawnFlags();

                if( !entity.ContainsKey( "m_flAttenuation" ) && flags != 0 )
                {
                    if( ( flags & AMBIENT_SOUND_EVERYWHERE ) != 0 )
                    {
                        entity.SetDouble( "m_flAttenuation", ATTN_NONE );
                    }
                    else if( ( flags & AMBIENT_SOUND_SMALLRADIUS ) != 0 )
                    {
                        entity.SetDouble( "m_flAttenuation", ATTN_IDLE );
                    }
                    else if( ( flags & AMBIENT_SOUND_MEDIUMRADIUS ) != 0 )
                    {
                        entity.SetDouble( "m_flAttenuation", ATTN_STATIC );
                    }
                    else if( ( flags & AMBIENT_SOUND_LARGERADIUS ) != 0 )
                    {
                        entity.SetDouble( "m_flAttenuation", ATTN_NORM );
                    }
                    else
                    {
                        // if the designer didn't set a sound attenuation, default to one.
                        entity.SetDouble( "m_flAttenuation", ATTN_STATIC );
                    }

                    if ( ( flags & AMBIENT_SOUND_EVERYWHERE ) != 0 )
                        flags &= ~AMBIENT_SOUND_EVERYWHERE;
                    if ( ( flags & AMBIENT_SOUND_SMALLRADIUS ) != 0 )
                        flags &= ~AMBIENT_SOUND_SMALLRADIUS;
                    if ( ( flags & AMBIENT_SOUND_MEDIUMRADIUS ) != 0 )
                        flags &= ~AMBIENT_SOUND_MEDIUMRADIUS;
                    if ( ( flags & AMBIENT_SOUND_LARGERADIUS ) != 0 )
                        flags &= ~AMBIENT_SOUND_LARGERADIUS;

                    entity.SetSpawnFlags( flags );
                }
            }
        }
    }
}
