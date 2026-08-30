#include "appstyle.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QFont>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPolygonF>
#include <QPixmap>
#include <QProxyStyle>
#include <QScreen>
#include <QStyle>
#include <QStyleFactory>
#include <QTimer>
#include <QWheelEvent>

#include <functional>

namespace {

QIcon paintIcon(const QColor &color, const std::function<void(QPainter &, const QRectF &)> &draw)
{
    QIcon icon;
    const int sizes[] = {16, 20, 24, 32, 40};
    for (int s : sizes) {
        QPixmap pm(s, s);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF r(s * 0.14, s * 0.14, s * 0.72, s * 0.72);
        p.setPen(QPen(color, qMax(1.15, s / 14.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        draw(p, r);
        p.end();
        icon.addPixmap(pm);
    }
    return icon;
}

class AppProxyStyle : public QProxyStyle
{
public:
    explicit AppProxyStyle(QStyle *base)
        : QProxyStyle(base)
    {
    }

    int styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
                  QStyleHintReturn *returnData) const override
    {
        if (hint == QStyle::SH_ComboBox_Popup)
            return 0;
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};

class StyledComboBox : public QComboBox
{
public:
    explicit StyledComboBox(QWidget *parent = nullptr)
        : QComboBox(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setMaxVisibleItems(10);
        if (QAbstractItemView *itemView = view()) {
            itemView->setCursor(Qt::PointingHandCursor);
            itemView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        }
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QComboBox::paintEvent(event);

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QColor color = isEnabled() ? QColor(QStringLiteral("#6B7280"))
                                         : QColor(QStringLiteral("#B0B6BE"));
        p.setPen(QPen(color, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

        const QRectF area(width() - 28, 0, 28, height());
        const QPointF c = area.center();
        const qreal dx = 4.4;
        const qreal dy = 2.2;
        if (m_popupOpen) {
            p.drawPolyline(QPolygonF{
                QPointF(c.x() - dx, c.y() + dy * 0.4),
                QPointF(c.x(), c.y() - dy),
                QPointF(c.x() + dx, c.y() + dy * 0.4),
            });
        } else {
            p.drawPolyline(QPolygonF{
                QPointF(c.x() - dx, c.y() - dy * 0.4),
                QPointF(c.x(), c.y() + dy),
                QPointF(c.x() + dx, c.y() - dy * 0.4),
            });
        }
    }

    void showPopup() override
    {
        m_popupOpen = true;
        update();
        QComboBox::showPopup();
        placePopupBelow();
        QTimer::singleShot(0, this, [this]() {
            if (m_popupOpen)
                placePopupBelow();
        });
    }

    void hidePopup() override
    {
        m_popupOpen = false;
        update();
        QComboBox::hidePopup();
    }

    void wheelEvent(QWheelEvent *event) override
    {
        event->ignore();
    }

private:
    void placePopupBelow()
    {
        QAbstractItemView *itemView = view();
        if (!itemView)
            return;
        QWidget *container = itemView->parentWidget();
        if (!container)
            return;

        container->resize(width(), container->height());

        QPoint pos = mapToGlobal(QPoint(0, height() + 2));
        if (QScreen *sc = screen()) {
            const QRect avail = sc->availableGeometry();
            if (pos.x() + container->width() > avail.right())
                pos.setX(qMax(avail.left(), avail.right() - container->width() + 1));
            if (pos.x() < avail.left())
                pos.setX(avail.left());
            if (pos.y() + container->height() > avail.bottom()) {
                const int aboveY = mapToGlobal(QPoint(0, 0)).y() - container->height() - 2;
                if (aboveY >= avail.top())
                    pos.setY(aboveY);
            }
        }
        container->move(pos);
    }

    bool m_popupOpen = false;
};

} // namespace

void AppStyle::apply(QApplication &app)
{
    if (QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion")))
        app.setStyle(new AppProxyStyle(fusion));

    QFont font(QStringLiteral("Segoe UI"));
    font.setStyleHint(QFont::SansSerif);
    font.setPixelSize(13);
    app.setFont(font);

    QPalette pal = app.palette();
    pal.setColor(QPalette::Window, QColor(QStringLiteral("#F4F5F7")));
    pal.setColor(QPalette::WindowText, QColor(QStringLiteral("#1B2838")));
    pal.setColor(QPalette::Base, QColor(QStringLiteral("#FFFFFF")));
    pal.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#F4F5F7")));
    pal.setColor(QPalette::Text, QColor(QStringLiteral("#1B2838")));
    pal.setColor(QPalette::Button, QColor(QStringLiteral("#FFFFFF")));
    pal.setColor(QPalette::ButtonText, QColor(QStringLiteral("#1B2838")));
    pal.setColor(QPalette::Highlight, QColor(QStringLiteral("#E11D2E")));
    pal.setColor(QPalette::HighlightedText, Qt::white);
    pal.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#8B939C")));
    pal.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#1B2838")));
    pal.setColor(QPalette::ToolTipText, Qt::white);
    pal.setColor(QPalette::Link, QColor(QStringLiteral("#4DA3FF")));
    app.setPalette(pal);

    static const char kCss[] = R"qss(
* {
  font-family: "Segoe UI", "Microsoft YaHei UI", "Microsoft YaHei", sans-serif;
}

QMainWindow, QDialog, QWidget#page, QWidget#centralRoot {
  background: #F4F5F7;
  color: #1B2838;
}

QStatusBar {
  background: #F4F5F7;
  color: #6B7280;
  border-top: 1px solid #E6E8EC;
  min-height: 22px;
}

QLabel#titleLabel {
  font-size: 16px;
  font-weight: 600;
  color: #1B2838;
}

QLabel#hintLabel {
  color: #8B939C;
  font-size: 12px;
}

QLabel#sectionLabel {
  color: #6B7280;
  font-size: 11px;
  font-weight: 600;
  padding-top: 8px;
}

QWidget#settingsCard {
  background: #FFFFFF;
  border: 1px solid #E2E4E8;
  border-radius: 12px;
}

QWidget#foldHeader {
  background: transparent;
  border-radius: 12px;
}

QWidget#foldHeader:hover {
  background: #F7F8FA;
}

QLabel#cardTitle {
  font-size: 13px;
  font-weight: 600;
  color: #1B2838;
}

QLabel#fieldLabel {
  color: #6B7280;
  font-size: 13px;
}

QCheckBox#settingsCheck {
  spacing: 8px;
  min-height: 28px;
  color: #1B2838;
}

QPushButton {
  background: #FFFFFF;
  color: #1B2838;
  border: 1px solid #E2E4E8;
  border-radius: 8px;
  padding: 6px 12px;
  min-height: 32px;
}

QPushButton:hover {
  background: #F3F4F6;
  border-color: #C9CDD4;
}

QPushButton:pressed {
  background: #E8EAED;
}

QPushButton:disabled {
  color: #B0B6BE;
  background: #F3F4F6;
}

QPushButton#primaryButton {
  background: #E11D2E;
  color: #FFFFFF;
  border: none;
  border-radius: 12px;
  font-size: 15px;
  font-weight: 600;
  min-height: 44px;
  padding: 8px 20px;
}

QPushButton#primaryButton:hover {
  background: #C91828;
}

QPushButton#primaryButton:pressed {
  background: #A81421;
}

QPushButton#primaryButton:disabled {
  background: #E7B4B9;
  color: #FFFFFF;
}

QToolButton#iconButton {
  border: none;
  background: transparent;
  border-radius: 8px;
  padding: 6px;
}

QToolButton#iconButton:hover {
  background: #E8EAED;
}

QToolButton#iconButton:pressed {
  background: #DDE0E5;
}

QToolButton#segBtn {
  background: #FFFFFF;
  color: #1B2838;
  border: 1px solid #E2E4E8;
  border-radius: 0;
  padding: 8px 10px;
  min-height: 34px;
  font-weight: 500;
}

QToolButton#segBtn:hover {
  background: #F3F4F6;
}

QToolButton#segBtn:checked {
  background: #1B2838;
  color: #FFFFFF;
  border-color: #1B2838;
}

QToolButton#segBtn[pos="first"],
QToolButton#segBtn[pos="first"]:checked {
  border-top-left-radius: 10px;
  border-bottom-left-radius: 10px;
}

QToolButton#segBtn[pos="last"],
QToolButton#segBtn[pos="last"]:checked {
  border-top-right-radius: 10px;
  border-bottom-right-radius: 10px;
}

QCheckBox#chipCheck {
  background: #FFFFFF;
  border: 1px solid #E2E4E8;
  border-radius: 10px;
  padding: 8px 12px 8px 10px;
  spacing: 8px;
  min-height: 36px;
}

QCheckBox#chipCheck:hover {
  border-color: #C9CDD4;
}

QCheckBox#chipCheck:checked {
  background: #FDECEE;
  border-color: #E11D2E;
}

QCheckBox#chipCheck::indicator {
  width: 16px;
  height: 16px;
}

QComboBox, QLineEdit, QKeySequenceEdit {
  background: #FFFFFF;
  color: #1B2838;
  border: 1px solid #E2E4E8;
  border-radius: 8px;
  padding: 4px 10px;
  min-height: 32px;
}

QComboBox {
  padding-right: 28px;
}

QComboBox:hover, QLineEdit:hover, QKeySequenceEdit:hover {
  border-color: #C9CDD4;
}

QComboBox:focus, QLineEdit:focus, QKeySequenceEdit:focus {
  border-color: #4DA3FF;
}

QComboBox::drop-down {
  subcontrol-origin: padding;
  subcontrol-position: center right;
  width: 28px;
  border: none;
  background: transparent;
}

QComboBox::down-arrow {
  image: none;
  width: 0px;
  height: 0px;
}

QComboBox QAbstractItemView {
  background: #FFFFFF;
  border: 1px solid #E2E4E8;
  selection-background-color: #FDECEE;
  selection-color: #1B2838;
  outline: none;
  padding: 4px;
}

QScrollArea {
  border: none;
  background: transparent;
}

QScrollBar:vertical {
  background: transparent;
  width: 10px;
  margin: 0;
}

QScrollBar::handle:vertical {
  background: #D0D4DA;
  border-radius: 4px;
  min-height: 24px;
}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
  height: 0;
}

QMessageBox {
  background: #F4F5F7;
}

QMessageBox QPushButton {
  min-width: 72px;
  min-height: 30px;
}

QToolTip {
  background: #1B2838;
  color: #FFFFFF;
  border: none;
  padding: 6px 8px;
  border-radius: 6px;
}
)qss";

    app.setStyleSheet(QString::fromUtf8(kCss));
}

QComboBox *AppStyle::createComboBox(QWidget *parent)
{
    return new StyledComboBox(parent);
}

QIcon AppIcons::record(const QColor &color)
{
    return paintIcon(color, [color](QPainter &p, const QRectF &r) {
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(r.adjusted(r.width() * 0.08, r.height() * 0.08,
                                 -r.width() * 0.08, -r.height() * 0.08));
    });
}

QIcon AppIcons::pause(const QColor &color)
{
    return paintIcon(color, [color](QPainter &p, const QRectF &r) {
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        const qreal w = r.width() * 0.28;
        const qreal gap = r.width() * 0.16;
        const qreal x1 = r.center().x() - gap / 2 - w;
        const qreal x2 = r.center().x() + gap / 2;
        p.drawRoundedRect(QRectF(x1, r.top(), w, r.height()), 2, 2);
        p.drawRoundedRect(QRectF(x2, r.top(), w, r.height()), 2, 2);
    });
}

QIcon AppIcons::resume(const QColor &color)
{
    return paintIcon(color, [color](QPainter &p, const QRectF &r) {
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        QPainterPath path;
        path.moveTo(r.left() + r.width() * 0.12, r.top());
        path.lineTo(r.right(), r.center().y());
        path.lineTo(r.left() + r.width() * 0.12, r.bottom());
        path.closeSubpath();
        p.drawPath(path);
    });
}

QIcon AppIcons::stop(const QColor &color)
{
    return paintIcon(color, [color](QPainter &p, const QRectF &r) {
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        const qreal inset = r.width() * 0.12;
        p.drawRoundedRect(r.adjusted(inset, inset, -inset, -inset), 3, 3);
    });
}

QIcon AppIcons::mic(const QColor &color)
{
    return paintIcon(color, [](QPainter &p, const QRectF &r) {
        const QRectF cap(r.center().x() - r.width() * 0.22,
                         r.top(),
                         r.width() * 0.44,
                         r.height() * 0.58);
        p.drawRoundedRect(cap, cap.width() / 2, cap.width() / 2);
        p.drawArc(QRectF(r.left() + r.width() * 0.08,
                         r.top() + r.height() * 0.18,
                         r.width() * 0.84,
                         r.height() * 0.58),
                  200 * 16, 140 * 16);
        p.drawLine(QPointF(r.center().x(), r.top() + r.height() * 0.72),
                   QPointF(r.center().x(), r.bottom() - r.height() * 0.06));
        p.drawLine(QPointF(r.center().x() - r.width() * 0.22, r.bottom()),
                   QPointF(r.center().x() + r.width() * 0.22, r.bottom()));
    });
}

QIcon AppIcons::speaker(const QColor &color)
{
    return paintIcon(color, [](QPainter &p, const QRectF &r) {
        QPainterPath path;
        const qreal cx = r.left() + r.width() * 0.42;
        path.moveTo(r.left(), r.top() + r.height() * 0.32);
        path.lineTo(r.left() + r.width() * 0.22, r.top() + r.height() * 0.32);
        path.lineTo(cx, r.top() + r.height() * 0.12);
        path.lineTo(cx, r.bottom() - r.height() * 0.12);
        path.lineTo(r.left() + r.width() * 0.22, r.bottom() - r.height() * 0.32);
        path.lineTo(r.left(), r.bottom() - r.height() * 0.32);
        path.closeSubpath();
        p.drawPath(path);
        p.drawArc(QRectF(r.left() + r.width() * 0.38, r.top() + r.height() * 0.18,
                         r.width() * 0.42, r.height() * 0.64),
                  -50 * 16, 100 * 16);
        p.drawArc(QRectF(r.left() + r.width() * 0.50, r.top() + r.height() * 0.02,
                         r.width() * 0.48, r.height() * 0.96),
                  -50 * 16, 100 * 16);
    });
}

QIcon AppIcons::settings(const QColor &color)
{
    return paintIcon(color, [color](QPainter &p, const QRectF &r) {
        const qreal y1 = r.top() + r.height() * 0.18;
        const qreal y2 = r.center().y();
        const qreal y3 = r.bottom() - r.height() * 0.18;
        p.drawLine(QPointF(r.left(), y1), QPointF(r.right(), y1));
        p.drawLine(QPointF(r.left(), y2), QPointF(r.right(), y2));
        p.drawLine(QPointF(r.left(), y3), QPointF(r.right(), y3));
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        const qreal knob = r.width() * 0.13;
        p.drawEllipse(QPointF(r.left() + r.width() * 0.28, y1), knob, knob);
        p.drawEllipse(QPointF(r.left() + r.width() * 0.68, y2), knob, knob);
        p.drawEllipse(QPointF(r.left() + r.width() * 0.40, y3), knob, knob);
    });
}

QIcon AppIcons::back(const QColor &color)
{
    return paintIcon(color, [](QPainter &p, const QRectF &r) {
        p.drawPolyline(QPolygonF{
            QPointF(r.center().x() + r.width() * 0.18, r.top() + r.height() * 0.08),
            QPointF(r.left() + r.width() * 0.12, r.center().y()),
            QPointF(r.center().x() + r.width() * 0.18, r.bottom() - r.height() * 0.08),
        });
    });
}

QIcon AppIcons::chevronDown(const QColor &color)
{
    return paintIcon(color, [color](QPainter &p, const QRectF &r) {
        p.setPen(QPen(color, qMax(2.6, r.width() / 5.5), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(QPolygonF{
            QPointF(r.left() + r.width() * 0.08, r.top() + r.height() * 0.30),
            QPointF(r.center().x(), r.bottom() - r.height() * 0.22),
            QPointF(r.right() - r.width() * 0.08, r.top() + r.height() * 0.30),
        });
    });
}

QIcon AppIcons::chevronRight(const QColor &color)
{
    return paintIcon(color, [color](QPainter &p, const QRectF &r) {
        p.setPen(QPen(color, qMax(2.6, r.width() / 5.5), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(QPolygonF{
            QPointF(r.left() + r.width() * 0.30, r.top() + r.height() * 0.08),
            QPointF(r.right() - r.width() * 0.22, r.center().y()),
            QPointF(r.left() + r.width() * 0.30, r.bottom() - r.height() * 0.08),
        });
    });
}

QIcon AppIcons::folder(const QColor &color)
{
    return paintIcon(color, [](QPainter &p, const QRectF &r) {
        QPainterPath path;
        path.moveTo(r.left(), r.top() + r.height() * 0.28);
        path.lineTo(r.left(), r.top() + r.height() * 0.18);
        path.lineTo(r.left() + r.width() * 0.38, r.top() + r.height() * 0.18);
        path.lineTo(r.left() + r.width() * 0.48, r.top() + r.height() * 0.30);
        path.lineTo(r.right(), r.top() + r.height() * 0.30);
        path.lineTo(r.right(), r.bottom());
        path.lineTo(r.left(), r.bottom());
        path.closeSubpath();
        p.drawPath(path);
    });
}
