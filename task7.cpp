#include "raylib.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>
// 【手术刀修改】: 引入多线程与异步任务所需的标准库 (PPT 第八周要求)
#include <thread>
#include <future>
#include <mutex>
#include <chrono>

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

// 全局得分计算器，将在下方 UpdateGame 中实际使用
ScoreCalculator globalScoreCalc; 


// ========================================================
// === 【作业要求 2】: 类的继承、虚函数与多态演示区块 ===
// (使用 namespace 隔离，确保绝不影响下方实际游戏核心逻辑的结构体)
// ========================================================
namespace OOP_Task {
    // 抽象基类
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
// === 以下为你原本的游戏核心代码 (完全保留原有功能) ===
// ========================================================

// === 屏幕与基础配置 ===
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

const int BRICK_WIDTH = 50;
const int BRICK_HEIGHT = 20;
const int BALL_RADIUS = 8;
const int POWERUP_RADIUS = 20; // 星星道具变大，现在比小球大很多，非常醒目

// 挡板初始长度减半 (204 -> 102)
const int INITIAL_PADDLE_WIDTH = 102;
const int PADDLE_HEIGHT = 20;
// 挡板最大长度限制为现在的两倍 (102 * 2 = 204)
const float MAX_PADDLE_WIDTH = INITIAL_PADDLE_WIDTH * 2.0f; 

// === 颜色配置 ===
const Color BRICK_COLORS[] = {RED, ORANGE, YELLOW, GREEN, BLUE, PURPLE, PINK, LIGHTGRAY};

// === 核心结构体 ===
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
    Color originalColor; // 用于在加载指示结束后恢复颜色
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

// === 全局变量 ===
std::vector<Ball> balls;
::Paddle paddle; // 使用全局命名空间的 Paddle
std::vector<::Brick> bricks;
std::vector<Particle> particles;
std::vector<PowerUp> powerUps;

int score = 0;
int lives = 1;     
int bricksHit = 0; 
bool gameOver = false;
bool gameStarted = false;
bool secondBallTriggered = false; 

// 引入关卡与菜单状态机
bool inMenu = true;
int currentLevel = 0; // 默认游标
float powerUpCooldown = 0.0f; // 控制星星基于时间均匀掉落的冷却计时器

// 第八周作业 - 多线程异步加载状态机与共享资源保护
enum class LoadState { IDLE, LOADING, DONE };
LoadState loadState = LoadState::IDLE;
std::future<int> loadFuture;
std::mutex loadMtx; // 互斥锁，用于保护跨线程状态读取与写入
float colorChangeTimer = 0.0f; // 用于控制加载成功后“变红反馈”的时长 (0.5s)

// 模拟耗时任务：后台线程独立执行，绝不干扰主线程
int AsyncResourceLoader() {
    // 恢复到老师要求的 2.5 秒时长
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    return 999; // 假装返回了一个纹理缓存 ID
}

// 视觉特效变量
float shakeDuration = 0.0f;
float shakeMagnitude = 0.0f;
float rippleRadius = 0.0f;
float rippleAlpha = 0.0f;
Vector2 ripplePos = {0};

// 音频
Music bgm;
Sound hitPaddleFx;
Sound hitBrickFx;
Sound catchStarFx;  
Sound starDropFx;   
Sound secondBallFx; 

// === 功能函数 ===

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

// 废弃基于常量的总数计算，改为动态遍历当前存活砖块
int GetActiveBrickCount()
{
    int activeCount = 0;
    for (const auto &b : bricks) {
        if (b.active) activeCount++;
    }
    return activeCount;
}

void InitGame(int level)
{
    paddle.rec = {(float)(SCREEN_WIDTH - INITIAL_PADDLE_WIDTH) / 2, (float)SCREEN_HEIGHT - 50, (float)INITIAL_PADDLE_WIDTH, (float)PADDLE_HEIGHT};
    paddle.color = BLUE;

    balls.clear();
    balls.push_back({{SCREEN_WIDTH / 2.0f, paddle.rec.y - BALL_RADIUS - 5}, {0, 0}, (float)BALL_RADIUS, true});

    // 基于二维字符串数组的动态关卡解析器
    std::vector<std::string> map;
    switch (level) {
        case 0: // 新增 Level 0 试玩关卡，仅需一行 6 个砖块字符
            map = {
                "111111"
            };
            break;
        case 1: // 第一关：原版长方形
            map = {
                "11111111111111", "11111111111111", "11111111111111", "11111111111111",
                "11111111111111", "11111111111111", "11111111111111", "11111111111111"
            };
            break;
        case 2: // 第二关：爱心（高度更高）
            map = {
                "..1111..1111..", ".111111111111.", "11111111111111", "11111111111111",
                "11111111111111", ".111111111111.", "..1111111111..", "...11111111...",
                "....111111....", ".....1111.....", "......11......"
            };
            break;
        case 3: // 第三关：镂空的五角星
            // 将行数暴增至 14 行，抵消 50x20 砖块的“宽扁”长宽比，拉出锐利的 72 度星尖
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
        case 4: // 第四关：四叶草
            map = {
                "...111..111...", "..1111111111..", "..1111..1111..", "...11....11...",
                "1111..11..1111", "11111111111111", "1111..11..1111", "...11....11...",
                "..1111..1111..", "..1111111111..", "...111..111...", "......11......"
            };
            break;
        case 5: // 第五关：小房子
            map = {
                "......11......", ".....1..1.....", "....1....1....", "...1......1...",
                "..1........1..", ".111111111111.", "...1......1...", "...1......1...",
                "...1..11..1...", "...1..11..1...", "...11111111..."
            };
            break;
    }

    bricks.clear();
    int rows = map.size();
    int cols = map[0].size();
    // 动态居中计算 (无论是一行 6 个，还是 14x14 的大阵列，都会基于这一行物理居中)
    float startX = (SCREEN_WIDTH - cols * (BRICK_WIDTH + 2)) / 2.0f;
    float startY = 80.0f; 

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            if (map[r][c] == '1') {
                ::Brick b;
                b.rec = {startX + c * (BRICK_WIDTH + 2), startY + r * (BRICK_HEIGHT + 2), (float)BRICK_WIDTH, (float)BRICK_HEIGHT};
                b.color = BRICK_COLORS[r % 8];
                b.originalColor = b.color; // 初始化时记录原始颜色
                b.active = true;
                bricks.push_back(b);
            }
        }
    }

    particles.clear();
    powerUps.clear();
    // 每次进入关卡不重置总分数，保持累计得分体验，但重置生命与状态
    lives = 1; 
    bricksHit = 0;
    powerUpCooldown = 3.0f; // 开局给 3 秒缓冲期，防止刚发球就掉落
    gameOver = false;
    gameStarted = false;
    secondBallTriggered = false;
    rippleAlpha = 0;
    
    // 每次初始化关卡时，安全重置加载任务状态
    {
        std::lock_guard<std::mutex> lock(loadMtx);
        loadState = LoadState::IDLE;
        colorChangeTimer = 0.0f; // 重置变色计时器
    }
}

void UpdateGame()
{
    // 1. 响应作业要求，按 L 键触发后台多线程异步加载
    if (IsKeyPressed(KEY_L)) {
        std::lock_guard<std::mutex> lock(loadMtx);
        // 如果不在加载中且不在反馈期间，允许重新启动加载
        if (loadState != LoadState::LOADING && colorChangeTimer <= 0) {
            loadState = LoadState::LOADING;
            // 采用 std::launch::async 强制开辟新工作线程
            loadFuture = std::async(std::launch::async, AsyncResourceLoader);
        }
    }

    // 2. 主线程每帧非阻塞轮询，检测多线程是否跑完
    if (loadState == LoadState::LOADING) {
        // wait_for(0) 瞬间返回，不卡主线程
        if (loadFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            loadFuture.get(); // 提取子线程返回值
            
            std::lock_guard<std::mutex> lock(loadMtx);
            loadState = LoadState::DONE;
            
            // 加载完成，启动 0.5 秒的变色倒计时，并立即变色
            colorChangeTimer = 0.5f; 
            for (auto &b : bricks) {
                b.color = MAGENTA; // 全部强刷为极其显眼的洋红色
            }
            PlaySound(catchStarFx); 
        }
    }

    // 3. 颜色反馈计时器管理 (让指示指示持续 0.5 秒)
    if (colorChangeTimer > 0) {
        colorChangeTimer -= GetFrameTime();
        if (colorChangeTimer <= 0) {
            // 0.5 秒时间到，恢复所有砖块的原始色彩，重置加载状态
            for (auto &b : bricks) {
                b.color = b.originalColor;
            }
            loadState = LoadState::IDLE; 
        }
    }

    // 拦截菜单状态，检测关卡点击
    if (inMenu)
    {
        Vector2 mousePos = GetMousePosition();
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            // 循环区间从 5 增加到 6，囊括 Level 0
            for (int i = 0; i <= 5; i++) {
                // 微调了按钮高度(45)与间距(60)，确保 6 个按钮能优雅放在同一屏内
                Rectangle btn = { (float)SCREEN_WIDTH / 2 - 100, 140.0f + i * 60.0f, 200, 45 };
                if (CheckCollisionPointRec(mousePos, btn)) {
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
        shakeDuration -= GetFrameTime();

    if (rippleAlpha > 0)
    {
        rippleRadius += 400.0f * GetFrameTime();
        rippleAlpha -= 1.0f * GetFrameTime();
    }

    if (gameOver)
    {
        if (IsKeyPressed(KEY_R)) InitGame(currentLevel);
        // 允许死亡后返回主菜单
        if (IsKeyPressed(KEY_M)) {
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
                balls[0].speed = {4.0f, -4.0f}; 
        }
    }

    // 只有在游戏进行时，计算道具掉落的冷却时间
    if (gameStarted && powerUpCooldown > 0) {
        powerUpCooldown -= GetFrameTime();
    }

    if (IsKeyDown(KEY_LEFT) && paddle.rec.x > 0)
        paddle.rec.x -= 8;
    if (IsKeyDown(KEY_RIGHT) && paddle.rec.x < SCREEN_WIDTH - paddle.rec.width)
        paddle.rec.x += 8;

    for (int i = 0; i < (int)balls.size(); i++)
    {
        if (!balls[i].active || !gameStarted)
            continue;

        balls[i].pos.x += balls[i].speed.x;
        balls[i].pos.y += balls[i].speed.y;

        if (balls[i].pos.x <= balls[i].radius)
        {
            balls[i].pos.x = balls[i].radius;
            balls[i].speed.x *= -1;
            TriggerShake(0.05f, 2.0f);
        }
        else if (balls[i].pos.x >= SCREEN_WIDTH - balls[i].radius)
        {
            balls[i].pos.x = SCREEN_WIDTH - balls[i].radius;
            balls[i].speed.x *= -1;
            TriggerShake(0.05f, 2.0f);
        }

        if (balls[i].pos.y <= balls[i].radius)
        {
            balls[i].pos.y = balls[i].radius;
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

                // 废除按剩余砖块比例的前期集中掉落，改为时间冷却机制
                if (powerUpCooldown <= 0.0f)
                {
                    powerUps.push_back({{b.rec.x + b.rec.width / 2, b.rec.y}, 3.5f, true});
                    PlaySound(starDropFx);
                    powerUpCooldown = (float)GetRandomValue(5, 8); // 触发后，进入 5 ~ 8 秒的随机冷却
                }
                
                // === 【新增胜利判断】：过关后自动返回主菜单 ===
                int activeCount = GetActiveBrickCount();
                if (activeCount == 0)
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
            p.active = false;
    }

    for (auto &p : particles)
    {
        if (p.active)
        {
            p.position.x += p.speed.x;
            p.position.y += p.speed.y;
            p.alpha -= 0.02f;
            if (p.alpha <= 0)
                p.active = false;
        }
    }
}

void DrawGame()
{
    BeginDrawing();
    ClearBackground({10, 10, 15, 255});

    // 绘制独立的全英文菜单界面
    if (inMenu)
    {
        // 标题颜色现在仅在计时器活动期间变色
        Color titleColor = (colorChangeTimer > 0) ? MAGENTA : GOLD; 
        DrawText("LEVEL SELECT", SCREEN_WIDTH / 2 - 135, 60, 40, titleColor);
        DrawText(TextFormat("TOTAL SCORE: %05d", score), SCREEN_WIDTH / 2 - 100, 105, 20, LIGHTGRAY);
        
        Vector2 mousePos = GetMousePosition();
        // 按钮渲染循环拓展，并增加针对 Level 0 的文本特判
        for (int i = 0; i <= 5; i++) {
            Rectangle btn = { (float)SCREEN_WIDTH / 2 - 100, 140.0f + i * 60.0f, 200, 45 };
            bool hover = CheckCollisionPointRec(mousePos, btn);
            DrawRectangleRec(btn, hover ? DARKBLUE : BLUE);
            DrawRectangleLinesEx(btn, 2, SKYBLUE);
            
            if (i == 0) {
                DrawText("LEVEL 0 (TRIAL)", btn.x + 20, btn.y + 12, 20, RAYWHITE);
            } else {
                DrawText(TextFormat("LEVEL %d", i), btn.x + 55, btn.y + 12, 20, RAYWHITE);
            }
        }
    }
    else
    {
        Camera2D camera = {0};
        if (shakeDuration > 0)
            camera.offset = {(float)GetRandomValue(-shakeMagnitude, shakeMagnitude), (float)GetRandomValue(-shakeMagnitude, shakeMagnitude)};
        else
            camera.offset = {0, 0};
        
        camera.target = {0, 0};
        camera.rotation = 0.0f;
        camera.zoom = 1.0f;

        if (gameOver)
        {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.8f));
            DrawText("GAME OVER", SCREEN_WIDTH / 2 - 120, SCREEN_HEIGHT / 2 - 40, 45, RED);
            DrawText(TextFormat("FINAL SCORE: %05d", score), SCREEN_WIDTH / 2 - 110, SCREEN_HEIGHT / 2 + 20, 20, WHITE);
            // 新增菜单返回提示
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

            // 发光特效批处理，强化第五关的光污染视觉干扰
            BeginBlendMode(BLEND_ADDITIVE);
            
            int glowLayers = (currentLevel == 5) ? 8 : 4; 
            float glowSpread = (currentLevel == 5) ? 3.0f : 2.0f;
            // 动态呼吸函数，让光污染更刺眼
            float shimmerIntensity = (sinf(GetTime() * 15.0f) * 0.5f) + 0.5f; 

            for (const auto &b : bricks)
            {
                if (b.active)
                {
                    for (float glow = 1.0f; glow <= glowLayers; glow += 1.0f) {
                        Rectangle glowRec = {
                            b.rec.x - glow * glowSpread,
                            b.rec.y - glow * glowSpread,
                            b.rec.width + glow * glowSpread * 2.0f,
                            b.rec.height + glow * glowSpread * 2.0f
                        };
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
                    float shimmer = (sinf(GetTime() * 20.0f) * 0.5f) + 0.5f;
                    DrawGeminiStar(p.pos, POWERUP_RADIUS, shimmer);
                }
            }

            for (const auto &p : particles)
            {
                if (p.active)
                    DrawRectangleV(p.position, {3, 3}, Fade(p.color, p.alpha));
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
            }
        }
    }

    // 渲染异步 Loading 动画。挪到外层绘制，保证在菜单界面按 L 也能立刻看到闪烁。
    if (loadState == LoadState::LOADING) {
        float alpha = (sinf(GetTime() * 8.0f) * 0.5f) + 0.5f; // 高频呼吸灯效果
        DrawRectangle(SCREEN_WIDTH - 240, SCREEN_HEIGHT - 50, 220, 40, Fade(BLACK, 0.8f));
        DrawText("ASYNC LOADING...", SCREEN_WIDTH - 230, SCREEN_HEIGHT - 40, 20, Fade(GREEN, alpha));
    }

    EndDrawing();
}

int main()
{
    // ========================================================
    // 【作业控制台输出展示区】
    // ========================================================
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
    for (auto obj : hwObjects) {
        obj->Update();
        obj->Draw();
    }

    std::cout << "\n>>> 释放内存，测试虚析构：\n";
    for (auto obj : hwObjects) {
        delete obj;
    }
    std::cout << "\n[ 控制台作业验证完毕，开始启动图形化游戏主程序... ]\n\n";

    
    // ========================================================
    // 【原有游戏启动逻辑】
    // ========================================================
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Gemini Breakout: Dual Core");
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
    // 取消在 main 中的强制开局，让程序平滑进入 inMenu 状态
    InitGame(0);

    while (!WindowShouldClose())
    {
        UpdateMusicStream(bgm);
        UpdateGame();
        DrawGame();
    }

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