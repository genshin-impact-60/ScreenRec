#include "countdownoverlay.h"
#include "captureexclude.h"

#include <QEasingCurve>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QShowEvent>
#include <QVariantAnimation>

CountdownOverlay::CountdownOverlay(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
    , m_pulse(new QVariantAnimation(this))
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("点击跳过倒计时，Esc 取消"));
    resize(240, 260);

    m_pulse->setStartValue(0.72);
    m_pulse->setEndValue(1.0);
    m_pulse->setDuration(240);
    m_pulse->setEasingCurve(QEasingCurve::OutBack);
    connect(m_pulse, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_scale = value.toReal();
        update();
    });
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
    grabKeyboard();
}

void CountdownOverlay::setRemaining(int seconds)
{
    const bool changed = m_remaining != seconds;
    m_remaining = seconds;
    if (changed && seconds > 0)
        pulse();
    update();
}

void CountdownOverlay::dismiss()
{
    releaseKeyboard();
    m_pulse->stop();
    hide();
}

void CountdownOverlay::pulse()
{
    m_pulse->stop();
    m_scale = 0.72;
    m_pulse->start();
}

void CountdownOverlay::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    excludeWidgetFromCapture(this);
}

void CountdownOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit skipped();
}

void CountdownOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit cancelled();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter
        || event->key() == Qt::Key_Space) {
        emit skipped();
        return;
    }
    QWidget::keyPressEvent(event);
}

void CountdownOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF circle = QRectF(rect()).adjusted(20, 8, -20, -28);
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

    p.save();
    p.translate(circle.center());
    p.scale(m_scale, m_scale);
    p.translate(-circle.center());
    p.setPen(QColor(255, 255, 255));
    QFont font = p.font();
    font.setPixelSize(84);
    font.setBold(true);
    p.setFont(font);
    p.drawText(circle.toRect(), Qt::AlignCenter, QString::number(qMax(0, m_remaining)));
    p.restore();

    QFont hintFont = p.font();
    hintFont.setPixelSize(12);
    p.setFont(hintFont);
    p.setPen(QColor(255, 255, 255, 180));
    p.drawText(QRect(16, height() - 28, width() - 32, 22), Qt::AlignCenter,
               tr("点击跳过 · Esc 取消"));
}
