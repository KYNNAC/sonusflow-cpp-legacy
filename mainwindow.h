#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "miniaudio.h"
#include <QMainWindow>
#include <QByteArray>
#include <QMap>
#include <QString>
#include <QMutex>

#define MA_ENABLE_MP3
#define MA_ENABLE_FLAC
#define MA_ENABLE_FILTERS


#include "miniaudio.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onAmbienceChanged(int index);
    void onPlayVoiceClicked();
    void onVoiceSelectionChanged(int index);
    void onPlayBackgroundClicked();
    void onVoiceMasterVolumeChanged(int value);
    void onVoiceEqLowChanged(int value);
    void onVoiceEqMidChanged(int value);
    void onVoiceEqHighChanged(int value);
    void stopVoicePlayback();

private:
    Ui::MainWindow *ui;

    // MiniAudio Members
    ma_device device;
    bool audioInitialized;
    ma_loshelf2 eqFilterLow;
    ma_peak2   eqFilterMid;
    ma_hishelf2 eqFilterHigh;
    float voiceVolume;
    QMutex audioStateMutex;

    // Voice sound variables
    ma_decoder voiceDecoder;
    bool isVoiceLoaded;
    bool isVoicePlaying;
    QString currentVoiceName;

    // Background sound variables
    ma_decoder backgroundDecoder;
    bool isBackgroundLoaded;
    bool isBackgroundPlaying;
    QString currentBackgroundName;

    // Sonus Flow Data
    QMap<QString, QString> voicePathMap;
    QByteArray voiceData;
    QMap<QString, QString> backgroundPathMap;
    QByteArray backgroundData;

    // Private Methods
    void initializeAudio();
    void shutdownAudio();
    void populateVoiceComboBox();
    void populateBackgroundComboBox();

    static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
};

#endif
