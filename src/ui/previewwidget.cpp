#include "previewwidget.h"
#include "appstyle.h"

#include <QEnterEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QToolButton>
#include <QVBoxLayout>

PreviewWidget::PreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_Hover);

    m_settingsButton = new QToolButton(this);
    m_settingsButton->setObjectName(QStringLiteral("previewIconButton"));
    m_settingsButton->setIcon(AppIcons::settings(QColor(QStringLiteral("#F3F4F6"))));
    m_settingsButton->setIconSize(QSize(18, 18));
    m_settingsButton->setCursor(Qt::PointingHandCursor);
    m_settingsButton->setToolTip(tr("设置"));
    m_settingsButton->setFixedSize(32, 32);
    connect(m_settingsButton, &QToolButton::clicked, this, &PreviewWidget::settingsClicked);

    m_result = new QWidget(this);
    m_result->setObjectName(QStringLiteral("resultCard"));
    m_result->setAttribute(Qt::WA_StyledBackground, true);
    auto *root = new QVBoxLayout(m_result);
    root->setContentsMargins(14, 12, 14, 12);
    root->setSpacing(8);

    m_resultTitle = new QLabel(m_result);
    m_resultTitle->setObjectName(QStringLiteral("resultTitle"));
    m_resultMeta = new QLabel(m_result);
    m_resultMeta->setObjectName(QStringLiteral("resultMeta"));
    m_resultMeta->setWordWrap(true);

    auto *buttons = new QWidget(m_result);
    auto *row = new QHBoxLayout(buttons);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);
    auto *openFile = new QPushButton(tr("打开文件"), buttons);
    auto *openFolder = new QPushButton(tr("打开目录"), buttons);
    auto *dismiss = new QPushButton(tr("关闭"), buttons);
    openFile->setObjectName(QStringLiteral("resultPrimary"));
    openFolder->setObjectName(QStringLiteral("resultSecondary"));
    dismiss->setObjectName(QStringLiteral("resultSecondary"));
    openFile->setCursor(Qt::PointingHandCursor);
    openFolder->setCursor(Qt::PointingHandCursor);
    dismiss->setCursor(Qt::PointingHandCursor);
    row->addWidget(openFile, 1);
    row->addWidget(openFolder, 1);
    row->addWidget(dismiss);

    root->addWidget(m_resultTitle);
    root->addWidget(m_resultMeta);
    root->addWidget(buttons);
    m_result->hide();

    m_result->setStyleSheet(QStringLiteral(
        "#resultCard { background: rgba(22, 24, 28, 230); border-radius: 12px; }"
        "QLabel#resultTitle { color: #FFFFFF; font-weight: 600; font-size: 13px; background: transparent; }"
        "QLabel#resultMeta { color: #C5CAD1; font-size: 12px; background: transparent; }"
        "QPushButton { border: none; min-height: 30px; padding: 4px 10px;"
        " border-radius: 8px; font-weight: 600; }"
        "QPushButton#resultPrimary { background: #E11D2E; color: white; }"
        "QPushButton#resultPrimary:hover { background: #C91828; }"
        "QPushButton#resultSecondary { background: #3A3D44; color: white; }"
        "QPushButton#resultSecondary:hover { background: #4A4E56; }"));

    connect(openFile, &QPushButton::clicked, this, &PreviewWidget::openFileClicked);
    connect(openFolder, &QPushButton::clicked, this, &PreviewWidget::openFolderClicked);
    connect(dismiss, &QPushButton::clicked, this, [this]() {
        clearResult();
        emit resultDismissed();
    });
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

void PreviewWidget::showResult(const QString &title, const QString &meta)
{
    m_resultTitle->setText(title);
    m_resultMeta->setText(meta);
    m_result->show();
    m_result->raise();
    layoutOverlays();
    update();
}

void PreviewWidget::clearResult()
{
    if (!m_result->isVisible())
        return;
    m_result->hide();
    update();
}

bool PreviewWidget::hasResult() const
{
    return m_result && m_result->isVisible();
}

void PreviewWidget::layoutOverlays()
{
    m_settingsButton->move(width() - m_settingsButton->width() - 10, 10);
    m_settingsButton->raise();
    if (m_result->isVisible()) {
        const int margin = 10;
        const int w = qMax(160, width() - margin * 2);
        const int h = m_result->sizeHint().height();
        m_result->setGeometry(margin, height() - h - margin, w, h);
        m_result->raise();
    }
}

void PreviewWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutOverlays();
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

    if (m_clickable && m_hovered && !hasResult())
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

    if (hasResult())
        p.fillPath(clip, QColor(0, 0, 0, 70));

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
    if (hasResult())
        return;
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
