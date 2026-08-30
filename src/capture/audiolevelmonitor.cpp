#include "audiolevelmonitor.h"
#include "systemaudiocapture.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QIODevice>
#include <QMediaDevices>
#include <QTimer>

namespace {

qreal peakFromInt16(const QByteArray &pcm)
{
    if (pcm.size() < 2)
        return 0;
    const auto *samples = reinterpret_cast<const qint16 *>(pcm.constData());
    const int count = pcm.size() / int(sizeof(qint16));
    int peak = 0;
    for (int i = 0; i < count; ++i)
        peak = qMax(peak, qAbs(int(samples[i])));
    return qMin(1.0, peak / 32768.0);
}

qreal peakFromDevicePcm(const QByteArray &pcm, const QAudioFormat &format)
{
    if (!format.isValid() || format.bytesPerFrame() <= 0 || pcm.isEmpty())
        return 0;
    qreal peak = 0;
    const int frames = pcm.size() / format.bytesPerFrame();
    const int channels = qMax(1, format.channelCount());
    for (int i = 0; i < frames; ++i) {
        const char *frame = pcm.constData() + i * format.bytesPerFrame();
        for (int c = 0; c < channels; ++c) {
            qreal sample = 0;
            switch (format.sampleFormat()) {
            case QAudioFormat::Int16:
                sample = reinterpret_cast<const qint16 *>(frame)[c] / 32768.0;
                break;
            case QAudioFormat::Int32:
                sample = reinterpret_cast<const qint32 *>(frame)[c] / 2147483648.0;
                break;
            case QAudioFormat::Float:
                sample = qreal(reinterpret_cast<const float *>(frame)[c]);
                break;
            case QAudioFormat::UInt8:
                sample = (quint8(frame[c]) - 128) / 128.0;
                break;
            default:
                break;
            }
            peak = qMax(peak, qAbs(sample));
        }
    }
    return qMin(1.0, peak);
}

} // namespace

AudioLevelMonitor::AudioLevelMonitor(QObject *parent)
    : QObject(parent)
    , m_system(new SystemAudioCapture(this))
    , m_decay(new QTimer(this))
{
    m_decay->setInterval(80);
    connect(m_decay, &QTimer::timeout, this, &AudioLevelMonitor::decayTick);
    connect(m_system, &SystemAudioCapture::pcmReady, this, [this](const QByteArray &pcm) {
        m_systemLevel = qMax(m_systemLevel * 0.55, peakFromInt16(pcm));
        emit systemLevelChanged(m_systemLevel);
    });
    connect(m_system, &SystemAudioCapture::failed, this, [this](const QString &) {
        m_systemLevel = 0;
        emit systemLevelChanged(0);
    });
}

AudioLevelMonitor::~AudioLevelMonitor()
{
    stop();
}

void AudioLevelMonitor::setActive(bool mic, bool system)
{
    m_wantMic = mic;
    m_wantSystem = system;
    sync();
}

void AudioLevelMonitor::stop()
{
    m_wantMic = false;
    m_wantSystem = false;
    sync();
}

void AudioLevelMonitor::sync()
{
    if (m_wantMic)
        startMic();
    else
        stopMic();

    if (m_wantSystem)
        startSystem();
    else
        stopSystem();

    if (m_wantMic || m_wantSystem) {
        if (!m_decay->isActive())
            m_decay->start();
    } else {
        m_decay->stop();
        if (m_micLevel != 0) {
            m_micLevel = 0;
            emit micLevelChanged(0);
        }
        if (m_systemLevel != 0) {
            m_systemLevel = 0;
            emit systemLevelChanged(0);
        }
    }
}

void AudioLevelMonitor::startMic()
{
    if (m_mic)
        return;

    const QAudioDevice device = QMediaDevices::defaultAudioInput();
    if (device.isNull()) {
        m_micLevel = 0;
        emit micLevelChanged(0);
        return;
    }

    QAudioFormat format = device.preferredFormat();
    if (!device.isFormatSupported(format) || format.sampleFormat() == QAudioFormat::Unknown) {
        format = QAudioFormat();
        format.setSampleRate(48000);
        format.setChannelCount(1);
        format.setSampleFormat(QAudioFormat::Int16);
        format.setChannelConfig(QAudioFormat::ChannelConfigMono);
    }

    m_mic = new QAudioSource(device, format, this);
    m_mic->setBufferSize(format.bytesForDuration(40000));
    m_micIo = m_mic->start();
    if (!m_micIo) {
        delete m_mic;
        m_mic = nullptr;
        return;
    }
    connect(m_micIo, &QIODevice::readyRead, this, &AudioLevelMonitor::onMicReady);
}

void AudioLevelMonitor::stopMic()
{
    if (!m_mic)
        return;
    m_mic->stop();
    m_mic->deleteLater();
    m_mic = nullptr;
    m_micIo = nullptr;
}

void AudioLevelMonitor::startSystem()
{
    if (m_system->isRunning())
        return;
    QString error;
    QString warning;
    if (!m_system->start(false, &error, &warning)) {
        m_systemLevel = 0;
        emit systemLevelChanged(0);
    }
}

void AudioLevelMonitor::stopSystem()
{
    if (m_system->isRunning())
        m_system->stop();
}

void AudioLevelMonitor::onMicReady()
{
    if (!m_mic || !m_micIo)
        return;
    const QByteArray pcm = m_micIo->readAll();
    m_micLevel = qMax(m_micLevel * 0.45, peakFromDevicePcm(pcm, m_mic->format()));
    emit micLevelChanged(m_micLevel);
}

void AudioLevelMonitor::decayTick()
{
    if (m_wantMic) {
        m_micLevel *= 0.82;
        if (m_micLevel < 0.01)
            m_micLevel = 0;
        emit micLevelChanged(m_micLevel);
    }
    if (m_wantSystem) {
        m_systemLevel *= 0.82;
        if (m_systemLevel < 0.01)
            m_systemLevel = 0;
        emit systemLevelChanged(m_systemLevel);
    }
}
