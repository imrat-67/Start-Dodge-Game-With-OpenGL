# Star Dodge - UFO Edition

A simple 3D OpenGL game made with C++ and GLUT/freeglut. The player controls a UFO in a moon-like space arena, collects glowing rewards, and dodges falling meteors. The score increases as rewards are collected, and the difficulty rises with each level.

## Features

- 3D UFO movement in a space environment
- Falling meteors as obstacles
- Glowing collectible rewards
- Score and level system
- Pause, restart, and game over states
- Mouse-controlled camera rotation
- Star field, nebula background, moon surface, craters, particles, and visual effects

## Controls

| Key / Input | Action |
| --- | --- |
| `W` / `S` | Move forward / backward |
| `A` / `D` | Move left / right |
| `Q` / `E` | Fly up / down |
| `Space` | Fly up |
| Arrow keys | Move the UFO |
| Mouse drag | Rotate camera |
| `P` | Pause / resume |
| `Enter` | Start or restart |
| `Esc` | Exit |

## How to Run

Install OpenGL and freeglut first.

On Ubuntu/Debian:

```bash
sudo apt install freeglut3-dev
```

Compile the game:

```bash
g++ learning/Star_Dodge.cpp -o Star_Dodge -lGL -lGLU -lglut
```

Run it:

```bash
./Star_Dodge
```

## Goal

Collect as many glowing orbs as possible while avoiding the meteors. Every few points, the level increases and the meteors become harder to avoid.

## Project Info

This project was created as part of a Computer Graphics practice/lab project using C++ and OpenGL.


<img width="1838" height="1001" alt="Screenshot from 2026-05-11 01-39-44" src="https://github.com/user-attachments/assets/677a5c58-966b-48c5-b587-9e7ded480267" />
<img width="1838" height="1001" alt="Screenshot from 2026-05-11 01-39-15" src="https://github.com/user-attachments/assets/f12b77f7-2556-4fe3-ba85-0174b98ba7e5" />
<img width="1838" height="1001" alt="Screenshot from 2026-05-11 01-38-51" src="https://github.com/user-attachments/assets/1ec0ce1e-7402-4bd4-acd2-2283d95cabb7" />
<img width="1838" height="1001" alt="Screenshot from 2026-05-11 01-38-42" src="https://github.com/user-attachments/assets/b8e5f4cf-33f6-40f2-a4fd-f37a9990f3b9" />
<img width="1838" height="1001" alt="Screenshot from 2026-05-11 01-38-25" src="https://github.com/user-attachments/assets/c652954a-4bae-41a7-bd50-35a4c5c4d61a" />
