#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <algorithm>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include "main.h"

typedef void (*fptr)(char*);
char* nullchar = (char*)malloc(1);

const int SELECT_ACCURACY = 20;

float randf(float min, float max) {
	return (((float)rand() / (float)RAND_MAX) * (max - min)) + min;
}

template <typename T>
float max(T a, T b) { return a > b ? a : b; }

enum ALIGN {
	LEFT,
	CENTER,
	RIGHT
};

enum UIRect {
	MAP_RECT,
	NEWS_TICKER_RECT,
	SHORTCUTS_RECT,
	MENU_RECT,
	INFO_RECT,
	UI_RECT_MAX
};

const SDL_Rect uiRects[ UI_RECT_MAX ] = {
	{48, 48, 720, 720},
	{0, 0, 1600, 48},
	{0, 48, 48, 720},
	{768, 48, 832, 852},
	{0, 768, 768, 132}
};

template <typename T>
struct v2 {
	T x, y;
};

const v2<float> nullv2 = { NULL, NULL };

float dist2d(v2<float> a, v2<float> b) {
	return sqrtf(powf(b.x - a.x, 2) + powf(b.y - a.y, 2));
}

struct button {
	v2<int> pos;
	fptr function;
	char* fArgs;
	SDL_Color border, fill, text;
	int xPad, yPad, textScale;
	std::string message;
};

struct {
	SDL_Window* window;
	SDL_Renderer* renderer;

	std::vector<SDL_Texture*> textures;
	std::vector<TTF_Font*> fonts;

	std::vector<button> buttons;

	struct {
		v2<float> pos;
		float zoom;
	} camera;

	int selectedMapItem[ 3 ];
	int selectedIndustry;
	v2<float> selectedPos;

	std::chrono::high_resolution_clock::time_point tickStart;

	int age;

	bool running;
} state;

enum TEXTURE_DICT {
	CITY = 2,
	DEBUG = 11
};

struct resourceAmount {
	int type;
	float amount;
};

struct resourceData {
	std::string name;
	int texture;
};

struct improvementData {
	std::string name;
	std::vector<resourceAmount> outputs;
	int maxWorkers, texture;
};

struct industryData {
	std::string name;
	std::vector<resourceAmount> inputs, outputs;
	int maxWorkers, texture;
};

const int RESOURCE_MAX = 34;
const int NATURAL_RESOURCE_MAX = 10;
const int IMPROVEMENT_MAX = 10;
const int INDUSTRY_MAX = 9;

int resourceMenuScroll = 0;
const int RESOURCE_MENU_MAX = 18;

resourceData resources[ RESOURCE_MAX ] = {
	{"Coal Ore", 5},
	{"Copper Ore", 9},
	{"Iron Ore", 18},
	{"Aluminium Ore", 1},
	{"Titanium Ore", 23},
	{"Tungsten Ore", DEBUG},
	{"Lithium Ore", DEBUG},
	{"Gold Ore", 15},
	{"Crude Oil", 10},
	{"Uranium Ore", 25},
	{"Coal", 3},
	{"Copper", 7},
	{"Iron", 16},
	{"Aluminium", DEBUG},
	{"Titanium", DEBUG},
	{"Tungsten", DEBUG},
	{"Lithium", DEBUG},
	{"Gold", 13},
	{"Gasoline", DEBUG},
	{"Diesel", DEBUG},
	{"Lubricant", DEBUG},
	{"Jet Fuel", DEBUG},
	{"Industrial Acid", DEBUG},
	{"Uranium", DEBUG},
	{"Enriched Uranium", DEBUG},
	{"Steel", 21},
	{"Mechanical Parts", DEBUG},
	{"Electrical Parts", 12},
	{"Aerospace Parts", DEBUG},
	{"Motor Parts", DEBUG},
	{"Water", DEBUG},
	{"Wood", DEBUG},
	{"Sulfur Ore", DEBUG},
	{"Sulfur", DEBUG}
};

improvementData improvements[ IMPROVEMENT_MAX ] = {
	{"Coal Mine", {{0, 20.0f}}, 100, 4},
	{"Copper Mine", {{1, 20.0f}}, 100, 8},
	{"Iron Mine", {{2, 20.0f}}, 100, 17},
	{"Aluminium Mine",{{3, 20.0f}}, 100, 0},
	{"Titanium Mine", {{4, 20.0f}}, 100, 22},
	{"Tungsten Mine", {{5, 20.0f}}, 100, DEBUG},
	{"Lithium Mine", {{6, 20.0f}}, 100, DEBUG},
	{"Gold Mine", {{7, 20.0f}}, 100, 14},
	{"Pumpjack", {{8, 20.0f}}, 100, 20},
	{"Uranium Mine", {{9, 20.0f}}, 100, 23}
};

industryData industries[ INDUSTRY_MAX ] = {
	{"Coal Plant", {{0, 10.0f}}, {{10, 8.0f}}, 250, 6},
	{"Copper Plant", {{1, 10.0f}}, {{11, 8.0f}}, 250, DEBUG},
	{"Iron Plant", {{2, 10.0f}}, {{12, 8.0f}}, 250, 19},
	{"Gold Plant", {{7, 10.0f}}, {{17, 8.0f}}, 250, DEBUG},
	{"Steel Mill", {{10, 10.0f}, {12, 10.0f}}, {{25, 8.0f}}, 250, DEBUG },
	{"Electronics Factory", {{11, 2.0f}, {17, 2.0f}}, {{27, 3.0f}}, 250, DEBUG },
	{"Oil Refinery", {{8, 10.0f}}, {{18, 4.0f}, {19, 3.0f}, {20, 2.0f}}, 250, DEBUG},
	{"Fuel Depot", {}, {}, 0, DEBUG},
	{"Motor Factory", {{25, 5.0f}, {13, 10.0f}}, {{29, 12.0f}}, 250, DEBUG}
};

struct improvement {
	v2<float> pos;
	int type, workers;
	std::vector<float> inventory;
};

struct industry {
	int type, workers;
	char* id;
};

struct city {
	std::string name;
	v2<float> pos;
	uint32_t population, employed;
	std::vector<industry> industries;
};

struct {
	std::vector<city> cities;
	std::vector<improvement> improvements;

	int money, stability;

	float resources[ RESOURCE_MAX ], deltaResources[ RESOURCE_MAX ];
} player;

struct resourceDeposit {
	v2<float> pos;
	int type;
};

struct {
	std::vector<resourceDeposit> resources;
} map;

v2<float> project(v2<float> p) {
	return { ((p.x - state.camera.pos.x) / state.camera.zoom) + 0.5f ,
	((p.y - state.camera.pos.y) / state.camera.zoom) + 0.5f };
}

v2<float> aproject(v2<float> p) {
	return { (state.camera.zoom * (p.x - 0.5f)) + state.camera.pos.x,
	(state.camera.zoom * (p.y - 0.5f)) + state.camera.pos.y };
}

int wa[ 6 ] = { 100, 10, 1, -1, -10, -100 };
char* cwa[ 6 ];

int rsp[ 2 ] = { 0, RESOURCE_MAX - RESOURCE_MENU_MAX };
char srsp[ 4 ] = { 'T', 'U', 'D', 'B' };
char* crsp[ 4 ];

char* ind = (char*)malloc(INDUSTRY_MAX * sizeof(int));

void init() {
	srand(time(0));
	SDL_Init(SDL_INIT_EVERYTHING);
	IMG_Init(IMG_INIT_PNG);
	TTF_Init();

	state.camera.pos = { 0.0f, 0.0f };
	state.camera.zoom = 1.0f;

	for (int i = 0; i < 3; i++) { state.selectedMapItem[ i ] = -1; }
	state.selectedIndustry = -1;
	state.selectedPos = nullv2;

	state.age = 0;

	for (int i = 0; i < RESOURCE_MAX; i++) {
		player.resources[ i ] = 0;
		player.deltaResources[ i ] = 0;
	}

	for (int i = 0; i < INDUSTRY_MAX; i++) {
		memcpy(ind + (i * sizeof(int)), &i, sizeof(int));
	}

	for (int i = 0; i < 6; i++) {
		cwa[ i ] = (char*)malloc(sizeof(int));
		memcpy(cwa[ i ], wa + i, sizeof(int));
	}

	for (int i = 0; i < 4; i++) {
		crsp[ i ] = (char*)malloc(sizeof(int));
	}

	memcpy(crsp[ 0 ], rsp, sizeof(int));
	memcpy(crsp[ 3 ], rsp + 1, sizeof(int));

	player.cities.push_back({ "Capitol", {0.0f, 0.0f}, 10000, 0, {} });
}

void startWindow() {
	state.window = SDL_CreateWindow("bob", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1600, 900, 0);
	state.renderer = SDL_CreateRenderer(state.window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
	state.running = true;
}

void loadTextures() {
	size_t fsize = 0;
	for (const auto& entry : std::filesystem::directory_iterator("./assets/textures")) {
		state.textures.push_back(IMG_LoadTexture(state.renderer, entry.path().string().c_str()));
		fsize += entry.file_size();
	}

	printf("Loaded %d textures totaling %d bytes.\n", state.textures.size(), fsize);
}

void loadFonts() {
	size_t fsize = 0;
	for (const auto& entry : std::filesystem::directory_iterator("./assets/fonts")) {
		state.fonts.push_back(TTF_OpenFont(entry.path().string().c_str(), 16));
		fsize += entry.file_size();
	}

	printf("Loaded %d fonts totaling %d bytes.\n", state.fonts.size(), fsize);
}

void generateMap() {
	for (int i = 0; i < 256; i++) {
		map.resources.push_back({ {randf(-500.0f, 500.0f), randf(-500.0f, 500.0f)}, rand() % NATURAL_RESOURCE_MAX });
	}
}

void startGame() {
	init();
	startWindow();
	loadTextures();
	loadFonts();
	generateMap();
	state.tickStart = std::chrono::high_resolution_clock::now();
	printf("Game started successfully.\n");
}

void drawTexture(uint32_t texID, v2<int> pos, int scale) {
	int rx, ry;
	SDL_QueryTexture(state.textures[ texID ], NULL, NULL, &rx, &ry);
	rx *= scale; ry *= scale;
	SDL_Rect r = { pos.x - (rx / 2), pos.y - (ry / 2), rx, ry };
	SDL_RenderCopy(state.renderer, state.textures[ texID ], NULL, &r);
}

void drawText(uint32_t fontIndex, std::string content, SDL_Color color, v2<int> pos, ALIGN align, int scale) {
	uint16_t* text = (uint16_t*)malloc((content.length() + 1) * 2);
	int i = 0;
	for (char& c : content) {
		*(text + i) = c;
		i++;
	}

	*(text + i) = 0x0000;

	SDL_Surface* surface = TTF_RenderUNICODE_Solid_Wrapped(state.fonts[ fontIndex ], text, color, 0);
	SDL_Texture* texture = SDL_CreateTextureFromSurface(state.renderer, surface);

	int rx, ry;
	SDL_QueryTexture(texture, NULL, NULL, &rx, &ry);
	SDL_Rect r;
	r.w = rx * scale; r.h = ry * scale;
	r.y = pos.y - (r.h / 2);

	switch (align) {
		case LEFT:
			r.x = pos.x;
			break;
		case CENTER:
			r.x = pos.x - (r.w / 2);
			break;
		case RIGHT:
			r.x = pos.x - r.w;
			break;
	}

	SDL_RenderCopy(state.renderer, texture, NULL, &r);

	free(text);
	SDL_FreeSurface(surface);
	SDL_DestroyTexture(texture);
}

void drawButton(int buttonID) {
	button i = state.buttons[ buttonID ];
	int w = (i.message.length() * i.textScale * 8) + (i.xPad * 2);
	int h = (i.textScale * 16) + (i.yPad * 2);

	SDL_Color c;
	SDL_Rect r = { i.pos.x, i.pos.y, w, h };

	c = i.fill;
	SDL_SetRenderDrawColor(state.renderer, c.r, c.g, c.b, c.a);
	SDL_RenderFillRect(state.renderer, &r);

	c = i.border;
	SDL_SetRenderDrawColor(state.renderer, c.r, c.g, c.b, c.a);
	SDL_RenderDrawRect(state.renderer, &r);

	drawText(0, i.message, i.text, { i.pos.x + i.xPad, i.pos.y + (h / 2) }, LEFT, i.textScale);
}

void drawUIRect(int part, SDL_Color color) {
	SDL_SetRenderDrawColor(state.renderer, color.r, color.g, color.b, color.a);
	SDL_RenderFillRect(state.renderer, uiRects + part);

	SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
	SDL_RenderDrawRect(state.renderer, uiRects + part);
}

void drawCountryInformation() {
	drawUIRect(INFO_RECT, { 0, 0, 0, 255 });
	drawText(0, "Population: ", { 255, 255, 255, 255 }, { 8, 792 }, LEFT, 2);
	drawText(0, "Money: ", { 255, 255, 255, 255 }, { 8, 832 }, LEFT, 2);
	drawText(0, "Stability: ", { 255, 255, 255, 255 }, { 8, 872 }, LEFT, 2);

	size_t p = 0;
	for (city& i : player.cities) {
		p += i.population;
	}

	drawText(0, std::to_string(p), { 255, 255, 255, 255 }, { 472, 792 }, RIGHT, 2);
	drawText(0, std::to_string(player.money), { 255, 255, 255, 255 }, { 472, 832 }, RIGHT, 2);
	drawText(0, std::to_string(player.stability), { 255, 255, 255, 255 }, { 472, 872 }, RIGHT, 2);
}

void drawNewsTicker() {
	drawUIRect(NEWS_TICKER_RECT, { 0, 0, 0, 255 });
	drawText(0, "Day " + std::to_string(int(state.age / 24)) + ", " + std::to_string(state.age % 24) + ":00", { 255, 255, 255, 255 }, { 8, 24 }, LEFT, 2);
	drawText(0, "Pre-Alpha Version 0.0.2", { 255, 255, 255, 255 }, { 1592, 24 }, RIGHT, 2);
}

void drawCities() {
	for (const city& i : player.cities) {
		v2<float> p = project(i.pos);
		v2<int> pm = { (p.x * 720) + 48 , 768 - (p.y * 720) };
		if (pm.x > 56 && pm.x < 760 && pm.y > 56 && pm.y < 760) {
			drawTexture(CITY, pm, 1);
		}
	}
}

void drawResources() {
	for (const resourceDeposit& i : map.resources) {
		v2<float> p = project(i.pos);
		v2<int> pm = { (p.x * 720) + 48 , 768 - (p.y * 720) };
		if (pm.x > 56 && pm.x < 760 && pm.y > 56 && pm.y < 760) {
			drawTexture(resources[ i.type ].texture, pm, 1);
		}
	}
}

void drawImprovements() {
	for (const improvement& i : player.improvements) {
		v2<float> p = project(i.pos);
		v2<int> pm = { (p.x * 720) + 48 , 768 - (p.y * 720) };
		if (pm.x > 56 && pm.x < 760 && pm.y > 56 && pm.y < 760) {
			drawTexture(improvements[ i.type ].texture, pm, 1);
		}
	}
}

void drawMap() {
	drawUIRect(MAP_RECT, { 0, 63, 0, 255 });
	drawCities();
	drawResources();
	drawImprovements();

	drawText(0, "Zoom: " + std::to_string(1.0f / state.camera.zoom), { 0, 0, 0, 255 }, { 408, 744 }, CENTER, 1);
	drawText(0, std::to_string(state.camera.pos.x) + " , " + std::to_string(state.camera.pos.y), { 0, 0, 0, 255 }, { 408, 756 }, CENTER, 1);
}

void closeWindow() {
	for (SDL_Texture*& i : state.textures) { SDL_DestroyTexture(i); }
	for (TTF_Font*& i : state.fonts) { TTF_CloseFont(i); }

	SDL_DestroyRenderer(state.renderer);
	SDL_DestroyWindow(state.window);

	TTF_Quit();
	IMG_Quit();
	SDL_Quit();

	free(nullchar);

	for (city& c : player.cities) {
		for (industry& i : c.industries) {
			free(i.id);
		}
	}

	printf("Game closed successfully.\n");
}

bool resourceMenuOpen = false;

void openMenuInit(std::string title) {
	drawUIRect(MENU_RECT, { 0, 0, 0, 255 });
	drawText(0, title, { 255, 255, 255, 255 }, { 1184, 80 }, CENTER, 3);

	if (state.buttons.size() > 5) {
		state.buttons.erase(state.buttons.begin() + 5, state.buttons.end());
	}

	resourceMenuOpen = false;
}

void openTechnologyMenu(char* args) {
	openMenuInit("Technology");
	SDL_RenderPresent(state.renderer);
}

void openEconomyMenu(char* args) {
	openMenuInit("Economy");
	SDL_RenderPresent(state.renderer);
}

void resourceMenuScrollRefreshMemory() {
	memset(crsp[ 1 ], 0, sizeof(int));
	rsp[ 1 ] = resourceMenuScroll - 1 < 0 ? 0 : resourceMenuScroll - 1;
	memcpy(crsp[ 1 ], rsp + 1, sizeof(int));

	memset(crsp[ 2 ], 0, sizeof(int));
	rsp[ 2 ] = resourceMenuScroll + 1 > (RESOURCE_MAX - RESOURCE_MENU_MAX) - 1 ? (RESOURCE_MAX - RESOURCE_MENU_MAX) : resourceMenuScroll + 1;
	memcpy(crsp[ 2 ], rsp + 2, sizeof(int));
}

void scrollResourceMenu(char* args) {
	int scrollPos;
	memcpy(&scrollPos, args, sizeof(int));
	resourceMenuScroll = scrollPos;
	resourceMenuScrollRefreshMemory();

	openResourcesMenu(nullchar);
}

void openResourcesMenu(char* args) {
	openMenuInit("Resources");
	resourceMenuOpen = true;

	SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
	SDL_RenderDrawLine(state.renderer, 768, 112, 1600, 112);
	SDL_RenderDrawLine(state.renderer, 768, 840, 1600, 840);
	SDL_RenderDrawLine(state.renderer, 1400, 112, 1400, 840);

	drawText(0, "Showing resources " +
		std::to_string(resourceMenuScroll) + " to " +
		std::to_string(resourceMenuScroll + RESOURCE_MENU_MAX),
		{ 255, 255, 255, 255 }, { 1592, 864 }, RIGHT, 2);

	int y = -1;
	for (int i = resourceMenuScroll; i < (resourceMenuScroll + RESOURCE_MENU_MAX); i++) {
		y++;
		drawTexture(resources[ i ].texture, { 800, (40 * y) + 136 }, 2);
		drawText(0, resources[ i ].name, { 255, 255, 255, 255 }, { 824, (40 * y) + 136 }, LEFT, 2);
		drawText(0, std::format("{:.1f}", player.resources[ i ]), { 255, 255, 255, 255 }, { 1392, (40 * y) + 136 }, RIGHT, 2);

		std::string dS; SDL_Color c;
		if (player.deltaResources[ i ] > 0) { dS = "+"; }

		if (player.deltaResources[ i ] > 1) {
			c = { 0, 255, 0, 255 };
		} else if (player.deltaResources[ i ] > -1) {
			c = { 255, 255, 0, 255 };
		} else {
			c = { 255, 0, 0, 255 };
		}

		dS += std::format("{:.1f}", player.deltaResources[ i ]);
		drawText(0, dS, c, { 1408, (40 * y) + 136 }, LEFT, 2);
	}

	for (int i = 0; i < 4; i++) {
		state.buttons.push_back({ {776 + (40 * i), 848}, scrollResourceMenu, crsp[ i ],
			{255, 255, 255, 255},
			{127, 127, 127, 255},
			{255, 255, 255, 255}, 8, 4, 2, std::string(1, srsp[ i ]) });
		drawButton(i + 5);
	}

	SDL_RenderPresent(state.renderer);
}

void openMilitaryMenu(char* args) {
	openMenuInit("Military");
	SDL_RenderPresent(state.renderer);
}

void openDiplomacyMenu(char* args) {
	openMenuInit("Diplomacy");
	SDL_RenderPresent(state.renderer);
}

void openCityMenu() {
	city c = player.cities[ state.selectedMapItem[ 0 ] ];
	openMenuInit(c.name);

	drawText(0, "Population: " + std::to_string(c.population), { 255, 255, 255, 255 }, { 1184, 128 }, CENTER, 2);

	std::vector<int> builtIndustries = {};
	for (int i = 0; i < c.industries.size(); i++) {
		state.buttons.push_back({ { 1192, (i * 56) + 176 }, openIndustryMenu, c.industries[ i ].id,
			{255, 255, 255, 255},
			{127, 127, 127, 255},
			{255, 255, 255, 255}, 8, 8, 2, "Open " + industries[ c.industries[ i ].type ].name });

		drawButton(i + 5);
		builtIndustries.push_back(c.industries[ i ].type);
	}

	int u = -1;
	for (int i = 0; i < INDUSTRY_MAX; i++) {
		if (std::find(builtIndustries.begin(), builtIndustries.end(), i) != builtIndustries.end()) {
			continue;
		}

		u++;
		state.buttons.push_back({ { 776, (u * 56) + 176 }, buildIndustry, ind + (i * sizeof(int)),
			{255, 255, 255, 255},
			{127, 127, 127, 255},
			{255, 255, 255, 255}, 8, 8, 2, "Build " + industries[ i ].name });

		drawButton(state.buttons.size() - 1);
	}

	SDL_RenderPresent(state.renderer);
}

void allocateWorkers(char* args) {
	int workerAmount;
	memcpy(&workerAmount, args, sizeof(int));

	if (state.selectedMapItem[ 1 ] != -1) {
		improvement i = player.improvements[ state.selectedMapItem[ 1 ] ];
		if (i.workers + workerAmount <= improvements[ i.type ].maxWorkers && i.workers + workerAmount >= 0) {
			player.improvements[ state.selectedMapItem[ 1 ] ].workers += workerAmount;
			openImprovementMenu();
		}
		return;
	}

	if (state.selectedIndustry != -1) {
		industry i = player.cities[ state.selectedMapItem[ 0 ] ].industries[ state.selectedIndustry ];
		if (i.workers + workerAmount <= industries[ i.type ].maxWorkers && i.workers + workerAmount >= 0) {
			player.cities[ state.selectedMapItem[ 0 ] ].industries[ state.selectedIndustry ].workers += workerAmount;
			openIndustryMenu(i.id);
		}
	}
}

void drawWorkerAllocationButtons() {
	for (int i = 0; i < 6; i++) {
		std::string s = wa[ i ] > 0 ? "+" : "";
		state.buttons.push_back({ {776, 156 + (48 * i)}, allocateWorkers, cwa[ i ],
			{255, 255, 255, 255},
			{127, 127, 127, 255},
			{255, 255, 255, 255}, 8, 4, 2, s + std::to_string(wa[ i ]) + " workers" });
		drawButton(i + 5);
	}
}

void openImprovementMenu() {
	improvement i = player.improvements[ state.selectedMapItem[ 1 ] ];
	openMenuInit(improvements[ i.type ].name);

	drawText(0, std::to_string(i.workers) + "/" + std::to_string(improvements[ i.type ].maxWorkers) + " workers", { 255, 255, 255, 255 }, { 776, 128 }, LEFT, 2);
	drawWorkerAllocationButtons();

	drawText(0, "Maximum Production:", { 255, 255, 255, 255 }, { 776, 492 }, LEFT, 2);
	int j = 0;
	for (resourceAmount& r : improvements[ i.type ].outputs) {
		drawTexture(resources[ r.type ].texture, { 800, 532 + (48 * j) }, 2);
		drawText(0, std::format("{:.2f} ", r.amount) + resources[ r.type ].name, { 255, 255, 255, 255 }, { 824, 532 + (48 * j) }, LEFT, 2);
		j++;
	}
}

void openIndustryMenu(char* args) {
	memcpy(&state.selectedIndustry, args, sizeof(int));
	industry i = player.cities[ state.selectedMapItem[ 0 ] ].industries[ state.selectedIndustry ];
	openMenuInit(industries[ i.type ].name);

	drawText(0, std::to_string(i.workers) + "/" + std::to_string(industries[ i.type ].maxWorkers) + " workers", { 255, 255, 255, 255 }, { 776, 128 }, LEFT, 2);
	drawWorkerAllocationButtons();

	SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
	SDL_RenderDrawLine(state.renderer, 768, 468, 1600, 468);

	drawText(0, "Maximum Usage:", { 255, 255, 255, 255 }, { 776, 492 }, LEFT, 2);
	int j = 0;
	for (resourceAmount& r : industries[ i.type ].inputs) {
		drawTexture(resources[ r.type ].texture, { 800, 532 + (48 * j) }, 2);
		drawText(0, std::format("{:.2f} ", r.amount) + resources[ r.type ].name, { 255, 255, 255, 255 }, { 824, 532 + (48 * j) }, LEFT, 2);
		j++;
	}

	drawText(0, "Maximum Production:", { 255, 255, 255, 255 }, { 1192, 492 }, LEFT, 2);
	int k = 0;
	for (resourceAmount& r : industries[ i.type ].outputs) {
		drawTexture(resources[ r.type ].texture, { 1216, 532 + (48 * k) }, 2);
		drawText(0, std::format("{:.2f} ", r.amount) + resources[ r.type ].name, { 255, 255, 255, 255 }, { 1240, 532 + (48 * k) }, LEFT, 2);
		k++;
	}
}

void buildImprovement(char* args) {
	int resourceID = state.selectedMapItem[ 2 ];
	player.improvements.push_back({ map.resources[ resourceID ].pos, map.resources[ resourceID ].type, 0, {0.0f} });
	map.resources.erase(map.resources.begin() + resourceID);
	state.selectedMapItem[ 2 ] = -1;
	state.selectedMapItem[ 1 ] = player.improvements.size() - 1;
	openImprovementMenu();
}

void buildIndustry(char* args) {
	int t = 0;
	memcpy(&t, args, sizeof(int));

	player.cities[ state.selectedMapItem[ 0 ] ].industries.push_back({ t, 0, (char*)malloc(sizeof(int)) });
	state.selectedIndustry = player.cities[ state.selectedMapItem[ 0 ] ].industries.size() - 1;
	memcpy(player.cities[ state.selectedMapItem[ 0 ] ].industries[ state.selectedIndustry ].id, &state.selectedIndustry, sizeof(int));

	openIndustryMenu(player.cities[ state.selectedMapItem[ 0 ] ].industries[ state.selectedIndustry ].id);
}

void openResourceDepositMenu() {
	resourceDeposit r = map.resources[ state.selectedMapItem[ 2 ] ];
	openMenuInit(resources[ r.type ].name);

	state.buttons.push_back({ {776, 112}, buildImprovement, nullchar,
		{255, 255, 255, 255},
		{127, 127, 127, 255},
		{255, 255, 255, 255}, 8, 8, 2, "Build Mine" });
	drawButton(5);
}

void buildCity(char* args) {
	//if (player.resources[8] >= 100.0f && player.resources[9] >= 100.0f) {
		//player.resources[8] -= 100.0f;
		//player.resources[9] -= 100.0f;
	player.cities.push_back({ "Settlement " + std::to_string(player.cities.size()), state.selectedPos, 0, 0, {} });
	state.selectedMapItem[ 0 ] = player.cities.size() - 1;
	openCityMenu();
	//}
}

void openLandMenu() {
	openMenuInit("Empty land");
	drawText(0, std::to_string(state.selectedPos.x) + " , " + std::to_string(state.selectedPos.y), { 255, 255, 255, 255 }, { 1184, 128 }, CENTER, 2);
	state.buttons.push_back({ {776, 112}, buildCity, nullchar,
		{255, 255, 255, 255},
		{127, 127, 127, 255},
		{255, 255, 255, 255}, 8, 8, 2, "Found City" });
	drawButton(5);
}

//returns -1 if no button is selected
int isInsideButton(int x, int y) {
	int i = 0;

	for (const button& b : state.buttons) {
		int w = (b.message.length() * b.textScale * 8) + (b.xPad * 2);
		int h = (b.textScale * 16) + (b.yPad * 2);

		if (x >= b.pos.x && y >= b.pos.y && x <= (b.pos.x + w) && y <= (b.pos.y + h)) { return i; }
		i++;
	}

	return -1;
}

struct selection {
	v2<float> pos;
	int type, ID;
};

void select(v2<int> m) {
	for (int i = 0; i < 3; i++) { state.selectedMapItem[ i ] = -1; }
	v2<float> mf = { (float)m.x, (float)(m.y) };
	std::vector<selection> selections;

	for (int i = 0; i < player.cities.size(); i++) { selections.push_back({ player.cities[ i ].pos, 0, i }); }
	for (int i = 0; i < player.improvements.size(); i++) { selections.push_back({ player.improvements[ i ].pos, 1, i }); }
	for (int i = 0; i < map.resources.size(); i++) { selections.push_back({ map.resources[ i ].pos, 2, i }); }

	for (selection& i : selections) {
		v2<float> p = project(i.pos);
		v2<float> pf = { float((p.x * 720) + 48) , float(768 - (p.y * 720)) };
		if (dist2d(mf, pf) < SELECT_ACCURACY) {
			state.selectedMapItem[ i.type ] = i.ID;
			return;
		}
	}
}

void tick() {

	for (int i = 0; i < RESOURCE_MAX; i++) {
		player.deltaResources[ i ] = 0;
	}

	state.tickStart = std::chrono::high_resolution_clock::now();
	for (city& c : player.cities) {
		c.population *= randf(1.0f, 1.001f);
		c.population += rand() % 11;

		player.money += (c.population * 30000) / (2 * 20 * 24 * 365);

		for (industry& i : c.industries) {
			int productionSuccess = 0;
			for (resourceAmount r : industries[ i.type ].inputs) {
				if (player.resources[ r.type ] >= (r.amount * ((float)i.workers / (float)industries[ i.type ].maxWorkers))) {
					productionSuccess++;
				}
			}

			if (productionSuccess == industries[ i.type ].inputs.size()) {
				for (resourceAmount r : industries[ i.type ].inputs) {
					player.deltaResources[ r.type ] -= r.amount * ((float)i.workers / (float)industries[ i.type ].maxWorkers);
				}

				for (resourceAmount r : industries[ i.type ].outputs) {
					player.deltaResources[ r.type ] += r.amount * ((float)i.workers / (float)industries[ i.type ].maxWorkers);
				}
			}
		}
	}

	for (improvement& i : player.improvements) {
		for (resourceAmount r : improvements[ i.type ].outputs) {
			player.deltaResources[ r.type ] += r.amount * ((float)i.workers / (float)improvements[ i.type ].maxWorkers);
		}
	}

	for (int i = 0; i < RESOURCE_MAX; i++) {
		player.resources[ i ] += player.deltaResources[ i ];
	}

	if (resourceMenuOpen) {
		openResourcesMenu(nullchar);
	}

	state.age++;

	drawCountryInformation();
	drawNewsTicker();
}

int main(int argc, char** argv) {
	startGame();

	drawUIRect(0, { 0, 63, 0, 255 });
	for (int i = 1; i < UI_RECT_MAX; i++) {
		drawUIRect(i, { 0, 0, 0, 255 });
	}

	state.buttons.push_back({ {4, 52}, openTechnologyMenu, nullchar, {255, 255, 255, 255}, {127, 127, 127, 255}, {255, 255, 255, 255}, 12, 4, 2, "T" });
	state.buttons.push_back({ {4, 96}, openEconomyMenu, nullchar, {255, 255, 255, 255}, {127, 127, 127, 255}, {255, 255, 255, 255}, 12, 4, 2, "E" });
	state.buttons.push_back({ {4, 140}, openResourcesMenu, nullchar, {255, 255, 255, 255}, {127, 127, 127, 255}, {255, 255, 255, 255}, 12, 4, 2, "R" });
	state.buttons.push_back({ {4, 184}, openMilitaryMenu, nullchar, {255, 255, 255, 255}, {127, 127, 127, 255}, {255, 255, 255, 255}, 12, 4, 2, "M" });
	state.buttons.push_back({ {4, 228}, openDiplomacyMenu, nullchar, {255, 255, 255, 255}, {127, 127, 127, 255}, {255, 255, 255, 255}, 12, 4, 2, "D" });

	drawMap();
	for (int i = 0; i < 5; i++) {
		drawButton(i);
	}

	drawCountryInformation();
	drawNewsTicker();

	SDL_RenderPresent(state.renderer);

	while (state.running) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
				case SDL_QUIT:
					state.running = false;
					break;
				case SDL_MOUSEBUTTONDOWN:
					int mx, my;
					SDL_GetMouseState(&mx, &my);

					fptr f; int i;
					i = isInsideButton(mx, my);
					if (i != -1) {
						f = state.buttons[ i ].function;
						f(state.buttons[ i ].fArgs);
					}

					if (mx > 48 && my > 48 && mx < 768 && my < 768) {
						select({ mx, my });
						if (state.selectedMapItem[ 0 ] != -1) {
							openCityMenu();
						} else if (state.selectedMapItem[ 1 ] != -1) {
							openImprovementMenu();
						} else if (state.selectedMapItem[ 2 ] != -1) {
							openResourceDepositMenu();
						} else {
							v2<int> m = { mx - 48, my - 48 };
							v2<float> mf = { (float)m.x / 720.0f, (720.0f - (float)m.y) / 720.0f };
							state.selectedPos = aproject(mf);
							resourceMenuOpen = false;
							openLandMenu();
						}
					}

					break;
			}
		}

		const uint8_t* keystate = SDL_GetKeyboardState(NULL);

		if (keystate[ 20 ]) { state.camera.zoom *= 1.05; }
		if (keystate[ 8 ]) { state.camera.zoom *= 0.95; }
		if (keystate[ 7 ]) { state.camera.pos.x += (0.01 * state.camera.zoom); }
		if (keystate[ 4 ]) { state.camera.pos.x -= (0.01 * state.camera.zoom); }
		if (keystate[ 26 ]) { state.camera.pos.y += (0.01 * state.camera.zoom); }
		if (keystate[ 22 ]) { state.camera.pos.y -= (0.01 * state.camera.zoom); }

		if (resourceMenuOpen) {
			if (keystate[ 82 ]) {
				if (resourceMenuScroll != 0) {
					resourceMenuScroll--;
					resourceMenuScrollRefreshMemory();
					openResourcesMenu(nullchar);
				}
			}

			if (keystate[ 81 ]) {
				if (resourceMenuScroll + RESOURCE_MENU_MAX != RESOURCE_MAX) {
					resourceMenuScroll++;
					resourceMenuScrollRefreshMemory();
					openResourcesMenu(nullchar);
				}
			}
		}

		drawMap();

		std::chrono::duration<double, std::milli> span = std::chrono::high_resolution_clock::now() - state.tickStart;
		if (span > (std::chrono::duration<double, std::milli>)1000) {
			tick();
		}

		SDL_RenderPresent(state.renderer);
	}

	closeWindow();
	return 0;
}