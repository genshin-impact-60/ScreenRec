#ifndef COUNTDOWNOVERLAY_H
#define COUNTDOWNOVERLAY_H

#include <QWidget>

class QScreen;
class QShowEvent;

class CountdownOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit CountdownOverlay(QWidget *parent = nullptr);

    void showOnScreen(QScreen *screen, int seconds);
    void setRemaining(int seconds);
    void dismiss();

protected:
    void showEvent(QShowEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    int m_remaining = 0;
    int m_total = 0;
};

#endif
