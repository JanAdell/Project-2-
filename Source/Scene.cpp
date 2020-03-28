#include "Scene.h"
#include "Application.h"
#include "Input.h"
#include "Audio.h"
#include "Window.h"
#include "Render.h"
#include "Map.h"
#include "Editor.h"
#include "TimeManager.h"
#include "TextureManager.h"
#include "Sprite.h"
#include "Behaviour.h"
#include "AudioSource.h"
#include "Defs.h"
#include "Log.h"
#include "PathfindingManager.h"
#include "Point.h"

#include "SDL/include/SDL_scancode.h"
#include "optick-1.3.0.0/include/optick.h"

#include <queue>
#include <vector>

Scene::Scene() : Module("scene")
{
	root.SetName("______root______");
}

Scene::~Scene()
{}

bool Scene::PreUpdate()
{
	root.PreUpdate();

	return true;
}

bool Scene::Update()
{
	bool ret = true;

	OPTICK_EVENT();

	// Load sample maps
	if (App->input->GetKey(SDL_SCANCODE_1) == KEY_DOWN)
	{
		ret = map.Load("maps/iso.tmx");
	}
	else if (App->input->GetKey(SDL_SCANCODE_2) == KEY_DOWN)
	{
		ret = map.Load("maps/level1.tmx");
	}

	if (App->input->GetKey(SDL_SCANCODE_F2) == KEY_DOWN)
		map.draw_walkability = !map.draw_walkability;

	if (App->input->GetKey(SDL_SCANCODE_F3) == KEY_DOWN)
		App->audio->PauseMusic();

	if (App->input->GetKey(SDL_SCANCODE_F4) == KEY_DOWN)
		App->audio->PlayMusic("audio/Music/alexander-nakarada-buzzkiller.ogg");

	// Swap map orientation
	if (App->input->GetKey(SDL_SCANCODE_SPACE) == KEY_DOWN) Map::SwapMapType();

	// Update gameobject hierarchy
	
	root.Update();

	int x, y;
	App->input->GetMousePosition(x, y);
	SDL_Rect cam = App->render->GetCameraRect();

	if (App->input->GetKey(SDL_SCANCODE_3) == KEY_DOWN)
	{
		std::pair<int, int> position = Map::WorldToTileBase(float(x + cam.x), float(y + cam.y));

		Gameobject* audio_go = AddGameobject("AudioSource - son of root", &root);
		audio_go->GetTransform()->SetLocalPos({ float(position.first), float(position.second), 0.0f });
		audio_go->GetTransform()->GetGlobalPosition();
		new AudioSource(audio_go, App->audio->LoadFx("audio/Effects/Buildings/Nails/HamerNail13Hits.wav"));

		Sprite* s3 = new Sprite(audio_go);
		s3->tex_id = id_mouse_tex;
		s3->section = { 128, 0, 64, 64 };
	}

	if (App->input->GetKey(SDL_SCANCODE_4) == KEY_DOWN)
	{
		std::pair<int, int> position = Map::WorldToTileBase(float(x + cam.x), float(y + cam.y));

		Gameobject* unit_go = AddGameobject("Game Unit - son of root", &root);
		unit_go->GetTransform()->SetLocalPos({ float(position.first), float(position.second), 0.0f });

		new B_Movable(unit_go);

		Sprite* s3 = new Sprite(unit_go);
		s3->tex_id = id_mouse_tex;
		s3->section = { 64, 0, 64, 64 };

	}

	/*if (App->input->GetKey(SDL_SCANCODE_Z) == KEY_DOWN)
	{		
		std::pair<int, int> mouseOnMap = Map::WorldToTileBase(x + cam.x, y + cam.y);
		startPath = iPoint(mouseOnMap.first, mouseOnMap.second);
		path = App->pathfinding.CreatePath(startPath, destinationPath);
	}*/

	if (App->input->GetKey(SDL_SCANCODE_X) == KEY_DOWN)
	{
		std::pair<int, int> mouseOnMap = Map::WorldToTileBase(x + cam.x, y + cam.y);
		destinationPath = iPoint(mouseOnMap.first, mouseOnMap.second);
		path = App->pathfinding.CreatePath(startPath, destinationPath,0);
		if (path != nullptr) LOG("Path length: %d", path->size());
	}

	if (App->input->GetKey(SDL_SCANCODE_C) == KEY_DOWN)
	{
		std::pair<int, int> mouseOnMap = Map::WorldToTileBase(x + cam.x, y + cam.y);
		destinationPath = iPoint(mouseOnMap.first, mouseOnMap.second);
		path = App->pathfinding.CreatePath(startPath, destinationPath,1);
		if (path != nullptr) LOG("Path length: %d", path->size());
	}

	if (App->input->GetKey(SDL_SCANCODE_V) == KEY_DOWN)
	{
		std::pair<int, int> mouseOnMap = Map::WorldToTileBase(x + cam.x, y + cam.y);
		destinationPath = iPoint(mouseOnMap.first, mouseOnMap.second);
		path = App->pathfinding.CreatePath(startPath, destinationPath,2);
		if(path != nullptr) LOG("Path length: %d", path->size());
	}

	if (App->input->GetKey(SDL_SCANCODE_Z) == KEY_DOWN)
	{
		App->pathfinding.DebugShowPaths();
	}

	if (App->input->GetMouseButtonDown(2) == KEY_DOWN)
	{
		Gameobject* go = App->editor->selection;

		std::pair<int, int> mouseOnMap = Map::WorldToTileBase(x + cam.x, y + cam.y);


		if (go != nullptr) {
			LOG("Object selected");
			Event::Push(ON_RIGHT_CLICK, go, mouseOnMap.first, mouseOnMap.second);
			LOG("Event triggered");
		}
	}

	return ret;
}

bool Scene::PostUpdate()
{
	root.PostUpdate();

	map.Draw();

	SDL_Rect cam_rect = App->render->GetCameraRect();
	SDL_Rect rect = { 0, 0, 64, 64 };

	std::pair<int, int> render_pos = Map::I_MapToWorld(startPath.x, startPath.y);
	App->render->Blit(id_mouse_tex, render_pos.first, render_pos.second, &rect, DEBUG_MAP);

	if (path != nullptr && !path->empty())
	{
		for (std::vector<iPoint>::const_iterator it = path->cbegin(); it != path->cend(); ++it)
		{
			std::pair<int, int> render_pos = Map::I_MapToWorld(it->x, it->y);
			App->render->Blit(id_mouse_tex, render_pos.first, render_pos.second, &rect, DEBUG_MAP);
		}
	}

	// Debug Pointer Info on Window Title
	int mouse_x, mouse_y;
	App->input->GetMousePosition(mouse_x, mouse_y);
	std::pair<int, int> map_coordinates = Map::WorldToTileBase(cam_rect.x + mouse_x, cam_rect.y + mouse_y);

	// Editor Selection
	Gameobject* sel = App->editor->selection;

	// Log onto window title
	static char tmp_str[220];
	sprintf_s(tmp_str, 220, "FPS: %d, Zoom: %0.2f, Mouse: %dx%d, Tile: %dx%d, Selection: %s",
		App->time.GetLastFPS(),
		App->render->GetZoom(),
		mouse_x, mouse_y,
		map_coordinates.first, map_coordinates.second,
		sel != nullptr ? sel->GetName() : "none selected");

	App->win->SetTitle(tmp_str);

	return true;
}

bool Scene::CleanUp()
{
	LOG("Freeing scene");
	
	return true;
}

void Scene::RecieveEvent(const Event& e)
{
	switch (e.type)
	{
	case SCENE_PLAY:
		Event::Push(ON_PLAY, &root, e.data1);
		break;
	case SCENE_PAUSE:
		Event::Push(ON_PAUSE, &root, e.data1);
		break;
	case SCENE_STOP:
		Event::Push(ON_STOP, &root, e.data1);
		break;
	default:
		break;
	}
}

bool Scene::LoadTestScene()
{
	// Play sample track
	bool ret = App->audio->PlayMusic("audio/Music/alexander-nakarada-buzzkiller.ogg");

	// Remove fps cap
	App->time.SetMaxFPS(60);

	// Load mouse debug texture for identifying tiles
	if (ret)
	{
		id_mouse_tex = App->tex.Load("textures/meta.png");
		ret = (id_mouse_tex != -1);
	}

	if (ret)
	{
		// Run test content
		Gameobject* go1 = AddGameobject("g1 - son of root", &root);
		Gameobject* go2 = AddGameobject("g2 - son of g1", go1);

		Sprite* s1 = new Sprite(go1);
		s1->tex_id = id_mouse_tex;
		s1->section = { 0, 0, 64, 64 };
		
		Sprite* s2 = new Sprite(go2);
		s2->tex_id = id_mouse_tex;
		s2->section = { 64, 0, 64, 64 };

		//B_Unit* b1 = new B_Unit(go1);
		
		B_Unit* b2 = new B_Unit(go2);
	}

	return ret;
}

Gameobject* Scene::GetRoot()
{
	return &root;
}

const Gameobject* Scene::GetRoot() const
{
	return &root;
}

Gameobject * Scene::AddGameobject(const char * name, Gameobject * parent)
{
	return new Gameobject(name, parent != nullptr ? parent : &root);
}

Gameobject* Scene::MouseClickSelect(int mouse_x, int mouse_y)
{
	Gameobject* ret = nullptr;
	std::queue<Gameobject*> queue;
	std::vector<Gameobject*> root_childs = root.GetChilds();
	
	for (std::vector<Gameobject*>::iterator it = root_childs.begin(); it != root_childs.end(); ++it)
		queue.push(*it);

	if (!queue.empty())
	{
		SDL_Rect cam_rect = App->render->GetCameraRect();
		std::pair<float, float> map_coordinates = Map::WorldToTileBase(cam_rect.x + mouse_x, cam_rect.y + mouse_y);

		while (!queue.empty())
		{
			// TODO: Quadtree
			Gameobject* go = queue.front();
			if (go != nullptr)
			{
				// Get transform
				Transform* t = go->GetTransform();
				if (t != nullptr)
				{
					// Check intersection
					if (t->Intersects(map_coordinates))
						ret = go;

					// Push childs
					std::vector<Gameobject*> go_childs = go->GetChilds();
					for (std::vector<Gameobject*>::iterator it = go_childs.begin(); it != go_childs.end(); ++it)
						queue.push(*it);
				}
			}

			queue.pop();
		}
	}

	if (ret != nullptr)
		Event::Push(ON_SELECT, ret);

	return ret;
}