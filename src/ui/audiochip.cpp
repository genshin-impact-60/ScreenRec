#include "audiochip.h"

#include <QPainter>
#include <QPaintEvent>
#include <QStyle>

AudioChip::AudioChip(const QString &text, QWidget *parent)
    : QCheckBox(text, parent)
{
    setObjectName(QStringLiteral("chipCheck"));
    setAttribute(Qt::WA_StyledBackground, true);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void AudioChip::setIdleTip(const QString &text)
{
    m_baseTip = text;
    if (!m_silent)
        setToolTip(text);
}

void AudioChip::setSilentHint(const QString &text)
{
    m_silentHint = text;
    if (m_silent)
        setToolTip(text);
}

void AudioChip::setLevel(qreal level)
{
    const qreal next = qBound(0.0, level, 1.0);
    m_level = m_level * 0.4 + next * 0.6;

    if (!isChecked()) {
        if (m_silent)
            setSilent(false);
        m_quietSince.invalidate();
        update();
        return;
    }

    if (m_level >= 0.03) {
        m_quietSince.invalidate();
        setSilent(false);
    } else {
        if (!m_quietSince.isValid())
            m_quietSince.start();
        else if (m_quietSince.elapsed() > 1600)
            setSilent(true);
    }
    update();
}

void AudioChip::setSilent(bool silent)
{
    if (m_silent == silent)
        return;
    m_silent = silent;
    setProperty("silent", silent);
    if (style()) {
        style()->unpolish(this);
        style()->polish(this);
    }
    if (silent && !m_silentHint.isEmpty())
        setToolTip(m_silentHint);
    else
        setToolTip(m_baseTip);
    update();
}

void AudioChip::paintEvent(QPaintEvent *event)
{
    QCheckBox::paintEvent(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int bars = 5;
    const int barW = 3;
    const int gap = 2;
    const int totalW = bars * barW + (bars - 1) * gap;
    const int x0 = width() - totalW - 12;
    const int y0 = height() / 2 - 8;
    const int fullH = 16;
    const bool on = isChecked();

    for (int i = 0; i < bars; ++i) {
        const qreal threshold = (i + 1) / qreal(bars);
        const bool lit = on && m_level >= threshold * 0.18 + qreal(i) * 0.14;
        const int h = 6 + i * 2;
        const QRectF bar(x0 + i * (barW + gap), y0 + fullH - h, barW, h);
        QColor color;
        if (!on)
            color = QColor(QStringLiteral("#D0D4DA"));
        else if (m_silent)
            color = lit ? QColor(QStringLiteral("#E6A317")) : QColor(QStringLiteral("#E8D7A8"));
        else if (lit)
            color = QColor(QStringLiteral("#E11D2E"));
        else
            color = QColor(QStringLiteral("#E7B4B9"));
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(bar, 1.2, 1.2);
    }
}
