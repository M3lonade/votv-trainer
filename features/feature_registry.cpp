#include "features/features.hpp"

namespace votv::features {

void tick_all()
{
    tick_player_features();
    tick_vehicle_features();
}

} // namespace votv::features
