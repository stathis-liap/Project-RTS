# Project-RTS
**❄️ OpenGL Real-Time Strategy Project**

A high-performance C++/OpenGL RTS Engine featuring procedural terrain, skeletal animations, and dynamic environment deformation.

<img width="2864" height="1746" alt="terrain-panoramic" src="https://github.com/user-attachments/assets/4b3d4aed-a480-4b19-845f-3c123a20a437" />


**🚀 Overview**

This project is a custom-built Real-Time Strategy (RTS) engine developed for the Graphics and Virtual Reality course (February 2026). It demonstrates advanced computer graphics techniques, including procedural world generation, unit AI, and optimized rendering for massive unit counts.

The game is set in a winter landscape where players manage resources, construct buildings with gradual visual progression, and command an army of animated skeletal units.

**🛠 Core Technical Features**
1. Procedural Terrain & Environment

    Heightmap Generation: Uses a deterministic Hash function combined with Noise (Hermite Curve/Smoothstep) and multiple octaves to create realistic hills and valleys.

    Diagonal Symmetry: Ensures perfectly fair maps for competitive play by mirroring the terrain across the diagonal axis.

    Dynamic Deformation: * Flattening: Terrain automatically levels out when buildings are placed.

    Craters: Explosions create permanent, bell-curved craters in the geometry.

    Snow Trails: A dynamic SnowTrailMap uses vertex displacement to create physical ruts in the snow where units walk, with a "healing" shader that gradually refills trails over time.

2. Advanced Graphics Pipeline

    Skeletal Animation (Skinning): Full support for .fbx/.obj animated models via Assimp. Every vertex is influenced by up to 4 bones with linear interpolation (Lerp) between keyframes.

    Geometry Instancing: Allows rendering hundreds of workers, warriors, and mages with minimal Draw Calls. Units are batched by state (IDLE, WALK, ATTACK) to share bone matrices efficiently.

    Optimized Rendering: * Frustum Culling: Uses 6 clipping planes to discard objects outside the camera's view.

   Shadow Mapping: Directional shadows with PCF (Percentage Closer Filtering) for soft edges and slope-scaled bias to prevent shadow acne.

3. Navigation & AI

    _A* Pathfinding_: Implements the A-Star algorithm with a priority queue for optimal pathing.

    Navigation Grid: A spatial memory system that handles obstacle avoidance for buildings, trees, and rocks.

    Smart Sliding Logic: Units "slide" along walls and obstacles rather than getting stuck when their path is partially blocked.

    Finite State Machine (FSM): Units autonomously transition between IDLE, MOVING, GATHERING, and ATTACKING states.

4. Gameplay Systems

    Economy: Resource tracking for Wood and Rock.

    Construction: Buildings rise gradually from the ground using a custom Clipping Shader based on construction progress.

    Combat: Melee units engage in close-quarters combat, while Mages utilize a Particle System for energy beams and impact effects.

**🎮 Controls**

Key/Action	Function

**W, A, S, D**	Move Camera (XZ Plane)

Mouse Scroll	Zoom to Cursor (Raycasting)

Edge Panning	Move camera by touching screen edges

Left Click & Drag	Selection Box (Drag-select units)

Right Click	Move Units / Target Resource / Attack Building

G (Warrior)	Trigger Explosion & Terrain Deformation

C	Toggle 3rd Person POV (Follow Unit)

F1	Toggle Depth Map View (Debug)

**🏗 Tech Stack**

    Language: C++

    Graphics API: OpenGL 3.3+ (Core Profile)

    Libraries: * GLFW / GLEW: Window management and extension loading.

        GLM: Mathematical operations (Matrices, Vectors).

        Assimp: 3D Model and Animation loading.

        SOIL / Texture Loaders: Image processing.

**📸 Gallery**

**Dynamic Construction**

<img width="792" height="522" alt="gradualbuilding" src="https://github.com/user-attachments/assets/5fa1de0e-6341-4f23-8c6e-e87abffc633a" />




**Pathfinding & Formations**

<img width="1144" height="1082" alt="pathduring" src="https://github.com/user-attachments/assets/bc77720b-c43c-4eb0-bf25-a8f6f8858347" />
