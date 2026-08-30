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
    m_total = qMax(1, seconds);
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

    const QRectF circle = QRectF(rect()).adjusted(16, 16, -16, -16);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(17, 19, 24, 200));
    p.drawEllipse(circle);

    const QRectF ring = circle.adjusted(8, 8, -8, -8);
    QPen track(QColor(255, 255, 255, 40), 6, Qt::SolidLine, Qt::RoundCap);
    p.setPen(track);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(ring);

    const qreal ratio = m_total > 0 ? qBound(0.0, qreal(m_remaining) / m_total, 1.0) : 0.0;
    if (ratio > 0) {
        QPen arc(QColor(QStringLiteral("#E11D2E")), 6, Qt::SolidLine, Qt::RoundCap);
        p.setPen(arc);
        p.drawArc(ring, 90 * 16, int(ratio * 360 * 16));
    }

    p.setPen(QColor(255, 255, 255));
    QFont font = p.font();
    font.setPixelSize(84);
    font.setBold(true);
    p.setFont(font);
    p.drawText(rect(), Qt::AlignCenter, QString::number(qMax(0, m_remaining)));
}
