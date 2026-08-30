#ifndef SYSTEMAUDIOCAPTURE_H
#define SYSTEMAUDIOCAPTURE_H

#include <QAudioFormat>
#include <QByteArray>
#include <QObject>

class QAudioSource;
class QIODevice;
class QThread;

class SystemAudioCapture : public QObject
{
    Q_OBJECT

public:
    explicit SystemAudioCapture(QObject *parent = nullptr);
    ~SystemAudioCapture() override;

    static QAudioFormat mixFormat();

    bool start(bool includeMicrophone, QString *error, QString *warning);
    void stop();
    bool isRunning() const { return m_running; }

signals:
    void pcmReady(const QByteArray &pcm);
    void failed(const QString &message);

private slots:
    void onLoopbackPcm(const QByteArray &systemPcm);
    void onLoopbackFailed(const QString &message);

private:
    bool startMicrophone(QString *warning);
    QByteArray mix(const QByteArray &systemPcm, const QByteArray &micPcm) const;
    QByteArray consumeMic(int byteCount);

    QThread *m_thread = nullptr;
    QAudioSource *m_mic = nullptr;
    QIODevice *m_micIo = nullptr;
    QAudioFormat m_micFormat;
    QByteArray m_micNativeRemainder;
    QByteArray m_micRemainder;
    bool m_running = false;
    bool m_includeMic = false;
};

#endif
