#include "raylib.h"
#include <math.h>

// --- 常量定义 ---
const int screenWidth = 800;
const int screenHeight = 450;

struct Slime {
    Vector2 position;
    Vector2 speed;
    float radius;
    Color color;
    bool isGrounded;
    float stretch; // 用于实现史莱姆的挤压和拉伸效果 🧪
};

int main() {
    // 初始化窗口
    InitWindow(screenWidth, screenHeight, "Slime Adventure - Raylib");

    Slime player = { 
        { screenWidth / 2.0f, screenHeight / 2.0f }, 
        { 0, 0 }, 
        25.0f, 
        LIME, 
        false, 
        1.0f 
    };

    float gravity = 0.5f;
    SetTargetFPS(60);

    // --- 游戏主循环 ---
    while (!WindowShouldClose()) {
        // 1. 处理输入 ⌨️
        if (IsKeyDown(KEY_LEFT)) player.speed.x = -4.0f;
        else if (IsKeyDown(KEY_RIGHT)) player.speed.x = 4.0f;
        else player.speed.x = 0;

        // 跳跃逻辑：只有在地面上才能跳
        if (IsKeyPressed(KEY_SPACE) && player.isGrounded) {
            player.speed.y = -12.0f;
            player.isGrounded = false;
        }

        // 2. 物理更新 ⚙️
        player.speed.y += gravity; // 应用重力
        player.position.x += player.speed.x;
        player.position.y += player.speed.y;

        // 地面碰撞检测
        if (player.position.y + player.radius >= screenHeight - 20) {
            player.position.y = screenHeight - 20 - player.radius;
            player.speed.y = 0;
            player.isGrounded = true;
        }

        // 简单的视觉反馈：根据速度改变形状
        if (!player.isGrounded) {
            // 空中拉伸
            player.stretch = 0.8f + (fabs(player.speed.y) / 40.0f);
        } else {
            // 地面恢复正常或挤压
            player.stretch = 1.0f;
        }

        // 3. 绘制图像 🎨
        BeginDrawing();
            ClearBackground(RAYWHITE);

            DrawText("左右键移动，空格键跳跃!", 10, 10, 20, GRAY);
            
            // 绘制“地面”
            DrawRectangle(0, screenHeight - 20, screenWidth, 20, DARKGRAY);

            // 绘制史莱姆 (使用椭圆模拟变形)
            DrawEllipse(
                (int)player.position.x, 
                (int)player.position.y, 
                player.radius * (2.0f - player.stretch), // 宽度
                player.radius * player.stretch,           // 高度
                player.color
            );
            
            // 给史莱姆加两只小眼睛 👀
            DrawCircle((int)player.position.x - 8, (int)player.position.y - 5, 3, BLACK);
            DrawCircle((int)player.position.x + 8, (int)player.position.y - 5, 3, BLACK);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}