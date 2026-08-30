#ifndef COUNTDOWNOVERLAY_H
#define COUNTDOWNOVERLAY_H

#include <QWidget>

class QScreen;
class QShowEvent;
class QVariantAnimation;

class CountdownOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit CountdownOverlay(QWidget *parent = nullptr);

    void showOnScreen(QScreen *screen, int seconds);
    void setRemaining(int seconds);
    void dismiss();

signals:
    void skipped();
    void cancelled();

protected:
    void showEvent(QShowEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void pulse();

    int m_remaining = 0;
    int m_total = 0;
    qreal m_scale = 1.0;
    QVariantAnimation *m_pulse = nullptr;
};

#endif
