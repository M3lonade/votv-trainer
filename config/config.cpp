#include "config/config.hpp"

namespace votv::config {

TrainerConfig& current()
{
    static TrainerConfig cfg;
    return cfg;
}

} // namespace votv::config
