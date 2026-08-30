#ifndef AUDIOLEVELMONITOR_H
#define AUDIOLEVELMONITOR_H

#include <QObject>

class QAudioSource;
class QIODevice;
class QTimer;
class SystemAudioCapture;

class AudioLevelMonitor : public QObject
{
    Q_OBJECT

public:
    explicit AudioLevelMonitor(QObject *parent = nullptr);
    ~AudioLevelMonitor() override;

    void setActive(bool mic, bool system);
    void stop();

signals:
    void micLevelChanged(qreal level);
    void systemLevelChanged(qreal level);

private:
    void sync();
    void startMic();
    void stopMic();
    void startSystem();
    void stopSystem();
    void onMicReady();
    void decayTick();

    QAudioSource *m_mic = nullptr;
    QIODevice *m_micIo = nullptr;
    SystemAudioCapture *m_system = nullptr;
    QTimer *m_decay = nullptr;
    bool m_wantMic = false;
    bool m_wantSystem = false;
    qreal m_micLevel = 0;
    qreal m_systemLevel = 0;
};

#endif
