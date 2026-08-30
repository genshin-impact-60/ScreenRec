#ifndef PREVIEWWIDGET_H
#define PREVIEWWIDGET_H

#include <QPixmap>
#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;
class QToolButton;

class PreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PreviewWidget(QWidget *parent = nullptr);

    void setFrame(const QPixmap &pixmap);
    void setPlaceholder(const QString &title, const QString &hint = QString());
    void setBadge(const QString &badge);
    void setClickable(bool clickable);
    void showResult(const QString &title, const QString &meta);
    void clearResult();
    bool hasResult() const;

signals:
    void clicked();
    void settingsClicked();
    void openFileClicked();
    void openFolderClicked();
    void resultDismissed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void layoutOverlays();

    QPixmap m_pixmap;
    QString m_title;
    QString m_hint;
    QString m_badge;
    bool m_clickable = false;
    bool m_hovered = false;

    QToolButton *m_settingsButton = nullptr;
    QWidget *m_result = nullptr;
    QLabel *m_resultTitle = nullptr;
    QLabel *m_resultMeta = nullptr;
};

#endif
