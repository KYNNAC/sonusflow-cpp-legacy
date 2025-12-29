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

// Data callback - Reads, processes and outputs audio.
void MainWindow::data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    MainWindow* mainWindow = static_cast<MainWindow*>(pDevice->pUserData);
    if (mainWindow == nullptr) return;

    std::vector<float> backgroundBuffer(frameCount * pDevice->playback.channels, 0.0f);
    std::vector<float> voiceBuffer(frameCount * pDevice->playback.channels, 0.0f);

    // --- PING-PONG BUFFER ---
    std::vector<float> processedBuffer(frameCount * pDevice->playback.channels, 0.0f);
    std::vector<float> finalMixBuffer(frameCount * pDevice->playback.channels, 0.0f);

    { // Scoped lock
        QMutexLocker locker(&mainWindow->audioStateMutex);

        // --- STEP 1: Process Background ---
        if (mainWindow->isBackgroundLoaded && mainWindow->isBackgroundPlaying) {
            ma_uint64 framesRead = 0;
            ma_decoder_read_pcm_frames(&mainWindow->backgroundDecoder, backgroundBuffer.data(), frameCount, &framesRead);
            if (framesRead < frameCount) {
                ma_decoder_seek_to_pcm_frame(&mainWindow->backgroundDecoder, 0);
            }
        }

        // --- STEP 2: Process Voice with the Ping-Pong Algorithm ---
        if (mainWindow->isVoiceLoaded && mainWindow->isVoicePlaying) {
            ma_uint64 framesRead = 0;
            ma_decoder_read_pcm_frames(&mainWindow->voiceDecoder, voiceBuffer.data(), frameCount, &framesRead);

            if (framesRead > 0) {
                // 1. Low Shelf: Reads from original `voiceBuffer`, writes to `processedBuffer`
                ma_loshelf2_process_pcm_frames(&mainWindow->eqFilterLow, processedBuffer.data(), voiceBuffer.data(), framesRead);

                // 2. Mid Peak: Reads from `processedBuffer`, writes back to `voiceBuffer`
                ma_peak2_process_pcm_frames(&mainWindow->eqFilterMid, voiceBuffer.data(), processedBuffer.data(), framesRead);

                // 3. High Shelf: Reads from `voiceBuffer`, writes back to `processedBuffer`
                ma_hishelf2_process_pcm_frames(&mainWindow->eqFilterHigh, processedBuffer.data(), voiceBuffer.data(), framesRead);          
            }

            if (framesRead < frameCount) {
                QMetaObject::invokeMethod(mainWindow, "stopVoicePlayback", Qt::QueuedConnection);
            }
        }
    } // Mutex released

    // --- STEP 3: Mix the final buffers ---
    for (ma_uint32 i = 0; i < frameCount * pDevice->playback.channels; ++i) {

        // Use the final processedBuffer for the voice
        float voiceSample = processedBuffer[i] * mainWindow->voiceVolume;
        float backgroundSample = backgroundBuffer[i];

        finalMixBuffer[i] = voiceSample + backgroundSample;
    }

    // --- STEP 4: Output the final mix ---
    memcpy(pOutput, finalMixBuffer.data(), finalMixBuffer.size() * sizeof(float));
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
    connect(ui->voiceComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onVoiceSelectionChanged);
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
        qDebug() << "ERROR: Failed to initialize audio device. Code:" << result_init;
        return;
    }

    ma_result result_start = ma_device_start(&device);
    if (result_start != MA_SUCCESS) {
        qDebug() << "ERROR: Failed to start audio device. Code:" << result_start;
        ma_device_uninit(&device);
        return;
    }

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

// Unload the decoders and reset the UI
void MainWindow::onAmbienceChanged(int index)
{
    qDebug() << "Ambience selection changed, preparing for new sound.";
    QMutexLocker locker(&audioStateMutex);

    if (isBackgroundLoaded) {
        isBackgroundPlaying = false;
        ma_decoder_uninit(&backgroundDecoder);
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
        ma_decoder_uninit(&voiceDecoder);
        isVoiceLoaded = false;
        ui->playVoiceButton->setText("Play Voice");
        ui->voiceStatusLabel->setText("Status: Stopped");

        ui->slider_voice_low->setValue(100);
        ui->slider_voice_mid->setValue(100);
        ui->slider_voice_high->setValue(100);
    }
}

// Playback
void MainWindow::onPlayBackgroundClicked()
{
    qDebug() << "onPlayBackgroundClicked() was triggered!";
    if (!audioInitialized) return;
    QMutexLocker locker(&audioStateMutex);

    if (!isBackgroundLoaded) {
        QString selected = ui->ambienceComboBox->currentText();
        if (selected.isEmpty()) return;
        QString path = backgroundPathMap.value(selected);

        qDebug() << "Attempting to load background from filesystem path:" << path;

        QFile backgroundFile(path);
        if (!backgroundFile.open(QIODevice::ReadOnly)) {

            qDebug() << "FILE ERROR: Could not open background file. Reason:" << backgroundFile.errorString();
            ui->backgroundStatusLabel->setText("Status: File Error");
            return;
        }

        backgroundData = backgroundFile.readAll();
        backgroundFile.close();

        if (backgroundData.isEmpty()) {

            qDebug() << "FILE ERROR: File is empty or could not be read.";
            return;
        }

        qDebug() << "Background file loaded successfully," << backgroundData.size() << "bytes.";

        ma_decoder_config cfg = ma_decoder_config_init(device.playback.format, device.playback.channels, device.sampleRate);
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

    // --- 1: Load file if needed ---

    if (!isVoiceLoaded) {
        QString selected = ui->voiceComboBox->currentText();
        if (selected.isEmpty()) return;
        QString path = voicePathMap.value(selected);

        qDebug() << "Attempting to load voice from filesystem path:" << path;

        QFile voiceFile(path);
        if (!voiceFile.open(QIODevice::ReadOnly)) {
            qDebug() << "FILE ERROR: Could not open voice file. Reason:" << voiceFile.errorString();
            ui->voiceStatusLabel->setText("Status: File Error");
            return;
        }

        voiceData = voiceFile.readAll();
        voiceFile.close();

        if (voiceData.isEmpty()) {
            qDebug() << "FILE ERROR: File is empty or could not be read.";
            return;
        }

        qDebug() << "Voice file loaded successfully," << voiceData.size() << "bytes.";

        ma_decoder_config cfg = ma_decoder_config_init(device.playback.format, device.playback.channels, device.sampleRate);
        if (ma_decoder_init_memory(voiceData.constData(), voiceData.size(), &cfg, &voiceDecoder) != MA_SUCCESS) {
            ui->voiceStatusLabel->setText("Status: Decode Error");
            return;
        }

        ma_result result;

        ma_loshelf2_config lowConfig = ma_loshelf2_config_init(device.playback.format, device.playback.channels, device.sampleRate, 250, 0, 1.0);
        result = ma_loshelf2_init(&lowConfig, NULL, &eqFilterLow);
        if (result != MA_SUCCESS) {
            qDebug() << "CRITICAL: Failed to initialize LOW SHELF filter. Error:" << result;
            return;
        }

        ma_peak2_config midConfig = ma_peak2_config_init(device.playback.format, device.playback.channels, device.sampleRate, 1200, 0.707, 0);
        result = ma_peak2_init(&midConfig, NULL, &eqFilterMid);
        if (result != MA_SUCCESS) {
            qDebug() << "CRITICAL: Failed to initialize MID PEAK filter. Error:" << result;
            return;
        }

        ma_hishelf2_config highConfig = ma_hishelf2_config_init(device.playback.format, device.playback.channels, device.sampleRate, 5000, 0, 1.0);
        result = ma_hishelf2_init(&highConfig, NULL, &eqFilterHigh);
        if (result != MA_SUCCESS) {
            qDebug() << "CRITICAL: Failed to initialize HIGH SHELF filter. Error:" << result;
            return;
        }

        qDebug() << "All EQ filters initialized successfully.";

        ui->slider_voice_low->setValue(100);
        ui->slider_voice_mid->setValue(100);
        ui->slider_voice_high->setValue(100);

        isVoiceLoaded = true;
        currentVoiceName = selected;
    }

    // Toggle playing state
    isVoicePlaying = !isVoicePlaying;

    if (isVoicePlaying) {
        // --- 2: Reset for playback ---
        ma_decoder_seek_to_pcm_frame(&voiceDecoder, 0);

        // Update UI
        ui->voiceStatusLabel->setText("Status: Playing");
        ui->playVoiceButton->setText("Stop Voice");
    } else {
        ui->voiceStatusLabel->setText("Status: Stopped");
        ui->playVoiceButton->setText("Play Voice");
    }
}

void MainWindow::stopVoicePlayback()
{
    QMutexLocker locker(&audioStateMutex);
    isVoicePlaying = false;
    ui->voiceStatusLabel->setText("Status: Finished");
    ui->playVoiceButton->setText("Play Voice");
}

void MainWindow::onVoiceMasterVolumeChanged(int value)
{
    if (!audioInitialized) return;
    QMutexLocker locker(&audioStateMutex);
    voiceVolume = value / 100.0f;
}

void MainWindow::onVoiceEqLowChanged(int value)
{
    if (!isVoiceLoaded) return;
    if (!audioInitialized) return;
    double gainDB = (value / 100.0) * 24.0 - 24.0;

    QMutexLocker locker(&audioStateMutex);
    // Correct order: frequency, gainDB, slope
    ma_loshelf2_config config = ma_loshelf2_config_init(device.playback.format, device.playback.channels, device.sampleRate, 250, gainDB, 1.0);
    ma_loshelf2_reinit(&config, &eqFilterLow);
}

void MainWindow::onVoiceEqMidChanged(int value)
{
    if (!isVoiceLoaded) return;
    if (!audioInitialized) return;
    double gainDB = (value / 100.0) * 24.0 - 24.0;

    QMutexLocker locker(&audioStateMutex);
    // Correct order: frequency, Q, gainDB
    ma_peak2_config config = ma_peak2_config_init(device.playback.format, device.playback.channels, device.sampleRate, 1200, 0.707, gainDB);
    ma_peak2_reinit(&config, &eqFilterMid);
}

void MainWindow::onVoiceEqHighChanged(int value)
{
    if (!isVoiceLoaded) return;
    if (!audioInitialized) return;
    double gainDB = (value / 100.0) * 24.0 - 24.0;

    QMutexLocker locker(&audioStateMutex);
    // Correct order: frequency, gainDB, slope
    ma_hishelf2_config config = ma_hishelf2_config_init(device.playback.format, device.playback.channels, device.sampleRate, 5000, gainDB, 1.0);
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
        QString fullPath = backgroundDir.filePath(filename);
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
        QString fullPath = voiceDir.filePath(filename);
        voicePathMap[prettyName] = fullPath;
    }

    ui->voiceComboBox->addItems(voicePathMap.keys());
    qDebug() << "Found and added" << voicePathMap.size() << "voice files.";
}
