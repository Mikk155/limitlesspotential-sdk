#pragma once

struct SteamID
{
    public:

        static constexpr uint64_t steamOffset = 76561197960265728ULL;

        /**
         * @brief Steam ID in uint64 form
         */
        const uint64_t steamID64;

#ifndef CLIENT_DLL
        const bool local{false};
#else
        const bool local{true};
#endif

        static uint64_t ToUint( uint32_t y, uint32_t z )
        {
            return static_cast<uint64_t>( ( static_cast<uint64_t>( y ) << 32 ) | z );
        }

        static uint64_t ToUint( const char* sid )
        {
            return static_cast<uint64_t>( strtoull( sid, nullptr, 10 ) );
        }

        SteamID( uint64_t sid ) : steamID64( sid ) { }

        SteamID( uint32_t y, uint32_t z ) : steamID64( ToUint( y, z ) ) { }

        /**
         * @brief Constructor used in the ServerLibrary by keyvalue buffer at CBasePlayer
         */
        SteamID( const char* sid ) : steamID64( ToUint( sid ) ) { }

        /**
         * @brief Get the lower value of a steam ID
         */
        const uint32_t Y() const
        {
            return static_cast<uint32_t>( steamID64 & 0xFFFFFFFF );
        }

        /**
         * @brief Get the upper value of a steam ID
         */
        const uint32_t Z() const
        {
            return static_cast<uint32_t>( ( steamID64 >> 32 ) & 0xFFFFFFFF );
        }

        /**
         * @brief Steam ID in string format (STEAM_0:{}:{})
         */
        const std::string steamID() const
        {
            uint64_t authID = steamID64 - steamOffset;
            return fmt::format( "STEAM_0:{}:{}", std::to_string( authID % 2 ), std::to_string( static_cast<uint32_t>( authID / 2 ) ) );
        }

        /**
         * @brief converts steamID64 as-is to string
         */
        const std::string ToString() const
        {
            return std::to_string( steamID64 );
        }

        /**
         * @brief Steam profile url "https://steamcommunity.com/profiles/{steamID64}"
         */
        const std::string steamProfile() const
        {
            return fmt::format( "https://steamcommunity.com/profiles/{}", steamID64 );
        }

        /**
         * @brief Return whatever the given SteamID is equal in uin64 or string form.
         */
        bool StringEquals( std::string_view other ) const
        {
            return ( other == steamID() || other == ToString() );
        }

#ifndef CLIENT_DLL
        static CBasePlayer* FindPlayerBySteamID( uint64_t sid )
        {
            for( CBasePlayer* search : UTIL_FindPlayers() )
            {
                if( search != nullptr )
                {
                    if( SteamID* psid = search->GetSteamID(); psid != nullptr )
                    {
                        if( psid->steamID64 == sid )
                            return search;
                    }
                }
            }
            return nullptr;
        }
#endif

        bool operator==( const SteamID& other ) const {
            return steamID64 == other.steamID64;
        }
        bool operator!=( const SteamID& other ) const {
            return steamID64 != other.steamID64;
        }
        bool operator==( const SteamID* other ) const {
            return steamID64 == other->steamID64;
        }
        bool operator!=( const SteamID* other ) const {
            return steamID64 != other->steamID64;
        }
        bool operator==( const char* other ) const {
            return StringEquals( std::string_view( other ) );
        }
        bool operator!=( const char* other ) const {
            return !StringEquals( std::string_view( other ) );
        }
        bool operator==( const std::string& other ) const {
            return StringEquals( std::string_view( other ) );
        }
        bool operator!=( const std::string& other ) const {
            return !StringEquals( std::string_view( other ) );
        }
        bool operator==( const std::string_view other ) const {
            return StringEquals( other );
        }
        bool operator!=( const std::string_view other ) const {
            return !StringEquals( other );
        }
};

#ifdef CLIENT_DLL
inline std::shared_ptr<SteamID> ClientSteamIDPointer;

// Updated by CHud::MsgFunc_SteamID in client/ui/hud.cpp at the moment when the client joins in a server
inline uint64_t ClientSteamIDUint64;

// Get the steamID on the client side. Note: the client must join a server first or this will be nullptr
inline SteamID* GetClientSteamID()
{
    if( !ClientSteamIDUint64 )
    {
        return nullptr; // Do not try to create if is not initialized.
    }

    if( !ClientSteamIDPointer )
    {
        ClientSteamIDPointer = std::make_shared<SteamID>( std::to_string( ClientSteamIDUint64 ).c_str() );
    }

    return ClientSteamIDPointer.get();
}
#endif
