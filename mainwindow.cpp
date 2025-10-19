#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPushButton>
#include <QComboBox>
#include <utility>
#include <QCoreApplication>
#include <vector>

// The data callback is now simple: it just reads, processes, and outputs audio.
void MainWindow::data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    MainWindow* mainWindow = static_cast<MainWindow*>(pDevice->pUserData);
    if (mainWindow == nullptr) return;

    // Use a temporary buffer that is guaranteed to be large enough
    std::vector<float> mixBuffer(frameCount * pDevice->playback.channels, 0.0f);

    { // Scoped lock to protect shared data
        QMutexLocker locker(&mainWindow->audioStateMutex);

        // 1. Process Background Audio
        if (mainWindow->isBackgroundLoaded && mainWindow->isBackgroundPlaying) {
            ma_uint64 framesRead = 0;
            ma_decoder_read_pcm_frames(&mainWindow->backgroundDecoder, mixBuffer.data(), frameCount, &framesRead);
            if (framesRead < frameCount) {
                // We've reached the end of the file, but don't stop it, let it loop in the play function
            }
        }

        // 2. Process Voice Audio and Mix it in
        if (mainWindow->isVoiceLoaded && mainWindow->isVoicePlaying) {
            std::vector<float> voiceBuffer(frameCount * pDevice->playback.channels, 0.0f);
            ma_uint64 framesRead = 0;
            ma_decoder_read_pcm_frames(&mainWindow->voiceDecoder, voiceBuffer.data(), frameCount, &framesRead);

            // Mix voice into the main buffer, applying volume
            for (ma_uint32 i = 0; i < mixBuffer.size(); ++i) {
                mixBuffer[i] += voiceBuffer[i] * mainWindow->voiceVolume;
            }

            if (framesRead < frameCount) {
                mainWindow->isVoicePlaying = false; // Stop at the end
            }
        }
    } // Mutex is released automatically here

    // 3. CRITICAL: Copy the final mixed audio to the output for the sound card
    memcpy(pOutput, mixBuffer.data(), mixBuffer.size() * sizeof(float));
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    // Initialize all flags
    audioInitialized = false; isVoiceLoaded = false; isVoicePlaying = false;
    isBackgroundLoaded = false; isBackgroundPlaying = false;

    ui->setupUi(this);
    initializeAudio();
    populateVoiceComboBox();
    populateBackgroundComboBox();

    // Connect all UI signals to their slots
    connect(ui->ambienceComboBox, &QComboBox::currentIndexChanged, this, &MainWindow::onAmbienceChanged);
    connect(ui->voiceComboBox, &QComboBox::currentIndexChanged, this, &MainWindow::onVoiceSelectionChanged);
    connect(ui->playVoiceButton, &QPushButton::clicked, this, &MainWindow::onPlayVoiceClicked);
    connect(ui->playBackgroundButton, &QPushButton::clicked, this, &MainWindow::onPlayBackgroundClicked);
    connect(ui->slider_voice_master, &QSlider::valueChanged, this, &MainWindow::onVoiceMasterVolumeChanged);
    connect(ui->slider_voice_low, &QSlider::valueChanged, this, &MainWindow::onVoiceEqLowChanged);
    connect(ui->slider_voice_mid, &QSlider::valueChanged, this, &MainWindow::onVoiceEqMidChanged);
    connect(ui->slider_voice_high, &QSlider::valueChanged, this, &MainWindow::onVoiceEqHighChanged);

    // Set initial slider positions
    ui->slider_voice_master->setValue(100);
    ui->slider_voice_low->setValue(100);
    ui->slider_voice_mid->setValue(100);
    ui->slider_voice_high->setValue(100);
}

MainWindow::~MainWindow()
{
    shutdownAudio();
    delete ui;
}

void MainWindow::initializeAudio()
{
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = ma_format_f32;
    deviceConfig.playback.channels = 2;
    deviceConfig.sampleRate        = 48000;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = this;

    ma_result result_init = ma_device_init(NULL, &deviceConfig, &device);
    if (result_init != MA_SUCCESS) {
        // NEW: Added debug message for initialization failure
        qDebug() << "ERROR: Failed to initialize audio device. Code:" << result_init;
        return;
    }

    ma_loshelf2_config lowConfig = ma_loshelf2_config_init(device.playback.format, device.playback.channels, device.sampleRate, 250, 1.0, 0);
    ma_peak2_config midConfig = ma_peak2_config_init(device.playback.format, device.playback.channels, device.sampleRate, 1200, 0.707, 0);
    ma_hishelf2_config highConfig = ma_hishelf2_config_init(device.playback.format, device.playback.channels, device.sampleRate, 5000, 1.0, 0);

    ma_loshelf2_init(&lowConfig, NULL, &eqFilterLow);
    ma_peak2_init(&midConfig, NULL, &eqFilterMid);
    ma_hishelf2_init(&highConfig, NULL, &eqFilterHigh);

    ma_result result_start = ma_device_start(&device);
    if (result_start != MA_SUCCESS) {
        // NEW: Added debug message for start failure and cleanup
        qDebug() << "ERROR: Failed to start audio device. Code:" << result_start;
        ma_device_uninit(&device); // Clean up what was initialized
        return;
    }

    // NEW: Added a success message so we know it worked
    qDebug() << "Audio device started successfully!";
    audioInitialized = true;
    voiceVolume = 1.0f;
}

void MainWindow::shutdownAudio()
{
    if (audioInitialized)
    {
        ma_device_uninit(&device);
        audioInitialized = false;
    }
}

// These two functions now simply unload the decoders and reset the UI
void MainWindow::onAmbienceChanged(int index)
{
    qDebug() << "Ambience selection changed, preparing for new sound.";
    QMutexLocker locker(&audioStateMutex);

    if (isBackgroundLoaded) {
        isBackgroundPlaying = false;
        ma_decoder_uninit(&backgroundDecoder); // Take the old engine out
        isBackgroundLoaded = false;
        ui->playBackgroundButton->setText("Play Background");
        ui->backgroundStatusLabel->setText("Status: Stopped");
    }
}

void MainWindow::onVoiceSelectionChanged(int index)
{
    qDebug() << "Voice selection changed, preparing for new sound.";
    QMutexLocker locker(&audioStateMutex);

    if (isVoiceLoaded) {
        isVoicePlaying = false;
        ma_decoder_uninit(&voiceDecoder); // Take the old engine out
        isVoiceLoaded = false;
        ui->playVoiceButton->setText("Play Voice");
        ui->voiceStatusLabel->setText("Status: Stopped");
    }
}

// The onPlay...Clicked functions are now the single source of truth for playback.
void MainWindow::onPlayBackgroundClicked()
{
    qDebug() << "onPlayBackgroundClicked() was triggered!";
    if (!audioInitialized) return;
    QMutexLocker locker(&audioStateMutex);

    if (!isBackgroundLoaded) {
        QString selected = ui->ambienceComboBox->currentText();
        if (selected.isEmpty()) return;
        QString path = backgroundPathMap.value(selected);

        qDebug() << "Attempting to load background from filesystem path:" << path; // Corrected message

        QFile backgroundFile(path);
        if (!backgroundFile.open(QIODevice::ReadOnly)) {
            // NEW: This gives us the REAL error
            qDebug() << "FILE ERROR: Could not open background file. Reason:" << backgroundFile.errorString();
            ui->backgroundStatusLabel->setText("Status: File Error");
            return;
        }

        backgroundData = backgroundFile.readAll();
        backgroundFile.close(); // Good practice to close the file

        if (backgroundData.isEmpty()) {
            qDebug() << "FILE ERROR: File is empty or could not be read.";
            return;
        }

        qDebug() << "Background file loaded successfully," << backgroundData.size() << "bytes.";

        ma_decoder_config cfg = ma_decoder_config_init_default();
        if (ma_decoder_init_memory(backgroundData.constData(), backgroundData.size(), &cfg, &backgroundDecoder) != MA_SUCCESS) {
            ui->backgroundStatusLabel->setText("Status: Decode Error");
            return;
        }
        isBackgroundLoaded = true;
        currentBackgroundName = selected;
    }

    isBackgroundPlaying = !isBackgroundPlaying;
    if (isBackgroundPlaying) {
        ma_decoder_seek_to_pcm_frame(&backgroundDecoder, 0);
        ui->backgroundStatusLabel->setText("Status: Playing");
        ui->playBackgroundButton->setText("Stop Background");
    } else {
        ui->backgroundStatusLabel->setText("Status: Stopped");
        ui->playBackgroundButton->setText("Play Background");
    }
}

void MainWindow::onPlayVoiceClicked()
{
    qDebug() << "onPlayVoiceClicked() was triggered!";
    if (!audioInitialized) return;
    QMutexLocker locker(&audioStateMutex);

    if (!isVoiceLoaded) {
        QString selected = ui->voiceComboBox->currentText();
        if (selected.isEmpty()) return;
        QString path = voicePathMap.value(selected);

        qDebug() << "Attempting to load voice from filesystem path:" << path; // Corrected message

        QFile voiceFile(path);
        if (!voiceFile.open(QIODevice::ReadOnly)) {
            // NEW: This gives us the REAL error
            qDebug() << "FILE ERROR: Could not open voice file. Reason:" << voiceFile.errorString();
            ui->voiceStatusLabel->setText("Status: File Error");
            return;
        }

        voiceData = voiceFile.readAll();
        voiceFile.close(); // Good practice to close the file

        if (voiceData.isEmpty()) {
            qDebug() << "FILE ERROR: File is empty or could not be read.";
            return;
        }

        qDebug() << "Voice file loaded successfully," << voiceData.size() << "bytes.";

        ma_decoder_config cfg = ma_decoder_config_init_default();
        if (ma_decoder_init_memory(voiceData.constData(), voiceData.size(), &cfg, &voiceDecoder) != MA_SUCCESS) {
            ui->voiceStatusLabel->setText("Status: Decode Error");
            return;
        }
        isVoiceLoaded = true;
        currentVoiceName = selected;
    }

    isVoicePlaying = !isVoicePlaying;
    if (isVoicePlaying) {
        ma_decoder_seek_to_pcm_frame(&voiceDecoder, 0);
        ui->voiceStatusLabel->setText("Status: Playing");
        ui->playVoiceButton->setText("Stop Voice");
    } else {
        ui->voiceStatusLabel->setText("Status: Stopped");
        ui->playVoiceButton->setText("Play Voice");
    }
}

void MainWindow::onVoiceMasterVolumeChanged(int value)
{
    if (!audioInitialized) return;
    QMutexLocker locker(&audioStateMutex);
    voiceVolume = value / 100.0f;
}

void MainWindow::onVoiceEqLowChanged(int value)
{
    if (!audioInitialized) return;
    double gainDB = (value / 100.0) * 24.0 - 24.0;

    QMutexLocker locker(&audioStateMutex);
    ma_loshelf2_config config = ma_loshelf2_config_init(device.playback.format, device.playback.channels, device.sampleRate, 250, 1.0, gainDB);
    ma_loshelf2_reinit(&config, &eqFilterLow);
}

void MainWindow::onVoiceEqMidChanged(int value)
{
    if (!audioInitialized) return;
    double gainDB = (value / 100.0) * 24.0 - 24.0;

    QMutexLocker locker(&audioStateMutex);
    ma_peak2_config config = ma_peak2_config_init(device.playback.format, device.playback.channels, device.sampleRate, 1200, 0.707, gainDB);
    ma_peak2_reinit(&config, &eqFilterMid);
}

void MainWindow::onVoiceEqHighChanged(int value)
{
    if (!audioInitialized) return;
    double gainDB = (value / 100.0) * 24.0 - 24.0;

    QMutexLocker locker(&audioStateMutex);
    ma_hishelf2_config config = ma_hishelf2_config_init(device.playback.format, device.playback.channels, device.sampleRate, 5000, 1.0, gainDB);
    ma_hishelf2_reinit(&config, &eqFilterHigh);
}

void MainWindow::populateBackgroundComboBox()
{
    backgroundPathMap.clear();
    ui->ambienceComboBox->clear();

    // Get the path to the directory where the executable is running
    QString exeDirPath = QCoreApplication::applicationDirPath();
    QDir backgroundDir(exeDirPath + "/sounds/backgrounds");

    qDebug() << "Scanning for background files in:" << backgroundDir.path();

    if (!backgroundDir.exists()) {
        qDebug() << "ERROR: The directory" << backgroundDir.path() << "does not exist!";
        return;
    }

    QStringList fileFilters;
    fileFilters << "*.mp3" << "*.flac" << "*.wav";
    QStringList backgroundFiles = backgroundDir.entryList(fileFilters, QDir::Files);

    for (const QString& filename : std::as_const(backgroundFiles))
    {
        QString prettyName = QFileInfo(filename).baseName();
        prettyName.replace('_', ' ');
        QString fullPath = backgroundDir.filePath(filename); // This is now a real filesystem path
        backgroundPathMap[prettyName] = fullPath;
    }

    ui->ambienceComboBox->addItems(backgroundPathMap.keys());
    qDebug() << "Found and added" << backgroundPathMap.size() << "background files.";
}

void MainWindow::populateVoiceComboBox()
{
    voicePathMap.clear();
    ui->voiceComboBox->clear();

    // Get the path to the directory where the executable is running
    QString exeDirPath = QCoreApplication::applicationDirPath();
    QDir voiceDir(exeDirPath + "/sounds/voices");

    qDebug() << "Scanning for voice files in:" << voiceDir.path();

    if (!voiceDir.exists()) {
        qDebug() << "ERROR: The directory" << voiceDir.path() << "does not exist!";
        return;
    }

    QStringList fileFilters;
    fileFilters << "*.mp3" << "*.flac" << "*.wav";
    QStringList voiceFiles = voiceDir.entryList(fileFilters, QDir::Files);

    for (const QString& filename : std::as_const(voiceFiles))
    {
        QString prettyName = QFileInfo(filename).baseName();
        prettyName.replace('_', ' ');
        QString fullPath = voiceDir.filePath(filename); // This is now a real filesystem path
        voicePathMap[prettyName] = fullPath;
    }

    ui->voiceComboBox->addItems(voicePathMap.keys());
    qDebug() << "Found and added" << voicePathMap.size() << "voice files.";
}
