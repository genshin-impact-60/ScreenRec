#ifndef SETTINGS_H
#define SETTINGS_H

#include <QKeySequence>
#include <QObject>
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

    QString lastWindowDescription() const;
    void setLastWindowDescription(const QString &description);

    bool hotkeysEnabled() const;
    void setHotkeysEnabled(bool enabled);

    QKeySequence startHotkey() const;
    void setStartHotkey(const QKeySequence &seq);

    QKeySequence stopHotkey() const;
    void setStopHotkey(const QKeySequence &seq);

    QKeySequence pauseHotkey() const;
    void setPauseHotkey(const QKeySequence &seq);

    static QString defaultOutputDirectory();

private:
    QKeySequence keySequence(const QString &key, const QKeySequence &fallback) const;
    void setKeySequence(const QString &key, const QKeySequence &seq);

    QSettings m_settings;
};

#endif
