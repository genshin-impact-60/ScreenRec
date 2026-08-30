#ifndef HOTKEYMANAGER_H
#define HOTKEYMANAGER_H

#include <QAbstractNativeEventFilter>
#include <QKeySequence>
#include <QObject>

class HotkeyManager : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    enum Id {
        Start = 1,
        Stop = 2,
        Pause = 3
    };

    explicit HotkeyManager(QObject *parent = nullptr);
    ~HotkeyManager() override;

    void setNativeHandle(quintptr hwnd);
    bool setEnabled(bool enabled, const QKeySequence &start, const QKeySequence &stop,
                    const QKeySequence &pause, QString *error = nullptr);

signals:
    void hotkeyPressed(int id);

protected:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    void unregisterAll();
    bool registerOne(int id, const QKeySequence &seq, QString *error);

    quintptr m_hwnd = 0;
    bool m_enabled = false;
    bool m_filterInstalled = false;
};

#endif
