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
    /// Update ambient_generic
    /// </summary>
    internal sealed class ConvertAmbientGeneric : MapUpgrade
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
        private static readonly int AMBIENT_SOUND_START_SILENT = 16;
        private static readonly int AMBIENT_SOUND_NOT_LOOPING = 32;

        protected override void ApplyCore( MapUpgradeContext context )
        {
            foreach( Entity entity in context.Map.Entities.Where( e => e.ClassName == "ambient_generic" ) )
            {
                int flags = entity.GetSpawnFlags();

                if( flags != 0 )
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

                    if( ( flags & AMBIENT_SOUND_NOT_LOOPING ) == 0 )
                    {
                        entity.SetBool( "m_fLooping", 1 ); // Entity not marked as not looped
                    }

                    if( ( flags & AMBIENT_SOUND_START_SILENT ) != 1 )
                    {
                        entity.SetBool( "m_bStartOff", 1 );
                    }

                    List<int> RemoveFlags = new(){
                        AMBIENT_SOUND_EVERYWHERE,
                        AMBIENT_SOUND_SMALLRADIUS,
                        AMBIENT_SOUND_MEDIUMRADIUS,
                        AMBIENT_SOUND_LARGERADIUS,
                        AMBIENT_SOUND_START_SILENT,
                        AMBIENT_SOUND_NOT_LOOPING
                    };

                    foreach( int flag in RemoveFlags )
                    {
                        if( ( flags & flag ) != 0 )
                        {
                            flags &= ~flag;
                        }
                    }
                }

                if( entity.ContainsKey( "message" ) )
                {
                    entity.SetString( "m_sPlaySound", entity.GetString( "message" ) );
                    entity.Remove( "message" );
                }

                flags |= 2; // Don't remove on fire

                entity.SetSpawnFlags( flags );
            }
        }
    }
}
