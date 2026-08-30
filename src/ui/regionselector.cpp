#include "regionselector.h"
#include "captureexclude.h"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QShowEvent>

namespace {
constexpr int kHandle = 10;
constexpr int kMinSize = 16;
} // namespace

RegionSelector::RegionSelector(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::StrongFocus);

    m_toolbar = new QWidget(this);
    m_toolbar->setObjectName(QStringLiteral("regionToolbar"));
    m_toolbar->setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QHBoxLayout(m_toolbar);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(8);
    m_okButton = new QPushButton(tr("确认"), m_toolbar);
    m_cancelButton = new QPushButton(tr("取消"), m_toolbar);
    m_okButton->setObjectName(QStringLiteral("okBtn"));
    m_cancelButton->setObjectName(QStringLiteral("cancelBtn"));
    m_okButton->setCursor(Qt::PointingHandCursor);
    m_cancelButton->setCursor(Qt::PointingHandCursor);
    m_okButton->setDefault(true);
    layout->addWidget(m_okButton);
    layout->addWidget(m_cancelButton);
    m_toolbar->setStyleSheet(QStringLiteral(
        "#regionToolbar { background: rgba(22, 24, 28, 230); border-radius: 12px; }"
        "QPushButton { border: none; min-height: 30px; padding: 4px 16px;"
        " border-radius: 8px; font-weight: 600; }"
        "QPushButton#okBtn { background: #E11D2E; color: white; }"
        "QPushButton#okBtn:hover { background: #C91828; }"
        "QPushButton#cancelBtn { background: #3A3D44; color: white; }"
        "QPushButton#cancelBtn:hover { background: #4A4E56; }"));
    m_toolbar->adjustSize();
    m_toolbar->hide();

    connect(m_okButton, &QPushButton::clicked, this, &RegionSelector::confirm);
    connect(m_cancelButton, &QPushButton::clicked, this, &RegionSelector::cancel);
}

void RegionSelector::start(QScreen *screen)
{
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    m_rect = QRect();
    m_dragging = false;
    m_activeHandle = Handle::None;
    m_toolbar->hide();
    setGeometry(screen->geometry());
    show();
    raise();
    activateWindow();
    setFocus(Qt::ActiveWindowFocusReason);
    grabKeyboard();
}

void RegionSelector::dismiss()
{
    releaseKeyboard();
    hide();
    m_toolbar->hide();
}

void RegionSelector::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    excludeWidgetFromCapture(this);
}

void RegionSelector::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const QRect full = rect();
    const QRect r = normalizedRect();
    const QColor mask(0, 0, 0, 140);

    if (r.width() < 2 || r.height() < 2) {
        p.fillRect(full, mask);
        const QString hint = tr("拖拽选择区域    Enter 确认    Esc 取消");
        QFont hintFont = p.font();
        hintFont.setPixelSize(13);
        p.setFont(hintFont);
        const QFontMetrics fm(hintFont);
        const int w = fm.horizontalAdvance(hint) + 28;
        const QRect badge((full.width() - w) / 2, 28, w, 32);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(22, 24, 28, 210));
        p.drawRoundedRect(badge, 16, 16);
        p.setPen(Qt::white);
        p.drawText(badge, Qt::AlignCenter, hint);
        return;
    }

    p.fillRect(QRect(full.left(), full.top(), full.width(), qMax(0, r.top() - full.top())), mask);
    p.fillRect(QRect(full.left(), r.bottom() + 1, full.width(),
                     qMax(0, full.bottom() - r.bottom())),
               mask);
    p.fillRect(QRect(full.left(), r.top(), qMax(0, r.left() - full.left()), r.height()), mask);
    p.fillRect(QRect(r.right() + 1, r.top(), qMax(0, full.right() - r.right()), r.height()), mask);

    p.setPen(QPen(QColor(77, 163, 255), 2));
    p.setBrush(Qt::NoBrush);
    p.drawRect(r.adjusted(0, 0, -1, -1));

    p.setBrush(QColor(77, 163, 255));
    p.setPen(QPen(Qt::white, 1));
    const Handle handles[] = {Handle::Left,       Handle::Right,      Handle::Top,
                              Handle::Bottom,     Handle::TopLeft,    Handle::TopRight,
                              Handle::BottomLeft, Handle::BottomRight};
    for (Handle h : handles)
        p.drawEllipse(handleRect(h));

    const QString label = QStringLiteral("%1 × %2").arg(r.width()).arg(r.height());
    QFont badgeFont = p.font();
    badgeFont.setPixelSize(12);
    badgeFont.setBold(true);
    p.setFont(badgeFont);
    const QFontMetrics fm(badgeFont);
    const int bw = fm.horizontalAdvance(label) + 16;
    const int bh = fm.height() + 8;
    QRect textRect(r.left(), r.top() - bh - 8, bw, bh);
    if (textRect.top() < 8)
        textRect.moveTop(r.top() + 8);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(22, 24, 28, 210));
    p.drawRoundedRect(textRect, 8, 8);
    p.setPen(Qt::white);
    p.drawText(textRect, Qt::AlignCenter, label);
}

void RegionSelector::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    const QPoint pos = event->pos();
    const Handle handle = hitTest(pos);
    m_dragOrigin = pos;
    m_rectAtPress = normalizedRect();

    if (handle == Handle::None) {
        m_rect = QRect(pos, pos);
        m_activeHandle = Handle::BottomRight;
        m_dragging = true;
        m_toolbar->hide();
        setCursor(Qt::CrossCursor);
    } else {
        m_activeHandle = handle;
        m_dragging = true;
        setCursor(cursorFor(handle));
    }
}

void RegionSelector::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint pos = event->pos();
    if (!m_dragging) {
        setCursor(cursorFor(hitTest(pos)));
        return;
    }

    QRect r = m_rectAtPress;
    const QPoint delta = pos - m_dragOrigin;

    switch (m_activeHandle) {
    case Handle::Body:
        r.translate(delta);
        break;
    case Handle::Left:
        r.setLeft(m_rectAtPress.left() + delta.x());
        break;
    case Handle::Right:
        r.setRight(m_rectAtPress.right() + delta.x());
        break;
    case Handle::Top:
        r.setTop(m_rectAtPress.top() + delta.y());
        break;
    case Handle::Bottom:
        r.setBottom(m_rectAtPress.bottom() + delta.y());
        break;
    case Handle::TopLeft:
        r.setTopLeft(m_rectAtPress.topLeft() + delta);
        break;
    case Handle::TopRight:
        r.setTopRight(m_rectAtPress.topRight() + delta);
        break;
    case Handle::BottomLeft:
        r.setBottomLeft(m_rectAtPress.bottomLeft() + delta);
        break;
    case Handle::BottomRight:
        r.setBottomRight(m_rectAtPress.bottomRight() + delta);
        break;
    case Handle::None:
        break;
    }

    r = r.normalized();
    if (r.width() < kMinSize)
        r.setWidth(kMinSize);
    if (r.height() < kMinSize)
        r.setHeight(kMinSize);
    r = r.intersected(rect());
    m_rect = r;
    update();
}

void RegionSelector::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_dragging)
        return;
    m_dragging = false;
    m_activeHandle = Handle::None;
    m_rect = normalizedRect();
    updateToolbar();
    update();
}

void RegionSelector::mouseDoubleClickEvent(QMouseEvent *)
{
    confirm();
}

void RegionSelector::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        cancel();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        confirm();
        return;
    }
    QWidget::keyPressEvent(event);
}

RegionSelector::Handle RegionSelector::hitTest(const QPoint &pos) const
{
    const QRect r = normalizedRect();
    if (r.width() < 2 || r.height() < 2)
        return Handle::None;

    const Handle handles[] = {Handle::TopLeft, Handle::TopRight, Handle::BottomLeft,
                              Handle::BottomRight, Handle::Left, Handle::Right, Handle::Top,
                              Handle::Bottom};
    for (Handle h : handles) {
        if (handleRect(h).adjusted(-2, -2, 2, 2).contains(pos))
            return h;
    }
    if (r.contains(pos))
        return Handle::Body;
    return Handle::None;
}

QRect RegionSelector::handleRect(Handle handle) const
{
    const QRect r = normalizedRect();
    const QPoint c = r.center();
    QPoint p;
    switch (handle) {
    case Handle::Left:
        p = QPoint(r.left(), c.y());
        break;
    case Handle::Right:
        p = QPoint(r.right(), c.y());
        break;
    case Handle::Top:
        p = QPoint(c.x(), r.top());
        break;
    case Handle::Bottom:
        p = QPoint(c.x(), r.bottom());
        break;
    case Handle::TopLeft:
        p = r.topLeft();
        break;
    case Handle::TopRight:
        p = r.topRight();
        break;
    case Handle::BottomLeft:
        p = r.bottomLeft();
        break;
    case Handle::BottomRight:
        p = r.bottomRight();
        break;
    default:
        return {};
    }
    return QRect(p.x() - kHandle / 2, p.y() - kHandle / 2, kHandle, kHandle);
}

QRect RegionSelector::normalizedRect() const
{
    return m_rect.normalized();
}

QCursor RegionSelector::cursorFor(Handle handle) const
{
    switch (handle) {
    case Handle::Left:
    case Handle::Right:
        return Qt::SizeHorCursor;
    case Handle::Top:
    case Handle::Bottom:
        return Qt::SizeVerCursor;
    case Handle::TopLeft:
    case Handle::BottomRight:
        return Qt::SizeFDiagCursor;
    case Handle::TopRight:
    case Handle::BottomLeft:
        return Qt::SizeBDiagCursor;
    case Handle::Body:
        return Qt::SizeAllCursor;
    case Handle::None:
        break;
    }
    return Qt::CrossCursor;
}

void RegionSelector::updateToolbar()
{
    const QRect r = normalizedRect();
    if (r.width() < kMinSize || r.height() < kMinSize) {
        m_toolbar->hide();
        return;
    }

    m_toolbar->adjustSize();
    QPoint pos(r.center().x() - m_toolbar->width() / 2, r.bottom() + 10);
    if (pos.y() + m_toolbar->height() > height() - 8)
        pos.setY(r.top() - m_toolbar->height() - 10);
    if (pos.y() < 8)
        pos.setY(8);
    if (pos.x() < 8)
        pos.setX(8);
    if (pos.x() + m_toolbar->width() > width() - 8)
        pos.setX(width() - m_toolbar->width() - 8);
    m_toolbar->move(pos);
    m_toolbar->show();
    m_toolbar->raise();
}

void RegionSelector::confirm()
{
    const QRect r = normalizedRect();
    if (r.width() < kMinSize || r.height() < kMinSize)
        return;
    dismiss();
    emit selected(r);
}

void RegionSelector::cancel()
{
    dismiss();
    emit cancelled();
}
