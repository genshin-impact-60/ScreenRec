#include "previewwidget.h"

#include <QEnterEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

PreviewWidget::PreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_Hover);
}

void PreviewWidget::setFrame(const QPixmap &pixmap)
{
    m_pixmap = pixmap;
    m_title.clear();
    m_hint.clear();
    update();
}

void PreviewWidget::setPlaceholder(const QString &title, const QString &hint)
{
    m_pixmap = QPixmap();
    m_title = title;
    m_hint = hint;
    m_badge.clear();
    update();
}

void PreviewWidget::setBadge(const QString &badge)
{
    m_badge = badge;
    update();
}

void PreviewWidget::setClickable(bool clickable)
{
    m_clickable = clickable;
    setCursor(clickable ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void PreviewWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath clip;
    clip.addRoundedRect(bounds, 12, 12);
    p.setClipPath(clip);
    p.fillPath(clip, QColor(QStringLiteral("#111318")));

    if (m_clickable && m_hovered)
        p.fillPath(clip, QColor(255, 255, 255, 18));

    if (!m_pixmap.isNull()) {
        const QSize scaled = m_pixmap.size().scaled(size(), Qt::KeepAspectRatio);
        const QRect target((width() - scaled.width()) / 2,
                           (height() - scaled.height()) / 2,
                           scaled.width(),
                           scaled.height());
        p.drawPixmap(target, m_pixmap);
    } else {
        QFont titleFont = font();
        titleFont.setPixelSize(15);
        titleFont.setWeight(QFont::DemiBold);
        QFont hintFont = font();
        hintFont.setPixelSize(12);

        const int textWidth = qMax(40, width() - 32);
        const QRect titleBound = QFontMetrics(titleFont).boundingRect(
            0, 0, textWidth, 200, Qt::AlignCenter | Qt::TextWordWrap, m_title);
        const QRect hintBound = m_hint.isEmpty()
            ? QRect()
            : QFontMetrics(hintFont).boundingRect(
                  0, 0, textWidth, 80, Qt::AlignCenter | Qt::TextWordWrap, m_hint);
        const int blockH = titleBound.height() + (m_hint.isEmpty() ? 0 : 8 + hintBound.height());
        int y = (height() - blockH) / 2;

        p.setFont(titleFont);
        p.setPen(QColor(QStringLiteral("#F3F4F6")));
        p.drawText(QRect(16, y, width() - 32, titleBound.height()),
                   Qt::AlignCenter | Qt::TextWordWrap, m_title);
        if (!m_hint.isEmpty()) {
            y += titleBound.height() + 8;
            p.setFont(hintFont);
            p.setPen(QColor(QStringLiteral("#9AA0A6")));
            p.drawText(QRect(16, y, width() - 32, hintBound.height()),
                       Qt::AlignCenter | Qt::TextWordWrap, m_hint);
        }
    }

    if (!m_badge.isEmpty()) {
        QFont badgeFont = font();
        badgeFont.setPixelSize(11);
        badgeFont.setWeight(QFont::DemiBold);
        p.setFont(badgeFont);
        const QFontMetrics fm(badgeFont);
        const int w = fm.horizontalAdvance(m_badge) + 16;
        const int h = fm.height() + 8;
        const QRectF badge(12, 12, w, h);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 170));
        p.drawRoundedRect(badge, 8, 8);
        p.setPen(Qt::white);
        p.drawText(badge, Qt::AlignCenter, m_badge);
    }

    p.setClipping(false);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 28), 1));
    p.drawRoundedRect(bounds, 12, 12);
}

void PreviewWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_clickable && event->button() == Qt::LeftButton && rect().contains(event->pos()))
        emit clicked();
}

void PreviewWidget::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    m_hovered = true;
    update();
}

void PreviewWidget::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_hovered = false;
    update();
}
