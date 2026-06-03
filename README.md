# 🌟 Hi there, I'm insom836! 

Welcome to my GitHub profile. I am a Computer Science student at Sichuan University (SCU), passionate about C++ software architecture, game development, and algorithmic optimization. 

---

## 🎮 Featured Project: Breakout - Dual Core (Max Edition)
> **Branch Note:** The single-player core and asset management are in the `main` branch, while the multiplayer ENet implementation is maintained in the `feature/networking` branch.

This is a hardcore, highly optimized 2D Breakout game built from scratch using **C++** and **Raylib**. It goes beyond classic mechanics by introducing data-driven architecture, multi-threaded asynchronous loading, and real-time network synchronization.

### 🚀 Core Technical Highlights
* **Data-Driven Level Design (`level0.json` to `level5.json`):** Integrated `nlohmann/json` to decouple game logic from level data. Levels can be modified dynamically via the in-game level editor (using `E` to toggle, `L-Click/R-Click` to draw/erase, and `S` to serialize and save), without recompiling the C++ source.
* **Multi-threading & Asynchronous Rendering:** Utilized `std::thread`, `std::future`, and `std::packaged_task` to offload the heavy Perlin noise background generation to a background thread. Combined with a "Dual Visual Feedback" mechanism (magenta brick color switch + UI loading text) to prevent main-thread freezing and provide excellent UX.
* **High-Performance Object Pool (`ParticlePool`):** Engineered a custom memory management system to handle hundreds of concurrent particle explosion effects, completely eliminating the GC overhead of frequent `new/delete` or `std::vector::push_back` operations.
* **Real-Time Multiplayer Synchronization (ENet):** Implemented a Host/Client network architecture. The host broadcasts game snapshots (ball coordinates, brick active states), while the client sends input predictions, ensuring consistent states across the network.
* **Persistent Progress Saving:** Built a robust local Save/Load system using C++ standard file streams (`std::ifstream`/`std::ofstream`) to record scores, lives, and level progress seamlessly.

### 🎬 Immersive Audio-Visual Feedback
* **Dynamic Camera Shake & Particles:** Triggered precisely upon boundary collisions or life loss.
* **Multi-Track Audio Engine:** Integrated specialized SFX (`brick.wav`, `paddle.wav`, `catch-stars.wav`, `stars-drop.wav`) and a continuous background track (`background.mp3`) for a complete sensory experience.

---

## 📂 Academic Progression & Mini-Projects
Beyond the main project, this repository logs my progression in C++ OOP and algorithmic design:
* 🐍 **`snake.cpp` & `slime.cpp`**: Classic algorithmic game loops focusing on entity state management and grid-based movement logic.
* 🧬 **`task10.cpp` & `task11.cpp`**: Advanced OOP validations. Showcases deep understanding of Virtual Functions, Polymorphism, Inheritance, and Function Overloading. Features early experiments with procedural generation (e.g., Perlin noise rendering).

---

## 👥 Team & Collaboration
This repository reflects collaborative academic work, divided into core responsibilities:
* **UI/UX & Networking:** Scene loop management, JSON persistence, and multiplayer ENet integration.
* **Architecture & Performance:** OOP architecture, custom Singleton `TextureCache`, `ParticlePool` optimization, and async thread management.

*(Development efficiency significantly accelerated through AI-assisted Code Review and structural Prompt Engineering).*
