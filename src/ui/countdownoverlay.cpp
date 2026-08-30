#include "countdownoverlay.h"
#include "captureexclude.h"

#include <QPainter>
#include <QScreen>
#include <QShowEvent>

CountdownOverlay::CountdownOverlay(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    resize(220, 220);
}

void CountdownOverlay::showOnScreen(QScreen *screen, int seconds)
{
    if (!screen)
        return;

    const QRect sg = screen->geometry();
    move(sg.center() - QPoint(width() / 2, height() / 2));
    setRemaining(seconds);
    show();
    raise();
}

void CountdownOverlay::setRemaining(int seconds)
{
    m_remaining = seconds;
    update();
}

void CountdownOverlay::dismiss()
{
    hide();
}

void CountdownOverlay::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    excludeWidgetFromCapture(this);
}

void CountdownOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF circle = QRectF(rect()).adjusted(12, 12, -12, -12);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 170));
    p.drawEllipse(circle);

    p.setPen(QColor(255, 255, 255));
    QFont font = p.font();
    font.setPixelSize(96);
    font.setBold(true);
    p.setFont(font);
    p.drawText(rect(), Qt::AlignCenter, QString::number(m_remaining));
}
