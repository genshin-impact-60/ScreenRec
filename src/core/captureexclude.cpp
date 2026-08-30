#include "captureexclude.h"

#include <QWidget>

#ifdef Q_OS_WIN
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#    ifndef WDA_EXCLUDEFROMCAPTURE
#        define WDA_EXCLUDEFROMCAPTURE 0x00000011
#    endif
#endif

void excludeWidgetFromCapture(QWidget *widget)
{
#ifdef Q_OS_WIN
    if (!widget)
        return;
    const HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    if (hwnd)
        SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
#else
    Q_UNUSED(widget);
#endif
}
