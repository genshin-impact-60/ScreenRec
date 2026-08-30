#ifndef RECORDINGCONTROLLER_H
#define RECORDINGCONTROLLER_H

#include <QCapturableWindow>
#include <QElapsedTimer>
#include <QObject>
#include <QRect>
#include <QSize>
#include <QString>

class QAudioBufferInput;
class QAudioInput;
class QMediaCaptureSession;
class QMediaRecorder;
class QScreen;
class QTimer;
class QVideoFrame;
class QVideoFrameInput;
class QVideoSink;
class QWindowCapture;
class ScreenCaptureSource;
class SystemAudioCapture;

class RecordingController : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Idle,
        Countdown,
        Recording,
        Paused,
        Stopping
    };
    Q_ENUM(State)

    enum class CaptureMode {
        Screen,
        Region,
        Window
    };
    Q_ENUM(CaptureMode)

    struct Request {
        CaptureMode mode = CaptureMode::Screen;
        QScreen *screen = nullptr;
        QRect region;
        QCapturableWindow window;
        QString outputDirectory;
        bool recordMicrophone = false;
        bool recordSystemAudio = false;
        int countdownSeconds = 3;
        int frameRate = 30;
        int quality = 3; // QMediaRecorder::HighQuality
        QSize maxResolution;
    };

    explicit RecordingController(QObject *parent = nullptr);

    State state() const { return m_state; }
    qint64 durationMs() const;
    QString currentFilePath() const { return m_actualPath; }

    void setPreviewOutput(QObject *output);

public slots:
    void start(const Request &request);
    void pause();
    void resume();
    void stop();
    void cancelCountdown();

signals:
    void stateChanged(RecordingController::State state);
    void errorOccurred(const QString &message);
    void warningOccurred(const QString &message);
    void durationChanged(qint64 milliseconds);
    void countdownTick(int remainingSeconds);
    void recordingFinished(const QString &filePath);

private:
    void setState(State state);
    void beginCapture();
    void fail(const QString &message);
    void cleanupCapture();
    bool prepareOutputPath(QString *error);
    void onRecorderStateChanged();
    void onCountdownTimeout();
    void onVideoFrame(const QVideoFrame &frame);
    void onSystemPcm(const QByteArray &pcm);
    QRect mappedCropRect(const QSize &frameSize) const;
    bool applyAudio(QMediaCaptureSession *session);
    QSize fitOutputSize(QSize source) const;

    State m_state = State::Idle;
    Request m_request;
    QString m_actualPath;
    QString m_pendingPath;
    bool m_handlingError = false;
    bool m_processFrames = false;
    QRect m_physicalCrop;
    qint64 m_lastVideoUs = -1;
    qint64 m_audioUs = -1;
    QElapsedTimer m_videoClock;

    QMediaCaptureSession *m_session = nullptr;
    QMediaCaptureSession *m_encodeSession = nullptr;
    ScreenCaptureSource *m_source = nullptr;
    QWindowCapture *m_windowCapture = nullptr;
    QAudioInput *m_audioInput = nullptr;
    QAudioBufferInput *m_audioBufferInput = nullptr;
    SystemAudioCapture *m_systemAudio = nullptr;
    QMediaRecorder *m_recorder = nullptr;
    QVideoSink *m_videoSink = nullptr;
    QVideoFrameInput *m_frameInput = nullptr;
    QTimer *m_countdownTimer = nullptr;
    int m_countdownRemaining = 0;
};

#endif
