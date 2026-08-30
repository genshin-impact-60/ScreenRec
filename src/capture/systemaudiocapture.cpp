#include "systemaudiocapture.h"

#include <QAudioDevice>
#include <QAudioSource>
#include <QIODevice>
#include <QMediaDevices>
#include <QMetaObject>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include <atomic>
#include <vector>

#ifdef Q_OS_WIN
#    define INITGUID
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#    include <audioclient.h>
#    include <ks.h>
#    include <ksmedia.h>
#    include <mmdeviceapi.h>
#    include <mmreg.h>
#    include <objbase.h>
#endif

#ifndef AUDCLNT_STREAMFLAGS_LOOPBACK
#    define AUDCLNT_STREAMFLAGS_LOOPBACK 0x00020000
#endif
#ifndef AUDCLNT_STREAMFLAGS_EVENTCALLBACK
#    define AUDCLNT_STREAMFLAGS_EVENTCALLBACK 0x00040000
#endif

namespace {

constexpr int kMixRate = 48000;

QAudioFormat makeMixFormat()
{
    QAudioFormat format;
    format.setSampleRate(kMixRate);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);
    format.setChannelConfig(QAudioFormat::ChannelConfigStereo);
    return format;
}

void resampleStereo(const float *src, int srcFrames, int srcRate, std::vector<float> *dst, int dstRate)
{
    dst->clear();
    if (srcFrames <= 0 || srcRate <= 0)
        return;
    if (srcRate == dstRate) {
        dst->assign(src, src + srcFrames * 2);
        return;
    }
    const int dstFrames = qMax(1, int(qint64(srcFrames) * dstRate / srcRate));
    dst->resize(size_t(dstFrames * 2));
    const double step = double(srcFrames) / double(dstFrames);
    for (int i = 0; i < dstFrames; ++i) {
        const double srcPos = i * step;
        const int i0 = qMin(srcFrames - 1, int(srcPos));
        const int i1 = qMin(srcFrames - 1, i0 + 1);
        const float t = float(srcPos - i0);
        (*dst)[size_t(i * 2)] = src[i0 * 2] * (1.f - t) + src[i1 * 2] * t;
        (*dst)[size_t(i * 2 + 1)] = src[i0 * 2 + 1] * (1.f - t) + src[i1 * 2 + 1] * t;
    }
}

QByteArray floatStereoToInt16(const float *src, int frames)
{
    QByteArray out;
    out.resize(frames * 4);
    auto *dst = reinterpret_cast<qint16 *>(out.data());
    for (int i = 0; i < frames * 2; ++i)
        dst[i] = qint16(qBound(-1.0f, src[i], 1.0f) * 32767.0f);
    return out;
}

void pcmToStereoFloat(const char *data, int frames, const QAudioFormat &format, std::vector<float> *out)
{
    out->assign(size_t(frames * 2), 0.f);
    const int channels = qMax(1, format.channelCount());
    for (int i = 0; i < frames; ++i) {
        const char *frame = data + i * format.bytesPerFrame();
        float left = 0.f;
        float right = 0.f;
        switch (format.sampleFormat()) {
        case QAudioFormat::Float: {
            const auto *s = reinterpret_cast<const float *>(frame);
            left = s[0];
            right = channels > 1 ? s[1] : s[0];
            break;
        }
        case QAudioFormat::Int16: {
            const auto *s = reinterpret_cast<const qint16 *>(frame);
            left = s[0] / 32768.0f;
            right = channels > 1 ? s[1] / 32768.0f : left;
            break;
        }
        case QAudioFormat::Int32: {
            const auto *s = reinterpret_cast<const qint32 *>(frame);
            left = s[0] / 2147483648.0f;
            right = channels > 1 ? s[1] / 2147483648.0f : left;
            break;
        }
        case QAudioFormat::UInt8:
            left = (quint8(frame[0]) - 128) / 128.0f;
            right = channels > 1 ? (quint8(frame[1]) - 128) / 128.0f : left;
            break;
        default:
            break;
        }
        (*out)[size_t(i * 2)] = left;
        (*out)[size_t(i * 2 + 1)] = right;
    }
}

QByteArray convertToMixPcm(const QByteArray &src, const QAudioFormat &srcFormat)
{
    if (!srcFormat.isValid() || srcFormat.bytesPerFrame() <= 0)
        return {};
    const int frames = src.size() / srcFormat.bytesPerFrame();
    if (frames <= 0)
        return {};
    std::vector<float> stereo;
    pcmToStereoFloat(src.constData(), frames, srcFormat, &stereo);
    std::vector<float> resampled;
    resampleStereo(stereo.data(), frames, srcFormat.sampleRate(), &resampled, kMixRate);
    if (resampled.empty())
        return {};
    return floatStereoToInt16(resampled.data(), int(resampled.size() / 2));
}

#ifdef Q_OS_WIN

bool isIeeeFloat(const WAVEFORMATEX *wfx)
{
    if (wfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
        return true;
    if (wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE && wfx->cbSize >= 22) {
        const auto *ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(wfx);
        return ext->SubFormat.Data1 == WAVE_FORMAT_IEEE_FLOAT;
    }
    return false;
}

void packetToStereoFloat(const BYTE *data, UINT32 frames, const WAVEFORMATEX *wfx, std::vector<float> *out)
{
    out->assign(size_t(frames * 2), 0.f);
    if (!data || frames == 0)
        return;
    const int channels = qMax(1, int(wfx->nChannels));
    const int bits = int(wfx->wBitsPerSample);
    const bool floatFmt = isIeeeFloat(wfx);
    for (UINT32 i = 0; i < frames; ++i) {
        const BYTE *frame = data + i * wfx->nBlockAlign;
        float left = 0.f;
        float right = 0.f;
        if (floatFmt && bits == 32) {
            const auto *s = reinterpret_cast<const float *>(frame);
            left = s[0];
            right = channels > 1 ? s[1] : s[0];
        } else if (bits == 16) {
            const auto *s = reinterpret_cast<const qint16 *>(frame);
            left = s[0] / 32768.0f;
            right = channels > 1 ? s[1] / 32768.0f : left;
        } else if (bits == 32 && !floatFmt) {
            const auto *s = reinterpret_cast<const qint32 *>(frame);
            left = s[0] / 2147483648.0f;
            right = channels > 1 ? s[1] / 2147483648.0f : left;
        } else if (bits == 8) {
            left = (frame[0] - 128) / 128.0f;
            right = channels > 1 ? (frame[1] - 128) / 128.0f : left;
        }
        (*out)[size_t(i * 2)] = left;
        (*out)[size_t(i * 2 + 1)] = right;
    }
}

} // namespace

class LoopbackThread : public QThread
{
public:
    explicit LoopbackThread(QObject *receiver)
        : m_receiver(receiver)
        , m_stopEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr))
    {
    }

    ~LoopbackThread() override
    {
        if (m_stopEvent)
            CloseHandle(m_stopEvent);
    }

    void requestStop()
    {
        m_stop = true;
        if (m_stopEvent)
            SetEvent(m_stopEvent);
    }

    QMutex initMutex;
    QWaitCondition initWait;
    bool initFinished = false;
    bool initOk = false;
    QString initError;

protected:
    void run() override
    {
        const auto finishInit = [this](bool ok, const QString &error) {
            QMutexLocker locker(&initMutex);
            initFinished = true;
            initOk = ok;
            initError = error;
            initWait.wakeAll();
        };

        struct ComGuard {
            bool active = false;
            ~ComGuard()
            {
                if (active)
                    CoUninitialize();
            }
        } comGuard;

        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (hr == S_OK) {
            comGuard.active = true;
        } else if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            finishInit(false, QStringLiteral("无法初始化 COM。"));
            return;
        }

        IMMDeviceEnumerator *enumerator = nullptr;
        hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                              IID_IMMDeviceEnumerator, reinterpret_cast<void **>(&enumerator));
        if (FAILED(hr) || !enumerator) {
            finishInit(false, QStringLiteral("无法创建音频设备枚举器。"));
            return;
        }

        IMMDevice *device = nullptr;
        hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        enumerator->Release();
        if (FAILED(hr) || !device) {
            finishInit(false, QStringLiteral("找不到默认播放设备，无法录制系统声音。"));
            return;
        }

        IAudioClient *client = nullptr;
        hr = device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr,
                              reinterpret_cast<void **>(&client));
        device->Release();
        if (FAILED(hr) || !client) {
            finishInit(false, QStringLiteral("无法激活音频客户端。"));
            return;
        }

        WAVEFORMATEX *mix = nullptr;
        hr = client->GetMixFormat(&mix);
        if (FAILED(hr) || !mix) {
            client->Release();
            finishInit(false, QStringLiteral("无法读取播放设备格式。"));
            return;
        }

        const REFERENCE_TIME bufferDuration = 2000000;
        bool eventMode = true;
        HANDLE eventHandle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                bufferDuration, 0, mix, nullptr);
        if (FAILED(hr)) {
            eventMode = false;
            if (eventHandle) {
                CloseHandle(eventHandle);
                eventHandle = nullptr;
            }
            hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
                                    bufferDuration, 0, mix, nullptr);
        }
        if (FAILED(hr)) {
            CoTaskMemFree(mix);
            client->Release();
            finishInit(false, QStringLiteral("无法以 loopback 方式打开播放设备。"));
            return;
        }

        if (eventMode) {
            hr = client->SetEventHandle(eventHandle);
            if (FAILED(hr))
                eventMode = false;
        }

        IAudioCaptureClient *capture = nullptr;
        hr = client->GetService(IID_IAudioCaptureClient, reinterpret_cast<void **>(&capture));
        if (FAILED(hr) || !capture) {
            CoTaskMemFree(mix);
            client->Release();
            if (eventHandle)
                CloseHandle(eventHandle);
            finishInit(false, QStringLiteral("无法创建 loopback 采集服务。"));
            return;
        }

        hr = client->Start();
        if (FAILED(hr)) {
            capture->Release();
            CoTaskMemFree(mix);
            client->Release();
            if (eventHandle)
                CloseHandle(eventHandle);
            finishInit(false, QStringLiteral("无法开始系统内录。"));
            return;
        }

        finishInit(true, {});

        std::vector<float> stereo;
        std::vector<float> resampled;
        const int srcRate = int(mix->nSamplesPerSec);

        while (!m_stop) {
            if (eventMode && eventHandle) {
                HANDLE handles[2] = {eventHandle, m_stopEvent};
                const DWORD count = m_stopEvent ? 2 : 1;
                WaitForMultipleObjects(count, handles, FALSE, 100);
            } else if (m_stopEvent) {
                WaitForSingleObject(m_stopEvent, 10);
            } else {
                Sleep(10);
            }
            if (m_stop)
                break;

            UINT32 packetFrames = 0;
            hr = capture->GetNextPacketSize(&packetFrames);
            if (FAILED(hr))
                break;

            while (packetFrames > 0 && !m_stop) {
                BYTE *data = nullptr;
                UINT32 frames = 0;
                DWORD flags = 0;
                hr = capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
                if (FAILED(hr))
                    break;

                stereo.assign(size_t(frames * 2), 0.f);
                if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && data)
                    packetToStereoFloat(data, frames, mix, &stereo);

                capture->ReleaseBuffer(frames);
                resampleStereo(stereo.data(), int(frames), srcRate, &resampled, kMixRate);
                if (!resampled.empty()) {
                    const QByteArray pcm =
                        floatStereoToInt16(resampled.data(), int(resampled.size() / 2));
                    QMetaObject::invokeMethod(m_receiver, "onLoopbackPcm", Qt::QueuedConnection,
                                              Q_ARG(QByteArray, pcm));
                }

                hr = capture->GetNextPacketSize(&packetFrames);
                if (FAILED(hr))
                    break;
            }
        }

        client->Stop();
        capture->Release();
        CoTaskMemFree(mix);
        client->Release();
        if (eventHandle)
            CloseHandle(eventHandle);
    }

private:
    QObject *m_receiver = nullptr;
    HANDLE m_stopEvent = nullptr;
    std::atomic<bool> m_stop{false};
};

#endif // Q_OS_WIN

SystemAudioCapture::SystemAudioCapture(QObject *parent)
    : QObject(parent)
{
}

SystemAudioCapture::~SystemAudioCapture()
{
    stop();
}

QAudioFormat SystemAudioCapture::mixFormat()
{
    return makeMixFormat();
}

bool SystemAudioCapture::start(bool includeMicrophone, QString *error, QString *warning)
{
    stop();

#ifndef Q_OS_WIN
    Q_UNUSED(includeMicrophone);
    Q_UNUSED(warning);
    if (error)
        *error = tr("系统内录目前仅支持 Windows。");
    return false;
#else
    auto *thread = new LoopbackThread(this);
    m_thread = thread;
    thread->start();

    {
        QMutexLocker locker(&thread->initMutex);
        if (!thread->initFinished)
            thread->initWait.wait(&thread->initMutex, 2500);
        if (!thread->initOk) {
            const QString message = thread->initError.isEmpty()
                                        ? tr("系统内录启动超时。")
                                        : thread->initError;
            locker.unlock();
            stop();
            if (error)
                *error = message;
            return false;
        }
    }

    m_includeMic = includeMicrophone;
    m_running = true;
    if (includeMicrophone) {
        QString micWarning;
        if (!startMicrophone(&micWarning) && warning)
            *warning = micWarning;
    }
    return true;
#endif
}

void SystemAudioCapture::stop()
{
#ifdef Q_OS_WIN
    if (auto *thread = static_cast<LoopbackThread *>(m_thread)) {
        thread->requestStop();
        if (thread->wait(5000)) {
            delete thread;
        } else {
            qWarning("ScreenRec: system audio thread did not stop in time");
            QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        }
    }
    m_thread = nullptr;
#endif
    if (m_mic) {
        m_mic->stop();
        delete m_mic;
        m_mic = nullptr;
        m_micIo = nullptr;
    }
    m_micNativeRemainder.clear();
    m_micRemainder.clear();
    m_running = false;
}

bool SystemAudioCapture::startMicrophone(QString *warning)
{
    const QAudioDevice device = QMediaDevices::defaultAudioInput();
    if (device.isNull()) {
        if (warning)
            *warning = tr("未找到麦克风，将只录制系统声音。");
        return false;
    }

    QAudioFormat format = mixFormat();
    if (!device.isFormatSupported(format))
        format = device.preferredFormat();

    m_micFormat = format;
    m_mic = new QAudioSource(device, format, this);
    m_micIo = m_mic->start();
    if (!m_micIo || m_mic->error() != QtAudio::NoError) {
        delete m_mic;
        m_mic = nullptr;
        m_micIo = nullptr;
        if (warning)
            *warning = tr("无法打开麦克风，将只录制系统声音。");
        return false;
    }
    return true;
}

QByteArray SystemAudioCapture::mix(const QByteArray &systemPcm, const QByteArray &micPcm) const
{
    QByteArray mixed = systemPcm;
    const int samples = mixed.size() / 2;
    auto *dst = reinterpret_cast<qint16 *>(mixed.data());
    const auto *mic = reinterpret_cast<const qint16 *>(micPcm.constData());
    const int micSamples = micPcm.size() / 2;
    for (int i = 0; i < samples; ++i) {
        const int micValue = (i < micSamples) ? int(mic[i]) : 0;
        const int mixed = (int(dst[i]) * 7 + micValue * 7) / 10;
        dst[i] = qint16(qBound(-32768, mixed, 32767));
    }
    return mixed;
}

QByteArray SystemAudioCapture::consumeMic(int byteCount)
{
    if (!m_micIo || byteCount <= 0)
        return {};

    m_micNativeRemainder.append(m_micIo->readAll());
    const int bpf = m_micFormat.bytesPerFrame();
    if (bpf <= 0)
        return {};
    const int complete = m_micNativeRemainder.size() / bpf * bpf;
    if (complete > 0) {
        m_micRemainder.append(convertToMixPcm(m_micNativeRemainder.left(complete), m_micFormat));
        m_micNativeRemainder.remove(0, complete);
    }

    QByteArray chunk;
    if (m_micRemainder.size() >= byteCount) {
        chunk = m_micRemainder.left(byteCount);
        m_micRemainder.remove(0, byteCount);
    } else {
        chunk = m_micRemainder;
        m_micRemainder.clear();
    }

    const int maxQueued = mixFormat().bytesForDuration(500000);
    if (m_micRemainder.size() > maxQueued)
        m_micRemainder.remove(0, m_micRemainder.size() - maxQueued);
    return chunk;
}

void SystemAudioCapture::onLoopbackPcm(const QByteArray &systemPcm)
{
    if (!m_running || systemPcm.isEmpty())
        return;
    if (m_includeMic && m_micIo)
        emit pcmReady(mix(systemPcm, consumeMic(systemPcm.size())));
    else
        emit pcmReady(systemPcm);
}

void SystemAudioCapture::onLoopbackFailed(const QString &message)
{
    emit failed(message);
}
