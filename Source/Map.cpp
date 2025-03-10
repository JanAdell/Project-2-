#include "Map.h"
#include "Application.h"
#include "Render.h"
#include "Input.h"
#include "Scene.h"
#include "PathfindingManager.h"
#include "Minimap.h"
#include "JuicyMath.h"
#include "Defs.h"
#include "Log.h"

#include "optick-1.3.0.0/include/optick.h"
#include "SDL/include/SDL_scancode.h"

#include <math.h>
#include <vector>

Map* Map::map = nullptr;
MapOrientation Map::type = MAPTYPE_UNKNOWN;
float Map::scale = 1.0f;
float Map::base_offset = 0.0f;
int Map::width = 0;
int Map::height = 0;
int Map::tile_width = 0;
int Map::tile_height = 0;
std::pair<int, int> Map::size_i = { 0,0 };
std::pair<float, float> Map::size_f = { 0.0f, 0.0f };

Map::Map()
{
	if (map == nullptr)
		map = this;
}

Map::~Map()
{
	if (loaded)
		CleanUp();

	if (map == this)
		map = nullptr;
}

bool Map::Load(const char* file)
{
	OPTICK_EVENT();

	if (loaded)
		CleanUp();

	path = file;

	pugi::xml_document doc;
	if (App->files.LoadXML(file, doc))
	{
		pugi::xml_node map_node = doc.child("map");
		if (map_node)
		{
			ParseHeader(map_node);

			LOG("Loading Map XML file: %s (width: %d, height: %d, tile_width: %d, tile_height: %d)",
				path.c_str(), width, height, tile_width, tile_height);

			if (ParseTilesets(map_node))
			{
				if (ParseLayers(map_node))
				{
					App->pathfinding.SetWalkabilityLayer(GetMapWalkabilityLayer());
					ParseObjectGroups(map_node);
					SetMapScale(scale);
					loaded = true;
				}
				else
					LOG("Error parsing map xml file: Could not load layers.");
			}
			else
				LOG("Error parsing map xml file: Could not load tilesets.");
		}
		else
			LOG("Error parsing map xml file: Cannot find 'map' tag.");
	}
	else
		LOG("ERROR: Could not load map xml file %s.", file);

	doc.reset();
	return loaded;
}

void Map::CleanUp()
{
	tilesets.clear();
	layers.clear();
	obj_groups.clear();
	loaded = false;
}

bool Map::IsValid() const
{
	return loaded;
}

void Map::Draw() const
{
	if (!loaded) return;

	int mouse_x, mouse_y;
	App->input->GetMousePosition(mouse_x, mouse_y);

	SDL_Rect cam = App->render->GetCameraRect();
	mouse_x += cam.x;
	mouse_y += cam.y;

	std::pair<int, int> up_left = I_WorldToMap(cam.x, cam.y);
	std::pair<int, int> down_right = I_WorldToMap(cam.x + cam.w, cam.y + cam.h);

	if (type == MAPTYPE_ORTHOGONAL)
	{
		for (std::vector<MapLayer>::const_iterator it = layers.begin(); it != layers.end(); ++it)
		{
			if (it->drawable)
			{
				for (int y = up_left.second; y <= down_right.second; ++y)
				{
					for (int x = up_left.first; x <= down_right.first; ++x)
					{
						unsigned int tile_id = it->GetID(x, y);
						int tex_id;
						SDL_Rect section;
						if (GetRectAndTexId(tile_id, section, tex_id))
						{
							std::pair<int, int> render_pos = I_MapToWorld(x, y);
							App->render->Blit(tex_id, render_pos.first, render_pos.second, &section, MAP);
						}
					}
				}
			}
		}
	}
	else if (type == MAPTYPE_ISOMETRIC)
	{
		// Get corner coordinates
		std::pair<int, int> up_right = I_WorldToMap(cam.x + cam.w, cam.y);
		std::pair<int, int> down_left = I_WorldToMap(cam.x, cam.y + cam.h);
		SDL_Rect cam_area = { cam.x - size_i.first, cam.y - (size_i.second * 2), cam.w +size_i.first, cam.h + (size_i.second * 2) };

		for (std::vector<MapLayer>::const_iterator it = layers.begin(); it != layers.end(); ++it)
		{
			if (it->drawable)
			{
				for (int y = up_right.second - 2; y <= down_left.second; ++y)
				{
					for (int x = up_left.first - 2; x <= down_right.first; ++x)
					{
						std::pair<int, int> render_pos = I_MapToWorld(x, y);
						if (JMath::PointInsideRect(render_pos.first, render_pos.second, cam_area))
						{
							int tex_id;
							SDL_Rect section;
							if (GetRectAndTexId(it->GetID(x, y), section, tex_id))
							{
								// Draw tileset spite at render_pos
								App->render->Blit(tex_id, render_pos.first, render_pos.second, &section, MAP);
							}
						}
					}
				}
			}
		}
	}
}

Map* Map::GetMap()
{
	return map;
}

const Map* Map::GetMapC()
{
	return map;
}

void Map::SwapMapType()
{
	type = (type == MAPTYPE_ISOMETRIC) ? MAPTYPE_ORTHOGONAL : MAPTYPE_ISOMETRIC;
}

void Map::SetMapScale(float s)
{
	scale = s;

	size_i = { int(float(tile_width) * scale), int(float(tile_height) * scale) };
	size_f = { float(tile_width) * scale, float(tile_height) * scale };

	base_offset = size_f.first / (2.0f * sin(60.0f * DEGTORAD));

	Event::Push(TRANSFORM_MODIFIED, App->scene->GetRoot(), vec(), vec(1.f));
}

bool Map::GetTilesetFromTileId(int id, TileSet& set) const
{
	bool ret = false;

	if (id >= tilesets.front().firstgid)
	{
		for (std::vector<TileSet>::const_iterator it = tilesets.begin(); it != tilesets.end(); ++it)
		{
			if (id <= it->firstgid + it->tilecount)
			{
				set = *it;
				ret = true;
				break;
			}
		}
	}

	return ret;
}

bool Map::GetRectAndTexId(int tile_id, SDL_Rect& section, int& text_id) const
{
	bool ret = false;

	if (tile_id >= tilesets.front().firstgid)
	{
		for (std::vector<TileSet>::const_iterator it = tilesets.begin(); it != tilesets.end(); ++it)
		{
			if (tile_id <= it->firstgid + it->tilecount || it->tilecount < 0)
			{
				int relative_id = tile_id - it->firstgid;
				section.w = it->tile_width;
				section.h = it->tile_height;
				section.x = it->margin + ((it->tile_width + it->spacing) * (relative_id % it->num_tiles_width));
				section.y = it->margin + ((it->tile_height + it->spacing) * (relative_id / it->num_tiles_width));
				text_id = it->texture_id;
				ret = true;
				break;
			}
		}
	}

	return ret;
}

const MapLayer& Map::GetMapWalkabilityLayer()
{
	for (std::vector<MapLayer>::const_iterator item = layers.cbegin(); item != layers.cend(); ++item)
		if (!item->drawable)
			return *item;

	return *layers.cbegin();
}

std::pair<int, int> Map::GetTileSize_I()
{
	return size_i;
}

std::pair<float, float> Map::GetTileSize_F()
{
	return size_f;
}

float Map::GetBaseOffset()
{
	return base_offset;
}

std::pair<int, int> Map::GetMapSize_I()
{
	return { width, height };
}

std::pair<float, float> Map::GetMapSize_F()
{
	return { float(width), float(height) };
}

std::pair<int, int> Map::I_MapToWorld(int x, int y, int z)
{
	switch (type)
	{
	case MAPTYPE_ISOMETRIC: return {
		(x - y) * size_i.first / 2,
		(x + y) * size_i.second / 2 };
	case MAPTYPE_ORTHOGONAL: return {
		x * size_i.first,
		y * size_i.second };
	default: return { x, y };
	}
}

std::pair<int, int> Map::I_WorldToMap(int x, int y)
{
	switch (type)
	{
	case MAPTYPE_ISOMETRIC: return {
		(x / size_i.first) + (y / size_i.second),
		(y / size_i.second) - (x / size_i.first) };
	case MAPTYPE_ORTHOGONAL: return {
		(float(x) / size_f.first < 0) ? (x / size_i.first) - 1 : x / size_i.first,
		(float(y) / size_f.second < 0) ? (y / size_i.second) - 1 : y / size_i.second };
	default: return { x, y };
	}
}

std::pair<float, float> Map::F_MapToWorld(float x, float y, float z)
{
	switch (type)
	{
	case MAPTYPE_ISOMETRIC: return {
		(x - y) * size_f.first * 0.5f,
		(x + y - z) * size_f.second * 0.5f };
	case MAPTYPE_ORTHOGONAL: return {
		x * size_f.first,
		y * size_f.second };
	default: return { x, y };
	}
}

std::pair<float, float> Map::F_MapToWorld(vec vec)
{
	return F_MapToWorld(vec.x, vec.y, vec.z);
}

std::pair<float, float> Map::F_WorldToMap(float x, float y)
{
	switch (type)
	{
	case MAPTYPE_ISOMETRIC: return {
		(x / size_f.first) + (y / size_f.second),
		(y / size_f.second) - (x / size_f.first) };
	case MAPTYPE_ORTHOGONAL: return {
		x / size_f.first,
		y / size_f.second };
	default: return { x, y };
	}
}

std::pair<int, int> Map::WorldToTileBase(float x, float y)
{
	std::pair<float, float> ret = F_WorldToMap(x, y);
	std::pair<float, float> tile_position = F_MapToWorld(ret.first, ret.second);

	if (JMath::PointInsideTriangle({ x , y - scale },
		{ tile_position.first, tile_position.second },
		{ tile_position.first + float(tile_width) * scale, tile_position.second },
		{ tile_position.first, tile_position.second + base_offset }))
		ret.first--;

	if (JMath::PointInsideTriangle({ x , y - scale },
		{ tile_position.first, tile_position.second },
		{ tile_position.first + float(tile_width) * scale, tile_position.second },
		{ tile_position.first + float(tile_width) * scale, tile_position.second + base_offset }))
		ret.second--;

	// Fix decimal clipping
	ret.first--;
	if (ret.first < 0.0f) ret.first--;
	if (ret.second < 0.0f) ret.second--;

	return { int(ret.first), int(ret.second) };
}

SDL_Texture* Map::GetFullMap(std::vector<std::pair<SDL_Rect, SDL_Rect>>& rects) const
{
	SDL_Texture* ret = nullptr;

	if (loaded)
	{
		int tex_id;
		int visible_count = 0;

		for (std::vector<MapLayer>::const_iterator it = layers.cbegin(); it != layers.cend(); ++it)
		{
			if (it->drawable)
			{
				for (int y = 0; y < width; ++y)
				{
					for (int x = 0; x < height; ++x)
					{
						std::pair<SDL_Rect, SDL_Rect> target;
						if (GetRectAndTexId(it->GetID(x, y), target.first, tex_id))
						{
							visible_count++;
							std::pair<float, float> render_pos = F_MapToWorld(float(x), float(y));
							target.second = { int(render_pos.first), int(render_pos.second), size_i.first, size_i.second };
							rects.push_back(target);
						}
					}
				}
			}
		}

		if (visible_count > 0)
			ret = App->tex.GetTexture(tex_id);
	}

	return ret;
}

void Map::ParseHeader(pugi::xml_node& node)
{
	width = node.attribute("width").as_int();
	height = node.attribute("height").as_int();
	tile_width = node.attribute("tilewidth").as_int();
	tile_height = node.attribute("tileheight").as_int();

	std::string orientation = node.attribute("orientation").as_string();

	if (orientation == "orthogonal") type = MAPTYPE_ORTHOGONAL;
	else if (orientation == "isometric") type = MAPTYPE_ISOMETRIC;
	else if (orientation == "staggered") type = MAPTYPE_STAGGERED;
	else type = MAPTYPE_UNKNOWN;
}

bool Map::ParseTilesets(pugi::xml_node& node)
{
	bool ret = true;

	// Load all tilesets info
	for (pugi::xml_node tileset_node = node.child("tileset"); tileset_node && ret; tileset_node = tileset_node.next_sibling("tileset"))
	{
		// Load Tileset Header
		TileSet tileset;
		tileset.firstgid = tileset_node.attribute("firstgid").as_int();
		tileset.name = tileset_node.attribute("name").as_string();
		tileset.tile_width = tileset_node.attribute("tilewidth").as_int();
		tileset.tile_height = tileset_node.attribute("tileheight").as_int();

		tileset.spacing = tileset_node.attribute("spacing").as_int();
		tileset.margin = tileset_node.attribute("margin").as_int();
		tileset.tilecount = tileset_node.attribute("tilecount").as_int(-1);
		//tileset.columns = tileset_node.attribute("columns").as_int();

		pugi::xml_node offset_node = tileset_node.child("tileoffset");
		tileset.offset_x = offset_node ? offset_node.attribute("x").as_int() : 0;
		tileset.offset_y = offset_node ? offset_node.attribute("y").as_int() : 0;

		// Load Tileset Image
		pugi::xml_node image_node = tileset_node.child("image");
		if (image_node)
		{
			std::string tex_path = "maps/";
			tex_path += image_node.attribute("source").as_string();
			tileset.texture_id = App->tex.Load(tex_path.c_str());

			TextureData tex_data;
			if (App->tex.GetTextureData(tileset.texture_id, tex_data))
			{
				// not needed as we query texture size on loading
				//tileset.tex_width = image_node.attribute("width").as_int();
				//tileset.tex_height = image_node.attribute("height").as_int();

				tileset.num_tiles_width = tex_data.width / tileset.tile_width;
				tileset.num_tiles_height = tex_data.height / tileset.tile_height;

				LOG("Loading tileset - %s - correctly!!", tileset.name.c_str());
				tilesets.push_back(tileset);
			}
			else
			{
				LOG("Error loading tileset texture: %s", tex_path.c_str());
				ret = false;
			}
		}
		else
		{
			LOG("Error parsing tileset xml file: Cannot find 'image' tag.");
			ret = false;
		}
	}

	return ret;
}

bool Map::ParseLayers(pugi::xml_node& node)
{
	bool ret = true;

	// Load layer info
	for (pugi::xml_node layer_node = node.child("layer"); layer_node && ret; layer_node = layer_node.next_sibling("layer"))
	{
		static MapLayer layer;
		layer.name = layer_node.attribute("name").as_string();
		layer.width = layer_node.attribute("width").as_int();
		layer.height = layer_node.attribute("height").as_int();

		// Load Layer Properties
		if (!layer.ParseProperties(layer_node.child("properties")))
			LOG("WARNING parsing map xml file: Cannot find 'layer/properties' tag.");

		// Load Layer Data
		if (layer.ParseData(layer_node.child("data")))
		{
			LOG("Loaded Layer Data - %s", layer.name.c_str());
			layers.push_back(layer);
		}
		else
		{
			LOG("Error parsing map xml file: Cannot find 'layer/data' tag.");
			ret = false;
		}
	}

	return ret;
}

void Map::ParseObjectGroups(pugi::xml_node& node)
{
	//Load object groups
	MapObjectGroup obj_group;
	for (pugi::xml_node obj_group_node = node.child("objectgroup"); obj_group_node; obj_group_node = obj_group_node.next_sibling("objectgroup"))
	{
		// Load Map Object Group
		obj_group.name = obj_group_node.attribute("name").as_string();
		//obj_group.color = obj_group_node.attribute("color").as_string();

		// Load objects
		for (pugi::xml_node object_node = obj_group_node.child("object"); object_node; object_node = object_node.next_sibling("object"))
		{
			MapObject obj;
			obj.id = object_node.attribute("id").as_uint();
			obj.x = object_node.attribute("x").as_float();
			obj.y = object_node.attribute("y").as_float();
			obj.width = object_node.attribute("width").as_float();
			obj.height = object_node.attribute("height").as_float();

			obj_group.objects.push_back(obj);
		}

		obj_groups.push_back(obj_group);
	}
}