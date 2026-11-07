# ThreadTales

**ThreadTales** is a co-op Action-Adventure game developed on **Unreal Engine 5.7** featuring Souls-like combat (I-Frames, precision timings, melee combos, and tactical positioning). Gameplay combines two-player synergy in the spirit of "It Takes Two" with deep combat mechanics and seamless vertical traversal.

---

## 🎬 Gameplay Showreel (UE5 C++ / GAS / Co-op)

> 🔗 **Watch in Full HD (60 FPS):**  
> ▶️ **[Watch on YouTube](https://www.youtube.com/watch?v=5FWR5H_m0Uo)** | 📁 **[Watch on Google Drive (Direct Stream)](https://drive.google.com/file/d/1PHn-aUbboQbwkMk6IsxL7PhkfH22Pdaz/view?usp=sharing)**

https://github.com/user-attachments/assets/201c37a5-00c8-4c6c-a6fc-326cbbe4da8e

---

## 🎮 Key Features

- **2-Player Co-op (PvE):** Online network play and local split-screen. Two unique characters (`BP_HeroMale` and `BP_HeroFemale`) built upon a shared, symmetrical C++ codebase.
- **Dynamic Vertical Traversal:** 
  - Spatial 3D ledge and surface scanner (`FThreadTalesLedgeDetector`).
  - Kinematic 8-directional jumps, braced wall hangs, and freehang beam suspension.
  - Seamless inner and outer corner navigation with curved trajectory interpolation.
- **Combat via Gameplay Ability System (GAS):**
  - Attributes (Health, Stamina, Poise) and damage pipeline driven by GAS.
  - Combo chains with configurable input windows (`AnimNotifyState_ComboWindow`).
  - Tactical dodges with invulnerability frames (I-Frames), air evades, and double jumps.
  - Melee and ranged weapons (katana, slingshot) with dynamic hitboxes.
- **Animation System:** 
  - GASPALS integration and Motion Matching locomotion.
  - UEFN skeleton-based animations with synchronized kinematics and Root Motion.

---

## 🛠 Project Architecture

The codebase is organized into modular C++ directories:

```
Source/ThreadTales/
├── Abilities/      # GAS classes: AbilitySystemComponent, AttributeSet, attacks, evades
├── Animation/      # Montage hitboxes, combo notifies, Motion Matching library
├── Characters/     # Base character, climbing state machine, kinematics, ledge configs
├── Combat/         # Weapons (melee/ranged), projectiles, equipment component
├── Core/           # GameMode, PlayerController, Gameplay Tags
└── Input/          # Enhanced Input Component and input mapping contexts
```

---

## 🚀 Getting Started

### Requirements
- **Unreal Engine 5.7**
- **IDE:** JetBrains Rider (recommended) or Visual Studio 2022
- **Platform:** Windows 10/11 x64

### Build & Run
1. Clone the repository:
   ```bash
   git clone <repository-url>
   ```
2. Generate project files:
   - Right-click `ThreadTales.uproject` and select **Generate Visual Studio project files**.
3. Open the project in JetBrains Rider or Visual Studio.
4. Select the **Development Editor** configuration and run **Build**.
5. Launch the project via the Unreal Engine 5.7 Editor.

---

## 🎯 Default Controls

| Action | Input (Keyboard + Mouse) |
|---|---|
| Move | `W`, `A`, `S`, `D` |
| Look | Mouse |
| Jump / Mantle Up | `Space` |
| Drop from Ledge / Crouch | `C` |
| Light Attack | `Left Mouse Button` |
| Aim / Ranged Attack | `Right Mouse Button` |
| Evade (I-Frame) | `Left Shift` / `Alt` |
| Interact | `E` |

---

## 📄 License

All rights reserved. Developed for ThreadTales.
