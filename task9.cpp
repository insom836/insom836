#include "raylib.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>
// 引入多线程与异步任务所需的标准库 
#include <thread>
#include <future>
#include <mutex>
#include <chrono>
// 引入加分项所需的缓存与函数包装库
#include <unordered_map>
#include <functional>

// ========================================================
// === 【加分项 2】: 线程安全的资源缓存 (TextureCache) ===
// ========================================================
class TextureCache {
private:
    std::unordered_map<std::string, Texture2D> cache;
    std::mutex mtx;
    TextureCache() {} // 私有构造函数，保证单例

public:
    // 线程安全的单例获取
    static TextureCache& getInstance() {
        static TextureCache instance;
        return instance;
    }

    // 线程安全的写入 (包含严谨的显存泄漏防范)
    void put(const std::string& key, Texture2D tex) {
        std::lock_guard<std::mutex> lock(mtx);
        // 【防显存泄漏优化】: 如果缓存中已有旧贴图，先释放 GPU 内存再覆盖
        if (cache.find(key) != cache.end()) {
            UnloadTexture(cache[key]);
        }
        cache[key] = tex;
    }

    // 线程安全的查询
    bool has(const std::string& key) {
        std::lock_guard<std::mutex> lock(mtx);
        return cache.find(key) != cache.end();
    }

    // 线程安全的读取
    Texture2D get(const std::string& key) {
        std::lock_guard<std::mutex> lock(mtx);
        return cache[key];
    }

    // 清理显存资源
    void clear() {
        std::lock_guard<std::mutex> lock(mtx);
        for (auto& pair : cache) {
            UnloadTexture(pair.second);
        }
        cache.clear();
    }
};

// ========================================================
// === 【作业要求 1】: 工具类 ScoreCalculator 与函数重载 ===
// ========================================================
class ScoreCalculator {
public:
    // 重载1：根据砖块类型返回得分
    int CalculateScore(int type) {
        switch (type) {
            case 1: return 10;   // 普通砖块
            case 2: return 20;   // 金色砖块
            case 3: return -5;   // 炸弹砖块
            default: return 0;
        }
    }

    // 重载2：根据类型和连击数计算额外奖励
    int CalculateScore(int type, int combo) {
        int baseScore = CalculateScore(type);
        return baseScore + combo * 2;
    }
};

// 全局得分计算器
ScoreCalculator globalScoreCalc; 

// ========================================================
// === 【作业要求 2】: 类的继承、虚函数与多态演示区块 ===
// ========================================================
namespace OOP_Task {
    class GameObject {
    public:
        virtual void Update() = 0;
        virtual void Draw() = 0;
        virtual ~GameObject() {
            std::cout << "  [销毁] GameObject 基类析构" << std::endl;
        }
    };

    class Ball : public GameObject {
    public:
        void Update() override { std::cout << "  [Ball] Update: 执行球体运动与碰撞计算" << std::endl; }
        void Draw() override { std::cout << "  [Ball] Draw: 绘制圆形小球" << std::endl; }
        ~Ball() override { std::cout << "  [销毁] Ball 派生类析构" << std::endl; }
    };

    class Paddle : public GameObject {
    public:
        void Update() override { std::cout << "  [Paddle] Update: 响应按键移动挡板" << std::endl; }
        void Draw() override { std::cout << "  [Paddle] Draw: 绘制长方形挡板" << std::endl; }
        ~Paddle() override { std::cout << "  [销毁] Paddle 派生类析构" << std::endl; }
    };

    class Brick : public GameObject {
    public:
        void Update() override { std::cout << "  [Brick] Update: 检查是否被击碎" << std::endl; }
        void Draw() override { std::cout << "  [Brick] Draw: 绘制彩色矩形" << std::endl; }
        ~Brick() override { std::cout << "  [销毁] Brick 派生类析构" << std::endl; }
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

const Color BRICK_COLORS[] = {RED, ORANGE, YELLOW, GREEN, BLUE, PURPLE, PINK, LIGHTGRAY};

struct Ball 
{ 
    Vector2 pos; 
    Vector2 speed; 
    float radius; 
    bool active; 
};

struct Paddle 
{ 
    Rectangle rec; 
    Color color; 
};

struct Brick 
{ 
    Rectangle rec; 
    Color color; 
    Color originalColor; 
    bool active; 
};

struct Particle 
{ 
    Vector2 position; 
    Vector2 speed; 
    Color color; 
    float alpha; 
    bool active; 
};

struct PowerUp 
{ 
    Vector2 pos; 
    float speed; 
    bool active; 
};

std::vector<Ball> balls;
::Paddle paddle; 
std::vector<::Brick> bricks;
std::vector<Particle> particles;
std::vector<PowerUp> powerUps;

int score = 0;
int lives = 1;     
int bricksHit = 0; 
bool gameOver = false;
bool gameStarted = false;
bool secondBallTriggered = false; 

bool inMenu = true;
int currentLevel = 0; 
float powerUpCooldown = 0.0f; 

// 状态机，future 返回真实图像内存，而非 int
enum class LoadState { IDLE, LOADING, DONE };
LoadState loadState = LoadState::IDLE;
std::future<Image> loadFuture; 
std::mutex loadMtx; 
float colorChangeTimer = 0.0f; 

float shakeDuration = 0.0f;
float shakeMagnitude = 0.0f;
float rippleRadius = 0.0f;
float rippleAlpha = 0.0f;
Vector2 ripplePos = {0};

Music bgm;
Sound hitPaddleFx;
Sound hitBrickFx;
Sound catchStarFx;  
Sound starDropFx;   
Sound secondBallFx; 

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

void CreateExplosion(Vector2 pos, Color color, int count = 20)
{
    for (int i = 0; i < count; i++)
    {
        Particle p;
        p.position = pos;
        p.speed = {(float)GetRandomValue(-200, 200) / 40.0f, (float)GetRandomValue(-200, 200) / 40.0f};
        p.color = color;
        p.alpha = 1.0f;
        p.active = true;
        particles.push_back(p);
    }
}

int GetActiveBrickCount()
{
    int activeCount = 0;
    for (const auto &b : bricks) 
    {
        if (b.active) 
        {
            activeCount++;
        }
    }
    return activeCount;
}

void InitGame(int level)
{
    paddle.rec = {(float)(SCREEN_WIDTH - INITIAL_PADDLE_WIDTH) / 2, (float)SCREEN_HEIGHT - 50, (float)INITIAL_PADDLE_WIDTH, (float)PADDLE_HEIGHT};
    paddle.color = BLUE;

    balls.clear();
    balls.push_back({{SCREEN_WIDTH / 2.0f, paddle.rec.y - BALL_RADIUS - 5}, {0, 0}, (float)BALL_RADIUS, true});

    std::vector<std::string> map;
    switch (level) 
    {
        case 0: 
            map = { 
                "111111" 
            }; 
            break;
        case 1: 
            map = { 
                "11111111111111", 
                "11111111111111", 
                "11111111111111", 
                "11111111111111", 
                "11111111111111", 
                "11111111111111", 
                "11111111111111", 
                "11111111111111" 
            }; 
            break;
        case 2: 
            map = { 
                "..1111..1111..", 
                ".111111111111.", 
                "11111111111111", 
                "11111111111111", 
                "11111111111111", 
                ".111111111111.", 
                "..1111111111..", 
                "...11111111...", 
                "....111111....", 
                ".....1111.....", 
                "......11......" 
            }; 
            break;
        case 3: 
            map = { 
                "......11......", 
                ".....1..1.....", 
                ".....1..1.....", 
                "....1....1....", 
                "....1....1....", 
                "1111......1111", 
                ".1..........1.", 
                "..1........1..", 
                "...1......1...", 
                "....1....1....", 
                "...1..11..1...", 
                "..1..1..1..1..", 
                ".1..1....1..1.", 
                "1111......1111" 
            }; 
            break;
        case 4: 
            map = { 
                "...111..111...", 
                "..1111111111..", 
                "..1111..1111..", 
                "...11....11...", 
                "1111..11..1111", 
                "11111111111111", 
                "1111..11..1111", 
                "...11....11...", 
                "..1111..1111..", 
                "..1111111111..", 
                "...111..111...", 
                "......11......" 
            }; 
            break;
        case 5: 
            map = { 
                "......11......", 
                ".....1..1.....", 
                "....1....1....", 
                "...1......1...", 
                "..1........1..", 
                ".111111111111.", 
                "...1......1...", 
                "...1......1...", 
                "...1..11..1...", 
                "...1..11..1...", 
                "...11111111..." 
            }; 
            break;
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
                b.rec = {startX + c * (BRICK_WIDTH + 2), startY + r * (BRICK_HEIGHT + 2), (float)BRICK_WIDTH, (float)BRICK_HEIGHT};
                b.color = BRICK_COLORS[r % 8];
                b.originalColor = b.color;
                b.active = true;
                bricks.push_back(b);
            }
        }
    }

    particles.clear();
    powerUps.clear();
    lives = 1; 
    bricksHit = 0;
    powerUpCooldown = 3.0f; 
    gameOver = false;
    gameStarted = false;
    secondBallTriggered = false;
    rippleAlpha = 0;
    
    {
        std::lock_guard<std::mutex> lock(loadMtx);
        loadState = LoadState::IDLE;
        colorChangeTimer = 0.0f; 
    }
}

void UpdateGame()
{
    // 1. 响应按键 L 触发后台多线程异步加载
    if (IsKeyPressed(KEY_L)) 
    {
        std::lock_guard<std::mutex> lock(loadMtx);
        if (loadState != LoadState::LOADING && colorChangeTimer <= 0) 
        {
            loadState = LoadState::LOADING;
            
            // ======================================================================
            // 【硬核优化】: 保证 Raylib 状态机安全的随机参数抽取
            // ======================================================================
            // 在主线程提取随机数，彻底避免子线程直接访问 Raylib 全局随机状态可能导致的竞态条件
            int randOffsetX = GetRandomValue(0, 100000);
            int randOffsetY = GetRandomValue(0, 100000);

            // 将随机种子按值捕获传入 Lambda 表达式
            std::packaged_task<Image()> task([randOffsetX, randOffsetY]() -> Image {
                // 生成真·随机的大尺寸柏林噪声，每一张贴图都独一无二，硬扛 CPU 算力
                Image img = GenImagePerlinNoise(1920, 1080, randOffsetX, randOffsetY, 4.0f);
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
                return img; 
            });
            
            loadFuture = task.get_future();
            std::thread(std::move(task)).detach();
        }
    }

    // 2. 主线程每帧非阻塞轮询
    if (loadState == LoadState::LOADING) 
    {
        if (loadFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) 
        {
            Image loadedImg = loadFuture.get(); 
            
            Texture2D bgTex = LoadTextureFromImage(loadedImg);
            UnloadImage(loadedImg); 
            
            // 存入严密防泄漏的缓存池
            TextureCache::getInstance().put("async_bg", bgTex);
            
            std::lock_guard<std::mutex> lock(loadMtx);
            loadState = LoadState::DONE;
            colorChangeTimer = 0.5f; 
            for (auto &b : bricks) 
            {
                b.color = MAGENTA; 
            }
            PlaySound(catchStarFx); 
        }
    }

    // 3. 颜色反馈计时器管理
    if (colorChangeTimer > 0) 
    {
        colorChangeTimer -= GetFrameTime();
        if (colorChangeTimer <= 0) 
        {
            for (auto &b : bricks) 
            {
                b.color = b.originalColor;
            }
            loadState = LoadState::IDLE; 
        }
    }

    if (inMenu)
    {
        Vector2 mousePos = GetMousePosition();
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            for (int i = 0; i <= 5; i++) 
            {
                Rectangle btn = { (float)SCREEN_WIDTH / 2 - 100, 140.0f + i * 60.0f, 200, 45 };
                if (CheckCollisionPointRec(mousePos, btn)) 
                {
                    currentLevel = i;
                    InitGame(currentLevel);
                    inMenu = false;
                    return;
                }
            }
        }
        return; 
    }

    if (shakeDuration > 0) 
    {
        shakeDuration -= GetFrameTime();
    }
    if (rippleAlpha > 0) 
    { 
        rippleRadius += 400.0f * GetFrameTime(); 
        rippleAlpha -= 1.0f * GetFrameTime(); 
    }

    if (gameOver)
    {
        if (IsKeyPressed(KEY_R)) 
        {
            InitGame(currentLevel);
        }
        if (IsKeyPressed(KEY_M)) 
        { 
            inMenu = true; 
            gameOver = false; 
            gameStarted = false; 
        }
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
            if (!balls.empty()) 
            {
                balls[0].speed = {4.0f, -4.0f}; 
            }
        }

        // 允许在发球前（未开始状态）按 M 键丝滑退回主菜单
        if (IsKeyPressed(KEY_M))
        {
            inMenu = true;
            return;
        }
    }

    if (gameStarted && powerUpCooldown > 0) 
    {
        powerUpCooldown -= GetFrameTime();
    }
    
    if (IsKeyDown(KEY_LEFT) && paddle.rec.x > 0) 
    {
        paddle.rec.x -= 8;
    }
    if (IsKeyDown(KEY_RIGHT) && paddle.rec.x < SCREEN_WIDTH - paddle.rec.width) 
    {
        paddle.rec.x += 8;
    }

    for (int i = 0; i < (int)balls.size(); i++)
    {
        if (!balls[i].active || !gameStarted) 
            continue;
            
        balls[i].pos.x += balls[i].speed.x; 
        balls[i].pos.y += balls[i].speed.y;

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
            TriggerShake(0.3f, 8.0f); 
            gameOver = true; 
            break; 
        }

        if (balls[i].speed.y > 0 && CheckCollisionCircleRec(balls[i].pos, balls[i].radius, paddle.rec))
        {
            balls[i].pos.y = paddle.rec.y - balls[i].radius; 
            balls[i].speed.y *= -1;
            float hitOffset = (balls[i].pos.x - (paddle.rec.x + paddle.rec.width / 2.0f)) / (paddle.rec.width / 2.0f);
            balls[i].speed.x = hitOffset * 6.0f; 
            PlaySound(hitPaddleFx); 
            TriggerShake(0.08f, 3.0f);
        }

        for (auto &b : bricks)
        {
            if (b.active && CheckCollisionCircleRec(balls[i].pos, balls[i].radius, b.rec))
            {
                b.active = false; 
                bricksHit++; 
                score += globalScoreCalc.CalculateScore(1); 
                
                PlaySound(hitBrickFx); 
                CreateExplosion({b.rec.x + b.rec.width / 2, b.rec.y + b.rec.height / 2}, b.color);
                
                balls[i].speed.y *= -1; 
                TriggerShake(0.12f, 5.0f);
                
                if (powerUpCooldown <= 0.0f) 
                { 
                    powerUps.push_back({{b.rec.x + b.rec.width / 2, b.rec.y}, 3.5f, true}); 
                    PlaySound(starDropFx); 
                    powerUpCooldown = (float)GetRandomValue(5, 8); 
                }
                
                if (GetActiveBrickCount() == 0) 
                { 
                    inMenu = true; 
                    gameStarted = false; 
                }
                break;
            }
        }
    }

    for (auto &p : powerUps)
    {
        if (!p.active) 
            continue;
            
        p.pos.y += p.speed;
        
        if (CheckCollisionCircleRec(p.pos, POWERUP_RADIUS, paddle.rec))
        {
            p.active = false; 
            score += 10; 
            
            if (paddle.rec.width < MAX_PADDLE_WIDTH) 
            { 
                paddle.rec.width += 34.0f; 
                if (paddle.rec.width > MAX_PADDLE_WIDTH) 
                {
                    paddle.rec.width = MAX_PADDLE_WIDTH; 
                }
            }
            PlaySound(catchStarFx); 
            CreateExplosion(p.pos, SKYBLUE, 30); 
            TriggerShake(0.15f, 4.0f);
        }
        if (p.pos.y > SCREEN_HEIGHT) 
        {
            p.active = false;
        }
    }

    for (auto &p : particles) 
    { 
        if (p.active) 
        { 
            p.position.x += p.speed.x; 
            p.position.y += p.speed.y; 
            p.alpha -= 0.02f; 
            if (p.alpha <= 0) 
            {
                p.active = false; 
            }
        } 
    }
}

void DrawGame()
{
    BeginDrawing();
    ClearBackground({10, 10, 15, 255});

    if (TextureCache::getInstance().has("async_bg")) 
    {
        DrawTexturePro(
            TextureCache::getInstance().get("async_bg"),
            {0, 0, 1920, 1080},
            {0, 0, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT},
            {0, 0}, 0.0f, Fade(DARKGRAY, 0.4f) 
        );
    }

    if (inMenu)
    {
        Color titleColor = (colorChangeTimer > 0) ? MAGENTA : GOLD; 
        DrawText("LEVEL SELECT", SCREEN_WIDTH / 2 - 135, 60, 40, titleColor);
        DrawText(TextFormat("TOTAL SCORE: %05d", score), SCREEN_WIDTH / 2 - 100, 105, 20, LIGHTGRAY);
        
        Vector2 mousePos = GetMousePosition();
        for (int i = 0; i <= 5; i++) 
        {
            Rectangle btn = { (float)SCREEN_WIDTH / 2 - 100, 140.0f + i * 60.0f, 200, 45 };
            bool hover = CheckCollisionPointRec(mousePos, btn);
            DrawRectangleRec(btn, hover ? DARKBLUE : BLUE);
            DrawRectangleLinesEx(btn, 2, SKYBLUE);
            
            if (i == 0) 
            {
                DrawText("LEVEL 0 (TRIAL)", btn.x + 20, btn.y + 12, 20, RAYWHITE);
            } 
            else 
            {
                DrawText(TextFormat("LEVEL %d", i), btn.x + 55, btn.y + 12, 20, RAYWHITE);
            }
        }
    }
    else
    {
        Camera2D camera = {0};
        if (shakeDuration > 0) 
        {
            camera.offset = {(float)GetRandomValue(-shakeMagnitude, shakeMagnitude), (float)GetRandomValue(-shakeMagnitude, shakeMagnitude)};
        }
        else 
        {
            camera.offset = {0, 0};
        }
        
        camera.target = {0, 0}; 
        camera.rotation = 0.0f; 
        camera.zoom = 1.0f;

        if (gameOver)
        {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.8f));
            DrawText("GAME OVER", SCREEN_WIDTH / 2 - 120, SCREEN_HEIGHT / 2 - 40, 45, RED);
            DrawText(TextFormat("FINAL SCORE: %05d", score), SCREEN_WIDTH / 2 - 110, SCREEN_HEIGHT / 2 + 20, 20, WHITE);
            DrawText("PRESS R TO RESTART", SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 + 60, 20, GRAY);
            DrawText("PRESS M TO MENU", SCREEN_WIDTH / 2 - 90, SCREEN_HEIGHT / 2 + 90, 20, GRAY);
        }
        else
        {
            BeginMode2D(camera);

            if (rippleAlpha > 0) 
            { 
                DrawCircleLines(ripplePos.x, ripplePos.y, rippleRadius, Fade(WHITE, rippleAlpha)); 
                DrawCircleLines(ripplePos.x, ripplePos.y, rippleRadius * 0.7f, Fade(SKYBLUE, rippleAlpha * 0.5f)); 
            }
            DrawRectangleRec(paddle.rec, paddle.color);
            DrawRectangleLinesEx(paddle.rec, 2, SKYBLUE);

            BeginBlendMode(BLEND_ADDITIVE);
            int glowLayers = (currentLevel == 5) ? 8 : 4; 
            float glowSpread = (currentLevel == 5) ? 3.0f : 2.0f;
            float shimmerIntensity = (sinf(GetTime() * 15.0f) * 0.5f) + 0.5f; 

            for (const auto &b : bricks) 
            {
                if (b.active) 
                {
                    for (float glow = 1.0f; glow <= glowLayers; glow += 1.0f) 
                    {
                        Rectangle glowRec = { b.rec.x - glow * glowSpread, b.rec.y - glow * glowSpread, b.rec.width + glow * glowSpread * 2.0f, b.rec.height + glow * glowSpread * 2.0f };
                        float alphaBase = (currentLevel == 5) ? (0.25f + 0.2f * shimmerIntensity) : 0.15f;
                        DrawRectangleRec(glowRec, Fade(b.color, alphaBase / glow));
                    }
                }
            }
            EndBlendMode(); 

            for (const auto &b : bricks) 
            { 
                if (b.active) 
                { 
                    DrawRectangleRec(b.rec, b.color); 
                    DrawRectangleLinesEx(b.rec, 1, Fade(BLACK, 0.3f)); 
                } 
            }
            for (const auto &p : powerUps) 
            { 
                if (p.active) 
                {
                    DrawGeminiStar(p.pos, POWERUP_RADIUS, (sinf(GetTime() * 20.0f) * 0.5f) + 0.5f); 
                }
            }
            for (const auto &p : particles) 
            { 
                if (p.active) 
                {
                    DrawRectangleV(p.position, {3, 3}, Fade(p.color, p.alpha)); 
                }
            }
            for (const auto &b : balls) 
            { 
                if (b.active) 
                { 
                    DrawCircleV(b.pos, b.radius, WHITE); 
                    DrawCircleLines(b.pos.x, b.pos.y, b.radius + 1, GRAY); 
                } 
            }

            EndMode2D();

            DrawText(TextFormat("SCORE: %05d", score), 25, 20, 25, GOLD);
            DrawText(TextFormat("LEVEL: %d", currentLevel), SCREEN_WIDTH - 150, 20, 25, LIGHTGRAY);

            if (!gameStarted) 
            {
                DrawText("PRESS SPACE TO START", SCREEN_WIDTH / 2 - 120, SCREEN_HEIGHT / 2 + 100, 20, RAYWHITE);
                // M 键返回提示
                DrawText("PRESS M TO RETURN MENU", SCREEN_WIDTH / 2 - 140, SCREEN_HEIGHT / 2 + 130, 20, GRAY);
            }
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
    std::cout << "========== 作业要求 1：函数重载与得分计算 ==========\n";
    ScoreCalculator testCalc;
    std::cout << "打到普通砖块得分: " << testCalc.CalculateScore(1) << " 分\n";
    std::cout << "打到金色砖块(+3连击)得分: " << testCalc.CalculateScore(2, 3) << " 分\n";

    std::cout << "\n========== 作业要求 2：多态与游戏对象管理 ==========\n";
    std::vector<OOP_Task::GameObject*> hwObjects;
    hwObjects.push_back(new OOP_Task::Ball());
    hwObjects.push_back(new OOP_Task::Paddle());
    hwObjects.push_back(new OOP_Task::Brick());

    std::cout << ">>> 模拟一帧更新绘制，验证多态：\n";
    for (auto obj : hwObjects) 
    { 
        obj->Update(); 
        obj->Draw(); 
    }

    std::cout << "\n>>> 释放内存，测试虚析构：\n";
    for (auto obj : hwObjects) 
    { 
        delete obj; 
    }
    std::cout << "\n[ 控制台作业验证完毕，开始启动图形化游戏主程序... ]\n\n";

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Gemini Breakout: Dual Core (Max Edition)");
    InitAudioDevice();

    bgm = LoadMusicStream("background.mp3");
    hitPaddleFx = LoadSound("paddle.wav"); 
    hitBrickFx = LoadSound("brick.wav");
    catchStarFx = LoadSound("catch-stars.wav"); 
    starDropFx = LoadSound("stars-drop.wav"); 
    secondBallFx = LoadSound("second-ball.wav");

    PlayMusicStream(bgm); 
    SetMusicVolume(bgm, 0.4f); 
    SetTargetFPS(60);
    InitGame(0);

    while (!WindowShouldClose())
    {
        UpdateMusicStream(bgm);
        UpdateGame();
        DrawGame();
    }

    // 清理资源缓存中的大图片显存
    TextureCache::getInstance().clear();

    UnloadMusicStream(bgm);
    UnloadSound(hitPaddleFx); 
    UnloadSound(hitBrickFx);
    UnloadSound(catchStarFx); 
    UnloadSound(starDropFx); 
    UnloadSound(secondBallFx);
    CloseAudioDevice(); 
    CloseWindow();
    
    return 0;
}