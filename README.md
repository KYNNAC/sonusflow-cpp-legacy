# Sonus Flow

*A simple, powerful hearing trainer designed to help you focus.*

![Sonus Flow in Action](path/to/your/sonusflow.gif)  
*(You'll need to replace this path with the GIF you recorded and placed in your repository)*

---

## About The Project

In a world full of noise, our brains have a natural ability to filter out distractions and focus on what's important, like a single conversation in a bustling cafe. This project aims to improve focus on important sounds in complex auditory environments by allowing users to manipulate and isolate audio layers from real-world ambiences.

**Sonus Flow** is a non-commercial, therapeutic tool designed to help users retrain this essential hearing skill. It was born from a desire to create an accessible tool, especially for those with unique hearing challenges, to provide both a training exercise and a space for auditory relaxation.

---

# A Focus on Hearing Health

Sonus Flow is designed with user safety as a top priority. Hearing health is delicate, and this tool is built to be protective, not harmful. To achieve this, the application follows two key principles:

### 1. Subtractive EQ by Design
Unlike traditional equalizers that can boost frequencies to potentially dangerous levels, the EQs in Sonus Flow are **subtractive-only**. This means the controls can only be used to "carve away" or reduce the volume of specific frequency bands. This is a common technique in professional audio mixing that promotes clarity and prevents excessive loudness, ensuring a safe and comfortable listening experience.

### 2. Safe Mode (Planned Feature)
A planned "Safe Mode" will provide an optional master volume limiter. When enabled, it will ensure the total output of the application never exceeds a comfortable, standardized listening level (e.g., -14 LUFS), giving users peace of mind during longer training sessions.

---

## Features

* **Dynamic Soundscapes:** Choose from a variety of background scenarios (e.g., "Quiet Cafe," "Busy Sidewalk").
* **Focus Voice Training:** Select a distinct vocal track (e.g., "Poem Recital") to focus on amidst the background noise.
* **Independent Audio Control:** Separate playback controls for the background and the focus voice.
* **Granular Sound Control:**
    * Adjust the volume of individual layers within each background scene.
    * Control the Low, Mid, and High frequencies of the focus voice with a 3-band EQ.
* **Extensible:** The application automatically discovers and loads new voice files from its resource folder.

---

## Built With

* C++17
* Qt 6 (for GUI development)
* CMake (for build system management)
* MiniAudio (for audio playback)

---

### How the Audio Engine Works

The audio processing in Sonus Flow is powered by the **MiniAudio** library. The 3-band equalizer for the focus voice is built using a chain of **biquad filters**. Each filter's shape and response are defined by several key parameters:

* **Filter Type:** The EQ uses *low-shelf*, *peaking*, and *high-shelf* filters to provide broad tonal control over the bass, mids, and treble, respectively.
* **Frequency:** The center or corner frequency for each band is set to a musically relevant point to effectively shape the vocal range.
* **Q-Factor:** A moderate Q-factor is used to create gentle, natural-sounding curves rather than sharp, artificial-sounding cuts.
* **Gain:** The gain for each band is controlled by the UI sliders and is limited to a subtractive (cut-only) range to ensure user safety.

---

## Getting Started (Building from Source)

To get a local copy up and running, follow these steps.

### Prerequisites

This guide assumes a Linux environment. You will need a C++ compiler, CMake, and the Qt 6 development libraries.

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
    git clone [https://github.com/your_username/SonusFlow.git](https://github.com/your_username/SonusFlow.git)
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
    ```sh
    ./SonusFlow
    ```
