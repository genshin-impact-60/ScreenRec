#include "settings.h"

#include <QDir>
#include <QStandardPaths>

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
    , m_settings(QStringLiteral("ScreenRec"), QStringLiteral("ScreenRec"))
{
}

QString AppSettings::defaultOutputDirectory()
{
    const QString movies = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    return QDir(movies).filePath(QStringLiteral("ScreenRec"));
}

QString AppSettings::outputDirectory() const
{
    return m_settings.value(QStringLiteral("outputDirectory"), defaultOutputDirectory()).toString();
}

void AppSettings::setOutputDirectory(const QString &dir)
{
    m_settings.setValue(QStringLiteral("outputDirectory"), dir);
}

bool AppSettings::recordMicrophone() const
{
    return m_settings.value(QStringLiteral("recordMicrophone"), true).toBool();
}

void AppSettings::setRecordMicrophone(bool enabled)
{
    m_settings.setValue(QStringLiteral("recordMicrophone"), enabled);
}

bool AppSettings::recordSystemAudio() const
{
    return m_settings.value(QStringLiteral("recordSystemAudio"), true).toBool();
}

void AppSettings::setRecordSystemAudio(bool enabled)
{
    m_settings.setValue(QStringLiteral("recordSystemAudio"), enabled);
}

int AppSettings::frameRate() const
{
    return m_settings.value(QStringLiteral("frameRate"), 30).toInt();
}

void AppSettings::setFrameRate(int fps)
{
    m_settings.setValue(QStringLiteral("frameRate"), fps);
}

int AppSettings::quality() const
{
    return m_settings.value(QStringLiteral("quality"), 3).toInt();
}

void AppSettings::setQuality(int quality)
{
    m_settings.setValue(QStringLiteral("quality"), quality);
}

QSize AppSettings::maxResolution() const
{
    return m_settings.value(QStringLiteral("maxResolution"), QSize()).toSize();
}

void AppSettings::setMaxResolution(const QSize &size)
{
    m_settings.setValue(QStringLiteral("maxResolution"), size);
}

QString AppSettings::lastScreenName() const
{
    return m_settings.value(QStringLiteral("lastScreenName")).toString();
}

void AppSettings::setLastScreenName(const QString &name)
{
    m_settings.setValue(QStringLiteral("lastScreenName"), name);
}

int AppSettings::countdownSeconds() const
{
    return m_settings.value(QStringLiteral("countdownSeconds"), 3).toInt();
}

void AppSettings::setCountdownSeconds(int seconds)
{
    m_settings.setValue(QStringLiteral("countdownSeconds"), seconds);
}

int AppSettings::captureMode() const
{
    return m_settings.value(QStringLiteral("captureMode"), 0).toInt();
}

void AppSettings::setCaptureMode(int mode)
{
    m_settings.setValue(QStringLiteral("captureMode"), mode);
}

QRect AppSettings::region() const
{
    return m_settings.value(QStringLiteral("region")).toRect();
}

void AppSettings::setRegion(const QRect &region)
{
    m_settings.setValue(QStringLiteral("region"), region);
}

QString AppSettings::lastWindowDescription() const
{
    return m_settings.value(QStringLiteral("lastWindowDescription")).toString();
}

void AppSettings::setLastWindowDescription(const QString &description)
{
    m_settings.setValue(QStringLiteral("lastWindowDescription"), description);
}

bool AppSettings::hotkeysEnabled() const
{
    return m_settings.value(QStringLiteral("hotkeysEnabled"), true).toBool();
}

void AppSettings::setHotkeysEnabled(bool enabled)
{
    m_settings.setValue(QStringLiteral("hotkeysEnabled"), enabled);
}

QKeySequence AppSettings::keySequence(const QString &key, const QKeySequence &fallback) const
{
    const QString stored = m_settings.value(key).toString();
    if (stored.isEmpty())
        return fallback;
    return QKeySequence(stored, QKeySequence::PortableText);
}

void AppSettings::setKeySequence(const QString &key, const QKeySequence &seq)
{
    m_settings.setValue(key, seq.toString(QKeySequence::PortableText));
}

QKeySequence AppSettings::startHotkey() const
{
    return keySequence(QStringLiteral("startHotkey"), QKeySequence(QStringLiteral("Ctrl+Shift+R")));
}

void AppSettings::setStartHotkey(const QKeySequence &seq)
{
    setKeySequence(QStringLiteral("startHotkey"), seq);
}

QKeySequence AppSettings::stopHotkey() const
{
    return keySequence(QStringLiteral("stopHotkey"), QKeySequence(QStringLiteral("Ctrl+Shift+S")));
}

void AppSettings::setStopHotkey(const QKeySequence &seq)
{
    setKeySequence(QStringLiteral("stopHotkey"), seq);
}

QKeySequence AppSettings::pauseHotkey() const
{
    return keySequence(QStringLiteral("pauseHotkey"), QKeySequence(QStringLiteral("Ctrl+Shift+P")));
}

void AppSettings::setPauseHotkey(const QKeySequence &seq)
{
    setKeySequence(QStringLiteral("pauseHotkey"), seq);
}
