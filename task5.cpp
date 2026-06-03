#include "raylib.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>

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

const int BRICK_ROWS = 8;
const int BRICK_COLS = 14;
const int BRICK_WIDTH = 50;
const int BRICK_HEIGHT = 20;
const int BALL_RADIUS = 8;
const int POWERUP_RADIUS = 20; // 星星道具变大，现在比小球大很多，非常醒目

// 【手术刀修改】: 挡板初始长度减半 (204 -> 102)
const int INITIAL_PADDLE_WIDTH = 102;
const int PADDLE_HEIGHT = 20;
// 【手术刀修改】: 挡板最大长度限制为现在的两倍 (102 * 2 = 204)
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
// 【手术刀修改】: 取消生命机制，lives变量保留初始化结构但不再递减判定
int lives = 1;     
int bricksHit = 0; // 击碎砖块计数（单球连击计数）
bool gameOver = false;
bool gameStarted = false;
bool secondBallTriggered = false; // 变量保留以避免破坏原有初始化结构，但已不再触发双球

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

// DrawUIHeart 予以保留，虽然下方不再调用，但为了不破坏原本的函数结构
void DrawUIHeart(float x, float y, float size, Color color)
{
    float r = size / 4.0f;
    DrawCircleV({x - r, y}, r, color);
    DrawCircleV({x + r, y}, r, color);
    DrawTriangle({x - size / 2, y}, {x, y + size / 2 + 2}, {x + size / 2, y}, color);
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

int GetTotalDestroyedBricks()
{
    int activeCount = 0;
    for (const auto &b : bricks) {
        if (b.active) activeCount++;
    }
    return (BRICK_ROWS * BRICK_COLS) - activeCount;
}

void ResetBallState()
{
    balls.clear();
    paddle.rec.width = INITIAL_PADDLE_WIDTH; 
    balls.push_back({{paddle.rec.x + paddle.rec.width / 2, paddle.rec.y - BALL_RADIUS - 5}, {0, 0}, (float)BALL_RADIUS, true});
    
    bricksHit = 0; 
    secondBallTriggered = false;
    gameStarted = false;
    powerUps.clear(); 
}

void InitGame()
{
    paddle.rec = {(float)(SCREEN_WIDTH - INITIAL_PADDLE_WIDTH) / 2, (float)SCREEN_HEIGHT - 50, (float)INITIAL_PADDLE_WIDTH, (float)PADDLE_HEIGHT};
    paddle.color = BLUE;

    balls.clear();
    balls.push_back({{SCREEN_WIDTH / 2.0f, paddle.rec.y - BALL_RADIUS - 5}, {0, 0}, (float)BALL_RADIUS, true});

    bricks.clear();
    for (int row = 0; row < BRICK_ROWS; row++)
    {
        for (int col = 0; col < BRICK_COLS; col++)
        {
            ::Brick b;
            b.rec = {(float)col * (BRICK_WIDTH + 2) + 35, (float)row * (BRICK_HEIGHT + 2) + 120, (float)BRICK_WIDTH, (float)BRICK_HEIGHT};
            b.color = BRICK_COLORS[row % 8];
            b.active = true;
            bricks.push_back(b);
        }
    }

    particles.clear();
    powerUps.clear();
    score = 0;
    lives = 1; // 重置为一命
    bricksHit = 0;
    gameOver = false;
    gameStarted = false;
    secondBallTriggered = false;
    rippleAlpha = 0;
}

void UpdateGame()
{
    if (shakeDuration > 0)
        shakeDuration -= GetFrameTime();

    if (rippleAlpha > 0)
    {
        rippleRadius += 400.0f * GetFrameTime();
        rippleAlpha -= 1.0f * GetFrameTime();
    }

    if (gameOver)
    {
        if (IsKeyPressed(KEY_R))
            InitGame();
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
                // 【手术刀修改】: 回调基准发球速度，适应流畅后的手感
                balls[0].speed = {4.0f, -4.0f}; 
        }
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
            // 【手术刀修改】: 取消多条命，小球触底直接结束游戏
            TriggerShake(0.3f, 8.0f);
            gameOver = true;
            break; 
        }

        if (balls[i].speed.y > 0 && CheckCollisionCircleRec(balls[i].pos, balls[i].radius, paddle.rec))
        {
            balls[i].pos.y = paddle.rec.y - balls[i].radius;
            balls[i].speed.y *= -1;
            float hitOffset = (balls[i].pos.x - (paddle.rec.x + paddle.rec.width / 2.0f)) / (paddle.rec.width / 2.0f);
            // 【手术刀修改】: 同步下调挡板边缘的水平极速，防止暴走
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
                
                // ========================================================
                // 【实际应用作业要求 1】: 调用 ScoreCalculator 工具类增加得分
                // ========================================================
                score += globalScoreCalc.CalculateScore(1); // 1 代表普通砖块
                
                PlaySound(hitBrickFx);
                CreateExplosion({b.rec.x + b.rec.width / 2, b.rec.y + b.rec.height / 2}, b.color);
                
                balls[i].speed.y *= -1; 
                TriggerShake(0.12f, 5.0f);

                int totalDestroyed = GetTotalDestroyedBricks();
                int dropChance = (totalDestroyed <= 40) ? 80 : 15; 

                if (GetRandomValue(1, 100) <= dropChance)
                {
                    powerUps.push_back({{b.rec.x + b.rec.width / 2, b.rec.y}, 3.5f, true});
                    PlaySound(starDropFx);
                }

                // 【已移除双球逻辑】: 保持单球小游戏体验
                
                // === 【新增胜利判断】：如果全部砖块都打完了，直接结束游戏 ===
                if (totalDestroyed == BRICK_ROWS * BRICK_COLS)
                {
                    gameOver = true;
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
            
            // 【手术刀修改】: 挡板到达上限后只加分，未到上限则加分并延长挡板
            score += 10; 
            
            if (paddle.rec.width < MAX_PADDLE_WIDTH)
            {
                // 每次接到星星，挡板增加 34 像素（接3颗星星即可达到最大长度204）
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
    Camera2D camera = {0};
    if (shakeDuration > 0)
        camera.offset = {(float)GetRandomValue(-shakeMagnitude, shakeMagnitude), (float)GetRandomValue(-shakeMagnitude, shakeMagnitude)};
    else
        camera.offset = {0, 0};
    
    camera.target = {0, 0};
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    BeginDrawing();
    ClearBackground({10, 10, 15, 255});

    if (gameOver)
    {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.8f));
        
        // === 【新增胜利UI判断】：根据清空的砖块数量决定显示文字 ===
        if (GetTotalDestroyedBricks() == BRICK_ROWS * BRICK_COLS) {
            DrawText("YOU WIN!", SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 - 40, 45, GREEN);
        } else {
            DrawText("GAME OVER", SCREEN_WIDTH / 2 - 120, SCREEN_HEIGHT / 2 - 40, 45, RED);
        }
        
        DrawText(TextFormat("FINAL SCORE: %05d", score), SCREEN_WIDTH / 2 - 110, SCREEN_HEIGHT / 2 + 20, 20, WHITE);
        DrawText("PRESS R TO RESTART", SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 + 60, 20, GRAY);
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

        // 【手术刀修改】: 发光特效批处理（Batching）。分离渲染层，只需 1 次状态切换，消灭 Draw Call 引起的掉帧
        BeginBlendMode(BLEND_ADDITIVE);
        for (const auto &b : bricks)
        {
            if (b.active)
            {
                for (float glow = 1.0f; glow <= 4.0f; glow += 1.0f) {
                    Rectangle glowRec = {
                        b.rec.x - glow * 2.0f,
                        b.rec.y - glow * 2.0f,
                        b.rec.width + glow * 4.0f,
                        b.rec.height + glow * 4.0f
                    };
                    DrawRectangleRec(glowRec, Fade(b.color, 0.15f / glow));
                }
            }
        }
        EndBlendMode(); // 恢复 Alpha 混合

        // 实体砖块绘制层（独立处理）
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
        
        // 【手术刀修改】: 取消画右上角的爱心生命值，既然一击必杀，UI也就不需要这个冗余元素了

        if (!gameStarted)
        {
            DrawText("PRESS SPACE TO START", SCREEN_WIDTH / 2 - 120, SCREEN_HEIGHT / 2 + 100, 20, RAYWHITE);
        }
    }

    EndDrawing();
}

int main()
{
    // ========================================================
    // 【作业控制台输出展示区】
    // 游戏窗口弹出前，先在控制台把老师要求的打印输出跑一遍
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
    InitGame();

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