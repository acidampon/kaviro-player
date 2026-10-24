#pragma once
#include "ump/PlatformServices.h"
#include <memory>
namespace ump { std::unique_ptr<PlatformServices> createPlatformServices(); }
