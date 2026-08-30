#ifndef SETTINGS_H
#define SETTINGS_H

#include <QByteArray>
#include <QKeySequence>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QSettings>
#include <QSize>
#include <QString>

class AppSettings : public QObject
{
    Q_OBJECT

public:
    explicit AppSettings(QObject *parent = nullptr);

    QString outputDirectory() const;
    void setOutputDirectory(const QString &dir);

    bool recordMicrophone() const;
    void setRecordMicrophone(bool enabled);

    bool recordSystemAudio() const;
    void setRecordSystemAudio(bool enabled);

    int frameRate() const;
    void setFrameRate(int fps);

    int quality() const;
    void setQuality(int quality);

    QSize maxResolution() const;
    void setMaxResolution(const QSize &size);

    QString lastScreenName() const;
    void setLastScreenName(const QString &name);

    int countdownSeconds() const;
    void setCountdownSeconds(int seconds);

    int captureMode() const;
    void setCaptureMode(int mode);

    QRect region() const;
    void setRegion(const QRect &region);

    QSize windowSize() const;
    void setWindowSize(const QSize &size);

    bool windowMaximized() const;
    void setWindowMaximized(bool maximized);

    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &geometry);

    bool sectionExpanded(const QString &id, bool fallback = true) const;
    void setSectionExpanded(const QString &id, bool expanded);

    QPoint floatingBarPos() const;
    void setFloatingBarPos(const QPoint &pos);

    QString lastSavedPath() const;
    void setLastSavedPath(const QString &path);

    qint64 lastDurationMs() const;
    void setLastDurationMs(qint64 ms);

    bool hotkeysEnabled() const;

    static QKeySequence defaultStartHotkey();
    static QKeySequence defaultStopHotkey();
    static QKeySequence defaultPauseHotkey();
    void setHotkeysEnabled(bool enabled);

    QKeySequence startHotkey() const;
    void setStartHotkey(const QKeySequence &seq);

    QKeySequence stopHotkey() const;
    void setStopHotkey(const QKeySequence &seq);

    QKeySequence pauseHotkey() const;
    void setPauseHotkey(const QKeySequence &seq);

    static QString defaultOutputDirectory();

    void sync();

private:
    QKeySequence keySequence(const QString &key, const QKeySequence &fallback) const;
    void setKeySequence(const QString &key, const QKeySequence &seq);

    QSettings m_settings;
};

#endif
