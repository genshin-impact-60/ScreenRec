#ifndef PREVIEWWIDGET_H
#define PREVIEWWIDGET_H

#include <QPixmap>
#include <QString>
#include <QWidget>

class QMouseEvent;
class QPaintEvent;

class PreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PreviewWidget(QWidget *parent = nullptr);

    void setFrame(const QPixmap &pixmap);
    void setPlaceholder(const QString &title, const QString &hint = QString());
    void setBadge(const QString &badge);
    void setClickable(bool clickable);

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QPixmap m_pixmap;
    QString m_title;
    QString m_hint;
    QString m_badge;
    bool m_clickable = false;
    bool m_hovered = false;
};

#endif
