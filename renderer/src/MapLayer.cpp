#include "MapLayer.h"

MapLayer::MapLayer()
    : m_config()
    , m_mapData()
    , m_tileset(nullptr)
{
}

void MapLayer::SetConfig(const MapLayerConfig& config)
{
    m_config = config;
}

const MapLayerConfig& MapLayer::GetConfig() const
{
    return m_config;
}

void MapLayer::SetMapData(const MapData& mapData)
{
    m_mapData = mapData;
}

const MapData& MapLayer::GetMapData() const
{
    return m_mapData;
}

void MapLayer::SetTileset(const Tileset* tileset)
{
    m_tileset = tileset;
}

const Tileset* MapLayer::GetTileset() const
{
    return m_tileset;
}
