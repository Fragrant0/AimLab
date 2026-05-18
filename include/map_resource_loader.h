#ifndef MAP_RESOURCE_LOADER_H
#define MAP_RESOURCE_LOADER_H

#include <map>
#include <memory>
#include <string>

#include "map_manager.h"
#include "model.h"

class MapResourceLoader
{
public:
    using ModelMap = std::map<std::string, std::unique_ptr<Model>>;

    void Load(const MapConfig& map, ModelMap& propModels);
    void Unload(const MapConfig& map, ModelMap& propModels);
};

#endif
