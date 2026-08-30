#include "hotkeymanager.h"

#include <QCoreApplication>
#include <QKeyCombination>
#include <QKeySequence>

#ifdef Q_OS_WIN
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#endif

HotkeyManager::HotkeyManager(QObject *parent)
    : QObject(parent)
{
}

HotkeyManager::~HotkeyManager()
{
    unregisterAll();
    if (m_filterInstalled && qApp)
        qApp->removeNativeEventFilter(this);
}

void HotkeyManager::setNativeHandle(quintptr hwnd)
{
    m_hwnd = hwnd;
}

bool HotkeyManager::setEnabled(bool enabled, const QKeySequence &start, const QKeySequence &stop,
                               const QKeySequence &pause, QString *error)
{
    unregisterAll();
    m_enabled = false;

    if (!enabled)
        return true;

#ifndef Q_OS_WIN
    if (error)
        *error = tr("当前平台暂不支持全局热键。");
    return false;
#else
    if (!m_filterInstalled) {
        qApp->installNativeEventFilter(this);
        m_filterInstalled = true;
    }

    if (!registerOne(Start, start, error) || !registerOne(Stop, stop, error)
        || !registerOne(Pause, pause, error)) {
        unregisterAll();
        return false;
    }

    m_enabled = true;
    return true;
#endif
}

void HotkeyManager::unregisterAll()
{
#ifdef Q_OS_WIN
    if (!m_hwnd)
        return;
    const HWND hwnd = reinterpret_cast<HWND>(m_hwnd);
    UnregisterHotKey(hwnd, Start);
    UnregisterHotKey(hwnd, Stop);
    UnregisterHotKey(hwnd, Pause);
#else
    Q_UNUSED(this);
#endif
}

bool HotkeyManager::registerOne(int id, const QKeySequence &seq, QString *error)
{
#ifdef Q_OS_WIN
    if (seq.isEmpty()) {
        if (error)
            *error = tr("热键不能为空。");
        return false;
    }

    const QKeyCombination combo = seq[0];
    const Qt::KeyboardModifiers mods = combo.keyboardModifiers();
    const int key = combo.key();

    UINT nativeMods = 0;
    if (mods & Qt::ControlModifier)
        nativeMods |= MOD_CONTROL;
    if (mods & Qt::ShiftModifier)
        nativeMods |= MOD_SHIFT;
    if (mods & Qt::AltModifier)
        nativeMods |= MOD_ALT;
    if (mods & Qt::MetaModifier)
        nativeMods |= MOD_WIN;
    nativeMods |= MOD_NOREPEAT;

    UINT vk = 0;
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        vk = UINT(key);
    else if (key >= Qt::Key_0 && key <= Qt::Key_9)
        vk = UINT(key);
    else if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        vk = UINT(VK_F1 + (key - Qt::Key_F1));
    else {
        if (error)
            *error = tr("不支持的热键：%1").arg(seq.toString());
        return false;
    }

    const HWND hwnd = reinterpret_cast<HWND>(m_hwnd);
    if (!RegisterHotKey(hwnd, id, nativeMods, vk)) {
        if (error)
            *error = tr("无法注册热键 %1，可能已被占用。").arg(seq.toString(QKeySequence::NativeText));
        return false;
    }
    return true;
#else
    Q_UNUSED(id);
    Q_UNUSED(seq);
    Q_UNUSED(error);
    return false;
#endif
}

bool HotkeyManager::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(result);
#ifdef Q_OS_WIN
    if (!m_enabled)
        return false;
    if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG")
        return false;
    const MSG *msg = static_cast<MSG *>(message);
    if (msg->message == WM_HOTKEY) {
        emit hotkeyPressed(int(msg->wParam));
        return true;
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
#endif
    return false;
}
