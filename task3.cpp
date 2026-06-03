#include "raylib.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

// === 屏幕与游戏基础配置 ===
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

const int BRICK_ROWS = 6;
const int BRICK_COLS = 12;
const int BRICK_WIDTH = 60;
const int BRICK_HEIGHT = 20;

// === 核心数据结构 ===
struct Ball
{
    Vector2 pos;
    Vector2 speed;
    float radius;
    Color color;
    std::vector<Vector2> history; // 记录历史位置用于拖尾
};

struct Paddle
{
    Rectangle rec;
    Color color;
};

struct Brick
{
    Rectangle rec;
    int health;
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

// === 全局变量 ===
Ball ball;
Paddle playerTop;    
Paddle playerBottom; 
std::vector<Brick> bricks;
std::vector<Particle> particles;

std::string winnerText = "";
bool gameOver = false;

// 抖动效果变量
float shakeDuration = 0.0f;
float shakeMagnitude = 0.0f;

enum LastHit
{
    NONE,
    TOP,
    BOTTOM
};
LastHit lastHitBy = NONE;

// 音频资源变量
Music bgm;
Sound hitPaddleFx;
Sound hitBrickFx;

// === 核心辅助功能 ===

// 触发屏幕抖动
void TriggerShake(float duration, float magnitude)
{
    shakeDuration = duration;
    shakeMagnitude = magnitude;
}

// 创建爆炸粒子
void CreateExplosion(Vector2 pos, Color color)
{
    for (int i = 0; i < 15; i++)
    {
        Particle p;
        p.position = pos;
        p.speed = {(float)GetRandomValue(-150, 150) / 30.0f, (float)GetRandomValue(-150, 150) / 30.0f};
        p.color = color;
        p.alpha = 1.0f;
        p.active = true;
        particles.push_back(p);
    }
}

// === 游戏初始化 ===
void InitGame()
{
    ball.pos = {SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f};
    ball.speed = {4.0f, 4.0f};
    ball.radius = 8.0f;
    ball.color = WHITE;
    ball.history.clear(); // 清空拖尾记录
    lastHitBy = NONE;

    playerTop = {{SCREEN_WIDTH / 2.0f - 40, 450.0f, 80.0f, 20.0f}, RED};
    playerBottom = {{SCREEN_WIDTH / 2.0f - 70, 550.0f, 140.0f, 20.0f}, BLUE};

    bricks.clear();
    for (int i = 0; i < BRICK_ROWS; i++)
    {
        for (int j = 0; j < BRICK_COLS; j++)
        {
            Brick b;
            b.rec = {(float)j * (BRICK_WIDTH + 5) + 15, (float)i * (BRICK_HEIGHT + 5) + 70, (float)BRICK_WIDTH, (float)BRICK_HEIGHT};
            b.active = true;
            b.health = (i == 0) ? 2 : 1;
            float hue = (float)i / (BRICK_ROWS - 1) * 270.0f;
            b.color = ColorFromHSV(hue, 0.8f, 0.9f);
            bricks.push_back(b);
        }
    }
    
    particles.clear();
    gameOver = false;
    winnerText = "";
    shakeDuration = 0.0f;
}

// === 游戏逻辑更新 ===
void UpdateGame()
{
    if (shakeDuration > 0) shakeDuration -= GetFrameTime();

    if (gameOver)
    {
        if (IsKeyPressed(KEY_R)) InitGame();
        return;
    }

    // 玩家控制
    if (IsKeyDown(KEY_A) && playerTop.rec.x > 0) playerTop.rec.x -= 8;
    if (IsKeyDown(KEY_D) && playerTop.rec.x < SCREEN_WIDTH - playerTop.rec.width) playerTop.rec.x += 8;
    if (IsKeyDown(KEY_LEFT) && playerBottom.rec.x > 0) playerBottom.rec.x -= 8;
    if (IsKeyDown(KEY_RIGHT) && playerBottom.rec.x < SCREEN_WIDTH - playerBottom.rec.width) playerBottom.rec.x += 8;

    // --- 记录拖尾位置 ---
    ball.history.insert(ball.history.begin(), ball.pos);
    if (ball.history.size() > 12) ball.history.pop_back();

    // 球体运动
    ball.pos.x += ball.speed.x;
    ball.pos.y += ball.speed.y;

    if (ball.pos.x <= ball.radius || ball.pos.x >= SCREEN_WIDTH - ball.radius) {
        ball.speed.x *= -1;
        TriggerShake(0.05f, 2.0f);
    }

    // 无限回旋：穿越边界时清空拖尾，防止出现横跨屏幕的长线
    if (ball.pos.y < -ball.radius) {
        ball.pos.y = SCREEN_HEIGHT + ball.radius;
        ball.history.clear();
    }
    if (ball.pos.y > SCREEN_HEIGHT + ball.radius) {
        ball.pos.y = -ball.radius;
        ball.history.clear();
    }

    // 挡板碰撞
    if (CheckCollisionCircleRec(ball.pos, ball.radius, playerTop.rec))
    {
        ball.pos.y = playerTop.rec.y - ball.radius; 
        ball.speed.y = -std::abs(ball.speed.y);
        lastHitBy = TOP;
        ball.color = RED;
        PlaySound(hitPaddleFx);
        TriggerShake(0.1f, 4.0f);
    }
    if (CheckCollisionCircleRec(ball.pos, ball.radius, playerBottom.rec))
    {
        ball.pos.y = playerBottom.rec.y - ball.radius;
        ball.speed.y = -std::abs(ball.speed.y);
        lastHitBy = BOTTOM;
        ball.color = BLUE; 
        PlaySound(hitPaddleFx);
        TriggerShake(0.1f, 4.0f);
    }

    // 砖块碰撞
    int remainingBricks = 0;
    for (auto &b : bricks)
    {
        if (b.active)
        {
            remainingBricks++;
            if (CheckCollisionCircleRec(ball.pos, ball.radius, b.rec))
            {
                ball.speed.y *= -1;
                b.health--;
                PlaySound(hitBrickFx);
                TriggerShake(0.12f, 5.0f);
                
                if (b.health <= 0)
                {
                    b.active = false;
                    remainingBricks--;
                    CreateExplosion({b.rec.x + b.rec.width/2, b.rec.y + b.rec.height/2}, b.color);
                    
                    float speedIncrease = 1.0f + (1.0f - (float)remainingBricks / (BRICK_ROWS * BRICK_COLS)) * 0.5f;
                    ball.speed.x = (ball.speed.x > 0 ? 4.0f : -4.0f) * speedIncrease;
                    ball.speed.y = (ball.speed.y > 0 ? 4.0f : -4.0f) * speedIncrease;

                    if (remainingBricks == 0)
                    {
                        gameOver = true;
                        winnerText = (lastHitBy == TOP) ? "RED WINS! (TOP)" : "BLUE WINS! (BOTTOM)";
                    }
                }
                else b.color = GRAY; 
                break;
            }
        }
    }

    // 更新粒子
    for (auto &p : particles)
    {
        if (p.active)
        {
            p.position.x += p.speed.x;
            p.position.y += p.speed.y;
            p.alpha -= 0.02f;
            if (p.alpha <= 0) p.active = false;
        }
    }
    particles.erase(std::remove_if(particles.begin(), particles.end(), [](const Particle &p) { return !p.active; }), particles.end());
}

// === 画面渲染 ===
void DrawGame()
{
    Camera2D camera = { 0 };
    camera.target = (Vector2){ 0, 0 };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;
    if (shakeDuration > 0) camera.offset = (Vector2){(float)GetRandomValue(-shakeMagnitude, shakeMagnitude), (float)GetRandomValue(-shakeMagnitude, shakeMagnitude)};
    else camera.offset = (Vector2){ 0, 0 };

    BeginDrawing();
    ClearBackground(BLACK);

    BeginMode2D(camera);
    if (!gameOver)
    {
        // 1. 绘制拖尾效果 (要在球体之前画，这样拖尾在球后面)
        for (size_t i = 0; i < ball.history.size(); i++)
        {
            float alpha = 1.0f - (float)i / ball.history.size();
            float scale = 1.0f - (float)i / ball.history.size() * 0.5f;
            DrawCircleV(ball.history[i], ball.radius * scale, Fade(ball.color, alpha * 0.4f));
        }

        // 2. 绘制挡板和球
        DrawRectangleRec(playerTop.rec, playerTop.color);
        DrawRectangleRec(playerBottom.rec, playerBottom.color);
        DrawCircleV(ball.pos, ball.radius, ball.color);

        // 3. 绘制砖块
        for (const auto &b : bricks)
        {
            if (b.active)
            {
                Color bottomColor = b.color;
                bottomColor.r *= 0.6f; bottomColor.g *= 0.6f; bottomColor.b *= 0.6f;
                DrawRectangleGradientV((int)b.rec.x, (int)b.rec.y, (int)b.rec.width, (int)b.rec.height, b.color, bottomColor);
                DrawRectangleLinesEx(b.rec, 1, DARKGRAY);
            }
        }

        // 4. 绘制粒子
        for (const auto &p : particles)
        {
            if (p.active) DrawRectangleV(p.position, {4, 4}, Fade(p.color, p.alpha));
        }
    }
    EndMode2D();

    if (!gameOver)
    {
        DrawText("TOP PLAYER: A/D", 10, 10, 20, RED);
        DrawText("BOTTOM PLAYER: LEFT/RIGHT", 10, SCREEN_HEIGHT - 30, 20, BLUE);
    }
    else
    {
        DrawText(winnerText.c_str(), SCREEN_WIDTH / 2 - MeasureText(winnerText.c_str(), 40) / 2, SCREEN_HEIGHT / 2 - 20, 40, ball.color);
        DrawText("PRESS R TO RESTART", SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 + 40, 20, LIGHTGRAY);
    }

    EndDrawing();
}

int main()
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Versus Breakout - Extreme Edition (With Trail)");
    InitAudioDevice();

    bgm = LoadMusicStream("background.mp3");
    hitPaddleFx = LoadSound("paddle.wav");
    hitBrickFx = LoadSound("brick.wav");

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
    CloseAudioDevice();
    CloseWindow();
    return 0;
}