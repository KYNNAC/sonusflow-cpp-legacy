# Sonus Flow

*An Auditory Training Tool for Auditory Processing Disorder (APD)*

---

## The Challenge: Understanding APD

Auditory Processing Disorder (APD) is a neurological condition that affects how the brain processes sound. Individuals with APD can hear perfectly well, but their brains struggle to interpret and make sense of what they hear, especially in complex auditory environments. This can make everyday situations, like holding a conversation in a bustling cafe or focusing on a lecture, incredibly challenging.

This difficulty in filtering out background noise to focus on a primary sound source is often related to the "cocktail party effect," a cognitive skill that many of us take for granted. For those with APD, the brain doesn't easily separate the "signal" from the "noise."

---

## The Solution: Sonus Flow

**Sonus Flow** is a non-commercial, therapeutic tool designed to help individuals with APD practice and improve their auditory discrimination skills in a controlled, safe environment.

By simulating real-world scenarios, the application provides targeted exercises that aim to leverage the principles of neuroplasticity—the brain's ability to reorganize itself by forming new neural connections. The goal is to help the user's brain become more efficient at identifying and focusing on a target voice amidst a backdrop of ambient sound, with the hope that this practice translates to improved focus in daily life.

---

## Core Features

* **Simulated Real-World Environments:** Choose from a library of background soundscapes to replicate challenging listening situations.
* **Targeted Voice Training:** Isolate and focus on a specific vocal track, training your brain to "lock on" to a single speaker.
* **Independent Playback Control:** Manage the voice and background channels separately to customize the difficulty of your training session.
* **Focus-Targeting EQ:** Utilize a professional-grade 3-band subtractive equalizer to gently "carve out" space for the voice, making it more intelligible without artificially boosting volume.
* **Extensible by Design:** The application automatically discovers and loads new voice and background files placed in the `sounds` directory, allowing for an ever-growing library of training scenarios.

---

## Safety by Design

Hearing health is paramount. Sonus Flow is built from the ground up to be a protective tool, not a harmful one.

### 1. Subtractive-Only EQ
Unlike traditional equalizers that can boost frequencies to potentially dangerous levels, the EQs in Sonus Flow are **subtractive-only**. The controls can only reduce the volume of specific frequency bands. This is a common technique in professional audio that promotes clarity and prevents excessive loudness, ensuring a safe and comfortable listening experience.

### 2. Safe Mode (Planned Feature)
A planned "Safe Mode" will provide an optional master volume limiter. When enabled, it will ensure the total output of the application never exceeds a comfortable, standardized listening level (e.g., -14 LUFS), providing peace of mind during longer training sessions.

---

## Technical Implementation

* **Languages:** C++17
* **Framework:** Qt 6 (for the cross-platform GUI)
* **Build System:** CMake
* **Audio Engine:** The core audio playback and processing is powered by the lightweight **MiniAudio** library. The 3-band equalizer is a chain of biquad filters (*low-shelf, peaking, high-shelf*) designed to provide broad, musical control over the vocal frequency range.

---

## Getting Started (Building from Source)

This guide assumes a Linux environment. You will need a C++ compiler, CMake, and the Qt 6 development libraries.

### Prerequisites

* **On Arch Linux:**
    ```sh
    sudo pacman -S base-devel cmake qt6-base
    ```
* **On Ubuntu/Debian:**
    ```sh
    sudo apt-get install build-essential cmake qt6-base-dev
    ```

### Build Steps

1.  **Clone the repository:**
    ```sh
    git clone [https://github.com/KYNNAC/SonusFlow.git](https://github.com/KYNNAC/SonusFlow.git)
    cd SonusFlow
    ```
2.  **Create a build directory:**
    ```sh
    mkdir build && cd build
    ```
3.  **Run CMake to configure the project:**
    ```sh
    cmake ..
    ```
4.  **Compile the project:**
    ```sh
    make
    ```
5.  **Run the application:**
    The application requires its `sounds` asset folder to be in the same directory as the executable. The CMake configuration handles this automatically.
    ```sh
    ./SonusFlow
    ```
