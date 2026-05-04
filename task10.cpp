#include "raylib.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>

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
/*
 * 这是一个工业级的单例类，用于管理显存中的纹理资源。
 * 即使你在多线程环境下疯狂按 L 键，互斥锁也能保证显存不会崩溃，
 * 并且新贴图生成后会自动释放旧贴图，防止显存泄漏。
 */
class TextureCache 
{
private:
    std::unordered_map<std::string, Texture2D> cache;
    std::mutex mtx;
    
    // 私有构造函数，保证单例
    TextureCache() {} 

public:
    // 获取单例实例
    static TextureCache& getInstance() 
    {
        static TextureCache instance;
        return instance;
    }

    // 线程安全地存入贴图
    void put(const std::string& key, Texture2D tex) 
    {
        std::lock_guard<std::mutex> lock(mtx);
        
        // 如果缓存中已有旧贴图，必须先调用 GPU API 释放，否则 5060 的显存会被撑爆
        if (cache.find(key) != cache.end()) 
        {
            UnloadTexture(cache[key]);
        }
        cache[key] = tex;
    }

    // 检查缓存是否存在
    bool has(const std::string& key) 
    {
        std::lock_guard<std::mutex> lock(mtx);
        return cache.find(key) != cache.end();
    }

    // 提取贴图
    Texture2D get(const std::string& key) 
    {
        std::lock_guard<std::mutex> lock(mtx);
        return cache[key];
    }

    // 游戏结束时彻底清理显存
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
    // 重载 1：基础得分
    int CalculateScore(int type) 
    {
        switch (type) 
        {
            case 1: 
                return 10;   // 普通砖块
            case 2: 
                return 20;   // 金色砖块
            case 3: 
                return -5;   // 炸弹砖块
            default: 
                return 0;
        }
    }

    // 重载 2：连击加成得分
    int CalculateScore(int type, int combo) 
    {
        int baseScore = CalculateScore(type);
        return baseScore + combo * 2;
    }
};

// 全局得分计算器
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
        virtual ~GameObject() 
        {
            std::cout << "  [销毁] GameObject 基类析构" << std::endl;
        }
    };

    class Ball : public GameObject 
    {
    public:
        void Update() override 
        { 
            std::cout << "  [Ball] Update: 计算物理位移" << std::endl; 
        }
        void Draw() override 
        { 
            std::cout << "  [Ball] Draw: 渲染球体" << std::endl; 
        }
        ~Ball() override {}
    };

    class Paddle : public GameObject 
    {
    public:
        void Update() override 
        { 
            std::cout << "  [Paddle] Update: 检测键盘输入" << std::endl; 
        }
        void Draw() override 
        { 
            std::cout << "  [Paddle] Draw: 渲染挡板" << std::endl; 
        }
        ~Paddle() override {}
    };

    class Brick : public GameObject 
    {
    public:
        void Update() override 
        { 
            std::cout << "  [Brick] Update: 状态检查" << std::endl; 
        }
        void Draw() override 
        { 
            std::cout << "  [Brick] Draw: 渲染彩色矩形" << std::endl; 
        }
        ~Brick() override {}
    };
}


// ========================================================
// === 游戏核心代码 (全量展开排版) ===
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

// --- 结构体定义 ---
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

// --- 全局容器 ---
std::vector<Ball> balls;
::Paddle paddle; 
std::vector<::Brick> bricks;
std::vector<Particle> particles;
std::vector<PowerUp> powerUps;

// --- 游戏状态变量 ---
int score = 0;
int lives = 1;     
int bricksHit = 0; 
bool gameOver = false;
bool gameStarted = false;
bool secondBallTriggered = false; 
bool inMenu = true;
int currentLevel = 0; 
float powerUpCooldown = 0.0f; 

// --- 【异步加载核心状态机】 ---
enum class LoadState { IDLE, LOADING, DONE };
LoadState loadState = LoadState::IDLE;
std::future<Image> loadFuture; // 返回值改为内存图片
std::mutex loadMtx; 
float colorChangeTimer = 0.0f; // 加载成功后的颜色闪烁反馈计时器

// --- 视觉特效变量 (消除紫色警告：完整初始化所有成员) ---
float shakeDuration = 0.0f;
float shakeMagnitude = 0.0f;
float rippleRadius = 0.0f;
float rippleAlpha = 0.0f;
Vector2 ripplePos = { 0.0f, 0.0f }; // 显式初始化 x 和 y

// --- 音频资源 ---
Music bgm;
Sound hitPaddleFx;
Sound hitBrickFx;
Sound catchStarFx;  
Sound starDropFx;   
Sound secondBallFx; 

// ========================================================
// === 逻辑函数 ===
// ========================================================

/**
 * 绘制一个具有外发光效果的星星
 */
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
        Particle p;
        p.position = pos;
        p.speed = { (float)GetRandomValue(-200, 200) / 40.0f, (float)GetRandomValue(-200, 200) / 40.0f };
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

/**
 * 关卡初始化
 * 这里的 Map 使用全量展开排版，拒绝压缩，方便直接在代码里修改形状
 */
void InitGame(int level)
{
    paddle.rec = { (float)(SCREEN_WIDTH - INITIAL_PADDLE_WIDTH) / 2, (float)SCREEN_HEIGHT - 50, (float)INITIAL_PADDLE_WIDTH, (float)PADDLE_HEIGHT };
    paddle.color = BLUE;

    balls.clear();
    balls.push_back({ {SCREEN_WIDTH / 2.0f, paddle.rec.y - BALL_RADIUS - 5}, {0, 0}, (float)BALL_RADIUS, true });

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
        default: 
            map = { 
                "11111111111111", 
                "11111111111111" 
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
                b.rec = { startX + c * (BRICK_WIDTH + 2), startY + r * (BRICK_HEIGHT + 2), (float)BRICK_WIDTH, (float)BRICK_HEIGHT };
                b.color = BRICK_COLORS[r % 8];
                b.originalColor = b.color;
                b.active = true;
                bricks.push_back(b);
            }
        }
    }

    particles.clear();
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
    // 1. 响应按键 L 触发后台多线程异步任务
    if (IsKeyPressed(KEY_L)) 
    {
        std::lock_guard<std::mutex> lock(loadMtx);
        if (loadState != LoadState::LOADING && colorChangeTimer <= 0) 
        {
            loadState = LoadState::LOADING;
            
            // 主线程生成随机种子和主题色，按值捕获进入 Lambda
            int randOffsetX = GetRandomValue(0, 100000);
            int randOffsetY = GetRandomValue(0, 100000);
            Color themes[] = { PINK, GREEN, SKYBLUE, VIOLET, ORANGE, GOLD, LIME, MAROON };
            Color selectedTheme = themes[GetRandomValue(0, 7)];

            // 使用 std::packaged_task 对耗时计算进行打包
            std::packaged_task<Image()> task([randOffsetX, randOffsetY, selectedTheme]() -> Image 
            {
                // 生成真·随机的高清柏林噪声贴图
                Image img = GenImagePerlinNoise(1920, 1080, randOffsetX, randOffsetY, 4.0f);
                
                // 【加分项】：在后台线程直接操作像素，进行色彩合成，榨干多核性能
                ImageColorTint(&img, selectedTheme);
                
                // 满足作业要求的 2.5 秒异步加载模拟
                std::this_thread::sleep_for(std::chrono::milliseconds(2500));
                return img; 
            });
            
            loadFuture = task.get_future();
            
            // 开辟独立线程运行任务
            std::thread(std::move(task)).detach();
        }
    }

    // 2. 主线程非阻塞轮询异步状态
    if (loadState == LoadState::LOADING) 
    {
        if (loadFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) 
        {
            // 拿到子线程算好的 Image 数据
            Image loadedImg = loadFuture.get(); 
            
            // 【PPT 核心红线】：在主线程将 Image 转换为显存 Texture
            Texture2D bgTex = LoadTextureFromImage(loadedImg);
            UnloadImage(loadedImg); // 释放内存
            
            // 存入单例缓存
            TextureCache::getInstance().put("async_bg", bgTex);
            
            std::lock_guard<std::mutex> lock(loadMtx);
            loadState = LoadState::DONE;
            colorChangeTimer = 0.5f; // 0.5s 颜色反馈开始
            
            for (auto &b : bricks) 
            {
                b.color = MAGENTA; 
            }
            PlaySound(catchStarFx); 
        }
    }

    // 3. 视觉反馈计时器 (0.5s 恢复原色)
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

    // 菜单拦截
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
        // 球跟随挡板
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

        // 发球前允许预览完关卡后按 M 键返回
        if (IsKeyPressed(KEY_M))
        {
            inMenu = true;
            return;
        }
    }

    // 基础移动
    if (IsKeyDown(KEY_LEFT) && paddle.rec.x > 0) 
    {
        paddle.rec.x -= 8;
    }
    if (IsKeyDown(KEY_RIGHT) && paddle.rec.x < SCREEN_WIDTH - paddle.rec.width) 
    {
        paddle.rec.x += 8;
    }

    // 物理循环
    for (int i = 0; i < (int)balls.size(); i++)
    {
        if (!balls[i].active || !gameStarted) 
        {
            continue;
        }
            
        balls[i].pos.x += balls[i].speed.x; 
        balls[i].pos.y += balls[i].speed.y;

        // 墙壁碰撞
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

        // 挡板碰撞
        if (balls[i].speed.y > 0 && CheckCollisionCircleRec(balls[i].pos, balls[i].radius, paddle.rec))
        {
            balls[i].pos.y = paddle.rec.y - balls[i].radius; 
            balls[i].speed.y *= -1;
            float hitOffset = (balls[i].pos.x - (paddle.rec.x + paddle.rec.width / 2.0f)) / (paddle.rec.width / 2.0f);
            balls[i].speed.x = hitOffset * 6.0f; 
            PlaySound(hitPaddleFx); 
        }

        // 砖块碰撞
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
                
                if (GetActiveBrickCount() == 0) 
                { 
                    inMenu = true; 
                    gameStarted = false; 
                }
                break;
            }
        }
    }

    // 粒子效果更新
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

    // 【加分项显示】：如果缓存中有异步生成的彩色噪声背景，则绘制它
    if (TextureCache::getInstance().has("async_bg")) 
    {
        DrawTexturePro(
            TextureCache::getInstance().get("async_bg"),
            {0, 0, 1920, 1080},
            {0, 0, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT},
            {0, 0}, 0.0f, Fade(WHITE, 0.4f) 
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
        // 修复紫色警告: 显式初始化 Camera2D 的所有成员 (offset, target, rotation, zoom)
        Camera2D camera = { { 0.0f, 0.0f }, { 0.0f, 0.0f }, 0.0f, 1.0f }; 
        
        if (shakeDuration > 0) 
        {
            camera.offset = { (float)GetRandomValue(-5, 5), (float)GetRandomValue(-5, 5) };
        }
        
        BeginMode2D(camera);

        DrawRectangleRec(paddle.rec, paddle.color);
        DrawRectangleLinesEx(paddle.rec, 2, SKYBLUE);

        // 绘制实体砖块
        for (const auto &b : bricks) 
        { 
            if (b.active) 
            { 
                DrawRectangleRec(b.rec, b.color); 
                DrawRectangleLinesEx(b.rec, 1, Fade(BLACK, 0.3f)); 
            } 
        }
        
        // 绘制粒子
        for (const auto &p : particles) 
        { 
            if (p.active) 
            {
                DrawRectangleV(p.position, {3, 3}, Fade(p.color, p.alpha)); 
            }
        }
        
        // 绘制球
        for (const auto &b : balls) 
        { 
            if (b.active) 
            {
                DrawCircleV(b.pos, b.radius, WHITE); 
            }
        }

        EndMode2D();

        DrawText(TextFormat("SCORE: %05d", score), 25, 20, 25, GOLD);
        DrawText(TextFormat("LEVEL: %d", currentLevel), SCREEN_WIDTH - 150, 20, 25, LIGHTGRAY);

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

    // 异步加载时的浮层动画
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
    // ========================================================
    // === 控制台作业验证输出 ===
    // ========================================================
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

    // 加载音效 (路径已通过软链接或命令行根目录映射解决)
    bgm = LoadMusicStream("background.mp3");
    hitPaddleFx = LoadSound("paddle.wav"); 
    hitBrickFx = LoadSound("brick.wav");
    catchStarFx = LoadSound("catch-stars.wav"); 
    starDropFx = LoadSound("stars-drop.wav"); 

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