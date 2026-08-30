#ifndef REGIONSELECTOR_H
#define REGIONSELECTOR_H

#include <QRect>
#include <QWidget>

class QPushButton;
class QScreen;
class QShowEvent;

class RegionSelector : public QWidget
{
    Q_OBJECT

public:
    explicit RegionSelector(QWidget *parent = nullptr);

    void start(QScreen *screen, const QRect &initial = QRect());
    void dismiss();

signals:
    void selected(const QRect &screenLocalLogical);
    void cancelled();

protected:
    void showEvent(QShowEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    enum class Handle {
        None,
        Body,
        Left,
        Right,
        Top,
        Bottom,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight
    };

    Handle hitTest(const QPoint &pos) const;
    QRect handleRect(Handle handle) const;
    QRect normalizedRect() const;
    QCursor cursorFor(Handle handle) const;
    void updateToolbar();
    void confirm();
    void cancel();
    void applyRatio(qreal ratio);
    void applyFixedSize(QSize size);
    void setLockedRatio(qreal ratio);
    QRect fitRatio(QRect rect, qreal ratio) const;

    QRect m_rect;
    bool m_dragging = false;
    Handle m_activeHandle = Handle::None;
    QPoint m_dragOrigin;
    QRect m_rectAtPress;
    qreal m_lockRatio = 0;
    QWidget *m_toolbar = nullptr;
    QPushButton *m_okButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QPushButton *m_ratio169 = nullptr;
    QPushButton *m_ratio43 = nullptr;
    QPushButton *m_ratio11 = nullptr;
    QPushButton *m_size1080 = nullptr;
};

#endif
