#include <iostream>
#include <nlohmann/json.hpp>
#include <type_traits>

using json = nlohmann::json;
using namespace std;

// Clase personalizada que hereda de nlohmann::json
class dictionary : public json
{
public:
    // Método get con valor predeterminado y detección de tipo
    template <typename T>
    T get( const std::string& key, const T& default_value ) const
    {
        if (this->contains(key))
        {
            try
            {
                if constexpr (std::is_same<T, std::string>::value)
                {
                    if (this->at(key).is_string())
                    {
                        return this->at(key).get<std::string>();
                    }
                }
                else if constexpr (std::is_integral<T>::value)
                {
                    if (this->at(key).is_number())
                    {
                        return this->at(key).get<int>();
                    }
                }
            }
            catch (const json::exception& e)
            {
                std::cerr << "Error al obtener el valor: " << e.what() << std::endl;
            }
        }

        return default_value;
    }
};
