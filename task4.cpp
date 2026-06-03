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
    std::vector<Vector2> history; 
    bool isSuper; 
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
Paddle player1; 
Paddle player2; 
std::vector<Brick> bricks;
std::vector<Particle> particles;

int teamScore = 0;
int comboCount = 0;
int maxCombo = 0;
int lives = 3;             
int totalActiveBricks = 0; 
bool gameOver = false;     
bool levelCleared = false;
int lastPlayerHit = 0; 

float shakeDuration = 0.0f;
float shakeMagnitude = 0.0f;

Music bgm;
Sound hitPaddleFx;
Sound hitBrickFx;

// === 核心功能函数 ===

void DrawUIHeart(float x, float y, float size, Color color)
{
    float radius = size / 4.0f;
    DrawCircleV({ x - radius, y }, radius, color);
    DrawCircleV({ x + radius, y }, radius, color);
    Vector2 v1 = { x - size / 2.0f, y };
    Vector2 v2 = { x + size / 2.0f, y };
    Vector2 v3 = { x, y + size / 2.0f + 2.0f };
    DrawTriangle(v1, v3, v2, color);
}

void TriggerShake(float duration, float magnitude)
{
    shakeDuration = duration;
    shakeMagnitude = magnitude;
}

void CreateExplosion(Vector2 pos, Color color)
{
    for (int i = 0; i < 15; i++)
    {
        Particle p;
        p.position = pos;
        p.speed = {(float)GetRandomValue(-250, 250) / 40.0f, (float)GetRandomValue(-250, 250) / 40.0f};
        p.color = color;
        p.alpha = 1.0f;
        p.active = true;
        particles.push_back(p);
    }
}

void ResetBall()
{
    ball.pos = {SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f + 50};
    ball.speed = {4.0f, -4.0f}; 
    ball.radius = 8.0f; // <--- 致命Bug修复！把球的实体找回来了！
    ball.isSuper = false;
    ball.color = WHITE;
    ball.history.clear();
    lastPlayerHit = 0;
    comboCount = 0;
}

void InitGame()
{
    ResetBall();
    lives = 3; 
    
    player1 = {{SCREEN_WIDTH / 2.0f - 40, 480.0f, 80.0f, 20.0f}, RED};
    player2 = {{SCREEN_WIDTH / 2.0f - 70, 560.0f, 140.0f, 20.0f}, BLUE};

    bricks.clear();
    totalActiveBricks = 0;
    for (int i = 0; i < BRICK_ROWS; i++)
    {
        for (int j = 0; j < BRICK_COLS; j++)
        {
            Brick b;
            b.rec = {(float)j * (BRICK_WIDTH + 5) + 15, (float)i * (BRICK_HEIGHT + 5) + 160, (float)BRICK_WIDTH, (float)BRICK_HEIGHT};
            b.active = true;
            
            if (GetRandomValue(1, 10) <= 4) b.health = 2;
            else b.health = 1;

            float hue = (float)i / (BRICK_ROWS - 1) * 270.0f;
            b.color = ColorFromHSV(hue, 0.8f, 0.9f);
            bricks.push_back(b);
            totalActiveBricks++;
        }
    }
    
    particles.clear();
    teamScore = 0;
    maxCombo = 0;
    levelCleared = false;
    gameOver = false;
    shakeDuration = 0.0f;
}

void UpdateGame()
{
    if (shakeDuration > 0) shakeDuration -= GetFrameTime();

    if (gameOver || levelCleared)
    {
        if (IsKeyPressed(KEY_R)) InitGame();
        return;
    }

    if (IsKeyDown(KEY_A) && player1.rec.x > 0) player1.rec.x -= 8;
    if (IsKeyDown(KEY_D) && player1.rec.x < SCREEN_WIDTH - player1.rec.width) player1.rec.x += 8;
    if (IsKeyDown(KEY_LEFT) && player2.rec.x > 0) player2.rec.x -= 8;
    if (IsKeyDown(KEY_RIGHT) && player2.rec.x < SCREEN_WIDTH - player2.rec.width) player2.rec.x += 8;

    ball.history.insert(ball.history.begin(), ball.pos);
    if (ball.history.size() > 15) ball.history.pop_back();

    float currentSpeed = 4.0f + (comboCount * 0.4f);
    if (currentSpeed > 12.0f) currentSpeed = 12.0f; 
    
    float speedLen = sqrt(ball.speed.x * ball.speed.x + ball.speed.y * ball.speed.y);
    if (speedLen > 0) {
        ball.speed.x = (ball.speed.x / speedLen) * currentSpeed;
        ball.speed.y = (ball.speed.y / speedLen) * currentSpeed;
    }

    ball.pos.x += ball.speed.x;
    ball.pos.y += ball.speed.y;

    if (ball.pos.x <= ball.radius || ball.pos.x >= SCREEN_WIDTH - ball.radius) {
        ball.speed.x *= -1;
        TriggerShake(0.05f, 1.5f);
    }
    if (ball.pos.y <= ball.radius) {
        ball.speed.y *= -1;
        TriggerShake(0.05f, 1.5f);
    }

    if (ball.pos.y > SCREEN_HEIGHT + ball.radius) {
        lives--;
        TriggerShake(0.4f, 10.0f); 
        if (lives <= 0) gameOver = true;
        else ResetBall();
    }

    auto HandlePaddleCollision = [](Paddle& p, int playerID) {
        if (CheckCollisionCircleRec(ball.pos, ball.radius, p.rec)) {
            ball.pos.y = p.rec.y - ball.radius;
            ball.speed.y = -std::abs(ball.speed.y);
            if (lastPlayerHit != 0 && lastPlayerHit != playerID) {
                comboCount++;
                ball.isSuper = true;
                ball.color = GOLD; 
                if (comboCount > maxCombo) maxCombo = comboCount;
            } else if (lastPlayerHit == playerID) {
                ball.isSuper = false;
                ball.color = WHITE;
            }
            lastPlayerHit = playerID;
            PlaySound(hitPaddleFx);
            TriggerShake(0.1f, 3.0f);
        }
    };

    HandlePaddleCollision(player1, 1);
    HandlePaddleCollision(player2, 2);

    for (auto &b : bricks)
    {
        if (b.active)
        {
            if (CheckCollisionCircleRec(ball.pos, ball.radius, b.rec))
            {
                ball.speed.y *= -1;
                PlaySound(hitBrickFx);
                int damage = ball.isSuper ? 2 : 1;
                b.health -= damage;
                if (b.health <= 0)
                {
                    b.active = false;
                    totalActiveBricks--;
                    teamScore += 50 * (comboCount + 1);
                    CreateExplosion({b.rec.x + b.rec.width/2, b.rec.y + b.rec.height/2}, b.color);
                    TriggerShake(0.12f, 4.0f);
                }
                else {
                    teamScore += 10;
                    b.color = GRAY; 
                }
                break; 
            }
        }
    }
    if (totalActiveBricks <= 0) levelCleared = true;

    for (auto &p : particles) {
        if (p.active) {
            p.position.x += p.speed.x; p.position.y += p.speed.y;
            p.alpha -= 0.02f;
            if (p.alpha <= 0) p.active = false;
        }
    }
}

void DrawGame()
{
    Camera2D camera = { {0,0}, {0,0}, 0.0f, 1.0f };
    if (shakeDuration > 0) camera.offset = {(float)GetRandomValue(-shakeMagnitude, shakeMagnitude), (float)GetRandomValue(-shakeMagnitude, shakeMagnitude)};

    BeginDrawing();
    ClearBackground({15, 15, 20, 255}); 

    BeginMode2D(camera);
    
    // 1. 先画砖块（最底层）
    for (const auto &b : bricks) {
        if (b.active) {
            if (b.health == 2) {
                float glowAlpha = 0.4f + 0.4f * sinf(GetTime() * 8.0f);
                DrawRectangleLinesEx({b.rec.x - 2, b.rec.y - 2, b.rec.width + 4, b.rec.height + 4}, 2, Fade(WHITE, glowAlpha));
            }
            Color btm = b.color; btm.r *= 0.5f; btm.g *= 0.5f; btm.b *= 0.5f;
            DrawRectangleGradientV((int)b.rec.x, (int)b.rec.y, (int)b.rec.width, (int)b.rec.height, b.color, btm);
            DrawRectangleLinesEx(b.rec, 1, DARKGRAY);
        }
    }

    // 2. 画爆炸粒子
    for (const auto &p : particles) {
        if (p.active) DrawRectangleV(p.position, {3, 3}, Fade(p.color, p.alpha));
    }

    // 3. 画挡板
    DrawRectangleRec(player1.rec, player1.color);
    DrawRectangleRec(player2.rec, player2.color);

    // 4. 画球的拖尾
    for (size_t i = 0; i < ball.history.size(); i++) {
        float alpha = 1.0f - (float)i / ball.history.size();
        DrawCircleV(ball.history[i], ball.radius * (1.0f - i*0.05f), Fade(ball.color, alpha * 0.3f));
    }

    // 5. 最后画球本体
    DrawCircleV(ball.pos, ball.radius, ball.color);
    if (ball.isSuper) {
        float ringPulse = sin(GetTime() * 15.0f) * 3.0f;
        DrawCircleLines(ball.pos.x, ball.pos.y, ball.radius + 3 + ringPulse, GOLD);
    }

    EndMode2D();

    // UI 绘制
    DrawText(TextFormat("SCORE: %06i", teamScore), 25, 25, 20, RAYWHITE);
    for (int i = 0; i < 3; i++) {
        Color heartColor = (i < lives) ? RED : DARKGRAY;
        float heartPulse = (i < lives) ? sinf(GetTime() * 4.0f + i) * 2.0f : 0;
        DrawUIHeart(40 + i * 35, 65, 20 + heartPulse, heartColor);
    }

    std::string comboStr = "COMBO: " + std::to_string(comboCount);
    Color comboColor = ball.isSuper ? GOLD : LIGHTGRAY;
    float comboShake = (comboCount > 5) ? sinf(GetTime() * 20.0f) * 2.0f : 0;
    DrawText(comboStr.c_str(), 27 + comboShake, 102, 35, BLACK); 
    DrawText(comboStr.c_str(), 25 + comboShake, 100, 35, comboColor);
    
    if (levelCleared) {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.85f));
        DrawText("MISSION ACCOMPLISHED!", SCREEN_WIDTH/2 - 200, 240, 40, GOLD);
        DrawText(TextFormat("FINAL SCORE: %i", teamScore), SCREEN_WIDTH/2 - 100, 300, 25, RAYWHITE);
        DrawText(TextFormat("HIGHEST COMBO: %i", maxCombo), SCREEN_WIDTH/2 - 100, 335, 25, RAYWHITE);
        DrawText("PRESS R TO PLAY AGAIN", SCREEN_WIDTH/2 - 120, 420, 20, GRAY);
    }
    if (gameOver) {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(MAROON, 0.85f));
        DrawText("MISSION FAILED", SCREEN_WIDTH/2 - 140, 240, 40, WHITE);
        DrawText("TEAM OUT OF LIVES", SCREEN_WIDTH/2 - 90, 300, 20, LIGHTGRAY);
        DrawText("PRESS R TO TRY AGAIN", SCREEN_WIDTH/2 - 110, 400, 20, RAYWHITE);
    }
    EndDrawing();
}

int main()
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Co-op Breakout: Elite Team (3 LIVES)");
    InitAudioDevice();
    bgm = LoadMusicStream("background.mp3");
    hitPaddleFx = LoadSound("paddle.wav");
    hitBrickFx = LoadSound("brick.wav");
    PlayMusicStream(bgm);
    SetTargetFPS(60);
    InitGame();
    while (!WindowShouldClose()) {
        UpdateMusicStream(bgm);
        UpdateGame();
        DrawGame();
    }
    UnloadMusicStream(bgm); UnloadSound(hitPaddleFx); UnloadSound(hitBrickFx);
    CloseAudioDevice(); CloseWindow();
    return 0;
}