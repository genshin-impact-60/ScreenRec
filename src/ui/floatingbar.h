#ifndef FLOATINGBAR_H
#define FLOATINGBAR_H

#include <QWidget>

class QLabel;
class QPushButton;
class QShowEvent;

class FloatingBar : public QWidget
{
    Q_OBJECT

public:
    explicit FloatingBar(QWidget *parent = nullptr);

    void setDurationText(const QString &text);
    void setRecordingUi(bool paused, bool countdown);

signals:
    void pauseClicked();
    void stopClicked();

protected:
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    QLabel *m_dot = nullptr;
    QLabel *m_duration = nullptr;
    QPushButton *m_pauseButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPoint m_dragOffset;
};

#endif
