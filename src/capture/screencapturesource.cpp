#include "screencapturesource.h"

#include <QScreenCapture>

ScreenCaptureSource::ScreenCaptureSource(QObject *parent)
    : QObject(parent)
    , m_capture(new QScreenCapture(this))
{
    connect(m_capture, &QScreenCapture::errorOccurred, this,
            [this](QScreenCapture::Error error, const QString &errorString) {
                if (error == QScreenCapture::NoError)
                    return;
                emit errorOccurred(errorString.isEmpty()
                                       ? tr("屏幕采集失败。")
                                       : errorString);
            },
            Qt::QueuedConnection);
}

QScreenCapture *ScreenCaptureSource::qtCapture() const
{
    return m_capture;
}

void ScreenCaptureSource::setScreen(QScreen *screen)
{
    m_capture->setScreen(screen);
}

QScreen *ScreenCaptureSource::screen() const
{
    return m_capture->screen();
}

void ScreenCaptureSource::start()
{
    m_capture->setActive(true);
}

void ScreenCaptureSource::stop()
{
    m_capture->setActive(false);
}

bool ScreenCaptureSource::isActive() const
{
    return m_capture->isActive();
}

QString ScreenCaptureSource::errorString() const
{
    return m_capture->errorString();
}
