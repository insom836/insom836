#include <iostream>
#include <deque>
#include <raylib.h>
#include <raymath.h>

using namespace std;

// 定义颜色
Color green = {173, 204, 96, 255};
Color darkGreen = {43, 51, 24, 255};

int cellSize = 30; // 每个方格的大小
int cellCount = 20; // 画面有多少个方格

// 食物类
class Food {
public:
    Vector2 position;
    Texture2D texture;

    Food(deque<Vector2> snakeBody) {
        Image image = LoadImage("food.png"); // 确保你有这张图，或改用 DrawRectangle
        texture = LoadTextureFromImage(image);
        UnloadImage(image);
        position = GenerateRandomPos(snakeBody);
    }

    ~Food() {
        UnloadTexture(texture);
    }

    void Draw() {
        DrawRectangle(position.x * cellSize, position.y * cellSize, cellSize, cellSize, RED);
    }

    Vector2 GenerateRandomPos(deque<Vector2> snakeBody) {
        float x = GetRandomValue(0, cellCount - 1);
        float y = GetRandomValue(0, cellCount - 1);
        return {x, y};
    }
};

// 蛇类
class Snake {
public:
    deque<Vector2> body = {{6, 9}, {5, 9}, {4, 9}};
    Vector2 direction = {1, 0};
    bool addSegment = false;

    void Draw() {
        for (const auto& segment : body) {
            Rectangle rect = {segment.x * cellSize, segment.y * cellSize, (float)cellSize, (float)cellSize};
            DrawRectangleRounded(rect, 0.5, 6, darkGreen);
        }
    }

    void Update() {
        body.push_front(Vector2Add(body[0], direction));
        if (addSegment) {
            addSegment = false;
        } else {
            body.pop_back();
        }
    }
};

int main() {
    InitWindow(cellSize * cellCount, cellSize * cellCount, "Raylib Snake Game 🐍");
    SetTargetFPS(60);

    Snake snake = Snake();
    Food food = Food(snake.body);
    
    double lastUpdateTime = 0;

    // 游戏主循环
    while (!WindowShouldClose()) {
        BeginDrawing();

        // 1. 输入控制
        if (IsKeyPressed(KEY_UP) && snake.direction.y != 1) snake.direction = {0, -1};
        if (IsKeyPressed(KEY_DOWN) && snake.direction.y != -1) snake.direction = {0, 1};
        if (IsKeyPressed(KEY_LEFT) && snake.direction.x != 1) snake.direction = {-1, 0};
        if (IsKeyPressed(KEY_RIGHT) && snake.direction.x != -1) snake.direction = {1, 0};

        // 2. 逻辑更新 (控制蛇移动速度)
        if (GetTime() - lastUpdateTime > 0.2) {
            snake.Update();
            lastUpdateTime = GetTime();
        }

        // 3. 碰撞检测 (蛇吃食物)
        if (Vector2Equals(snake.body[0], food.position)) {
            food.position = food.GenerateRandomPos(snake.body);
            snake.addSegment = true;
        }

        // 4. 边界和自身碰撞检测
        if (snake.body[0].x >= cellCount || snake.body[0].x < 0 || snake.body[0].y >= cellCount || snake.body[0].y < 0) {
            // 这里可以添加 Game Over 逻辑
            snake.body = {{6, 9}, {5, 9}, {4, 9}}; 
            snake.direction = {1, 0};
        }

        // 渲染
        ClearBackground(green);
        food.Draw();
        snake.Draw();

        EndDrawing();
    }

    CloseWindow();
    return 0;
}