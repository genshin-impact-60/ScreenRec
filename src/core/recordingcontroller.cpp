#include "recordingcontroller.h"
#include "screencapturesource.h"
#include "systemaudiocapture.h"

#include <QAudioBuffer>
#include <QAudioBufferInput>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioInput>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QMediaFormat>
#include <QMediaRecorder>
#include <QScreen>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoFrameInput>
#include <QVideoSink>
#include <QWindowCapture>

#if QT_CONFIG(permissions)
#    include <QPermission>
#endif

namespace {

QString extensionFor(const QMediaFormat &format)
{
    switch (format.fileFormat()) {
    case QMediaFormat::Matroska:
        return QStringLiteral("mkv");
    case QMediaFormat::WebM:
        return QStringLiteral("webm");
    case QMediaFormat::QuickTime:
        return QStringLiteral("mov");
    case QMediaFormat::AVI:
        return QStringLiteral("avi");
    case QMediaFormat::MPEG4:
    default:
        return QStringLiteral("mp4");
    }
}

QMediaFormat chooseFormat(bool withAudio)
{
    QMediaFormat format(QMediaFormat::MPEG4);
    format.setVideoCodec(QMediaFormat::VideoCodec::H264);
    if (withAudio)
        format.setAudioCodec(QMediaFormat::AudioCodec::AAC);
    format.resolveForEncoding(QMediaFormat::RequiresVideo);
    return format;
}

QRect evenRect(QRect rect)
{
    rect.setWidth(rect.width() & ~1);
    rect.setHeight(rect.height() & ~1);
    return rect;
}

QSize evenSize(QSize size)
{
    size.setWidth(size.width() & ~1);
    size.setHeight(size.height() & ~1);
    return size;
}

} // namespace

RecordingController::RecordingController(QObject *parent)
    : QObject(parent)
    , m_session(new QMediaCaptureSession(this))
    , m_encodeSession(new QMediaCaptureSession(this))
    , m_source(new ScreenCaptureSource(this))
    , m_windowCapture(new QWindowCapture(this))
    , m_audioInput(new QAudioInput(this))
    , m_audioBufferInput(new QAudioBufferInput(SystemAudioCapture::mixFormat(), this))
    , m_systemAudio(new SystemAudioCapture(this))
    , m_recorder(new QMediaRecorder(this))
    , m_videoSink(new QVideoSink(this))
    , m_frameInput(new QVideoFrameInput(this))
    , m_countdownTimer(new QTimer(this))
{
    m_session->setScreenCapture(m_source->qtCapture());
    m_session->setWindowCapture(m_windowCapture);
    m_session->setRecorder(m_recorder);

    m_countdownTimer->setInterval(1000);

    connect(m_recorder, &QMediaRecorder::recorderStateChanged, this,
            [this](QMediaRecorder::RecorderState) { onRecorderStateChanged(); });
    connect(m_recorder, &QMediaRecorder::durationChanged, this,
            &RecordingController::durationChanged);
    connect(m_recorder, &QMediaRecorder::actualLocationChanged, this,
            [this](const QUrl &url) { m_actualPath = url.toLocalFile(); });
    connect(m_recorder, &QMediaRecorder::errorOccurred, this,
            [this](QMediaRecorder::Error error, const QString &errorString) {
                if (error == QMediaRecorder::NoError)
                    return;
                fail(errorString.isEmpty() ? tr("录制失败。") : errorString);
            });
    connect(m_source, &ScreenCaptureSource::errorOccurred, this, &RecordingController::fail);
    connect(m_windowCapture, &QWindowCapture::errorOccurred, this,
            [this](QWindowCapture::Error error, const QString &errorString) {
                if (error == QWindowCapture::NoError)
                    return;
                if (error == QWindowCapture::NotFound)
                    fail(tr("目标窗口已关闭，录制已停止。"));
                else
                    fail(errorString.isEmpty() ? tr("窗口采集失败。") : errorString);
            },
            Qt::QueuedConnection);
    connect(m_videoSink, &QVideoSink::videoFrameChanged, this, &RecordingController::onVideoFrame);
    connect(m_systemAudio, &SystemAudioCapture::pcmReady, this, &RecordingController::onSystemPcm);
    connect(m_systemAudio, &SystemAudioCapture::failed, this, &RecordingController::warningOccurred);
    connect(m_countdownTimer, &QTimer::timeout, this, &RecordingController::onCountdownTimeout);

    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this](QScreen *screen) {
        if (m_request.screen != screen)
            return;
        m_request.screen = nullptr;
        if (m_state == State::Idle || m_state == State::Stopping)
            return;
        if (m_request.mode == CaptureMode::Window)
            return;
        fail(tr("目标显示器已断开，录制已停止。"));
    });
}

qint64 RecordingController::durationMs() const
{
    return m_recorder ? m_recorder->duration() : 0;
}

void RecordingController::setPreviewOutput(QObject *output)
{
    m_session->setVideoOutput(output);
}

void RecordingController::start(const Request &request)
{
    if (m_state != State::Idle)
        return;

    m_request = request;
    if (m_request.mode != CaptureMode::Window) {
        if (!m_request.screen)
            m_request.screen = QGuiApplication::primaryScreen();
        if (!m_request.screen) {
            emit errorOccurred(tr("没有可用的显示器。"));
            return;
        }
    }
    if (m_request.mode == CaptureMode::Region) {
        const QRect region = m_request.region.normalized();
        if (region.width() < 16 || region.height() < 16) {
            emit errorOccurred(tr("请先选择有效的录制区域。"));
            return;
        }
        m_request.region = region;
    }
    if (m_request.mode == CaptureMode::Window && !m_request.window.isValid()) {
        emit errorOccurred(tr("请选择要录制的窗口。"));
        return;
    }
    if (m_request.outputDirectory.trimmed().isEmpty()) {
        emit errorOccurred(tr("请选择保存目录。"));
        return;
    }
    if (!m_recorder->isAvailable()) {
        emit errorOccurred(tr("当前环境没有可用的录制后端。"));
        return;
    }

    auto beginAfterPermission = [this]() {
        if (m_request.countdownSeconds > 0) {
            m_countdownRemaining = m_request.countdownSeconds;
            setState(State::Countdown);
            emit countdownTick(m_countdownRemaining);
            m_countdownTimer->start();
        } else {
            beginCapture();
        }
    };

#if QT_CONFIG(permissions)
    if (m_request.recordMicrophone) {
        QMicrophonePermission permission;
        switch (qApp->checkPermission(permission)) {
        case Qt::PermissionStatus::Undetermined:
            qApp->requestPermission(permission, this, [this, beginAfterPermission](const QPermission &result) {
                if (result.status() != Qt::PermissionStatus::Granted) {
                    m_request.recordMicrophone = false;
                    emit warningOccurred(tr("麦克风权限未授予，将只录制画面。"));
                }
                beginAfterPermission();
            });
            return;
        case Qt::PermissionStatus::Denied:
            m_request.recordMicrophone = false;
            emit warningOccurred(tr("麦克风权限被拒绝，将只录制画面。"));
            break;
        case Qt::PermissionStatus::Granted:
            break;
        }
    }
#endif

    beginAfterPermission();
}

void RecordingController::pause()
{
    if (m_state != State::Recording)
        return;
    m_recorder->pause();
}

void RecordingController::resume()
{
    if (m_state != State::Paused)
        return;
    m_recorder->record();
}

void RecordingController::stop()
{
    if (m_state == State::Idle || m_state == State::Stopping)
        return;

    if (m_state == State::Countdown) {
        cancelCountdown();
        return;
    }

    setState(State::Stopping);
    m_recorder->stop();
}

void RecordingController::cancelCountdown()
{
    if (m_state != State::Countdown)
        return;
    m_countdownTimer->stop();
    m_countdownRemaining = 0;
    setState(State::Idle);
}

void RecordingController::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(state);
}

bool RecordingController::applyAudio(QMediaCaptureSession *session)
{
    m_session->setAudioInput(nullptr);
    m_encodeSession->setAudioInput(nullptr);
    m_session->setAudioBufferInput(nullptr);
    m_encodeSession->setAudioBufferInput(nullptr);

    if (m_request.recordSystemAudio) {
        QString error;
        QString warning;
        if (m_systemAudio->start(m_request.recordMicrophone, &error, &warning)) {
            if (!warning.isEmpty())
                emit warningOccurred(warning);
            session->setAudioBufferInput(m_audioBufferInput);
            return true;
        }
        emit warningOccurred(error.isEmpty() ? tr("系统内录不可用。") : error);
    }

    if (!m_request.recordMicrophone)
        return false;

    const QAudioDevice device = QMediaDevices::defaultAudioInput();
    if (device.isNull()) {
        emit warningOccurred(tr("未找到麦克风，将只录制画面。"));
        return false;
    }

    m_audioInput->setDevice(device);
    session->setAudioInput(m_audioInput);
    return true;
}

void RecordingController::beginCapture()
{
    QString error;
    if (!prepareOutputPath(&error)) {
        fail(error);
        return;
    }

    m_source->stop();
    m_windowCapture->stop();
    m_session->setVideoSink(nullptr);
    m_encodeSession->setVideoFrameInput(nullptr);
    m_session->setRecorder(nullptr);
    m_encodeSession->setRecorder(nullptr);

    const bool region = m_request.mode == CaptureMode::Region;
    m_processFrames = region;
    QMediaCaptureSession *recordSession = m_processFrames ? m_encodeSession : m_session;
    const bool audioConnected = applyAudio(recordSession);

    const QMediaFormat format = chooseFormat(audioConnected);
    m_recorder->setMediaFormat(format);
    m_recorder->setQuality(QMediaRecorder::Quality(m_request.quality));
    m_recorder->setEncodingMode(QMediaRecorder::ConstantQualityEncoding);
    m_recorder->setVideoFrameRate(m_request.frameRate > 0 ? m_request.frameRate : 30.0);
    if (!region && m_request.maxResolution.isValid())
        m_recorder->setVideoResolution(evenSize(m_request.maxResolution));
    else
        m_recorder->setVideoResolution(QSize());

    const QString ext = extensionFor(m_recorder->mediaFormat());
    const QFileInfo pendingInfo(m_pendingPath);
    m_pendingPath = pendingInfo.dir().filePath(pendingInfo.completeBaseName() + QLatin1Char('.') + ext);

    m_actualPath.clear();
    m_lastVideoUs = -1;
    m_audioUs = -1;
    m_videoClock.restart();
    m_recorder->setOutputLocation(QUrl::fromLocalFile(m_pendingPath));
    recordSession->setRecorder(m_recorder);

    if (m_processFrames) {
        m_session->setVideoSink(m_videoSink);
        m_encodeSession->setVideoFrameInput(m_frameInput);
    }

    if (m_request.mode == CaptureMode::Window) {
        m_windowCapture->setWindow(m_request.window);
        m_windowCapture->start();
    } else {
        m_source->setScreen(m_request.screen);
        if (region) {
            const qreal dpr = m_request.screen->devicePixelRatio();
            const QRect logical = m_request.region.normalized();
            m_physicalCrop = evenRect(QRect(qRound(logical.x() * dpr), qRound(logical.y() * dpr),
                                            qRound(logical.width() * dpr),
                                            qRound(logical.height() * dpr)));
            m_recorder->setVideoResolution(fitOutputSize(m_physicalCrop.size()));
        }
        m_source->start();
    }

    setState(State::Recording);
    m_recorder->record();
}

bool RecordingController::prepareOutputPath(QString *error)
{
    QDir dir(m_request.outputDirectory);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        *error = tr("无法创建保存目录：%1").arg(m_request.outputDirectory);
        return false;
    }

    const QFileInfo info(dir.absolutePath());
    if (!info.isDir() || !info.isWritable()) {
        *error = tr("保存目录不可写：%1").arg(dir.absolutePath());
        return false;
    }

    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HHmmss"));
    m_pendingPath = dir.filePath(QStringLiteral("ScreenRec_%1.mp4").arg(stamp));
    return true;
}

void RecordingController::cleanupCapture()
{
    m_source->stop();
    m_windowCapture->stop();
    m_systemAudio->stop();
    m_session->setVideoSink(nullptr);
    m_encodeSession->setVideoFrameInput(nullptr);
    m_session->setAudioInput(nullptr);
    m_encodeSession->setAudioInput(nullptr);
    m_session->setAudioBufferInput(nullptr);
    m_encodeSession->setAudioBufferInput(nullptr);
}

void RecordingController::fail(const QString &message)
{
    if (m_handlingError)
        return;
    m_handlingError = true;

    m_countdownTimer->stop();
    cleanupCapture();

    if (m_recorder->recorderState() != QMediaRecorder::StoppedState)
        m_recorder->stop();

    if (m_state != State::Idle)
        setState(State::Idle);

    emit errorOccurred(message);
    m_handlingError = false;
}

void RecordingController::onRecorderStateChanged()
{
    switch (m_recorder->recorderState()) {
    case QMediaRecorder::RecordingState:
        if (m_state != State::Recording)
            setState(State::Recording);
        break;
    case QMediaRecorder::PausedState:
        if (m_state != State::Paused)
            setState(State::Paused);
        break;
    case QMediaRecorder::StoppedState: {
        cleanupCapture();
        const QString path = m_actualPath.isEmpty() ? m_pendingPath : m_actualPath;
        const bool wasBusy = (m_state != State::Idle);
        setState(State::Idle);
        if (!wasBusy)
            break;
        if (m_handlingError)
            break;
        if (!path.isEmpty() && QFileInfo::exists(path) && QFileInfo(path).size() > 0)
            emit recordingFinished(path);
        else if (wasBusy && !path.isEmpty())
            emit errorOccurred(tr("录制已结束，但没有生成有效文件。"));
        break;
    }
    }
}

void RecordingController::onCountdownTimeout()
{
    if (m_state != State::Countdown)
        return;

    --m_countdownRemaining;
    emit countdownTick(m_countdownRemaining);
    if (m_countdownRemaining > 0)
        return;

    m_countdownTimer->stop();
    beginCapture();
}

QRect RecordingController::mappedCropRect(const QSize &frameSize) const
{
    if (!m_request.screen)
        return evenRect(m_physicalCrop.intersected(QRect(QPoint(0, 0), frameSize)));

    const qreal dpr = m_request.screen->devicePixelRatio();
    const QSize screenPhysical(qRound(m_request.screen->size().width() * dpr),
                               qRound(m_request.screen->size().height() * dpr));
    QRect crop = m_physicalCrop;
    if (screenPhysical.width() > 0 && screenPhysical.height() > 0 && frameSize != screenPhysical) {
        const qreal sx = double(frameSize.width()) / double(screenPhysical.width());
        const qreal sy = double(frameSize.height()) / double(screenPhysical.height());
        crop = QRect(qRound(m_physicalCrop.x() * sx), qRound(m_physicalCrop.y() * sy),
                     qRound(m_physicalCrop.width() * sx), qRound(m_physicalCrop.height() * sy));
    }
    return evenRect(crop.intersected(QRect(QPoint(0, 0), frameSize)));
}

QSize RecordingController::fitOutputSize(QSize source) const
{
    source = evenSize(source);
    if (!m_request.maxResolution.isValid() || m_request.maxResolution.width() <= 0)
        return source;
    if (source.width() <= m_request.maxResolution.width()
        && source.height() <= m_request.maxResolution.height())
        return source;
    source.scale(m_request.maxResolution, Qt::KeepAspectRatio);
    return evenSize(source);
}

void RecordingController::onSystemPcm(const QByteArray &pcm)
{
    if (m_state != State::Recording || pcm.isEmpty())
        return;
    const QAudioFormat format = SystemAudioCapture::mixFormat();
    if (m_audioUs < 0)
        m_audioUs = 0;
    const QAudioBuffer buffer(pcm, format, m_audioUs);
    m_audioUs += format.durationForBytes(pcm.size());
    m_audioBufferInput->sendAudioBuffer(buffer);
}

void RecordingController::onVideoFrame(const QVideoFrame &frame)
{
    if (m_state != State::Recording || !m_processFrames)
        return;
    if (!frame.isValid())
        return;

    if (m_request.frameRate > 0) {
        const qint64 minUs = 1000000 / m_request.frameRate;
        const qint64 now = m_videoClock.nsecsElapsed() / 1000;
        if (m_lastVideoUs >= 0 && now - m_lastVideoUs < minUs)
            return;
        m_lastVideoUs = now;
    }

    QImage image = frame.toImage();
    if (image.isNull())
        return;

    if (m_request.mode == CaptureMode::Region) {
        const QRect crop = mappedCropRect(frame.size());
        if (crop.width() < 16 || crop.height() < 16)
            return;
        image = image.copy(crop);
    }

    if (image.format() != QImage::Format_ARGB32 && image.format() != QImage::Format_RGB32)
        image = image.convertToFormat(QImage::Format_ARGB32);
    const QSize target = fitOutputSize(image.size());
    if (target != image.size())
        image = image.scaled(target, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    if (image.isNull())
        return;

    QVideoFrame output(image);
    output.setStartTime(frame.startTime());
    output.setEndTime(frame.endTime());
    output.setStreamFrameRate(m_request.frameRate > 0 ? m_request.frameRate : frame.streamFrameRate());
    m_frameInput->sendVideoFrame(output);
}
