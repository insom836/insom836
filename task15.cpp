#include "raylib.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>
#include <fstream>  // 用于存档的文件流
#include <cstdio>   // 用于字符串格式化读取

// ========================================================
// === 引入多线程与异步任务所需的标准库 (陈老师 PPT 要求) ===
// ========================================================
#include <thread>
#include <future>
#include <mutex>
#include <chrono>

// ========================================================
// === 引入加分项所需的缓存与函数包装库 (体现硬核架构) ===
// ========================================================
#include <unordered_map>
#include <functional>

// ========================================================
// === 【加分项 2】: 线程安全的资源缓存 (TextureCache) ===
// ========================================================
class TextureCache 
{
private:
    std::unordered_map<std::string, Texture2D> cache;
    std::mutex mtx;
    
    TextureCache() {} 

public:
    static TextureCache& getInstance() 
    {
        static TextureCache instance;
        return instance;
    }

    void put(const std::string& key, Texture2D tex) 
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (cache.find(key) != cache.end()) 
        {
            UnloadTexture(cache[key]);
        }
        cache[key] = tex;
    }

    bool has(const std::string& key) 
    {
        std::lock_guard<std::mutex> lock(mtx);
        return cache.find(key) != cache.end();
    }

    Texture2D get(const std::string& key) 
    {
        std::lock_guard<std::mutex> lock(mtx);
        return cache[key];
    }

    void clear() 
    {
        std::lock_guard<std::mutex> lock(mtx);
        for (auto& pair : cache) 
        {
            UnloadTexture(pair.second);
        }
        cache.clear();
    }
};

// ========================================================
// === 【作业要求 1】: 工具类 ScoreCalculator 与函数重载 ===
// ========================================================
class ScoreCalculator 
{
public:
    int CalculateScore(int type) 
    {
        switch (type) 
        {
            case 1: return 10;   
            case 2: return 20;   
            case 3: return -5;   
            default: return 0;
        }
    }

    int CalculateScore(int type, int combo) 
    {
        int baseScore = CalculateScore(type);
        return baseScore + combo * 2;
    }
};

ScoreCalculator globalScoreCalc; 

// ========================================================
// === 【作业要求 2】: 类的继承、虚函数与多态演示区块 ===
// ========================================================
namespace OOP_Task 
{
    class GameObject 
    {
    public:
        virtual void Update() = 0;
        virtual void Draw() = 0;
        virtual ~GameObject() { std::cout << "  [销毁] GameObject 基类析构" << std::endl; }
    };

    class Ball : public GameObject 
    {
    public:
        void Update() override { std::cout << "  [Ball] Update: 计算物理位移" << std::endl; }
        void Draw() override { std::cout << "  [Ball] Draw: 渲染球体" << std::endl; }
        ~Ball() override {}
    };

    class Paddle : public GameObject 
    {
    public:
        void Update() override { std::cout << "  [Paddle] Update: 检测键盘输入" << std::endl; }
        void Draw() override { std::cout << "  [Paddle] Draw: 渲染挡板" << std::endl; }
        ~Paddle() override {}
    };

    class Brick : public GameObject 
    {
    public:
        void Update() override { std::cout << "  [Brick] Update: 状态检查" << std::endl; }
        void Draw() override { std::cout << "  [Brick] Draw: 渲染彩色矩形" << std::endl; }
        ~Brick() override {}
    };
}

// ========================================================
// === 游戏核心代码 ===
// ========================================================

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

const int BRICK_WIDTH = 50;
const int BRICK_HEIGHT = 20;
const int BALL_RADIUS = 8;
const int POWERUP_RADIUS = 20;

const int INITIAL_PADDLE_WIDTH = 102;
const int PADDLE_HEIGHT = 20;
const float MAX_PADDLE_WIDTH = INITIAL_PADDLE_WIDTH * 2.0f; 

const Color BRICK_COLORS[] = { RED, ORANGE, YELLOW, GREEN, BLUE, PURPLE, PINK, LIGHTGRAY };

struct Ball { Vector2 pos; Vector2 speed; float radius; bool active; };
struct Paddle { Rectangle rec; Color color; };
struct Brick { Rectangle rec; Color color; Color originalColor; bool active; };

struct Particle 
{ 
    Vector2 position; 
    Vector2 speed; 
    Color color; 
    float alpha; 
    bool active; 
};

const int MAX_PARTICLES = 300;

class ParticlePool 
{
private:
    Particle particles[MAX_PARTICLES];
    bool active[MAX_PARTICLES];

public:
    ParticlePool() { std::fill(std::begin(active), std::end(active), false); }

    void spawn(Vector2 pos, Vector2 vel, Color col) 
    {
        for (int i = 0; i < MAX_PARTICLES; i++) 
        {
            if (!active[i]) 
            {
                particles[i] = { pos, vel, col, 1.0f, true };
                active[i] = true;
                break;
            }
        }
    }

    void update(float dtScale) 
    {
        for (int i = 0; i < MAX_PARTICLES; i++) 
        {
            if (active[i]) 
            {
                particles[i].position.x += particles[i].speed.x * dtScale;
                particles[i].position.y += particles[i].speed.y * dtScale;
                particles[i].alpha -= 0.02f * dtScale;
                if (particles[i].alpha <= 0) active[i] = false; 
            }
        }
    }

    void draw() 
    {
        for (int i = 0; i < MAX_PARTICLES; i++) 
        {
            if (active[i]) 
            {
                DrawRectangleV(particles[i].position, { 3, 3 }, Fade(particles[i].color, particles[i].alpha));
            }
        }
    }

    int getActiveCount() 
    {
        int count = 0;
        for (int i = 0; i < MAX_PARTICLES; i++) { if (active[i]) count++; }
        return count;
    }
    
    void clear() { std::fill(std::begin(active), std::end(active), false); }
};

ParticlePool globalParticlePool;
struct PowerUp { Vector2 pos; float speed; bool active; };

std::vector<Ball> balls;
::Paddle paddle; 
std::vector<::Brick> bricks;
std::vector<PowerUp> powerUps;

int score = 0;
int lives = 3; 
int bricksHit = 0; 
bool gameOver = false;
bool gameStarted = false;
bool inMenu = true;
int currentLevel = 0; 
float powerUpCooldown = 0.0f; 

enum class LoadState { IDLE, LOADING, DONE };
LoadState loadState = LoadState::IDLE;
std::future<Image> loadFuture; 
std::mutex loadMtx; 
float colorChangeTimer = 0.0f; 

float shakeDuration = 0.0f;
float shakeMagnitude = 0.0f;

Music bgm;
Sound hitPaddleFx;
Sound hitBrickFx;
Sound catchStarFx;  
Sound starDropFx;   

// ========================================================
// === 存档持久化系统 ===
// ========================================================
void SaveProgress()
{
    std::ofstream file("save.json");
    if (file.is_open())
    {
        // 写入极其轻量级的 JSON 配置结构
        file << "{\n";
        file << "  \"score\": " << score << ",\n";
        file << "  \"lives\": " << lives << ",\n";
        file << "  \"currentLevel\": " << currentLevel << "\n";
        file << "}\n";
        file.close();
        std::cout << "  [存档] 进度已成功保存至硬盘。" << std::endl;
    }
}

bool LoadProgress()
{
    std::ifstream file("save.json");
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line))
    {
        if (line.find("\"score\"") != std::string::npos) sscanf(line.c_str(), "  \"score\": %d,", &score);
        else if (line.find("\"lives\"") != std::string::npos) sscanf(line.c_str(), "  \"lives\": %d,", &lives);
        else if (line.find("\"currentLevel\"") != std::string::npos) sscanf(line.c_str(), "  \"currentLevel\": %d", &currentLevel);
    }
    file.close();
    std::cout << "  [读档] 进度恢复成功！" << std::endl;
    return true;
}

bool CheckSaveExists()
{
    std::ifstream file("save.json");
    return file.is_open();
}

void DeleteSave()
{
    std::remove("save.json");
}

void DrawGeminiStar(Vector2 pos, float size, float alpha)
{
    Color baseColor = { 74, 144, 226, 255 }; 
    Color color = Fade(baseColor, alpha);
    float inner = size * 0.35f;

    DrawTriangle({pos.x - inner, pos.y}, {pos.x, pos.y - size}, {pos.x + inner, pos.y}, color);
    DrawTriangle({pos.x - inner, pos.y}, {pos.x + inner, pos.y}, {pos.x, pos.y + size}, color);
    DrawTriangle({pos.x, pos.y - inner}, {pos.x - size, pos.y}, {pos.x, pos.y + inner}, color);
    DrawTriangle({pos.x, pos.y - inner}, {pos.x, pos.y + inner}, {pos.x + size, pos.y}, color);

    Color highlight = Fade(RAYWHITE, alpha);
    float inner2 = inner * 0.5f;
    float size2 = size * 0.6f;
    
    DrawTriangle({pos.x - inner2, pos.y}, {pos.x, pos.y - size2}, {pos.x + inner2, pos.y}, highlight);
    DrawTriangle({pos.x - inner2, pos.y}, {pos.x + inner2, pos.y}, {pos.x, pos.y + size2}, highlight);
    DrawTriangle({pos.x, pos.y - inner2}, {pos.x - size2, pos.y}, {pos.x, pos.y + inner2}, highlight);
    DrawTriangle({pos.x, pos.y - inner2}, {pos.x, pos.y + inner2}, {pos.x + size2, pos.y}, highlight);
}

void TriggerShake(float duration, float magnitude)
{
    shakeDuration = duration;
    shakeMagnitude = magnitude;
}

void CreateExplosion(Vector2 pos, Color color)
{
    for (int i = 0; i < 20; i++)
    {
        Vector2 speed = { (float)GetRandomValue(-200, 200) / 40.0f, (float)GetRandomValue(-200, 200) / 40.0f };
        globalParticlePool.spawn(pos, speed, color);
    }
}

int GetActiveBrickCount()
{
    int activeCount = 0;
    for (const auto &b : bricks) { if (b.active) activeCount++; }
    return activeCount;
}

// 修改点：增加 isNewGame 参数，以区分是全新开局还是加载/过关
void InitGame(int level, bool isNewGame = true)
{
    if (isNewGame)
    {
        lives = 3; 
        score = 0;
    }
    
    paddle.rec = { (float)(SCREEN_WIDTH - INITIAL_PADDLE_WIDTH) / 2, (float)SCREEN_HEIGHT - 50, (float)INITIAL_PADDLE_WIDTH, (float)PADDLE_HEIGHT };
    paddle.color = BLUE;

    balls.clear();
    balls.push_back({ {SCREEN_WIDTH / 2.0f, paddle.rec.y - BALL_RADIUS - 5}, {0, 0}, (float)BALL_RADIUS, true });

    std::vector<std::string> map;
    switch (level) 
    {
        case 0: map = { "111111" }; break;
        case 1: map = { 
            "11111111111111", "11111111111111", "11111111111111", "11111111111111", 
            "11111111111111", "11111111111111", "11111111111111", "11111111111111" }; break;
        case 2: map = { 
            "..1111..1111..", ".111111111111.", "11111111111111", "11111111111111", 
            "11111111111111", ".111111111111.", "..1111111111..", "...11111111...", 
            "....111111....", ".....1111.....", "......11......" }; break;
        case 3: map = { 
            "......11......", ".....1..1.....", ".....1..1.....", "....1....1....", 
            "....1....1....", "1111......1111", ".1..........1.", "..1........1..", 
            "...1......1...", "....1....1....", "...1..11..1...", "..1..1..1..1..", 
            ".1..1....1..1.", "1111......1111" }; break;
        case 4: map = { 
            "...111..111...", "..1111111111..", "..1111..1111..", "...11....11...", 
            "1111..11..1111", "11111111111111", "1111..11..1111", "...11....11...", 
            "..1111..1111..", "..1111111111..", "...111..111...", "......11......" }; break;
        case 5: map = { 
            "......11......", ".....1..1.....", "....1....1....", "...1......1...", 
            "..1........1..", ".111111111111.", "...1......1...", "...1......1...", 
            "...1..11..1...", "...1..11..1...", "...11111111..." }; break;
        default: map = { "11111111111111", "11111111111111" }; break;
    }

    bricks.clear();
    int rows = map.size();
    int cols = map[0].size();
    float startX = (SCREEN_WIDTH - cols * (BRICK_WIDTH + 2)) / 2.0f;
    float startY = 80.0f; 

    for (int r = 0; r < rows; r++) 
    {
        for (int c = 0; c < cols; c++) 
        {
            if (map[r][c] == '1') 
            {
                ::Brick b;
                b.rec = { startX + c * (BRICK_WIDTH + 2), startY + r * (BRICK_HEIGHT + 2), (float)BRICK_WIDTH, (float)BRICK_HEIGHT };
                b.color = BRICK_COLORS[r % 8];
                b.originalColor = b.color;
                b.active = true;
                bricks.push_back(b);
            }
        }
    }

    globalParticlePool.clear();
    powerUps.clear();
    bricksHit = 0;
    gameOver = false;
    gameStarted = false;
    
    {
        std::lock_guard<std::mutex> lock(loadMtx);
        loadState = LoadState::IDLE;
        colorChangeTimer = 0.0f; 
    }
}

void UpdateGame()
{
    float dtScale = GetFrameTime() * 60.0f;

    if (IsKeyPressed(KEY_L)) 
    {
        std::lock_guard<std::mutex> lock(loadMtx);
        if (loadState != LoadState::LOADING && colorChangeTimer <= 0) 
        {
            loadState = LoadState::LOADING;
            int randOffsetX = GetRandomValue(0, 100000);
            int randOffsetY = GetRandomValue(0, 100000);
            Color themes[] = { PINK, GREEN, SKYBLUE, VIOLET, ORANGE, GOLD, LIME, MAROON };
            Color selectedTheme = themes[GetRandomValue(0, 7)];

            std::packaged_task<Image()> task([randOffsetX, randOffsetY, selectedTheme]() -> Image 
            {
                Image img = GenImagePerlinNoise(1920, 1080, randOffsetX, randOffsetY, 4.0f);
                ImageColorTint(&img, selectedTheme);
                std::this_thread::sleep_for(std::chrono::milliseconds(2500));
                return img; 
            });
            
            loadFuture = task.get_future();
            std::thread(std::move(task)).detach();
        }
    }

    if (loadState == LoadState::LOADING) 
    {
        if (loadFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) 
        {
            Image loadedImg = loadFuture.get(); 
            Texture2D bgTex = LoadTextureFromImage(loadedImg);
            UnloadImage(loadedImg); 
            TextureCache::getInstance().put("async_bg", bgTex);
            
            std::lock_guard<std::mutex> lock(loadMtx);
            loadState = LoadState::DONE;
            colorChangeTimer = 0.5f; 
            
            for (auto &b : bricks) b.color = MAGENTA; 
            PlaySound(catchStarFx); 
        }
    }

    if (colorChangeTimer > 0) 
    {
        colorChangeTimer -= GetFrameTime();
        if (colorChangeTimer <= 0) 
        {
            for (auto &b : bricks) b.color = b.originalColor;
            loadState = LoadState::IDLE; 
        }
    }

    if (inMenu)
    {
        Vector2 mousePos = GetMousePosition();
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            // 优先检查“继续游戏”按钮
            if (CheckSaveExists()) 
            {
                Rectangle continueBtn = { (float)SCREEN_WIDTH / 2 - 100, 80.0f, 200, 45 };
                if (CheckCollisionPointRec(mousePos, continueBtn)) 
                {
                    if (LoadProgress()) 
                    {
                        InitGame(currentLevel, false); // 不重置分数和生命
                        inMenu = false;
                        return;
                    }
                }
            }

            // 检查常规关卡按钮
            for (int i = 0; i <= 5; i++) 
            {
                Rectangle btn = { (float)SCREEN_WIDTH / 2 - 100, 140.0f + i * 60.0f, 200, 45 };
                if (CheckCollisionPointRec(mousePos, btn)) 
                {
                    currentLevel = i;
                    InitGame(currentLevel, true); // 全新开局重置
                    inMenu = false;
                    return;
                }
            }
        }
        return; 
    }

    if (shakeDuration > 0) shakeDuration -= GetFrameTime();

    if (gameOver)
    {
        if (IsKeyPressed(KEY_R)) InitGame(currentLevel, true);
        if (IsKeyPressed(KEY_M)) { inMenu = true; gameOver = false; gameStarted = false; }
        return;
    }

    if (!gameStarted)
    {
        if (!balls.empty()) 
        { 
            balls[0].pos.x = paddle.rec.x + paddle.rec.width / 2.0f; 
            balls[0].pos.y = paddle.rec.y - balls[0].radius - 5; 
        }
        
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) 
        { 
            gameStarted = true;
            if (!balls.empty()) balls[0].speed = { 4.0f, -4.0f }; 
        }
        
        // 玩家手动返回菜单时触发存档
        if (IsKeyPressed(KEY_M)) 
        { 
            SaveProgress();
            inMenu = true; 
            return; 
        }
    }

    if (gameStarted && powerUpCooldown > 0) powerUpCooldown -= GetFrameTime();

    if (IsKeyDown(KEY_LEFT) && paddle.rec.x > 0) paddle.rec.x -= 8.0f * dtScale;
    if (IsKeyDown(KEY_RIGHT) && paddle.rec.x < SCREEN_WIDTH - paddle.rec.width) paddle.rec.x += 8.0f * dtScale;

    for (int i = 0; i < (int)balls.size(); i++)
    {
        if (!balls[i].active || !gameStarted) continue;
            
        balls[i].pos.x += balls[i].speed.x * dtScale; 
        balls[i].pos.y += balls[i].speed.y * dtScale;

        if (balls[i].pos.x <= balls[i].radius || balls[i].pos.x >= SCREEN_WIDTH - balls[i].radius) 
        { 
            balls[i].speed.x *= -1; 
            TriggerShake(0.05f, 2.0f); 
        }
        
        if (balls[i].pos.y <= balls[i].radius) 
        { 
            balls[i].speed.y *= -1; 
            TriggerShake(0.05f, 2.0f); 
        }
        
        if (balls[i].pos.y >= SCREEN_HEIGHT + balls[i].radius) 
        { 
            balls[i].active = false;
            continue; 
        }

        if (balls[i].speed.y > 0 && CheckCollisionCircleRec(balls[i].pos, balls[i].radius, paddle.rec))
        {
            balls[i].pos.y = paddle.rec.y - balls[i].radius; 
            balls[i].speed.y *= -1;
            float hitOffset = (balls[i].pos.x - (paddle.rec.x + paddle.rec.width / 2.0f)) / (paddle.rec.width / 2.0f);
            balls[i].speed.x = hitOffset * 6.0f; 
            PlaySound(hitPaddleFx); 
        }

        for (auto &b : bricks)
        {
            if (b.active && CheckCollisionCircleRec(balls[i].pos, balls[i].radius, b.rec))
            {
                b.active = false; 
                bricksHit++; 
                score += globalScoreCalc.CalculateScore(1); 
                PlaySound(hitBrickFx); 
                CreateExplosion({b.rec.x + 25, b.rec.y + 10}, b.color);
                
                balls[i].speed.y *= -1; 
                
                if (powerUpCooldown <= 0.0f) 
                { 
                    powerUps.push_back({ {b.rec.x + 25, b.rec.y}, 3.5f, true }); 
                    PlaySound(starDropFx); 
                    powerUpCooldown = (float)GetRandomValue(5, 8); 
                }
                
                break; // 碰撞后立刻跳出当前小球的砖块判定
            }
        }
    }

    // === 多关卡系统核心逻辑 (安全解耦版) ===
    // 将通关检测从上面的双重循环中抽离出来。
    // 如果放在碰撞循环里直接 InitGame() 刷新小球，会导致迭代器失效甚至内存崩溃，
    // 现在移到所有的物理结算之后，确保过渡绝对安全丝滑。
    if (gameStarted && !gameOver && GetActiveBrickCount() == 0) 
    { 
        currentLevel++;
        if (currentLevel > 5) 
        {
            // 彻底通关，销毁存档
            inMenu = true; 
            gameStarted = false; 
            DeleteSave();
        } 
        else 
        {
            // 自动加载下一关，保留分数和生命值，然后存档
            InitGame(currentLevel, false);
            gameStarted = false; // 等待玩家按空格重新发球
            SaveProgress();
        }
    }

    if (gameStarted && !gameOver)
    {
        bool hasActiveBall = false;
        for (const auto& b : balls) {
            if (b.active) { hasActiveBall = true; break; }
        }

        if (!hasActiveBall)
        {
            lives--; 
            TriggerShake(0.4f, 10.0f); 
            
            if (lives > 0) 
            {
                gameStarted = false;
                paddle.rec.width = INITIAL_PADDLE_WIDTH; 
                balls.clear();
                balls.push_back({ {paddle.rec.x + paddle.rec.width / 2.0f, paddle.rec.y - BALL_RADIUS - 5}, {0, 0}, (float)BALL_RADIUS, true });
                powerUps.clear(); 
                
                SaveProgress(); // 掉血后立即存档，防止玩家作弊退出（SL大法）
            } 
            else 
            {
                gameOver = true;
                DeleteSave(); // 命没了，删除存档惩罚
            }
        }
    }

    globalParticlePool.update(dtScale);

    for (auto &p : powerUps)
    {
        if (!p.active) continue;
            
        p.pos.y += p.speed * dtScale;
        
        if (CheckCollisionCircleRec(p.pos, POWERUP_RADIUS, paddle.rec))
        {
            p.active = false; 
            score += 10; 
            
            if (paddle.rec.width < MAX_PADDLE_WIDTH) 
            { 
                paddle.rec.width += 34.0f; 
                if (paddle.rec.width > MAX_PADDLE_WIDTH) paddle.rec.width = MAX_PADDLE_WIDTH; 
            }
            PlaySound(catchStarFx); 
            CreateExplosion(p.pos, SKYBLUE); 
            TriggerShake(0.15f, 4.0f);
        }
        
        if (p.pos.y > SCREEN_HEIGHT) p.active = false;
    }
}

void DrawGame()
{
    BeginDrawing();
    ClearBackground({ 10, 10, 15, 255 });

    if (TextureCache::getInstance().has("async_bg")) 
    {
        DrawTexturePro(
            TextureCache::getInstance().get("async_bg"),
            { 0, 0, 1920, 1080 },
            { 0, 0, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT },
            { 0, 0 }, 0.0f, Fade(WHITE, 0.4f) 
        );
    }

    if (inMenu)
    {
        Color titleColor = (colorChangeTimer > 0) ? MAGENTA : GOLD; 
        DrawText("LEVEL SELECT", SCREEN_WIDTH / 2 - 135, 30, 40, titleColor);
        
        Vector2 mousePos = GetMousePosition();
        
        // 如果存在存档，渲染绿色的 继续游戏 按钮
        if (CheckSaveExists()) 
        {
            Rectangle continueBtn = { (float)SCREEN_WIDTH / 2 - 100, 80.0f, 200, 45 };
            bool hover = CheckCollisionPointRec(mousePos, continueBtn);
            DrawRectangleRec(continueBtn, hover ? DARKGREEN : GREEN);
            DrawRectangleLinesEx(continueBtn, 2, LIME);
            DrawText("CONTINUE GAME", continueBtn.x + 25, continueBtn.y + 12, 20, RAYWHITE);
        }
        
        for (int i = 0; i <= 5; i++) 
        {
            Rectangle btn = { (float)SCREEN_WIDTH / 2 - 100, 140.0f + i * 60.0f, 200, 45 };
            bool hover = CheckCollisionPointRec(mousePos, btn);
            DrawRectangleRec(btn, hover ? DARKBLUE : BLUE);
            DrawRectangleLinesEx(btn, 2, SKYBLUE);
            
            if (i == 0) DrawText("LEVEL 0 (TRIAL)", btn.x + 20, btn.y + 12, 20, RAYWHITE);
            else DrawText(TextFormat("NEW: LEVEL %d", i), btn.x + 40, btn.y + 12, 20, RAYWHITE);
        }
    }
    else
    {
        Camera2D camera = { { 0.0f, 0.0f }, { 0.0f, 0.0f }, 0.0f, 1.0f }; 
        if (shakeDuration > 0) camera.offset = { (float)GetRandomValue(-5, 5), (float)GetRandomValue(-5, 5) };
        
        BeginMode2D(camera);

        DrawRectangleRec(paddle.rec, paddle.color);
        DrawRectangleLinesEx(paddle.rec, 2, SKYBLUE);

        for (const auto &b : bricks) 
        { 
            if (b.active) 
            { 
                DrawRectangleRec(b.rec, b.color); 
                DrawRectangleLinesEx(b.rec, 1, Fade(BLACK, 0.3f)); 
            } 
        }
        
        globalParticlePool.draw();
        
        for (const auto &b : balls) 
        { 
            if (b.active) DrawCircleV(b.pos, b.radius, WHITE); 
        }

        for (const auto &p : powerUps) 
        { 
            if (p.active) DrawGeminiStar(p.pos, POWERUP_RADIUS, (sinf(GetTime() * 20.0f) * 0.5f) + 0.5f); 
        }

        EndMode2D();

        DrawText(TextFormat("SCORE: %05d", score), 25, 20, 25, GOLD);
        DrawText(TextFormat("LEVEL: %d", currentLevel), SCREEN_WIDTH - 150, 20, 25, LIGHTGRAY);

        DrawText("LIVES:", SCREEN_WIDTH / 2 - 60, 22, 20, LIGHTGRAY);
        for (int i = 0; i < lives; i++)
        {
            DrawCircle(SCREEN_WIDTH / 2 + 25 + i * 25, 32, 8, RED);
            DrawCircleLines(SCREEN_WIDTH / 2 + 25 + i * 25, 32, 8, MAROON);
        }

        if (!gameStarted) 
        {
            DrawText("PRESS SPACE TO START", SCREEN_WIDTH / 2 - 120, SCREEN_HEIGHT / 2 + 100, 20, RAYWHITE);
            DrawText("PRESS M TO RETURN MENU", SCREEN_WIDTH / 2 - 140, SCREEN_HEIGHT / 2 + 130, 20, GRAY);
        }

        if (gameOver)
        {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.8f));
            DrawText("GAME OVER", SCREEN_WIDTH / 2 - 120, SCREEN_HEIGHT / 2 - 40, 45, RED);
            DrawText("PRESS R TO RESTART | M TO MENU", SCREEN_WIDTH / 2 - 145, SCREEN_HEIGHT / 2 + 60, 20, GRAY);
        }
    }

    if (loadState == LoadState::LOADING) 
    {
        float alpha = (sinf(GetTime() * 8.0f) * 0.5f) + 0.5f; 
        DrawRectangle(SCREEN_WIDTH - 240, SCREEN_HEIGHT - 50, 220, 40, Fade(BLACK, 0.8f));
        DrawText("ASYNC LOADING...", SCREEN_WIDTH - 230, SCREEN_HEIGHT - 40, 20, Fade(GREEN, alpha));
    }

    EndDrawing();
}

int main()
{
    std::cout << "========== 作业：多态与重载验证 ==========\n";
    ScoreCalculator testCalc;
    std::cout << "函数重载测试 (类型2, 连击5): " << testCalc.CalculateScore(2, 5) << " 分\n";
    
    std::vector<OOP_Task::GameObject*> hwObjects;
    hwObjects.push_back(new OOP_Task::Ball());
    hwObjects.push_back(new OOP_Task::Paddle());
    hwObjects.push_back(new OOP_Task::Brick());
    
    for (auto obj : hwObjects) 
    { 
        obj->Update(); 
        obj->Draw(); 
        delete obj; 
    }
    std::cout << "========================================\n\n";

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Gemini Breakout: Dual Core (Max Edition)");
    InitAudioDevice();

    bgm = LoadMusicStream("background.mp3");
    hitPaddleFx = LoadSound("paddle.wav"); 
    hitBrickFx = LoadSound("brick.wav");
    catchStarFx = LoadSound("catch-stars.wav"); 
    starDropFx = LoadSound("stars-drop.wav"); 

    PlayMusicStream(bgm); 
    SetMusicVolume(bgm, 0.4f); 
    
    SetTargetFPS(60); 
    InitGame(0, true);

    while (!WindowShouldClose())
    {
        UpdateMusicStream(bgm);
        UpdateGame();
        DrawGame(); 
    }

    TextureCache::getInstance().clear();
    UnloadMusicStream(bgm);
    UnloadSound(hitPaddleFx); 
    UnloadSound(hitBrickFx);
    UnloadSound(catchStarFx); 
    UnloadSound(starDropFx);
    
    CloseAudioDevice(); 
    CloseWindow();
    
    return 0;
}