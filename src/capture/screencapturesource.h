#ifndef SCREENCAPTURESOURCE_H
#define SCREENCAPTURESOURCE_H

#include <QObject>
#include <QString>

class QScreen;
class QScreenCapture;

class ScreenCaptureSource : public QObject
{
    Q_OBJECT

public:
    explicit ScreenCaptureSource(QObject *parent = nullptr);

    QScreenCapture *qtCapture() const;

    void setScreen(QScreen *screen);
    QScreen *screen() const;

    void start();
    void stop();
    bool isActive() const;

    QString errorString() const;

signals:
    void errorOccurred(const QString &message);

private:
    QScreenCapture *m_capture;
};

#endif
