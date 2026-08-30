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
    const int mode = m_settings.value(QStringLiteral("captureMode"), 0).toInt();
    return (mode == 1) ? 1 : 0;
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

QSize AppSettings::windowSize() const
{
    return m_settings.value(QStringLiteral("windowSize")).toSize();
}

void AppSettings::setWindowSize(const QSize &size)
{
    m_settings.setValue(QStringLiteral("windowSize"), size);
}

bool AppSettings::windowMaximized() const
{
    return m_settings.value(QStringLiteral("windowMaximized"), false).toBool();
}

void AppSettings::setWindowMaximized(bool maximized)
{
    m_settings.setValue(QStringLiteral("windowMaximized"), maximized);
}

QByteArray AppSettings::windowGeometry() const
{
    return m_settings.value(QStringLiteral("windowGeometry")).toByteArray();
}

void AppSettings::setWindowGeometry(const QByteArray &geometry)
{
    m_settings.setValue(QStringLiteral("windowGeometry"), geometry);
}

bool AppSettings::sectionExpanded(const QString &id, bool fallback) const
{
    return m_settings.value(QStringLiteral("fold/%1").arg(id), fallback).toBool();
}

void AppSettings::setSectionExpanded(const QString &id, bool expanded)
{
    m_settings.setValue(QStringLiteral("fold/%1").arg(id), expanded);
}

QPoint AppSettings::floatingBarPos() const
{
    return m_settings.value(QStringLiteral("floatingBarPos"), QPoint(-1, -1)).toPoint();
}

void AppSettings::setFloatingBarPos(const QPoint &pos)
{
    m_settings.setValue(QStringLiteral("floatingBarPos"), pos);
}

QString AppSettings::lastSavedPath() const
{
    return m_settings.value(QStringLiteral("lastSavedPath")).toString();
}

void AppSettings::setLastSavedPath(const QString &path)
{
    m_settings.setValue(QStringLiteral("lastSavedPath"), path);
}

qint64 AppSettings::lastDurationMs() const
{
    return m_settings.value(QStringLiteral("lastDurationMs"), 0).toLongLong();
}

void AppSettings::setLastDurationMs(qint64 ms)
{
    m_settings.setValue(QStringLiteral("lastDurationMs"), ms);
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

QKeySequence AppSettings::defaultStartHotkey()
{
    return QKeySequence(QStringLiteral("Ctrl+Shift+R"));
}

QKeySequence AppSettings::defaultStopHotkey()
{
    return QKeySequence(QStringLiteral("Ctrl+Shift+S"));
}

QKeySequence AppSettings::defaultPauseHotkey()
{
    return QKeySequence(QStringLiteral("Ctrl+Shift+P"));
}

QKeySequence AppSettings::startHotkey() const
{
    return keySequence(QStringLiteral("startHotkey"), defaultStartHotkey());
}

void AppSettings::setStartHotkey(const QKeySequence &seq)
{
    setKeySequence(QStringLiteral("startHotkey"), seq);
}

QKeySequence AppSettings::stopHotkey() const
{
    return keySequence(QStringLiteral("stopHotkey"), defaultStopHotkey());
}

void AppSettings::setStopHotkey(const QKeySequence &seq)
{
    setKeySequence(QStringLiteral("stopHotkey"), seq);
}

QKeySequence AppSettings::pauseHotkey() const
{
    return keySequence(QStringLiteral("pauseHotkey"), defaultPauseHotkey());
}

void AppSettings::setPauseHotkey(const QKeySequence &seq)
{
    setKeySequence(QStringLiteral("pauseHotkey"), seq);
}

void AppSettings::sync()
{
    m_settings.sync();
}
